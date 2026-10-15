/*
 * include/middleware/encoder_processor.h
 * 编码器处理器中间件
 * 功能：多模式切换、旋转加速度、去抖与方向判断
 * 与硬件解耦，输入原始旋转事件，输出处理后的编码器事件
 */

#ifndef MIDDLEWARE_ENCODER_PROCESSOR_H
#define MIDDLEWARE_ENCODER_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "device/encoder.h"  /* 复用 encoder_dir_e 定义 */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/* 编码器模式定义 */
typedef enum
{
    ENCODER_MODE_SCROLL = 0,    /* 滚动（鼠标滚轮） */
    ENCODER_MODE_VOLUME,        /* 音量（Consumer Control） */
    ENCODER_MODE_ZOOM,          /* 缩放（Ctrl+滚轮） */
    ENCODER_MODE_BRIGHTNESS,    /* 亮度（Consumer Control） */
    ENCODER_MODE_CUSTOM,        /* 自定义（可配置按键序列） */
    ENCODER_MODE_COUNT
} encoder_mode_t;

/* 编码器方向（复用 encoder.h 中的 encoder_dir_e） */
typedef encoder_dir_e encoder_dir_t;

/* 加速度等级 */
typedef enum
{
    ENCODER_ACCEL_1X = 0,    /* 1倍（慢速精细） */
    ENCODER_ACCEL_2X,        /* 2倍 */
    ENCODER_ACCEL_5X,        /* 5倍 */
    ENCODER_ACCEL_10X,       /* 10倍（快速翻页） */
    ENCODER_ACCEL_LEVELS
} encoder_accel_level_t;

/* 编码器事件（处理器输出） */
typedef struct
{
    encoder_dir_t direction;       /* 方向 */
    encoder_accel_level_t accel;   /* 加速度等级 */
    uint8_t multiplier;            /* 实际倍数（1,2,5,10） */
    encoder_mode_t mode;           /* 当前模式 */
} encoder_event_t;

/* 编码器配置 */
typedef struct
{
    uint8_t  reverse;              /* 方向反转：0=正常，1=反转 */
    uint8_t  steps_per_tick;       /* 多少步算一个tick（防抖） */
    uint8_t  scroll_speed;         /* 滚动速度倍数 */
    uint8_t  accel_enabled;        /* 加速度启用：0=禁用，1=启用 */
    uint8_t  mode_switch_key;      /* 模式切换按键索引（0xFF=中键） */
    uint8_t  accel_profile;        /* 加速度曲线：0=标准,1=慢,2=快 */
} encoder_config_t;

/* ==================== 接口函数 ==================== */

/**
 * @brief 初始化编码器处理器
 */
void encoder_processor_init(void);

/**
 * @brief 设置编码器配置
 * @param config 配置参数
 */
void encoder_processor_set_config(const encoder_config_t *config);

/**
 * @brief 获取当前配置
 * @return 当前配置
 */
encoder_config_t encoder_processor_get_config(void);

/**
 * @brief 输入原始旋转事件（由 Core1 调用）
 * @param dir 旋转方向（CW/CCW/NONE）
 * @param timestamp_ms 事件时间戳（毫秒）
 */
void encoder_processor_input(encoder_dir_t dir, uint32_t timestamp_ms);

/**
 * @brief 获取待处理的编码器事件（由 Core0 调用）
 * @param event 输出事件
 * @return true=有事件，false=无事件
 */
bool encoder_processor_get_event(encoder_event_t *event);

/**
 * @brief 获取当前模式
 * @return 当前模式
 */
encoder_mode_t encoder_processor_get_mode(void);

/**
 * @brief 设置当前模式
 * @param mode 模式
 */
void encoder_processor_set_mode(encoder_mode_t mode);

/**
 * @brief 切换到下一个模式（循环切换）
 * @return 切换后的模式
 */
encoder_mode_t encoder_processor_cycle_mode(void);

/**
 * @brief 输入中键按下事件（用于模式切换）
 * @param pressed 是否按下
 * @param timestamp_ms 时间戳
 */
void encoder_processor_input_switch(bool pressed, uint32_t timestamp_ms);

/**
 * @brief 获取加速度等级对应的倍数
 * @param level 加速度等级
 * @return 倍数（1,2,5,10）
 */
uint8_t encoder_processor_get_accel_multiplier(encoder_accel_level_t level);

/**
 * @brief 重置加速度状态（清除速度历史）
 */
void encoder_processor_reset_accel(void);

/**
 * @brief 设置加速度曲线
 * @param profile 曲线类型：0=标准,1=慢,2=快
 */
void encoder_processor_set_accel_profile(uint8_t profile);

/**
 * @brief 获取加速度曲线
 * @return 曲线类型：0=标准,1=慢,2=快
 */
uint8_t encoder_processor_get_accel_profile(void);

/**
 * @brief 设置自定义模式的按键码
 * @param cw_key 顺时针旋转时发送的键码（HID 键码，0=禁用）
 * @param ccw_key 逆时针旋转时发送的键码（HID 键码，0=禁用）
 */
void encoder_processor_set_custom_keys(uint8_t cw_key, uint8_t ccw_key);

/**
 * @brief 获取自定义模式的按键码
 * @param cw_key 输出顺时针键码
 * @param ccw_key 输出逆时针键码
 */
void encoder_processor_get_custom_keys(uint8_t *cw_key, uint8_t *ccw_key);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_ENCODER_PROCESSOR_H */
