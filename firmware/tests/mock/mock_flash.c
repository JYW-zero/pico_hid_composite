/*
 * tests/mock/mock_flash.c
 * Mock版Flash实现
 * 用内存数组模拟Flash的擦除和写入操作
 */

#include "mock_flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <string.h>

/* ==================== 模拟Flash数组 ==================== */

/* 全局数组，用于地址计算（让CONFIG_FLASH_BASE_ADDR可以指向这里） */
uint8_t s_mock_flash[MOCK_FLASH_SIZE];

/* 覆盖config.c里的弱函数，让Flash基地址指向我们的mock数组
 * 减去MOCK_FLASH_BASE_OFFSET，这样加上offset后正好指向数组开头
 */
uintptr_t config_get_flash_base(void)
{
    return (uintptr_t)s_mock_flash - MOCK_FLASH_BASE_OFFSET;
}

/* ==================== 内部函数 ==================== */

/* 转换偏移：真实Flash偏移 -> mock数组索引 */
static uint32_t offset_to_index(uint32_t offset)
{
    return offset - MOCK_FLASH_BASE_OFFSET;
}

/* 检查偏移是否在有效范围内 */
static bool offset_valid(uint32_t offset, size_t len)
{
    if (offset < MOCK_FLASH_BASE_OFFSET)
    {
        return false;
    }
    uint32_t idx = offset_to_index(offset);
    if (idx + len > MOCK_FLASH_SIZE)
    {
        return false;
    }
    return true;
}

/* ==================== Mock Flash 接口 ==================== */

void mock_flash_reset(void)
{
    memset(s_mock_flash, 0xFF, MOCK_FLASH_SIZE);
}

void mock_flash_read(uint32_t offset, uint8_t* buf, size_t len)
{
    if (!offset_valid(offset, len))
    {
        return;
    }
    uint32_t idx = offset_to_index(offset);
    memcpy(buf, &s_mock_flash[idx], len);
}

void mock_flash_direct_write(uint32_t offset, const uint8_t* data, size_t len)
{
    if (!offset_valid(offset, len))
    {
        return;
    }
    uint32_t idx = offset_to_index(offset);
    memcpy(&s_mock_flash[idx], data, len);
}

bool mock_flash_is_erased(uint32_t offset, size_t len)
{
    if (!offset_valid(offset, len))
    {
        return false;
    }
    uint32_t idx = offset_to_index(offset);
    for (size_t i = 0; i < len; i++)
    {
        if (s_mock_flash[idx + i] != 0xFF)
        {
            return false;
        }
    }
    return true;
}

const uint8_t* mock_flash_get_ptr(void)
{
    return s_mock_flash;
}

/* ==================== 替换 SDK 的 Flash 函数 ==================== */

/* 擦除Flash扇区 */
void flash_range_erase(uint32_t flash_offset, size_t count)
{
    if (!offset_valid(flash_offset, count))
    {
        return;
    }
    uint32_t idx = offset_to_index(flash_offset);
    memset(&s_mock_flash[idx], 0xFF, count);
}

/* 写入Flash数据 */
void flash_range_program(uint32_t flash_offset, const uint8_t* data, size_t count)
{
    if (!offset_valid(flash_offset, count))
    {
        return;
    }
    uint32_t idx = offset_to_index(flash_offset);
    /* Flash写入只能把1改成0，不能把0改成1（模拟真实Flash特性）
     * 不过为了测试简单，我们直接memcpy，测试里一般都是先擦除再写入
     */
    memcpy(&s_mock_flash[idx], data, count);
}

/* ==================== 替换 SDK 的中断函数 ==================== */

uint32_t save_and_disable_interrupts(void)
{
    /* 空实现，测试不需要真正关中断 */
    return 0;
}

void restore_interrupts(uint32_t status)
{
    /* 空实现 */
    (void)status;
}




