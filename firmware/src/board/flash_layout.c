/*
 * src/board/flash_layout.c
 * Flash布局统一管理模块实现
 *
 * 使用pico-sdk官方API运行时检测Flash大小
 */

#include "board/flash_layout.h"
#include "middleware/fault.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ==================== 静态变量 ==================== */

static bool s_initialized = false;
static uint32_t s_flash_total_size = 0;
static const char* s_size_string = "unknown";

/* 各区域的偏移（相对于Flash起始地址）
 * 布局（从末尾向前）：
 *   偏移  size  用途
 *   end-4K  4K   配置B区
 *   end-8K  4K   配置A区
 *   end-12K 4K   错误日志区
 *   end-16K 4K   按键统计区
 */
static uint32_t s_region_offsets[FLASH_REGION_COUNT];
static uint32_t s_region_sizes[FLASH_REGION_COUNT];

/* ==================== 内部函数 ==================== */

/* 检测Flash实际大小
 * 优先使用pico-sdk官方API flash_devinfo_get_cs_size()
 * 如果失败，回退到保守的2MB（最小可能值）
 */
static uint32_t detect_flash_size(void)
{
    uint32_t size_bytes = 0;

#if !PICO_RP2040
    /* RP2350: 使用官方API从FLASH_DEVINFO获取 */
    flash_devinfo_size_t devinfo_size = flash_devinfo_get_cs_size(0);
    if (devinfo_size != FLASH_DEVINFO_SIZE_NONE)
    {
        size_bytes = flash_devinfo_size_to_bytes(devinfo_size);
    }
#endif

    /* 如果官方API返回0或不支持，尝试通过SFDP检测
     * 或者回退到保守估计
     */
    if (size_bytes == 0)
    {
        /* 保守回退：假设最小2MB
         * 这样即使是2MB Flash也能正常工作
         * 4MB Flash上也只是用了前2MB的末尾，不影响功能
         */
        size_bytes = 2 * 1024 * 1024;  /* 2MB */
    }

    return size_bytes;
}

/* 根据Flash总大小计算各区域偏移 */
static void calculate_layout(uint32_t total_size)
{
    /* 从末尾向前计算偏移 */
    uint32_t offset = total_size;

    /* 配置B区：最后第1个扇区 */
    offset -= FLASH_LAYOUT_SECTOR_SIZE;
    s_region_offsets[FLASH_REGION_CONFIG_B] = offset;
    s_region_sizes[FLASH_REGION_CONFIG_B] = FLASH_LAYOUT_SECTOR_SIZE;

    /* 配置A区：最后第2个扇区 */
    offset -= FLASH_LAYOUT_SECTOR_SIZE;
    s_region_offsets[FLASH_REGION_CONFIG_A] = offset;
    s_region_sizes[FLASH_REGION_CONFIG_A] = FLASH_LAYOUT_SECTOR_SIZE;

    /* 错误日志区：最后第3个扇区 */
    offset -= FLASH_LAYOUT_SECTOR_SIZE;
    s_region_offsets[FLASH_REGION_FAULT_LOG] = offset;
    s_region_sizes[FLASH_REGION_FAULT_LOG] = FLASH_LAYOUT_SECTOR_SIZE;

    /* 按键统计区：最后第4个扇区 */
    offset -= FLASH_LAYOUT_SECTOR_SIZE;
    s_region_offsets[FLASH_REGION_KEY_STATS] = offset;
    s_region_sizes[FLASH_REGION_KEY_STATS] = FLASH_LAYOUT_SECTOR_SIZE;
}

/* 获取大小描述字符串 */
static const char* size_to_string(uint32_t size_bytes)
{
    switch (size_bytes)
    {
        case 128 * 1024:   return "128KB";
        case 256 * 1024:   return "256KB";
        case 512 * 1024:   return "512KB";
        case 1 * 1024 * 1024:  return "1MB";
        case 2 * 1024 * 1024:  return "2MB";
        case 4 * 1024 * 1024:  return "4MB";
        case 8 * 1024 * 1024:  return "8MB";
        case 16 * 1024 * 1024: return "16MB";
        default:
            if (size_bytes >= 1024 * 1024)
            {
                return ">=16MB";
            }
            else if (size_bytes >= 1024)
            {
                return "unknown KB";
            }
            else
            {
                return "unknown";
            }
    }
}

/* ==================== 对外接口 ==================== */

void flash_layout_init(void)
{
    if (s_initialized)
    {
        return;
    }

    /* 检测Flash大小 */
    s_flash_total_size = detect_flash_size();
    s_size_string = size_to_string(s_flash_total_size);

    /* 计算各区域布局 */
    calculate_layout(s_flash_total_size);

    s_initialized = true;

    /* 记录检测结果 */
    fault_record(FAULT_LEVEL_INFO, "flash", "detected flash size");
}

uint32_t flash_layout_get_total_size(void)
{
    return s_flash_total_size;
}

const char* flash_layout_get_size_string(void)
{
    return s_size_string;
}

bool flash_layout_is_valid(void)
{
    if (!s_initialized)
    {
        return false;
    }

    /* 检查Flash是否足够大（至少需要16KB用于存储） */
    if (s_flash_total_size < FLASH_LAYOUT_TOTAL_SECTORS * FLASH_LAYOUT_SECTOR_SIZE)
    {
        return false;
    }

    /* 检查所有区域都在Flash范围内 */
    for (int i = 0; i < FLASH_REGION_COUNT; i++)
    {
        if (s_region_offsets[i] + s_region_sizes[i] > s_flash_total_size)
        {
            return false;
        }
    }

    return true;
}

uint32_t flash_layout_get_offset(flash_region_e region)
{
    if (!s_initialized || region >= FLASH_REGION_COUNT)
    {
        return 0xFFFFFFFFU;
    }
    return s_region_offsets[region];
}

uint32_t flash_layout_get_xip_addr(flash_region_e region)
{
    if (!s_initialized || region >= FLASH_REGION_COUNT)
    {
        return 0xFFFFFFFFU;
    }
    return FLASH_LAYOUT_XIP_BASE + s_region_offsets[region];
}

uint32_t flash_layout_get_size(flash_region_e region)
{
    if (!s_initialized || region >= FLASH_REGION_COUNT)
    {
        return 0;
    }
    return s_region_sizes[region];
}
