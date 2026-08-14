/*
 * src/middleware/fault.c
 * 统一故障处理中间件实现
 * 完整版本：分级处理 + Flash持久化存储
 * 掉电不丢失，最多保留最近64条错误日志
 */

#include "middleware/fault.h"
#include "middleware/flash_service.h"
#include "board/flash_layout.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/time.h"
#include "pico/platform.h"

/* ==================== 静态变量 ==================== */

static uint32_t s_total_count = 0;     /* 总故障计数 */
static uint32_t s_log_count = 0;       /* 当前日志条数 */
static uint32_t s_write_index = 0;     /* 下一个写入位置 */
static bool     s_initialized = false;

/* ==================== 内部函数 ==================== */

/* 扫描Flash，找到下一个可写入的位置
 * 同时统计有效日志条数
 */
static void scan_flash_logs(void)
{
    const fault_log_entry_t* flash_logs = (const fault_log_entry_t*)FAULT_LOG_FLASH_ADDR;

    s_log_count = 0;
    s_write_index = 0;

    /* 逐个扫描，找到第一条无效的 */
    for (uint32_t i = 0; i < FAULT_LOG_MAX_COUNT; i++)
    {
        if (flash_logs[i].magic == FAULT_LOG_MAGIC)
        {
            s_log_count++;
            s_write_index = i + 1;
        }
        else
        {
            /* 遇到无效的，后面的都无效了（顺序写入） */
            break;
        }
    }

    /* 如果写满了，就从0开始（下次写入前会先擦除） */
    if (s_write_index >= FAULT_LOG_MAX_COUNT)
    {
        s_write_index = 0;
    }
}

/* 擦除整个日志扇区 */
static void erase_log_flash(void)
{
    flash_service_erase(FAULT_LOG_FLASH_OFFSET, FAULT_LOG_FLASH_SIZE);
}

/* 写入一条日志到Flash指定位置 */
static void write_log_to_flash(uint32_t index, const fault_log_entry_t* entry)
{
    if (index >= FAULT_LOG_MAX_COUNT)
    {
        return;
    }

    /* 使用静态缓冲区，避免传入栈指针（flash_range_program要求RAM指针） */
    static uint8_t write_buf[FAULT_LOG_ENTRY_SIZE];
    memset(write_buf, 0xFF, sizeof(write_buf));
    memcpy(write_buf, entry, sizeof(fault_log_entry_t));

    uint32_t offset = FAULT_LOG_FLASH_OFFSET + index * FAULT_LOG_ENTRY_SIZE;
    flash_service_program(offset, write_buf, FAULT_LOG_ENTRY_SIZE);
}

/* ==================== 对外接口 ==================== */

void fault_init(void)
{
    /* 确保Flash布局已初始化（检测Flash大小） */
    flash_layout_init();

    scan_flash_logs();
    s_initialized = true;
    s_total_count = s_log_count;  /* 初始计数等于日志条数 */

    printf("[FAULT] 初始化完成，历史日志: %d 条\n", s_log_count);
}

void fault_record(fault_level_e level, const char *module, const char *msg)
{
    s_total_count++;

    /* 串口输出（Phase 1 兼容） */
    const char *level_str = fault_level_name(level);
    printf("[FAULT][%s] %s: %s\n", level_str, module ? module : "?", msg ? msg : "?");

    /* 如果未初始化，只输出串口，不写Flash */
    if (!s_initialized)
    {
        return;
    }

    /* 参数检查 */
    if (module == NULL) module = "unknown";
    if (msg == NULL) msg = "unknown";

    /* 构造日志条目 */
    fault_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.magic = FAULT_LOG_MAGIC;
    entry.timestamp_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    entry.level = (uint8_t)level;

    /* 复制模块名 */
    size_t module_len = strlen(module);
    if (module_len > 31) module_len = 31;
    entry.module_len = (uint8_t)module_len;
    memcpy(entry.module, module, module_len);
    entry.module[module_len] = '\0';

    /* 复制消息 */
    size_t msg_len = strlen(msg);
    if (msg_len > 199) msg_len = 199;
    entry.msg_len = (uint16_t)msg_len;
    memcpy(entry.msg, msg, msg_len);
    entry.msg[msg_len] = '\0';

    /* 只在Core0中写Flash，避免Core1和Core0同时调用flash_safe_execute导致死锁 */
    /* Core1中发生的错误只输出串口，由Core0统一处理Flash写入 */
    if (level >= FAULT_LEVEL_ERROR && get_core_num() == 0)
    {
        if (s_write_index == 0 && s_log_count >= FAULT_LOG_MAX_COUNT)
        {
            erase_log_flash();
            s_log_count = 0;
        }

        write_log_to_flash(s_write_index, &entry);

        s_write_index++;
        if (s_log_count < FAULT_LOG_MAX_COUNT)
        {
            s_log_count++;
        }

        if (s_write_index >= FAULT_LOG_MAX_COUNT)
        {
            s_write_index = 0;
        }
    }

    /* 致命错误：触发看门狗复位（10ms后复位，确保日志写入完成） */
    if (level == FAULT_LEVEL_FATAL)
    {
        watchdog_reboot(0, 0, 10);
    }
}

uint32_t fault_get_count(void)
{
    return s_total_count;
}

uint32_t fault_get_log_count(void)
{
    return s_log_count;
}

bool fault_get_log(uint32_t index, fault_log_entry_t* out_entry)
{
    if (out_entry == NULL)
    {
        return false;
    }
    if (index >= s_log_count)
    {
        return false;
    }

    const fault_log_entry_t* flash_logs = (const fault_log_entry_t*)FAULT_LOG_FLASH_ADDR;
    memcpy(out_entry, &flash_logs[index], sizeof(fault_log_entry_t));
    return true;
}

uint32_t fault_get_latest_logs(fault_log_entry_t* out_entries, uint32_t max_count)
{
    if (out_entries == NULL || max_count == 0)
    {
        return 0;
    }

    uint32_t count = (max_count < s_log_count) ? max_count : s_log_count;

    /* 最新的在后面，倒序复制 */
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t src_index = s_log_count - 1 - i;
        fault_get_log(src_index, &out_entries[i]);
    }

    return count;
}

void fault_clear(void)
{
    erase_log_flash();
    s_log_count = 0;
    s_write_index = 0;
    s_total_count = 0;
}

const char* fault_level_name(fault_level_e level)
{
    switch (level)
    {
        case FAULT_LEVEL_INFO:  return "INFO";
        case FAULT_LEVEL_WARN:  return "WARN";
        case FAULT_LEVEL_ERROR: return "ERROR";
        case FAULT_LEVEL_FATAL: return "FATAL";
        default:                return "UNKNOWN";
    }
}
