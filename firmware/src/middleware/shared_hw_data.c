/*
 * src/middleware/shared_hw_data.c
 * 双核共享硬件数据模块实现
 * 用多个官方自旋锁保护不同数据组，减少锁竞争
 * 锁分组：
 *   - motion_lock: 鼠标位移 + 滚轮 + 鼠标按键（高频）
 *   - keys_lock:   键盘状态（中频）
 *   - joystick_lock: 摇杆状态（低频）
 *   - misc_lock:   心跳 + 统计计数器（低频）
 */
#include "middleware/shared_hw_data.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ==================== 私有数据 ==================== */

/* 多个自旋锁，按数据类型分组，减少锁竞争 */
static spin_lock_t *s_motion_lock = NULL;    /* 鼠标位移 + 滚轮 + 鼠标按键 */
static spin_lock_t *s_keys_lock = NULL;      /* 键盘状态 */
static spin_lock_t *s_joystick_lock = NULL;  /* 摇杆状态 */
static spin_lock_t *s_misc_lock = NULL;      /* 心跳 + 统计计数器 */

/* volatile 确保多核心可见性 */
static volatile bool s_initialized = false;

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

    /* Core1 运行状态统计（Core1 递增，Core0 读取） */
    uint32_t keypad_scan_count;
    uint32_t optical_sensor_read_count;
    uint32_t encoder_scan_count;
    uint32_t joystick_read_count;
    uint32_t error_count;

    /* 去抖阈值配置（Core0 设置，Core1 读取应用） */
    uint8_t keypad_debounce_threshold;  /* 采样次数，默认2（10ms@5ms扫描） */
    bool debounce_threshold_updated;     /* 阈值已更新标志，Core1 读取后清除 */

    /* 摇杆校准（Core0 触发，Core1 执行） */
    bool joystick_calib_trigger;         /* 校准触发标志，Core1 读取后清除 */
    uint16_t joystick_calib_sample_count;/* 校准采样次数 */
    uint8_t joystick_calib_state;        /* 校准状态：0=空闲,1=校准中,2=完成 */
    /* 摇杆校准数据（Core1 写入，Core0 读取保存到配置；Core0 启动时写入，Core1 读取） */
    uint16_t joystick_calib_center_x;    /* X 轴中心点（ADC 值） */
    uint16_t joystick_calib_center_y;    /* Y 轴中心点（ADC 值） */
    uint8_t joystick_calib_calibrated;   /* 是否已校准 */
    bool joystick_calib_data_updated;     /* 校准数据已更新标志（Core1 设置，Core0 读取后清除） */
} s_data;

/* ==================== 初始化 ==================== */

void shared_hw_data_init(void)
{
    if (s_initialized)
    {
        return;
    }

    /* 分配并初始化多个自旋锁（使用 striped spin lock 自动分配） */
    s_motion_lock   = spin_lock_init(next_striped_spin_lock_num());
    s_keys_lock     = spin_lock_init(next_striped_spin_lock_num());
    s_joystick_lock = spin_lock_init(next_striped_spin_lock_num());
    s_misc_lock     = spin_lock_init(next_striped_spin_lock_num());

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
    s_data.keypad_scan_count = 0;
    s_data.optical_sensor_read_count = 0;
    s_data.encoder_scan_count = 0;
    s_data.joystick_read_count = 0;
    s_data.error_count = 0;
    s_data.keypad_debounce_threshold = 2;  /* 默认阈值2，5ms扫描 → 10ms去抖 */
    s_data.debounce_threshold_updated = false;
    s_data.joystick_calib_trigger = false;
    s_data.joystick_calib_sample_count = 0;
    s_data.joystick_calib_state = 0;
    s_data.joystick_calib_center_x = 2048;
    s_data.joystick_calib_center_y = 2048;
    s_data.joystick_calib_calibrated = 0;
    s_data.joystick_calib_data_updated = false;

    /* 编译器内存屏障：确保所有初始化数据写入完成后，再设置 s_initialized */
    asm volatile("" ::: "memory");
    s_initialized = true;
}

/* ==================== Core1 写入接口（生产者） ==================== */

void shared_hw_add_motion(int32_t dx, int32_t dy)
{
    if (!s_initialized)
    {
        return;
    }

    uint32_t irq = spin_lock_blocking(s_motion_lock);
    s_data.mouse_dx += dx;
    s_data.mouse_dy += dy;
    spin_unlock(s_motion_lock, irq);
}

void shared_hw_add_wheel(int32_t delta)
{
    if (!s_initialized)
    {
        return;
    }

    uint32_t irq = spin_lock_blocking(s_motion_lock);
    s_data.wheel += delta;
    spin_unlock(s_motion_lock, irq);
}

void shared_hw_set_keys(uint64_t keys)
{
    if (!s_initialized)
    {
        return;
    }

    uint32_t irq = spin_lock_blocking(s_keys_lock);
    s_data.keys = keys;
    spin_unlock(s_keys_lock, irq);
}

void shared_hw_set_mouse_buttons(uint8_t buttons)
{
    if (!s_initialized)
    {
        return;
    }

    uint32_t irq = spin_lock_blocking(s_motion_lock);
    s_data.mouse_buttons |= buttons;  /* OR合并：只设置位，不清除其他位 */
    spin_unlock(s_motion_lock, irq);
}

void shared_hw_clear_mouse_buttons(uint8_t buttons)
{
    if (!s_initialized)
    {
        return;
    }

    uint32_t irq = spin_lock_blocking(s_motion_lock);
    s_data.mouse_buttons &= ~buttons;  /* 清除指定位 */
    spin_unlock(s_motion_lock, irq);
}

void shared_hw_set_joystick(int16_t x, int16_t y, bool btn)
{
    if (!s_initialized)
    {
        return;
    }

    uint32_t irq = spin_lock_blocking(s_joystick_lock);
    s_data.joy_x = x;
    s_data.joy_y = y;
    s_data.joy_btn = btn;
    spin_unlock(s_joystick_lock, irq);
}

void shared_hw_increment_heartbeat(void)
{
    if (!s_initialized)
    {
        return;
    }

    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.heartbeat++;
    spin_unlock(s_misc_lock, irq);
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

    uint32_t irq = spin_lock_blocking(s_motion_lock);
    *out_dx = s_data.mouse_dx;
    *out_dy = s_data.mouse_dy;
    s_data.mouse_dx = 0;
    s_data.mouse_dy = 0;
    spin_unlock(s_motion_lock, irq);
}

int32_t shared_hw_take_wheel(void)
{
    if (!s_initialized)
    {
        return 0;
    }

    int32_t result;
    uint32_t irq = spin_lock_blocking(s_motion_lock);
    result = s_data.wheel;
    s_data.wheel = 0;
    spin_unlock(s_motion_lock, irq);
    return result;
}

uint64_t shared_hw_get_keys(void)
{
    if (!s_initialized)
    {
        return 0xFFFFFFFFFFFFFFFFULL;
    }

    uint64_t result;
    uint32_t irq = spin_lock_blocking(s_keys_lock);
    result = s_data.keys;
    spin_unlock(s_keys_lock, irq);
    return result;
}

uint8_t shared_hw_get_mouse_buttons(void)
{
    if (!s_initialized)
    {
        return 0;
    }

    uint8_t result;
    uint32_t irq = spin_lock_blocking(s_motion_lock);
    result = s_data.mouse_buttons;
    spin_unlock(s_motion_lock, irq);
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

    uint32_t irq = spin_lock_blocking(s_joystick_lock);
    if (out_x != NULL) *out_x = s_data.joy_x;
    if (out_y != NULL) *out_y = s_data.joy_y;
    if (out_btn != NULL) *out_btn = s_data.joy_btn;
    spin_unlock(s_joystick_lock, irq);
}

uint32_t shared_hw_get_heartbeat(void)
{
    if (!s_initialized)
    {
        return 0;
    }

    uint32_t result;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    result = s_data.heartbeat;
    spin_unlock(s_misc_lock, irq);
    return result;
}

/* ==================== 状态统计 ==================== */

uint32_t shared_hw_get_keypad_scan_count(void)
{
    if (!s_initialized) return 0;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    uint32_t result = s_data.keypad_scan_count;
    spin_unlock(s_misc_lock, irq);
    return result;
}

uint32_t shared_hw_get_optical_sensor_read_count(void)
{
    if (!s_initialized) return 0;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    uint32_t result = s_data.optical_sensor_read_count;
    spin_unlock(s_misc_lock, irq);
    return result;
}

uint32_t shared_hw_get_encoder_scan_count(void)
{
    if (!s_initialized) return 0;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    uint32_t result = s_data.encoder_scan_count;
    spin_unlock(s_misc_lock, irq);
    return result;
}

uint32_t shared_hw_get_joystick_read_count(void)
{
    if (!s_initialized) return 0;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    uint32_t result = s_data.joystick_read_count;
    spin_unlock(s_misc_lock, irq);
    return result;
}

uint32_t shared_hw_get_error_count(void)
{
    if (!s_initialized) return 0;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    uint32_t result = s_data.error_count;
    spin_unlock(s_misc_lock, irq);
    return result;
}

void shared_hw_reset_stats(void)
{
    if (!s_initialized) return;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.keypad_scan_count = 0;
    s_data.optical_sensor_read_count = 0;
    s_data.encoder_scan_count = 0;
    s_data.joystick_read_count = 0;
    s_data.error_count = 0;
    spin_unlock(s_misc_lock, irq);
}

/* ==================== Core1 专用递增函数 ==================== */

void shared_hw_inc_keypad_scan(void)
{
    if (!s_initialized) return;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.keypad_scan_count++;
    spin_unlock(s_misc_lock, irq);
}

void shared_hw_inc_optical_sensor_read(void)
{
    if (!s_initialized) return;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.optical_sensor_read_count++;
    spin_unlock(s_misc_lock, irq);
}

void shared_hw_inc_encoder_scan(void)
{
    if (!s_initialized) return;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.encoder_scan_count++;
    spin_unlock(s_misc_lock, irq);
}

void shared_hw_inc_joystick_read(void)
{
    if (!s_initialized) return;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.joystick_read_count++;
    spin_unlock(s_misc_lock, irq);
}

void shared_hw_inc_error(void)
{
    if (!s_initialized) return;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.error_count++;
    spin_unlock(s_misc_lock, irq);
}

/* ==================== 去抖阈值配置 ==================== */

void shared_hw_set_keypad_debounce_threshold(uint8_t threshold)
{
    if (!s_initialized) return;
    if (threshold == 0) threshold = 1;  /* 最小阈值为1 */
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.keypad_debounce_threshold = threshold;
    s_data.debounce_threshold_updated = true;
    spin_unlock(s_misc_lock, irq);
}

uint8_t shared_hw_get_keypad_debounce_threshold(void)
{
    if (!s_initialized) return 2;  /* 默认值 */
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    uint8_t result = s_data.keypad_debounce_threshold;
    spin_unlock(s_misc_lock, irq);
    return result;
}

bool shared_hw_check_and_clear_debounce_updated(void)
{
    if (!s_initialized) return false;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    bool result = s_data.debounce_threshold_updated;
    s_data.debounce_threshold_updated = false;
    spin_unlock(s_misc_lock, irq);
    return result;
}

/* ==================== 摇杆校准 ==================== */

bool shared_hw_trigger_joystick_calibration(uint16_t sample_count)
{
    if (!s_initialized) return false;
    if (sample_count == 0) sample_count = 50;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    /* 如果已经在校准中，返回 false */
    if (s_data.joystick_calib_state == 1)
    {
        spin_unlock(s_misc_lock, irq);
        return false;
    }
    s_data.joystick_calib_trigger = true;
    s_data.joystick_calib_sample_count = sample_count;
    s_data.joystick_calib_state = 0;
    spin_unlock(s_misc_lock, irq);
    return true;
}

bool shared_hw_check_and_clear_joystick_calib_trigger(uint16_t *sample_count)
{
    if (!s_initialized) return false;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    bool result = s_data.joystick_calib_trigger;
    if (result && sample_count != NULL)
    {
        *sample_count = s_data.joystick_calib_sample_count;
    }
    s_data.joystick_calib_trigger = false;
    spin_unlock(s_misc_lock, irq);
    return result;
}

uint8_t shared_hw_get_joystick_calibration_state(void)
{
    if (!s_initialized) return 0;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    uint8_t result = s_data.joystick_calib_state;
    spin_unlock(s_misc_lock, irq);
    return result;
}

void shared_hw_set_joystick_calibration_state(uint8_t state)
{
    if (!s_initialized) return;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.joystick_calib_state = state;
    spin_unlock(s_misc_lock, irq);
}

void shared_hw_get_joystick_calibration_data(uint16_t *out_center_x,
                                                uint16_t *out_center_y,
                                                uint8_t *out_calibrated)
{
    if (!s_initialized)
    {
        if (out_center_x) *out_center_x = 2048;
        if (out_center_y) *out_center_y = 2048;
        if (out_calibrated) *out_calibrated = 0;
        return;
    }
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    if (out_center_x) *out_center_x = s_data.joystick_calib_center_x;
    if (out_center_y) *out_center_y = s_data.joystick_calib_center_y;
    if (out_calibrated) *out_calibrated = s_data.joystick_calib_calibrated;
    spin_unlock(s_misc_lock, irq);
}

void shared_hw_set_joystick_calibration_data(uint16_t center_x,
                                                uint16_t center_y,
                                                uint8_t calibrated)
{
    if (!s_initialized) return;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.joystick_calib_center_x = center_x;
    s_data.joystick_calib_center_y = center_y;
    s_data.joystick_calib_calibrated = calibrated;
    spin_unlock(s_misc_lock, irq);
}

bool shared_hw_check_and_clear_joystick_calib_updated(void)
{
    if (!s_initialized) return false;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    bool result = s_data.joystick_calib_data_updated;
    s_data.joystick_calib_data_updated = false;
    spin_unlock(s_misc_lock, irq);
    return result;
}

void shared_hw_notify_joystick_calib_updated(void)
{
    if (!s_initialized) return;
    uint32_t irq = spin_lock_blocking(s_misc_lock);
    s_data.joystick_calib_data_updated = true;
    spin_unlock(s_misc_lock, irq);
}
