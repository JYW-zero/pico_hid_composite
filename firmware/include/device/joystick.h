/*
 * include/device/joystick.h
 * PS2 双轴摇杆ADC驱动头文件
 * 驱动无内部状态，配置与运行状态分离，完全可重入
 */

#ifndef DEVICE_JOYSTICK_H
#define DEVICE_JOYSTICK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 摇杆数据结构体 */
typedef struct
{
    uint16_t x;     /* X轴ADC值 (0-4095) */
    uint16_t y;     /* Y轴ADC值 (0-4095) */
    bool btn_pressed; /* 按键是否按下 */
} joystick_data_t;

/* 摇杆配置结构体：仅包含只读硬件参数 */
typedef struct
{
    uint32_t adc_x_pin;  /* X轴ADC引脚 */
    uint32_t adc_y_pin;  /* Y轴ADC引脚 */
    uint32_t btn_pin;    /* 按键引脚 */
} joystick_cfg_t;

/* 初始化摇杆 */
int joystick_init(const joystick_cfg_t *cfg);

/* 读取摇杆数据 */
int joystick_read(const joystick_cfg_t *cfg, joystick_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_JOYSTICK_H */
