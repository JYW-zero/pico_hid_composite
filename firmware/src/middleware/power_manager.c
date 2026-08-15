/*
 * src/middleware/power_manager.c
 * 低功耗管理模块实现
 * 使用官方 pico_low_power API
 * 自动适配有线/无线模式
 */
#include "middleware/power_manager.h"
#include "middleware/fault.h"
#include "middleware/watchdog.h"
#include "board/pins.h"
#include "pico/low_power.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "middleware/ipc.h"
#include "pico/multicore.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ==================== 静态变量 ==================== */

static power_level_t s_current_level = POWER_LEVEL_ACTIVE;
static power_mode_t s_current_mode = POWER_MODE_WIRELESS;
static bool s_initialized = false;
static bool s_remote_wakeup_enabled = false;
static volatile bool s_pending_sleep = false;  /* 待休眠标志：USB回调中设置，主循环中执行 */

/* 最后一次用户活动的时间戳 */
static uint32_t s_last_activity_ms = 0;

/* 累计休眠时间（毫秒） */
static uint32_t s_total_sleep_ms = 0;

/* ==================== 内部函数 ==================== */

/* 配置 Dormant 模式的 GPIO 唤醒源 */
static void configure_dormant_wakeup_pins(void)
{
    /* PAW3395 MOT 引脚：鼠标移动时触发（下降沿） */
    gpio_set_dormant_irq_enabled(PAW3395_MOT_PIN, GPIO_IRQ_EDGE_FALL, true);
    
    /* 编码器 A 相：旋转时触发 */
    gpio_set_dormant_irq_enabled(ENCODER_A_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    
    /* 编码器 B 相：旋转时触发 */
    gpio_set_dormant_irq_enabled(ENCODER_B_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    
    /* 摇杆按键：按下时触发 */
    gpio_set_dormant_irq_enabled(JOYSTICK_BTN_PIN, GPIO_IRQ_EDGE_FALL, true);
    
    /* 注意：64 键 SPI 键盘的唤醒比较特殊
     * 因为 SPI 是主模式，CPU 休眠时不会主动读 SPI
     * 如果需要按键唤醒，需要硬件上有中断输出，或者用矩阵键盘的行列检测
     * 暂时先用 PAW3395、编码器、摇杆作为唤醒源
     * 键盘唤醒以后可以优化，比如用一个专用的中断引脚
     */
}

/* 清除 Dormant 模式的 GPIO 唤醒配置 */
static void clear_dormant_wakeup_pins(void)
{
    gpio_set_dormant_irq_enabled(PAW3395_MOT_PIN, GPIO_IRQ_EDGE_FALL, false);
    gpio_set_dormant_irq_enabled(ENCODER_A_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    gpio_set_dormant_irq_enabled(ENCODER_B_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    gpio_set_dormant_irq_enabled(JOYSTICK_BTN_PIN, GPIO_IRQ_EDGE_FALL, false);
}

/* 进入 Sleep 模式
 * Sleep 模式：CPU 停止，外设继续运行，任何中断都能唤醒
 * 功耗约 5.9mA，用于 USB 挂起时的有线模式
 */
static void enter_sleep_mode(void)
{
    if (s_current_level != POWER_LEVEL_ACTIVE)
    {
        return;
    }

    s_current_level = POWER_LEVEL_SLEEP;
    
    fault_record(FAULT_LEVEL_INFO, "power", "enter sleep");
    
    uint32_t enter_time = to_ms_since_boot(get_absolute_time());
    
    /* 进入 Sleep 模式，直到任何中断唤醒
     * keep_enabled 传 NULL，使用默认配置（保持必要的时钟）
     */
    low_power_sleep_until_irq(NULL);
    
    /* 唤醒后 */
    uint32_t exit_time = to_ms_since_boot(get_absolute_time());
    s_total_sleep_ms += (exit_time - enter_time);
    
    s_current_level = POWER_LEVEL_ACTIVE;
    
    fault_record(FAULT_LEVEL_INFO, "power", "wake from sleep");
}

/* 进入 Dormant 模式
 * Dormant 模式：XOSC 和 ROSC 都停止，功耗更低
 * 功耗约 3.3mA，用于无线模式无操作超时
 * 只能通过 GPIO 中断或 AON timer 唤醒
 */
static void enter_dormant_mode(void)
{
    if (s_current_level != POWER_LEVEL_ACTIVE)
    {
        return;
    }

    s_current_level = POWER_LEVEL_DORMANT;
    
    fault_record(FAULT_LEVEL_INFO, "power", "enter dormant");
    
    uint32_t enter_time = to_ms_since_boot(get_absolute_time());
    
    /* 配置 GPIO 唤醒源 */
    configure_dormant_wakeup_pins();
    
    /* 进入 Dormant 模式
     * 用 AON timer 设置一个很长的超时（1小时），主要靠 GPIO 唤醒
     * 使用 LPOSC 作为 dormant 时钟源（RP2350 默认）
     */
    absolute_time_t wakeup_time = make_timeout_time_ms(3600 * 1000);  /* 1小时超时 */
    low_power_dormant_until_aon_timer(wakeup_time, DORMANT_CLOCK_SOURCE_DEFAULT, NULL);
    
    /* 唤醒后 */
    uint32_t exit_time = to_ms_since_boot(get_absolute_time());
    s_total_sleep_ms += (exit_time - enter_time);
    
    /* 清除唤醒配置 */
    clear_dormant_wakeup_pins();

    s_current_level = POWER_LEVEL_ACTIVE;

    /* 调用唤醒回调（弱函数，上层可覆盖以重新初始化SPI/ADC等外设） */
    power_manager_on_dormant_wakeup();

    fault_record(FAULT_LEVEL_INFO, "power", "wake from dormant");
}

/* Dormant 唤醒回调默认实现（空函数，可由上层覆盖） */
__attribute__((weak)) void power_manager_on_dormant_wakeup(void)
{
    /* 默认不做任何操作
     * board/app 层可覆盖此函数，在 Dormant 唤醒后重新初始化 SPI/ADC 等外设
     */
}

/* ==================== 对外接口 ==================== */

void power_manager_init(void)
{
    s_current_level = POWER_LEVEL_ACTIVE;
    s_current_mode = POWER_MODE_WIRELESS;  /* 默认无线模式，USB 挂载后切换 */
    s_last_activity_ms = to_ms_since_boot(get_absolute_time());
    s_total_sleep_ms = 0;
    s_initialized = true;
    
    fault_record(FAULT_LEVEL_INFO, "power", "init complete");
}

void power_manager_tick(void)
{
    if (!s_initialized)
    {
        return;
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    /* 检查当前模式：USB 是否挂载 */
    extern bool tud_mounted(void);
    extern bool tud_suspended(void);
    
    if (tud_mounted())
    {
        s_current_mode = POWER_MODE_WIRED;
    }
    else
    {
        s_current_mode = POWER_MODE_WIRELESS;
    }
    
    /* 根据模式决定是否进入低功耗 */
    if (s_current_mode == POWER_MODE_WIRED)
    {
        /* 有线模式：USB 挂起时进入 Sleep
         * 优先处理 pending_sleep 标志（由 USB 回调设置）
         * tud_suspended() 作为后备检查
         */
        if (s_pending_sleep && s_current_level == POWER_LEVEL_ACTIVE)
        {
            s_pending_sleep = false;
            /* 休眠前喂一次所有层看门狗，确保休眠期间不会逻辑超时 */
            watchdog_feed_layer(WDG_LAYER_BOARD);
            watchdog_feed_layer(WDG_LAYER_DEVICE);
            watchdog_feed_layer(WDG_LAYER_APP);
            enter_sleep_mode();
        }
        else if (tud_suspended() && s_current_level == POWER_LEVEL_ACTIVE)
        {
            /* 后备：如果回调丢失但检测到挂起，也进入休眠 */
            s_pending_sleep = false;
            watchdog_feed_layer(WDG_LAYER_BOARD);
            watchdog_feed_layer(WDG_LAYER_DEVICE);
            watchdog_feed_layer(WDG_LAYER_APP);
            enter_sleep_mode();
        }
    }
    else
    {
        /* 无线模式：无操作超时进入 Dormant */
        uint32_t idle_time = now - s_last_activity_ms;
        if (idle_time >= POWER_DEEP_SLEEP_TIMEOUT_MS && 
            s_current_level == POWER_LEVEL_ACTIVE)
        {
            enter_dormant_mode();
            /* 唤醒后重置活动时间 */
            s_last_activity_ms = to_ms_since_boot(get_absolute_time());
        }
    }
}

void power_manager_notify_activity(void)
{
    if (!s_initialized)
    {
        return;
    }
    s_last_activity_ms = to_ms_since_boot(get_absolute_time());
}

void power_manager_on_usb_suspend(bool remote_wakeup_en)
{
    if (!s_initialized)
    {
        return;
    }
    
    s_remote_wakeup_enabled = remote_wakeup_en;
    s_current_mode = POWER_MODE_WIRED;
    
    fault_record(FAULT_LEVEL_INFO, "power", "usb suspend");
    
    /* 不在 USB 回调中直接休眠（会阻塞主循环导致看门狗超时）
     * 只设置标志位，由 power_manager_tick() 在主循环中执行休眠
     */
    s_pending_sleep = true;
}

void power_manager_on_usb_resume(void)
{
    if (!s_initialized)
    {
        return;
    }
    
    if (s_current_level == POWER_LEVEL_SLEEP)
    {
        s_current_level = POWER_LEVEL_ACTIVE;
        fault_record(FAULT_LEVEL_INFO, "power", "usb resume");
    }
    
    /* 重置活动时间 */
    s_last_activity_ms = to_ms_since_boot(get_absolute_time());
}

power_level_t power_manager_get_level(void)
{
    return s_current_level;
}

power_mode_t power_manager_get_mode(void)
{
    return s_current_mode;
}

uint32_t power_manager_get_sleep_time_ms(void)
{
    return s_total_sleep_ms;
}



