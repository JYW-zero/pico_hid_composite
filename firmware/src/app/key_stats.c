/*
 * src/app/key_stats.c
 * 按键统计模块实现
 * 完整版本：RAM统计 + Flash持久化 + 磨损均衡
 */

#include "app/key_stats.h"
#include "board/flash_layout.h"
#include "middleware/fault.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include <string.h>
#include <stdio.h>

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
    uint32_t saved = save_and_disable_interrupts();
    flash_range_erase(KEY_STATS_FLASH_OFFSET, KEY_STATS_FLASH_SIZE);
    restore_interrupts(saved);
}

/* 写入一条记录到Flash指定位置 */
static void write_record_to_flash(uint32_t index, const key_stats_record_t* record)
{
    if (index >= KEY_STATS_MAX_RECORDS)
    {
        return;
    }

    uint32_t offset = KEY_STATS_FLASH_OFFSET + index * KEY_STATS_RECORD_SIZE;
    uint32_t saved = save_and_disable_interrupts();
    flash_range_program(offset, (const uint8_t*)record, KEY_STATS_RECORD_SIZE);
    restore_interrupts(saved);
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

    printf("[KEY_STATS] 初始化完成，历史保存次数: %lu\n", s_save_count);
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

    printf("[KEY_STATS] 已保存到Flash，位置: %lu，总次数: %lu\n",
           s_next_save_index - 1, s_total);
}

bool key_stats_load_from_flash(void)
{
    const key_stats_record_t* records = (const key_stats_record_t*)KEY_STATS_FLASH_ADDR;

    /* 找到最后一条有效记录 */
    int last_index = -1;
    for (uint32_t i = 0; i < KEY_STATS_MAX_RECORDS; i++)
    {
        if (records[i].magic == KEY_STATS_MAGIC)
        {
            last_index = (int)i;
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

    printf("[KEY_STATS] 从Flash加载统计，总按键数: %lu\n", s_total);
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

    /* 自动保存：每隔5分钟保存一次 */
    if (now - s_last_save_ms >= KEY_STATS_AUTO_SAVE_INTERVAL_MS)
    {
        if (s_total > 0)
        {
            key_stats_save_to_flash();
        }
        else
        {
            s_last_save_ms = now;  /* 没有按键也更新时间，避免重复检查 */
        }
    }
}

uint32_t key_stats_get_save_count(void)
{
    return s_save_count;
}
