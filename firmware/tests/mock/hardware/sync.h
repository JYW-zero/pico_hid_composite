/*
 * tests/mock/hardware/sync.h
 * Mock版hardware/sync.h头文件，用于单元测试
 */

#ifndef HARDWARE_SYNC_H
#define HARDWARE_SYNC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 保存并关闭中断，返回之前的中断状态 */
uint32_t save_and_disable_interrupts(void);

/* 恢复中断状态 */
void restore_interrupts(uint32_t status);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_SYNC_H */
