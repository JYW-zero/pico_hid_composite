/*
 * src/middleware/watchdog.c
 * Layered Logical Watchdog Implementation
 * Uses timestamp method to detect alive status of each layer
 * Hardware watchdog as the last line of defense
 */

#include "middleware/watchdog.h"
#include "middleware/fault.h"
#include "middleware/shared_hw_data.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "pico/time.h"

/* Static variables: last feed timestamp for each layer (microseconds) */
static uint32_t s_last_feed_us[WDG_LAYER_COUNT];
static uint32_t s_timeout_us;          /* Logical timeout in microseconds */
static bool     s_initialized = false;
static wdg_layer_t s_last_fault_layer = WDG_LAYER_COUNT;

/* ==================== Core1 Heartbeat Monitoring State ==================== */
/* Last read heartbeat counter value */
static uint32_t s_last_heartbeat = 0;
/* Timestamp of last heartbeat change (microseconds) */
static uint32_t s_last_heartbeat_change_us = 0;
/* Core1 monitor pause reference count (>0 means paused)
 * Uses reference count to support nested calls (e.g., multiple nested Flash operations) */
static uint32_t s_core1_monitor_paused_count = 0;
/* Core1 heartbeat timeout threshold (microseconds) */
static uint32_t s_core1_heartbeat_timeout_us;
/* Records whether at least one heartbeat change has been detected (avoids false positives during startup) */
static bool s_heartbeat_seen_change = false;

/* Pre-reset delay (milliseconds): ensures fault_record Flash write completes
 * Flash sector erase ~30-50ms, page program ~0.5-1ms, with sufficient margin */
#define WDG_RESET_DELAY_MS  100

/* Internal function: reset system (record fault then trigger hardware reset) */
static void system_reset_with_fault(wdg_layer_t layer)
{
    s_last_fault_layer = layer;

    /* 提前将最后故障层写入 watchdog scratch 寄存器（复位后保留，可用于诊断）
     * 放在延迟之前，确保即使延迟期间硬件看门狗意外触发复位，scratch 也已设置 */
    watchdog_hw->scratch[0] = (uint32_t)layer;

    /* Record fault information (FATAL level is not rate-limited, synchronously writes to Flash) */
    switch (layer)
    {
        case WDG_LAYER_BOARD:
            fault_record(FAULT_LEVEL_FATAL, "watchdog", "BOARD layer timeout - system reset");
            break;
        case WDG_LAYER_DEVICE:
            fault_record(FAULT_LEVEL_FATAL, "watchdog", "DEVICE layer timeout - system reset");
            break;
        case WDG_LAYER_APP:
            fault_record(FAULT_LEVEL_FATAL, "watchdog", "APP layer timeout - system reset");
            break;
        default:
            fault_record(FAULT_LEVEL_FATAL, "watchdog", "unknown layer timeout - system reset");
            break;
    }

    /* 延迟复位，确保 Flash 写入完成
     * 使用忙等待，因为此时系统已经异常，不能依赖调度器 */
    {
        uint32_t start = time_us_32();
        while ((time_us_32() - start) < (WDG_RESET_DELAY_MS * 1000U))
        {
            /* Feed hardware watchdog, preventing premature reset during delay */
            watchdog_update();
        }
    }

    /* Trigger hardware watchdog reset (delay 0ms, immediate reset) */
    watchdog_reboot(0, 0, 0);

    /* Wait for reset to take effect */
    while (1)
    {
        /* Empty loop, preventing continued execution */
    }
}

void watchdog_init(uint32_t timeout_ms)
{
    /* Detect whether it is a watchdog reset at startup, and record the reason */
    if (watchdog_caused_reboot())
    {
        /* 读取复位原因（watchdog_enable 的 scratch 寄存器可以存储自定义数据）
         * RP2350 的 watchdog 有 4 个 scratch 寄存器，复位后保留
         * 这里使用 scratch[0] 存储最后故障层 */
        uint32_t scratch = watchdog_hw->scratch[0];
        if (scratch != 0 && scratch < WDG_LAYER_COUNT)
        {
            s_last_fault_layer = (wdg_layer_t)scratch;
        }

        /* Record watchdog reset event (INFO level, does not trigger rate limiting issues) */
        const char* layer_name = "unknown";
        switch (s_last_fault_layer)
        {
            case WDG_LAYER_BOARD:  layer_name = "BOARD"; break;
            case WDG_LAYER_DEVICE: layer_name = "DEVICE"; break;
            case WDG_LAYER_APP:    layer_name = "APP"; break;
            default: break;
        }

        /* 注意：此时 fault_init 可能还未调用，先记录到串口
         * fault_init 后会从 Flash 加载历史日志 */
        printf("[WATCHDOG] System reset caused by watchdog (last fault layer: %s)\n", layer_name);
    }

    if (timeout_ms == 0U)
    {
        timeout_ms = 1000U;  /* Default 1 second logical timeout, meets user requirement of 1~2 second range */
    }

    s_timeout_us = timeout_ms * 1000U;

    /* Initialize Core1 heartbeat timeout threshold */
    s_core1_heartbeat_timeout_us = WDG_CORE1_HEARTBEAT_TIMEOUT_MS * 1000U;
    s_last_heartbeat = 0;
    s_last_heartbeat_change_us = 0;
    s_core1_monitor_paused_count = 0;
    s_heartbeat_seen_change = false;

    /* Initialize all timestamps to current time (avoid initial timeout) */
    uint32_t now = time_us_32();
    for (int i = 0; i < WDG_LAYER_COUNT; i++)
    {
        s_last_feed_us[i] = now;
    }
    s_last_heartbeat_change_us = now;

    /* 启用硬件看门狗，超时时间设为逻辑超时的2倍
     * 逻辑超时1秒 → 硬件超时2秒，符合用户要求的1~2秒范围 */
    uint32_t hw_timeout_ms = timeout_ms * 2U;
    if (hw_timeout_ms < 400U)
    {
        hw_timeout_ms = 400U;
    }

    /* Second parameter: pause_on_debug, pause watchdog during debugging */
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

    /* Update last feed time for this layer */
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

    /* ==================== Core1 心跳监控（DEVICE 层存活检测） ====================
     *
     * 原方案：Core0 无条件代为喂 DEVICE 层，导致 Core1 死锁无法被检测。
     * 新方案：通过 shared_hw_data 的 heartbeat 计数器监控 Core1 存活状态。
     *
     * - 正常情况：Core1 主循环每 1ms 递增 heartbeat → 喂 DEVICE 层
     * - Flash 写入：flash_safe_execute() 暂停 Core1，此时监控已被显式暂停
     *   （通过 watchdog_pause_core1_monitor()），在此期间正常喂 DEVICE 层
     * - Core1 死锁：heartbeat 停止递增 → 超过阈值触发 DEVICE 层复位
     */
    if (s_core1_monitor_paused_count > 0)
    {
        /* 监控已暂停（Flash 写入中），Core1 被 flash_safe_execute 正常暂停，
         * 正常喂 DEVICE 层避免误触发 */
        s_last_feed_us[WDG_LAYER_DEVICE] = now;
    }
    else
    {
        /* Monitoring not paused, check if heartbeat has changed */
        uint32_t current_heartbeat = shared_hw_get_heartbeat();

        if (current_heartbeat != s_last_heartbeat)
        {
            /* heartbeat changed, Core1 is alive */
            s_last_heartbeat = current_heartbeat;
            s_last_heartbeat_change_us = now;
            s_heartbeat_seen_change = true;
            s_last_feed_us[WDG_LAYER_DEVICE] = now;
        }
        else if (s_heartbeat_seen_change)
        {
            /* heartbeat 未变化，且已至少见过一次变化（排除启动初期）
             * 检查是否超过死锁阈值 */
            uint32_t elapsed = now - s_last_heartbeat_change_us;
            if (elapsed > s_core1_heartbeat_timeout_us)
            {
                /* Core1 heartbeat timeout,判定 deadlock, trigger DEVICE layer reset */
                printf("[WATCHDOG] Core1 heartbeat timeout! last_hb=%u, elapsed=%ums\n",
                       (unsigned)current_heartbeat, (unsigned)(elapsed / 1000U));
                system_reset_with_fault(WDG_LAYER_DEVICE);
                /* Will not return */
                return;
            }
            /* 未超过阈值，DEVICE 层时间戳不更新（让常规超时检查兜底）
             * 注意：常规超时 s_timeout_us 通常为 1000ms，
             * 而 heartbeat 超时为 500ms，所以 heartbeat 会先触发 */
        }
        else
        {
            /* 启动初期还未见过 heartbeat 变化，更新时间戳避免误报
             * （Core1 启动可能需要几十 ms） */
            s_last_heartbeat_change_us = now;
        }
    }

    /* Check whether each layer has timed out */
    for (int i = 0; i < WDG_LAYER_COUNT; i++)
    {
        /* Calculate difference (use unsigned subtraction to automatically handle overflow) */
        uint32_t elapsed = now - s_last_feed_us[i];

        /* If difference exceeds timeout threshold, trigger reset */
        if (elapsed > s_timeout_us)
        {
            system_reset_with_fault((wdg_layer_t)i);
            /* Will not return */
            return;
        }
    }

    /* All layers are alive, feed hardware watchdog */
    watchdog_update();
}

wdg_layer_t watchdog_get_last_fault_layer(void)
{
    return s_last_fault_layer;
}

void watchdog_system_reset(void)
{
    fault_record(FAULT_LEVEL_FATAL, "watchdog", "manual system reset");

    /* Delay reset, ensuring Flash write completes */
    uint32_t start = time_us_32();
    while ((time_us_32() - start) < (WDG_RESET_DELAY_MS * 1000U))
    {
        watchdog_update();
    }

    /* Mark as manual reset (use special value 0xFF) */
    watchdog_hw->scratch[0] = 0xFF;

    watchdog_reboot(0, 0, 0);
    while (1)
    {
        /* Wait for reset */
    }
}

/* ==================== Core1 Heartbeat Monitoring Interface Implementation ==================== */

void watchdog_pause_core1_monitor(void)
{
    /* 引用计数 +1，支持嵌套调用
     * 注意：本函数可能在中断禁用的上下文中调用（Flash 写入前），
     * 所以不使用任何可能阻塞的操作
     * 溢出保护：防止计数溢出回0导致监控意外恢复（理论极端场景） */
    if (s_core1_monitor_paused_count < UINT32_MAX) {
        s_core1_monitor_paused_count++;
    }

    /* Update DEVICE layer timestamp during pause, avoiding time accumulated during pause causing immediate timeout after resume */
    if (s_initialized)
    {
        s_last_feed_us[WDG_LAYER_DEVICE] = time_us_32();
    }
}

void watchdog_resume_core1_monitor(void)
{
    if (s_core1_monitor_paused_count > 0)
    {
        s_core1_monitor_paused_count--;
    }

    /* 恢复时更新 heartbeat 变化时间戳，避免暂停期间 heartbeat 不变化
     * 导致恢复后立即判定为死锁 */
    if (s_initialized && s_core1_monitor_paused_count == 0)
    {
        uint32_t now = time_us_32();
        s_last_heartbeat = shared_hw_get_heartbeat();
        s_last_heartbeat_change_us = now;
        s_last_feed_us[WDG_LAYER_DEVICE] = now;
    }
}

bool watchdog_is_core1_monitor_paused(void)
{
    return (s_core1_monitor_paused_count > 0);
}
