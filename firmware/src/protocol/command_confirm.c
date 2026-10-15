/*
 * src/protocol/command_confirm.c
 * 命令确认机制实现
 * 从 config_hid.c 提取，包含 DFU/Reboot 命令的多次确认机制
 * 防止恶意程序通过单次命令触发危险操作（如重启、进入 DFU 模式）
 */
#include "protocol/command_confirm.h"
#include "pico/time.h"

/* ==================== 私有数据 ==================== */

/* 重启命令确认状态 */
static volatile uint8_t  s_reboot_attempts = 0;
static volatile uint32_t s_last_reboot_ms = 0;

/* DFU 命令确认状态 */
static volatile uint8_t  s_dfu_attempts = 0;
static volatile uint32_t s_last_dfu_ms = 0;

/* BOOTSEL 命令确认状态 */
static volatile uint8_t  s_bootsel_attempts = 0;
static volatile uint32_t s_last_bootsel_ms = 0;

/* ==================== 初始化 ==================== */

void command_confirm_init(void)
{
  s_reboot_attempts = 0;
  s_last_reboot_ms = 0;
  s_dfu_attempts = 0;
  s_last_dfu_ms = 0;
  s_bootsel_attempts = 0;
  s_last_bootsel_ms = 0;
}

/* ==================== 重启命令确认 ==================== */

uint8_t command_confirm_attempt_reboot(void)
{
  uint32_t now = to_ms_since_boot(get_absolute_time());

  /* 超过时间窗口则重置计数 */
  if (now - s_last_reboot_ms > COMMAND_CONFIRM_REBOOT_WINDOW_MS)
  {
    s_reboot_attempts = 0;
  }

  s_last_reboot_ms = now;
  s_reboot_attempts++;

  if (s_reboot_attempts >= COMMAND_CONFIRM_REBOOT_REQUIRED)
  {
    s_reboot_attempts = 0;
    return COMMAND_CONFIRM_REBOOT_REQUIRED;  /* 返回3表示已确认 */
  }

  return s_reboot_attempts;  /* 返回当前进度（1-2） */
}

bool command_confirm_is_reboot_confirmed(void)
{
  return (s_reboot_attempts >= COMMAND_CONFIRM_REBOOT_REQUIRED);
}

void command_confirm_reset_reboot(void)
{
  s_reboot_attempts = 0;
  s_last_reboot_ms = 0;
}

/* ==================== DFU 命令确认 ==================== */

uint8_t command_confirm_attempt_dfu(void)
{
  uint32_t now = to_ms_since_boot(get_absolute_time());

  /* 超过时间窗口则重置计数 */
  if (now - s_last_dfu_ms > COMMAND_CONFIRM_DFU_WINDOW_MS)
  {
    s_dfu_attempts = 0;
  }

  s_last_dfu_ms = now;
  s_dfu_attempts++;

  if (s_dfu_attempts >= COMMAND_CONFIRM_DFU_REQUIRED)
  {
    s_dfu_attempts = 0;
    return COMMAND_CONFIRM_DFU_REQUIRED;  /* 返回3表示已确认 */
  }

  return s_dfu_attempts;  /* 返回当前进度（1-2） */
}

bool command_confirm_is_dfu_confirmed(void)
{
  return (s_dfu_attempts >= COMMAND_CONFIRM_DFU_REQUIRED);
}

void command_confirm_reset_dfu(void)
{
  s_dfu_attempts = 0;
  s_last_dfu_ms = 0;
}

/* ==================== BOOTSEL 命令确认 ==================== */

uint8_t command_confirm_attempt_bootsel(void)
{
  uint32_t now = to_ms_since_boot(get_absolute_time());

  /* 超过时间窗口则重置计数 */
  if (now - s_last_bootsel_ms > COMMAND_CONFIRM_BOOTSEL_WINDOW_MS)
  {
    s_bootsel_attempts = 0;
  }

  s_last_bootsel_ms = now;
  s_bootsel_attempts++;

  if (s_bootsel_attempts >= COMMAND_CONFIRM_BOOTSEL_REQUIRED)
  {
    s_bootsel_attempts = 0;
    return COMMAND_CONFIRM_BOOTSEL_REQUIRED;  /* 返回3表示已确认 */
  }

  return s_bootsel_attempts;  /* 返回当前进度（1-2） */
}

bool command_confirm_is_bootsel_confirmed(void)
{
  return (s_bootsel_attempts >= COMMAND_CONFIRM_BOOTSEL_REQUIRED);
}

void command_confirm_reset_bootsel(void)
{
  s_bootsel_attempts = 0;
  s_last_bootsel_ms = 0;
}
