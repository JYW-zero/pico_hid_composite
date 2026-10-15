/*
 * tests/mock/pico/platform.h
 * Mock版Pico平台相关头文件
 * 用于单元测试，定义平台相关宏和函数
 */
#ifndef PICO_PLATFORM_H
#define PICO_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 核心数量 */
#define NUM_CORES 2

/* 当前核心号（mock：返回0） */
uint8_t get_core_num(void);

/* 内存屏障（mock：空操作） */
void __dmb(void);

/* 数据同步屏障（mock：空操作） */
void __dsb(void);

/* 指令同步屏障（mock：空操作） */
void __isb(void);

/* 等待中断（mock：空操作） */
void __wfi(void);

/* 等待事件（mock：空操作） */
void __wfe(void);

/* 发送事件（mock：空操作） */
void __sev(void);

/* 无操作（mock：空操作） */
void __nop(void);

/* 断点（mock：空操作） */
void __breakpoint(void);

/* 禁用中断（mock：返回0） */
uint32_t __disable_irq(void);

/* 使能中断（mock：空操作） */
void __enable_irq(void);

/* 恢复中断状态（mock：空操作） */
void __restore_irq(uint32_t status);

/* 原子操作：设置位（mock：简单实现） */
void hw_set_bits(volatile uint32_t *addr, uint32_t mask);

/* 原子操作：清除位（mock：简单实现） */
void hw_clear_bits(volatile uint32_t *addr, uint32_t mask);

/* 原子操作：翻转位（mock：简单实现） */
void hw_xor_bits(volatile uint32_t *addr, uint32_t mask);

/* 原子操作：写位（mock：简单实现） */
void hw_write_masked(volatile uint32_t *addr, uint32_t values, uint32_t write_mask);

/* 申请未使用的自旋锁号（mock：返回递增的计数器） */
uint32_t next_striped_spin_lock_num(void);

#ifdef __cplusplus
}
#endif

#endif /* PICO_PLATFORM_H */
