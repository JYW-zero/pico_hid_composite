/*
 * include/middleware/keyboard_event.h
 * 按键事件回调机制
 * 抽象按键事件（按下/释放/保持），应用层注册回调函数
 * 与具体硬件解耦，支持 64 键位图输入
 */

#ifndef MIDDLEWARE_KEYBOARD_EVENT_H
#define MIDDLEWARE_KEYBOARD_EVENT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/* 最大按键数（64 键位图） */
#define KEYBOARD_MAX_KEYS  64

/* 最大回调函数数量 */
#define KEYBOARD_MAX_CALLBACKS  8

/* ==================== 事件类型 ==================== */

/* 按键事件类型 */
typedef enum
{
    KEYBOARD_EVENT_PRESS = 0,   /* 按键按下（上升沿） */
    KEYBOARD_EVENT_RELEASE,      /* 按键释放（下降沿） */
    KEYBOARD_EVENT_HOLD          /* 按键保持（持续按下，每次扫描都触发） */
} keyboard_event_type_t;

/* 按键事件结构体 */
typedef struct
{
    uint8_t key_index;              /* 键索引（0-63） */
    keyboard_event_type_t type;     /* 事件类型 */
    uint64_t timestamp_ms;          /* 事件时间戳（启动后毫秒数） */
    uint64_t current_state;         /* 当前完整按键状态位图（bit=0 表示按下） */
} keyboard_event_t;

/* 回调函数类型
 * @param event 按键事件
 * @param user_data 用户自定义数据（注册时传入）
 */
typedef void (*keyboard_event_callback_t)(const keyboard_event_t *event, void *user_data);

/* ==================== 接口函数 ==================== */

/**
 * @brief 初始化按键事件模块
 * 清空所有回调注册
 */
void keyboard_event_init(void);

/**
 * @brief 注册按键事件回调
 * @param callback 回调函数（NULL 无效）
 * @param user_data 用户自定义数据（可为 NULL）
 * @return 回调 ID（>=0 成功，<0 失败，如回调已满）
 */
int keyboard_event_register(keyboard_event_callback_t callback, void *user_data);

/**
 * @brief 注销按键事件回调
 * @param callback_id 回调 ID（由 register 返回）
 * @return true=成功，false=ID 无效
 */
bool keyboard_event_unregister(int callback_id);

/**
 * @brief 输入按键状态位图，检测变化并触发回调
 * @param new_state 新的按键状态位图（bit=0 表示按下）
 * @param trigger_hold 是否触发 HOLD 事件（true=每次扫描都对按下的键触发 HOLD，false=只触发 PRESS/RELEASE）
 * @return 触发的事件总数
 */
uint32_t keyboard_event_update(uint64_t new_state, bool trigger_hold);

/**
 * @brief 获取当前按键状态
 * @return 当前按键状态位图（bit=0 表示按下）
 */
uint64_t keyboard_event_get_state(void);

/**
 * @brief 获取指定键是否按下
 * @param key_index 键索引（0-63）
 * @return true=按下，false=释放或索引无效
 */
bool keyboard_event_is_pressed(uint8_t key_index);

/**
 * @brief 获取当前按下的键数量
 * @return 按下的键数量
 */
uint8_t keyboard_event_get_pressed_count(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_KEYBOARD_EVENT_H */
