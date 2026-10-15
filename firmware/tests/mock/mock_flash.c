/*
 * tests/mock/mock_flash.c
 * Mock版Flash实现
 * 用内存数组模拟Flash的擦除和写入操作
 */

#include "mock_flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "board/flash_layout.h"
#include "middleware/flash_service.h"
#include <string.h>

/* ==================== 模拟Flash数组 ==================== */

/* 全局数组，用于地址计算（让CONFIG_FLASH_BASE_ADDR可以指向这里） */
uint8_t s_mock_flash[MOCK_FLASH_SIZE];

/* 编程/擦除统计（磨损均衡、按页写入等测试用） */
static uint32_t s_program_count = 0;
static uint32_t s_erase_count = 0;
static uint32_t s_last_program_offset = 0;

/* 覆盖config.c里的弱函数，让Flash基地址指向我们的mock数组
 * 减去MOCK_FLASH_BASE_OFFSET，这样加上offset后正好指向数组开头
 */
uintptr_t config_get_flash_base(void)
{
    return (uintptr_t)s_mock_flash - MOCK_FLASH_BASE_OFFSET;
}

/* 覆盖 flash_layout.c 的弱函数，host 下让 XIP 基址指向 mock 数组
 * （与 config_get_flash_base 同样减去 BASE_OFFSET，叠加 region 偏移后正好落在数组内） */
uintptr_t flash_layout_xip_base(void)
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
    s_program_count = 0;
    s_erase_count = 0;
    s_last_program_offset = 0;
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
    s_erase_count++;
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
    s_program_count++;
    s_last_program_offset = flash_offset;
}

uint32_t mock_flash_get_program_count(void) { return s_program_count; }
uint32_t mock_flash_get_last_program_offset(void) { return s_last_program_offset; }
uint32_t mock_flash_get_erase_count(void) { return s_erase_count; }
void mock_flash_reset_program_count(void) { s_program_count = 0; s_last_program_offset = 0; }
void mock_flash_reset_erase_count(void) { s_erase_count = 0; }

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

/* ==================== flash_layout Mock 实现 ==================== */

void flash_layout_init(void)
{
    /* mock 实现：什么都不做 */
}

uint32_t flash_layout_get_total_size(void)
{
    return MOCK_FLASH_SIZE;
}

bool flash_layout_is_valid(void)
{
    return true;
}

uint32_t flash_layout_get_offset(flash_region_e region)
{
    /* mock 实现：返回简单的偏移 */
    switch (region)
    {
        case FLASH_REGION_CONFIG_A:
            return MOCK_FLASH_BASE_OFFSET;
        case FLASH_REGION_CONFIG_B:
            return MOCK_FLASH_BASE_OFFSET + FLASH_SECTOR_SIZE;
        case FLASH_REGION_FAULT_LOG_A:
            return MOCK_FLASH_BASE_OFFSET + FLASH_SECTOR_SIZE * 2;
        case FLASH_REGION_FAULT_LOG_B:
            return MOCK_FLASH_BASE_OFFSET + FLASH_SECTOR_SIZE * 3;
        case FLASH_REGION_KEY_STATS:
            return MOCK_FLASH_BASE_OFFSET + FLASH_SECTOR_SIZE * 4;
        case FLASH_REGION_PROVISIONING:
            return MOCK_FLASH_BASE_OFFSET + FLASH_SECTOR_SIZE * 5;
        default:
            return MOCK_FLASH_BASE_OFFSET;
    }
}

uint32_t flash_layout_get_xip_addr(flash_region_e region)
{
    /* mock 实现：返回 mock 数组的地址 */
    return (uint32_t)(uintptr_t)s_mock_flash + (flash_layout_get_offset(region) - MOCK_FLASH_BASE_OFFSET);
}

uint32_t flash_layout_get_size(flash_region_e region)
{
    (void)region;
    return FLASH_SECTOR_SIZE;
}

/* ==================== flash_service Mock 实现 ==================== */

void flash_service_init(void)
{
    /* mock 实现：什么都不做 */
}

void flash_service_core1_init(void)
{
    /* mock 实现：什么都不做 */
}

bool flash_service_erase(uint32_t flash_offset, uint32_t size)
{
    flash_range_erase(flash_offset, size);
    return true;
}

bool flash_service_program(uint32_t flash_offset, const uint8_t *data, uint32_t size)
{
    flash_range_program(flash_offset, data, size);
    return true;
}

bool flash_service_write_sector(uint32_t flash_offset, const uint8_t *data)
{
    flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
    flash_range_program(flash_offset, data, FLASH_SECTOR_SIZE);
    return true;
}




