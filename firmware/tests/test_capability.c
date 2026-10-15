/*
 * tests/test_capability.c
 * 运行时能力位图 / 设备标志单元测试（功能角色分离，协议 v0.2）
 *
 * host 单元测试默认不覆盖 FEATURE_*，features.h 全部默认为 1（dev/工程态全开），
 * 因此这里验证“全开形态”的位图构成。release 裁剪形态（缺诊断/制造位）由
 * 全关固件构建（build_off）保证可编译，其位图收窄逻辑与本表同一套 #if 分支。
 */

#include "unity.h"
#include "features.h"
#include "protocol/capability.h"
#include "protocol/config_hid.h"
#include <stdint.h>

/* 能力总数随诊断/制造段扩展到 26（含 18-23 预留空洞） */
void test_capability_count_extended_to_26(void)
{
    TEST_ASSERT_EQUAL_UINT8(26, CAP_COUNT);
}

/* 消费者基础能力位始终编入，与 FEATURE_* 无关。
 * 注意 CAP_DFU_UPGRADE(bit10) 不在此列，它跟随 FEATURE_DFU_BOOTLOADER，见下方专测。 */
void test_capability_bitmap_contains_consumer_base(void)
{
    uint64_t b = capability_bitmap_runtime();

    TEST_ASSERT_TRUE((b & (1ULL << CAP_KEYBOARD_BASIC)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_MOUSE_BASIC)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_JOYSTICK)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_ENCODER)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_KEYMAP)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_MACRO)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_CONFIG_STORAGE)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_NKRO)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DPI_SLOTS)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_JOYSTICK_CALIB)) != 0);
}

/* 自定义 DFU bootloader 能力位严格跟随 FEATURE_DFU_BOOTLOADER：
 * host 主套件不带 CMake 参数，走 features.h 默认全开（=1），bit10 应置位。
 * release 裁剪形态（=0，bootloader 未编入）由 build_mfg_lock_tests 套件验证 bit10=0。
 * BOOTSEL UF2 为芯片原生升级路径，不占用能力位，不在此断言。 */
void test_capability_dfu_follows_bootloader_flag(void)
{
    uint64_t b = capability_bitmap_runtime();
#if FEATURE_DFU_BOOTLOADER
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DFU_UPGRADE)) != 0);
#else
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DFU_UPGRADE)) == 0);
#endif
}

/* 全开形态：诊断能力位（perf/fault/keystats/raw/uart）全部置位 */
void test_capability_bitmap_contains_diag_bits(void)
{
    uint64_t b = capability_bitmap_runtime();

    TEST_ASSERT_TRUE((b & (1ULL << CAP_PERF_MONITOR)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_FAULT_LOG)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_KEY_STATS)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DIAG_RAW_STATE)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DIAG_DEBUG_UART)) != 0);
}

/* 全开工程态：工厂能力编入且未出厂锁定，制造位置位 */
void test_capability_bitmap_contains_mfg_bits(void)
{
    uint64_t b = capability_bitmap_runtime();

    TEST_ASSERT_TRUE(capability_mfg_factory_available());
    TEST_ASSERT_TRUE((b & (1ULL << CAP_MFG_FACTORY_TEST)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_MFG_PROVISION)) != 0);
}

/* 阶段1工程态：未接 Provisioning 分区，device_flags 恒为 0 */
void test_capability_device_flags_zero_in_engineering(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, capability_device_flags());
}

/* 预留位 18-23 不应被置位 */
void test_capability_reserved_holes_clear(void)
{
    uint64_t b = capability_bitmap_runtime();
    int bit;

    for (bit = 18; bit <= 23; bit++) {
        TEST_ASSERT_TRUE((b & (1ULL << bit)) == 0);
    }
}
