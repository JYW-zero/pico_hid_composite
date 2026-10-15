/*
 * tests/test_debounce.c
 * 按键消抖模块单元测试
 */

#include "unity.h"
#include "middleware/debounce.h"


/* 测试1：初始状态都是松开的（全1，因为bit=1表示松开） */
void test_debounce_initial_state(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 5);

    uint64_t state = debounce_64key_get(&db);
    TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, state);
}

/* 测试2：单个按键按下
 * 消抖逻辑：第1次新值重置计数器为1，
 * 之后每次相同计数器+1，当计数器达到threshold后，
 * 下一次采样才更新稳定状态
 * 所以threshold=5时，第6次采样才稳定
 */
void test_debounce_single_key_press(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 5);

    /* 第0位按下（bit=0） */
    uint64_t pressed = 0xFFFFFFFFFFFFFFFEULL;

    /* 前5次采样，状态还没稳定 */
    for (int i = 0; i < 5; i++) {
        uint64_t state = debounce_64key_update(&db, pressed);
        TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, state);
    }

    /* 第6次采样，状态稳定 */
    uint64_t state = debounce_64key_update(&db, pressed);
    TEST_ASSERT_EQUAL_UINT64(pressed, state);
}

/* 测试3：单个按键松开 */
void test_debounce_single_key_release(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 5);

    /* 先让按键稳定按下 */
    uint64_t pressed = 0xFFFFFFFFFFFFFFFEULL;
    for (int i = 0; i < 10; i++) {
        debounce_64key_update(&db, pressed);
    }
    TEST_ASSERT_EQUAL_UINT64(pressed, debounce_64key_get(&db));

    /* 然后松开（全1） */
    uint64_t released = 0xFFFFFFFFFFFFFFFFULL;

    /* 前5次采样，状态还没稳定 */
    for (int i = 0; i < 5; i++) {
        uint64_t state = debounce_64key_update(&db, released);
        TEST_ASSERT_EQUAL_UINT64(pressed, state);
    }

    /* 第6次采样，状态稳定 */
    uint64_t state = debounce_64key_update(&db, released);
    TEST_ASSERT_EQUAL_UINT64(released, state);
}

/* 测试4：多个按键同时按下
 * threshold=3时，第4次采样稳定
 */
void test_debounce_multiple_keys(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 3);

    /* 第0、1、2位都按下 */
    uint64_t pressed = 0xFFFFFFFFFFFFFFF8ULL;

    /* 前3次采样，不稳定 */
    for (int i = 0; i < 3; i++) {
        uint64_t state = debounce_64key_update(&db, pressed);
        TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, state);
    }

    /* 第4次采样，稳定 */
    uint64_t state = debounce_64key_update(&db, pressed);
    TEST_ASSERT_EQUAL_UINT64(pressed, state);
}

/* 测试5：抖动测试 - 快速按下又松开，不应该稳定 */
void test_debounce_bounce(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 5);

    uint64_t pressed = 0xFFFFFFFFFFFFFFFEULL;
    uint64_t released = 0xFFFFFFFFFFFFFFFFULL;

    /* 按下2次，又松开2次，再按下2次... */
    debounce_64key_update(&db, pressed);
    debounce_64key_update(&db, pressed);
    debounce_64key_update(&db, released);
    debounce_64key_update(&db, released);
    debounce_64key_update(&db, pressed);
    debounce_64key_update(&db, pressed);

    /* 因为没有连续7次相同，状态应该还是松开 */
    TEST_ASSERT_EQUAL_UINT64(released, debounce_64key_get(&db));

    /* 再连续按6次，应该稳定 */
    for (int i = 0; i < 6; i++) {
        debounce_64key_update(&db, pressed);
    }
    TEST_ASSERT_EQUAL_UINT64(pressed, debounce_64key_get(&db));
}

/* 测试6：阈值为1时，第2次采样稳定 */
void test_debounce_threshold_1(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 1);

    uint64_t pressed = 0xFFFFFFFFFFFFFFFEULL;

    /* 第1次：重置计数器 */
    uint64_t state = debounce_64key_update(&db, pressed);
    TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, state);

    /* 第2次：计数到1，达到阈值，更新稳定状态 */
    state = debounce_64key_update(&db, pressed);
    TEST_ASSERT_EQUAL_UINT64(pressed, state);
}

/* ==================== 边界条件测试 ==================== */

/* 测试7：设置和获取阈值 */
void test_debounce_set_get_threshold(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 5);

    /* 初始阈值应为5 */
    TEST_ASSERT_EQUAL_UINT8(5, debounce_64key_get_threshold(&db));

    /* 运行时修改阈值为3 */
    debounce_64key_set_threshold(&db, 3);
    TEST_ASSERT_EQUAL_UINT8(3, debounce_64key_get_threshold(&db));

    /* 修改阈值为10 */
    debounce_64key_set_threshold(&db, 10);
    TEST_ASSERT_EQUAL_UINT8(10, debounce_64key_get_threshold(&db));
}

/* 测试8：所有按键同时按下/释放 */
void test_debounce_all_keys_press_release(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 2);

    /* 所有按键同时按下（全0） */
    uint64_t all_pressed = 0x0000000000000000ULL;

    /* 前2次采样，状态还没稳定 */
    for (int i = 0; i < 2; i++) {
        uint64_t state = debounce_64key_update(&db, all_pressed);
        TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, state);
    }

    /* 第3次采样，所有按键稳定按下 */
    uint64_t state = debounce_64key_update(&db, all_pressed);
    TEST_ASSERT_EQUAL_UINT64(all_pressed, state);

    /* 所有按键同时释放（全1） */
    uint64_t all_released = 0xFFFFFFFFFFFFFFFFULL;

    /* 前2次采样，状态还没稳定 */
    for (int i = 0; i < 2; i++) {
        state = debounce_64key_update(&db, all_released);
        TEST_ASSERT_EQUAL_UINT64(all_pressed, state);
    }

    /* 第3次采样，所有按键稳定释放 */
    state = debounce_64key_update(&db, all_released);
    TEST_ASSERT_EQUAL_UINT64(all_released, state);
}

/* 测试9：get函数不改变状态 */
void test_debounce_get_does_not_change_state(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 3);

    /* 先让第0位稳定按下 */
    uint64_t pressed = 0xFFFFFFFFFFFFFFFEULL;
    for (int i = 0; i < 10; i++) {
        debounce_64key_update(&db, pressed);
    }
    TEST_ASSERT_EQUAL_UINT64(pressed, debounce_64key_get(&db));

    /* 连续调用get多次，状态应该保持不变 */
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT64(pressed, debounce_64key_get(&db));
    }
}

/* 测试10：运行时修改阈值后立即生效 */
void test_debounce_threshold_change_runtime(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 5);

    uint64_t pressed = 0xFFFFFFFFFFFFFFFEULL;

    /* 用阈值5采样2次，状态不稳定 */
    debounce_64key_update(&db, pressed);
    debounce_64key_update(&db, pressed);
    TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, debounce_64key_get(&db));

    /* 运行时修改阈值为1 */
    debounce_64key_set_threshold(&db, 1);

    /* 再采样1次，应该立即稳定（因为阈值=1，第2次相同就稳定） */
    uint64_t state = debounce_64key_update(&db, pressed);
    TEST_ASSERT_EQUAL_UINT64(pressed, state);
}

