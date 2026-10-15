/**
 * version.h
 * 版本号统一管理头文件（单一权威来源）
 *
 * 所有版本号、魔数、协议常量在此定义一次，其他文件直接引用。
 * 修改版本号时只需修改此文件，避免多处重复定义导致不一致。
 *
 * 版本号规则：
 * - 固件版本：bcdDevice 格式（BCD编码，如 0x0010 = v0.1.0）
 * - 协议版本：主版本.次版本（主版本不同则不兼容）
 * - 配置版本：配置结构版本（结构变更时递增）
 */

#ifndef VERSION_H
#define VERSION_H

#include <stdint.h>

/* ==================== 固件版本 ==================== */

/**
 * 固件版本号（USB bcdDevice 格式，BCD编码）
 * 格式：0xMMNN，其中 MM=主版本.次版本，NN=修订版本
 * 例如：0x0010 = v0.1.0，0x0100 = v1.0.0，0x0123 = v1.2.3
 */
#define FIRMWARE_VERSION_BCD      0x0010U  /* v0.1.0 */

/** 固件版本主版本号 */
#define FIRMWARE_VERSION_MAJOR    0U

/** 固件版本次版本号 */
#define FIRMWARE_VERSION_MINOR    1U

/** 固件版本修订号 */
#define FIRMWARE_VERSION_PATCH    0U

/* ==================== 协议版本 ==================== */

/**
 * HID 配置协议版本
 * 主版本不同则不兼容，次版本不同则向下兼容
 * v0.2：能力位扩展至 26（新增诊断/制造段）、Device Info 增加 device_flags、
 *       命令三级分级与固件侧闸门、新增错误码 DIAG_LOCKED/MFG_LOCKED
 */
#define PROTOCOL_VERSION_MAJOR    0U
#define PROTOCOL_VERSION_MINOR    2U
#define PROTOCOL_VERSION          ((PROTOCOL_VERSION_MAJOR << 8) | PROTOCOL_VERSION_MINOR)

/* ==================== 配置常量 ==================== */

/**
 * 配置结构版本号
 * 配置结构体字段变更时递增，用于配置迁移和兼容性判断
 */
#define CONFIG_VERSION            0x0008U  /* v8 */

/** 配置魔数（用于识别有效配置，与上位机保持一致） */
#define CONFIG_MAGIC              0x5A5A5A5AU

/** 配置块大小（每块传输的数据字节数） */
#define CONFIG_BLOCK_SIZE         62U

/** 配置结构总大小（字节，含CRC32） */
#define CONFIG_TOTAL_SIZE         1510U

/** 配置块数量（支持 1510 字节配置，1510/62 向上取整为 25 块） */
#define CONFIG_BLOCK_COUNT        25U

/* ==================== 协议常量 ==================== */

/** 直连设备索引（有线版本固定为 0xFF） */
#define DEVICE_INDEX_DIRECT       0xFFU

/** 广播设备索引 */
#define DEVICE_INDEX_BROADCAST    0x00U

/** 软件ID广播（不需要响应匹配） */
#define SW_ID_BROADCAST           0x00U

/** 软件ID最小值 */
#define SW_ID_MIN                 0x01U

/** 软件ID最大值（15） */
#define SW_ID_MAX                 0x0FU

#endif /* VERSION_H */
