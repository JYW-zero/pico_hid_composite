/*
 * src/app/key_stats.c
 * 按键统计模块实现
 * 完整版本：RAM统计 + Flash持久化 + 磨损均衡
 */

#include "app/key_stats.h"
#include "board/flash_layout.h"
#include "middleware/fault.h"
#include "middleware/flash_service.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include <string.h>
#include <stdio.h>

/* ==================== CRC32 实现 ==================== */

static const uint32_t s_crc32_table[16] =
{
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

static uint32_t key_stats_crc32(const uint8_t* data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++)
    {
        uint8_t byte = data[i];
        crc = (crc >> 4) ^ s_crc32_table[(crc & 0x0Fu) ^ (byte & 0x0Fu)];
        crc = (crc >> 4) ^ s_crc32_table[(crc & 0x0Fu) ^ (byte >> 4)];
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ==================== 静态变量 ==================== */

static uint32_t s_counts[KEY_STATS_MAX_KEYS];  /* 当前按键计数 */
static uint32_t s_total = 0;                    /* 总按键数 */
static bool s_initialized = false;

static uint32_t s_next_save_index = 0;          /* 下一个保存位置 */
static uint32_t s_save_count = 0;               /* 本次周期保存次数 */
static uint32_t s_last_save_ms = 0;             /* 上次保存时间 */
static uint32_t s_uptime_s = 0;                 /* 运行时间（秒） */

/* ==================== 内部函数 ==================== */

/* 扫描Flash，找到最新的有效记录和下一个写入位置 */
static void scan_flash_records(void)
{
    const key_stats_record_t* records = (const key_stats_record_t*)KEY_STATS_FLASH_ADDR;

    s_next_save_index = 0;
    s_save_count = 0;

    /* 找到最后一条有效记录 */
    for (uint32_t i = 0; i < KEY_STATS_MAX_RECORDS; i++)
    {
        if (records[i].magic == KEY_STATS_MAGIC)
        {
            s_next_save_index = i + 1;
            s_save_count++;
        }
        else
        {
            /* 遇到无效的，后面的都无效了（顺序写入） */
            break;
        }
    }

    /* 如果写满了，下次从0开始（写入前会先擦除） */
    if (s_next_save_index >= KEY_STATS_MAX_RECORDS)
    {
        s_next_save_index = 0;
    }
}

/* 擦除整个统计扇区 */
static void erase_stats_flash(void)
{
    flash_service_erase(KEY_STATS_FLASH_OFFSET, KEY_STATS_FLASH_SIZE);
}

/* 写入一条记录到Flash指定位置 */
static void write_record_to_flash(uint32_t index, const key_stats_record_t* record)
{
    if (index >= KEY_STATS_MAX_RECORDS)
    {
        return;
    }

    /* 使用静态缓冲区，先清零再复制结构体，避免越界读取栈数据 */
    static uint8_t write_buf[KEY_STATS_RECORD_SIZE];
    memset(write_buf, 0, sizeof(write_buf));
    memcpy(write_buf, record, sizeof(key_stats_record_t));

    uint32_t offset = KEY_STATS_FLASH_OFFSET + index * KEY_STATS_RECORD_SIZE;
    flash_service_program(offset, write_buf, KEY_STATS_RECORD_SIZE);
}

/* ==================== 对外接口 ==================== */

void key_stats_init(void)
{
    /* 确保Flash布局已初始化（检测Flash大小） */
    flash_layout_init();

    memset(s_counts, 0, sizeof(s_counts));
    s_total = 0;
    s_last_save_ms = 0;
    s_uptime_s = 0;

    /* 扫描Flash */
    scan_flash_records();

    /* 尝试加载最近的记录 */
    if (s_save_count > 0)
    {
        key_stats_load_from_flash();
    }

    s_initialized = true;
}

void key_stats_increment(uint8_t key_index)
{
    if (!s_initialized || key_index >= KEY_STATS_MAX_KEYS)
    {
        return;
    }

    s_counts[key_index]++;
    s_total++;
}

uint32_t key_stats_get_count(uint8_t key_index)
{
    if (key_index >= KEY_STATS_MAX_KEYS)
    {
        return 0;
    }
    return s_counts[key_index];
}

uint32_t key_stats_get_total(void)
{
    return s_total;
}

void key_stats_reset(void)
{
    memset(s_counts, 0, sizeof(s_counts));
    s_total = 0;
}

void key_stats_save_to_flash(void)
{
    if (!s_initialized)
    {
        return;
    }

    /* 如果写满了，先擦除整个扇区，然后从头开始写 */
    if (s_next_save_index == 0 && s_save_count >= KEY_STATS_MAX_RECORDS)
    {
        erase_stats_flash();
        s_save_count = 0;
    }

    /* 构造记录 */
    key_stats_record_t record;
    memset(&record, 0, sizeof(record));
    record.magic = KEY_STATS_MAGIC;
    record.timestamp_s = s_uptime_s;
    record.total_keystrokes = s_total;
    memcpy(record.counts, s_counts, sizeof(s_counts));

    /* 计算CRC32（除crc32字段外的所有内容） */
    uint32_t crc = key_stats_crc32((const uint8_t*)&record,
                                   sizeof(key_stats_record_t) - sizeof(uint32_t));
    record.crc32 = crc;

    /* 写入Flash */
    write_record_to_flash(s_next_save_index, &record);

    /* 更新索引 */
    s_next_save_index++;
    s_save_count++;
    s_last_save_ms = (uint32_t)to_ms_since_boot(get_absolute_time());

    if (s_next_save_index >= KEY_STATS_MAX_RECORDS)
    {
        s_next_save_index = 0;
    }
}

bool key_stats_load_from_flash(void)
{
    const key_stats_record_t* records = (const key_stats_record_t*)KEY_STATS_FLASH_ADDR;

    /* 找到最后一条有效记录（magic + CRC双重校验） */
    int last_index = -1;
    for (uint32_t i = 0; i < KEY_STATS_MAX_RECORDS; i++)
    {
        if (records[i].magic == KEY_STATS_MAGIC)
        {
            /* 验证CRC32 */
            uint32_t calc_crc = key_stats_crc32((const uint8_t*)&records[i],
                                                 sizeof(key_stats_record_t) - sizeof(uint32_t));
            if (calc_crc == records[i].crc32)
            {
                last_index = (int)i;
            }
            else
            {
                /* CRC不匹配，记录损坏，停止查找（后续记录也不可信） */
                break;
            }
        }
        else
        {
            break;
        }
    }

    if (last_index < 0)
    {
        return false;
    }

    /* 加载数据 */
    s_total = records[last_index].total_keystrokes;
    memcpy(s_counts, records[last_index].counts, sizeof(s_counts));
    s_uptime_s = records[last_index].timestamp_s;

    return true;
}

void key_stats_tick(void)
{
    if (!s_initialized)
    {
        return;
    }

    static uint32_t last_tick_ms = 0;
    uint32_t now = (uint32_t)to_ms_since_boot(get_absolute_time());

    /* 每秒更新运行时间 */
    if (now - last_tick_ms >= 1000)
    {
        s_uptime_s += (now - last_tick_ms) / 1000;
        last_tick_ms = now;
    }

    /* 自动保存：每隔30分钟保存一次（使用安全写入服务，双核同步） */
    if (now - s_last_save_ms >= KEY_STATS_AUTO_SAVE_INTERVAL_MS)
    {
        if (s_total > 0)
        {
            key_stats_save_to_flash();
        }
        else
        {
            s_last_save_ms = now;
        }
    }
}

uint32_t key_stats_get_save_count(void)
{
    return s_save_count;
}
