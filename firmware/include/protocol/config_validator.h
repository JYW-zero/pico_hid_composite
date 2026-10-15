/*
 * include/protocol/config_validator.h
 * 配置验证接口
 * 从 config_hid.c 提取，包含配置魔数验证、CRC校验、参数验证和配置合并
 */
#ifndef PROTOCOL_CONFIG_VALIDATOR_H
#define PROTOCOL_CONFIG_VALIDATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "board/config.h"
#include "protocol/config_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 验证函数 ==================== */

/* 验证配置魔数
 * @param config 配置结构体指针
 * @return true=魔数正确，false=魔数错误
 */
bool config_validator_check_magic(const device_config_t *config);

/* 计算配置 CRC32（忽略 crc32 字段本身）
 * @param config 配置结构体指针
 * @return CRC32 校验值
 */
uint32_t config_validator_calc_crc(const device_config_t *config);

/* 验证配置 CRC32
 * @param config 配置结构体指针
 * @return true=CRC正确，false=CRC错误
 */
bool config_validator_check_crc(const device_config_t *config);

/* 完整配置验证（魔数 + CRC + 基本参数范围）
 * @param config 配置结构体指针
 * @return 错误码（CONFIG_HID_OK 表示验证通过）
 */
config_hid_error_t config_validator_validate(const device_config_t *config);

/* ==================== 配置合并 ==================== */

/* 合并新配置到当前配置（只覆盖前142字节，保留 macro_data）
 * 强制设置正确的 magic 和 version，重新计算 CRC
 * @param current 当前配置（将被修改）
 * @param new_config 新配置
 */
void config_validator_merge_config(device_config_t *current, const device_config_t *new_config);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_CONFIG_VALIDATOR_H */
