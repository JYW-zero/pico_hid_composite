/*
 * include/middleware/perf_monitor.h
 * Performance Monitor Module
 * Full version: task execution time stats, CPU usage, main loop frequency
 * For real-time system monitoring and performance troubleshooting
 */

#ifndef MIDDLEWARE_PERF_MONITOR_H
#define MIDDLEWARE_PERF_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "project_features.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Constant Definitions ==================== */

/* Maximum number of monitored tasks */
#define PERF_MAX_TASKS     16

/* Sliding window size (seconds) */
#define PERF_WINDOW_10S    10
#define PERF_WINDOW_30S    30

/* ==================== Data Structures ==================== */

/* Task performance statistics */
typedef struct
{
    const char* name;       /* Task name */
    uint32_t count;         /* Execution count */
    uint32_t total_us;      /* Total execution time (microseconds) */
    uint32_t min_us;        /* Minimum execution time */
    uint32_t max_us;        /* Maximum execution time */
    uint32_t last_us;       /* Last execution time */
    uint8_t  cpu_percent;   /* Task CPU percentage (0-100, percentage of total busy time) */
    uint32_t threshold_us;  /* Timeout alarm threshold (microseconds), 0=disabled */
    uint32_t overrun_count; /* Timeout count */
} perf_task_stat_t;

/* System performance statistics */
typedef struct
{
    uint32_t loop_freq_hz;      /* Main loop frequency (Hz) - instantaneous */
    uint32_t cpu_usage;         /* CPU usage (0-100) - instantaneous */
    uint32_t uptime_s;          /* Uptime (seconds) */
    uint8_t  cpu_usage_avg_10s; /* Average CPU usage over last 10 seconds */
    uint8_t  cpu_usage_avg_30s; /* Average CPU usage over last 30 seconds */
    uint32_t loop_freq_avg_10s; /* Average main loop frequency over last 10 seconds */
} perf_system_stat_t;

/* ==================== Public API ==================== */

#if FEATURE_PERF_MONITOR

/**
 * @brief Initialize performance monitor module
 */
void perf_init(void);

/**
 * @brief Register a monitored task
 * @param index Task index (0~PERF_MAX_TASKS-1)
 * @param name Task name
 */
void perf_register_task(uint8_t index, const char* name);

/**
 * @brief Start task timing
 * @param index Task index
 */
void perf_start(uint8_t index);

/**
 * @brief End task timing
 * @param index Task index
 */
void perf_end(uint8_t index);

/**
 * @brief Get number of registered tasks
 * @return Number of tasks
 */
uint8_t perf_get_task_count(void);

/**
 * @brief Get statistics for specified task
 * @param index Task index
 * @param out_stat Output statistics
 * @return true=success, false=invalid index
 */
bool perf_get_task_stat(uint8_t index, perf_task_stat_t* out_stat);

/**
 * @brief Get overall system statistics
 * @param out_stat Output statistics
 */
void perf_get_system_stat(perf_system_stat_t* out_stat);

/**
 * @brief Reset all statistics
 */
void perf_reset(void);

/**
 * @brief Set task timeout alarm threshold
 * @param index Task index
 * @param threshold_us Timeout threshold (microseconds), 0=disable alarm
 */
void perf_set_threshold(uint8_t index, uint32_t threshold_us);

/**
 * @brief Batch load all task timeout thresholds from config
 * @param thresholds Threshold array (unit: 100 microseconds, 0=disable alarm)
 * @param count Array length (max PERF_MAX_TASKS)
 * @note Thresholds stored in config are in 100us units, this function auto-converts to us
 */
void perf_load_thresholds(const uint16_t* thresholds, uint8_t count);

/**
 * @brief Main loop tick (call at start of main loop, for calculating loop frequency and CPU usage)
 */
void perf_loop_tick(void);

/**
 * @brief Set performance monitor enable/disable
 * @param enabled true=enabled, false=disabled
 * @note When disabled, all perf_start/perf_end/perf_loop_tick return immediately, zero overhead
 */
void perf_set_enabled(bool enabled);

/**
 * @brief Get performance monitor enable status
 * @return true=enabled, false=disabled
 */
bool perf_is_enabled(void);

/**
 * @brief Print performance statistics to serial port (for debugging)
 */
void perf_print_stats(void);

#else  /* FEATURE_PERF_MONITOR == 0：release 空桩，编译期消除、零运行时开销，
          使 core1_scanner 等处的埋点无需散落 #if 包裹 */

static inline void perf_init(void) {}
static inline void perf_register_task(uint8_t index, const char* name) { (void)index; (void)name; }
static inline void perf_start(uint8_t index) { (void)index; }
static inline void perf_end(uint8_t index) { (void)index; }
static inline uint8_t perf_get_task_count(void) { return 0; }
static inline bool perf_get_task_stat(uint8_t index, perf_task_stat_t* out_stat) {
    (void)index;
    if (out_stat) memset(out_stat, 0, sizeof(*out_stat));
    return false;
}
static inline void perf_get_system_stat(perf_system_stat_t* out_stat) {
    if (out_stat) memset(out_stat, 0, sizeof(*out_stat));
}
static inline void perf_reset(void) {}
static inline void perf_set_threshold(uint8_t index, uint32_t threshold_us) { (void)index; (void)threshold_us; }
static inline void perf_load_thresholds(const uint16_t* thresholds, uint8_t count) { (void)thresholds; (void)count; }
static inline void perf_loop_tick(void) {}
static inline void perf_set_enabled(bool enabled) { (void)enabled; }
static inline bool perf_is_enabled(void) { return false; }
static inline void perf_print_stats(void) {}

#endif /* FEATURE_PERF_MONITOR */

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_PERF_MONITOR_H */
