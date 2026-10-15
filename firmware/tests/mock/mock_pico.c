/*
 * tests/mock/mock_pico.c
 * Pico SDK相关函数的mock实现
 * 用于单元测试，在单线程环境下简化实现
 * 注意：
 *   - flash 函数在 mock_flash.c 中实现
 *   - 看门狗函数在 mock_watchdog.c 中实现
 *   - 时间函数在 mock_time.c 中实现
 *   - 本文件实现 sync(自旋锁) 和 platform 相关函数
 */
#include "hardware/sync.h"
#include "pico/platform.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ==================== hardware/sync.h ==================== */

static spin_lock_t s_mock_lock = {0};
static uint32_t s_next_lock_num = 0;

uint32_t spin_lock_claim_unused(bool required)
{
    (void)required;
    return s_next_lock_num++;
}

spin_lock_t *spin_lock_init(uint32_t lock_num)
{
    (void)lock_num;
    return &s_mock_lock;
}

uint32_t spin_lock_blocking(spin_lock_t *lock)
{
    (void)lock;
    return 0;
}

void spin_unlock(spin_lock_t *lock, uint32_t irq_state)
{
    (void)lock;
    (void)irq_state;
}

/* ==================== pico/platform.h ==================== */

uint8_t get_core_num(void)
{
    return 0;
}

void __dmb(void) {}
void __dsb(void) {}
void __isb(void) {}
void __wfi(void) {}
void __wfe(void) {}
void __sev(void) {}
void __nop(void) {}
void __breakpoint(void) {}

uint32_t __disable_irq(void)
{
    return 0;
}

void __enable_irq(void) {}

void __restore_irq(uint32_t status)
{
    (void)status;
}

void hw_set_bits(volatile uint32_t *addr, uint32_t mask)
{
    *addr |= mask;
}

void hw_clear_bits(volatile uint32_t *addr, uint32_t mask)
{
    *addr &= ~mask;
}

void hw_xor_bits(volatile uint32_t *addr, uint32_t mask)
{
    *addr ^= mask;
}

void hw_write_masked(volatile uint32_t *addr, uint32_t values, uint32_t write_mask)
{
    *addr = (*addr & ~write_mask) | (values & write_mask);
}

static uint32_t s_next_spin_lock_num = 0;

uint32_t next_striped_spin_lock_num(void)
{
    return s_next_spin_lock_num++;
}
