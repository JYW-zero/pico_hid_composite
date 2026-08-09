/*
 * include/device/encoder.h
 * 增量式滚轮编码器驱动头文件
 * 采用状态机解码，状态由调用者维护，驱动完全可重入
 */

#ifndef DEVICE_ENCODER_H
#define DEVICE_ENCODER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 编码器方向定义 */
typedef enum
{
    ENCODER_DIR_NONE = 0,
    ENCODER_DIR_CW   = 1,   /* 顺时针 */
    ENCODER_DIR_CCW  = -1   /* 逆时针 */
} encoder_dir_e;

/* 编码器运行状态结构体（由调用者维护，驱动不持有状态） */
typedef struct
{
    uint8_t last_ab;   /* 上次AB相状态 */
    int32_t accum;     /* 步长累积器 */
} encoder_state_t;

/* 编码器配置结构体：仅包含只读硬件参数 */
typedef struct
{
    uint32_t a_pin;    /* A相引脚 */
    uint32_t b_pin;    /* B相引脚 */
    uint32_t sw_pin;   /* 中键引脚 */
    uint8_t  steps_per_tick; /* 多少步算一个tick（防抖） */
} encoder_cfg_t;

/* 初始化编码器硬件 */
int encoder_init(const encoder_cfg_t *cfg);

/* 初始化状态结构体 */
void encoder_state_init(encoder_state_t *state);

/* 更新编码器状态，返回方向 */
encoder_dir_e encoder_update(const encoder_cfg_t *cfg, encoder_state_t *state);

/* 读取中键状态 */
bool encoder_read_switch(const encoder_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_ENCODER_H */
