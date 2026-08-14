/*
 * include/middleware/flash_service.h
 * Flash安全写入服务（双核同步）
 *
 * 使用Pico SDK官方的flash_safe_execute()机制，
 * 在写Flash前自动暂停Core1、禁用中断，
 * 完成后恢复，避免USB断开、看门狗超时、系统卡死。
 */

#ifndef MIDDLEWARE_FLASH_SERVICE_H
#define MIDDLEWARE_FLASH_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 初始化 ==================== */

/**
 * @brief 初始化Flash安全写入服务
 * @note 必须在Core0启动时早期调用
 */
void flash_service_init(void);

/**
 * @brief 在Core1中调用，注册为可被锁定的核心
 * @note 必须在Core1主函数开头调用，否则flash_safe_execute会失败
 */
void flash_service_core1_init(void);

/* ==================== 安全写入接口 ==================== */

/**
 * @brief 安全擦除Flash扇区
 * @param flash_offset 相对于Flash起始地址的偏移（必须4096对齐）
 * @param size 擦除大小（必须4096的倍数）
 * @return true=成功，false=失败
 */
bool flash_service_erase(uint32_t flash_offset, uint32_t size);

/**
 * @brief 安全写入Flash（按页编程，256字节对齐）
 * @param flash_offset 相对于Flash起始地址的偏移（必须256对齐）
 * @param data 数据指针（必须指向RAM）
 * @param size 写入大小（必须256的倍数）
 * @return true=成功，false=失败
 */
bool flash_service_program(uint32_t flash_offset, const uint8_t *data, uint32_t size);

/**
 * @brief 安全擦除并写入整个扇区
 * @param flash_offset 相对于Flash起始地址的偏移（必须4096对齐）
 * @param data 数据指针（必须指向RAM，大小为4096字节）
 * @return true=成功，false=失败
 */
bool flash_service_write_sector(uint32_t flash_offset, const uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_FLASH_SERVICE_H */
