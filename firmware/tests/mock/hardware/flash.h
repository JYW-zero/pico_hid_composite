/*
 * tests/mock/hardware/flash.h
 * Mock版hardware/flash.h头文件，用于单元测试
 */

#ifndef HARDWARE_FLASH_H
#define HARDWARE_FLASH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 擦除Flash扇区 */
void flash_range_erase(uint32_t flash_offset, size_t count);

/* 写入Flash数据 */
void flash_range_program(uint32_t flash_offset, const uint8_t* data, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_FLASH_H */
