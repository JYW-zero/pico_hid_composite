/*
 * tests/test_command_gate.c
 * HID 命令 / 报告授权闸门单元测试（功能角色分离，协议 v0.2）
 *
 * host 测试为全开工程态（FEATURE_* 默认 1、device_flags=0），验证：
 * - 消费者命令与能力查询命令一律放行；
 * - 诊断命令在能力位编入时放行；
 * - 未知命令放行（交回分发 switch 的 default，保持静默现状）；
 * - GET_REPORT 报告 -> 所需能力位映射正确，消费者报告返回 0（不限制）。
 * release 缺能力位返回 NOT_SUPPORTED / 出厂锁定返回 MFG_LOCKED 的路径，
 * 与本表同一判定逻辑，由全关/全关锁定固件构建与阶段2/3集成测试覆盖。
 */

#include "unity.h"
#include "protocol/command_gate.h"
#include "protocol/capability.h"
#include "protocol/config_hid.h"
#include "protocol/usb_descriptors.h"
#include <stdint.h>

/* 消费者公开命令：全开下放行 */
void test_gate_consumer_commands_allowed(void)
{
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_SAVE_CONFIG));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_APPLY_CONFIG));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_REBOOT));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_SWITCH_DPI));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_SWITCH_LAYER));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_MACRO_PLAY));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_UNLOCK_CONFIG));
}

/* bootloader 编入（host 主套件 features.h 默认 FEATURE_DFU_BOOTLOADER=1）：
 * 自定义 DFU(0x04) 放行；BOOTSEL UF2(0x14) 为芯片原生能力，任何形态都放行。
 * bootloader 未编入（release 矩阵 FEATURE_DFU_BOOTLOADER=0）时 0x04 回 NOT_SUPPORTED，
 * 由 build_mfg_lock_tests 套件（test_mfg_lock.c）验证。 */
void test_gate_dfu_and_bootsel_allowed_when_bootloader_compiled(void)
{
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_ENTER_DFU));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_ENTER_BOOTSEL));
}

/* 能力查询必须为 Public：配置锁定 / 未解锁时上位机也要能读能力位图 */
void test_gate_get_capability_info_is_public(void)
{
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK,
                            command_gate_check_command(CMD_GET_CAPABILITY_INFO));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK,
                            command_gate_check_command(CMD_GET_VERSION));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK,
                            command_gate_check_command(CMD_NEGOTIATE_VERSION));
}

/* 全开形态：诊断命令能力位齐备，放行（不应误返回 LOCKED/NOT_SUPPORTED） */
void test_gate_diag_commands_allowed_when_compiled_in(void)
{
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_RESET_STATS));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_RESET_PERF));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_SET_PERF_ENABLE));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_SET_PERF_THRESHOLD));
}

/* 工程态未出厂锁定（flags=0）：清除 Fault 放行；阶段4出厂锁定后才返回 DIAG_LOCKED */
void test_gate_clear_fault_allowed_engineering(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, capability_device_flags());
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(CMD_CLEAR_FAULT));
}

/* 未知命令放行回 default（保持原有“静默不回 ACK”行为，闸门不拦截） */
void test_gate_unknown_command_passes_through(void)
{
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(0x00));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(0x1F));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK, command_gate_check_command(0x7F));
}

/* 按键统计报告需要 CAP_KEY_STATS */
void test_gate_report_cap_keystats(void)
{
    uint64_t need = (1ULL << CAP_KEY_STATS);
    TEST_ASSERT_EQUAL_UINT64(need, command_gate_report_required_cap(REPORT_ID_KEY_STATS0));
    TEST_ASSERT_EQUAL_UINT64(need, command_gate_report_required_cap(REPORT_ID_KEY_STATS1));
    TEST_ASSERT_EQUAL_UINT64(need, command_gate_report_required_cap(REPORT_ID_KEY_STATS2));
    TEST_ASSERT_EQUAL_UINT64(need, command_gate_report_required_cap(REPORT_ID_KEY_STATS3));
}

/* 性能报告需要 CAP_PERF_MONITOR */
void test_gate_report_cap_perf(void)
{
    uint64_t need = (1ULL << CAP_PERF_MONITOR);
    TEST_ASSERT_EQUAL_UINT64(need, command_gate_report_required_cap(REPORT_ID_PERF_SYSTEM));
    TEST_ASSERT_EQUAL_UINT64(need, command_gate_report_required_cap(REPORT_ID_PERF_TASK));
}

/* 实时 raw 报告需要 CAP_DIAG_RAW_STATE */
void test_gate_report_cap_raw_state(void)
{
    TEST_ASSERT_EQUAL_UINT64((1ULL << CAP_DIAG_RAW_STATE),
                            command_gate_report_required_cap(REPORT_ID_KEY_STATE));
    TEST_ASSERT_EQUAL_UINT64((1ULL << CAP_DIAG_RAW_STATE),
                            command_gate_report_required_cap(REPORT_ID_JOYSTICK_STATE));
}

/* Fault 报告需要 CAP_FAULT_LOG（release 保留精简只读，该位仍置位） */
void test_gate_report_cap_fault(void)
{
    uint64_t need = (1ULL << CAP_FAULT_LOG);
    TEST_ASSERT_EQUAL_UINT64(need, command_gate_report_required_cap(REPORT_ID_FAULT_INFO));
    TEST_ASSERT_EQUAL_UINT64(need, command_gate_report_required_cap(REPORT_ID_FAULT_LOG));
}

/* 消费者 / 非诊断报告不做能力限制，返回 0 */
void test_gate_report_cap_public_reports_zero(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, command_gate_report_required_cap(REPORT_ID_DEVICE_INFO));
    TEST_ASSERT_EQUAL_UINT64(0, command_gate_report_required_cap(REPORT_ID_CONFIG_BLOCK0));
    TEST_ASSERT_EQUAL_UINT64(0, command_gate_report_required_cap(REPORT_ID_CONFIG_BLOCK1));
    TEST_ASSERT_EQUAL_UINT64(0, command_gate_report_required_cap(REPORT_ID_ACK));
    TEST_ASSERT_EQUAL_UINT64(0, command_gate_report_required_cap(REPORT_ID_MACRO_CONFIG));
}
