/*
 * tests/mock/pico/time.h
 * Mock版Pico时间相关头文件
 * 用于单元测试，定义时间相关类型和函数
 */
#ifndef PICO_TIME_H
#define PICO_TIME_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 绝对时间类型（mock：简单的uint64_t封装） */
typedef struct {
    uint64_t _private_us_since_boot;
} absolute_time_t;

/* 获取当前绝对时间（mock：返回递增的计数器） */
absolute_time_t get_absolute_time(void);

/* 获取当前微秒时间（32位） */
uint32_t time_us_32(void);

/* 将绝对时间转换为毫秒数 */
uint64_t to_ms_since_boot(absolute_time_t t);

/* 将绝对时间转换为微秒数 */
uint64_t to_us_since_boot(absolute_time_t t);

/* 延迟指定毫秒（mock：空操作） */
void sleep_ms(uint32_t ms);

/* 延迟指定微秒（mock：空操作） */
void sleep_us(uint64_t us);

/* 延迟到指定绝对时间（mock：空操作） */
void sleep_until(absolute_time_t target);

/* 比较两个绝对时间（mock：简单比较） */
bool time_reached(absolute_time_t target);

/* 绝对时间差值（微秒） */
int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to);

/* 构造绝对时间（从启动毫秒数） */
absolute_time_t make_timeout_time_ms(uint32_t ms);

/* 构造绝对时间（从启动微秒数） */
absolute_time_t make_timeout_time_us(uint64_t us);

/* 从微秒数构造绝对时间 */
absolute_time_t at_the_end_of_time(void);

/* 零时间点 */
absolute_time_t nil_time(void);

#ifdef __cplusplus
}
#endif

#endif /* PICO_TIME_H */
