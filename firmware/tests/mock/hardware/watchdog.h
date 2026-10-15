/*
 * tests/mock/hardware/watchdog.h
 * Mock版硬件看门狗头文件
 * 用于单元测试，定义看门狗相关常量和函数
 */
#ifndef HARDWARE_WATCHDOG_H
#define HARDWARE_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 看门狗复位原因 */
#define WATCHDOG_REASON_NONE       (0x00000000U)
#define WATCHDOG_REASON_WATCHDOG   (0x00000002U)
#define WATCHDOG_REASON_POWER_ON   (0x00000001U)

/* 看门狗硬件寄存器（mock） */
typedef struct {
    volatile uint32_t reason;
    volatile uint32_t load;
    volatile uint32_t scratch[8];
} watchdog_hw_t;

/* 看门狗硬件实例（mock） */
extern watchdog_hw_t watchdog_hw;

/* 使能看门狗（mock：空操作） */
void watchdog_enable(uint32_t delay_ms, bool pause_on_debug);

/* 喂狗（mock：空操作） */
void watchdog_update(void);

/* 获取复位原因（mock：返回0） */
uint32_t watchdog_get_reset_reason(void);

/* 检查本次上电是否由看门狗/HardFault 复位触发（mock，默认 false） */
bool watchdog_caused_reboot(void);

/* 测试辅助：设置 watchdog_caused_reboot() 的返回值 */
void mock_watchdog_set_caused_reboot(bool caused);

/* 看门狗复位（mock：空操作） */
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_WATCHDOG_H */
