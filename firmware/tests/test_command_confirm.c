/*
 * tests/test_command_confirm.c
 * 命令确认机制模块单元测试
 * 测试 command_confirm.c 中的所有函数
 *
 * 覆盖范围：
 * - 重启命令确认（reboot）：3次确认，5秒窗口
 * - DFU命令确认（dfu）：3次确认，5秒窗口
 * - BOOTSEL命令确认（bootsel）：3次确认，5秒窗口
 * - 每个命令的初始化、尝试、确认、重置、时间窗口
 *
 * 注意：使用 mock_time 控制时间，精确测试时间窗口逻辑。
 */

#include "unity.h"
#include "protocol/command_confirm.h"
#include "mock_time.h"
#include <stdint.h>
#include <stdbool.h>

/* ==================== 辅助函数 ==================== */

/* 每个测试前重置状态和时间 */
static void reset_confirm_state(void)
{
    mock_time_reset();
    command_confirm_init();
}

/* ==================== 重启命令确认测试 ==================== */

/* 测试1：初始化后重启未确认 */
void test_command_confirm_reboot_init_not_confirmed(void)
{
    reset_confirm_state();
    TEST_ASSERT_FALSE(command_confirm_is_reboot_confirmed());
}

/* 测试2：重启第1次尝试返回1 */
void test_command_confirm_reboot_attempt_1(void)
{
    reset_confirm_state();
    uint8_t result = command_confirm_attempt_reboot();
    TEST_ASSERT_EQUAL_UINT8(1, result);
    TEST_ASSERT_FALSE(command_confirm_is_reboot_confirmed());
}

/* 测试3：重启第2次尝试返回2 */
void test_command_confirm_reboot_attempt_2(void)
{
    reset_confirm_state();
    command_confirm_attempt_reboot();
    mock_time_advance(1000 * 1000);  /* 推进1秒 */
    uint8_t result = command_confirm_attempt_reboot();
    TEST_ASSERT_EQUAL_UINT8(2, result);
    TEST_ASSERT_FALSE(command_confirm_is_reboot_confirmed());
}

/* 测试4：重启第3次尝试返回3（已确认） */
void test_command_confirm_reboot_attempt_3_confirmed(void)
{
    reset_confirm_state();
    command_confirm_attempt_reboot();
    mock_time_advance(1000 * 1000);
    command_confirm_attempt_reboot();
    mock_time_advance(1000 * 1000);
    uint8_t result = command_confirm_attempt_reboot();
    TEST_ASSERT_EQUAL_UINT8(3, result);
    /* 注意：确认完成后计数被重置，is_confirmed 返回 false */
    TEST_ASSERT_FALSE(command_confirm_is_reboot_confirmed());
}

/* 测试5：重启时间窗口超时重置计数 */
void test_command_confirm_reboot_window_timeout_reset(void)
{
    reset_confirm_state();
    command_confirm_attempt_reboot();  /* 第1次 */
    mock_time_advance(6000 * 1000);  /* 推进6秒（超过5秒窗口） */
    uint8_t result = command_confirm_attempt_reboot();  /* 应该重置为第1次 */
    TEST_ASSERT_EQUAL_UINT8(1, result);
}

/* 测试6：重启时间窗口内连续尝试成功 */
void test_command_confirm_reboot_window_within_success(void)
{
    reset_confirm_state();
    command_confirm_attempt_reboot();
    mock_time_advance(2000 * 1000);  /* 推进2秒 */
    command_confirm_attempt_reboot();
    mock_time_advance(2000 * 1000);  /* 推进2秒（总共4秒，在窗口内） */
    uint8_t result = command_confirm_attempt_reboot();  /* 第3次，应该成功 */
    TEST_ASSERT_EQUAL_UINT8(3, result);
}

/* ==================== DFU命令确认测试 ==================== */

/* 测试7：初始化后DFU未确认 */
void test_command_confirm_dfu_init_not_confirmed(void)
{
    reset_confirm_state();
    TEST_ASSERT_FALSE(command_confirm_is_dfu_confirmed());
}

/* 测试8：DFU第1次尝试返回1 */
void test_command_confirm_dfu_attempt_1(void)
{
    reset_confirm_state();
    uint8_t result = command_confirm_attempt_dfu();
    TEST_ASSERT_EQUAL_UINT8(1, result);
    TEST_ASSERT_FALSE(command_confirm_is_dfu_confirmed());
}

/* 测试9：DFU第3次尝试返回3（已确认） */
void test_command_confirm_dfu_attempt_3_confirmed(void)
{
    reset_confirm_state();
    command_confirm_attempt_dfu();
    mock_time_advance(1000 * 1000);
    command_confirm_attempt_dfu();
    mock_time_advance(1000 * 1000);
    uint8_t result = command_confirm_attempt_dfu();
    TEST_ASSERT_EQUAL_UINT8(3, result);
    TEST_ASSERT_FALSE(command_confirm_is_dfu_confirmed());
}

/* 测试10：DFU时间窗口超时重置计数 */
void test_command_confirm_dfu_window_timeout_reset(void)
{
    reset_confirm_state();
    command_confirm_attempt_dfu();
    mock_time_advance(6000 * 1000);  /* 超过5秒窗口 */
    uint8_t result = command_confirm_attempt_dfu();
    TEST_ASSERT_EQUAL_UINT8(1, result);
}

/* ==================== BOOTSEL命令确认测试 ==================== */

/* 测试11：初始化后BOOTSEL未确认 */
void test_command_confirm_bootsel_init_not_confirmed(void)
{
    reset_confirm_state();
    TEST_ASSERT_FALSE(command_confirm_is_bootsel_confirmed());
}

/* 测试12：BOOTSEL第1次尝试返回1 */
void test_command_confirm_bootsel_attempt_1(void)
{
    reset_confirm_state();
    uint8_t result = command_confirm_attempt_bootsel();
    TEST_ASSERT_EQUAL_UINT8(1, result);
    TEST_ASSERT_FALSE(command_confirm_is_bootsel_confirmed());
}

/* 测试13：BOOTSEL第3次尝试返回3（已确认） */
void test_command_confirm_bootsel_attempt_3_confirmed(void)
{
    reset_confirm_state();
    command_confirm_attempt_bootsel();
    mock_time_advance(1000 * 1000);
    command_confirm_attempt_bootsel();
    mock_time_advance(1000 * 1000);
    uint8_t result = command_confirm_attempt_bootsel();
    TEST_ASSERT_EQUAL_UINT8(3, result);
    TEST_ASSERT_FALSE(command_confirm_is_bootsel_confirmed());
}

/* 测试14：BOOTSEL时间窗口超时重置计数 */
void test_command_confirm_bootsel_window_timeout_reset(void)
{
    reset_confirm_state();
    command_confirm_attempt_bootsel();
    mock_time_advance(6000 * 1000);  /* 超过5秒窗口 */
    uint8_t result = command_confirm_attempt_bootsel();
    TEST_ASSERT_EQUAL_UINT8(1, result);
}

/* ==================== 重置功能测试 ==================== */

/* 测试15：重置重启确认状态 */
void test_command_confirm_reset_reboot(void)
{
    reset_confirm_state();
    command_confirm_attempt_reboot();
    command_confirm_attempt_reboot();
    TEST_ASSERT_FALSE(command_confirm_is_reboot_confirmed());

    command_confirm_reset_reboot();
    /* 重置后第1次尝试应该返回1 */
    uint8_t result = command_confirm_attempt_reboot();
    TEST_ASSERT_EQUAL_UINT8(1, result);
}

/* 测试16：重置DFU确认状态 */
void test_command_confirm_reset_dfu(void)
{
    reset_confirm_state();
    command_confirm_attempt_dfu();
    command_confirm_attempt_dfu();

    command_confirm_reset_dfu();
    uint8_t result = command_confirm_attempt_dfu();
    TEST_ASSERT_EQUAL_UINT8(1, result);
}

/* 测试17：重置BOOTSEL确认状态 */
void test_command_confirm_reset_bootsel(void)
{
    reset_confirm_state();
    command_confirm_attempt_bootsel();
    command_confirm_attempt_bootsel();

    command_confirm_reset_bootsel();
    uint8_t result = command_confirm_attempt_bootsel();
    TEST_ASSERT_EQUAL_UINT8(1, result);
}

/* ==================== 独立性测试 ==================== */

/* 测试18：三个命令确认状态相互独立 */
void test_command_confirm_independent(void)
{
    reset_confirm_state();

    /* 重启确认2次 */
    command_confirm_attempt_reboot();
    mock_time_advance(500 * 1000);
    command_confirm_attempt_reboot();

    /* DFU确认1次 */
    mock_time_advance(500 * 1000);
    command_confirm_attempt_dfu();

    /* BOOTSEL确认0次 */

    /* 验证各自状态：重启第3次应该成功，DFU第2次应该返回2，BOOTSEL第1次应该返回1 */
    mock_time_advance(500 * 1000);
    uint8_t reboot_result = command_confirm_attempt_reboot();
    TEST_ASSERT_EQUAL_UINT8(3, reboot_result);  /* 重启第3次成功 */

    mock_time_advance(500 * 1000);
    uint8_t dfu_result = command_confirm_attempt_dfu();
    TEST_ASSERT_EQUAL_UINT8(2, dfu_result);  /* DFU第2次 */

    mock_time_advance(500 * 1000);
    uint8_t bootsel_result = command_confirm_attempt_bootsel();
    TEST_ASSERT_EQUAL_UINT8(1, bootsel_result);  /* BOOTSEL第1次 */
}
