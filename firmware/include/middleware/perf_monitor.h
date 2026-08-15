/*
 * include/middleware/perf_monitor.h
 * 性能监控模块
 * 完整版本：任务执行时间统计、CPU使用率、主循环频率
 * 用于实时监控系统运行状态，排查性能问题
 */

#ifndef MIDDLEWARE_PERF_MONITOR_H
#define MIDDLEWARE_PERF_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/* 最大监控任务数量 */
#define PERF_MAX_TASKS     16

/* 滑动窗口大小（秒） */
#define PERF_WINDOW_10S    10
#define PERF_WINDOW_30S    30

/* ==================== 数据结构 ==================== */

/* 任务性能统计 */
typedef struct
{
    const char* name;       /* 任务名称 */
    uint32_t count;         /* 执行次数 */
    uint32_t total_us;      /* 总执行时间（微秒） */
    uint32_t min_us;        /* 最小执行时间 */
    uint32_t max_us;        /* 最大执行时间 */
    uint32_t last_us;       /* 最近一次执行时间 */
    uint8_t  cpu_percent;   /* 任务CPU占比（0-100，占总忙碌时间的百分比） */
    uint32_t threshold_us;  /* 超时告警阈值（微秒），0=不告警 */
    uint32_t overrun_count; /* 超时次数统计 */
} perf_task_stat_t;

/* 系统性能统计 */
typedef struct
{
    uint32_t loop_freq_hz;      /* 主循环频率（Hz） - 瞬时值 */
    uint32_t cpu_usage;         /* CPU使用率（0-100） - 瞬时值 */
    uint32_t uptime_s;          /* 运行时间（秒） */
    uint8_t  cpu_usage_avg_10s; /* 最近10秒平均CPU使用率 */
    uint8_t  cpu_usage_avg_30s; /* 最近30秒平均CPU使用率 */
    uint32_t loop_freq_avg_10s; /* 最近10秒平均主循环频率 */
} perf_system_stat_t;

/* ==================== 对外接口 ==================== */

/**
 * @brief 初始化性能监控模块
 */
void perf_init(void);

/**
 * @brief 注册一个监控任务
 * @param index 任务索引（0~PERF_MAX_TASKS-1）
 * @param name 任务名称
 */
void perf_register_task(uint8_t index, const char* name);

/**
 * @brief 任务开始计时
 * @param index 任务索引
 */
void perf_start(uint8_t index);

/**
 * @brief 任务结束计时
 * @param index 任务索引
 */
void perf_end(uint8_t index);

/**
 * @brief 获取已注册的任务数量
 * @return 任务数量
 */
uint8_t perf_get_task_count(void);

/**
 * @brief 获取指定任务的统计数据
 * @param index 任务索引
 * @param out_stat 输出统计数据
 * @return true=成功，false=索引无效
 */
bool perf_get_task_stat(uint8_t index, perf_task_stat_t* out_stat);

/**
 * @brief 获取系统整体统计数据
 * @param out_stat 输出统计数据
 */
void perf_get_system_stat(perf_system_stat_t* out_stat);

/**
 * @brief 重置所有统计数据
 */
void perf_reset(void);

/**
 * @brief 设置任务超时告警阈值
 * @param index 任务索引
 * @param threshold_us 超时阈值（微秒），0=禁用告警
 */
void perf_set_threshold(uint8_t index, uint32_t threshold_us);

/**
 * @brief 主循环 tick（在主循环开头调用，用于计算循环频率和CPU使用率）
 */
void perf_loop_tick(void);

/**
 * @brief 设置性能监控开关
 * @param enabled true=开启，false=关闭
 * @note 关闭后所有 perf_start/perf_end/perf_loop_tick 直接返回，0开销
 */
void perf_set_enabled(bool enabled);

/**
 * @brief 获取性能监控开关状态
 * @return true=开启，false=关闭
 */
bool perf_is_enabled(void);

/**
 * @brief 打印性能统计到串口（调试用）
 */
void perf_print_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_PERF_MONITOR_H */
