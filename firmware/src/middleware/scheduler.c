/*
 * src/middleware/scheduler.c
 * 无OS协作式时间片调度器实现
 * 采用int32_t差值处理time_us_32溢出，杜绝长时间运行任务风暴
 * 支持任务优先级，高优先级任务先执行
 */

#include "middleware/scheduler.h"
#include "pico/time.h"

void sched_init(void)
{
    /* 空初始化，首次运行自动填充时间戳 */
}

void sched_run(const sched_task_t *task_list, uint8_t task_count)
{
    const uint32_t now = time_us_32();
    uint8_t ready_indices[16];  /* 最多支持16个任务 */
    uint8_t ready_count = 0;

    /* 第一步：收集所有到期的任务 */
    for (uint8_t i = 0; i < task_count; i++)
    {
        sched_task_t *task = (sched_task_t *)&task_list[i];
        if (task->task_func == NULL)
        {
            continue;
        }

        /* 溢出安全差值计算：转int32判断是否超时 */
        const int32_t delta_us = (int32_t)(now - task->last_run_us);
        if (delta_us >= (int32_t)task->interval_us)
        {
            if (ready_count < 16)
            {
                ready_indices[ready_count] = i;
                ready_count++;
            }
        }
    }

    /* 第二步：按优先级从高到低排序（冒泡排序，任务数量少，简单够用） */
    for (uint8_t i = 0; i < ready_count; i++)
    {
        for (uint8_t j = i + 1; j < ready_count; j++)
        {
            uint8_t prio_i = task_list[ready_indices[i]].priority;
            uint8_t prio_j = task_list[ready_indices[j]].priority;
            if (prio_i > prio_j)
            {
                /* 交换 */
                uint8_t tmp = ready_indices[i];
                ready_indices[i] = ready_indices[j];
                ready_indices[j] = tmp;
            }
        }
    }

    /* 第三步：按优先级顺序执行任务 */
    for (uint8_t i = 0; i < ready_count; i++)
    {
        sched_task_t *task = (sched_task_t *)&task_list[ready_indices[i]];
        task->last_run_us = now;
        task->task_func();
    }
}
