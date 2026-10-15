/*
 * include/middleware/crc32.h
 * CRC32 计算公共模块（查表法，半字节算法，多项式 0xEDB88320）
 *
 * 从 config.c 和 config_hid.c 提取，避免代码重复。
 * 初始值 0xFFFFFFFF，结果异或 0xFFFFFFFF，与标准 CRC32 一致。
 */

#ifndef MIDDLEWARE_CRC32_H
#define MIDDLEWARE_CRC32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CRC32 初始值 */
#define CRC32_INIT_VALUE  0xFFFFFFFFu

/* 计算数据的 CRC32（使用初始值 0xFFFFFFFF，结果异或 0xFFFFFFFF）
 * data: 数据指针
 * len:  数据长度（字节）
 * 返回: CRC32 值
 */
uint32_t crc32_calc(const uint8_t* data, uint32_t len);

/* 增量式 CRC32 计算：用已有 CRC 值继续计算
 * crc:  之前的 CRC 值（首次调用传 CRC32_INIT_VALUE）
 * data: 数据指针
 * len:  数据长度（字节）
 * 返回: 更新后的 CRC 值（未异或 0xFFFFFFFF，需调用 crc32_finalize 完成）
 */
uint32_t crc32_update(uint32_t crc, const uint8_t* data, uint32_t len);

/* 完成 CRC32 计算（异或 0xFFFFFFFF）
 * crc: crc32_update 返回的中间值
 * 返回: 最终 CRC32 值
 */
static inline uint32_t crc32_finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_CRC32_H */
