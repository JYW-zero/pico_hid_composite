/*
 * tests/test_encoder.c
 * 编码器模块单元测试
 */

#include "unity.h"
#include "device/encoder.h"
#include "mock/mock_gpio.h"

/* ==================== 测试用引脚 ==================== */
#define TEST_PIN_A  0
#define TEST_PIN_B  1
#define TEST_PIN_SW 2

static encoder_cfg_t s_cfg;
static encoder_state_t s_state;

/* ==================== 辅助函数 ==================== */

/* 设置AB相电平
 * curr_ab = (a << 1) | b
 *
 * 顺时针(CW, +1)序列：00 → 01 → 11 → 10 → 00
 * 逆时针(CCW, -1)序列：00 → 10 → 11 → 01 → 00
 */
static void set_phase(bool a, bool b)
{
    mock_gpio_set(TEST_PIN_A, a);
    mock_gpio_set(TEST_PIN_B, b);
}

/* 每个测试前初始化 */
static void encoder_test_setup(void)
{
    mock_gpio_reset();

    s_cfg.a_pin = TEST_PIN_A;
    s_cfg.b_pin = TEST_PIN_B;
    s_cfg.sw_pin = TEST_PIN_SW;
    s_cfg.steps_per_tick = 1;  /* 测试用1步一个tick，方便验证 */

    /* 先初始化硬件（配置上拉） */
    encoder_init(&s_cfg);
    encoder_state_init(&s_state);

    /* 然后设置初始相位（模拟外部信号），并调用一次update同步状态 */
    set_phase(false, false);
    encoder_update(&s_cfg, &s_state);
}

/* ==================== 测试用例 ==================== */

/* 测试1：初始化状态 */
void test_encoder_init_state(void)
{
    encoder_test_setup();

    /* 初始化后，last_ab应该是0（00），accum是0 */
    TEST_ASSERT_EQUAL_UINT8(0, s_state.last_ab);
    TEST_ASSERT_EQUAL_INT32(0, s_state.accum);
}

/* 测试2：顺时针旋转 - 完整4步
 * 序列：00 → 01 → 11 → 10 → 00
 * 每步都应该返回CW（+1）
 */
void test_encoder_clockwise_full(void)
{
    encoder_test_setup();
    encoder_dir_e dir;

    /* 初始状态：00，相位不变，返回NONE */
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);

    /* 第1步：00 → 01 */
    set_phase(false, true);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_CW, dir);

    /* 第2步：01 → 11 */
    set_phase(true, true);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_CW, dir);

    /* 第3步：11 → 10 */
    set_phase(true, false);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_CW, dir);

    /* 第4步：10 → 00 */
    set_phase(false, false);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_CW, dir);
}

/* 测试3：逆时针旋转 - 完整4步
 * 序列：00 → 10 → 11 → 01 → 00
 * 每步都应该返回CCW（-1）
 */
void test_encoder_counter_clockwise_full(void)
{
    encoder_test_setup();
    encoder_dir_e dir;

    /* 初始状态：00 */
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);

    /* 第1步：00 → 10 */
    set_phase(true, false);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_CCW, dir);

    /* 第2步：10 → 11 */
    set_phase(true, true);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_CCW, dir);

    /* 第3步：11 → 01 */
    set_phase(false, true);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_CCW, dir);

    /* 第4步：01 → 00 */
    set_phase(false, false);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_CCW, dir);
}

/* 测试4：抖动过滤 - 来回跳变不应该计数
 * 我们用steps_per_tick=2来测试防抖效果
 */
void test_encoder_bounce(void)
{
    encoder_test_setup();
    s_cfg.steps_per_tick = 2;  /* 2步一个tick */
    encoder_dir_e dir;

    /* 初始：00 */
    encoder_update(&s_cfg, &s_state);

    /* 00 → 01：+1，accum=1，还没到2，返回NONE */
    set_phase(false, true);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);
    TEST_ASSERT_EQUAL_INT32(1, s_state.accum);

    /* 01 → 00：-1，accum=0，返回NONE */
    set_phase(false, false);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);
    TEST_ASSERT_EQUAL_INT32(0, s_state.accum);

    /* 00 → 01：+1，accum=1，返回NONE */
    set_phase(false, true);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);
    TEST_ASSERT_EQUAL_INT32(1, s_state.accum);

    /* 01 → 11：再+1，accum=2，达到阈值，返回CW */
    set_phase(true, true);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_CW, dir);
    TEST_ASSERT_EQUAL_INT32(0, s_state.accum);  /* 输出后清零 */
}

/* 测试5：steps_per_tick=4时的防抖效果
 * 顺时针转4步才输出一个tick
 */
void test_encoder_steps_per_tick_4(void)
{
    encoder_test_setup();
    s_cfg.steps_per_tick = 4;
    encoder_dir_e dir;

    /* 初始：00 */
    encoder_update(&s_cfg, &s_state);

    /* 第1步：00 → 01，accum=1 */
    set_phase(false, true);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);
    TEST_ASSERT_EQUAL_INT32(1, s_state.accum);

    /* 第2步：01 → 11，accum=2 */
    set_phase(true, true);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);
    TEST_ASSERT_EQUAL_INT32(2, s_state.accum);

    /* 第3步：11 → 10，accum=3 */
    set_phase(true, false);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);
    TEST_ASSERT_EQUAL_INT32(3, s_state.accum);

    /* 第4步：10 → 00，accum=4，达到阈值，输出CW */
    set_phase(false, false);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_CW, dir);
    TEST_ASSERT_EQUAL_INT32(0, s_state.accum);
}

/* 测试6：方向反转 - 顺时针转2步，再逆时针转2步
 * accum应该回到0
 */
void test_encoder_direction_reverse(void)
{
    encoder_test_setup();
    s_cfg.steps_per_tick = 4;  /* 用大一点的阈值，避免中途输出 */
    encoder_dir_e dir;

    /* 初始：00 */
    encoder_update(&s_cfg, &s_state);

    /* 顺时针2步：00→01→11，accum=2 */
    set_phase(false, true);
    encoder_update(&s_cfg, &s_state);
    set_phase(true, true);
    encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT32(2, s_state.accum);

    /* 逆时针2步回去：11→01→00，accum=0 */
    set_phase(false, true);
    encoder_update(&s_cfg, &s_state);
    set_phase(false, false);
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);
    TEST_ASSERT_EQUAL_INT32(0, s_state.accum);
}

/* 测试7：相位不变时返回NONE */
void test_encoder_no_change(void)
{
    encoder_test_setup();
    encoder_dir_e dir;

    /* 相位不变，再次调用，还是NONE */
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);

    /* 第三次还是NONE */
    dir = encoder_update(&s_cfg, &s_state);
    TEST_ASSERT_EQUAL_INT(ENCODER_DIR_NONE, dir);
}

/* 测试8：中键读取 */
void test_encoder_switch(void)
{
    encoder_test_setup();

    /* 上拉默认高电平，未按下 */
    TEST_ASSERT_FALSE(encoder_read_switch(&s_cfg));

    /* 低电平，按下 */
    mock_gpio_set(TEST_PIN_SW, false);
    TEST_ASSERT_TRUE(encoder_read_switch(&s_cfg));

    /* 恢复高电平，松开 */
    mock_gpio_set(TEST_PIN_SW, true);
    TEST_ASSERT_FALSE(encoder_read_switch(&s_cfg));
}

