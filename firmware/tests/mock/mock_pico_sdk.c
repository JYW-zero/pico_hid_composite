/*
 * tests/mock/mock_pico_sdk.c
 * Mock Pico SDK functions for host unit tests.
 * Provides stub implementations of spinlock, fault backend, and core ID APIs
 * that would normally come from the Pico SDK.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "middleware/fault.h"
#include "middleware/fault_backend.h"

/* ---- spinlock mock ---- */
typedef struct {
    uint32_t ID;
    volatile uint32_t lock;
} spin_lock_t;

static spin_lock_t s_fault_lock;

uint32_t spin_lock_claim_unused(bool required) {
    (void)required;
    s_fault_lock.ID = 1;
    s_fault_lock.lock = 0;
    return s_fault_lock.ID;
}

spin_lock_t *spin_lock_init(uint32_t lock_num) {
    (void)lock_num;
    s_fault_lock.lock = 0;
    return &s_fault_lock;
}

uint32_t spin_lock_blocking(spin_lock_t *lock) {
    if (lock) lock->lock = 1;
    return 0;
}

void spin_unlock(spin_lock_t *lock, uint32_t irq_state) {
    (void)irq_state;
    if (lock) lock->lock = 0;
}

/* ---- core number mock ---- */
uint8_t get_core_num(void) {
    return 0;
}

/* ---- fault backend mock ---- */
void fault_backend_init(void) {}

void fault_backend_record(fault_level_e level, const char *module, const char *msg) {
    (void)level; (void)module; (void)msg;
}

uint32_t fault_backend_get_log_count(void) { return 0; }

bool fault_backend_get_log(uint32_t index, fault_log_entry_t *out_entry) {
    (void)index; (void)out_entry;
    return false;
}

void fault_backend_clear(void) {}

void fault_backend_flush(void) {}
