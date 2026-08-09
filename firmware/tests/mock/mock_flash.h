/*
 * tests/mock/mock_flash.h
 * Mock版Flash，用于单元测试
 * 用内存数组模拟Flash的擦除和写入操作
 */

#ifndef MOCK_FLASH_H
#define MOCK_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 模拟Flash大小：8KB（A区4KB + B区4KB） */
#define MOCK_FLASH_SIZE 8192U

/* 模拟Flash基地址偏移：对应真实Flash的0x3FE000位置 */
#define MOCK_FLASH_BASE_OFFSET 0x3FE000U

/* 重置模拟Flash为全0xFF（擦除状态） */
void mock_flash_reset(void);

/* 读取模拟Flash数据 */
void mock_flash_read(uint32_t offset, uint8_t* buf, size_t len);

/* 写入模拟Flash数据（不擦除，直接写，模拟program操作） */
void mock_flash_direct_write(uint32_t offset, const uint8_t* data, size_t len);

/* 检查指定区域是否全为0xFF */
bool mock_flash_is_erased(uint32_t offset, size_t len);

/* 模拟Flash数组（全局，用于地址计算） */
extern uint8_t s_mock_flash[MOCK_FLASH_SIZE];

/* 获取模拟Flash数组指针（用于直接访问） */
const uint8_t* mock_flash_get_ptr(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_FLASH_H */

