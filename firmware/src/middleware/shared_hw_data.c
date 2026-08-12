/*
 * src/middleware/shared_hw_data.c
 * 双核共享硬件数据模块实现
 * 用官方自旋锁保护，线程安全
 */
#include "middleware/shared_hw_data.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ==================== 私有数据 ==================== */

/* 自旋锁指针 */
static spin_lock_t *s_spinlock = NULL;
static bool s_initialized = false;

/* 共享数据 */
static struct
{
    /* 累积型数据（Core1 加，Core0 取走清零） */
    int32_t mouse_dx;
    int32_t mouse_dy;
    int32_t wheel;
    
    /* 状态型数据（最新值） */
    uint64_t keys;
    uint8_t mouse_buttons;
    int16_t joy_x;
    int16_t joy_y;
    bool joy_btn;
    
    /* 心跳计数器：Core1 递增，Core0 读取，用于监控 Core1 是否正常运行 */
    uint32_t heartbeat;

    /* 状态统计计数器：Core1 递增，Core0 读取 */
    uint32_t keypad_scan_count;
    uint32_t paw3395_read_count;
    uint32_t encoder_scan_count;
    uint32_t joystick_read_count;
    uint32_t error_count;
} s_data;

/* ==================== 初始化 ==================== */

void shared_hw_data_init(void)
{
    if (s_initialized)
    {
        return;
    }

    /* 分配并初始化自旋锁 */
    uint lock_num = next_striped_spin_lock_num();
    s_spinlock = spin_lock_init(lock_num);
    
    /* 初始化数据 */
    s_data.mouse_dx = 0;
    s_data.mouse_dy = 0;
    s_data.wheel = 0;
    s_data.keys = 0xFFFFFFFFFFFFFFFFULL;  /* 全1，所有按键松开 */
    s_data.mouse_buttons = 0;
    s_data.joy_x = 0;
    s_data.joy_y = 0;
    s_data.joy_btn = false;
    s_data.heartbeat = 0;
    
    s_initialized = true;
}

/* ==================== Core1 写入接口（生产者） ==================== */

void shared_hw_add_motion(int32_t dx, int32_t dy)
{
    if (!s_initialized)
    {
        return;
    }
    
    uint32_t irq = spin_lock_blocking(s_spinlock);
    s_data.mouse_dx += dx;
    s_data.mouse_dy += dy;
    spin_unlock(s_spinlock, irq);
}

void shared_hw_add_wheel(int32_t delta)
{
    if (!s_initialized)
    {
        return;
    }
    
    uint32_t irq = spin_lock_blocking(s_spinlock);
    s_data.wheel += delta;
    spin_unlock(s_spinlock, irq);
}

void shared_hw_set_keys(uint64_t keys)
{
    if (!s_initialized)
    {
        return;
    }
    
    uint32_t irq = spin_lock_blocking(s_spinlock);
    s_data.keys = keys;
    spin_unlock(s_spinlock, irq);
}

void shared_hw_set_mouse_buttons(uint8_t buttons)
{
    if (!s_initialized)
    {
        return;
    }
    
    uint32_t irq = spin_lock_blocking(s_spinlock);
    s_data.mouse_buttons = buttons;
    spin_unlock(s_spinlock, irq);
}

void shared_hw_set_joystick(int16_t x, int16_t y, bool btn)
{
    if (!s_initialized)
    {
        return;
    }
    
    uint32_t irq = spin_lock_blocking(s_spinlock);
    s_data.joy_x = x;
    s_data.joy_y = y;
    s_data.joy_btn = btn;
    spin_unlock(s_spinlock, irq);
}

void shared_hw_increment_heartbeat(void)
{
    if (!s_initialized)
    {
        return;
    }
    
    uint32_t irq = spin_lock_blocking(s_spinlock);
    s_data.heartbeat++;
    spin_unlock(s_spinlock, irq);
}

/* ==================== Core0 读取接口（消费者） ==================== */

void shared_hw_take_motion(int32_t *out_dx, int32_t *out_dy)
{
    if (!s_initialized || out_dx == NULL || out_dy == NULL)
    {
        if (out_dx != NULL) *out_dx = 0;
        if (out_dy != NULL) *out_dy = 0;
        return;
    }
    
    uint32_t irq = spin_lock_blocking(s_spinlock);
    *out_dx = s_data.mouse_dx;
    *out_dy = s_data.mouse_dy;
    s_data.mouse_dx = 0;
    s_data.mouse_dy = 0;
    spin_unlock(s_spinlock, irq);
}

int32_t shared_hw_take_wheel(void)
{
    if (!s_initialized)
    {
        return 0;
    }
    
    int32_t result;
    uint32_t irq = spin_lock_blocking(s_spinlock);
    result = s_data.wheel;
    s_data.wheel = 0;
    spin_unlock(s_spinlock, irq);
    return result;
}

uint64_t shared_hw_get_keys(void)
{
    if (!s_initialized)
    {
        return 0xFFFFFFFFFFFFFFFFULL;
    }
    
    uint64_t result;
    uint32_t irq = spin_lock_blocking(s_spinlock);
    result = s_data.keys;
    spin_unlock(s_spinlock, irq);
    return result;
}

uint8_t shared_hw_get_mouse_buttons(void)
{
    if (!s_initialized)
    {
        return 0;
    }
    
    uint8_t result;
    uint32_t irq = spin_lock_blocking(s_spinlock);
    result = s_data.mouse_buttons;
    spin_unlock(s_spinlock, irq);
    return result;
}

void shared_hw_get_joystick(int16_t *out_x, int16_t *out_y, bool *out_btn)
{
    if (!s_initialized)
    {
        if (out_x != NULL) *out_x = 0;
        if (out_y != NULL) *out_y = 0;
        if (out_btn != NULL) *out_btn = false;
        return;
    }
    
    uint32_t irq = spin_lock_blocking(s_spinlock);
    if (out_x != NULL) *out_x = s_data.joy_x;
    if (out_y != NULL) *out_y = s_data.joy_y;
    if (out_btn != NULL) *out_btn = s_data.joy_btn;
    spin_unlock(s_spinlock, irq);
}

uint32_t shared_hw_get_heartbeat(void)
{
    if (!s_initialized)
    {
        return 0;
    }
    
    uint32_t result;
    uint32_t irq = spin_lock_blocking(s_spinlock);
    result = s_data.heartbeat;
    spin_unlock(s_spinlock, irq);
    return result;
}



