/*
 * tests/mock/mock_time.c
 * Mock版时间函数实现
 */

#include "mock_time.h"
#include "pico/time.h"

static uint32_t s_current_time_us = 0;

void mock_time_set(uint32_t time_us)
{
    s_current_time_us = time_us;
}

void mock_time_advance(uint32_t delta_us)
{
    s_current_time_us += delta_us;
}

uint32_t mock_time_get(void)
{
    return s_current_time_us;
}

void mock_time_reset(void)
{
    s_current_time_us = 0;
}

/* 提供 pico-sdk 的 time_us_32() 函数 */
uint32_t time_us_32(void)
{
    return s_current_time_us;
}

/* 获取当前绝对时间 */
absolute_time_t get_absolute_time(void)
{
    absolute_time_t t;
    t._private_us_since_boot = s_current_time_us;
    return t;
}

/* 将绝对时间转换为毫秒 */
uint64_t to_ms_since_boot(absolute_time_t t)
{
    return t._private_us_since_boot / 1000;
}

/* 睡眠指定毫秒（mock 实现：直接推进时间） */
void sleep_ms(uint32_t ms)
{
    s_current_time_us += ms * 1000;
}

/* 睡眠指定微秒（mock 实现：直接推进时间） */
void sleep_us(uint64_t us)
{
    s_current_time_us += (uint32_t)us;
}
