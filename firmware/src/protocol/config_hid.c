/*
 * protocol/config_hid.c
 * HID 配置协议模块（从 main.c 提取）
 */

#include "protocol/config_hid.h"
#include <string.h>
#include <stdio.h>
#include "usb_descriptors.h"
#include "board/config.h"
#include "board/board.h"
#include "app/macro.h"
#include "app/key_stats.h"
#include "middleware/fault.h"
#include "middleware/perf_monitor.h"
#include "middleware/watchdog.h"
#include "hardware/watchdog.h"
#include "device/paw3395.h"
#include "pico/time.h"
#include "pico/bootrom.h"

/* 日志宏（与 main.c 中保持一致） */
#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif
#define LOG_ERROR 1
#define LOG_INFO 2
#define LOG_DEBUG 3
#define LOG_ERROR_PRINT(fmt, ...) do { if (LOG_LEVEL >= LOG_ERROR) printf("[ERR] " fmt, ##__VA_ARGS__); } while(0)
#define LOG_INFO_PRINT(fmt, ...)  do { if (LOG_LEVEL >= LOG_INFO)  printf("[INFO] " fmt, ##__VA_ARGS__); } while(0)
#define LOG_DEBUG_PRINT(fmt, ...) do { if (LOG_LEVEL >= LOG_DEBUG) printf("[DBG] " fmt, ##__VA_ARGS__); } while(0)

/* 临时配置缓冲区 */
static device_config_t g_tmp_config;
static volatile bool g_config_pending = false;
static volatile uint8_t g_pending_cmd = 0;
static volatile uint8_t g_pending_cmd_param = 0;

/* 宏配置读取索引 */
static uint8_t s_macro_read_id = 0;
static uint8_t s_macro_read_block = 0;

/* 错误日志读取索引 */
static uint8_t s_fault_read_index = 0;

/* 性能监控 - 任务读取索引 */
static uint8_t s_perf_task_index = 0;

/* 宏配置脏标志（需要保存到Flash） */
static bool s_macro_dirty = false;
static uint32_t s_macro_dirty_time_us = 0;

/* 控制命令码（与 main.c 保持一致） */
#define CMD_SAVE_CONFIG   0x01
#define CMD_RESET_CONFIG  0x02
#define CMD_REBOOT        0x03
#define CMD_ENTER_DFU     0x04
#define CMD_APPLY_CONFIG  0x05
#define CMD_RESET_STATS   0x06
#define CMD_CLEAR_FAULT   0x07
#define CMD_RESET_PERF    0x08
#define CMD_MACRO_PLAY    0x09
#define CMD_MACRO_STOP    0x0A

/* DPI数值转枚举 */
static paw3395_dpi_e dpi_val_to_enum(uint16_t dpi_val)
{
    switch (dpi_val)
    {
        case 400:  return PAW3395_DPI_400;
        case 800:  return PAW3395_DPI_800;
        case 1600: return PAW3395_DPI_1600;
        case 3200: return PAW3395_DPI_3200;
        default:   return PAW3395_DPI_1600;
    }
}

void config_hid_init(void)
{
    /* Nothing for now. Placeholder if init needed. */
}

/* GET_REPORT 回调：只处理 Feature 报告（配置接口） */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_type;

    // 只处理 Feature 报告
    if (report_type != HID_REPORT_TYPE_FEATURE) return 0;

    LOG_INFO_PRINT("[CONFIG] 主机读取 Feature 报告: Report ID=%d, 请求长度=%d\n", report_id, reqlen);

    switch (report_id)
    {
        case REPORT_ID_CONFIG_BLOCK0:
        case REPORT_ID_CONFIG_BLOCK1:
        case REPORT_ID_CONFIG_BLOCK2:
        {
            const device_config_t* cfg = config_get();
            const uint8_t* cfg_bytes = (const uint8_t*)cfg;
            uint16_t cfg_size = sizeof(device_config_t);

            uint16_t offset;
            switch (report_id) {
                case REPORT_ID_CONFIG_BLOCK0: offset = 0; break;
                case REPORT_ID_CONFIG_BLOCK1: offset = CONFIG_BLOCK_SIZE; break;
                case REPORT_ID_CONFIG_BLOCK2: offset = CONFIG_BLOCK_SIZE * 2; break;
                default: return 0;
            }

            if (reqlen == 0) return 0;

            if (offset >= cfg_size) {
                /* 偏移超出，返回空数据（但不越界） */
                memset(buffer, 0, reqlen);
                return (reqlen < CONFIG_BLOCK_SIZE) ? reqlen : CONFIG_BLOCK_SIZE;
            }

            uint16_t remaining = cfg_size - offset;
            uint16_t copy_len = (reqlen < remaining) ? reqlen : remaining;
            if (copy_len > CONFIG_BLOCK_SIZE) copy_len = CONFIG_BLOCK_SIZE;

            /* 仅写入不超过 reqlen 的数据 */
            if (copy_len > 0) memcpy(buffer, cfg_bytes + offset, copy_len);

            if (copy_len < CONFIG_BLOCK_SIZE && reqlen > copy_len) {
                uint16_t fill_len = (reqlen < CONFIG_BLOCK_SIZE) ? reqlen : CONFIG_BLOCK_SIZE;
                memset(buffer + copy_len, 0, fill_len - copy_len);
                copy_len = fill_len;
            }

            return copy_len;
        }

        case REPORT_ID_DEVICE_INFO:
        {
            if (reqlen == 0) return 0;
            if (reqlen >= 1) buffer[0] = 1;
            if (reqlen >= 2) buffer[1] = 0;
            if (reqlen >= 3) buffer[2] = 0;
            if (reqlen >= 4) buffer[3] = 1;
            if (reqlen >= 5) buffer[4] = 0;
            if (reqlen >= 6) buffer[5] = (uint8_t)(sizeof(device_config_t) & 0xFF);
            if (reqlen >= 7) buffer[6] = (uint8_t)((sizeof(device_config_t) >> 8) & 0xFF);
            for (int i = 7; i < 32 && i < (int)reqlen; i++) buffer[i] = 0;
            return (reqlen > 32) ? 32 : reqlen;
        }

        case REPORT_ID_CONTROL:
        {
            if (reqlen < 1) return 0;
            buffer[0] = 0;
            return 1;
        }

        case REPORT_ID_KEY_STATS0:
        case REPORT_ID_KEY_STATS1:
        case REPORT_ID_KEY_STATS2:
        case REPORT_ID_KEY_STATS3:
        {
            int start_idx = (report_id - REPORT_ID_KEY_STATS0) * 16;
            int count = 16;
            if (start_idx + count > 64) count = 64 - start_idx;

            int data_len = count * 2;
            uint16_t to_write = (reqlen < (uint16_t)data_len) ? (uint16_t)reqlen : (uint16_t)data_len;

            for (int i = 0; i < count; i++) {
                int off = i * 2;
                if ((uint16_t)off >= to_write) break;
                uint32_t cnt = key_stats_get_count(start_idx + i);
                uint16_t val = (uint16_t)(cnt & 0xFFFF);
                buffer[off] = (uint8_t)(val & 0xFF);
                if ((uint16_t)(off + 1) < to_write) buffer[off + 1] = (uint8_t)((val >> 8) & 0xFF);
            }
            return to_write;
        }

        case REPORT_ID_MACRO_CONFIG:
        {
            if (reqlen < 2) return 0;
            uint8_t macro_id = s_macro_read_id;
            uint8_t block = s_macro_read_block;
            if (macro_id >= MACRO_MAX_COUNT) macro_id = 0;
            if (block > 2) block = 0;
            const macro_def_t* macro = macro_get(macro_id);
            buffer[0] = macro_id;
            buffer[1] = block;
            if (macro == NULL) {
                if (reqlen > 2) memset(&buffer[2], 0, (size_t)(reqlen - 2));
                return (reqlen > 62) ? 62 : reqlen;
            }
            uint16_t offset = block * 60;
            const uint8_t* macro_bytes = (const uint8_t*)macro;
            uint16_t copy_len = 60;
            if (offset + copy_len > sizeof(macro_def_t)) copy_len = (uint16_t)(sizeof(macro_def_t) - offset);
            uint16_t avail = (reqlen > 2) ? (uint16_t)(reqlen - 2) : 0;
            uint16_t write_len = (avail < copy_len) ? avail : copy_len;
            if (write_len > 0) memcpy(&buffer[2], macro_bytes + offset, write_len);
            if (avail > write_len) memset(&buffer[2 + write_len], 0, (size_t)(avail - write_len));
            LOG_DEBUG_PRINT("[宏] 读取宏 %d 块 %d，期望 %d，实际写入 %d\n", macro_id, block, copy_len, (write_len + 2));
            return (reqlen > 62) ? 62 : reqlen;
        }

        case REPORT_ID_PERF_SYSTEM:
        {
            perf_system_stat_t sys_stat;
            perf_get_system_stat(&sys_stat);
            if (reqlen == 0) return 0;
            memset(buffer, 0, reqlen);
            if (reqlen > 0) buffer[0] = (uint8_t)(sys_stat.cpu_usage & 0xFF);
            if (reqlen > 1) buffer[1] = (uint8_t)(sys_stat.loop_freq_hz & 0xFF);
            if (reqlen > 2) buffer[2] = (uint8_t)((sys_stat.loop_freq_hz >> 8) & 0xFF);
            if (reqlen > 3) buffer[3] = (uint8_t)(sys_stat.uptime_s & 0xFF);
            if (reqlen > 4) buffer[4] = (uint8_t)((sys_stat.uptime_s >> 8) & 0xFF);
            if (reqlen > 5) buffer[5] = (uint8_t)((sys_stat.uptime_s >> 16) & 0xFF);
            if (reqlen > 6) buffer[6] = (uint8_t)((sys_stat.uptime_s >> 24) & 0xFF);
            if (reqlen > 7) buffer[7] = perf_get_task_count();
            if (reqlen > 8) buffer[8] = sys_stat.cpu_usage_avg_10s;
            if (reqlen > 9) buffer[9] = sys_stat.cpu_usage_avg_30s;
            if (reqlen > 10) buffer[10] = (uint8_t)(sys_stat.loop_freq_avg_10s & 0xFF);
            if (reqlen > 11) buffer[11] = (uint8_t)((sys_stat.loop_freq_avg_10s >> 8) & 0xFF);
            return (reqlen > 62) ? 62 : reqlen;
        }

        case REPORT_ID_PERF_TASK:
        {
            uint8_t index = s_perf_task_index;
            perf_task_stat_t task_stat;
            bool valid = perf_get_task_stat(index, &task_stat);
            if (reqlen == 0) return 0;
            memset(buffer, 0, reqlen);
            if (reqlen > 0) buffer[0] = index;
            if (reqlen > 1) buffer[1] = valid ? 1 : 0;
            if (valid) {
                uint32_t avg_us = task_stat.count > 0 ? (task_stat.total_us / task_stat.count) : 0;
                if (reqlen > 2) buffer[2] = (uint8_t)(task_stat.count & 0xFF);
                if (reqlen > 3) buffer[3] = (uint8_t)((task_stat.count >> 8) & 0xFF);
                if (reqlen > 4) buffer[4] = (uint8_t)((task_stat.count >> 16) & 0xFF);
                if (reqlen > 5) buffer[5] = (uint8_t)((task_stat.count >> 24) & 0xFF);
                if (reqlen > 6) buffer[6] = (uint8_t)(task_stat.min_us & 0xFF);
                if (reqlen > 7) buffer[7] = (uint8_t)((task_stat.min_us >> 8) & 0xFF);
                if (reqlen > 8) buffer[8] = (uint8_t)((task_stat.min_us >> 16) & 0xFF);
                if (reqlen > 9) buffer[9] = (uint8_t)((task_stat.min_us >> 24) & 0xFF);
                if (reqlen > 10) buffer[10] = (uint8_t)(task_stat.max_us & 0xFF);
                if (reqlen > 11) buffer[11] = (uint8_t)((task_stat.max_us >> 8) & 0xFF);
                if (reqlen > 12) buffer[12] = (uint8_t)((task_stat.max_us >> 16) & 0xFF);
                if (reqlen > 13) buffer[13] = (uint8_t)((task_stat.max_us >> 24) & 0xFF);
                if (reqlen > 14) buffer[14] = (uint8_t)(avg_us & 0xFF);
                if (reqlen > 15) buffer[15] = (uint8_t)((avg_us >> 8) & 0xFF);
                if (reqlen > 16) buffer[16] = (uint8_t)((avg_us >> 16) & 0xFF);
                if (reqlen > 17) buffer[17] = (uint8_t)((avg_us >> 24) & 0xFF);
                if (reqlen > 18) buffer[18] = (uint8_t)(task_stat.last_us & 0xFF);
                if (reqlen > 19) buffer[19] = (uint8_t)((task_stat.last_us >> 8) & 0xFF);
                if (reqlen > 20) buffer[20] = (uint8_t)((task_stat.last_us >> 16) & 0xFF);
                if (reqlen > 21) buffer[21] = (uint8_t)((task_stat.last_us >> 24) & 0xFF);
                if (reqlen > 22) buffer[22] = task_stat.cpu_percent;
                if (reqlen > 23) buffer[23] = (uint8_t)(task_stat.overrun_count & 0xFF);
                if (reqlen > 24) buffer[24] = (uint8_t)((task_stat.overrun_count >> 8) & 0xFF);
                if (reqlen > 25) buffer[25] = (uint8_t)((task_stat.overrun_count >> 16) & 0xFF);
                if (reqlen > 26) buffer[26] = (uint8_t)((task_stat.overrun_count >> 24) & 0xFF);
                if (reqlen > 27) buffer[27] = (uint8_t)(task_stat.threshold_us & 0xFF);
                if (reqlen > 28) buffer[28] = (uint8_t)((task_stat.threshold_us >> 8) & 0xFF);
                if (reqlen > 29) buffer[29] = (uint8_t)((task_stat.threshold_us >> 16) & 0xFF);
                if (reqlen > 30) buffer[30] = (uint8_t)((task_stat.threshold_us >> 24) & 0xFF);
                if (task_stat.name != NULL && reqlen > 31) {
                    uint16_t max_name = (uint16_t)(reqlen - 31);
                    if (max_name > 31) max_name = 31;
                    uint8_t name_len = 0;
                    while (task_stat.name[name_len] != '\0' && name_len < (int)max_name - 1) {
                        buffer[31 + name_len] = (uint8_t)task_stat.name[name_len];
                        name_len++;
                    }
                    buffer[31 + name_len] = 0;
                } else if (reqlen > 31) {
                    buffer[31] = 0;
                }
            }
            return (reqlen > 62) ? 62 : reqlen;
        }

        case REPORT_ID_FAULT_INFO:
        {
            uint32_t log_count = fault_get_log_count();
            uint32_t total_count = fault_get_count();
            if (reqlen == 0) return 0;
            memset(buffer, 0, reqlen);
            if (reqlen > 0) buffer[0] = (uint8_t)(log_count & 0xFF);
            if (reqlen > 1) buffer[1] = (uint8_t)((log_count >> 8) & 0xFF);
            if (reqlen > 2) buffer[2] = (uint8_t)((log_count >> 16) & 0xFF);
            if (reqlen > 3) buffer[3] = (uint8_t)((log_count >> 24) & 0xFF);
            if (reqlen > 4) buffer[4] = (uint8_t)(total_count & 0xFF);
            if (reqlen > 5) buffer[5] = (uint8_t)((total_count >> 8) & 0xFF);
            if (reqlen > 6) buffer[6] = (uint8_t)((total_count >> 16) & 0xFF);
            if (reqlen > 7) buffer[7] = (uint8_t)((total_count >> 24) & 0xFF);
            return (reqlen > 62) ? 62 : reqlen;
        }

        case REPORT_ID_FAULT_LOG:
        {
            uint8_t index = s_fault_read_index;
            fault_log_entry_t entry;
            bool valid = fault_get_log(index, &entry);
            if (reqlen == 0) return 0;
            memset(buffer, 0, reqlen);
            if (reqlen > 0) buffer[0] = index;
            if (reqlen > 1) buffer[1] = valid ? 1 : 0;
            if (valid) {
                if (reqlen > 2) buffer[2] = (uint8_t)(entry.timestamp_ms & 0xFF);
                if (reqlen > 3) buffer[3] = (uint8_t)((entry.timestamp_ms >> 8) & 0xFF);
                if (reqlen > 4) buffer[4] = (uint8_t)((entry.timestamp_ms >> 16) & 0xFF);
                if (reqlen > 5) buffer[5] = (uint8_t)((entry.timestamp_ms >> 24) & 0xFF);
                if (reqlen > 6) buffer[6] = (uint8_t)entry.level;
                if (reqlen > 7) buffer[7] = entry.module_len;
                if (reqlen > 8) {
                    uint16_t max_mod = (uint16_t)(reqlen - 8);
                    if (max_mod > 32) max_mod = 32;
                    int module_len = entry.module_len;
                    if (module_len > (int)max_mod - 1) module_len = (int)max_mod - 1;
                    if (module_len < 0) module_len = 0;
                    if (module_len > 0) memcpy(&buffer[8], entry.module, (size_t)module_len);
                    if ((uint16_t)(8 + module_len) < reqlen) buffer[8 + module_len] = 0;
                }
                if (reqlen > 40) {
                    uint16_t max_msg = (uint16_t)(reqlen - 40);
                    if (max_msg > 22) max_msg = 22;
                    int msg_len = entry.msg_len;
                    if (msg_len > (int)max_msg - 1) msg_len = (int)max_msg - 1;
                    if (msg_len < 0) msg_len = 0;
                    if (msg_len > 0) memcpy(&buffer[40], entry.msg, (size_t)msg_len);
                    if ((uint16_t)(40 + msg_len) < reqlen) buffer[40 + msg_len] = 0;
                }
            }
            return (reqlen > 62) ? 62 : reqlen;
        }

        default:
            return 0;
    }
}

/* SET_REPORT 回调（Feature 写入/命令处理） */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void) instance;
    (void) report_type;

    if (report_type != HID_REPORT_TYPE_FEATURE) return;

    switch (report_id)
    {
        case REPORT_ID_CONFIG_BLOCK0:
        case REPORT_ID_CONFIG_BLOCK1:
        case REPORT_ID_CONFIG_BLOCK2:
        {
            uint8_t* cfg_bytes = (uint8_t*)&g_tmp_config;
            uint16_t cfg_size = sizeof(device_config_t);
            uint16_t offset;
            switch (report_id) {
                case REPORT_ID_CONFIG_BLOCK0: offset = 0; break;
                case REPORT_ID_CONFIG_BLOCK1: offset = CONFIG_BLOCK_SIZE; break;
                case REPORT_ID_CONFIG_BLOCK2: offset = CONFIG_BLOCK_SIZE * 2; break;
                default: break;
            }
            LOG_INFO_PRINT("[CONFIG] 收到配置块 %d (Report ID=%d, 偏移=%d, 长度=%d)\n", report_id - 5, report_id, offset, bufsize);
            if (offset < cfg_size) {
                uint16_t remaining = cfg_size - offset;
                uint16_t copy_len = (bufsize < remaining) ? bufsize : remaining;
                if (copy_len > CONFIG_BLOCK_SIZE) copy_len = CONFIG_BLOCK_SIZE;
                if (copy_len > 0) {
                    memcpy(cfg_bytes + offset, buffer, copy_len);
                    g_config_pending = true;
                }
                LOG_DEBUG_PRINT("[CONFIG] 配置块 %d 写入成功，复制 %d 字节\n", report_id - 5, copy_len);
            }
            break;
        }

        case REPORT_ID_CONTROL:
        {
            if (bufsize >= 1) {
                g_pending_cmd = buffer[0];
                g_pending_cmd_param = (bufsize >= 2) ? buffer[1] : 0;
                LOG_INFO_PRINT("[CONFIG] 收到控制命令: 0x%02X, 参数: 0x%02X\n", buffer[0], g_pending_cmd_param);
            }
            break;
        }

        case REPORT_ID_MACRO_CONFIG:
        {
            if (bufsize < 2) break;
            uint8_t macro_id = buffer[0];
            uint8_t block_raw = buffer[1];
            bool is_read_cmd = (block_raw & 0x80) != 0;
            uint8_t block = block_raw & 0x7F;
            LOG_INFO_PRINT("[宏] 收到宏配置: 宏ID=%d, 块=%d, 长度=%d, 读命令=%d\n", macro_id, block, bufsize, is_read_cmd);
            if (macro_id >= MACRO_MAX_COUNT) break;
            if (block > 2) break;
            s_macro_read_id = macro_id;
            s_macro_read_block = block;
            if (!is_read_cmd && bufsize > 2) {
                macro_def_t tmp_macro;
                const macro_def_t* current = macro_get(macro_id);
                if (current != NULL) memcpy(&tmp_macro, current, sizeof(macro_def_t));
                else { memset(&tmp_macro, 0, sizeof(macro_def_t)); tmp_macro.id = macro_id; }
                uint16_t offset = block * 60;
                uint8_t* macro_bytes = (uint8_t*)&tmp_macro;
                uint16_t copy_len = bufsize - 2;
                if (offset + copy_len > sizeof(macro_def_t)) copy_len = sizeof(macro_def_t) - offset;
                if (copy_len > 0) memcpy(macro_bytes + offset, &buffer[2], copy_len);
                tmp_macro.id = macro_id;
                if (tmp_macro.action_count > MACRO_MAX_ACTIONS) tmp_macro.action_count = MACRO_MAX_ACTIONS;
                macro_set(macro_id, &tmp_macro);
                s_macro_dirty = true;
                s_macro_dirty_time_us = time_us_32();
                LOG_DEBUG_PRINT("[宏] 写入宏配置: 宏ID=%d, 块=%d, 长度=%d\n", macro_id, block, copy_len);
            }
            break;
        }

        case REPORT_ID_FAULT_LOG:
        {
            if (bufsize >= 1) {
                s_fault_read_index = buffer[0];
                LOG_INFO_PRINT("[FAULT] 设置日志读取索引: %d\n", s_fault_read_index);
            }
            break;
        }

        case REPORT_ID_PERF_TASK:
        {
            if (bufsize >= 1) {
                s_perf_task_index = buffer[0];
                LOG_INFO_PRINT("[PERF] 设置任务读取索引: %d\n", s_perf_task_index);
            }
            break;
        }

        default:
            LOG_DEBUG_PRINT("[CONFIG] 未知 Report ID: %d\n", report_id);
            break;
    }
}

/* 周期性处理函数（替代原 hid_config_task） */
void hid_config_task(void)
{
    /* 处理宏配置的延迟保存 */
    if (s_macro_dirty) {
        uint32_t now = time_us_32();
        if (now - s_macro_dirty_time_us > 1000000U) {
            const device_config_t* current_cfg = config_get();
            if (current_cfg != NULL) {
                device_config_t new_cfg;
                memcpy(&new_cfg, current_cfg, sizeof(device_config_t));
                macro_save_to_config(new_cfg.macro_data, CONFIG_MACRO_DATA_SIZE);
                config_save(&new_cfg);
                LOG_INFO_PRINT("[宏] 配置已保存到Flash\n");
            }
            s_macro_dirty = false;
        }
    }

    /* 处理待写入的临时配置 */
    if (g_config_pending) {
        g_config_pending = false;
        if (g_tmp_config.magic == CONFIG_MAGIC) {
            /* 未来：应用到模块或校验 */
        }
    }

    /* 处理待执行的命令 */
    if (g_pending_cmd != 0) {
        uint8_t cmd = g_pending_cmd;
        g_pending_cmd = 0;
        switch (cmd) {
            case CMD_SAVE_CONFIG:
                if (g_tmp_config.magic == CONFIG_MAGIC) config_save(&g_tmp_config);
                break;
            case CMD_RESET_CONFIG:
                config_init();
                break;
            case CMD_REBOOT:
                LOG_INFO_PRINT("[CMD] 重启设备...\n");
                sleep_ms(10);
                watchdog_reboot(0, 0, 1);
                while (1) {};
                break;
            case CMD_ENTER_DFU:
                LOG_INFO_PRINT("[CMD] 进入 BOOTSEL 模式...\n");
                sleep_ms(10);
                rom_reset_usb_boot(0, 0);
                break;
            case CMD_APPLY_CONFIG:
                LOG_INFO_PRINT("[CONFIG] 收到应用配置命令\n");
                if (g_tmp_config.magic == CONFIG_MAGIC) {
                    int save_ret = config_save(&g_tmp_config);
                    if (save_ret == 0) {
                        LOG_INFO_PRINT("[CONFIG] ✅ 配置已保存到 Flash\n");
                        const paw3395_cfg_t *paw_cfg = board_get_paw3395_cfg();
                        if (paw_cfg != NULL) {
                            paw3395_dpi_e dpi_enum = dpi_val_to_enum(g_tmp_config.dpi);
                            int ret = paw3395_set_dpi(paw_cfg, dpi_enum);
                            if (ret == 0) LOG_INFO_PRINT("[CONFIG] ✅ DPI 已应用: %d\n", g_tmp_config.dpi);
                            else LOG_ERROR_PRINT("[CONFIG] ❌ DPI 应用失败，错误码: %d\n", ret);
                        }
                    } else {
                        LOG_ERROR_PRINT("[CONFIG] ❌ 配置保存到 Flash 失败\n");
                    }
                } else {
                    LOG_ERROR_PRINT("[CONFIG] ❌ 魔数错误: 0x%08X (期望 0x%08X)\n", g_tmp_config.magic, CONFIG_MAGIC);
                }
                break;
            case CMD_RESET_STATS:
                key_stats_reset();
                LOG_INFO_PRINT("[STATS] 按键统计已清零\n");
                break;
            case CMD_CLEAR_FAULT:
                fault_clear();
                LOG_INFO_PRINT("[FAULT] 错误日志已清除\n");
                break;
            case CMD_RESET_PERF:
                perf_reset();
                LOG_INFO_PRINT("[PERF] 性能统计已重置\n");
                break;
            case CMD_MACRO_PLAY:
            {
                uint8_t macro_id = g_pending_cmd_param;
                if (macro_id < MACRO_MAX_COUNT) {
                    bool ok = macro_trigger(macro_id);
                    LOG_INFO_PRINT("[MACRO] 播放宏 %d: %s\n", macro_id, ok ? "成功" : "失败");
                } else {
                    LOG_ERROR_PRINT("[MACRO] 无效的宏ID: %d\n", macro_id);
                }
                break;
            }
            case CMD_MACRO_STOP:
            {
                uint8_t macro_id = g_pending_cmd_param;
                if (macro_id < MACRO_MAX_COUNT) {
                    macro_stop(macro_id);
                    LOG_INFO_PRINT("[MACRO] 停止宏 %d\n", macro_id);
                } else {
                    macro_stop_all();
                    LOG_INFO_PRINT("[MACRO] 停止所有宏\n");
                }
                break;
            }
            default:
                break;
        }
    }
}

