/*
 * tests/test_mfg_lock.c
 * 量产固件（release 裁剪宏 + FEATURE_MFG_DEFAULT_LOCKED=1）制造门控单元测试
 *
 * 独立可执行（自带 Unity main）：capability.c 的制造可用性是编译期开关决定的，
 * 无法在与 dev 态同一二进制内切换，因此用 build_mfg_lock_tests.bat 以 release
 * 裁剪宏单独编译。验证量产固件：
 *   - 制造能力默认锁定（HID 制造命令在固件侧即被拒绝，不只是上位机藏按钮）
 *   - CAP_MFG_FACTORY_TEST / CAP_MFG_PROVISION 不上报
 *   - device_flags 置 DEVICE_FLAG_MFG_SHIPPED_LOCKED
 *   - 消费者基础能力仍在
 *   - 诊断位符合 release 矩阵（perf/keystats/raw/uart 关，精简 fault 保留）
 */

#include "unity.h"
#include "protocol/capability.h"
#include "protocol/command_gate.h"
#include "protocol/config_hid.h"
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

/* 量产固件上电默认锁定制造能力 */
static void test_mfg_unavailable_when_default_locked(void)
{
    TEST_ASSERT_FALSE(capability_mfg_factory_available());
}

/* 锁定时制造能力位不得上报 */
static void test_mfg_bits_cleared_in_release(void)
{
    uint64_t b = capability_bitmap_runtime();
    TEST_ASSERT_TRUE((b & (1ULL << CAP_MFG_FACTORY_TEST)) == 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_MFG_PROVISION)) == 0);
}

/* device_flags 必须报告出厂锁定 */
static void test_shipped_locked_flag_set(void)
{
    TEST_ASSERT_TRUE((capability_device_flags() & DEVICE_FLAG_MFG_SHIPPED_LOCKED) != 0);
}

/* 消费者基础能力不受门控影响 */
static void test_consumer_base_still_present(void)
{
    uint64_t b = capability_bitmap_runtime();
    TEST_ASSERT_TRUE((b & (1ULL << CAP_KEYBOARD_BASIC)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_CONFIG_STORAGE)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_NKRO)) != 0);
}

/* release 未编入自定义 DFU bootloader（FEATURE_DFU_BOOTLOADER=0），bit10 不得上报；
 * 消费者仍可走芯片原生 BOOTSEL UF2 升级（该路径不占用能力位）。 */
static void test_dfu_bit_cleared_without_bootloader(void)
{
    uint64_t b = capability_bitmap_runtime();
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DFU_UPGRADE)) == 0);
}

/* release 未编入 bootloader：即使 raw HID 直接发 CMD_ENTER_DFU(0x04)，固件闸门也必须
 * 拒绝（NOT_SUPPORTED），不只是上位机隐藏按钮；BOOTSEL UF2(0x14) 为芯片原生能力仍放行。
 * 顺带覆盖“能力位缺失 -> 诊断命令被拒、消费者命令放行”的 release 闸门分支。 */
static void test_dfu_command_rejected_but_bootsel_allowed(void)
{
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_ERR_NOT_SUPPORTED,
                            command_gate_check_command(CMD_ENTER_DFU));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK,
                            command_gate_check_command(CMD_ENTER_BOOTSEL));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_ERR_NOT_SUPPORTED,
                            command_gate_check_command(CMD_SET_PERF_ENABLE));
    TEST_ASSERT_EQUAL_UINT8(CONFIG_HID_OK,
                            command_gate_check_command(CMD_SAVE_CONFIG));
}

/* 诊断位符合 release 编译矩阵：perf/keystats/raw/uart 关闭，精简 Fault 保留 */
static void test_diag_bits_match_release_matrix(void)
{
    uint64_t b = capability_bitmap_runtime();
    TEST_ASSERT_TRUE((b & (1ULL << CAP_PERF_MONITOR)) == 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_KEY_STATS)) == 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DIAG_RAW_STATE)) == 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DIAG_DEBUG_UART)) == 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_FAULT_LOG)) != 0);
}

int main(void)
{
    UnityBegin("test_mfg_lock.c");
    RUN_TEST(test_mfg_unavailable_when_default_locked);
    RUN_TEST(test_mfg_bits_cleared_in_release);
    RUN_TEST(test_shipped_locked_flag_set);
    RUN_TEST(test_consumer_base_still_present);
    RUN_TEST(test_dfu_bit_cleared_without_bootloader);
    RUN_TEST(test_dfu_command_rejected_but_bootsel_allowed);
    RUN_TEST(test_diag_bits_match_release_matrix);
    return UnityEnd();
}
