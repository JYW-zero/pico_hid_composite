/*
 * include/middleware/debounce.h
 * 按键消抖中间件
 * 专用于按键信号去抖动，支持64位按键状态
 */

#ifndef MIDDLEWARE_DEBOUNCE_H
#define MIDDLEWARE_DEBOUNCE_H

#include <stdint.h>
#include <stdbool.h>

/* 64键消抖状态结构体 */
typedef struct
{
    uint64_t last_raw;       /* 上次原始读数 */
    uint64_t stable_state;   /* 稳定状态输出 */
    uint8_t debounce_count;  /* 消抖计数器 */
    uint8_t debounce_threshold; /* 消抖阈值(采样次数) */
} debounce_64key_t;

/* 初始化消抖实例 */
void debounce_64key_init(debounce_64key_t *db, uint8_t threshold);

/* 输入原始按键值，返回消抖后的稳定状态 */
uint64_t debounce_64key_update(debounce_64key_t *db, uint64_t raw);

/* 获取当前稳定状态 */
uint64_t debounce_64key_get(const debounce_64key_t *db);

#endif /* MIDDLEWARE_DEBOUNCE_H */
