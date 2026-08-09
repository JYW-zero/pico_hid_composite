/*
 * src/device/encoder.c
 * 增量式滚轮编码器驱动实现
 * 采用4状态状态机解码，支持防抖累积
 * 严格遵循 MISRA-like 约束：单返回点、显式类型转换、禁止动态内存
 * 状态由调用者维护，驱动完全可重入
 */

#include "device/encoder.h"
#include "middleware/fault.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

/* 状态转换表：[当前AB][上次AB] = 方向
 * 1 = 顺时针, -1 = 逆时针, 0 = 无效/抖动
 * 与MicroPython版本完全一致
 */
static const int8_t s_enc_table[4][4] =
{
    { 0, -1,  1,  0},  /* 当前 00 */
    { 1,  0,  0, -1},  /* 当前 01 */
    {-1,  0,  0,  1},  /* 当前 10 */
    { 0,  1, -1,  0}   /* 当前 11 */
};

int encoder_init(const encoder_cfg_t *cfg)
{
    int status = 0;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "encoder", "init null cfg");
        status = -1;
    }
    else
    {
        /* 配置A相引脚：上拉输入 */
        gpio_init((uint)cfg->a_pin);
        gpio_set_dir((uint)cfg->a_pin, GPIO_IN);
        gpio_pull_up((uint)cfg->a_pin);

        /* 配置B相引脚：上拉输入 */
        gpio_init((uint)cfg->b_pin);
        gpio_set_dir((uint)cfg->b_pin, GPIO_IN);
        gpio_pull_up((uint)cfg->b_pin);

        /* 配置中键引脚：上拉输入 */
        gpio_init((uint)cfg->sw_pin);
        gpio_set_dir((uint)cfg->sw_pin, GPIO_IN);
        gpio_pull_up((uint)cfg->sw_pin);

        fault_record(FAULT_LEVEL_INFO, "encoder", "init complete");
    }

    return status;
}

void encoder_state_init(encoder_state_t *state)
{
    if (state == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "encoder", "state_init null");
        return;
    }

    state->last_ab = 0u;
    state->accum = 0;
}

encoder_dir_e encoder_update(const encoder_cfg_t *cfg, encoder_state_t *state)
{
    encoder_dir_e result = ENCODER_DIR_NONE;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "encoder", "update null cfg");
    }
    else if (state == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "encoder", "update null state");
    }
    else
    {
        uint8_t a = (uint8_t)gpio_get((uint)cfg->a_pin);
        uint8_t b = (uint8_t)gpio_get((uint)cfg->b_pin);
        uint8_t curr_ab = (uint8_t)((a << 1) | b);

        if (curr_ab != state->last_ab)
        {
            int8_t dir = s_enc_table[curr_ab][state->last_ab];
            state->last_ab = curr_ab;

            state->accum += (int32_t)dir;

            /* 累积足够步数才输出一个有效tick */
            if (state->accum >= (int32_t)cfg->steps_per_tick)
            {
                result = ENCODER_DIR_CW;
                state->accum = 0;
            }
            else if (state->accum <= -(int32_t)cfg->steps_per_tick)
            {
                result = ENCODER_DIR_CCW;
                state->accum = 0;
            }
            else
            {
                result = ENCODER_DIR_NONE;
            }
        }
        else
        {
            result = ENCODER_DIR_NONE;
        }
    }

    return result;
}

bool encoder_read_switch(const encoder_cfg_t *cfg)
{
    bool pressed = false;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "encoder", "read_switch null cfg");
    }
    else
    {
        /* 低电平为按下（上拉输入） */
        pressed = (gpio_get((uint)cfg->sw_pin) == 0) ? true : false;
    }

    return pressed;
}
