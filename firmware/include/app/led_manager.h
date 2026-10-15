/*
 * include/app/led_manager.h
 * LED 管理模块接口
 * 从 main.c 提取，包含 LED 闪烁任务和模式切换反馈
 */
#ifndef APP_LED_MANAGER_H
#define APP_LED_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "middleware/encoder_processor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 初始化 ==================== */

/* 初始化 LED 管理模块（清零反馈状态） */
void led_manager_init(void);

/* ==================== LED 闪烁任务 ==================== */

/* LED 闪烁任务（调度器每10ms调用一次）
 * 优先级：模式切换反馈 > USB 状态闪烁
 * - 模式切换反馈：100ms 快速闪烁指定次数
 * - USB 状态闪烁：根据 USB 挂载/挂起状态以不同间隔闪烁
 */
void led_manager_blinking_task(void);

/* ==================== 模式切换 LED 反馈 ==================== */

/* 触发模式切换 LED 反馈
 * 闪烁次数 = 模式编号 + 1（SCROLL=1次, VOLUME=2次, ...）
 * @param mode 编码器模式
 */
void led_manager_trigger_mode_feedback(encoder_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* APP_LED_MANAGER_H */
