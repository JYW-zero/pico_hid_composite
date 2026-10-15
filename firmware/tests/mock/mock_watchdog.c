/*
 * tests/mock/mock_watchdog.c
 * Mock版看门狗函数实现
 */

#include "hardware/watchdog.h"

/* 看门狗硬件实例（mock） */
watchdog_hw_t watchdog_hw = {0};

static bool s_watchdog_enabled = false;
static uint32_t s_watchdog_delay_ms = 0;
static bool s_caused_reboot = false;

/* 测试辅助：设置本次上电是否由看门狗/HardFault 复位触发 */
void mock_watchdog_set_caused_reboot(bool caused)
{
    s_caused_reboot = caused;
}

/* 启用看门狗 */
void watchdog_enable(uint32_t delay_ms, bool pause_on_debug)
{
    (void)pause_on_debug;
    s_watchdog_enabled = true;
    s_watchdog_delay_ms = delay_ms;
    watchdog_hw.load = delay_ms;
}

/* 禁用看门狗 */
void watchdog_disable(void)
{
    s_watchdog_enabled = false;
}

/* 喂狗 */
void watchdog_update(void)
{
    /* mock 实现：什么都不做 */
}

/* 启动看门狗定时器 */
void watchdog_start_tick(uint32_t cycles)
{
    (void)cycles;
}

/* 检查是否是看门狗复位 */
bool watchdog_caused_reboot(void)
{
    return s_caused_reboot;
}

/* 获取复位原因 */
uint32_t watchdog_get_reboot_reason(void)
{
    return 0;
}

/* 看门狗复位（mock 实现：不实际复位，只记录） */
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms)
{
    (void)pc;
    (void)sp;
    (void)delay_ms;
    /* mock 实现：不实际复位 */
}
