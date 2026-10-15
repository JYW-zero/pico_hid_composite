/*
 * tests/mock/pico/stdlib.h
 * Mock版pico标准库头文件
 */

#ifndef _PICO_STDLIB_H
#define _PICO_STDLIB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "pico/time.h"
#include "pico/platform.h"

typedef unsigned int uint;

/* 初始化 stdlib */
void stdio_init_all(void);

/* 睡眠指定毫秒 */
void sleep_ms(uint32_t ms);

/* 睡眠指定微秒 */
void sleep_us(uint64_t us);

/* 获取当前微秒时间戳 */
uint32_t time_us_32(void);

/* 绝对时间 */
absolute_time_t get_absolute_time(void);

/* 绝对时间转毫秒 */
uint64_t to_ms_since_boot(absolute_time_t t);

#endif /* _PICO_STDLIB_H */
