/*
 * src/device/joystick.c
 * PS2 双轴摇杆ADC驱动实现
 * 严格遵循 MISRA-like 约束：单返回点、显式类型转换、禁止动态内存
 * 驱动无内部状态，完全可重入
 */

#include "device/joystick.h"
#include "middleware/fault.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

/* 错误码定义 */
enum
{
    JOYSTICK_OK = 0,
    JOYSTICK_ERR_INVALID_PARAM = -1,
    JOYSTICK_ERR_HW = -2
};

/* ADC 输入通道与引脚对应关系
 * RP2350: GPIO26=ADC0, GPIO27=ADC1, GPIO28=ADC2, GPIO29=ADC3
 */
static uint8_t joystick_pin_to_adc_channel(uint32_t pin)
{
    uint8_t ch = 0u;
    switch (pin)
    {
        case 26u:
            ch = 0u;
            break;
        case 27u:
            ch = 1u;
            break;
        case 28u:
            ch = 2u;
            break;
        case 29u:
            ch = 3u;
            break;
        default:
            ch = 0u;
            break;
    }
    return ch;
}

int joystick_init(const joystick_cfg_t *cfg)
{
    int status = JOYSTICK_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "joystick", "init null cfg");
        status = JOYSTICK_ERR_INVALID_PARAM;
    }
    else
    {
        /* 初始化ADC外设（仅第一次调用有效） */
        adc_init();

        /* 配置X轴ADC引脚 */
        adc_gpio_init((uint)cfg->adc_x_pin);

        /* 配置Y轴ADC引脚 */
        adc_gpio_init((uint)cfg->adc_y_pin);

        /* 配置按键引脚：上拉输入 */
        gpio_init((uint)cfg->btn_pin);
        gpio_set_dir((uint)cfg->btn_pin, GPIO_IN);
        gpio_pull_up((uint)cfg->btn_pin);

        fault_record(FAULT_LEVEL_INFO, "joystick", "init complete");
    }

    return status;
}

int joystick_read(const joystick_cfg_t *cfg, joystick_data_t *data)
{
    int status = JOYSTICK_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "joystick", "read null cfg");
        status = JOYSTICK_ERR_INVALID_PARAM;
    }
    else if (data == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "joystick", "read null output");
        status = JOYSTICK_ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t ch_x = joystick_pin_to_adc_channel(cfg->adc_x_pin);
        uint8_t ch_y = joystick_pin_to_adc_channel(cfg->adc_y_pin);

        /* 读取X轴 */
        adc_select_input(ch_x);
        data->x = (uint16_t)adc_read();

        /* 读取Y轴 */
        adc_select_input(ch_y);
        data->y = (uint16_t)adc_read();

        /* 读取按键：低电平为按下（上拉输入） */
        data->btn_pressed = (gpio_get((uint)cfg->btn_pin) == 0) ? true : false;

        status = JOYSTICK_OK;
    }

    return status;
}
