/*
 * include/middleware/scheduler.h
 * 无OS协作式时间片调度器
 * 采用非阻塞时间片轮询，支持多任务独立周期
 */

#ifndef MIDDLEWARE_SCHEDULER_H
#define MIDDLEWARE_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

/* 任务回调函数原型 */
typedef void (*sched_task_cb)(void);

/* 优先级定义：数值越小优先级越高 */
#define SCHED_PRIORITY_HIGHEST   0
#define SCHED_PRIORITY_HIGH      64
#define SCHED_PRIORITY_NORMAL    128
#define SCHED_PRIORITY_LOW       192
#define SCHED_PRIORITY_LOWEST    255

/* 任务描述结构体 */
typedef struct
{
    uint32_t last_run_us;    /* 上一次执行时间戳 */
    uint32_t interval_us;    /* 执行周期(微秒) */
    uint8_t  priority;       /* 优先级：0最高，255最低 */
    sched_task_cb task_func; /* 任务入口函数 */
} sched_task_t;

/* 调度器初始化 */
void sched_init(void);

/* 主循环调度入口，每轮遍历全部任务 */
void sched_run(const sched_task_t *task_list, uint8_t task_count);

#endif /* MIDDLEWARE_SCHEDULER_H */
