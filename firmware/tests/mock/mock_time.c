/*
 * tests/mock/mock_time.c
 * Mock版时间函数实现
 */

#include "mock_time.h"

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
