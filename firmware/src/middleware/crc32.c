/*
 * src/middleware/crc32.c
 * CRC32 计算公共模块实现（查表法，半字节算法，多项式 0xEDB88320）
 *
 * 从 config.c 和 config_hid.c 提取，避免代码重复。
 */

#include "middleware/crc32.h"

/* CRC32 半字节查找表（16项，多项式 0xEDB88320） */
static const uint32_t s_crc32_table[16] =
{
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

/* 增量式 CRC32 计算 */
uint32_t crc32_update(uint32_t crc, const uint8_t* data, uint32_t len)
{
    if (data == NULL || len == 0)
    {
        return crc;
    }

    for (uint32_t i = 0; i < len; i++)
    {
        uint8_t byte = data[i];
        crc = (crc >> 4) ^ s_crc32_table[(crc & 0x0Fu) ^ (byte & 0x0Fu)];
        crc = (crc >> 4) ^ s_crc32_table[(crc & 0x0Fu) ^ (byte >> 4)];
    }
    return crc;
}

/* 计算数据的 CRC32（完整计算） */
uint32_t crc32_calc(const uint8_t* data, uint32_t len)
{
    uint32_t crc = crc32_update(CRC32_INIT_VALUE, data, len);
    return crc32_finalize(crc);
}
