/*
 * protocol/config_hid.c
 * HID 配置协议模块（从 main.c 提取）
 */

#include "features.h"
#include "protocol/config_hid.h"
#include "protocol/config_hid_helpers.h"
#include "protocol/config_lock.h"
#include "protocol/config_block_receiver.h"
#include "protocol/config_validator.h"
#include "protocol/command_confirm.h"
#include "app/log.h"
#include <string.h>
#include <stdio.h>
#include "usb_descriptors.h"
#include "protocol/capability.h"
#include "protocol/command_gate.h"
#include "board/config.h"
#include "board/board.h"
#include "app/macro.h"
#include "app/keymap.h"
#include "app/key_stats.h"
#include "middleware/crc32.h"
#include "middleware/fault.h"
#include "middleware/perf_monitor.h"
#include "middleware/shared_hw_data.h"
#include "middleware/watchdog.h"
#include "middleware/ipc.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "device/optical_sensor.h"
#include "device/dfu/dfu_metadata.h"
#include "pico/time.h"
#include "pico/bootrom.h"

/* ==================== 常量定义 ==================== */
/* 宏配置延迟保存时间（微秒）
 * 宏配置修改后等待此时间再写入 Flash，合并短时间内的多次修改
 * 200ms 平衡了写入次数（Flash寿命）和数据持久性（断电丢失窗口）
 * 原1秒延迟在高频宏编辑场景下丢失窗口过大
 */
#define MACRO_DIRTY_SAVE_DELAY_US  (200000U)

/* IPC 命令 ACK 超时（微秒）
 * Core0 发送命令给 Core1 后，等待 ACK 的超时时间
 * 10ms 足够覆盖 Core1 最坏情况下的命令处理延迟
 */
#define CONFIG_IPC_ACK_TIMEOUT_US   (10000U)

/* 配置复位/重启延迟（毫秒）
 * 触发看门狗复位前的延迟，确保 Flash 写入和日志输出完成
 * 100ms 足够覆盖 Flash 扇区擦除（~50ms）和页编程（~1ms）
 */
#define CONFIG_RESET_DELAY_MS       (100U)

/* DPI 配置范围（DPI 值）
 * 光学传感器支持的 DPI 范围，超出此范围的配置视为无效
 */
#define CONFIG_DPI_MIN              (100U)
#define CONFIG_DPI_MAX              (6400U)

/* 宏配置块字段定义：block_raw 字节的最高位表示读/写，低7位表示块号 */
#define MACRO_BLOCK_READ_FLAG       (0x80U)  /* 读命令标志位（最高位=1表示读） */
#define MACRO_BLOCK_INDEX_MASK      (0x7FU)  /* 块号掩码（低7位） */

/* CRC32 计算使用公共模块 middleware/crc32.h */
/* 日志宏使用公共模块 app/log.h */

/* 临时配置缓冲区 */
static device_config_t g_tmp_config = {0};
static volatile bool g_config_pending = false;
static volatile uint8_t g_pending_cmd = 0;
static volatile uint16_t g_pending_cmd_param = 0;

/* v3.1 协议：命令响应缓冲区（同步请求-响应模式）
 * 替代原异步 ACK Input Report，避免 ACK 丢失导致的命令超时问题
 * 格式: [status, cmd, sw_id, device_idx, param_low, param_high, reserved[2]]
 *   status=0 表示成功，非0为错误码（config_hid_error_t）
 *   sw_id 回显请求的 Software ID，用于匹配请求响应
 *   device_idx 回显请求的 Device Index
 * 上位机发送 Output 命令后，立即 GET_REPORT(24) 读取此缓冲区
 * 如果命令尚未处理完成，返回 CONFIG_HID_ERR_BUSY，上位机应重试
 */
static uint8_t g_control_response[8] = {0};
static volatile bool g_control_response_valid = false;

/* 设置命令响应（替代 config_hid_set_response）
 * 命令处理完成后调用此函数设置响应，上位机通过 GET_REPORT(24) 读取
 */
/* v3.1 协议：当前命令的 Device Index 和 Software ID（从命令解析时缓存，响应时回显） */
static volatile uint8_t g_current_device_idx = DEVICE_INDEX_DIRECT;
static volatile uint8_t g_current_sw_id = SW_ID_BROADCAST;

static void config_hid_set_response(uint8_t cmd, bool success, uint8_t error_code, uint16_t param)
{
    /* v3.1 响应格式: [status, cmd, sw_id, device_idx, param_low, param_high, reserved[2]] */
    g_control_response[RESP_OFFSET_STATUS] = success ? 0 : error_code;
    g_control_response[RESP_OFFSET_CMD] = cmd;
    g_control_response[RESP_OFFSET_SW_ID] = g_current_sw_id;        /* 回显 SW ID */
    g_control_response[RESP_OFFSET_DEVICE_IDX] = g_current_device_idx; /* 回显 Device Index */
    g_control_response[RESP_OFFSET_PARAM_LOW] = (uint8_t)(param & 0xFF);
    g_control_response[RESP_OFFSET_PARAM_HIGH] = (uint8_t)((param >> 8) & 0xFF);
    g_control_response[6] = 0;
    g_control_response[7] = 0;
    g_control_response_valid = true;
}

/* 配置块接收状态管理已移至 protocol/config_block_receiver.c */

/* 重置配置块接收状态（USB 断开时调用，防止残留块与新会话组合导致配置损坏） */
void config_hid_reset_blocks_received(void)
{
    config_block_receiver_reset();
}

/* 通过 IPC 命令将 DPI 设置请求发送给 Core1 执行
 * 避免 Core0 在中断上下文中直接操作 SPI 总线与 Core1 竞争导致死锁
 * 返回：0=成功，非0=失败（FIFO满或ACK超时）
 */
static int config_hid_set_dpi_via_ipc(uint16_t dpi_val, bool is_raw)
{
    uint32_t cmd;
    if (is_raw) {
        cmd = IPC_MAKE_CMD(IPC_CMD_SET_DPI_RAW, dpi_val);
    } else {
        cmd = IPC_MAKE_CMD(IPC_CMD_SET_DPI, dpi_val);
    }

    /* 非阻塞发送：FIFO 满时立即失败 */
    if (!multicore_fifo_push_timeout_us(cmd, 0)) {
        LOG_ERROR_PRINT("[CONFIG] ❌ DPI设置失败：FIFO满\n");
        return -1;
    }

    /* 读取并丢弃 Core1 回复的 ACK（带超时，避免阻塞） */
    uint32_t ack_val = 0;
    if (!multicore_fifo_pop_timeout_us(CONFIG_IPC_ACK_TIMEOUT_US, &ack_val)) {
        LOG_WARN_PRINT("[CONFIG] ⚠️ DPI设置ACK超时（Core1可能忙）\n");
        /* 超时不视为失败，DPI会在Core1下次处理命令时生效 */
    }

    return 0;
}

/* 宏配置读取索引 */
static uint8_t s_macro_read_id = 0;
static uint8_t s_macro_read_block = 0;

/* 错误日志读取索引 */
static uint8_t s_fault_read_index = 0;

#if FEATURE_PERF_MONITOR
/* 性能监控 - 任务读取索引 */
static uint8_t s_perf_task_index = 0;
#endif

/* CONFIG_EXT2 当前子块索引（SET_REPORT设置，GET_REPORT返回对应子块数据） */
static uint8_t s_config_ext2_subblock = 0;

/* 宏配置脏标志（需要保存到Flash） */
static bool s_macro_dirty = false;
static uint32_t s_macro_dirty_time_us = 0;

/* 控制命令码（与 config_hid.h 保持一致） */
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
#define CMD_SET_PERF_ENABLE 0x0B  /* 设置性能监控开关：data[1]=1开启,0关闭 */
#define CMD_SET_JOYSTICK_DZ_RT 0x0C  /* 实时设置摇杆死区（不写Flash）：data[1-2]=死区值(小端) */
#define CMD_UNLOCK_CONFIG   0x0D  /* 解锁配置写入：需连续发送3次(5秒内)才能解锁 */
#define CMD_JOYSTICK_CALIBRATE 0x0E  /* 触发摇杆中心点校准：data[1-2]=采样次数(小端，默认50) */
#define CMD_SWITCH_DPI        0x0F  /* 循环切换 DPI 档位：data[1]=目标档位(0-3, 0xFF=循环下一档) */
#define CMD_GET_VERSION       0x10  /* 获取版本信息（返回 ACK 中携带版本） */
#define CMD_NEGOTIATE_VERSION 0x11  /* 版本协商（参数：上位机协议版本） */
#define CMD_SWITCH_LAYER      0x12  /* 切换按键层（参数：0-3 层号） */

/* ACK/NACK 发送和 DPI 转换已移至 protocol/config_hid_helpers.c */
/* 配置锁定机制已移至 protocol/config_lock.c */
/* DFU/Reboot 命令确认机制已移至 protocol/command_confirm.c */

/* DPI数值转枚举已移至 protocol/config_hid_helpers.c */

void config_hid_init(void)
{
    /* 初始化配置锁定机制（默认锁定状态） */
    config_lock_init();
    /* 初始化配置块接收状态（清零接收位图） */
    config_block_receiver_init();
    /* 初始化命令确认机制（清零 DFU/Reboot 确认计数） */
    command_confirm_init();
}

/* ============================================================================
 * GET_REPORT 处理函数（从 tud_hid_get_report_cb 拆分，每个函数职责单一）
 * ========================================================================== */

/* 处理配置块读取（REPORT_ID_CONFIG_BLOCK0/1/2） */
static uint16_t get_report_config_block(uint8_t report_id, uint8_t* buffer, uint16_t reqlen)
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

/* 处理设备信息读取（REPORT_ID_DEVICE_INFO） */
static uint16_t get_report_device_info(uint8_t* buffer, uint16_t reqlen)
{
    /* 设备信息报告（32字节，含 report_id）
     * byte[0] = Report ID
     * byte[1-2] = 固件版本（bcdDevice）
     * byte[3-4] = 配置版本
     * byte[5-6] = 协议版本（高字节=主版本，低字节=次版本）
     * byte[7-8] = 配置结构总大小
     * byte[9] = 支持的按键层数
     * byte[10] = 支持的宏数量
     * byte[11] = 最大支持 DPI
     * byte[12] = 支持的能力数量
     * byte[13-20] = 能力位图（64位，每位代表一个能力是否存在）
     * byte[21] = device_flags（v0.2 新增：制造/会话状态位 DEVICE_FLAG_*）
     * byte[22-31] = 预留（老上位机按短报告解析，自动忽略）
     */
    config_hid_device_info_t info;
    memset(&info, 0, sizeof(info));
    info.report_id = REPORT_ID_DEVICE_INFO;
    info.firmware_version = FIRMWARE_VERSION_BCD;  /* 固件版本（统一从 version.h 引用） */
    info.config_version = CONFIG_VERSION;
    info.protocol_version = CONFIG_HID_PROTOCOL_VERSION;
    info.config_size = (uint16_t)sizeof(device_config_t);
    info.layer_count = 4;  /* 支持 4 层按键映射 */
    info.macro_count = MACRO_MAX_COUNT;
    info.max_dpi = 64;  /* 最大 DPI 档位索引（实际值 6400） */

    /* 能力位图：按编译期 FEATURE_* 开关 + 制造状态运行时拼装（功能角色分离）。
     * 能力协商的核心：上位机据此位图判断功能是否可用，不再靠版本号推断；
     * 固件命令/报告闸门也以此为判据。实现见 capability.c。
     */
    info.capability_count = CAP_COUNT;
    info.capability_bitmap = capability_bitmap_runtime();
    info.device_flags = capability_device_flags();

    uint16_t to_write = (reqlen < (uint16_t)(sizeof(info) - 1)) ? reqlen : (uint16_t)(sizeof(info) - 1);
    /* 注意：tud_hid_get_report_cb 的 buffer 不应包含 Report ID（协议层自动添加），
     * 从 firmware_version 字段开始复制，跳过 report_id（1字节） */
    memcpy(buffer, (const uint8_t*)&info + 1, to_write);
    return to_write;
}

/* 处理控制命令读取（REPORT_ID_CONTROL，已废弃） */
static uint16_t get_report_control(uint8_t* buffer, uint16_t reqlen)
{
    /* v3.0 协议：REPORT_ID_CONTROL(7) 已改为 Output Report（仅用于主机发送命令）
     * GET_REPORT 不再返回有效数据，命令响应通过 REPORT_ID_ACK(24) 读取
     */
    if (reqlen < 1) return 0;
    buffer[0] = 0;
    return 1;
}

/* 处理命令响应读取（REPORT_ID_ACK） */
static uint16_t get_report_ack(uint8_t* buffer, uint16_t reqlen)
{
    /* v3.1 协议：命令响应 Feature Report（同步请求-响应模式）
     * 上位机发送 Output 命令后，立即 GET_REPORT(24) 读取执行结果
     * 格式: [status, cmd, sw_id, device_idx, param_low, param_high, reserved[2]]
     *   status=0 表示成功，非0为错误码
     * 读取后清除有效标志，防止重复读取旧响应
     *
     * 竞态条件防护：如果命令尚未处理完成（g_control_response_valid=false），
     * 返回 BUSY 状态，上位机应重试 GET_REPORT，避免读到旧响应或全零初始响应。
     */
    uint16_t to_write = (reqlen < 8) ? reqlen : 8;

    if (!g_control_response_valid) {
        /* 响应尚未准备好，返回 BUSY 状态（CONFIG_HID_ERR_BUSY=8）
         * cmd 字段回显当前待处理命令，sw_id/device_idx 回显当前缓存值 */
        buffer[RESP_OFFSET_STATUS] = CONFIG_HID_ERR_BUSY;
        buffer[RESP_OFFSET_CMD] = g_pending_cmd;
        buffer[RESP_OFFSET_SW_ID] = g_current_sw_id;
        buffer[RESP_OFFSET_DEVICE_IDX] = g_current_device_idx;
        buffer[RESP_OFFSET_PARAM_LOW] = 0;
        buffer[RESP_OFFSET_PARAM_HIGH] = 0;
        buffer[6] = 0;
        buffer[7] = 0;
        return to_write;
    }

    memcpy(buffer, g_control_response, to_write);
    g_control_response_valid = false;
    return to_write;
}

#if FEATURE_KEY_STATS
/* 处理按键统计读取（REPORT_ID_KEY_STATS0-3） */
static uint16_t get_report_key_stats(uint8_t report_id, uint8_t* buffer, uint16_t reqlen)
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
#endif /* FEATURE_KEY_STATS */

/* 处理宏配置读取（REPORT_ID_MACRO_CONFIG） */
static uint16_t get_report_macro_config(uint8_t* buffer, uint16_t reqlen)
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

#if FEATURE_PERF_MONITOR
/* 处理性能系统统计读取（REPORT_ID_PERF_SYSTEM） */
static uint16_t get_report_perf_system(uint8_t* buffer, uint16_t reqlen)
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

/* 处理性能任务统计读取（REPORT_ID_PERF_TASK） */
static uint16_t get_report_perf_task(uint8_t* buffer, uint16_t reqlen)
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
#endif /* FEATURE_PERF_MONITOR */

/* 处理错误日志信息读取（REPORT_ID_FAULT_INFO） */
static uint16_t get_report_fault_info(uint8_t* buffer, uint16_t reqlen)
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

/* 处理错误日志读取（REPORT_ID_FAULT_LOG） */
static uint16_t get_report_fault_log(uint8_t* buffer, uint16_t reqlen)
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

#if FEATURE_RAW_DIAG
/* 处理实时按键状态读取（REPORT_ID_KEY_STATE） */
static uint16_t get_report_key_state(uint8_t* buffer, uint16_t reqlen)
{
    /* 实时按键状态：64位bitmap，bit=1表示按下 */
    uint64_t keys = shared_hw_get_keys();
    /* keypad_spi读取的是bit=0表示按下，这里转换为bit=1表示按下 */
    uint64_t pressed = ~keys;
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);
    uint8_t max_bytes = (reqlen > 8) ? 8 : (uint8_t)reqlen;
    for (uint8_t i = 0; i < max_bytes; i++) {
        buffer[i] = (uint8_t)((pressed >> (i * 8)) & 0xFF);
    }
    return (reqlen > 8) ? 8 : reqlen;
}

/* 处理实时摇杆状态读取（REPORT_ID_JOYSTICK_STATE） */
static uint16_t get_report_joystick_state(uint8_t* buffer, uint16_t reqlen)
{
    /* 实时摇杆状态：x, y, btn */
    int16_t x, y;
    bool btn;
    shared_hw_get_joystick(&x, &y, &btn);
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);
    if (reqlen > 0) buffer[0] = (uint8_t)(int8_t)x;   /* 有符号字节 */
    if (reqlen > 1) buffer[1] = (uint8_t)(int8_t)y;   /* 有符号字节 */
    if (reqlen > 2) buffer[2] = btn ? 1 : 0;
    return (reqlen > 8) ? 8 : reqlen;
}
#endif /* FEATURE_RAW_DIAG */

/* 处理摇杆校准状态读取（REPORT_ID_JOYSTICK_CALIB） */
static uint16_t get_report_joystick_calib(uint8_t* buffer, uint16_t reqlen)
{
    /* 摇杆校准状态查询：
     * byte[0] = 校准状态（0=空闲,1=校准中,2=完成）
     * byte[1-2] = center_x（小端，ADC值）
     * byte[3-4] = center_y（小端，ADC值）
     * byte[5] = calibrated（0=未校准,1=已校准）
     * byte[6-7] = 死区值（小端，ADC原始值）
     * byte[8-9] = 灵敏度（小端，定点数1.0=1000）
     */
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);

    uint8_t calib_state = shared_hw_get_joystick_calibration_state();
    uint16_t center_x = 2048, center_y = 2048;
    uint8_t calibrated = 0;
    shared_hw_get_joystick_calibration_data(&center_x, &center_y, &calibrated);

    const device_config_t *cfg = config_get();

    if (reqlen > 0) buffer[0] = calib_state;
    if (reqlen > 1) buffer[1] = (uint8_t)(center_x & 0xFF);
    if (reqlen > 2) buffer[2] = (uint8_t)((center_x >> 8) & 0xFF);
    if (reqlen > 3) buffer[3] = (uint8_t)(center_y & 0xFF);
    if (reqlen > 4) buffer[4] = (uint8_t)((center_y >> 8) & 0xFF);
    if (reqlen > 5) buffer[5] = calibrated;
    if (reqlen > 6) buffer[6] = (uint8_t)(cfg->joystick_deadzone & 0xFF);
    if (reqlen > 7) buffer[7] = (uint8_t)((cfg->joystick_deadzone >> 8) & 0xFF);
    if (reqlen > 8) buffer[8] = (uint8_t)(cfg->joystick_sensitivity & 0xFF);
    if (reqlen > 9) buffer[9] = (uint8_t)((cfg->joystick_sensitivity >> 8) & 0xFF);

    return (reqlen > 10) ? 10 : reqlen;
}

/* 处理配置扩展字段读取（REPORT_ID_CONFIG_EXT） */
static uint16_t get_report_config_ext(uint8_t* buffer, uint16_t reqlen)
{
    /* v3/v4/v5 配置扩展字段：偏移1326+，共18字节
     * 与 usb_descriptors.c 中 REPORT_ID_CONFIG_EXT 描述符声明的 18 字节一致，
     * 与上位机 DeviceService.cs 中读写的 18 字节一致。
     * 包含: joystick_invert_x(1), joystick_invert_y(1), encoder_steps(1),
     *       encoder_scroll_speed(1), joystick_sensitivity(2), reserved2(2),
     *       length(2), write_count(4), joystick_center_x(2), joystick_center_y(2),
     *       joystick_calibrated(1), reserved3(1) = 18 字节
     */
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);
    uint8_t* ext_bytes = (uint8_t*)&g_tmp_config + 1326;
    uint16_t copy_len = (reqlen < 18) ? reqlen : 18;
    memcpy(buffer, ext_bytes, copy_len);
    return (reqlen > 18) ? 18 : reqlen;
}

/* 处理配置扩展块2读取（REPORT_ID_CONFIG_EXT2） */
static uint16_t get_report_config_ext2(uint8_t* buffer, uint16_t reqlen)
{
    /* v6/v7配置扩展块2（子块索引机制）
     * GET_REPORT: 返回当前子块的数据（子块索引用SET_REPORT设置）
     * SET_REPORT: byte[0]=子块索引，设置当前子块并可写入数据
     * 子块定义:
     *   0 = v6字段 (偏移1346, 14字节)
     *   1 = layer2_keymap[0..31] (偏移1360, 32字节)
     *   2 = layer2_keymap[32..63] (偏移1392, 32字节)
     *   3 = layer3_keymap[0..31] (偏移1424, 32字节)
     *   4 = layer3_keymap[32..63] (偏移1456, 32字节)
     */
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);

    uint16_t offset = 0;
    uint16_t data_len = 0;

    switch (s_config_ext2_subblock)
    {
        case 0: offset = 1346; data_len = 14; break;  // v6字段
        case 1: offset = 1360; data_len = 32; break;  // layer2_keymap[0..31]
        case 2: offset = 1392; data_len = 32; break;  // layer2_keymap[32..63]
        case 3: offset = 1424; data_len = 32; break;  // layer3_keymap[0..31]
        case 4: offset = 1456; data_len = 32; break;  // layer3_keymap[32..63]
        default: return 0;  // 无效子块索引
    }

    uint16_t copy_len = (reqlen < data_len) ? reqlen : data_len;
    if (offset + copy_len <= sizeof(device_config_t))
    {
        memcpy(buffer, (uint8_t*)&g_tmp_config + offset, copy_len);
    }
    LOG_INFO_PRINT("[CONFIG] GET CONFIG_EXT2 sub_block=%d offset=%d len=%d\n", s_config_ext2_subblock, offset, copy_len);
    return (reqlen > copy_len) ? copy_len : reqlen;
}

/* GET_REPORT 回调：只处理 Feature 报告（配置接口） */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    /* 只处理配置接口（instance 1）的 Feature 报告 */
    if (instance != ITF_NUM_HID_CONFIG) return 0;
    if (report_type != HID_REPORT_TYPE_FEATURE) return 0;

    /* 报告能力闸门：缺少对应诊断能力位（release 未编入）时返回空报告，
     * 防止绕过上位机直接 raw GET_REPORT 读取诊断数据 */
    {
        uint64_t required_cap = command_gate_report_required_cap(report_id);
        if (required_cap != 0 && (capability_bitmap_runtime() & required_cap) == 0) {
            return 0;
        }
    }

    /* 临时关闭频繁打印，避免干扰调试
    LOG_INFO_PRINT("[CONFIG] 主机读取 Feature 报告: Report ID=%d, 请求长度=%d\n", report_id, reqlen);
    */

    /* 分发到对应的处理函数（每个函数职责单一，便于维护和测试） */
    switch (report_id)
    {
        case REPORT_ID_CONFIG_BLOCK0:
        case REPORT_ID_CONFIG_BLOCK1:
        case REPORT_ID_CONFIG_BLOCK2:
            return get_report_config_block(report_id, buffer, reqlen);

        case REPORT_ID_DEVICE_INFO:
            return get_report_device_info(buffer, reqlen);

        case REPORT_ID_CONTROL:
            return get_report_control(buffer, reqlen);

        case REPORT_ID_ACK:
            return get_report_ack(buffer, reqlen);

#if FEATURE_KEY_STATS
        case REPORT_ID_KEY_STATS0:
        case REPORT_ID_KEY_STATS1:
        case REPORT_ID_KEY_STATS2:
        case REPORT_ID_KEY_STATS3:
            return get_report_key_stats(report_id, buffer, reqlen);
#endif

        case REPORT_ID_MACRO_CONFIG:
            return get_report_macro_config(buffer, reqlen);

#if FEATURE_PERF_MONITOR
        case REPORT_ID_PERF_SYSTEM:
            return get_report_perf_system(buffer, reqlen);

        case REPORT_ID_PERF_TASK:
            return get_report_perf_task(buffer, reqlen);
#endif

        case REPORT_ID_FAULT_INFO:
            return get_report_fault_info(buffer, reqlen);

        case REPORT_ID_FAULT_LOG:
            return get_report_fault_log(buffer, reqlen);

#if FEATURE_RAW_DIAG
        case REPORT_ID_KEY_STATE:
            return get_report_key_state(buffer, reqlen);

        case REPORT_ID_JOYSTICK_STATE:
            return get_report_joystick_state(buffer, reqlen);
#endif /* FEATURE_RAW_DIAG */

        case REPORT_ID_JOYSTICK_CALIB:
            return get_report_joystick_calib(buffer, reqlen);

        case REPORT_ID_CONFIG_EXT:
            return get_report_config_ext(buffer, reqlen);

        case REPORT_ID_CONFIG_EXT2:
            return get_report_config_ext2(buffer, reqlen);

        default:
            return 0;
    }
}

/* ============================================================================
 * SET_REPORT 处理函数（从 tud_hid_set_report_cb 拆分，每个函数职责单一）
 * ========================================================================== */

/* 处理配置块写入（REPORT_ID_CONFIG_BLOCK0/1/2） */
static void set_report_config_block(uint8_t report_id, uint8_t const* buffer, uint16_t bufsize)
{
    if (config_lock_is_locked()) {
        LOG_ERROR_PRINT("[CONFIG] ❌ 配置已锁定，拒绝写入配置块\n");
        return;
    }
    uint8_t* cfg_bytes = (uint8_t*)&g_tmp_config;
    uint16_t cfg_size = sizeof(device_config_t);
    uint16_t offset;
    uint8_t block_bit;
    config_block_receiver_get_block_info(report_id, &offset, &block_bit);
    LOG_INFO_PRINT("[CONFIG] 收到配置块 %d (Report ID=%d, 偏移=%d, 长度=%d)\n", report_id - 5, report_id, offset, bufsize);
    if (offset < cfg_size) {
        uint16_t remaining = cfg_size - offset;
        uint16_t copy_len = (bufsize < remaining) ? bufsize : remaining;
        if (copy_len > CONFIG_BLOCK_SIZE) copy_len = CONFIG_BLOCK_SIZE;
        if (copy_len > 0) {
            memcpy(cfg_bytes + offset, buffer, copy_len);
            g_config_pending = true;
            config_block_receiver_mark_block(block_bit);
        }
        LOG_DEBUG_PRINT("[CONFIG] 配置块 %d 写入成功，复制 %d 字节\n", report_id - 5, copy_len);
    }
}

/* 处理控制命令（REPORT_ID_CONTROL） */
static void set_report_control(uint8_t const* buffer, uint16_t bufsize)
{
    if (bufsize < 3) return;
    g_current_device_idx = buffer[CMD_OFFSET_DEVICE_IDX];
    g_current_sw_id = buffer[CMD_OFFSET_SW_ID];
    uint8_t cmd = buffer[CMD_OFFSET_CMD];

    if (cmd == CMD_UNLOCK_CONFIG) {
        uint8_t progress = config_lock_attempt_unlock();
        if (progress >= CONFIG_LOCK_UNLOCK_REQUIRED_ATTEMPTS) {
            LOG_INFO_PRINT("[SECURITY] ✅ 配置已解锁（30秒后自动锁定）\n");
            config_hid_set_response(CMD_UNLOCK_CONFIG, true, CONFIG_HID_OK, 1);
        } else {
            LOG_INFO_PRINT("[SECURITY] 解锁进度: %d/%d\n", progress, CONFIG_LOCK_UNLOCK_REQUIRED_ATTEMPTS);
            config_hid_set_response(CMD_UNLOCK_CONFIG, true, CONFIG_HID_OK, progress);
        }
        return;
    }

    if (config_lock_is_locked() && !config_lock_is_command_allowed(cmd)) {
        LOG_ERROR_PRINT("[SECURITY] ❌ 配置已锁定，拒绝命令 0x%02X\n", cmd);
        return;
    }

    config_lock_notify_write();
    g_pending_cmd = cmd;
    if (bufsize >= 5) {
        g_pending_cmd_param = (uint16_t)buffer[CMD_OFFSET_PARAM_LOW] | ((uint16_t)buffer[CMD_OFFSET_PARAM_HIGH] << 8);
    } else if (bufsize >= 4) {
        g_pending_cmd_param = buffer[CMD_OFFSET_PARAM_LOW];
    } else {
        g_pending_cmd_param = 0;
    }

    if (cmd == CMD_GET_CAPABILITY_INFO) {
        LOG_DEBUG_PRINT("[CONFIG] cmd: 0x%02X, param: 0x%04X, dev_idx: 0x%02X, sw_id: %d\n",
                         cmd, g_pending_cmd_param, g_current_device_idx, g_current_sw_id);
    } else {
        LOG_INFO_PRINT("[CONFIG] cmd: 0x%02X, param: 0x%04X, dev_idx: 0x%02X, sw_id: %d\n",
                       cmd, g_pending_cmd_param, g_current_device_idx, g_current_sw_id);
    }
}

/* 处理宏配置写入（REPORT_ID_MACRO_CONFIG） */
static void set_report_macro_config(uint8_t const* buffer, uint16_t bufsize)
{
    if (bufsize < 2) return;
    uint8_t macro_id = buffer[0];
    uint8_t block_raw = buffer[1];
    bool is_read_cmd = (block_raw & MACRO_BLOCK_READ_FLAG) != 0;
    uint8_t block = block_raw & MACRO_BLOCK_INDEX_MASK;

    if (!is_read_cmd && config_lock_is_locked()) {
        LOG_ERROR_PRINT("[SECURITY] ❌ 配置已锁定，拒绝写入宏配置\n");
        return;
    }
    if (!is_read_cmd) config_lock_notify_write();

    LOG_INFO_PRINT("[宏] 收到宏配置: 宏ID=%d, 块=%d, 长度=%d, 读命令=%d\n", macro_id, block, bufsize, is_read_cmd);
    if (macro_id >= MACRO_MAX_COUNT || block > 2) return;
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
}

/* 处理错误日志索引设置（REPORT_ID_FAULT_LOG） */
static void set_report_fault_log(uint8_t const* buffer, uint16_t bufsize)
{
    /* 能力闸门：未编入 Fault 模块时忽略索引选择 */
    if ((capability_bitmap_runtime() & (1ULL << CAP_FAULT_LOG)) == 0) return;
    if (bufsize >= 1) s_fault_read_index = buffer[0];
}

#if FEATURE_PERF_MONITOR
/* 处理性能任务索引设置（REPORT_ID_PERF_TASK） */
static void set_report_perf_task(uint8_t const* buffer, uint16_t bufsize)
{
    /* 能力闸门：未编入性能监控时忽略索引选择 */
    if ((capability_bitmap_runtime() & (1ULL << CAP_PERF_MONITOR)) == 0) return;
    if (bufsize >= 1) s_perf_task_index = buffer[0];
}
#endif /* FEATURE_PERF_MONITOR */

/* 处理配置扩展字段写入（REPORT_ID_CONFIG_EXT） */
static void set_report_config_ext(uint8_t const* buffer, uint16_t bufsize)
{
    if (config_lock_is_locked()) {
        LOG_ERROR_PRINT("[SECURITY] ❌ 配置已锁定，拒绝写入扩展字段\n");
        return;
    }
    if (bufsize > 0 && buffer != NULL) {
        uint8_t* ext_bytes = (uint8_t*)&g_tmp_config + 1326;
        uint16_t copy_len = (bufsize < 18) ? bufsize : 18;
        memcpy(ext_bytes, buffer, copy_len);
        LOG_INFO_PRINT("[CONFIG] 收到配置扩展字段 (%d字节)\n", copy_len);
    }
}

/* 处理配置扩展块2写入（REPORT_ID_CONFIG_EXT2） */
static void set_report_config_ext2(uint8_t const* buffer, uint16_t bufsize)
{
    if (bufsize < 1 || buffer == NULL) return;
    uint8_t sub_block = buffer[0];
    if (sub_block > 4) {
        LOG_ERROR_PRINT("[CONFIG] ❌ CONFIG_EXT2 无效子块索引: %d\n", sub_block);
        return;
    }
    s_config_ext2_subblock = sub_block;

    if (bufsize > 1) {
        if (config_lock_is_locked()) {
            LOG_ERROR_PRINT("[SECURITY] ❌ 配置已锁定，拒绝写入 CONFIG_EXT2 子块 %d\n", sub_block);
            return;
        }
        uint16_t offset = 0, data_len = 0;
        switch (sub_block) {
            case 0: offset = 1346; data_len = 14; break;
            case 1: offset = 1360; data_len = 32; break;
            case 2: offset = 1392; data_len = 32; break;
            case 3: offset = 1424; data_len = 32; break;
            case 4: offset = 1456; data_len = 32; break;
        }
        uint16_t copy_len = (bufsize - 1 < data_len) ? (bufsize - 1) : data_len;
        if (offset + copy_len <= sizeof(device_config_t)) {
            memcpy((uint8_t*)&g_tmp_config + offset, buffer + 1, copy_len);
            LOG_INFO_PRINT("[CONFIG] 收到 CONFIG_EXT2 子块 %d (偏移=%d, 写入=%d字节)\n", sub_block, offset, copy_len);
        }
    } else {
        LOG_INFO_PRINT("[CONFIG] 设置 CONFIG_EXT2 子块索引: %d\n", sub_block);
    }
}

/* SET_REPORT 回调（Feature 写入/Output 命令处理） */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    /* 只处理配置接口（instance 1） */
    if (instance != ITF_NUM_HID_CONFIG) return;

    /* v3.0 协议：支持 Feature Report（配置写入）和 Output Report（命令发送，中断传输）
     * Output Report(Report ID=7) 用于高速命令发送，走 USB 中断传输
     */
    if (report_type != HID_REPORT_TYPE_FEATURE && report_type != HID_REPORT_TYPE_OUTPUT) return;

    /* 防御性校验：Feature Report 最大 64 字节（含 Report ID），异常大的 bufsize 直接拒绝 */
    if (bufsize > 64) {
        LOG_ERROR_PRINT("[SECURITY] ❌ 异常的 Feature Report 长度: %d（最大64）\n", bufsize);
        return;
    }
    if (buffer == NULL) return;

    /* 分发到对应的处理函数（每个函数职责单一，便于维护和测试） */
    switch (report_id)
    {
        case REPORT_ID_CONFIG_BLOCK0:
        case REPORT_ID_CONFIG_BLOCK1:
        case REPORT_ID_CONFIG_BLOCK2:
            set_report_config_block(report_id, buffer, bufsize);
            break;

        case REPORT_ID_CONTROL:
            set_report_control(buffer, bufsize);
            break;

        case REPORT_ID_MACRO_CONFIG:
            set_report_macro_config(buffer, bufsize);
            break;

        case REPORT_ID_FAULT_LOG:
            set_report_fault_log(buffer, bufsize);
            break;

#if FEATURE_PERF_MONITOR
        case REPORT_ID_PERF_TASK:
            set_report_perf_task(buffer, bufsize);
            break;
#endif

        case REPORT_ID_CONFIG_EXT:
            set_report_config_ext(buffer, bufsize);
            break;

        case REPORT_ID_CONFIG_EXT2:
            set_report_config_ext2(buffer, bufsize);
            break;

        default:
            LOG_DEBUG_PRINT("[CONFIG] 未知 Report ID: %d\n", report_id);
            break;
    }
}

/* ============================================================================
 * 命令处理函数（从 hid_config_task 拆分，每个函数职责单一）
 * ========================================================================== */

/* 处理保存配置命令 */
static void handle_cmd_save_config(void)
{
    if (config_validator_check_magic(&g_tmp_config)) {
        int ret = config_save(&g_tmp_config);
        config_hid_set_response(CMD_SAVE_CONFIG, ret == 0,
                            ret == 0 ? CONFIG_HID_OK : CONFIG_HID_ERR_FLASH_WRITE, 0);
    } else {
        config_hid_set_response(CMD_SAVE_CONFIG, false, CONFIG_HID_ERR_CRC_MISMATCH, 0);
    }
}

/* 处理恢复出厂配置命令 */
static void handle_cmd_reset_config(void)
{
    config_reset_default();
    LOG_INFO_PRINT("[CONFIG] 已恢复出厂默认配置，即将重启...\n");
    fault_record(FAULT_LEVEL_WARN, "config", "factory reset triggered by command");
    uint32_t reset_start = to_ms_since_boot(get_absolute_time());
    while ((to_ms_since_boot(get_absolute_time()) - reset_start) < CONFIG_RESET_DELAY_MS) {
        watchdog_update();
    }
    watchdog_reboot(0, 0, 1);
    while (1) {};
}

/* 处理重启设备命令 */
static void handle_cmd_reboot(void)
{
    uint8_t progress = command_confirm_attempt_reboot();
    if (progress >= COMMAND_CONFIRM_REBOOT_REQUIRED) {
        LOG_INFO_PRINT("[CMD] 重启设备（已确认%d次）...\n", COMMAND_CONFIRM_REBOOT_REQUIRED);
        sleep_ms(10);
        watchdog_reboot(0, 0, 1);
        while (1) {};
    } else {
        LOG_INFO_PRINT("[CMD] 重启确认: %d/%d（5秒内连续发送）\n", progress, COMMAND_CONFIRM_REBOOT_REQUIRED);
    }
}

/* 处理进入DFU模式命令 */
static void handle_cmd_enter_dfu(void)
{
    uint8_t progress = command_confirm_attempt_dfu();
    if (progress >= COMMAND_CONFIRM_DFU_REQUIRED) {
        LOG_INFO_PRINT("[CMD] 进入 DFU 升级模式（已确认%d次）...\n", COMMAND_CONFIRM_DFU_REQUIRED);
        sleep_ms(10);
        dfu_set_command(DFU_CMD_ENTER_DFU);
        watchdog_reboot(0, 0, CONFIG_RESET_DELAY_MS);
        while (1) {}
    } else {
        LOG_INFO_PRINT("[CMD] DFU确认: %d/%d（5秒内连续发送）\n", progress, COMMAND_CONFIRM_DFU_REQUIRED);
    }
}

/* 处理应用配置命令（最复杂的命令，包含配置验证、合并、保存、DPI应用） */
static void handle_cmd_apply_config(void)
{
    LOG_INFO_PRINT("[CONFIG] 收到应用配置命令\n");
    config_hid_error_t apply_err = CONFIG_HID_OK;

    if (!config_block_receiver_is_complete()) {
        LOG_ERROR_PRINT("[CONFIG] ❌ 配置块不完整: 0x%02X (期望 0x%02X)\n",
                        config_block_receiver_get_status(), CONFIG_BLOCKS_ALL);
        apply_err = CONFIG_HID_ERR_INCOMPLETE;
    } else {
        config_block_receiver_reset();
        if (!config_validator_check_magic(&g_tmp_config)) {
            LOG_ERROR_PRINT("[CONFIG] ❌ 魔数错误: 0x%08X (期望 0x%08X)\n",
                            g_tmp_config.magic, CONFIG_MAGIC);
            apply_err = CONFIG_HID_ERR_CRC_MISMATCH;
        } else if (g_tmp_config.dpi < CONFIG_DPI_MIN || g_tmp_config.dpi > CONFIG_DPI_MAX) {
            LOG_ERROR_PRINT("[CONFIG] ❌ DPI 超出范围: %d (允许%d-%d)\n",
                            g_tmp_config.dpi, CONFIG_DPI_MIN, CONFIG_DPI_MAX);
            apply_err = CONFIG_HID_ERR_INVALID_PARAM;
        } else {
            g_tmp_config.dpi = (g_tmp_config.dpi / 25) * 25;
            const device_config_t* current_cfg = config_get();
            device_config_t merged;
            if (current_cfg != NULL) {
                memcpy(&merged, current_cfg, sizeof(device_config_t));
            } else {
                memset(&merged, 0, sizeof(device_config_t));
            }
            config_validator_merge_config(&merged, &g_tmp_config);
            watchdog_feed_layer(WDG_LAYER_DEVICE);
            int save_ret = config_save(&merged);
            if (save_ret == 0) {
                LOG_INFO_PRINT("[CONFIG] ✅ 配置已保存到 Flash (灵敏度=%d, 反转X=%d, 反转Y=%d)\n",
                               merged.joystick_sensitivity, merged.joystick_invert_x, merged.joystick_invert_y);
                const optical_sensor_cfg_t *paw_cfg = board_get_optical_sensor_cfg();
                if (paw_cfg != NULL) {
                    int ret;
                    uint8_t slot = merged.current_dpi_slot;
                    if (slot > 3) slot = 0;
                    uint16_t dpi_val = merged.dpi_slot[slot];
                    if (dpi_val == 0) dpi_val = merged.dpi;
                    if (dpi_val == 400 || dpi_val == 800 || dpi_val == 1600 || dpi_val == 3200) {
                        optical_sensor_dpi_e dpi_enum = config_hid_dpi_val_to_enum(dpi_val);
                        ret = config_hid_set_dpi_via_ipc((uint16_t)dpi_enum, false);
                    } else {
                        ret = config_hid_set_dpi_via_ipc(dpi_val, true);
                    }
                    if (ret == 0) LOG_INFO_PRINT("[CONFIG] ✅ DPI 已应用: %d (档位%d)\n", dpi_val, slot);
                    else LOG_ERROR_PRINT("[CONFIG] ❌ DPI 应用失败，错误码: %d\n", ret);
                }
                apply_err = CONFIG_HID_OK;
            } else {
                LOG_ERROR_PRINT("[CONFIG] ❌ 配置保存到 Flash 失败\n");
                apply_err = CONFIG_HID_ERR_FLASH_WRITE;
            }
            watchdog_feed_layer(WDG_LAYER_DEVICE);
        }
    }

    /* 失败时重置临时配置，确保 g_tmp_config 始终与 Flash 一致
     * 避免部分更新的配置被后续命令读取，导致数据不一致
     */
    if (apply_err != CONFIG_HID_OK) {
        const device_config_t* current = config_get();
        if (current != NULL) {
            memcpy(&g_tmp_config, current, sizeof(device_config_t));
        }
        config_block_receiver_reset();
        LOG_WARN_PRINT("[CONFIG] ⚠️ 应用配置失败(错误码=%d)，已重置临时配置\n", apply_err);
    }

    config_hid_set_response(CMD_APPLY_CONFIG, apply_err == CONFIG_HID_OK, apply_err, 0);
}

#if FEATURE_KEY_STATS
/* 处理重置按键统计命令 */
static void handle_cmd_reset_stats(void)
{
    key_stats_reset();
    LOG_INFO_PRINT("[STATS] 按键统计已清零\n");
    config_hid_set_response(CMD_RESET_STATS, true, CONFIG_HID_OK, 0);
}
#endif /* FEATURE_KEY_STATS */

#if FEATURE_FAULT_LOG
/* 处理清除错误日志命令 */
static void handle_cmd_clear_fault(void)
{
    fault_clear();
    LOG_INFO_PRINT("[FAULT] 错误日志已清除\n");
    config_hid_set_response(CMD_CLEAR_FAULT, true, CONFIG_HID_OK, 0);
}
#endif /* FEATURE_FAULT_LOG */

#if FEATURE_PERF_MONITOR
/* 处理重置性能统计命令 */
static void handle_cmd_reset_perf(void)
{
    perf_reset();
    LOG_INFO_PRINT("[PERF] 性能统计已重置\n");
    config_hid_set_response(CMD_RESET_PERF, true, CONFIG_HID_OK, 0);
}
#endif /* FEATURE_PERF_MONITOR */

/* 处理播放宏命令 */
static void handle_cmd_macro_play(void)
{
    uint8_t macro_id = g_pending_cmd_param;
    if (macro_id < MACRO_MAX_COUNT) {
        bool ok = macro_trigger(macro_id);
        LOG_INFO_PRINT("[MACRO] 播放宏 %d: %s\n", macro_id, ok ? "成功" : "失败");
        config_hid_set_response(CMD_MACRO_PLAY, ok,
                            ok ? CONFIG_HID_OK : CONFIG_HID_ERR_BUSY, macro_id);
    } else {
        LOG_ERROR_PRINT("[MACRO] 无效的宏ID: %d\n", macro_id);
        config_hid_set_response(CMD_MACRO_PLAY, false, CONFIG_HID_ERR_INVALID_PARAM, macro_id);
    }
}

/* 处理停止宏命令 */
static void handle_cmd_macro_stop(void)
{
    uint8_t macro_id = g_pending_cmd_param;
    if (macro_id < MACRO_MAX_COUNT) {
        macro_stop(macro_id);
        LOG_INFO_PRINT("[MACRO] 停止宏 %d\n", macro_id);
    } else {
        macro_stop_all();
        LOG_INFO_PRINT("[MACRO] 停止所有宏\n");
    }
    config_hid_set_response(CMD_MACRO_STOP, true, CONFIG_HID_OK, 0);
}

#if FEATURE_PERF_MONITOR
/* 处理设置性能监控开关命令 */
static void handle_cmd_set_perf_enable(void)
{
    bool enable = (g_pending_cmd_param != 0);
    perf_set_enabled(enable);
    if (enable) perf_reset();
    LOG_INFO_PRINT("[PERF] 性能监控: %s\n", enable ? "开启" : "关闭");
    config_hid_set_response(CMD_SET_PERF_ENABLE, true, CONFIG_HID_OK, enable ? 1 : 0);
}

/* 处理设置性能监控任务超时阈值命令
 * 参数格式（16位小端）：
 *   高字节（bit 8-15）：任务索引（0-15，对应 perf_monitor 中的任务）
 *   低字节（bit 0-7）：阈值（0-255，单位100微秒，0=禁用告警）
 *   最大阈值：255 × 100us = 25500us = 25.5ms
 * 响应参数：高字节=任务索引，低字节=实际设置的阈值/100
 */
static void handle_cmd_set_perf_threshold(void)
{
    uint8_t task_index = (uint8_t)((g_pending_cmd_param >> 8) & 0x0F);  /* 只用低4位，0-15 */
    uint8_t threshold_raw = (uint8_t)(g_pending_cmd_param & 0xFF);        /* 0-255，单位100us */
    uint32_t threshold_us = (uint32_t)threshold_raw * 100U;                /* 转换为微秒 */

    /* 验证任务索引有效性 */
    if (task_index >= PERF_MAX_TASKS) {
        LOG_ERROR_PRINT("[PERF] ❌ 无效任务索引: %d (最大%d)\n", task_index, PERF_MAX_TASKS - 1);
        config_hid_set_response(CMD_SET_PERF_THRESHOLD, false, CONFIG_HID_ERR_INVALID_PARAM, 0);
        return;
    }

    /* 设置阈值（0=禁用告警） */
    perf_set_threshold(task_index, threshold_us);

    /* Persist threshold to config (only first 8 tasks) */
    if (task_index < 8) {
        const device_config_t* current_cfg = config_get();
        if (current_cfg != NULL) {
            device_config_t new_cfg;
            memcpy(&new_cfg, current_cfg, sizeof(device_config_t));
            new_cfg.perf_thresholds[task_index] = threshold_raw;
            int save_ret = config_save(&new_cfg);
            if (save_ret != 0) {
                LOG_ERROR_PRINT("[PERF] threshold save failed: %d\n", save_ret);
            } else {
                LOG_INFO_PRINT("[PERF] task%d threshold saved to config\n", task_index);
            }
        }
    }

    if (threshold_us == 0) {
        LOG_INFO_PRINT("[PERF] 任务%d 超时告警已禁用\n", task_index);
    } else {
        LOG_INFO_PRINT("[PERF] 任务%d 超时阈值设置为: %lu us (%lu ms)\n",
                       task_index, (unsigned long)threshold_us, (unsigned long)(threshold_us / 1000U));
    }

    /* 响应参数：高字节=任务索引，低字节=阈值/100 */
    uint16_t resp_param = (uint16_t)((task_index << 8) | threshold_raw);
    config_hid_set_response(CMD_SET_PERF_THRESHOLD, true, CONFIG_HID_OK, resp_param);
}
#endif /* FEATURE_PERF_MONITOR */

/* 处理实时设置摇杆死区命令 */
static void handle_cmd_set_joystick_dz_rt(void)
{
    uint16_t deadzone = (uint16_t)g_pending_cmd_param;
    if (deadzone > 4095) deadzone = 4095;
    uint32_t cmd = IPC_MAKE_CMD(IPC_CMD_SET_JOYSTICK_DZ, deadzone);
    if (!multicore_fifo_push_timeout_us(cmd, 0)) {
        LOG_ERROR_PRINT("[JOYSTICK] FIFO 满，跳过死区设置\n");
        config_hid_set_response(CMD_SET_JOYSTICK_DZ_RT, false, CONFIG_HID_ERR_BUSY, deadzone);
        return;
    }
    uint32_t ack_val = 0;
    bool ack_received = multicore_fifo_pop_timeout_us(CONFIG_IPC_ACK_TIMEOUT_US, &ack_val);
    if (!ack_received) {
        LOG_WARN_PRINT("[JOYSTICK] 死区设置ACK超时（Core1可能忙）\n");
    }
    LOG_INFO_PRINT("[JOYSTICK] 实时死区设置: %u (已发送)\n", deadzone);
    config_hid_set_response(CMD_SET_JOYSTICK_DZ_RT, true, CONFIG_HID_OK, deadzone);
}

/* 处理摇杆校准命令 */
static void handle_cmd_joystick_calibrate(void)
{
    uint16_t sample_count = (uint16_t)g_pending_cmd_param;
    if (sample_count == 0) sample_count = 50;
    if (sample_count > 200) sample_count = 200;
    if (shared_hw_trigger_joystick_calibration(sample_count)) {
        LOG_INFO_PRINT("[JOYSTICK] 校准触发成功，采样次数: %u\n", sample_count);
        config_hid_set_response(CMD_JOYSTICK_CALIBRATE, true, CONFIG_HID_OK, sample_count);
    } else {
        LOG_ERROR_PRINT("[JOYSTICK] 校准触发失败（可能正在校准中）\n");
        config_hid_set_response(CMD_JOYSTICK_CALIBRATE, false, CONFIG_HID_ERR_BUSY, sample_count);
    }
}

/* 处理切换DPI档位命令 */
static void handle_cmd_switch_dpi(void)
{
    uint8_t target_slot = (uint8_t)(g_pending_cmd_param & 0xFF);
    const device_config_t *current_cfg = config_get();
    uint8_t new_slot = current_cfg->current_dpi_slot;
    if (target_slot == 0xFF) {
        new_slot = (current_cfg->current_dpi_slot + 1) % 4;
    } else if (target_slot < 4) {
        new_slot = target_slot;
    }
    if (new_slot != current_cfg->current_dpi_slot) {
        device_config_t new_cfg;
        memcpy(&new_cfg, current_cfg, sizeof(device_config_t));
        new_cfg.current_dpi_slot = new_slot;
        config_save(&new_cfg);
        const optical_sensor_cfg_t *opt_cfg = board_get_optical_sensor_cfg();
        if (opt_cfg != NULL) {
            uint16_t dpi_val = new_cfg.dpi_slot[new_slot];
            if (dpi_val == 0) dpi_val = new_cfg.dpi;
            int ret;
            if (dpi_val == 400 || dpi_val == 800 || dpi_val == 1600 || dpi_val == 3200) {
                optical_sensor_dpi_e dpi_enum = config_hid_dpi_val_to_enum(dpi_val);
                ret = config_hid_set_dpi_via_ipc((uint16_t)dpi_enum, false);
            } else {
                ret = config_hid_set_dpi_via_ipc(dpi_val, true);
            }
            if (ret == 0)
                LOG_INFO_PRINT("[CONFIG] ✅ DPI 切换到档位%d: %d DPI\n", new_slot, dpi_val);
            else
                LOG_ERROR_PRINT("[CONFIG] ❌ DPI 应用失败，错误码: %d\n", ret);
        }
    } else {
        LOG_INFO_PRINT("[CONFIG] DPI 已在档位%d\n", new_slot);
    }
    config_hid_set_response(CMD_SWITCH_DPI, true, CONFIG_HID_OK, new_slot);
}

/* 处理获取版本信息命令 */
static void handle_cmd_get_version(void)
{
    LOG_INFO_PRINT("[CONFIG] 版本查询: 协议=%d.%d, 配置=0x%04X\n",
                   CONFIG_HID_PROTOCOL_VERSION_MAJOR,
                   CONFIG_HID_PROTOCOL_VERSION_MINOR,
                   CONFIG_VERSION);
    config_hid_set_response(CMD_GET_VERSION, true, CONFIG_HID_OK,
                        CONFIG_HID_PROTOCOL_VERSION);
}

/* 处理版本协商命令 */
static void handle_cmd_negotiate_version(void)
{
    uint16_t host_version = g_pending_cmd_param;
    uint8_t host_major = (uint8_t)(host_version >> 8);
    uint8_t host_minor = (uint8_t)(host_version & 0xFF);
    LOG_INFO_PRINT("[CONFIG] 版本协商: 上位机=%d.%d, 固件=%d.%d\n",
                   host_major, host_minor,
                   CONFIG_HID_PROTOCOL_VERSION_MAJOR,
                   CONFIG_HID_PROTOCOL_VERSION_MINOR);
    if (host_major != CONFIG_HID_PROTOCOL_VERSION_MAJOR) {
        LOG_ERROR_PRINT("[CONFIG] ❌ 版本不兼容: 上位机主版本=%d, 固件主版本=%d\n",
                        host_major, CONFIG_HID_PROTOCOL_VERSION_MAJOR);
        config_hid_set_response(CMD_NEGOTIATE_VERSION, false,
                            CONFIG_HID_ERR_VERSION_MISMATCH,
                            CONFIG_HID_PROTOCOL_VERSION);
    } else {
        config_hid_set_response(CMD_NEGOTIATE_VERSION, true, CONFIG_HID_OK,
                            CONFIG_HID_PROTOCOL_VERSION);
    }
}

/* 处理切换按键层命令 */
static void handle_cmd_switch_layer(void)
{
    uint8_t target_layer = (uint8_t)(g_pending_cmd_param & 0xFF);
    if (target_layer >= 4) {
        LOG_ERROR_PRINT("[CONFIG] ❌ 无效层号: %d\n", target_layer);
        config_hid_set_response(CMD_SWITCH_LAYER, false,
                            CONFIG_HID_ERR_INVALID_PARAM, 0);
    } else {
        keymap_set_active_layer(target_layer);
        LOG_INFO_PRINT("[CONFIG] ✅ 切换到层 %d\n", target_layer);
        config_hid_set_response(CMD_SWITCH_LAYER, true, CONFIG_HID_OK, target_layer);
    }
}

/* 处理获取能力信息命令 */
static void handle_cmd_get_capability_info(void)
{
    uint8_t cap_id = (uint8_t)(g_pending_cmd_param & 0xFF);
    static const uint16_t capability_versions[CAP_COUNT] = {
        [CAP_KEYBOARD_BASIC]   = (1 << 8) | 0,
        [CAP_MOUSE_BASIC]      = (1 << 8) | 0,
        [CAP_JOYSTICK]         = (1 << 8) | 0,
        [CAP_ENCODER]          = (1 << 8) | 0,
        [CAP_KEYMAP]           = (2 << 8) | 0,
        [CAP_MACRO]            = (1 << 8) | 0,
        [CAP_CONFIG_STORAGE]   = (2 << 8) | 0,
        [CAP_PERF_MONITOR]     = (1 << 8) | 0,
        [CAP_FAULT_LOG]        = (1 << 8) | 0,
        [CAP_KEY_STATS]        = (1 << 8) | 0,
        [CAP_DFU_UPGRADE]      = (1 << 8) | 0,
        [CAP_NKRO]             = (1 << 8) | 0,
        [CAP_CONSUMER]         = (1 << 8) | 0,
        [CAP_GAMEPAD]          = (1 << 8) | 0,
        [CAP_DPI_SLOTS]        = (1 << 8) | 0,
        [CAP_JOYSTICK_CALIB]   = (1 << 8) | 0,
        [CAP_DIAG_RAW_STATE]   = (1 << 8) | 0,
        [CAP_DIAG_DEBUG_UART]  = (1 << 8) | 0,
        [CAP_MFG_FACTORY_TEST] = (1 << 8) | 0,
        [CAP_MFG_PROVISION]    = (1 << 8) | 0,
        /* bit18-23 预留，版本保持 0 */
    };
    if (cap_id >= CAP_COUNT) {
        LOG_ERROR_PRINT("[CAP] invalid cap_id: %d\n", cap_id);
        config_hid_set_response(CMD_GET_CAPABILITY_INFO, false,
                            CONFIG_HID_ERR_INVALID_PARAM, 0);
    } else {
        uint16_t version = capability_versions[cap_id];
        uint8_t major = (uint8_t)(version >> 8);
        uint8_t minor = (uint8_t)(version & 0xFF);
        LOG_DEBUG_PRINT("[CAP] cap%d version: v%d.%d\n", cap_id, major, minor);
        config_hid_set_response(CMD_GET_CAPABILITY_INFO, true,
                            CONFIG_HID_OK, version);
    }
}

/* 处理进入BOOTSEL模式命令 */
static void handle_cmd_enter_bootsel(void)
{
    uint8_t progress = command_confirm_attempt_bootsel();
    if (progress >= COMMAND_CONFIRM_BOOTSEL_REQUIRED) {
        LOG_INFO_PRINT("[CMD] 进入官方 BOOTSEL UF2 模式（已确认%d次）...\n", COMMAND_CONFIRM_BOOTSEL_REQUIRED);
        sleep_ms(10);
        rom_reset_usb_boot(0, 0);
        while (1) { }
    } else {
        LOG_INFO_PRINT("[CMD] BOOTSEL确认: %d/%d（5秒内连续发送）\n", progress, COMMAND_CONFIRM_BOOTSEL_REQUIRED);
        config_hid_set_response(CMD_ENTER_BOOTSEL, true, CONFIG_HID_OK, progress);
    }
}

/* 周期性处理函数（替代原 hid_config_task） */
void hid_config_task(void)
{
    /* 配置锁定：解锁后30秒无操作自动重新锁定 */
    if (config_lock_check_auto_relock()) {
        LOG_INFO_PRINT("[SECURITY] 🔒 配置已自动锁定（超时）\n");
    }

    /* 处理宏配置的延迟保存（使用安全写入服务，双核同步） */
    if (s_macro_dirty) {
        uint32_t now = time_us_32();
        if (now - s_macro_dirty_time_us > MACRO_DIRTY_SAVE_DELAY_US) {
            const device_config_t* current_cfg = config_get();
            if (current_cfg != NULL) {
                /* 使用 static 缓冲区避免栈溢出（device_config_t 约1330字节） */
                static device_config_t s_macro_save_buf;
                memcpy(&s_macro_save_buf, current_cfg, sizeof(device_config_t));
                macro_save_to_config(s_macro_save_buf.macro_data, CONFIG_MACRO_DATA_SIZE);
                config_save(&s_macro_save_buf);
                LOG_INFO_PRINT("[宏] 配置已保存到Flash\n");
            }
            s_macro_dirty = false;
        }
    }

    /* 处理待写入的临时配置 */
    if (g_config_pending) {
        g_config_pending = false;
        if (config_validator_check_magic(&g_tmp_config)) {
            /* 未来：应用到模块或校验 */
        }
    }

    /* 处理待执行的命令（分发到对应的处理函数） */
    if (g_pending_cmd != 0) {
        uint8_t cmd = g_pending_cmd;
        g_pending_cmd = 0;

        /* 固件侧命令闸门：能力位 / 授权级别校验。
         * 与 config_lock 的“配置写锁”正交（写锁在 set_report 入口已检查）。
         * 拒绝时回明确 NACK，避免只靠上位机隐藏按钮被 raw HID 绕过。 */
        uint8_t gate_err = command_gate_check_command(cmd);
        if (gate_err != CONFIG_HID_OK) {
            LOG_INFO_PRINT("[GATE] 命令 0x%02X 被拒绝，错误码=%d\n", cmd, gate_err);
            config_hid_set_response(cmd, false, gate_err, 0);
        } else {
            switch (cmd) {
            case CMD_SAVE_CONFIG:        handle_cmd_save_config(); break;
            case CMD_RESET_CONFIG:       handle_cmd_reset_config(); break;
            case CMD_REBOOT:             handle_cmd_reboot(); break;
            case CMD_ENTER_DFU:          handle_cmd_enter_dfu(); break;
            case CMD_APPLY_CONFIG:       handle_cmd_apply_config(); break;
#if FEATURE_KEY_STATS
            case CMD_RESET_STATS:        handle_cmd_reset_stats(); break;
#endif
#if FEATURE_FAULT_LOG
            case CMD_CLEAR_FAULT:        handle_cmd_clear_fault(); break;
#endif
#if FEATURE_PERF_MONITOR
            case CMD_RESET_PERF:         handle_cmd_reset_perf(); break;
#endif
            case CMD_MACRO_PLAY:         handle_cmd_macro_play(); break;
            case CMD_MACRO_STOP:         handle_cmd_macro_stop(); break;
#if FEATURE_PERF_MONITOR
            case CMD_SET_PERF_ENABLE:    handle_cmd_set_perf_enable(); break;
#endif
            case CMD_SET_JOYSTICK_DZ_RT: handle_cmd_set_joystick_dz_rt(); break;
            case CMD_JOYSTICK_CALIBRATE: handle_cmd_joystick_calibrate(); break;
            case CMD_SWITCH_DPI:         handle_cmd_switch_dpi(); break;
            case CMD_GET_VERSION:        handle_cmd_get_version(); break;
            case CMD_NEGOTIATE_VERSION:  handle_cmd_negotiate_version(); break;
            case CMD_SWITCH_LAYER:       handle_cmd_switch_layer(); break;
            case CMD_GET_CAPABILITY_INFO: handle_cmd_get_capability_info(); break;
            case CMD_ENTER_BOOTSEL:      handle_cmd_enter_bootsel(); break;
#if FEATURE_PERF_MONITOR
            case CMD_SET_PERF_THRESHOLD: handle_cmd_set_perf_threshold(); break;
#endif
            default: break;
            }
        }
    }
}

