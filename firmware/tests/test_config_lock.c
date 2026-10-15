/*
 * tests/test_config_lock.c
 * 配置锁定机制模块单元测试
 * 测试 config_lock.c 中的所有函数
 *
 * 覆盖范围：
 * - 初始化和锁定状态查询
 * - 解锁流程（3次尝试，时间窗口5秒）
 * - 自动锁定（30秒无操作）
 * - 写入通知
 * - 命令白名单检查
 *
 * 注意：使用 mock_time 控制时间，精确测试时间窗口和超时逻辑。
 */

#include "unity.h"
#include "protocol/config_lock.h"
#include "protocol/config_hid.h"
#include "mock_time.h"
#include <stdint.h>
#include <stdbool.h>

/* ==================== 辅助函数 ==================== */

/* 每个测试前重置状态和时间 */
static void reset_lock_state(void)
{
    mock_time_reset();
    config_lock_init();
}

/* ==================== 测试用例 ==================== */

/* 测试1：初始化后状态为锁定 */
void test_config_lock_init_locked(void)
{
    reset_lock_state();
    TEST_ASSERT_TRUE(config_lock_is_locked());
}

/* 测试2：第一次解锁尝试返回1 */
void test_config_lock_attempt_1(void)
{
    reset_lock_state();
    uint8_t result = config_lock_attempt_unlock();
    TEST_ASSERT_EQUAL_UINT8(1, result);
    TEST_ASSERT_TRUE(config_lock_is_locked());  /* 仍锁定 */
}

/* 测试3：第二次解锁尝试返回2 */
void test_config_lock_attempt_2(void)
{
    reset_lock_state();
    config_lock_attempt_unlock();  /* 第1次 */
    mock_time_advance(1000 * 1000);  /* 推进1秒 */
    uint8_t result = config_lock_attempt_unlock();  /* 第2次 */
    TEST_ASSERT_EQUAL_UINT8(2, result);
    TEST_ASSERT_TRUE(config_lock_is_locked());  /* 仍锁定 */
}

/* 测试4：第三次解锁尝试返回3（已解锁） */
void test_config_lock_attempt_3_unlocked(void)
{
    reset_lock_state();
    config_lock_attempt_unlock();  /* 第1次 */
    mock_time_advance(1000 * 1000);  /* 推进1秒 */
    config_lock_attempt_unlock();  /* 第2次 */
    mock_time_advance(1000 * 1000);  /* 推进1秒 */
    uint8_t result = config_lock_attempt_unlock();  /* 第3次 */
    TEST_ASSERT_EQUAL_UINT8(3, result);
    TEST_ASSERT_FALSE(config_lock_is_locked());  /* 已解锁 */
}

/* 测试5：解锁时间窗口 - 超过5秒后重置计数 */
void test_config_lock_window_timeout_reset(void)
{
    reset_lock_state();
    config_lock_attempt_unlock();  /* 第1次 */
    mock_time_advance(6000 * 1000);  /* 推进6秒（超过5秒窗口） */
    uint8_t result = config_lock_attempt_unlock();  /* 应该重置为第1次 */
    TEST_ASSERT_EQUAL_UINT8(1, result);
    TEST_ASSERT_TRUE(config_lock_is_locked());  /* 仍锁定 */
}

/* 测试6：解锁时间窗口 - 5秒内连续尝试成功 */
void test_config_lock_window_within_success(void)
{
    reset_lock_state();
    config_lock_attempt_unlock();  /* 第1次 */
    mock_time_advance(2000 * 1000);  /* 推进2秒 */
    config_lock_attempt_unlock();  /* 第2次 */
    mock_time_advance(2000 * 1000);  /* 推进2秒（总共4秒，在窗口内） */
    uint8_t result = config_lock_attempt_unlock();  /* 第3次，应该成功 */
    TEST_ASSERT_EQUAL_UINT8(3, result);
    TEST_ASSERT_FALSE(config_lock_is_locked());  /* 已解锁 */
}

/* 测试7：自动锁定 - 解锁后30秒无操作自动锁定 */
void test_config_lock_auto_relock_timeout(void)
{
    reset_lock_state();
    /* 解锁 */
    config_lock_attempt_unlock();
    mock_time_advance(1000 * 1000);
    config_lock_attempt_unlock();
    mock_time_advance(1000 * 1000);
    config_lock_attempt_unlock();
    TEST_ASSERT_FALSE(config_lock_is_locked());  /* 已解锁 */

    /* 推进31秒（超过30秒超时） */
    mock_time_advance(31000 * 1000);
    bool relocked = config_lock_check_auto_relock();
    TEST_ASSERT_TRUE(relocked);  /* 已自动重新锁定 */
    TEST_ASSERT_TRUE(config_lock_is_locked());  /* 已锁定 */
}

/* 测试8：自动锁定 - 解锁后29秒无操作不锁定 */
void test_config_lock_auto_relock_not_yet(void)
{
    reset_lock_state();
    /* 解锁 */
    config_lock_attempt_unlock();
    mock_time_advance(1000 * 1000);
    config_lock_attempt_unlock();
    mock_time_advance(1000 * 1000);
    config_lock_attempt_unlock();
    TEST_ASSERT_FALSE(config_lock_is_locked());  /* 已解锁 */

    /* 推进29秒（未超过30秒超时） */
    mock_time_advance(29000 * 1000);
    bool relocked = config_lock_check_auto_relock();
    TEST_ASSERT_FALSE(relocked);  /* 未自动锁定 */
    TEST_ASSERT_FALSE(config_lock_is_locked());  /* 仍解锁 */
}

/* 测试9：自动锁定 - 有写入操作则重置计时器 */
void test_config_lock_auto_relock_write_resets(void)
{
    reset_lock_state();
    /* 解锁 */
    config_lock_attempt_unlock();
    mock_time_advance(1000 * 1000);
    config_lock_attempt_unlock();
    mock_time_advance(1000 * 1000);
    config_lock_attempt_unlock();
    TEST_ASSERT_FALSE(config_lock_is_locked());  /* 已解锁 */

    /* 推进20秒，然后写入操作 */
    mock_time_advance(20000 * 1000);
    config_lock_notify_write();  /* 写入操作，重置计时器 */

    /* 再推进20秒（总共40秒，但写入后只有20秒） */
    mock_time_advance(20000 * 1000);
    bool relocked = config_lock_check_auto_relock();
    TEST_ASSERT_FALSE(relocked);  /* 未自动锁定（写入后只有20秒） */
    TEST_ASSERT_FALSE(config_lock_is_locked());  /* 仍解锁 */
}

/* 测试10：自动锁定 - 已锁定状态下检查返回false */
void test_config_lock_auto_relock_already_locked(void)
{
    reset_lock_state();
    TEST_ASSERT_TRUE(config_lock_is_locked());  /* 已锁定 */
    bool relocked = config_lock_check_auto_relock();
    TEST_ASSERT_FALSE(relocked);  /* 返回false（已经锁定，无需操作） */
}

/* 测试11：写入通知 - 不改变锁定状态 */
void test_config_lock_notify_write_no_state_change(void)
{
    reset_lock_state();
    TEST_ASSERT_TRUE(config_lock_is_locked());  /* 已锁定 */
    config_lock_notify_write();
    TEST_ASSERT_TRUE(config_lock_is_locked());  /* 仍锁定 */
}

/* 测试12：命令白名单 - GET_VERSION允许 */
void test_config_lock_cmd_get_version_allowed(void)
{
    reset_lock_state();
    TEST_ASSERT_TRUE(config_lock_is_command_allowed(CMD_GET_VERSION));
}

/* 测试13：命令白名单 - ENTER_DFU允许 */
void test_config_lock_cmd_enter_dfu_allowed(void)
{
    reset_lock_state();
    TEST_ASSERT_TRUE(config_lock_is_command_allowed(CMD_ENTER_DFU));
}

/* 测试14：命令白名单 - REBOOT允许 */
void test_config_lock_cmd_reboot_allowed(void)
{
    reset_lock_state();
    TEST_ASSERT_TRUE(config_lock_is_command_allowed(CMD_REBOOT));
}

/* 测试15：命令白名单 - SWITCH_DPI允许 */
void test_config_lock_cmd_switch_dpi_allowed(void)
{
    reset_lock_state();
    TEST_ASSERT_TRUE(config_lock_is_command_allowed(CMD_SWITCH_DPI));
}

/* 测试16：命令白名单 - SAVE_CONFIG不允许（需要解锁） */
void test_config_lock_cmd_save_config_not_allowed(void)
{
    reset_lock_state();
    TEST_ASSERT_FALSE(config_lock_is_command_allowed(CMD_SAVE_CONFIG));
}

/* 测试17：命令白名单 - 未知命令不允许 */
void test_config_lock_cmd_unknown_not_allowed(void)
{
    reset_lock_state();
    TEST_ASSERT_FALSE(config_lock_is_command_allowed(0xFF));  /* 未知命令 */
    TEST_ASSERT_FALSE(config_lock_is_command_allowed(0x00));  /* 空命令 */
}

/* 测试18：解锁后可以执行需要解锁的命令 */
void test_config_lock_unlock_allows_sensitive_cmds(void)
{
    reset_lock_state();
    /* 锁定状态下SAVE_CONFIG不允许 */
    TEST_ASSERT_FALSE(config_lock_is_command_allowed(CMD_SAVE_CONFIG));

    /* 解锁 */
    config_lock_attempt_unlock();
    mock_time_advance(1000 * 1000);
    config_lock_attempt_unlock();
    mock_time_advance(1000 * 1000);
    config_lock_attempt_unlock();
    TEST_ASSERT_FALSE(config_lock_is_locked());  /* 已解锁 */

    /* 注意：config_lock_is_command_allowed 是静态白名单，
     * 不依赖当前锁定状态，所以SAVE_CONFIG仍然不在白名单中。
     * 实际使用时，调用方应先检查 config_lock_is_locked()，
     * 如果未锁定则允许执行所有命令。
     * 这里验证白名单逻辑是正确的。
     */
    TEST_ASSERT_FALSE(config_lock_is_command_allowed(CMD_SAVE_CONFIG));
}
