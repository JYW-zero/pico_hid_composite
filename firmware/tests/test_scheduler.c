/*
 * tests/test_scheduler.c
 * 调度器模块单元测试
 */

#include "unity.h"
#include "middleware/scheduler.h"
#include "mock/mock_time.h"

/* ==================== 测试用全局变量 ==================== */

/* 记录每个任务的执行次数 */
static int s_task_count[8];

/* 记录执行顺序（用于验证优先级） */
static int s_exec_order[16];
static int s_exec_order_count;

/* ==================== 测试任务函数 ==================== */

static void task_0(void)  { s_task_count[0]++; s_exec_order[s_exec_order_count++] = 0; }
static void task_1(void)  { s_task_count[1]++; s_exec_order[s_exec_order_count++] = 1; }
static void task_2(void)  { s_task_count[2]++; s_exec_order[s_exec_order_count++] = 2; }
static void task_3(void)  { s_task_count[3]++; s_exec_order[s_exec_order_count++] = 3; }

/* ==================== 辅助函数 ==================== */

/* 重置所有计数 */
static void reset_counts(void)
{
    for (int i = 0; i < 8; i++)
    {
        s_task_count[i] = 0;
    }
    s_exec_order_count = 0;
}

/* 每个测试前初始化 */
static void scheduler_test_setup(void)
{
    mock_time_reset();
    reset_counts();
    sched_init();
}

/* ==================== 测试用例 ==================== */

/* 测试1：初始化 - sched_init可以正常调用 */
void test_scheduler_init(void)
{
    scheduler_test_setup();
    /* 只要不崩溃就算通过 */
    TEST_PASS();
}

/* 测试2：单任务按时执行
 * 周期100us，每次推进20us，推进10次（200us），应该执行2次
 */
void test_scheduler_single_task(void)
{
    scheduler_test_setup();

    sched_task_t tasks[] =
    {
        { .last_run_us = 0, .interval_us = 100, .priority = 128, .task_func = task_0 }
    };

    /* 初始状态：t=0，last_run=0，delta=0，0 >= 100？不，不执行 */
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(0, s_task_count[0]);

    /* 推进到50us：delta=50，不执行 */
    mock_time_advance(50);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(0, s_task_count[0]);

    /* 推进到100us：delta=100，100 >= 100，执行1次 */
    mock_time_advance(50);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[0]);

    /* 再推进到150us：delta=50，不执行 */
    mock_time_advance(50);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[0]);

    /* 再推进到200us：delta=100，执行第2次 */
    mock_time_advance(50);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(2, s_task_count[0]);
}

/* 测试3：多任务独立周期
 * task0: 100us周期
 * task1: 200us周期
 * 推进到200us时，task0执行2次，task1执行1次
 */
void test_scheduler_multiple_tasks(void)
{
    scheduler_test_setup();

    sched_task_t tasks[] =
    {
        { .last_run_us = 0, .interval_us = 100, .priority = 128, .task_func = task_0 },
        { .last_run_us = 0, .interval_us = 200, .priority = 128, .task_func = task_1 }
    };

    /* t=100us: task0执行，task1不执行 */
    mock_time_set(100);
    sched_run(tasks, 2);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[0]);
    TEST_ASSERT_EQUAL_INT(0, s_task_count[1]);

    /* t=200us: task0和task1都执行 */
    mock_time_set(200);
    sched_run(tasks, 2);
    TEST_ASSERT_EQUAL_INT(2, s_task_count[0]);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[1]);

    /* t=300us: task0执行，task1不执行 */
    mock_time_set(300);
    sched_run(tasks, 2);
    TEST_ASSERT_EQUAL_INT(3, s_task_count[0]);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[1]);

    /* t=400us: task0和task1都执行 */
    mock_time_set(400);
    sched_run(tasks, 2);
    TEST_ASSERT_EQUAL_INT(4, s_task_count[0]);
    TEST_ASSERT_EQUAL_INT(2, s_task_count[1]);
}

/* 测试4：优先级排序 - 高优先级任务先执行
 * 三个任务同时到期，优先级分别是0(高), 128(中), 255(低)
 * 执行顺序应该是：task0(0) -> task1(128) -> task2(255)
 *
 * 注意：任务在数组里的顺序是反过来的（低优先级在前），
 * 验证排序是否正确
 */
void test_scheduler_priority_order(void)
{
    scheduler_test_setup();

    /* 任务数组顺序：低优先级在前，高优先级在后 */
    sched_task_t tasks[] =
    {
        { .last_run_us = 0, .interval_us = 100, .priority = 255, .task_func = task_2 },  /* 最低 */
        { .last_run_us = 0, .interval_us = 100, .priority = 0,   .task_func = task_0 },  /* 最高 */
        { .last_run_us = 0, .interval_us = 100, .priority = 128, .task_func = task_1 }   /* 中等 */
    };

    /* 推进到100us，三个任务同时到期 */
    mock_time_set(100);
    sched_run(tasks, 3);

    /* 三个任务都执行了 */
    TEST_ASSERT_EQUAL_INT(1, s_task_count[0]);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[1]);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[2]);

    /* 执行顺序：优先级0 -> 128 -> 255，也就是 task0 -> task1 -> task2 */
    TEST_ASSERT_EQUAL_INT(3, s_exec_order_count);
    TEST_ASSERT_EQUAL_INT(0, s_exec_order[0]);  /* 最高优先级先执行 */
    TEST_ASSERT_EQUAL_INT(1, s_exec_order[1]);  /* 中等 */
    TEST_ASSERT_EQUAL_INT(2, s_exec_order[2]);  /* 最低优先级最后执行 */
}

/* 测试5：NULL任务函数被跳过 */
void test_scheduler_null_task(void)
{
    scheduler_test_setup();

    sched_task_t tasks[] =
    {
        { .last_run_us = 0, .interval_us = 100, .priority = 128, .task_func = NULL },     /* NULL任务 */
        { .last_run_us = 0, .interval_us = 100, .priority = 128, .task_func = task_1 }    /* 正常任务 */
    };

    mock_time_set(100);
    sched_run(tasks, 2);

    /* NULL任务不执行，正常任务执行 */
    TEST_ASSERT_EQUAL_INT(0, s_task_count[0]);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[1]);
}

/* 测试6：0个任务不崩溃 */
void test_scheduler_zero_count(void)
{
    scheduler_test_setup();

    sched_task_t tasks[1];  /* 空数组，只是占位 */

    /* 0个任务，应该正常返回，不崩溃 */
    sched_run(tasks, 0);
    TEST_PASS();
}

/* 测试7：时间溢出回绕
 * 模拟32位微秒计数器溢出（约71分钟）
 * 设置时间接近溢出边界，然后推进一小步，验证调度器正常工作
 *
 * 原理：使用int32_t差值计算，溢出后差值仍然正确
 * 例如：now=0xFFFFFFF0, last=0xFFFFFF00, 差值=0xF0=240（正数）
 */
void test_scheduler_overflow(void)
{
    scheduler_test_setup();

    /* 设置last_run_us为接近溢出的位置 */
    sched_task_t tasks[] =
    {
        { .last_run_us = 0xFFFFFF00, .interval_us = 100, .priority = 128, .task_func = task_0 }
    };

    /* t=0xFFFFFF00 + 50 = 0xFFFFFF50，差值=80 < 100，不执行 */
    mock_time_set(0xFFFFFF50);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(0, s_task_count[0]);

    /* t=0xFFFFFF00 + 100 = 0xFFFFFFA0，差值=160 >= 100，执行 */
    mock_time_set(0xFFFFFFA0);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[0]);

    /* 执行后last_run_us更新为now=0xFFFFFFA0 */
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFA0, tasks[0].last_run_us);

    /* 继续推进到溢出后：t=0x00000050（溢出后50us）
     * last=0xFFFFFFA0, now=0x00000050
     * 差值 = now - last = 0x00000050 - 0xFFFFFFA0 = 0xB0 = 176（int32_t为正）
     * 176 >= 100，应该执行
     */
    mock_time_set(0x00000050);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(2, s_task_count[0]);
}

/* 测试8：首次运行 - last_run_us=0时的行为
 * 当系统刚启动，last_run_us都是0，时间也是0，不应该执行
 * 时间推进到interval_us后才执行
 */
void test_scheduler_first_run(void)
{
    scheduler_test_setup();

    sched_task_t tasks[] =
    {
        { .last_run_us = 0, .interval_us = 1000, .priority = 128, .task_func = task_0 }
    };

    /* t=0，last=0，delta=0 < 1000，不执行 */
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(0, s_task_count[0]);

    /* t=500，delta=500 < 1000，不执行 */
    mock_time_set(500);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(0, s_task_count[0]);

    /* t=1000，delta=1000 >= 1000，执行 */
    mock_time_set(1000);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[0]);
}

/* 测试9：执行后更新last_run_us
 * 任务执行后，last_run_us应该被设置为当前时间now
 */
void test_scheduler_updates_last_run(void)
{
    scheduler_test_setup();

    sched_task_t tasks[] =
    {
        { .last_run_us = 0, .interval_us = 100, .priority = 128, .task_func = task_0 }
    };

    /* t=150时执行 */
    mock_time_set(150);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[0]);

    /* last_run_us应该被更新为150 */
    TEST_ASSERT_EQUAL_UINT32(150, tasks[0].last_run_us);

    /* t=200时，delta=200-150=50 < 100，不执行 */
    mock_time_set(200);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(1, s_task_count[0]);

    /* t=250时，delta=100 >= 100，执行 */
    mock_time_set(250);
    sched_run(tasks, 1);
    TEST_ASSERT_EQUAL_INT(2, s_task_count[0]);
    TEST_ASSERT_EQUAL_UINT32(250, tasks[0].last_run_us);
}
