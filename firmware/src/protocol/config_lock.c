/*
 * src/protocol/config_lock.c
 * 配置锁定机制实现
 * 从 config_hid.c 提取，包含配置锁定/解锁、自动超时锁定、命令白名单检查
 */
#include "protocol/config_lock.h"
#include "protocol/config_hid.h"
#include "pico/time.h"

/* ==================== 私有数据 ==================== */

/* 配置锁定状态（默认锁定，防止恶意程序篡改配置） */
static volatile bool s_config_locked = true;

/* 解锁尝试计数 */
static volatile uint8_t s_unlock_attempts = 0;

/* 上次解锁尝试时间 */
static volatile uint32_t s_last_unlock_ms = 0;

/* 上次写入操作时间（用于自动锁定超时） */
static volatile uint32_t s_last_write_ms = 0;

/* ==================== 初始化 ==================== */

void config_lock_init(void)
{
  s_config_locked = true;
  s_unlock_attempts = 0;
  s_last_unlock_ms = 0;
  s_last_write_ms = 0;
}

/* ==================== 锁定状态查询 ==================== */

bool config_lock_is_locked(void)
{
  return s_config_locked;
}

/* ==================== 解锁操作 ==================== */

uint8_t config_lock_attempt_unlock(void)
{
  uint32_t now = to_ms_since_boot(get_absolute_time());

  /* 超过时间窗口则重置计数 */
  if (now - s_last_unlock_ms > CONFIG_LOCK_UNLOCK_WINDOW_MS)
  {
    s_unlock_attempts = 0;
  }

  s_last_unlock_ms = now;
  s_unlock_attempts++;

  if (s_unlock_attempts >= CONFIG_LOCK_UNLOCK_REQUIRED_ATTEMPTS)
  {
    s_config_locked = false;
    s_unlock_attempts = 0;
    s_last_write_ms = now;
    return CONFIG_LOCK_UNLOCK_REQUIRED_ATTEMPTS;  /* 返回3表示已解锁 */
  }

  return s_unlock_attempts;  /* 返回当前进度（1-2） */
}

/* ==================== 写入通知 ==================== */

void config_lock_notify_write(void)
{
  s_last_write_ms = to_ms_since_boot(get_absolute_time());
}

/* ==================== 自动锁定检查 ==================== */

bool config_lock_check_auto_relock(void)
{
  if (s_config_locked) return false;  /* 已经锁定，无需操作 */

  uint32_t now = to_ms_since_boot(get_absolute_time());
  if (now - s_last_write_ms > CONFIG_LOCK_UNLOCK_TIMEOUT_MS)
  {
    s_config_locked = true;
    return true;  /* 已自动重新锁定 */
  }

  return false;  /* 仍保持解锁 */
}

/* ==================== 命令白名单检查 ==================== */

bool config_lock_is_command_allowed(uint8_t cmd)
{
  /* 白名单：锁定状态下允许执行的命令
   * - DFU/Reboot：有独立的3次确认机制
   * - 实时设置：不写Flash
   * - 只读查询：不修改配置
   * - 宏播放/停止：运行时操作
   */
  switch (cmd)
  {
    case CMD_ENTER_DFU:
    case CMD_ENTER_BOOTSEL:  /* 升级恢复命令，有独立的3次确认机制 */
    case CMD_REBOOT:
    case CMD_SET_JOYSTICK_DZ_RT:
    case CMD_SET_PERF_ENABLE:
    case CMD_JOYSTICK_CALIBRATE:
    case CMD_SWITCH_DPI:
    case CMD_GET_VERSION:
    case CMD_NEGOTIATE_VERSION:
    case CMD_SWITCH_LAYER:
    case CMD_GET_CAPABILITY_INFO:
    case CMD_RESET_STATS:
    case CMD_CLEAR_FAULT:
    case CMD_RESET_PERF:
    case CMD_MACRO_PLAY:
    case CMD_MACRO_STOP:
      return true;

    default:
      return false;
  }
}
