/*
 * src/middleware/watchdog.c
 * 分层逻辑看门狗实现
 * 采用时间戳方式检测各层存活状态
 * 硬件看门狗作为最后防线
 */

#include "middleware/watchdog.h"
#include "middleware/fault.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "hardware/watchdog.h"
#include "pico/time.h"

/* 静态变量：每层最后一次喂狗时间戳（微秒） */
static uint32_t s_last_feed_us[WDG_LAYER_COUNT];
static uint32_t s_timeout_us;          /* 逻辑超时微秒数 */
static bool     s_initialized = false;
static wdg_layer_t s_last_fault_layer = WDG_LAYER_COUNT;

/* 内部函数：复位系统（记录故障后触发硬件复位） */
static void system_reset_with_fault(wdg_layer_t layer)
{
    s_last_fault_layer = layer;

    /* 记录故障信息 */
    switch (layer)
    {
        case WDG_LAYER_BOARD:
            fault_record(FAULT_LEVEL_FATAL, "watchdog", "BOARD layer timeout");
            break;
        case WDG_LAYER_DEVICE:
            fault_record(FAULT_LEVEL_FATAL, "watchdog", "DEVICE layer timeout");
            break;
        case WDG_LAYER_APP:
            fault_record(FAULT_LEVEL_FATAL, "watchdog", "APP layer timeout");
            break;
        default:
            fault_record(FAULT_LEVEL_FATAL, "watchdog", "unknown layer timeout");
            break;
    }

    /* 触发硬件看门狗复位 */
    watchdog_reboot(0, 0, 0);

    /* 等待复位生效 */
    while (1)
    {
        /* 空循环，防止继续执行 */
    }
}

void watchdog_init(uint32_t timeout_ms)
{
    if (timeout_ms == 0U)
    {
        timeout_ms = 200U;  /* 默认200ms，避免误触发 */
    }

    s_timeout_us = timeout_ms * 1000U;

    /* 初始化所有时间戳为当前时间（避免初始即超时） */
    uint32_t now = time_us_32();
    for (int i = 0; i < WDG_LAYER_COUNT; i++)
    {
        s_last_feed_us[i] = now;
    }

    /* 启用硬件看门狗，超时时间设为逻辑超时的2倍（至少400ms） */
    uint32_t hw_timeout_ms = timeout_ms * 2U;
    if (hw_timeout_ms < 400U)
    {
        hw_timeout_ms = 400U;
    }

    /* 第二个参数：pause_on_debug，调试时暂停看门狗 */
    watchdog_enable(hw_timeout_ms, true);

    s_initialized = true;

    fault_record(FAULT_LEVEL_INFO, "watchdog", "init complete");
}

void watchdog_feed_layer(wdg_layer_t layer)
{
    if (!s_initialized)
    {
        return;
    }
    if (layer >= WDG_LAYER_COUNT)
    {
        fault_record(FAULT_LEVEL_ERROR, "watchdog", "feed invalid layer");
        return;
    }

    /* 更新该层最后喂狗时间 */
    s_last_feed_us[layer] = time_us_32();
}

void watchdog_tick(void)
{
    if (!s_initialized)
    {
        return;
    }

    /* USB 挂起时系统处于休眠状态，跳过逻辑超时检查
     * 只喂硬件看门狗，防止休眠期间误触发复位
     */
    extern bool tud_suspended(void);
    if (tud_suspended())
    {
        watchdog_update();
        return;
    }

    uint32_t now = time_us_32();

    /* 检查每一层是否超时 */
    for (int i = 0; i < WDG_LAYER_COUNT; i++)
    {
        /* 计算差值（利用无符号减法自动处理溢出） */
        uint32_t elapsed = now - s_last_feed_us[i];

        /* 如果差值大于超时阈值，触发复位 */
        if (elapsed > s_timeout_us)
        {
            system_reset_with_fault((wdg_layer_t)i);
            /* 不会返回 */
            return;
        }
    }

    /* 所有层都存活，喂硬件看门狗 */
    watchdog_update();
}

wdg_layer_t watchdog_get_last_fault_layer(void)
{
    return s_last_fault_layer;
}

void watchdog_system_reset(void)
{
    fault_record(FAULT_LEVEL_FATAL, "watchdog", "manual system reset");
    watchdog_reboot(0, 0, 0);
    while (1)
    {
        /* 等待复位 */
    }
}
