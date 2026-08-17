/*
 * include/board/board.h
 * RP2350 Pico2 板级支持包对外接口
 * 仅暴露硬件初始化接口，内部实现不对外公开
 */

#ifndef BOARD_BOARD_H
#define BOARD_BOARD_H

#include <stdint.h>
#include <stdbool.h>
#include "device/keypad_spi.h"
#include "device/optical_sensor.h"
#include "device/joystick.h"
#include "device/encoder.h"

/* 板级全局初始化入口
 * 注意：不使用 board_init 命名，避免与 TinyUSB BSP 的 board_init 冲突
 */
void bsp_init(void);

/* 获取SPI键盘硬件配置句柄 */
const keypad_spi_cfg_t* board_get_keypad_spi_cfg(void);

/* 获取OPTICAL_SENSOR传感器硬件配置句柄 */
const optical_sensor_cfg_t* board_get_optical_sensor_cfg(void);

/* 获取摇杆硬件配置句柄 */
const joystick_cfg_t* board_get_joystick_cfg(void);

/* 获取编码器硬件配置句柄 */
const encoder_cfg_t* board_get_encoder_cfg(void);

#endif /* BOARD_BOARD_H */
