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
 * 消抖逻辑：第1次新值重置计数器为0，
 * 之后每次相同计数器+1，当计数器达到threshold后，
 * 下一次采样才更新稳定状态
 * 所以threshold=5时，第7次采样才稳定
 */
void test_debounce_single_key_press(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 5);

    /* 第0位按下（bit=0） */
    uint64_t pressed = 0xFFFFFFFFFFFFFFFEULL;

    /* 前6次采样，状态还没稳定 */
    for (int i = 0; i < 6; i++) {
        uint64_t state = debounce_64key_update(&db, pressed);
        TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, state);
    }

    /* 第7次采样，状态稳定 */
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

    /* 前6次采样，状态还没稳定 */
    for (int i = 0; i < 6; i++) {
        uint64_t state = debounce_64key_update(&db, released);
        TEST_ASSERT_EQUAL_UINT64(pressed, state);
    }

    /* 第7次采样，状态稳定 */
    uint64_t state = debounce_64key_update(&db, released);
    TEST_ASSERT_EQUAL_UINT64(released, state);
}

/* 测试4：多个按键同时按下
 * threshold=3时，第5次采样稳定
 */
void test_debounce_multiple_keys(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 3);

    /* 第0、1、2位都按下 */
    uint64_t pressed = 0xFFFFFFFFFFFFFFF8ULL;

    /* 前4次采样，不稳定 */
    for (int i = 0; i < 4; i++) {
        uint64_t state = debounce_64key_update(&db, pressed);
        TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, state);
    }

    /* 第5次采样，稳定 */
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

    /* 再连续按7次，应该稳定 */
    for (int i = 0; i < 7; i++) {
        debounce_64key_update(&db, pressed);
    }
    TEST_ASSERT_EQUAL_UINT64(pressed, debounce_64key_get(&db));
}

/* 测试6：阈值为1时，第3次采样稳定 */
void test_debounce_threshold_1(void)
{
    debounce_64key_t db;
    debounce_64key_init(&db, 1);

    uint64_t pressed = 0xFFFFFFFFFFFFFFFEULL;

    /* 第1次：重置计数器 */
    uint64_t state = debounce_64key_update(&db, pressed);
    TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, state);

    /* 第2次：计数到1 */
    state = debounce_64key_update(&db, pressed);
    TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, state);

    /* 第3次：达到阈值，更新稳定状态 */
    state = debounce_64key_update(&db, pressed);
    TEST_ASSERT_EQUAL_UINT64(pressed, state);
}

