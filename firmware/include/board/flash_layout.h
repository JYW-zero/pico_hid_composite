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
 *   最后第3-4个4KB扇区: 错误日志区（A/B双扇区，磨损均衡）
 *   最后第5个4KB扇区: 按键统计区
 *   最后第6个4KB扇区: 出厂配置/制造标志区（Provisioning，阶段3启用，阶段2仅占位）
 *
 * 总计：24KB（6个扇区）
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
#define FLASH_LAYOUT_FAULT_SECTORS      2  /* 错误日志区（A/B双扇区，磨损均衡） */
#define FLASH_LAYOUT_KEY_STATS_SECTORS  1  /* 按键统计区 */
#define FLASH_LAYOUT_PROVISIONING_SECTORS 1  /* 出厂配置/制造标志区（阶段3启用） */

/* 总占用扇区数 */
#define FLASH_LAYOUT_TOTAL_SECTORS \
    (FLASH_LAYOUT_CONFIG_SECTORS + FLASH_LAYOUT_FAULT_SECTORS + \
     FLASH_LAYOUT_KEY_STATS_SECTORS + FLASH_LAYOUT_PROVISIONING_SECTORS)

/* Flash XIP基地址（pico-sdk官方定义） */
#define FLASH_LAYOUT_XIP_BASE    0x10000000U

/* ==================== 区域索引 ==================== */

typedef enum {
    FLASH_REGION_CONFIG_A = 0,   /* 配置A区 */
    FLASH_REGION_CONFIG_B,       /* 配置B区 */
    FLASH_REGION_FAULT_LOG_A,    /* 错误日志区A（磨损均衡） */
    FLASH_REGION_FAULT_LOG_B,    /* 错误日志区B（磨损均衡） */
    FLASH_REGION_KEY_STATS,      /* 按键统计区 */
    FLASH_REGION_PROVISIONING,   /* 出厂配置/制造标志区（阶段3启用，阶段2仅占位） */
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
 * @brief 获取 Flash 的 XIP 只读基地址（弱函数，单元测试可覆盖为 RAM 缓冲）
 *
 * 固件返回硬件 XIP 基址（0x10000000）；host 单元测试由 mock 覆盖为模拟
 * Flash 数组地址，从而让“直接指针读 Flash”的代码可在主机上测试。
 * 返回 uintptr_t，使用方转换为指针后再叠加区域偏移。
 *
 * @return XIP 基地址
 */
uintptr_t flash_layout_xip_base(void);

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

/* 错误日志区A（磨损均衡） */
static inline uint32_t flash_layout_fault_a_offset(void) {
    return flash_layout_get_offset(FLASH_REGION_FAULT_LOG_A);
}
static inline uint32_t flash_layout_fault_a_addr(void) {
    return flash_layout_get_xip_addr(FLASH_REGION_FAULT_LOG_A);
}

/* 错误日志区B（磨损均衡） */
static inline uint32_t flash_layout_fault_b_offset(void) {
    return flash_layout_get_offset(FLASH_REGION_FAULT_LOG_B);
}
static inline uint32_t flash_layout_fault_b_addr(void) {
    return flash_layout_get_xip_addr(FLASH_REGION_FAULT_LOG_B);
}

/* 错误日志区（兼容旧代码，返回A区） */
static inline uint32_t flash_layout_fault_offset(void) {
    return flash_layout_get_offset(FLASH_REGION_FAULT_LOG_A);
}
static inline uint32_t flash_layout_fault_addr(void) {
    return flash_layout_get_xip_addr(FLASH_REGION_FAULT_LOG_A);
}

/* 按键统计区 */
static inline uint32_t flash_layout_key_stats_offset(void) {
    return flash_layout_get_offset(FLASH_REGION_KEY_STATS);
}
static inline uint32_t flash_layout_key_stats_addr(void) {
    return flash_layout_get_xip_addr(FLASH_REGION_KEY_STATS);
}

/* 出厂配置/制造标志区（Provisioning，阶段3启用，阶段2仅占位） */
static inline uint32_t flash_layout_provisioning_offset(void) {
    return flash_layout_get_offset(FLASH_REGION_PROVISIONING);
}
static inline uint32_t flash_layout_provisioning_addr(void) {
    return flash_layout_get_xip_addr(FLASH_REGION_PROVISIONING);
}

#ifdef __cplusplus
}
#endif

#endif /* BOARD_FLASH_LAYOUT_H */
