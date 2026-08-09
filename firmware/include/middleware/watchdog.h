/*
 * include/middleware/watchdog.h
 * 分层逻辑看门狗
 * 多任务独立存活时间戳，任意模块卡死触发硬件复位
 * 硬件看门狗作为最后防线
 */

#ifndef MIDDLEWARE_WATCHDOG_H
#define MIDDLEWARE_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 分层定义：从底层到上层 */
typedef enum
{
    WDG_LAYER_BOARD = 0,   /* 板级初始化、硬件状态监控 */
    WDG_LAYER_DEVICE,      /* 所有外设扫描任务 */
    WDG_LAYER_APP,         /* 主循环、HID处理 */
    WDG_LAYER_COUNT
} wdg_layer_t;

/* 初始化看门狗（设置逻辑超时时间，单位毫秒） */
void watchdog_init(uint32_t timeout_ms);

/* 喂狗：某层报告存活 */
void watchdog_feed_layer(wdg_layer_t layer);

/* 巡检函数：检查所有层是否超时，超时则触发复位
 * 建议以 1ms ~ 5ms 周期调用 */
void watchdog_tick(void);

/* 获取最后触发复位的层（用于故障诊断） */
wdg_layer_t watchdog_get_last_fault_layer(void);

/* 立即触发系统复位 */
void watchdog_system_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_WATCHDOG_H */
