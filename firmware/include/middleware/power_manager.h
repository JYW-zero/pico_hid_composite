/*
 * include/middleware/power_manager.h
 * 低功耗管理模块
 * 使用官方 pico_low_power API
 * 自动适配有线/无线模式：
 *   - USB 挂载 → 有线模式：USB 挂起时进入 Sleep 模式
 *   - USB 未挂载 → 无线模式：无操作超时进入 Dormant 模式
 */
#ifndef MIDDLEWARE_POWER_MANAGER_H
#define MIDDLEWARE_POWER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置常量 ==================== */

/* 无线模式无操作超时时间（毫秒），超过后进入 Dormant 模式 */
#define POWER_DEEP_SLEEP_TIMEOUT_MS  30000U  /* 30秒 */

/* ==================== 功耗等级定义 ==================== */

typedef enum
{
    POWER_LEVEL_ACTIVE = 0,      /* 正常运行，全速 */
    POWER_LEVEL_SLEEP,           /* Sleep 模式：CPU 停止，外设运行，唤醒快，~5.9mA */
    POWER_LEVEL_DORMANT          /* Dormant 模式：振荡器停止，更省电，~3.3mA */
} power_level_t;

/* ==================== 运行模式定义 ==================== */

typedef enum
{
    POWER_MODE_WIRED = 0,        /* 有线模式：USB 已挂载，跟随 USB 挂起 */
    POWER_MODE_WIRELESS          /* 无线模式：USB 未挂载，无操作超时休眠 */
} power_mode_t;

/* ==================== 对外接口 ==================== */

/* 初始化低功耗管理模块 */
void power_manager_init(void);

/* 主循环 tick：检查是否需要进入低功耗
 * 注意：进入低功耗后会阻塞，直到唤醒才返回
 */
void power_manager_tick(void);

/* 通知有用户活动（按键、鼠标移动、编码器、摇杆等），重置超时计时器
 * 任何用户输入都应该调用这个函数
 */
void power_manager_notify_activity(void);

/* USB 挂起回调：有线模式下进入 Sleep */
void power_manager_on_usb_suspend(bool remote_wakeup_en);

/* USB 恢复回调：从 Sleep 唤醒 */
void power_manager_on_usb_resume(void);

/* 获取当前功耗等级 */
power_level_t power_manager_get_level(void);

/* 获取当前运行模式（有线/无线） */
power_mode_t power_manager_get_mode(void);

/* 获取累计休眠时间（毫秒），用于统计 */
uint32_t power_manager_get_sleep_time_ms(void);

/* Dormant 唤醒回调（弱函数，可由上层覆盖）
 * Dormant 模式会停止振荡器，唤醒后 SPI/ADC 等外设可能需要重新初始化
 * 默认实现为空，board/app 层可覆盖此函数执行外设恢复
 */
void power_manager_on_dormant_wakeup(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_POWER_MANAGER_H */
