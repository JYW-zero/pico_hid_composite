/*
 * tests/mock/hardware/flash.h
 * Mock版硬件Flash头文件
 * 用于单元测试，定义Flash相关常量
 */
#ifndef HARDWARE_FLASH_H
#define HARDWARE_FLASH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flash扇区大小（4KB） */
#define FLASH_SECTOR_SIZE     (4096U)

/* Flash页大小（256字节） */
#define FLASH_PAGE_SIZE       (256U)

/* Flash XIP基地址 */
#define XIP_BASE              (0x10000000U)

/* 擦除Flash扇区（mock：空操作） */
void flash_range_erase(uint32_t offset, size_t count);

/* 编程Flash页（mock：空操作） */
void flash_range_program(uint32_t offset, const uint8_t *data, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_FLASH_H */
