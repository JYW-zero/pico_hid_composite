/**
 * @file command_gate.c
 * @brief HID 命令 / 报告授权闸门实现（功能角色分离，协议 v0.2）
 */
#include "protocol/command_gate.h"
#include "protocol/capability.h"
#include "protocol/usb_descriptors.h"

/* 命令表：cmd -> {授权级别, 所需能力位(0=无能力要求)} */
typedef struct {
    uint8_t  cmd;
    uint8_t  level;        /* cmd_level_t */
    uint64_t required_cap; /* 0 = 不做能力裁剪 */
} cmd_gate_entry_t;

static const cmd_gate_entry_t cmd_gate_table[] = {
    /* ---- 消费者公开命令 ---- */
    { CMD_SAVE_CONFIG,             CMD_LEVEL_PUBLIC, 0 },
    { CMD_RESET_CONFIG,            CMD_LEVEL_PUBLIC, 0 },
    { CMD_REBOOT,                  CMD_LEVEL_PUBLIC, 0 },
    /* 自定义 in-app DFU 需 bootloader 随固件编入（CAP_DFU_UPGRADE bit10）；未编入时
     * 即使 raw HID 直接发 0x04 也回 NOT_SUPPORTED，不只是上位机隐藏按钮。
     * 芯片原生 BOOTSEL UF2（CMD_ENTER_BOOTSEL，见下）不依赖 bootloader，始终放行。 */
    { CMD_ENTER_DFU,               CMD_LEVEL_PUBLIC, (1ULL << CAP_DFU_UPGRADE) },
    { CMD_APPLY_CONFIG,            CMD_LEVEL_PUBLIC, 0 },
    { CMD_MACRO_PLAY,              CMD_LEVEL_PUBLIC, 0 },
    { CMD_MACRO_STOP,              CMD_LEVEL_PUBLIC, 0 },
    { CMD_SET_JOYSTICK_DZ_RT,      CMD_LEVEL_PUBLIC, 0 },
    { CMD_UNLOCK_CONFIG,           CMD_LEVEL_PUBLIC, 0 },
    { CMD_JOYSTICK_CALIBRATE,      CMD_LEVEL_PUBLIC, 0 },
    { CMD_SWITCH_DPI,              CMD_LEVEL_PUBLIC, 0 },
    { CMD_GET_VERSION,             CMD_LEVEL_PUBLIC, 0 },
    { CMD_NEGOTIATE_VERSION,       CMD_LEVEL_PUBLIC, 0 },
    { CMD_SWITCH_LAYER,            CMD_LEVEL_PUBLIC, 0 },
    { CMD_GET_CAPABILITY_INFO,     CMD_LEVEL_PUBLIC, 0 },
    { CMD_ENTER_BOOTSEL,           CMD_LEVEL_PUBLIC, 0 }, /* RP2350 原生 UF2，始终可用 */

    /* ---- 诊断命令：能力位缺失（release 未编入）即 NOT_SUPPORTED ---- */
    { CMD_RESET_STATS,             CMD_LEVEL_DIAG, (1ULL << CAP_KEY_STATS) },
    { CMD_CLEAR_FAULT,             CMD_LEVEL_DIAG, (1ULL << CAP_FAULT_LOG) },
    { CMD_RESET_PERF,              CMD_LEVEL_DIAG, (1ULL << CAP_PERF_MONITOR) },
    { CMD_SET_PERF_ENABLE,         CMD_LEVEL_DIAG, (1ULL << CAP_PERF_MONITOR) },
    { CMD_SET_PERF_THRESHOLD,      CMD_LEVEL_DIAG, (1ULL << CAP_PERF_MONITOR) },

    /* 制造命令 0x20-0x26 在阶段3加入，级别 CMD_LEVEL_MFG，闸门逻辑已就位 */
};

uint8_t command_gate_check_command(uint8_t cmd)
{
    const cmd_gate_entry_t *entry = 0;
    unsigned int i;

    for (i = 0; i < sizeof(cmd_gate_table) / sizeof(cmd_gate_table[0]); i++) {
        if (cmd_gate_table[i].cmd == cmd) {
            entry = &cmd_gate_table[i];
            break;
        }
    }

    /* 已预留但尚未实现的制造命令段 0x20-0x2F：显式回 NOT_SUPPORTED，
     * 避免 raw HID 探测时静默无响应、便于产线/售后排查。
     * 其余真正未知命令仍交回分发 switch 的 default，保持现状。 */
    if (entry == 0) {
        if (cmd >= 0x20U && cmd <= 0x2FU) {
            return CONFIG_HID_ERR_NOT_SUPPORTED;
        }
        return CONFIG_HID_OK;
    }

    /* 能力位检查：release 未编入对应模块时位图缺该位 -> NOT_SUPPORTED */
    if (entry->required_cap != 0 &&
        (capability_bitmap_runtime() & entry->required_cap) == 0) {
        return CONFIG_HID_ERR_NOT_SUPPORTED;
    }

    /* 制造级：已出厂锁定 / 未授权 */
    if (entry->level == CMD_LEVEL_MFG && !capability_mfg_factory_available()) {
        return CONFIG_HID_ERR_MFG_LOCKED;
    }

    /* 诊断级售后操作：release 保留只读 Fault，清除 Fault 属售后动作。
     * 已出厂锁定且本会话未售后解锁 -> DIAG_LOCKED（阶段4接挑战-应答）。 */
    if (entry->level == CMD_LEVEL_DIAG && cmd == CMD_CLEAR_FAULT) {
        uint8_t flags = capability_device_flags();
        if ((flags & DEVICE_FLAG_MFG_SHIPPED_LOCKED) &&
            !(flags & DEVICE_FLAG_MFG_SESSION_UNLOCKED)) {
            return CONFIG_HID_ERR_DIAG_LOCKED;
        }
    }

    return CONFIG_HID_OK;
}

uint64_t command_gate_report_required_cap(uint8_t report_id)
{
    switch (report_id) {
    /* 按键统计 */
    case REPORT_ID_KEY_STATS0:
    case REPORT_ID_KEY_STATS1:
    case REPORT_ID_KEY_STATS2:
    case REPORT_ID_KEY_STATS3:
        return (1ULL << CAP_KEY_STATS);

    /* 性能监控 */
    case REPORT_ID_PERF_SYSTEM:
    case REPORT_ID_PERF_TASK:
        return (1ULL << CAP_PERF_MONITOR);

    /* 实时 raw 诊断 */
    case REPORT_ID_KEY_STATE:
    case REPORT_ID_JOYSTICK_STATE:
        return (1ULL << CAP_DIAG_RAW_STATE);

    /* 错误日志（release 保留精简只读，能力位 CAP_FAULT_LOG 仍置位） */
    case REPORT_ID_FAULT_INFO:
    case REPORT_ID_FAULT_LOG:
        return (1ULL << CAP_FAULT_LOG);

    default:
        return 0; /* 消费者公开 / 非配置接口报告，不限制 */
    }
}
