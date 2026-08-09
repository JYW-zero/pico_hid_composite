/*
 * tests/mock/pico/time.h
 * Mock版pico时间头文件，用于单元测试
 */

#ifndef PICO_TIME_H
#define PICO_TIME_H

#include <stdint.h>

/* 获取当前微秒时间戳 */
uint32_t time_us_32(void);

#endif /* PICO_TIME_H */
