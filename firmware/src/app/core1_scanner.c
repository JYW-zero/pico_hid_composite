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
#include "hardware/structs/scb.h"
#include "device/dfu/dfu_metadata.h"
#include "device/keypad_spi.h"
#include "device/optical_sensor.h"
#include "device/encoder.h"
#include "device/joystick.h"
#include "middleware/joystick_processor.h"
#include "middleware/encoder_processor.h"
#include "board/board.h"
#include "board/config.h"
#include "pico/time.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ==================== 常量定义 ==================== */
/* Core1 暂停命令超时（微秒）
 * 防止 Core0 崩溃后 Core1 永久挂起，超时后自动恢复扫描
 * 5秒足够覆盖 Flash 擦除（~50ms）和固件写入等长操作
 */
#define CORE1_PAUSE_TIMEOUT_US  (5000000U)
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

/* 非阻塞发送 ACK（Core0 不读 ACK 时不阻塞 Core1，失败则记录错误） */
static void ipc_try_push_ack(uint32_t ack)
{
    if (!multicore_fifo_push_timeout_us(ack, 0))
    {
        /* FIFO 满，Core0 可能未及时读取 ACK，记录错误但不阻塞 */
        fault_record(FAULT_LEVEL_WARN, "core1_ipc", "ACK push timeout, FIFO full");
    }
}

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
    static uint32_t s_call_count = 0;
    if (s_call_count < 3) {
        log_printf("[C1] keypad_task #%lu\n", (unsigned long)s_call_count);
        s_call_count++;
    }
    perf_start(10);

    /* 检查去抖阈值是否已更新（Core0 设置） */
    if (shared_hw_check_and_clear_debounce_updated())
    {
        uint8_t new_threshold = shared_hw_get_keypad_debounce_threshold();
        debounce_64key_set_threshold(&s_keypad_debounce, new_threshold);
        log_printf("[CORE1] 键盘去抖阈值更新为: %d (约%dms)\n", new_threshold, new_threshold * 5);
    }

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
    static uint32_t s_call_count = 0;
    if (s_call_count < 3) {
        log_printf("[C1] optical_task #%lu\n", (unsigned long)s_call_count);
        s_call_count++;
    }
    /* 连续错误计数器，超过阈值尝试重新初始化 */
    static uint8_t s_consecutive_errors = 0;
#define OPTICAL_SENSOR_MAX_CONSECUTIVE_ERRORS  50u  /* 50次×2ms = 100ms */
#define OPTICAL_SENSOR_REINIT_MIN_INTERVAL_MS   5000u  /* 重新初始化最小间隔5秒，防止频繁初始化阻塞任务 */
static uint32_t s_last_reinit_ms = 0;

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
            /* 连续错误过多，尝试重新初始化传感器（限频：距上次初始化不足5秒则跳过，避免频繁初始化阻塞任务） */
            uint32_t now_ms = to_ms_since_boot(get_absolute_time());
            if (now_ms - s_last_reinit_ms >= OPTICAL_SENSOR_REINIT_MIN_INTERVAL_MS)
            {
                fault_record(FAULT_LEVEL_WARN, "optical_sensor", "consecutive errors, re-init");
                (void)optical_sensor_init(s_optical_sensor_cfg);
                s_last_reinit_ms = now_ms;
            }
            else
            {
                fault_record(FAULT_LEVEL_WARN, "optical_sensor", "re-init throttled, too frequent");
            }
            s_consecutive_errors = 0;
        }
    }
    
    /* 喂 DEVICE 层看门狗（主循环已更新 heartbeat，这里只喂看门狗） */
    watchdog_feed_layer(WDG_LAYER_DEVICE);
    
    perf_end(9);
}

/* 编码器扫描任务：1ms */
static void core1_encoder_task(void)
{
    static uint32_t s_call_count = 0;
    if (s_call_count < 3) {
        log_printf("[C1] encoder_task #%lu\n", (unsigned long)s_call_count);
        s_call_count++;
    }
    perf_start(8);
    shared_hw_inc_encoder_scan();

    /* 读取原始旋转方向 */
    encoder_dir_e dir = encoder_update(s_encoder_cfg, &s_encoder_state);

    /* 获取当前时间戳（毫秒） */
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    /* 输入原始事件到编码器处理器（带时间戳，用于加速度计算） */
    if (dir != ENCODER_DIR_NONE)
    {
        encoder_processor_input((encoder_dir_t)dir, now_ms);
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

            /* 中键状态变化时输入到编码器处理器（用于模式切换） */
            encoder_processor_input_switch(s_sw_stable, now_ms);
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
    static uint32_t s_call_count = 0;
    if (s_call_count < 3) {
        log_printf("[C1] joystick_task #%lu\n", (unsigned long)s_call_count);
        s_call_count++;
    }
    perf_start(11);
    joystick_data_t data;
    if (joystick_read(s_joystick_cfg, &data) != 0)
    {
        shared_hw_inc_error();
        perf_end(11);
        return;
    }
    shared_hw_inc_joystick_read();

    /* 检查是否有校准触发（Core0 通过 HID 命令触发） */
    uint16_t calib_samples = 0;
    if (shared_hw_check_and_clear_joystick_calib_trigger(&calib_samples))
    {
        if (joystick_processor_start_calibration(calib_samples))
        {
            shared_hw_set_joystick_calibration_state(1);  /* 校准中 */
            log_printf("[CORE1] 摇杆校准开始，采样次数: %d\n", calib_samples);
        }
    }

    /* 如果正在进行校准，输入采样值 */
    if (joystick_processor_get_calibration_state() == JOYSTICK_CALIB_CENTERING)
    {
        joystick_processor_calibration_sample(data.x, data.y);
    }
    else if (joystick_processor_get_calibration_state() == JOYSTICK_CALIB_DONE)
    {
        /* 校准完成，更新状态并将校准数据写入共享数据（供 Core0 保存到配置） */
        shared_hw_set_joystick_calibration_state(2);  /* 完成 */
        joystick_calibration_t calib = joystick_processor_get_calibration();
        shared_hw_set_joystick_calibration_data(calib.center_x, calib.center_y,
                                                  calib.calibrated ? 1 : 0);
        shared_hw_notify_joystick_calib_updated();  /* 通知 Core0 保存到配置 */
        log_printf("[CORE1] 摇杆校准完成: center_x=%d, center_y=%d\n", calib.center_x, calib.center_y);
        /* 重置状态为空闲，允许下次校准 */
        /* 注意：这里不重置 joystick_processor 的状态，因为校准数据需要保留 */
    }

    /* 从配置提取摇杆参数 */
    const device_config_t* cfg = config_get();
    joystick_config_t joy_cfg;
    joy_cfg.deadzone = cfg->joystick_deadzone;
    joy_cfg.sensitivity = cfg->joystick_sensitivity;
    joy_cfg.invert_x = cfg->joystick_invert_x;
    joy_cfg.invert_y = cfg->joystick_invert_y;
    joy_cfg.curve_type = cfg->reserved2[0];  /* 使用保留字段存储曲线类型 */
    joystick_processor_set_config(&joy_cfg);

    /* 处理摇杆信号 */
    joystick_result_t result;
    joystick_processor_process(data.x, data.y, &result);

    int16_t joy_x = result.x;
    int16_t joy_y = result.y;

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

/* ==================== 主入口子函数 ==================== */

/* Core1 硬件扫描初始化（硬件配置、消抖、处理器、调度器、任务注册） */
static void core1_scanner_init(void)
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

    /* 初始化摇杆信号处理器 */
    joystick_processor_init();

    /* 初始化编码器处理器 */
    encoder_processor_init();

    /* 从配置加载编码器模式（持久化） */
    uint8_t saved_mode = config_get()->reserved3[0];
    if (saved_mode < ENCODER_MODE_COUNT) {
        log_printf("[C1] before set_mode\n");
        encoder_processor_set_mode((encoder_mode_t)saved_mode);
        log_printf("[C1] after set_mode, mode=%d\n", saved_mode);
    }

    /* 从共享数据加载校准数据（Core0 启动时从配置写入） */
    uint16_t center_x = 2048, center_y = 2048;
    uint8_t calibrated = 0;
    shared_hw_get_joystick_calibration_data(&center_x, &center_y, &calibrated);
    if (calibrated) {
        joystick_calibration_t calib;
        calib.center_x = center_x;
        calib.center_y = center_y;
        calib.calibrated = true;
        joystick_processor_set_calibration(&calib);
        log_printf("[CORE1] 摇杆校准数据已加载: center_x=%d, center_y=%d\n", center_x, center_y);
    }

    /* 初始化调度器 */
    sched_init();

    /* 注册 Core1 性能监控任务（索引8-11，与Core0的0-3分开） */
    perf_register_task(8, "c1_encoder");
    perf_register_task(9, "c1_optical_sensor");
    perf_register_task(10, "c1_keypad");
    perf_register_task(11, "c1_joystick");
    perf_set_threshold(8, 200);    /* encoder: 0.2ms */
    perf_set_threshold(9, 500);    /* optical_sensor: 0.5ms */
    perf_set_threshold(10, 2000);  /* keypad: 2ms (SPI 64键读取，含总线竞争余量) */
    perf_set_threshold(11, 500);   /* joystick: 0.5ms */

    /* 配置任务列表 */
    int idx = 0;
    s_core1_tasks[idx].interval_us = 1000;  /* 编码器扫描：1ms，高优先级 */
    s_core1_tasks[idx].last_run_us = 0;
    s_core1_tasks[idx].priority = 64;
    s_core1_tasks[idx].task_func = core1_encoder_task;
    idx++;
    s_core1_tasks[idx].interval_us = 2000;  /* OPTICAL_SENSOR 传感器读取：2ms，高优先级 */
    s_core1_tasks[idx].last_run_us = 0;
    s_core1_tasks[idx].priority = 64;
    s_core1_tasks[idx].task_func = core1_optical_sensor_task;
    idx++;
    s_core1_tasks[idx].interval_us = 5000;  /* 键盘扫描：5ms，普通优先级 */
    s_core1_tasks[idx].last_run_us = 0;
    s_core1_tasks[idx].priority = 128;
    s_core1_tasks[idx].task_func = core1_keypad_task;
    idx++;
    s_core1_tasks[idx].interval_us = 10000; /* 摇杆读取：10ms，普通优先级 */
    s_core1_tasks[idx].last_run_us = 0;
    s_core1_tasks[idx].priority = 128;
    s_core1_tasks[idx].task_func = core1_joystick_task;
    idx++;
}

/* 处理 Core1 FIFO 命令（核间通信） */
static void core1_handle_fifo_cmd(uint32_t cmd)
{
    uint8_t type = IPC_GET_TYPE(cmd);
    uint32_t param = IPC_GET_PARAM(cmd);

    switch (type) {
        case IPC_CMD_NOP:
            ipc_try_push_ack(IPC_ACK_OK);
            break;
        case IPC_CMD_SET_DPI:
            optical_sensor_set_dpi(s_optical_sensor_cfg, (optical_sensor_dpi_e)param);
            ipc_try_push_ack(IPC_ACK_OK);
            break;
        case IPC_CMD_SET_DPI_RAW:
            optical_sensor_set_dpi_raw(s_optical_sensor_cfg, (uint16_t)param);
            ipc_try_push_ack(IPC_ACK_OK);
            break;
        case IPC_CMD_SLEEP:
            ipc_try_push_ack(IPC_ACK_OK);
            __dmb();
            __sev();
            __wfe();
            __wfe();
            break;
        case IPC_CMD_PAUSE: {
            ipc_try_push_ack(IPC_ACK_OK);
            uint32_t pause_start = time_us_32();
            bool timeout = false;
            while (1) {
                /* 等待 FIFO 数据，带超时检查
                 * 使用 WFE 低功耗等待，但每次唤醒后检查超时
                 */
                while (!multicore_fifo_rvalid()) {
                    __wfe();
                    /* 超时检查：5秒后自动恢复，防止 Core0 崩溃后永久挂起 */
                    if ((time_us_32() - pause_start) >= CORE1_PAUSE_TIMEOUT_US) {
                        timeout = true;
                        break;
                    }
                }
                if (timeout) {
                    fault_record(FAULT_LEVEL_WARN, "core1", "pause timeout (5s), auto resume");
                    log_printf("[CORE1] ⚠️ 暂停超时(5秒)，自动恢复扫描\n");
                    break;
                }
                uint32_t pause_cmd = multicore_fifo_pop_blocking();
                uint8_t pause_type = IPC_GET_TYPE(pause_cmd);
                if (pause_type == IPC_CMD_RESUME) {
                    ipc_try_push_ack(IPC_ACK_OK);
                    break;
                } else if (pause_type == IPC_CMD_NOP) {
                    ipc_try_push_ack(IPC_ACK_OK);
                }
                /* 其他命令在暂停状态下忽略，但重置超时计时器（说明 Core0 还活着） */
                pause_start = time_us_32();
            }
            break;
        }
        case IPC_CMD_RESUME:
            ipc_try_push_ack(IPC_ACK_OK);
            break;
        case IPC_CMD_SET_ENCODER_REV:
            s_encoder_reverse_rt = (int32_t)param;
            ipc_try_push_ack(IPC_ACK_OK);
            break;
        case IPC_CMD_SET_JOYSTICK_DZ:
            s_joystick_deadzone_rt = param;
            ipc_try_push_ack(IPC_ACK_OK);
            break;
        case IPC_CMD_PING:
            ipc_try_push_ack(IPC_ACK_OK);
            break;
        default:
            ipc_try_push_ack(IPC_ACK_ERR);
            break;
    }
}

/* ==================== 主入口 ==================== */

void core1_scanner_main(void)
{
    core1_scanner_init();

    /* 主循环 */
    log_printf("[C1] entering main loop\n");
    while (1) {
        /* 每次循环开始时更新 heartbeat，消除单点故障 */
        shared_hw_increment_heartbeat();

        /* 处理 FIFO 命令（核间通信） */
        while (multicore_fifo_rvalid()) {
            uint32_t cmd = multicore_fifo_pop_blocking();
            core1_handle_fifo_cmd(cmd);
        }

        /* 运行调度器 */
        sched_run(s_core1_tasks, CORE1_TASK_COUNT);
    }
}










