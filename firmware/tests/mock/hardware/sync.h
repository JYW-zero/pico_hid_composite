/*
 * tests/mock/hardware/sync.h
 * Mock版硬件同步原语头文件
 * 用于单元测试，在单线程环境中自旋锁简化为空操作
 */
#ifndef HARDWARE_SYNC_H
#define HARDWARE_SYNC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 自旋锁类型 mock */
typedef struct {
    uint32_t dummy;
} spin_lock_t;

/* 申请一个未使用的自旋锁号 */
uint32_t spin_lock_claim_unused(bool required);

/* 初始化自旋锁 */
spin_lock_t *spin_lock_init(uint32_t lock_num);

/* 阻塞式获取自旋锁，返回中断状态 */
uint32_t spin_lock_blocking(spin_lock_t *lock);

/* 释放自旋锁，恢复中断状态 */
void spin_unlock(spin_lock_t *lock, uint32_t irq_state);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_SYNC_H */
