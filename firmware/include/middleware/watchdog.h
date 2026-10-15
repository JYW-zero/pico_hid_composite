/*
 * include/middleware/watchdog.h
 * Layered Logical Watchdog
 * Multi-task independent alive timestamps, any module stall triggers hardware reset
 * Hardware watchdog as the last line of defense
 */

#ifndef MIDDLEWARE_WATCHDOG_H
#define MIDDLEWARE_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Layer definitions: from bottom to top */
typedef enum
{
    WDG_LAYER_BOARD = 0,   /* Board initialization, hardware status monitoring */
    WDG_LAYER_DEVICE,      /* All peripheral scan tasks */
    WDG_LAYER_APP,         /* Main loop, HID processing */
    WDG_LAYER_COUNT
} wdg_layer_t;

/* Initialize watchdog (set logical timeout in milliseconds) */
void watchdog_init(uint32_t timeout_ms);

/* Feed watchdog: a layer reports alive */
void watchdog_feed_layer(wdg_layer_t layer);

/* Patrol function: check if any layer has timed out, trigger reset if so
 * Recommended to call at 1ms ~ 5ms interval */
void watchdog_tick(void);

/* Get the last layer that triggered reset (for fault diagnosis) */
wdg_layer_t watchdog_get_last_fault_layer(void);

/* Immediately trigger system reset */
void watchdog_system_reset(void);

/* ==================== Core1 Heartbeat Monitoring ==================== */
/*
 * Core1 deadlock detection: monitor Core1 alive status via heartbeat counter in shared_hw_data.
 * Normally Core1 main loop increments heartbeat every 1ms.
 * If heartbeat does not change for more than CORE1_HEARTBEAT_TIMEOUT_MS, Core1 is considered deadlocked,
 * triggering DEVICE layer watchdog reset.
 *
 * Note: During Flash writes, flash_safe_execute() pauses Core1,
 * at which point watchdog_pause_core1_monitor() must be called to pause monitoring, avoiding false triggers.
 */

/* Core1 heartbeat timeout threshold (milliseconds)
 * Normal scheduling period 1ms, with sufficient margin: 500ms without increment is considered deadlock.
 * Must be greater than the maximum pause time during Flash writes (sector erase ~50ms + program ~10ms).
 */
#define WDG_CORE1_HEARTBEAT_TIMEOUT_MS  500U

/* Pause Core1 heartbeat monitoring (call before Flash write)
 * During pause, watchdog_tick will not check heartbeat, avoiding false deadlock reset
 * when Core1 is paused by flash_safe_execute. Can be nested, internally maintains reference count.
 */
void watchdog_pause_core1_monitor(void);

/* Resume Core1 heartbeat monitoring (call after Flash write completes)
 * Used in pair with watchdog_pause_core1_monitor().
 */
void watchdog_resume_core1_monitor(void);

/* Query whether Core1 monitoring is paused (mainly for testing and debugging) */
bool watchdog_is_core1_monitor_paused(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_WATCHDOG_H */
