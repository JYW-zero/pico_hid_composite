/*
 * include/protocol/config_hid_helpers.h
 * 配置 HID 辅助函数接口
 * 从 config_hid.c 提取，包含 ACK 发送和 DPI 转换等独立辅助函数
 */
#ifndef PROTOCOL_CONFIG_HID_HELPERS_H
#define PROTOCOL_CONFIG_HID_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include "device/optical_sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== ACK 响应发送 ==================== */

/* 发送配置命令 ACK 响应
 * @param cmd 命令字节
 * @param success 是否成功
 * @param error_code 错误码（CONFIG_HID_OK 等）
 * @param param 附加参数（如解锁进度、版本号等）
 */
void config_hid_send_ack(uint8_t cmd, bool success, uint8_t error_code, uint16_t param);

/* ==================== DPI 转换 ==================== */

/* DPI 数值转光学传感器枚举
 * @param dpi_val DPI 数值（400/800/1600/3200）
 * @return 对应的光学传感器 DPI 枚举，默认返回 1600
 */
optical_sensor_dpi_e config_hid_dpi_val_to_enum(uint16_t dpi_val);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_CONFIG_HID_HELPERS_H */
