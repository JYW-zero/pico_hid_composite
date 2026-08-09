/*
 * include/board/flash_layout.h
 * Flash布局统一管理模块
 *
 * 使用pico-sdk官方API运行时检测Flash大小，
 * 统一管理所有持久化存储区域的地址。
 *
 * 布局（从Flash末尾向前排列）：
 *   最后第1个4KB扇区: 配置B区
 *   最后第2个4KB扇区: 配置A区
 *   最后第3个4KB扇区: 错误日志区
 *   最后第4个4KB扇区: 按键统计区
 *
 * 总计：16KB（4个扇区）
 */

#ifndef BOARD_FLASH_LAYOUT_H
#define BOARD_FLASH_LAYOUT_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/* Flash扇区大小（pico-sdk官方定义：4KB） */
#define FLASH_LAYOUT_SECTOR_SIZE    FLASH_SECTOR_SIZE  /* 4096 bytes */

/* 每个存储区域占用的扇区数 */
#define FLASH_LAYOUT_CONFIG_SECTORS     2  /* 配置区：A区 + B区，双备份 */
#define FLASH_LAYOUT_FAULT_SECTORS      1  /* 错误日志区 */
#define FLASH_LAYOUT_KEY_STATS_SECTORS  1  /* 按键统计区 */

/* 总占用扇区数 */
#define FLASH_LAYOUT_TOTAL_SECTORS \
    (FLASH_LAYOUT_CONFIG_SECTORS + FLASH_LAYOUT_FAULT_SECTORS + FLASH_LAYOUT_KEY_STATS_SECTORS)

/* Flash XIP基地址（pico-sdk官方定义） */
#define FLASH_LAYOUT_XIP_BASE    0x10000000U

/* ==================== 区域索引 ==================== */

typedef enum {
    FLASH_REGION_CONFIG_A = 0,   /* 配置A区 */
    FLASH_REGION_CONFIG_B,       /* 配置B区 */
    FLASH_REGION_FAULT_LOG,      /* 错误日志区 */
    FLASH_REGION_KEY_STATS,      /* 按键统计区 */
    FLASH_REGION_COUNT           /* 区域数量 */
} flash_region_e;

/* ==================== 初始化与检测 ==================== */

/**
 * @brief 初始化Flash布局模块
 *
 * 使用pico-sdk官方API检测Flash实际大小，
 * 计算各个存储区域的地址。
 *
 * @note 必须在使用其他flash_layout函数之前调用
 * @note 建议在board_init()中早期调用
 */
void flash_layout_init(void);

/**
 * @brief 获取检测到的Flash总大小（字节）
 * @return Flash大小（字节），如果未初始化返回0
 */
uint32_t flash_layout_get_total_size(void);

/**
 * @brief 获取检测到的Flash大小描述字符串
 * @return 大小描述字符串，如 "4MB"、"2MB"
 */
const char* flash_layout_get_size_string(void);

/**
 * @brief 检查Flash布局是否有效（所有区域都在Flash范围内）
 * @return true=有效，false=Flash太小
 */
bool flash_layout_is_valid(void);

/* ==================== 区域地址获取 ==================== */

/**
 * @brief 获取指定区域的Flash偏移（相对于Flash起始地址）
 * @param region 区域索引
 * @return 偏移地址（字节），无效区域返回0xFFFFFFFF
 */
uint32_t flash_layout_get_offset(flash_region_e region);

/**
 * @brief 获取指定区域的XIP地址（可直接读取的内存地址）
 * @param region 区域索引
 * @return XIP地址，无效区域返回0xFFFFFFFF
 */
uint32_t flash_layout_get_xip_addr(flash_region_e region);

/**
 * @brief 获取指定区域的大小（字节）
 * @param region 区域索引
 * @return 区域大小（字节），无效区域返回0
 */
uint32_t flash_layout_get_size(flash_region_e region);

/* ==================== 便捷函数 ==================== */

/* 配置A区 */
static inline uint32_t flash_layout_config_a_offset(void) {
    return flash_layout_get_offset(FLASH_REGION_CONFIG_A);
}
static inline uint32_t flash_layout_config_a_addr(void) {
    return flash_layout_get_xip_addr(FLASH_REGION_CONFIG_A);
}

/* 配置B区 */
static inline uint32_t flash_layout_config_b_offset(void) {
    return flash_layout_get_offset(FLASH_REGION_CONFIG_B);
}
static inline uint32_t flash_layout_config_b_addr(void) {
    return flash_layout_get_xip_addr(FLASH_REGION_CONFIG_B);
}

/* 错误日志区 */
static inline uint32_t flash_layout_fault_offset(void) {
    return flash_layout_get_offset(FLASH_REGION_FAULT_LOG);
}
static inline uint32_t flash_layout_fault_addr(void) {
    return flash_layout_get_xip_addr(FLASH_REGION_FAULT_LOG);
}

/* 按键统计区 */
static inline uint32_t flash_layout_key_stats_offset(void) {
    return flash_layout_get_offset(FLASH_REGION_KEY_STATS);
}
static inline uint32_t flash_layout_key_stats_addr(void) {
    return flash_layout_get_xip_addr(FLASH_REGION_KEY_STATS);
}

#ifdef __cplusplus
}
#endif

#endif /* BOARD_FLASH_LAYOUT_H */
