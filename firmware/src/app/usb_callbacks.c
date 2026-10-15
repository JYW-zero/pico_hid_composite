/*
 * src/app/usb_callbacks.c
 * TinyUSB 回调函数实现
 * 从 main.c 提取，包含 USB 挂载/卸载/挂起/恢复回调和 LED 闪烁控制
 */
#include "app/usb_callbacks.h"
#include "protocol/config_hid.h"
#include "middleware/power_manager.h"
#include "bsp/board_api.h"
#include <stdio.h>

/* ==================== 私有数据 ==================== */

/* 当前 LED 闪烁间隔（ms），0 表示禁用闪烁 */
static uint32_t s_blink_interval_ms = BLINK_NOT_MOUNTED;

/* ==================== 初始化 ==================== */

void usb_callbacks_init(void)
{
  s_blink_interval_ms = BLINK_NOT_MOUNTED;
}

/* ==================== LED 闪烁控制 ==================== */

uint32_t usb_callbacks_get_blink_interval(void)
{
  return s_blink_interval_ms;
}

void usb_callbacks_set_blink_interval(uint32_t interval_ms)
{
  s_blink_interval_ms = interval_ms;
}

/* ==================== TinyUSB 回调函数 ==================== */

/* 设备挂载回调（USB 枚举完成） */
void tud_mount_cb(void)
{
  s_blink_interval_ms = BLINK_MOUNTED;
  printf("USB 挂载成功！\n");
}

/* 设备卸载回调（USB 断开） */
void tud_umount_cb(void)
{
  s_blink_interval_ms = BLINK_NOT_MOUNTED;
  /* 重置配置块接收状态，防止残留块与新会话组合导致配置损坏 */
  config_hid_reset_blocks_received();
}

/* USB 总线挂起回调（主机进入挂起状态）
 * remote_wakeup_en: 主机是否允许远程唤醒
 * 7ms 内设备必须从总线汲取平均电流小于 2.5mA
 */
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  s_blink_interval_ms = BLINK_SUSPENDED;
  power_manager_on_usb_suspend(remote_wakeup_en);
}

/* USB 总线恢复回调（从挂起状态恢复） */
void tud_resume_cb(void)
{
  s_blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
  power_manager_on_usb_resume();
}
