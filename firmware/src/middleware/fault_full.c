/*
 * src/middleware/fault_full.c
 * Fault 鎸佷箙鍖栧悗绔?- 鍏ㄩ噺鏂囨湰妗ｏ紙dev / FEATURE_FAULT_COMPACT=0锛? *
 * 鍘嗗彶琛屼负锛?56B/鏉★紙=1 Flash 椤碉級锛孉/B 鍙屾墖鍖猴紝娲诲姩鎵囧尯鍐欐弧 16 鏉″悗
 * 鎶婃渶杩?8 鏉℃惉鍒板彟涓€鎵囧尯骞跺垏鎹紙鍩虹纾ㄦ崯鍧囪　锛夈€? * 鏈枃浠跺湪 release锛團EATURE_FAULT_COMPACT=1锛変笅鏁翠綋缂栬瘧涓虹┖銆? */

#include "project_features.h"

#if !FEATURE_FAULT_COMPACT

#include "middleware/fault.h"
#include "middleware/fault_backend.h"
#include "middleware/flash_service.h"
#include "board/flash_layout.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "hardware/flash.h"
#include "pico/time.h"

/* Maximum module name length (excluding null terminator) */
#define FAULT_MODULE_NAME_MAX_LEN  (31U)
/* Maximum message length (excluding null terminator) */
#define FAULT_MSG_MAX_LEN          (199U)

/* ==================== Static State ==================== */

static uint32_t s_write_index = 0;     /* Next write position */
static uint8_t  s_active_sector = 0;   /* Active sector (0=A, 1=B) */
static uint32_t s_log_count = 0;       /* Valid entries in active sector */

static uint32_t get_active_sector_offset(void)
{
    return (s_active_sector == 0) ? flash_layout_fault_a_offset() : flash_layout_fault_b_offset();
}

static uint32_t get_other_sector_offset(void)
{
    return (s_active_sector == 0) ? flash_layout_fault_b_offset() : flash_layout_fault_a_offset();
}

/* 鍙鍩烘寚閽堬細鍥轰欢涓?XIP 鍦板潃锛宧ost 娴嬭瘯鐢?mock 閲嶅畾鍚戝埌 RAM 妯℃嫙 Flash */
static const uint8_t* sector_bytes(uint32_t offset)
{
    return (const uint8_t*)(uintptr_t)(flash_layout_xip_base() + offset);
}

/* 绗?i 鏉¤褰曠殑鍦板潃銆傛敞鎰忥細姣忔潯鎸?FAULT_LOG_ENTRY_SIZE(=256B, 1 Flash 椤?
 * 瀹氬潃锛岃€屼笉鏄?sizeof(fault_log_entry_t)(=252B)鈥斺€斿啓鍏ユ槸姣忛〉涓€鏉★紝璇诲彇蹇呴』鐢? * 鍚屾牱鐨?256B 姝ラ暱锛屽惁鍒欎粠绗?1 鏉¤捣浼氶敊浣嶏紙姣忔潯璁板綍鍚?4B 涓洪〉鍐呭～鍏咃級銆?*/
static const fault_log_entry_t* entry_at(uint32_t offset, uint32_t i)
{
    return (const fault_log_entry_t*)(const void*)(sector_bytes(offset) + i * FAULT_LOG_ENTRY_SIZE);
}
static const fault_log_entry_t* active_entry(uint32_t i)
{
    return entry_at(get_active_sector_offset(), i);
}

/* ==================== Scanning ==================== */

static uint32_t scan_sector(uint32_t sector_offset, uint32_t* out_write_index)
{
    uint32_t count = 0;
    uint32_t write_idx = 0;

    for (uint32_t i = 0; i < FAULT_LOG_MAX_COUNT; i++)
    {
        if (entry_at(sector_offset, i)->magic == FAULT_LOG_MAGIC)
        {
            count++;
            write_idx = i + 1;
        }
        else
        {
            break;
        }
    }

    if (write_idx >= FAULT_LOG_MAX_COUNT)
    {
        write_idx = 0;
    }

    if (out_write_index != NULL)
    {
        *out_write_index = write_idx;
    }
    return count;
}

/* Scan both sectors; active sector = the one with the newest last timestamp. */
static void scan_flash_logs(void)
{
    uint32_t write_idx_a = 0, write_idx_b = 0;
    uint32_t count_a = scan_sector(flash_layout_fault_a_offset(), &write_idx_a);
    uint32_t count_b = scan_sector(flash_layout_fault_b_offset(), &write_idx_b);

    if (count_a > 0 && count_b == 0)
    {
        s_active_sector = 0; s_log_count = count_a; s_write_index = write_idx_a;
    }
    else if (count_b > 0 && count_a == 0)
    {
        s_active_sector = 1; s_log_count = count_b; s_write_index = write_idx_b;
    }
    else if (count_a > 0 && count_b > 0)
    {
        uint32_t ts_a = entry_at(flash_layout_fault_a_offset(), count_a - 1U)->timestamp_ms;
        uint32_t ts_b = entry_at(flash_layout_fault_b_offset(), count_b - 1U)->timestamp_ms;

        if (ts_b > ts_a)
        {
            s_active_sector = 1; s_log_count = count_b; s_write_index = write_idx_b;
        }
        else
        {
            s_active_sector = 0; s_log_count = count_a; s_write_index = write_idx_a;
        }
    }
    else
    {
        s_active_sector = 0; s_log_count = 0; s_write_index = 0;
    }
}

/* ==================== Erase / Program ==================== */

static void erase_log_flash(void)
{
    flash_service_erase(get_active_sector_offset(), FAULT_LOG_FLASH_SIZE);
}

static void erase_sector_n(uint8_t sector)
{
    uint32_t offset = (sector == 0) ? flash_layout_fault_a_offset() : flash_layout_fault_b_offset();
    flash_service_erase(offset, FAULT_LOG_FLASH_SIZE);
}

static void write_log_to_flash(uint32_t index, const fault_log_entry_t* entry)
{
    if (index >= FAULT_LOG_MAX_COUNT)
    {
        return;
    }
    /* static buffer: flash_range_program requires a RAM-resident source */
    static uint8_t write_buf[FAULT_LOG_ENTRY_SIZE];
    memset(write_buf, 0xFF, sizeof(write_buf));
    memcpy(write_buf, entry, sizeof(fault_log_entry_t));

    uint32_t offset = get_active_sector_offset() + index * FAULT_LOG_ENTRY_SIZE;
    flash_service_program(offset, write_buf, FAULT_LOG_ENTRY_SIZE);
}

/* ==================== Backend API ==================== */

void fault_backend_init(void)
{
    scan_flash_logs();
}

void fault_backend_record(fault_level_e level, const char *module, const char *msg)
{
    fault_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.magic = FAULT_LOG_MAGIC;
    entry.timestamp_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    entry.level = (uint8_t)level;

    size_t module_len = strlen(module);
    if (module_len > FAULT_MODULE_NAME_MAX_LEN) module_len = FAULT_MODULE_NAME_MAX_LEN;
    entry.module_len = (uint8_t)module_len;
    memcpy(entry.module, module, module_len);
    entry.module[module_len] = '\0';

    size_t msg_len = strlen(msg);
    if (msg_len > FAULT_MSG_MAX_LEN) msg_len = FAULT_MSG_MAX_LEN;
    entry.msg_len = (uint16_t)msg_len;
    memcpy(entry.msg, msg, msg_len);
    entry.msg[msg_len] = '\0';

    if (s_write_index == 0 && s_log_count >= FAULT_LOG_MAX_COUNT)
    {
        /* Active sector full: copy the newest half to the other sector, then switch.
         * recent_logs is static (2KB) to avoid blowing the Core0 4KB stack. */
        static fault_log_entry_t recent_logs[FAULT_LOG_MAX_COUNT / 2];
        uint32_t keep_count = FAULT_LOG_MAX_COUNT / 2;
        uint8_t old_sector = s_active_sector;

        for (uint32_t i = 0; i < keep_count; i++)
        {
            uint32_t src_idx = (FAULT_LOG_MAX_COUNT - keep_count) + i;
            memcpy(&recent_logs[i], active_entry(src_idx), sizeof(fault_log_entry_t));
        }

        s_active_sector = (old_sector == 0) ? 1 : 0;
        (void)get_other_sector_offset();
        erase_log_flash();

        for (uint32_t i = 0; i < keep_count; i++)
        {
            write_log_to_flash(i, &recent_logs[i]);
        }

        s_write_index = keep_count;
        s_log_count = keep_count;

        printf("[INFO] fault: sector switch %d->%d (wear leveling), kept %d logs\n",
               old_sector, s_active_sector, (int)keep_count);
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

uint32_t fault_backend_get_log_count(void)
{
    return s_log_count;
}

bool fault_backend_get_log(uint32_t index, fault_log_entry_t* out_entry)
{
    if (index >= s_log_count)
    {
        return false;
    }
    memcpy(out_entry, active_entry(index), sizeof(fault_log_entry_t));
    return true;
}

void fault_backend_clear(void)
{
    erase_sector_n(0);
    erase_sector_n(1);
    s_active_sector = 0;
    s_log_count = 0;
    s_write_index = 0;
}

/* Full backend writes every entry immediately, nothing to flush. */
void fault_backend_flush(void)
{
}

#endif /* !FEATURE_FAULT_COMPACT */
