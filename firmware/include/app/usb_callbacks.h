/*
 * include/app/usb_callbacks.h
 * TinyUSB 回调函数接口
 * 从 main.c 提取，包含 USB 挂载/卸载/挂起/恢复回调和 LED 闪烁控制
 */
#ifndef APP_USB_CALLBACKS_H
#define APP_USB_CALLBACKS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== LED 闪烁间隔常量 ==================== */

#define BLINK_NOT_MOUNTED  250U   /* 未挂载时闪烁间隔（ms） */
#define BLINK_MOUNTED      1000U  /* 已挂载时闪烁间隔（ms） */
#define BLINK_SUSPENDED    2500U  /* 挂起时闪烁间隔（ms） */

/* ==================== 初始化 ==================== */

/* 初始化 USB 回调模块（设置默认闪烁间隔） */
void usb_callbacks_init(void);

/* ==================== LED 闪烁控制 ==================== */

/* 获取当前 LED 闪烁间隔（ms） */
uint32_t usb_callbacks_get_blink_interval(void);

/* 设置 LED 闪烁间隔（ms），0 表示禁用闪烁 */
void usb_callbacks_set_blink_interval(uint32_t interval_ms);

/* ==================== TinyUSB 回调函数 ==================== */

/* 设备挂载回调（USB 枚举完成） */
void tud_mount_cb(void);

/* 设备卸载回调（USB 断开） */
void tud_umount_cb(void);

/* USB 总线挂起回调（主机进入挂起状态）
 * remote_wakeup_en: 主机是否允许远程唤醒
 * 7ms 内设备必须从总线汲取平均电流小于 2.5mA
 */
void tud_suspend_cb(bool remote_wakeup_en);

/* USB 总线恢复回调（从挂起状态恢复） */
void tud_resume_cb(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_USB_CALLBACKS_H */
