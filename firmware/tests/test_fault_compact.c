/*
 * tests/test_fault_compact.c
 * Fault 精简档（FEATURE_FAULT_COMPACT=1）单元测试
 *
 * 独立可执行（不依赖 test_runner.c）：fault.c 不能在同一二进制里同时编入
 * 全量/紧凑两个后端，故紧凑档单独构建（见 build_compact_tests.bat）。
 *
 * 覆盖：
 *   - 开机 BOOT 事件 / boot_count / 复位原因
 *   - 公共 fault_log_entry_t 结构保持 256B（上位机零改动）
 *   - 按页攒批（未满页不落盘）与 FATAL seal（PAD 填充）
 *   - 同类故障 occurrences 合并
 *   - 多页 + 重启恢复 + 最近 32 条窗口
 *   - CRC 损坏页整页判废、不二次编程、从下一页继续
 *   - 扇区写满 A->B 搬移（磨损均衡，保留最近 128 条）
 *   - 看门狗复位启动事件立即 seal
 *   - clear 清空双扇区
 */

#include "unity.h"
#include "middleware/fault.h"
#include "board/flash_layout.h"
#include "mock_flash.h"
#include "mock_time.h"
#include "hardware/watchdog.h"
#include <string.h>

/* 15 个可稳定映射到不同 module_id 的模块名（"otherdev" 兜底 OTHER） */
static const char* const s_mods[15] =
{
    "watchdog", "flash", "core1", "ipc", "usb",
    "config", "keypad", "optical", "joystick", "encoder",
    "factory", "macro", "power", "scheduler", "otherdev"
};

void setUp(void) {}
void tearDown(void) {}

/* 模拟一次上电；reset_flash=false 时保留 Flash（模拟重启） */
static void boot(bool reset_flash, bool watchdog_reboot)
{
    if (reset_flash) mock_flash_reset();
    mock_time_reset();
    mock_watchdog_set_caused_reboot(watchdog_reboot);
    fault_init();
}

/* 记录一条 ERROR（推进时间越过 100ms 写限频），模块按索引轮换保证不合并 */
static void rec_err(uint32_t mod_idx)
{
    mock_time_advance(101000U);
    fault_record(FAULT_LEVEL_ERROR, s_mods[mod_idx % 15U], "event");
}

static bool region_all_ff(uint32_t offset, uint32_t len)
{
    static uint8_t buf[4096];
    mock_flash_read(offset, buf, len);
    for (uint32_t i = 0; i < len; i++)
    {
        if (buf[i] != 0xFFU) return false;
    }
    return true;
}

/* ==================== 测试用例 ==================== */

/* 首次正常上电：追加 1 条 power-on BOOT（boot#1），且攒批不立即写 Flash */
static void test_boot_event_first_poweron(void)
{
    boot(true, false);

    TEST_ASSERT_EQUAL_UINT32(1, fault_get_log_count());
    fault_log_entry_t e;
    TEST_ASSERT_TRUE(fault_get_log(0, &e));
    TEST_ASSERT_EQUAL_UINT8(FAULT_LEVEL_INFO, e.level);
    TEST_ASSERT_EQUAL_STRING("system", e.module);
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "power-on"));
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "boot#1"));

    /* 正常上电 BOOT 留在 RAM 攒批，Fault 区应仍为擦除态 */
    TEST_ASSERT_TRUE(region_all_ff(flash_layout_fault_a_offset(), 4096));
}

/* 公共日志结构 ABI 保持现状：结构体 252B，每条按 1 Flash 页 256B 定址（4B 填充），
 * HID 报告与上位机不变；紧凑后端对外仍还原为此结构。 */
static void test_public_entry_struct_unchanged(void)
{
    TEST_ASSERT_EQUAL_UINT32(252, (uint32_t)sizeof(fault_log_entry_t));
    TEST_ASSERT_EQUAL_UINT32(256, FAULT_LOG_ENTRY_SIZE);
}

/* 未满页只在 RAM 攒批；FATAL 触发 seal 落盘；重启后历史可恢复 */
static void test_page_buffered_until_flush(void)
{
    boot(true, false);
    for (uint32_t i = 0; i < 5; i++) rec_err(i);   /* BOOT + 5 = 6 条 */

    TEST_ASSERT_EQUAL_UINT32(6, fault_get_log_count());
    TEST_ASSERT_TRUE(region_all_ff(flash_layout_fault_a_offset(), 4096));  /* 未落盘 */

    mock_time_advance(101000U);
    fault_record(FAULT_LEVEL_FATAL, "config", "fatal event");              /* +FATAL=7，seal */

    TEST_ASSERT_FALSE(region_all_ff(flash_layout_fault_a_offset(), 256));  /* 第 0 页已编程 */
    TEST_ASSERT_EQUAL_UINT32(7, fault_get_log_count());

    /* 模拟重启：7 条有效 + 9 PAD，重新扫描后追加新 BOOT#2 */
    boot(false, false);
    TEST_ASSERT_EQUAL_UINT32(8, fault_get_log_count());

    fault_log_entry_t e;
    TEST_ASSERT_TRUE(fault_get_log(7, &e));
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "power-on"));
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "boot#2"));

    TEST_ASSERT_TRUE(fault_get_log(6, &e));                                /* FATAL 仍在 */
    TEST_ASSERT_EQUAL_STRING("config", e.module);
}

/* 同指纹故障在当前 RAM 页合并，occurrences 体现在还原文本 "(xN)" */
static void test_coalesce_same_fault(void)
{
    boot(true, false);
    mock_time_advance(101000U);
    fault_record(FAULT_LEVEL_ERROR, "flash", "flash write failed");
    mock_time_advance(101000U);
    fault_record(FAULT_LEVEL_ERROR, "flash", "flash write failed");

    TEST_ASSERT_EQUAL_UINT32(2, fault_get_log_count());   /* BOOT + 1（第二条合并） */

    fault_log_entry_t e;
    TEST_ASSERT_TRUE(fault_get_log(1, &e));
    TEST_ASSERT_EQUAL_STRING("flash", e.module);
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "(x2)"));
}

/* 多页编程 + 多次重启恢复，窗口恒为最近 32 条 */
static void test_window_and_multipage_reboot(void)
{
    boot(true, false);                 /* boot#1 */
    for (uint32_t round = 0; round < 3; round++)
    {
        if (round > 0) boot(false, false);
        for (uint32_t i = 0; i < 15; i++) rec_err(i);   /* BOOT + 15 = 满 1 页 */
    }
    boot(false, false);                /* boot#4，扫描到 48 条历史，追加新 BOOT 在 RAM */

    TEST_ASSERT_EQUAL_UINT32(32, fault_get_log_count());

    fault_log_entry_t e;
    TEST_ASSERT_TRUE(fault_get_log(31, &e));            /* 最新 = boot#4 上电事件 */
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "boot#4"));
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "power-on"));

    TEST_ASSERT_TRUE(fault_get_log(0, &e));             /* 窗口最旧一条仍可读 */
}

/* 页内 CRC 损坏：整页判废并跳过，从下一物理页继续（不二次编程损坏页） */
static void test_corrupt_page_is_skipped(void)
{
    boot(true, false);
    for (uint32_t i = 0; i < 15; i++) rec_err(i);       /* BOOT + 15 写满第 0 页 */

    /* 破坏第 0 页第 5 条记录 1 个字节，使其 CRC 失效 */
    uint8_t junk = 0x00;
    mock_flash_direct_write(flash_layout_fault_a_offset() + 5U * 16U, &junk, 1);

    /* 重扫：第 0 页（含原 BOOT#1）整页判废，仅追加新的上电事件。
     * 因历史 BOOT 随损坏页丢弃，boot_count 回落到 1；这里只校验是上电事件。 */
    boot(false, false);
    TEST_ASSERT_EQUAL_UINT32(1, fault_get_log_count());
    fault_log_entry_t e;
    TEST_ASSERT_TRUE(fault_get_log(0, &e));
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "power-on"));

    /* 新一页（BOOT + 15）应写到第 1 页，第 1 页被编程而不触碰损坏的第 0 页 */
    for (uint32_t i = 0; i < 15; i++) rec_err(i);
    TEST_ASSERT_EQUAL_UINT32(16, fault_get_log_count());
    TEST_ASSERT_FALSE(region_all_ff(flash_layout_fault_a_offset() + 256U, 256));
}

/* 扇区写满：A->B 搬移最近 128 条（磨损均衡），窗口仍为 32，boot_count 连续 */
static void test_sector_switch_wear_leveling(void)
{
    boot(true, false);                 /* boot#1 */
    for (uint32_t round = 0; round < 16; round++)
    {
        if (round > 0) boot(false, false);             /* boot#2 .. boot#16 */
        for (uint32_t i = 0; i < 15; i++) rec_err(i);  /* 每轮写满 1 页 */
    }

    /* 16 页 = 256 槽写满，应已擦 B 区并搬移 */
    TEST_ASSERT_TRUE(mock_flash_get_erase_count() >= 1U);
    TEST_ASSERT_FALSE(region_all_ff(flash_layout_fault_b_offset(), 128U * 16U));
    TEST_ASSERT_EQUAL_UINT32(32, fault_get_log_count());

    fault_log_entry_t e;
    TEST_ASSERT_TRUE(fault_get_log(31, &e));
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "boot#16"));
}

/* 看门狗/HardFault 复位启动：BOOT 事件立即 seal 落盘，重启后可追溯 */
static void test_watchdog_boot_sealed(void)
{
    boot(true, true);
    TEST_ASSERT_FALSE(region_all_ff(flash_layout_fault_a_offset(), 256));  /* 立即落盘 */
    TEST_ASSERT_TRUE(mock_flash_get_program_count() >= 1U);

    fault_log_entry_t e;
    TEST_ASSERT_EQUAL_UINT32(1, fault_get_log_count());
    TEST_ASSERT_TRUE(fault_get_log(0, &e));
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "watchdog/hard-fault"));
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "boot#1"));

    /* 正常重启后，异常复位记录仍在，新上电事件 boot#2 紧随其后 */
    boot(false, false);
    TEST_ASSERT_EQUAL_UINT32(2, fault_get_log_count());
    TEST_ASSERT_TRUE(fault_get_log(0, &e));
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "watchdog/hard-fault"));
    TEST_ASSERT_TRUE(fault_get_log(1, &e));
    TEST_ASSERT_NOT_NULL(strstr((const char*)e.msg, "boot#2"));
}

/* clear 擦除双扇区并复位计数，之后可继续记录 */
static void test_clear_erases_both_sectors(void)
{
    boot(true, false);
    for (uint32_t i = 0; i < 5; i++) rec_err(i);
    mock_time_advance(101000U);
    fault_record(FAULT_LEVEL_FATAL, "config", "fatal event");
    TEST_ASSERT_EQUAL_UINT32(7, fault_get_log_count());

    fault_clear();
    TEST_ASSERT_EQUAL_UINT32(0, fault_get_log_count());
    TEST_ASSERT_TRUE(region_all_ff(flash_layout_fault_a_offset(), 4096));
    TEST_ASSERT_TRUE(region_all_ff(flash_layout_fault_b_offset(), 4096));

    rec_err(0);
    TEST_ASSERT_EQUAL_UINT32(1, fault_get_log_count());
}

int main(void)
{
    UnityBegin("test_fault_compact.c");
    RUN_TEST(test_boot_event_first_poweron);
    RUN_TEST(test_public_entry_struct_unchanged);
    RUN_TEST(test_page_buffered_until_flush);
    RUN_TEST(test_coalesce_same_fault);
    RUN_TEST(test_window_and_multipage_reboot);
    RUN_TEST(test_corrupt_page_is_skipped);
    RUN_TEST(test_sector_switch_wear_leveling);
    RUN_TEST(test_watchdog_boot_sealed);
    RUN_TEST(test_clear_erases_both_sectors);
    return UnityEnd();
}
