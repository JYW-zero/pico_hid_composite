/*
 * src/middleware/encoder_processor.c
 * 编码器处理器中间件实现
 * 功能：多模式切换、旋转加速度、去抖与方向判断
 */

#include "middleware/encoder_processor.h"
#include "middleware/fault.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/sync.h"

/* ==================== 常量定义 ==================== */

/* 事件队列大小 */
#define ENCODER_EVENT_QUEUE_SIZE  16

/* 加速度时间阈值（毫秒） */
#define ACCEL_THRESHOLD_1X_MS    200   /* >200ms: 1x */
#define ACCEL_THRESHOLD_2X_MS    100   /* 100-200ms: 2x */
#define ACCEL_THRESHOLD_5X_MS    50    /* 50-100ms: 5x */
/* <50ms: 10x */

/* 加速度曲线阈值表 [profile][level]
 * profile: 0=标准,1=慢,2=快
 * level: 0=1x阈值,1=2x阈值,2=5x阈值
 */
static const uint16_t s_accel_thresholds[3][3] =
{
    {200, 100, 50},   /* 标准 */
    {400, 200, 100},  /* 慢 */
    {100, 50, 25}     /* 快 */
};

/* 模式切换长按时间（毫秒） */
#define MODE_SWITCH_LONG_PRESS_MS  500

/* 加速度倍数表 */
static const uint8_t s_accel_multiplier[ENCODER_ACCEL_LEVELS] = {1, 2, 5, 10};

/* ==================== 静态变量 ==================== */

static encoder_config_t s_config = {
    .reverse = 0,
    .steps_per_tick = 3,
    .scroll_speed = 1,
    .accel_enabled = 1,
    .mode_switch_key = 0xFF  /* 0xFF=中键 */
};

static encoder_mode_t s_current_mode = ENCODER_MODE_SCROLL;

/* 加速度状态 */
static uint32_t s_last_event_time = 0;
static encoder_accel_level_t s_current_accel = ENCODER_ACCEL_1X;

/* 事件队列（环形缓冲区，双核共享，自旋锁保护） */
static spin_lock_t *s_event_lock = NULL;
static encoder_event_t s_event_queue[ENCODER_EVENT_QUEUE_SIZE];
static volatile uint8_t s_event_head = 0;  /* 写入位置 */
static volatile uint8_t s_event_tail = 0;  /* 读取位置 */

/* 中键状态（用于模式切换） */
static bool s_switch_pressed = false;
static uint32_t s_switch_press_time = 0;
static bool s_switch_long_press_triggered = false;

/* 自定义模式按键配置 */
static uint8_t s_custom_cw_key = 0x52;   /* 默认：上箭头 */
static uint8_t s_custom_ccw_key = 0x51;  /* 默认：下箭头 */

/* ==================== 内部函数 ==================== */

/* 根据时间间隔计算加速度等级 */
static encoder_accel_level_t calc_accel_level(uint32_t interval_ms)
{
    uint8_t profile = (s_config.accel_profile < 3) ? s_config.accel_profile : 0;
    const uint16_t *thresholds = s_accel_thresholds[profile];

    if (interval_ms > thresholds[0])
        return ENCODER_ACCEL_1X;
    if (interval_ms > thresholds[1])
        return ENCODER_ACCEL_2X;
    if (interval_ms > thresholds[2])
        return ENCODER_ACCEL_5X;
    return ENCODER_ACCEL_10X;
}

/* 入队事件 */
static bool event_queue_push(const encoder_event_t *event)
{
    if (event == NULL) return false;

    uint32_t irq = spin_lock_blocking(s_event_lock);
    uint8_t next_head = (s_event_head + 1) % ENCODER_EVENT_QUEUE_SIZE;

    if (next_head == s_event_tail)
    {
        /* 队列满，丢弃最旧的事件 */
        s_event_tail = (s_event_tail + 1) % ENCODER_EVENT_QUEUE_SIZE;
    }

    memcpy(&s_event_queue[s_event_head], event, sizeof(encoder_event_t));
    s_event_head = next_head;
    spin_unlock(s_event_lock, irq);

    return true;
}

/* 出队事件 */
static bool event_queue_pop(encoder_event_t *event)
{
    if (event == NULL) return false;

    uint32_t irq = spin_lock_blocking(s_event_lock);
    if (s_event_head == s_event_tail)
    {
        spin_unlock(s_event_lock, irq);
        return false;  /* 队列空 */
    }

    memcpy(event, &s_event_queue[s_event_tail], sizeof(encoder_event_t));
    s_event_tail = (s_event_tail + 1) % ENCODER_EVENT_QUEUE_SIZE;
    spin_unlock(s_event_lock, irq);

    return true;
}

/* ==================== 接口实现 ==================== */

void encoder_processor_init(void)
{
    /* 初始化自旋锁 */
    if (s_event_lock == NULL)
    {
        s_event_lock = spin_lock_init(next_striped_spin_lock_num());
    }

    s_current_mode = ENCODER_MODE_SCROLL;
    s_last_event_time = 0;
    s_current_accel = ENCODER_ACCEL_1X;
    s_event_head = 0;
    s_event_tail = 0;
    s_switch_pressed = false;
    s_switch_press_time = 0;
    s_switch_long_press_triggered = false;

    fault_record(FAULT_LEVEL_INFO, "enc_proc", "init complete");
}

void encoder_processor_set_config(const encoder_config_t *config)
{
    if (config == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "enc_proc", "set_config null");
        return;
    }
    s_config = *config;

    /* 合法性检查 */
    if (s_config.steps_per_tick == 0) s_config.steps_per_tick = 1;
    if (s_config.scroll_speed == 0) s_config.scroll_speed = 1;
}

encoder_config_t encoder_processor_get_config(void)
{
    return s_config;
}

void encoder_processor_input(encoder_dir_t dir, uint32_t timestamp_ms)
{
    if (dir == ENCODER_DIR_NONE) return;

    /* 应用方向反转 */
    encoder_dir_t actual_dir = dir;
    if (s_config.reverse)
    {
        actual_dir = (dir == ENCODER_DIR_CW) ? ENCODER_DIR_CCW : ENCODER_DIR_CW;
    }

    /* 计算加速度 */
    encoder_accel_level_t accel = ENCODER_ACCEL_1X;
    if (s_config.accel_enabled && s_last_event_time > 0)
    {
        uint32_t interval = timestamp_ms - s_last_event_time;
        accel = calc_accel_level(interval);
    }
    s_current_accel = accel;
    s_last_event_time = timestamp_ms;

    /* 构建事件并入队 */
    encoder_event_t event;
    event.direction = actual_dir;
    event.accel = accel;
    event.multiplier = s_accel_multiplier[accel];
    event.mode = s_current_mode;

    event_queue_push(&event);
}

bool encoder_processor_get_event(encoder_event_t *event)
{
    return event_queue_pop(event);
}

encoder_mode_t encoder_processor_get_mode(void)
{
    return s_current_mode;
}

void encoder_processor_set_mode(encoder_mode_t mode)
{
    if (mode >= ENCODER_MODE_COUNT) return;
    s_current_mode = mode;
    fault_record(FAULT_LEVEL_INFO, "enc_proc", "mode changed");
}

encoder_mode_t encoder_processor_cycle_mode(void)
{
    s_current_mode = (encoder_mode_t)((s_current_mode + 1) % ENCODER_MODE_COUNT);
    return s_current_mode;
}

void encoder_processor_input_switch(bool pressed, uint32_t timestamp_ms)
{
    if (pressed && !s_switch_pressed)
    {
        /* 按下 */
        s_switch_pressed = true;
        s_switch_press_time = timestamp_ms;
        s_switch_long_press_triggered = false;
    }
    else if (!pressed && s_switch_pressed)
    {
        /* 释放 */
        uint32_t press_duration = timestamp_ms - s_switch_press_time;
        s_switch_pressed = false;

        /* 短按切换模式，长按不处理（已在按下时触发） */
        if (press_duration < MODE_SWITCH_LONG_PRESS_MS && !s_switch_long_press_triggered)
        {
            encoder_mode_t new_mode = encoder_processor_cycle_mode();
            fault_record(FAULT_LEVEL_INFO, "enc_proc", "mode switched by short press");
        }
    }

    /* 长按检测（在按下期间持续检测） */
    if (s_switch_pressed && !s_switch_long_press_triggered)
    {
        uint32_t press_duration = timestamp_ms - s_switch_press_time;
        if (press_duration >= MODE_SWITCH_LONG_PRESS_MS)
        {
            s_switch_long_press_triggered = true;
            /* 长按可以触发其他功能，这里预留 */
            fault_record(FAULT_LEVEL_INFO, "enc_proc", "long press detected");
        }
    }
}

uint8_t encoder_processor_get_accel_multiplier(encoder_accel_level_t level)
{
    if (level >= ENCODER_ACCEL_LEVELS) return 1;
    return s_accel_multiplier[level];
}

void encoder_processor_reset_accel(void)
{
    s_last_event_time = 0;
    s_current_accel = ENCODER_ACCEL_1X;
}

void encoder_processor_set_accel_profile(uint8_t profile)
{
    if (profile >= 3) profile = 0;
    s_config.accel_profile = profile;
}

uint8_t encoder_processor_get_accel_profile(void)
{
    return s_config.accel_profile;
}

void encoder_processor_set_custom_keys(uint8_t cw_key, uint8_t ccw_key)
{
    s_custom_cw_key = cw_key;
    s_custom_ccw_key = ccw_key;
}

void encoder_processor_get_custom_keys(uint8_t *cw_key, uint8_t *ccw_key)
{
    if (cw_key) *cw_key = s_custom_cw_key;
    if (ccw_key) *ccw_key = s_custom_ccw_key;
}
