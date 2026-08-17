/*
 * include/board/pins.h
 * RP2350 Pico2 平台硬件引脚宏（仅放置硬件相关宏）
 * 本文件仅包含引脚宏与默认时序宏，所有业务/驱动不得直接修改本文件
 */

#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <stdint.h>

/* ==================== 64键SPI键盘矩阵 (74HC165级联) ==================== */
/* SPI0 引脚映射，与 MicroPython 示例完全一致 */
#define KEYPAD_SPI_SCK_PIN   (18u)
#define KEYPAD_SPI_MOSI_PIN  (19u)
#define KEYPAD_SPI_MISO_PIN  (16u)
#define KEYPAD_SPI_CS_PIN    (17u)

/* 键盘SPI参数 */
#define KEYPAD_SPI_BAUDRATE_HZ  (100000u)
#define KEYPAD_SPI_CS_DELAY_US  (5u)  /* CS 下降/上升延时 5 us */

/* ==================== OPTICAL_SENSOR 光学鼠标传感器 ==================== */
/* SPI1 引脚映射 */
#define OPTICAL_SENSOR_SPI_SCK_PIN   (10u)
#define OPTICAL_SENSOR_SPI_MOSI_PIN  (11u)
#define OPTICAL_SENSOR_SPI_MISO_PIN  (12u)
#define OPTICAL_SENSOR_SPI_CS_PIN    (13u)
#define OPTICAL_SENSOR_MOT_PIN       (14u)  /* 运动中断引脚 */
#define OPTICAL_SENSOR_RST_PIN       (15u)  /* 复位引脚 */

/* OPTICAL_SENSOR SPI参数 */
#define OPTICAL_SENSOR_SPI_BAUDRATE_HZ  (500000u)
#define OPTICAL_SENSOR_SPI_CS_DELAY_US  (1u)   /* CS 下降/上升延时 1 us */
#define OPTICAL_SENSOR_REG_DELAY_US     (10u)  /* 寄存器读写延时 10 us */

/* ==================== PS2 双轴摇杆 ==================== */
#define JOYSTICK_ADC_X_PIN   (26u)  /* ADC0 - X轴 */
#define JOYSTICK_ADC_Y_PIN   (27u)  /* ADC1 - Y轴 */
#define JOYSTICK_BTN_PIN     (28u)  /* 摇杆按键 */

/* ==================== 滚轮编码器 ==================== */
#define ENCODER_A_PIN        (20u)  /* 编码器A相 */
#define ENCODER_B_PIN        (21u)  /* 编码器B相 */
#define ENCODER_SW_PIN       (22u)  /* 编码器中键 */

#endif /* BOARD_PINS_H */
