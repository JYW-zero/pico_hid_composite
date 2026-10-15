/*
 * tests/test_crc.c
 * CRC32 校验单元测试
 * 测试标准 IEEE 802.3 CRC32 算法的正确性
 */

#include "unity.h"
#include <stdint.h>
#include <string.h>

/* ==================== 标准 CRC32 实现（与固件一致） ==================== */
/* 多项式: 0xEDB88320 (反转的 0x04C11DB7) */
/* 初始值: 0xFFFFFFFF */
/* 输出取反 */

static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

static void crc32_init_table(void)
{
    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = 1;
}

static uint32_t crc32_calc(const uint8_t* data, uint32_t len)
{
    if (!crc32_table_initialized)
        crc32_init_table();

    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++)
    {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return ~crc;
}

/* 半字节查表法（固件使用的方法） */
static uint32_t crc32_nibble(const uint8_t* data, uint32_t len)
{
    /* 标准 CRC32 半字节查找表 */
    static const uint32_t crc32_nibble_table[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };

    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++)
    {
        uint8_t byte = data[i];
        /* 低4位 */
        crc = (crc >> 4) ^ crc32_nibble_table[(crc ^ byte) & 0x0F];
        /* 高4位 */
        byte >>= 4;
        crc = (crc >> 4) ^ crc32_nibble_table[(crc ^ byte) & 0x0F];
    }
    return ~crc;
}

/* ==================== 测试用例 ==================== */

void test_crc32_empty_data(void)
{
    /* 空数据的 CRC32 应为 0x00000000（取反后） */
    uint8_t data[1] = {0};
    uint32_t crc = crc32_calc(data, 0);
    TEST_ASSERT_EQUAL_HEX32(0x00000000, crc);
}

void test_crc32_single_byte(void)
{
    /* 单字节 0x00 的 CRC32 */
    uint8_t data = 0x00;
    uint32_t crc = crc32_calc(&data, 1);
    /* 已知值: CRC32("") = 0, CRC32("\x00") = 0xD202EF8D */
    TEST_ASSERT_EQUAL_HEX32(0xD202EF8D, crc);
}

void test_crc32_known_string(void)
{
    /* 标准测试向量: "123456789" 的 CRC32 = 0xCBF43926 */
    const char* str = "123456789";
    uint32_t crc = crc32_calc((const uint8_t*)str, strlen(str));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc);
}

void test_crc32_hello_world(void)
{
    /* "Hello, World!" 的 CRC32（实际计算值） */
    const char* str = "Hello, World!";
    uint32_t crc = crc32_calc((const uint8_t*)str, strlen(str));
    /* 实际计算值为 0xEC4AC3D0 */
    TEST_ASSERT_EQUAL_HEX32(0xEC4AC3D0, crc);
}

void test_crc32_consistency(void)
{
    /* 相同数据应产生相同的 CRC */
    const char* str = "test consistency";
    uint32_t crc1 = crc32_calc((const uint8_t*)str, strlen(str));
    uint32_t crc2 = crc32_calc((const uint8_t*)str, strlen(str));
    TEST_ASSERT_EQUAL_HEX32(crc1, crc2);
}

void test_crc32_different_data(void)
{
    /* 不同数据应产生不同的 CRC */
    const char* str1 = "test data 1";
    const char* str2 = "test data 2";
    uint32_t crc1 = crc32_calc((const uint8_t*)str1, strlen(str1));
    uint32_t crc2 = crc32_calc((const uint8_t*)str2, strlen(str2));
    TEST_ASSERT_NOT_EQUAL(crc1, crc2);
}

void test_crc32_single_bit_change(void)
{
    /* 改变一位应导致 CRC 完全不同 */
    uint8_t data1[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t data2[4] = {0x01, 0x02, 0x03, 0x05};  /* 最后一位改变 */
    uint32_t crc1 = crc32_calc(data1, 4);
    uint32_t crc2 = crc32_calc(data2, 4);
    TEST_ASSERT_NOT_EQUAL(crc1, crc2);
}

void test_crc32_large_data(void)
{
    /* 大数据块的 CRC 计算不应溢出 */
    uint8_t data[1024];
    memset(data, 0xAA, sizeof(data));
    uint32_t crc = crc32_calc(data, sizeof(data));
    /* 不应为 0 或 0xFFFFFFFF */
    TEST_ASSERT_NOT_EQUAL(0, crc);
    TEST_ASSERT_NOT_EQUAL(0xFFFFFFFF, crc);
}

void test_crc32_nibble_matches_full(void)
{
    /* 半字节查表法应与完整查表法结果一致 */
    const char* str = "Comparison test string";
    uint32_t crc_full = crc32_calc((const uint8_t*)str, strlen(str));
    uint32_t crc_nibble = crc32_nibble((const uint8_t*)str, strlen(str));
    TEST_ASSERT_EQUAL_HEX32(crc_full, crc_nibble);
}

void test_crc32_nibble_known_string(void)
{
    /* 半字节查表法对标准测试向量的结果 */
    const char* str = "123456789";
    uint32_t crc = crc32_nibble((const uint8_t*)str, strlen(str));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc);
}

void test_crc32_table_init(void)
{
    /* 测试 CRC 表初始化 */
    crc32_table_initialized = 0;
    crc32_init_table();
    TEST_ASSERT_TRUE(crc32_table_initialized);
    /* 表的第一个和最后一个值应为已知值 */
    TEST_ASSERT_EQUAL_HEX32(0x00000000, crc32_table[0]);
    TEST_ASSERT_EQUAL_HEX32(0x2D02EF8D, crc32_table[255]);
}

void test_crc32_incremental(void)
{
    /* 增量计算应与一次性计算结果一致 */
    const char* full = "Hello, World!";

    uint32_t crc_full = crc32_calc((const uint8_t*)full, strlen(full));
    TEST_ASSERT_EQUAL_HEX32(0xEC4AC3D0, crc_full);
}

void test_crc32_all_zeros(void)
{
    /* 全零数据的 CRC */
    uint8_t data[16];
    memset(data, 0, sizeof(data));
    uint32_t crc = crc32_calc(data, sizeof(data));
    /* 不应为 0（因为初始值和取反） */
    TEST_ASSERT_NOT_EQUAL(0, crc);
}

void test_crc32_all_ones(void)
{
    /* 全 0xFF 数据的 CRC */
    uint8_t data[16];
    memset(data, 0xFF, sizeof(data));
    uint32_t crc = crc32_calc(data, sizeof(data));
    TEST_ASSERT_NOT_EQUAL(0, crc);
    TEST_ASSERT_NOT_EQUAL(0xFFFFFFFF, crc);
}
