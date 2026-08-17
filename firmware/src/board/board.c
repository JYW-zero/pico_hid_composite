/*
 * src/board/board.c
 * Board 层板级支持包实现
 * 仅在 board 层包含 board/pins.h，负责所有硬件初始化
 * 向上层注入各外设配置结构体
 */

#include "board/board.h"
#include "board/pins.h"
#include "board/config.h"
#include "middleware/watchdog.h"
#include "middleware/fault.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

/* ==================== 静态硬件配置，全局唯一，只读 ==================== */
static keypad_spi_cfg_t s_keypad_cfg;
static optical_sensor_cfg_t    s_optical_sensor_cfg;
static joystick_cfg_t   s_joystick_cfg;
static encoder_cfg_t    s_encoder_cfg;

/* ==================== 64键SPI键盘初始化 ==================== */
static void board_init_keypad(void)
{
    spi_inst_t *spi = spi0;

    spi_init(spi, KEYPAD_SPI_BAUDRATE_HZ);
    spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(KEYPAD_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(KEYPAD_SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(KEYPAD_SPI_MISO_PIN, GPIO_FUNC_SPI);

    gpio_init(KEYPAD_SPI_CS_PIN);
    gpio_set_dir(KEYPAD_SPI_CS_PIN, GPIO_OUT);
    gpio_put(KEYPAD_SPI_CS_PIN, 1);

    s_keypad_cfg.spi = spi;
    s_keypad_cfg.cs_pin = KEYPAD_SPI_CS_PIN;
    s_keypad_cfg.baud_hz = KEYPAD_SPI_BAUDRATE_HZ;
    s_keypad_cfg.cs_delay_us = KEYPAD_SPI_CS_DELAY_US;
}

/* ==================== OPTICAL_SENSOR光学传感器初始化 ==================== */
static void board_init_optical_sensor(void)
{
    spi_inst_t *spi = spi1;

    spi_init(spi, OPTICAL_SENSOR_SPI_BAUDRATE_HZ);
    spi_set_format(spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);

    gpio_set_function(OPTICAL_SENSOR_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(OPTICAL_SENSOR_SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(OPTICAL_SENSOR_SPI_MISO_PIN, GPIO_FUNC_SPI);

    /* CS引脚 */
    gpio_init(OPTICAL_SENSOR_SPI_CS_PIN);
    gpio_set_dir(OPTICAL_SENSOR_SPI_CS_PIN, GPIO_OUT);
    gpio_put(OPTICAL_SENSOR_SPI_CS_PIN, 1);

    /* MOT运动中断引脚：上拉输入 */
    gpio_init(OPTICAL_SENSOR_MOT_PIN);
    gpio_set_dir(OPTICAL_SENSOR_MOT_PIN, GPIO_IN);
    gpio_pull_up(OPTICAL_SENSOR_MOT_PIN);

    /* RST复位引脚：默认高电平 */
    gpio_init(OPTICAL_SENSOR_RST_PIN);
    gpio_set_dir(OPTICAL_SENSOR_RST_PIN, GPIO_OUT);
    gpio_put(OPTICAL_SENSOR_RST_PIN, 1);

    s_optical_sensor_cfg.spi = spi;
    s_optical_sensor_cfg.cs_pin = OPTICAL_SENSOR_SPI_CS_PIN;
    s_optical_sensor_cfg.mot_pin = OPTICAL_SENSOR_MOT_PIN;
    s_optical_sensor_cfg.rst_pin = OPTICAL_SENSOR_RST_PIN;
    s_optical_sensor_cfg.baud_hz = OPTICAL_SENSOR_SPI_BAUDRATE_HZ;
    s_optical_sensor_cfg.cs_delay_us = OPTICAL_SENSOR_SPI_CS_DELAY_US;
    s_optical_sensor_cfg.reg_delay_us = OPTICAL_SENSOR_REG_DELAY_US;
}

/* ==================== PS2摇杆初始化 ==================== */
static void board_init_joystick(void)
{
    /* ADC 外设全局初始化（仅第一次有效） */
    adc_init();

    s_joystick_cfg.adc_x_pin = JOYSTICK_ADC_X_PIN;
    s_joystick_cfg.adc_y_pin = JOYSTICK_ADC_Y_PIN;
    s_joystick_cfg.btn_pin = JOYSTICK_BTN_PIN;
}

/* ==================== 滚轮编码器初始化 ==================== */
static void board_init_encoder(void)
{
    s_encoder_cfg.a_pin = ENCODER_A_PIN;
    s_encoder_cfg.b_pin = ENCODER_B_PIN;
    s_encoder_cfg.sw_pin = ENCODER_SW_PIN;
    s_encoder_cfg.steps_per_tick = 4u;  /* 4步一个tick，防抖 */
}

/* ==================== 全局板级初始化入口 ==================== */
void bsp_init(void)
{
    /* 加载Flash配置（失败自动加载默认值） */
    config_init();

    board_init_keypad();
    board_init_optical_sensor();
    board_init_joystick();
    board_init_encoder();

    fault_record(FAULT_LEVEL_INFO, "bsp", "all peripherals init complete");
}

/* ==================== 配置获取接口 ==================== */
const keypad_spi_cfg_t* board_get_keypad_spi_cfg(void)
{
    return &s_keypad_cfg;
}

const optical_sensor_cfg_t* board_get_optical_sensor_cfg(void)
{
    return &s_optical_sensor_cfg;
}

const joystick_cfg_t* board_get_joystick_cfg(void)
{
    return &s_joystick_cfg;
}

const encoder_cfg_t* board_get_encoder_cfg(void)
{
    return &s_encoder_cfg;
}
