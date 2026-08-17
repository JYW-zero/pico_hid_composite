/*
 * src/app/core1_scanner.c
 * Core1 硬件扫描模块实现
 * 负责所有硬件外设的周期性扫描，结果写入共享数据
 */
#include "app/core1_scanner.h"
#include "middleware/shared_hw_data.h"
#include "middleware/scheduler.h"
#include "middleware/debounce.h"
#include "middleware/watchdog.h"
#include "middleware/ipc.h"
#include "middleware/flash_service.h"
#include "middleware/perf_monitor.h"
#include "middleware/fault.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "device/keypad_spi.h"
#include "device/optical_sensor.h"
#include "device/encoder.h"
#include "device/joystick.h"
#include "board/board.h"
#include "board/config.h"
#include "pico/time.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* ==================== 鼠标加速配置（默认禁用） ==================== */
#ifndef MOUSE_ACCEL_ENABLE
#define MOUSE_ACCEL_ENABLE 0     /* 0=禁用, 1=启用鼠标指针加速 */
#endif
#ifndef MOUSE_ACCEL_THRESHOLD
#define MOUSE_ACCEL_THRESHOLD 10.0f  /* 加速阈值（移动速度） */
#endif
#ifndef MOUSE_ACCEL_GAIN
#define MOUSE_ACCEL_GAIN 0.01f       /* 加速增益系数 */
#endif

/* ==================== 私有变量 ==================== */

/* 硬件配置句柄（只读，全局共享） */
static const keypad_spi_cfg_t *s_keypad_cfg = NULL;
static const optical_sensor_cfg_t *s_optical_sensor_cfg = NULL;
static const encoder_cfg_t *s_encoder_cfg = NULL;
static const joystick_cfg_t *s_joystick_cfg = NULL;

/* 状态变量（Core1 私有） */
static debounce_64key_t s_keypad_debounce;
static encoder_state_t s_encoder_state;

/* 实时配置（不存 Flash，通过 FIFO 命令设置）
 * -1 / 0xFFFF 表示使用 Flash 配置值
 */
static int32_t s_encoder_reverse_rt = -1;
static uint32_t s_joystick_deadzone_rt = 0xFFFFFFFFUL;

/* 调度器任务列表 */
#define CORE1_TASK_COUNT 4
static sched_task_t s_core1_tasks[CORE1_TASK_COUNT];

/* ==================== 任务函数 ==================== */

/* 键盘扫描任务：5ms */
static void core1_keypad_task(void)
{
    perf_start(10);
    uint64_t raw_keys = 0;
    if (keypad_spi_read_u64(s_keypad_cfg, &raw_keys) == 0)
    {
        /* 恢复消抖，阈值降到2 */
        uint64_t stable_keys = debounce_64key_update(&s_keypad_debounce, raw_keys);
        shared_hw_set_keys(stable_keys);
        shared_hw_inc_keypad_scan();
    }
    else
    {
        shared_hw_inc_error();
    }
    perf_end(10);
}

/* OPTICAL_SENSOR 传感器读取任务：2ms */
static void core1_optical_sensor_task(void)
{
    /* 连续错误计数器，超过阈值尝试重新初始化 */
    static uint8_t s_consecutive_errors = 0;
#define OPTICAL_SENSOR_MAX_CONSECUTIVE_ERRORS  50u  /* 50次×2ms = 100ms */

    perf_start(9);
    optical_sensor_motion_t motion;
    if (optical_sensor_read_motion(s_optical_sensor_cfg, &motion) == 0)
    {
        s_consecutive_errors = 0;  /* 成功则重置错误计数 */
        shared_hw_inc_optical_sensor_read();

        if (motion.has_motion)
        {
            int32_t dx = motion.dx;
            int32_t dy = motion.dy;

#if MOUSE_ACCEL_ENABLE
            /* 鼠标指针加速：阈值+线性加速 */
            float speed = sqrtf((float)(dx*dx + dy*dy));
            if (speed > MOUSE_ACCEL_THRESHOLD)
            {
                float coeff = 1.0f + (speed - MOUSE_ACCEL_THRESHOLD) * MOUSE_ACCEL_GAIN;
                if (coeff > MOUSE_ACCEL_MAX)
                {
                    coeff = MOUSE_ACCEL_MAX;
                }
                dx = (int32_t)((float)dx * coeff);
                dy = (int32_t)((float)dy * coeff);
            }
#endif

            shared_hw_add_motion(dx, dy);
        }
    }
    else
    {
        shared_hw_inc_error();
        s_consecutive_errors++;
        if (s_consecutive_errors >= OPTICAL_SENSOR_MAX_CONSECUTIVE_ERRORS)
        {
            /* 连续错误过多，尝试重新初始化传感器 */
            fault_record(FAULT_LEVEL_WARN, "optical_sensor", "consecutive errors, re-init");
            (void)optical_sensor_init(s_optical_sensor_cfg);
            s_consecutive_errors = 0;
        }
    }
    
    /* 喂 DEVICE 层看门狗 */
    watchdog_feed_layer(WDG_LAYER_DEVICE);
    
    /* 递增心跳计数器，供 Core0 监控 */
    shared_hw_increment_heartbeat();
    perf_end(9);
}

/* 编码器扫描任务：1ms */
static void core1_encoder_task(void)
{
    perf_start(8);
    shared_hw_inc_encoder_scan();
    encoder_dir_e dir = encoder_update(s_encoder_cfg, &s_encoder_state);
    
    /* 根据配置反转方向（优先用实时设置） */
    bool reverse;
    if (s_encoder_reverse_rt >= 0)
    {
        reverse = (s_encoder_reverse_rt != 0);
    }
    else
    {
        reverse = (config_get()->encoder_reverse != 0);
    }
    int32_t delta = 0;
    
    if (dir == ENCODER_DIR_CW)
    {
        delta = reverse ? -1 : 1;
    }
    else if (dir == ENCODER_DIR_CCW)
    {
        delta = reverse ? 1 : -1;
    }
    
    if (delta != 0)
    {
        /* 应用步长和滚动速度 */
        const device_config_t* enc_cfg = config_get();
        uint8_t steps = enc_cfg->encoder_steps;
        uint8_t speed = enc_cfg->encoder_scroll_speed;
        if (steps == 0) steps = 1;
        if (speed == 0) speed = 1;
        int32_t total_delta = delta * (int32_t)steps * (int32_t)speed;
        shared_hw_add_wheel(total_delta);
    }
    
    /* 中键状态（带消抖：连续3次一致才确认） */
    static uint8_t s_sw_debounce_count = 0;
    static bool s_sw_stable = false;
#define ENCODER_SW_DEBOUNCE_THRESHOLD  3u

    bool sw_raw = encoder_read_switch(s_encoder_cfg);
    if (sw_raw == s_sw_stable)
    {
        s_sw_debounce_count = 0;
    }
    else
    {
        s_sw_debounce_count++;
        if (s_sw_debounce_count >= ENCODER_SW_DEBOUNCE_THRESHOLD)
        {
            s_sw_stable = sw_raw;
            s_sw_debounce_count = 0;
        }
    }
    /* 中键状态（OR合并：只设置/清除中键位，不影响其他来源的按钮） */
    if (s_sw_stable)
    {
        shared_hw_set_mouse_buttons(0x04);  /* 中键按下：设置 bit2 */
    }
    else
    {
        shared_hw_clear_mouse_buttons(0x04);  /* 中键释放：清除 bit2 */
    }
    perf_end(8);
}

/* 摇杆读取任务：10ms */
static void core1_joystick_task(void)
{
    perf_start(11);
    joystick_data_t data;
    if (joystick_read(s_joystick_cfg, &data) != 0)
    {
        shared_hw_inc_error();
        perf_end(11);
        return;
    }
    shared_hw_inc_joystick_read();

    /* ADC 值 0~4095 转换为 -2048~2047（中心值2048） */
    int32_t x = (int32_t)data.x - 2048;
    int32_t y = (int32_t)data.y - 2048;

    /* 读取死区（优先用实时设置） */
    uint16_t deadzone;
    if (s_joystick_deadzone_rt != 0xFFFFFFFFUL)
    {
        deadzone = (uint16_t)s_joystick_deadzone_rt;
    }
    else
    {
        deadzone = config_get()->joystick_deadzone;
    }
    int32_t range = 2048 - (int32_t)deadzone;
    if (range < 100)
    {
        range = 100;
    }

    /* 死区处理 */
    if (x > -(int32_t)deadzone && x < (int32_t)deadzone)
    {
        x = 0;
    }
    if (y > -(int32_t)deadzone && y < (int32_t)deadzone)
    {
        y = 0;
    }

    /* 缩放至 -127~127 */
    x = (x * 127) / range;
    y = (y * 127) / range;

    /* 应用灵敏度（定点数，1.0=1000） */
    const device_config_t* cfg = config_get();
    uint16_t sens = cfg->joystick_sensitivity;
    if (sens == 0) sens = 1000;  /* 防止除零 */
    x = (x * (int32_t)sens) / 1000;
    y = (y * (int32_t)sens) / 1000;

    /* 限制范围 */
    if (x > 127) x = 127;
    if (x < -127) x = -127;
    if (y > 127) y = 127;
    if (y < -127) y = -127;

    /* X/Y轴反转配置 */
    if (cfg->joystick_invert_x) x = -x;
    if (cfg->joystick_invert_y) y = -y;  /* 默认Y轴反转（物理方向与HID相反），可通过配置取消 */

    int16_t joy_x = (int16_t)x;
    int16_t joy_y = (int16_t)y;

    /* 摇杆按键消抖：连续3次一致才确认（10ms任务周期=30ms消抖） */
    static uint8_t s_joy_btn_debounce_count = 0;
    static bool s_joy_btn_stable = false;
#define JOYSTICK_BTN_DEBOUNCE_THRESHOLD  3u

    if (data.btn_pressed == s_joy_btn_stable)
    {
        s_joy_btn_debounce_count = 0;
    }
    else
    {
        s_joy_btn_debounce_count++;
        if (s_joy_btn_debounce_count >= JOYSTICK_BTN_DEBOUNCE_THRESHOLD)
        {
            s_joy_btn_stable = data.btn_pressed;
            s_joy_btn_debounce_count = 0;
        }
    }

    shared_hw_set_joystick(joy_x, joy_y, s_joy_btn_stable);
    perf_end(11);
}

/* ==================== 主入口 ==================== */

void core1_scanner_main(void)
{
    /* 注册Core1为可被flash_safe_execute锁定的核心（必须最早调用） */
    flash_service_core1_init();

    /* 获取硬件配置句柄（只读） */
    s_keypad_cfg = board_get_keypad_spi_cfg();
    s_optical_sensor_cfg = board_get_optical_sensor_cfg();
    s_encoder_cfg = board_get_encoder_cfg();
    s_joystick_cfg = board_get_joystick_cfg();
    
    /* 初始化消抖 */
    debounce_64key_init(&s_keypad_debounce, 2);
    
    /* 初始化编码器状态 */
    encoder_state_init(&s_encoder_state);
    
    /* 初始化调度器 */
    sched_init();

    /* 注册 Core1 性能监控任务（索引8-11，与Core0的0-3分开） */
    perf_register_task(8, "c1_encoder");
    perf_register_task(9, "c1_optical_sensor");
    perf_register_task(10, "c1_keypad");
    perf_register_task(11, "c1_joystick");
    perf_set_threshold(8, 200);    /* encoder: 0.2ms */
    perf_set_threshold(9, 500);    /* optical_sensor: 0.5ms */
    perf_set_threshold(10, 1000);  /* keypad: 1ms */
    perf_set_threshold(11, 500);   /* joystick: 0.5ms */
    
    /* 配置任务列表 */
    int idx = 0;
    
    /* 编码器扫描：1ms，高优先级 */
    s_core1_tasks[idx].interval_us = 1000;
    s_core1_tasks[idx].last_run_us = 0;
    s_core1_tasks[idx].priority = 64;
    s_core1_tasks[idx].task_func = core1_encoder_task;
    idx++;
    
    /* OPTICAL_SENSOR 传感器读取：2ms，高优先级 */
    s_core1_tasks[idx].interval_us = 2000;
    s_core1_tasks[idx].last_run_us = 0;
    s_core1_tasks[idx].priority = 64;
    s_core1_tasks[idx].task_func = core1_optical_sensor_task;
    idx++;
    
    /* 键盘扫描：5ms，普通优先级 */
    s_core1_tasks[idx].interval_us = 5000;
    s_core1_tasks[idx].last_run_us = 0;
    s_core1_tasks[idx].priority = 128;
    s_core1_tasks[idx].task_func = core1_keypad_task;
    idx++;
    
    /* 摇杆读取：10ms，普通优先级 */
    s_core1_tasks[idx].interval_us = 10000;
    s_core1_tasks[idx].last_run_us = 0;
    s_core1_tasks[idx].priority = 128;
    s_core1_tasks[idx].task_func = core1_joystick_task;
    idx++;
    
    /* 主循环 */
    while (1)
    {
        /* 处理 FIFO 命令（核间通信） */
        while (multicore_fifo_rvalid())
        {
            uint32_t cmd = multicore_fifo_pop_blocking();
            uint8_t type = IPC_GET_TYPE(cmd);
            uint32_t param = IPC_GET_PARAM(cmd);

            switch (type)
            {
                case IPC_CMD_NOP:
                    /* 空操作，回ACK */
                    multicore_fifo_push_blocking(IPC_ACK_OK);
                    break;

                case IPC_CMD_SET_DPI:
                    /* 设置 DPI */
                    optical_sensor_set_dpi(s_optical_sensor_cfg, (optical_sensor_dpi_e)param);
                    multicore_fifo_push_blocking(IPC_ACK_OK);
                    break;

                case IPC_CMD_SLEEP:
                    /* 进入休眠命令：先回ACK，然后进入WFE等待事件唤醒 */
                    multicore_fifo_push_blocking(IPC_ACK_OK);
                    __dmb();  /* 数据同步屏障 */
                    /* 清除事件标志，确保真正进入休眠 */
                    __sev();
                    __wfe();
                    __wfe();
                    break;

                case IPC_CMD_PAUSE:
                    /* 暂停扫描：回ACK，然后进入等待循环，直到收到RESUME */
                    multicore_fifo_push_blocking(IPC_ACK_OK);
                    while (1)
                    {
                        /* 等待 FIFO 数据 */
                        while (!multicore_fifo_rvalid())
                        {
                            __wfe();  /* 没事做就进入低功耗等待 */
                        }
                        uint32_t pause_cmd = multicore_fifo_pop_blocking();
                        uint8_t pause_type = IPC_GET_TYPE(pause_cmd);
                        
                        if (pause_type == IPC_CMD_RESUME)
                        {
                            multicore_fifo_push_blocking(IPC_ACK_OK);
                            break;  /* 退出暂停循环 */
                        }
                        else if (pause_type == IPC_CMD_NOP)
                        {
                            multicore_fifo_push_blocking(IPC_ACK_OK);
                        }
                        /* 其他命令在暂停状态下忽略 */
                    }
                    break;

                case IPC_CMD_RESUME:
                    /* 正常状态下收到RESUME，直接回ACK */
                    multicore_fifo_push_blocking(IPC_ACK_OK);
                    break;

                case IPC_CMD_SET_ENCODER_REV:
                    /* 设置编码器方向（实时，不存Flash） */
                    s_encoder_reverse_rt = (int32_t)param;
                    multicore_fifo_push_blocking(IPC_ACK_OK);
                    break;

                case IPC_CMD_SET_JOYSTICK_DZ:
                    /* 设置摇杆死区（实时，不存Flash） */
                    s_joystick_deadzone_rt = param;
                    multicore_fifo_push_blocking(IPC_ACK_OK);
                    break;

                case IPC_CMD_PING:
                    /* 测试命令，回ACK */
                    multicore_fifo_push_blocking(IPC_ACK_OK);
                    break;

                default:
                    /* 未知命令，回错误 */
                    multicore_fifo_push_blocking(IPC_ACK_ERR);
                    break;
            }
        }

        /* 运行调度器 */
        sched_run(s_core1_tasks, CORE1_TASK_COUNT);
    }
}










