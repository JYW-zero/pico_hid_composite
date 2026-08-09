/*
 * tests/mock/mock_time.h
 * Mock版时间函数，用于单元测试
 */

#ifndef MOCK_TIME_H
#define MOCK_TIME_H

#include <stdint.h>

/* 设置当前时间（微秒） */
void mock_time_set(uint32_t time_us);

/* 推进时间（微秒） */
void mock_time_advance(uint32_t delta_us);

/* 获取当前时间（微秒） */
uint32_t mock_time_get(void);

/* 重置时间为0 */
void mock_time_reset(void);

#endif /* MOCK_TIME_H */
