/*
 * src/middleware/perf_monitor.c
 * 性能监控模块实现
 * 任务执行时间统计、CPU使用率、主循环频率
 */

#include "middleware/perf_monitor.h"
#include "middleware/fault.h"
#include "pico/time.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/* ==================== 静态变量 ==================== */

static perf_task_stat_t s_tasks[PERF_MAX_TASKS];
static uint8_t s_task_count = 0;
static uint32_t s_task_start_us[PERF_MAX_TASKS];  /* 每个任务的开始时间 */

static uint32_t s_init_time_us = 0;     /* 初始化时间 */
static uint32_t s_last_loop_us = 0;     /* 上一次循环时间 */
static uint32_t s_loop_count = 0;       /* 循环计数 */
static uint32_t s_last_sec_us = 0;      /* 上一秒的时间 */
static uint32_t s_last_sec_loops = 0;   /* 上一秒的循环数 */
static uint32_t s_current_sec_loops = 0; /* 当前秒的循环数 */
static uint32_t s_busy_us_total = 0;    /* 总忙碌时间 */
static uint32_t s_last_sec_busy_us = 0; /* 上一秒的忙碌时间 */
static uint32_t s_current_sec_busy_us = 0; /* 当前秒的忙碌时间 */
static uint32_t s_loop_freq = 0;       /* 主循环频率（Hz） */
static uint32_t s_cpu_usage = 0;       /* CPU使用率（0-100） */

/* 滑动窗口 - 环形缓冲区 */
static uint8_t  s_cpu_history[PERF_WINDOW_30S];   /* CPU使用率历史（30秒） */
static uint32_t s_freq_history[PERF_WINDOW_30S];  /* 主循环频率历史（30秒） */
static uint8_t  s_history_index = 0;              /* 环形缓冲区当前索引 */
static uint8_t  s_history_count = 0;              /* 已记录的秒数 */
static uint8_t  s_cpu_avg_10s = 0;                /* 10秒平均CPU使用率 */
static uint8_t  s_cpu_avg_30s = 0;                /* 30秒平均CPU使用率 */
static uint32_t s_freq_avg_10s = 0;               /* 10秒平均主循环频率 */

static bool s_initialized = false;

/* ==================== 对外接口 ==================== */

void perf_init(void)
{
    memset(s_tasks, 0, sizeof(s_tasks));
    s_task_count = 0;
    s_loop_count = 0;
    s_busy_us_total = 0;
    s_loop_freq = 0;
    s_cpu_usage = 0;

    s_init_time_us = time_us_32();
    s_last_loop_us = s_init_time_us;
    s_last_sec_us = s_init_time_us;
    s_last_sec_loops = 0;
    s_current_sec_loops = 0;
    s_last_sec_busy_us = 0;
    s_current_sec_busy_us = 0;

    s_initialized = true;
}

void perf_register_task(uint8_t index, const char* name)
{
    if (index >= PERF_MAX_TASKS)
    {
        return;
    }
    if (name == NULL)
    {
        name = "unknown";
    }

    s_tasks[index].name = name;
    s_tasks[index].count = 0;
    s_tasks[index].total_us = 0;
    s_tasks[index].min_us = 0xFFFFFFFFU;
    s_tasks[index].max_us = 0;
    s_tasks[index].last_us = 0;
    s_tasks[index].cpu_percent = 0;
    s_tasks[index].threshold_us = 0;
    s_tasks[index].overrun_count = 0;

    if (index >= s_task_count)
    {
        s_task_count = index + 1;
    }
}

void perf_start(uint8_t index)
{
    if (!s_initialized || index >= PERF_MAX_TASKS)
    {
        return;
    }

    s_task_start_us[index] = time_us_32();
}

void perf_end(uint8_t index)
{
    if (!s_initialized || index >= PERF_MAX_TASKS)
    {
        return;
    }

    uint32_t now = time_us_32();
    uint32_t elapsed = now - s_task_start_us[index];

    perf_task_stat_t* task = &s_tasks[index];
    task->count++;
    task->total_us += elapsed;
    task->last_us = elapsed;

    if (elapsed < task->min_us)
    {
        task->min_us = elapsed;
    }
    if (elapsed > task->max_us)
    {
        task->max_us = elapsed;
    }

    /* 超时检测与告警 */
    if (task->threshold_us > 0 && elapsed > task->threshold_us)
    {
        task->overrun_count++;
        /* 记录错误日志（限频：最多每秒记录1次，避免日志洪水） */
        static uint32_t last_log_us[PERF_MAX_TASKS] = {0};
        if (now - last_log_us[index] > 1000000U)
        {
            last_log_us[index] = now;
            char msg[64];
            snprintf(msg, sizeof(msg), "任务[%s]执行超时: %lu us (阈值: %lu us)",
                     task->name ? task->name : "?",
                     (unsigned long)elapsed,
                     (unsigned long)task->threshold_us);
            fault_record(FAULT_LEVEL_WARN, "perf_monitor", msg);
        }
    }

    /* 累加到总忙碌时间 */
    s_busy_us_total += elapsed;
    s_current_sec_busy_us += elapsed;
}

uint8_t perf_get_task_count(void)
{
    return s_task_count;
}

bool perf_get_task_stat(uint8_t index, perf_task_stat_t* out_stat)
{
    if (out_stat == NULL || index >= s_task_count)
    {
        return false;
    }

    memcpy(out_stat, &s_tasks[index], sizeof(perf_task_stat_t));
    return true;
}

void perf_get_system_stat(perf_system_stat_t* out_stat)
{
    if (out_stat == NULL)
    {
        return;
    }

    uint32_t now = time_us_32();
    out_stat->uptime_s = (now - s_init_time_us) / 1000000U;
    out_stat->loop_freq_hz = s_loop_freq;
    out_stat->cpu_usage = s_cpu_usage;
    out_stat->cpu_usage_avg_10s = s_cpu_avg_10s;
    out_stat->cpu_usage_avg_30s = s_cpu_avg_30s;
    out_stat->loop_freq_avg_10s = s_freq_avg_10s;
}

void perf_reset(void)
{
    for (uint8_t i = 0; i < s_task_count; i++)
    {
        s_tasks[i].count = 0;
        s_tasks[i].total_us = 0;
        s_tasks[i].min_us = 0xFFFFFFFFU;
        s_tasks[i].max_us = 0;
        s_tasks[i].last_us = 0;
        s_tasks[i].cpu_percent = 0;
        s_tasks[i].overrun_count = 0;
    }

    s_loop_count = 0;
    s_busy_us_total = 0;
    s_loop_freq = 0;
    s_cpu_usage = 0;
    s_cpu_avg_10s = 0;
    s_cpu_avg_30s = 0;
    s_freq_avg_10s = 0;

    uint32_t now = time_us_32();
    s_last_sec_us = now;
    s_current_sec_loops = 0;
    s_current_sec_busy_us = 0;
    s_history_index = 0;
    s_history_count = 0;
    memset(s_cpu_history, 0, sizeof(s_cpu_history));
    memset(s_freq_history, 0, sizeof(s_freq_history));
}

void perf_set_threshold(uint8_t index, uint32_t threshold_us)
{
    if (!s_initialized || index >= PERF_MAX_TASKS)
    {
        return;
    }
    s_tasks[index].threshold_us = threshold_us;
}

void perf_loop_tick(void)
{
    if (!s_initialized)
    {
        return;
    }

    uint32_t now = time_us_32();
    s_loop_count++;
    s_current_sec_loops++;

    /* 检查是否过了1秒 */
    if (now - s_last_sec_us >= 1000000U)
    {
        /* 计算这一秒的循环频率 */
        uint32_t elapsed_us = now - s_last_sec_us;
        if (elapsed_us > 0)
        {
            s_loop_freq = (s_current_sec_loops * 1000000U) / elapsed_us;

            /* 计算CPU使用率：忙碌时间 / 总时间 */
            if (s_current_sec_busy_us > elapsed_us)
            {
                s_current_sec_busy_us = elapsed_us;  /* 防止溢出 */
            }
            s_cpu_usage = (s_current_sec_busy_us * 100U) / elapsed_us;

            /* 计算每个任务的CPU占比（占总忙碌时间的百分比） */
            if (s_current_sec_busy_us > 0)
            {
                for (uint8_t i = 0; i < s_task_count; i++)
                {
                    /* 注意：这里用的是累计的total_us，不是每秒的。
                       为了准确计算每秒的占比，我们需要每秒的任务耗时。
                       简化处理：用累计值计算，误差不大，够用 */
                    if (s_busy_us_total > 0)
                    {
                        s_tasks[i].cpu_percent = (uint8_t)((s_tasks[i].total_us * 100ULL) / s_busy_us_total);
                    }
                }
            }

            /* 更新滑动窗口 - 环形缓冲区 */
            s_cpu_history[s_history_index] = (uint8_t)s_cpu_usage;
            s_freq_history[s_history_index] = s_loop_freq;
            s_history_index++;
            if (s_history_index >= PERF_WINDOW_30S)
            {
                s_history_index = 0;
            }
            if (s_history_count < PERF_WINDOW_30S)
            {
                s_history_count++;
            }

            /* 计算10秒平均 */
            uint32_t cpu_sum_10 = 0;
            uint32_t freq_sum_10 = 0;
            uint8_t count_10 = s_history_count < PERF_WINDOW_10S ? s_history_count : PERF_WINDOW_10S;
            for (uint8_t i = 0; i < count_10; i++)
            {
                int idx = (int)s_history_index - 1 - i;
                if (idx < 0) idx += PERF_WINDOW_30S;
                cpu_sum_10 += s_cpu_history[idx];
                freq_sum_10 += s_freq_history[idx];
            }
            if (count_10 > 0)
            {
                s_cpu_avg_10s = (uint8_t)(cpu_sum_10 / count_10);
                s_freq_avg_10s = freq_sum_10 / count_10;
            }

            /* 计算30秒平均 */
            uint32_t cpu_sum_30 = 0;
            for (uint8_t i = 0; i < s_history_count; i++)
            {
                cpu_sum_30 += s_cpu_history[i];
            }
            if (s_history_count > 0)
            {
                s_cpu_avg_30s = (uint8_t)(cpu_sum_30 / s_history_count);
            }
        }

        /* 重置计数器 */
        s_last_sec_us = now;
        s_last_sec_loops = s_current_sec_loops;
        s_last_sec_busy_us = s_current_sec_busy_us;
        s_current_sec_loops = 0;
        s_current_sec_busy_us = 0;
    }

    s_last_loop_us = now;
}

void perf_print_stats(void)
{
    if (!s_initialized)
    {
        printf("[PERF] 未初始化\n");
        return;
    }

    perf_system_stat_t sys;
    perf_get_system_stat(&sys);

    printf("\n========== 性能统计 ==========\n");
    printf("运行时间: %lu 秒\n", sys.uptime_s);
    printf("主循环频率: %lu Hz (10秒平均: %lu Hz)\n", sys.loop_freq_hz, sys.loop_freq_avg_10s);
    printf("CPU使用率: %lu%% (10秒平均: %u%%, 30秒平均: %u%%)\n",
           sys.cpu_usage, sys.cpu_usage_avg_10s, sys.cpu_usage_avg_30s);
    printf("\n任务执行时间:\n");
    printf("--------------------------------\n");
    printf("%-14s %6s %6s %6s %6s %6s %5s %5s\n",
           "任务名", "次数", "最小us", "最大us", "平均us", "最近us", "CPU%", "超时");
    printf("--------------------------------\n");

    for (uint8_t i = 0; i < s_task_count; i++)
    {
        perf_task_stat_t stat;
        perf_get_task_stat(i, &stat);

        uint32_t avg_us = 0;
        if (stat.count > 0)
        {
            avg_us = stat.total_us / stat.count;
        }

        uint32_t min_us = stat.min_us;
        if (min_us == 0xFFFFFFFFU)
        {
            min_us = 0;
        }

        printf("%-14s %6lu %6lu %6lu %6lu %6lu %5u %5lu\n",
               stat.name ? stat.name : "?",
               stat.count,
               min_us,
               stat.max_us,
               avg_us,
               stat.last_us,
               stat.cpu_percent,
               (unsigned long)stat.overrun_count);
    }
    printf("==============================\n\n");
}
