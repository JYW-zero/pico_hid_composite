/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "features.h"
#include "app/log.h"
#include "app/usb_callbacks.h"
#include "app/keyboard_processor.h"
#include "app/encoder_report_queue.h"
#include "app/led_manager.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "hardware/structs/scb.h"
#include "bsp/board_api.h"
#define LED_PIN 25
#include "tusb.h"
#include "usb_descriptors.h"
#include "protocol/config_hid.h"
#include "board/board.h"
#include "board/config.h"
#include "device/keypad_spi.h"
#include "device/optical_sensor.h"
#include "device/encoder.h"
#include "device/joystick.h"
#include "middleware/joystick_processor.h"
#include "middleware/encoder_processor.h"
#include "middleware/debounce.h"
#include "middleware/watchdog.h"
#include "middleware/scheduler.h"
#include "middleware/fault.h"
#include "middleware/perf_monitor.h"
#include "middleware/flash_service.h"
#include "middleware/power_manager.h"
#include "middleware/keyboard_event.h"
#include "app/keymap.h"
#include "app/macro.h"
#include "app/key_stats.h"
#include "app/factory_test.h"
#include "middleware/shared_hw_data.h"
#include "middleware/ipc.h"
#include "app/core1_scanner.h"
#include "pico/multicore.h"
#include "device/dfu/dfu_metadata.h"

/* ==================== 常量定义 ==================== */
/* 调度器任务间隔（微秒）
 * 各任务的执行周期，根据实时性要求和CPU负载平衡设置
 * - 高频任务（1ms）：看门狗、编码器处理、鼠标HID报告
 * - 中频任务（5ms）：键盘扫描
 * - 低频任务（10ms）：摇杆处理、LED闪烁
 */
#define SCHED_INTERVAL_WATCHDOG_US    (1000U)   /* 1ms - 看门狗喂狗 */
#define SCHED_INTERVAL_ENCODER_US     (1000U)   /* 1ms - 编码器处理 */
#define SCHED_INTERVAL_MOUSE_HID_US   (1000U)   /* 1ms - 鼠标HID报告 */
#define SCHED_INTERVAL_KEYPAD_US      (5000U)   /* 5ms - 键盘扫描 */
#define SCHED_INTERVAL_JOYSTICK_US    (10000U)  /* 10ms - 摇杆处理 */
#define SCHED_INTERVAL_LED_BLINK_US   (10000U)  /* 10ms - LED闪烁 */

/* 看门狗初始化超时（毫秒）
 * 逻辑看门狗的超时时间，各层必须在此时间内喂狗
 * 1秒足够覆盖最坏情况下的任务调度延迟
 */
#define WATCHDOG_INIT_TIMEOUT_MS      (1000U)

/* ==================== HardFault 处理 ==================== */
/* Cortex-M33 HardFault 中断处理函数
 *
 * 【重要】不能在 C 函数中直接用内联汇编读 lr：编译器会在函数 prologue 中
 * push lr / 调用子函数，导致 lr 不再是 EXC_RETURN，栈帧解析全错（PC=0）。
 * 正确做法：用 naked 函数做入口，第一时间把 lr (EXC_RETURN) 传给 C 处理函数。
 */
#include "hardware/structs/scb.h"
/* naked 入口：保存 EXC_RETURN 后跳转到 C 处理函数 */
__attribute__((naked)) void isr_hardfault(void)
{
    __asm volatile(
        "mov r0, lr\n"
        "b hardfault_handler_c\n"
    );
}
/* 实际的 HardFault 处理函数，exc_return 是进入异常时的 lr 值 */
void hardfault_handler_c(uint32_t exc_return)
{
    /* 读取故障状态寄存器 */
    uint32_t cfsr = scb_hw->cfsr;   /* 可配置故障状态寄存器 */
    uint32_t hfsr = scb_hw->hfsr;   /* 硬故障状态寄存器 */
    uint32_t mmfar = scb_hw->mmfar;  /* 内存管理故障地址 */
    uint32_t bfar = scb_hw->bfar;    /* 总线故障地址 */
    /* 根据 EXC_RETURN bit2 判断异常发生时使用的是 MSP 还是 PSP
     * bit2=0 -> MSP, bit2=1 -> PSP
     */
    uint32_t sp_value;
    if (exc_return & (1u << 2)) {
        __asm volatile ("mrs %0, psp" : "=r"(sp_value));
    } else {
        __asm volatile ("mrs %0, msp" : "=r"(sp_value));
    }
    /* 从栈帧读取 PC / LR / xPSR
     * 异常入栈布局: [SP+0]=R0, +4=R1, +8=R2, +12=R3,
     *               +16=R12, +20=LR, +24=PC, +28=xPSR
     */
    uint32_t pc_value   = *(volatile uint32_t*)(sp_value + 24);
    uint32_t lr_value   = *(volatile uint32_t*)(sp_value + 20);
    uint32_t xpsr_value = *(volatile uint32_t*)(sp_value + 28);
    /* 直接输出（HardFault 上下文不加锁，避免死锁） */
    printf("\n");
    printf("========================================\n");
    printf("  !!! HardFault 异常 !!!\n");
    printf("========================================\n");
    printf("  核心: Core %d\n", get_core_num());
    printf("  EXC_RETURN: 0x%08lX\n", (unsigned long)exc_return);
    printf("  PC (出错地址): 0x%08lX\n", (unsigned long)pc_value);
    printf("  LR (返回地址): 0x%08lX\n", (unsigned long)lr_value);
    printf("  SP (栈指针):   0x%08lX\n", (unsigned long)sp_value);
    printf("  xPSR:           0x%08lX\n", (unsigned long)xpsr_value);
    printf("  CFSR: 0x%08lX\n", (unsigned long)cfsr);
    printf("  HFSR: 0x%08lX\n", (unsigned long)hfsr);
    printf("  MMFAR: 0x%08lX\n", (unsigned long)mmfar);
    printf("  BFAR: 0x%08lX\n", (unsigned long)bfar);
    /* 解析 CFSR 中的故障（修正位定义） */
    printf("  --- 故障原因解析 ---\n");
    /* MemManage Fault (低字节) */
    if (cfsr & (1u << 0))  printf("  -> IACCVIOL: 指令访问违例\n");
    if (cfsr & (1u << 1))  printf("  -> DACCVIOL: 数据访问违例\n");
    if (cfsr & (1u << 3))  printf("  -> MUNSTKERR: 出栈错误\n");
    if (cfsr & (1u << 4))  printf("  -> MSTKERR: 入栈错误\n");
    if (cfsr & (1u << 5))  printf("  -> MLSPERR: 浮点惰性压栈错误\n");
    if (cfsr & (1u << 7))  printf("  -> MMARVALID: MMFAR 地址有效\n");
    /* BusFault (第二字节) */
    if (cfsr & (1u << 8))  printf("  -> IBUSERR: 指令总线错误\n");
    if (cfsr & (1u << 9))  printf("  -> PRECISERR: 精确数据总线错误 (地址=BFAR)\n");
    if (cfsr & (1u << 10)) printf("  -> IMPRECISERR: 不精确数据总线错误\n");
    if (cfsr & (1u << 11)) printf("  -> UNSTKERR: 出栈总线错误\n");
    if (cfsr & (1u << 12)) printf("  -> STKERR: 入栈总线错误\n");
    if (cfsr & (1u << 15)) printf("  -> BFARVALID: BFAR 地址有效\n");
    /* UsageFault (第三字节) */
    if (cfsr & (1u << 16)) printf("  -> UNDEFINSTR: 未定义指令\n");
    if (cfsr & (1u << 17)) printf("  -> INVSTATE: 无效状态 (Thumb bit错误)\n");
    if (cfsr & (1u << 18)) printf("  -> INVPC: 无效 PC 跳转\n");
    if (cfsr & (1u << 19)) printf("  -> NOCP: 无协处理器\n");
    if (cfsr & (1u << 24)) printf("  -> UNALIGNED: 非对齐访问\n");
    if (cfsr & (1u << 25)) printf("  -> DIVBYZERO: 除零错误\n");
    /* HFSR 解析 */
    if (hfsr & (1u << 30)) printf("  -> FORCED: 可配置故障升级为HardFault\n");
    if (hfsr & (1u << 31)) printf("  -> DEBUGEVT: 调试事件\n");
    if (hfsr & (1u << 1))  printf("  -> VECTTBL: 向量表读取错误\n");
    printf("========================================\n");
    printf("  定位方法: arm-none-eabi-addr2line -e firmware.elf 0x%08lX\n",
           (unsigned long)pc_value);
    printf("========================================\n");
    fflush(stdout);
    /* 记录故障日志（FATAL 级别会触发复位） */
    char fault_msg[128];
    snprintf(fault_msg, sizeof(fault_msg),
             "HardFault Core%d PC=0x%08lX LR=0x%08lX CFSR=0x%08lX",
             get_core_num(), (unsigned long)pc_value,
             (unsigned long)lr_value, (unsigned long)cfsr);
    fault_record(FAULT_LEVEL_FATAL, "hardfault", fault_msg);
    /* fault_record 的 FATAL 会触发 watchdog_reboot，这里再确保 */
    watchdog_reboot(0, 0, 100);
    while (1) { /* 等待复位 */ }
}
/* ==================== 复位原因诊断 ==================== */
#if FEATURE_DEBUG_UART
/* 打印上次复位的原因，帮助诊断不断重启问题 */
static void print_reset_reason(void)
{
    uint32_t reason = watchdog_hw->reason;
    printf("\n");
    printf("=== 复位原因诊断 ===\n");
    printf("  watchdog_hw->reason = 0x%08lX\n", (unsigned long)reason);
    if (watchdog_caused_reboot())
    {
        printf("  -> 看门狗复位 (Watchdog)\n");
        uint32_t scratch = watchdog_hw->scratch[0];
        if (scratch < 3)
        {
            const char* layers[] = {"BOARD", "DEVICE", "APP"};
            printf("  -> 故障层: %s (逻辑看门狗超时)\n", layers[scratch]);
        }
        else if (scratch == 0xFF)
        {
            printf("  -> 手动复位 (Manual)\n");
        }
        else
        {
            printf("  -> 故障层: unknown (硬件看门狗直接超时，主循环可能卡住)\n");
        }
    }
    /* RP2350 复位原因寄存器位定义（参考 datasheet） */
    if (reason & (1u << 0)) printf("  -> 上电复位 (Power-on)\n");
    if (reason & (1u << 1)) printf("  -> Brown-out 复位 (电压过低)\n");
    if (reason & (1u << 2)) printf("  -> 调试器复位 (Debugger)\n");
    if (reason & (1u << 3)) printf("  -> 软件复位 (Software PS_RESTART)\n");
    if (reason & (1u << 4)) printf("  -> 看门狗复位 (Watchdog timer)\n");
    printf("====================\n");
    fflush(stdout);
}
#endif /* FEATURE_DEBUG_UART */
/* 日志宏使用公共模块 app/log.h */
//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
/* BLINK 常量和 blink_interval_ms 已移至 app/usb_callbacks.h/c */
/* ==================== 64键键盘相关 ==================== */
static const keypad_spi_cfg_t *keypad_cfg;
static debounce_64key_t keypad_debounce;
static uint64_t g_stable_keys = 0xFFFFFFFFFFFFFFFFULL;
/* ==================== OPTICAL_SENSOR鼠标传感器相关 ==================== */
static const optical_sensor_cfg_t *optical_sensor_cfg;
static int32_t g_mouse_dx = 0;  /* 累积的X位移（int32防止溢出） */
static int32_t g_mouse_dy = 0;  /* 累积的Y位移（int32防止溢出） */
static uint8_t g_mouse_buttons = 0; /* 鼠标按键位掩码 */
/* ==================== 滚轮编码器相关 ==================== */
static const encoder_cfg_t *encoder_cfg;
static encoder_state_t encoder_state;
static int16_t g_wheel = 0;  /* 累积的滚轮步数（int16防止溢出） */
/* 层切换键状态跟踪（防止每帧重复触发） */
static bool s_layer_switch_was_pressed = false;
/* ==================== PS2摇杆相关 ==================== */
static const joystick_cfg_t *joystick_cfg;
static int16_t g_joy_x = 0; /* 摇杆X轴 (-127 ~ 127) */
static int16_t g_joy_y = 0; /* 摇杆Y轴 (-127 ~ 127) */
static bool g_joy_btn = false; /* 摇杆按键 */
/* ==================== 64键键盘映射表 ====================
 * 索引0~63 对应 物理键1~64
 * 值为HID按键码，0xFF表示修饰键（单独处理）
 * 注：实际键映射存储在 device_config_t.keymap / fn_keymap 中，
 *     由 keymap 模块管理，此处不定义静态映射表。
 */
/* ==================== DPI切换相关 ==================== */
/* DPI切换键：数字3键（索引3） */
#define DPI_SWITCH_KEY_INDEX  3
/* DPI档位列表（实际数值）：400 / 800 / 1600 / 3200 */
static const uint16_t s_dpi_list[] = {400, 800, 1600, 3200};
#define DPI_COUNT (sizeof(s_dpi_list) / sizeof(s_dpi_list[0]))
/* ==================== 组合键恢复出厂设置 ==================== */
/* 组合键：Fn + Esc 长按 3 秒触发恢复出厂设置
 * Esc 键索引 = 0，Fn 键索引由 keymap_get_fn_key() 返回（默认 59）
 * 低电平有效（74HC165 读取，按下时 bit=0）
 */
#define FACTORY_RESET_COMBO_HOLD_MS  3000  /* 长按 3 秒 */
#define FACTORY_RESET_ESC_KEY_INDEX   0     /* Esc 键索引 */
static uint32_t s_factory_reset_start_ms = 0;  /* 组合键开始按下的时间 */
static bool     s_factory_reset_triggered = false; /* 是否已触发（防止重复触发） */
/* 检测组合键并处理恢复出厂设置
 * 返回 true 表示已触发恢复出厂设置（主循环应立即停止其他操作）
 */
static bool check_factory_reset_combo(void)
{
    if (s_factory_reset_triggered) return true;
    uint64_t keys = shared_hw_get_keys();
    uint8_t fn_key = keymap_get_fn_key();
    /* 低电平有效：bit=0 表示按下 */
    bool fn_pressed = ((keys >> fn_key) & 1ULL) == 0ULL;
    bool esc_pressed = ((keys >> FACTORY_RESET_ESC_KEY_INDEX) & 1ULL) == 0ULL;
    bool combo_pressed = fn_pressed && esc_pressed;
    uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    if (combo_pressed)
    {
        if (s_factory_reset_start_ms == 0)
        {
            /* 组合键刚按下，记录开始时间 */
            s_factory_reset_start_ms = now_ms;
            printf("[FACTORY_RESET] 检测到 Fn+Esc 组合键，长按 %d 秒恢复出厂设置...\n",
                   FACTORY_RESET_COMBO_HOLD_MS / 1000);
        }
        else
        {
            /* 持续按下，检查是否达到阈值 */
            uint32_t elapsed = now_ms - s_factory_reset_start_ms;
            if (elapsed >= FACTORY_RESET_COMBO_HOLD_MS)
            {
                /* 触发恢复出厂设置 */
                s_factory_reset_triggered = true;
                printf("[FACTORY_RESET] 组合键长按达到 %d 秒，触发恢复出厂设置！\n",
                       FACTORY_RESET_COMBO_HOLD_MS / 1000);
                fault_record(FAULT_LEVEL_WARN, "config", "factory reset triggered by key combo (Fn+Esc)");
                /* 恢复出厂默认配置（config_reset_default 内部已同步完成 Flash 写入） */
                config_reset_default();
                /* 短暂延迟 20ms 确保 Flash 操作完全稳定，然后重启（原 100ms 过长，阻塞主循环） */
                uint32_t reset_start = to_ms_since_boot(get_absolute_time());
                while ((to_ms_since_boot(get_absolute_time()) - reset_start) < 20)
                {
                    watchdog_update();
                }
                printf("[FACTORY_RESET] 重启设备...\n");
                watchdog_reboot(0, 0, 1);
                while (1) {};
                return true;
            }
        }
    }
    else
    {
        /* 组合键释放，重置计时 */
        if (s_factory_reset_start_ms != 0)
        {
            uint32_t elapsed = now_ms - s_factory_reset_start_ms;
            printf("[FACTORY_RESET] 组合键释放（按下 %lu ms，未达到阈值）\n", (unsigned long)elapsed);
            s_factory_reset_start_ms = 0;
        }
    }
    return false;
}
/* DPI数值转枚举 */
static optical_sensor_dpi_e dpi_val_to_enum(uint16_t dpi_val)
{
    switch (dpi_val)
    {
        case 400:  return optical_sensor_DPI_400;
        case 800:  return optical_sensor_DPI_800;
        case 1600: return optical_sensor_DPI_1600;
        case 3200: return optical_sensor_DPI_3200;
        default:   return optical_sensor_DPI_1600; /* 默认1600 */
    }
}
/* DPI 切换 IPC 超时时间（微秒）
 * Core1 主循环周期 1ms，DPI 切换（SPI 写寄存器）通常 <1ms，
 * 设 50ms 超时留足余量，避免 Core1 异常时永久阻塞 */
#define DPI_IPC_TIMEOUT_US  50000U
/* 切换到下一个DPI档位 */
static void dpi_cycle_next(void)
{
    uint16_t current_dpi = config_get()->dpi;
    /* 找到当前DPI在列表中的位置 */
    uint8_t idx = 0;
    for (uint8_t i = 0; i < DPI_COUNT; i++)
    {
        if (s_dpi_list[i] == current_dpi)
        {
            idx = i;
            break;
        }
    }
    /* 切换到下一个档位 */
    idx = (idx + 1) % DPI_COUNT;
    uint16_t new_dpi_val = s_dpi_list[idx];
    optical_sensor_dpi_e new_dpi_enum = dpi_val_to_enum(new_dpi_val);
    /* 通过IPC发送给Core1执行DPI切换，避免Core0直接操作SPI1与Core1竞争
     *
     * 使用非阻塞发送 + 超时等待ACK，防止Core1异常（死锁/崩溃）时
     * Core0 永久阻塞在 multicore_fifo_pop_blocking() 导致看门狗复位。
     */
    uint32_t cmd = IPC_MAKE_CMD(IPC_CMD_SET_DPI, (uint32_t)new_dpi_enum);
    /* 非阻塞发送：使用超时 0 的 push_timeout，FIFO 满时立即失败 */
    if (!multicore_fifo_push_timeout_us(cmd, 0))
    {
        fault_record(FAULT_LEVEL_ERROR, "dpi", "IPC FIFO full, DPI switch failed");
        printf("[DPI] IPC FIFO full, switch failed\n");
        return;
    }
    /* 带超时等待 ACK：使用 pop_timeout_us，避免 Core1 异常时永久阻塞 */
    uint32_t ack = IPC_ACK_TIMEOUT;
    bool ack_received = multicore_fifo_pop_timeout_us(DPI_IPC_TIMEOUT_US, &ack);
    if (!ack_received)
    {
        /* 超时：Core1 可能死锁或崩溃，记录错误但不复位
         * （watchdog 的 heartbeat 监控会在 500ms 后检测到 Core1 死锁并复位） */
        fault_record(FAULT_LEVEL_ERROR, "dpi", "IPC ACK timeout, Core1 may be stuck");
        printf("[DPI] IPC ACK timeout (%lu us), Core1 may be stuck\n",
               (unsigned long)DPI_IPC_TIMEOUT_US);
        return;
    }
    if (ack == IPC_ACK_OK)
    {
        /* 保存配置到Flash（使用安全写入服务，双核同步） */
        device_config_t cfg = *config_get();
        cfg.dpi = new_dpi_val;
        config_save(&cfg);
        printf("[DPI] Switched to %u DPI\n", (unsigned)new_dpi_val);
    }
    else
    {
        fault_record(FAULT_LEVEL_ERROR, "dpi", "IPC ACK error");
        printf("[DPI] IPC ACK error: 0x%08lX\n", (unsigned long)ack);
    }
}
/* ==================== 任务函数声明 ==================== */
static void keypad_task(void);
static void mouse_hid_task(void);
static void joystick_task(void);
static void encoder_processor_task(void);
/* ==================== 调度器任务列表 ==================== */
/* 任务列表：按优先级排序（高优先级在前）
 * 注意：任务函数内部不再做时间判断，统一由调度器管理
 */
static sched_task_t g_task_list[] =
{
    /* 看门狗巡检：1ms，最高优先级 */
    {.interval_us = SCHED_INTERVAL_WATCHDOG_US,  .last_run_us = 0, .priority = SCHED_PRIORITY_HIGHEST, .task_func = watchdog_tick},
    /* 编码器处理器：1ms，高优先级（必须在 mouse_hid_task 之前运行，累积滚轮步数） */
    {.interval_us = SCHED_INTERVAL_ENCODER_US,   .last_run_us = 0, .priority = SCHED_PRIORITY_HIGH,    .task_func = encoder_processor_task},
    /* 鼠标HID发送：1ms（1000Hz回报率），高优先级 */
    {.interval_us = SCHED_INTERVAL_MOUSE_HID_US, .last_run_us = 0, .priority = SCHED_PRIORITY_HIGH,    .task_func = mouse_hid_task},
    /* 键盘业务处理：5ms（200Hz）- 硬件扫描在 Core1，普通优先级 */
    {.interval_us = SCHED_INTERVAL_KEYPAD_US,    .last_run_us = 0, .priority = SCHED_PRIORITY_NORMAL,  .task_func = keypad_task},
    /* 摇杆业务处理：10ms（100Hz）- 硬件扫描在 Core1，普通优先级 */
    {.interval_us = SCHED_INTERVAL_JOYSTICK_US,  .last_run_us = 0, .priority = SCHED_PRIORITY_NORMAL,  .task_func = joystick_task},
    /* LED闪烁：10ms（内部有自己的时间判断），低优先级 */
    {.interval_us = SCHED_INTERVAL_LED_BLINK_US, .last_run_us = 0, .priority = SCHED_PRIORITY_LOW,     .task_func = led_manager_blinking_task},
};
#define TASK_COUNT (sizeof(g_task_list) / sizeof(g_task_list[0]))
/*------------- MAIN -------------*/
/* 系统初始化函数（从 main 提取，约220行初始化逻辑）
 * 包含：板级初始化、USB栈、Flash、配置、按键、传感器、编码器、摇杆、
 *       调度器、共享数据、Core1启动、看门狗等
 */
static void system_init_all(void);
int main(void)
{
  // VTOR 由 Bootloader 跳转时设置（dfu_jump_to_app 中 SCB_VTOR_ADDR = app_addr）
  // pico-sdk 启动代码也会根据链接脚本设置
  // 注意：不要在此处重复设置 VTOR，在中断已启用时切换向量表会导致 HardFault
  // Core1 的 VTOR 由 pico-sdk 启动时复制 Core0 的 VTOR（multicore.c: vector_table = scb_hw->vtor）
  /* 执行所有系统初始化（板级、USB、Flash、配置、按键、传感器、编码器、摇杆、调度器、Core1、看门狗） */
  system_init_all();
  while (1)
  {
    /* 组合键检测：Fn + Esc 长按 3 秒恢复出厂设置
     * 优先检测，确保即使其他任务异常也能触发恢复 */
    check_factory_reset_combo();
    /* 性能监控：主循环tick */
#if FEATURE_PERF_MONITOR
    perf_loop_tick();
#endif
    /* 按键统计：自动保存计时 */
#if FEATURE_KEY_STATS
    key_stats_tick();
#endif
    /* 低功耗管理：检查是否需要进入休眠 */
    power_manager_tick();
    /* 摇杆校准数据更新检查：Core1 校准完成后会写入共享数据，
     * Core0 检测到更新后保存到配置 Flash（持久化） */
    if (shared_hw_check_and_clear_joystick_calib_updated())
    {
      uint16_t center_x = 2048, center_y = 2048;
      uint8_t calibrated = 0;
      shared_hw_get_joystick_calibration_data(&center_x, &center_y, &calibrated);
      /* 读取当前配置，修改校准字段后保存 */
      device_config_t new_cfg;
      memcpy(&new_cfg, config_get(), sizeof(device_config_t));
      new_cfg.joystick_center_x = center_x;
      new_cfg.joystick_center_y = center_y;
      new_cfg.joystick_calibrated = calibrated;
      config_save(&new_cfg);
      printf("[JOYSTICK] 校准数据已保存到 Flash: center_x=%d, center_y=%d\n",
             center_x, center_y);
    }
    /* 编码器模式变化检测：模式切换后保存到配置 Flash（持久化） */
    {
      static encoder_mode_t s_last_encoder_mode = ENCODER_MODE_SCROLL;
      encoder_mode_t current_mode = encoder_processor_get_mode();
      if (current_mode != s_last_encoder_mode)
      {
        s_last_encoder_mode = current_mode;
        device_config_t new_cfg;
        memcpy(&new_cfg, config_get(), sizeof(device_config_t));
        new_cfg.reserved3[0] = (uint8_t)current_mode;
        config_save(&new_cfg);
        printf("[ENCODER] 模式已保存到 Flash: mode=%d\n", current_mode);
        /* 模式切换 LED 反馈：闪烁次数 = 模式编号 + 1 */
        led_manager_trigger_mode_feedback(current_mode);
      }
    }
    /* TinyUSB设备任务：每轮都调用，保证USB响应及时 */
#if FEATURE_PERF_MONITOR
    perf_start(0);
#endif
    tud_task();
#if FEATURE_PERF_MONITOR
    perf_end(0);
    perf_start(1);
#endif
    hid_config_task();
#if FEATURE_PERF_MONITOR
    perf_end(1);
    perf_start(2);
#endif
    macro_task();
#if FEATURE_PERF_MONITOR
    perf_end(2);
#endif
    /* 调度器运行所有任务 */
#if FEATURE_PERF_MONITOR
    perf_start(3);
#endif
    sched_run(g_task_list, TASK_COUNT);
#if FEATURE_PERF_MONITOR
    perf_end(3);
#endif
    /* 主循环正常运行，喂BOARD层和APP层（DEVICE层由Core1喂） */
    watchdog_feed_layer(WDG_LAYER_BOARD);
    watchdog_feed_layer(WDG_LAYER_APP);
  }
}
//--------------------------------------------------------------------+
// USB 回调函数已移至 app/usb_callbacks.c
// 包含 tud_mount_cb / tud_umount_cb / tud_suspend_cb / tud_resume_cb
//--------------------------------------------------------------------+
//--------------------------------------------------------------------+
// USB HID 回调（已移除 TinyUSB 示例残留的 send_hid_report 链式发送）
// 键盘/鼠标/消费者报告由各自的任务函数直接发送
//--------------------------------------------------------------------+
#if TUSB_VERSION_NUMBER > 1800
// board_millis has been removed from tinyusb. Use tusb_time_millis_api instead
#define board_millis tusb_time_millis_api
#endif
//--------------------------------------------------------------------+
// 编码器处理器任务 - 从编码器处理器获取事件，根据模式生成输出
// SCROLL: 累积滚轮步数到 g_wheel
// VOLUME: 发送 Consumer Control 音量增减
// ZOOM: 发送 Ctrl+滚轮
// BRIGHTNESS: 发送 Consumer Control 亮度增减
// CUSTOM: 预留
//--------------------------------------------------------------------+
static void encoder_processor_task(void)
{
  /* 每帧先发送队列中的一个待发送报告（确保报告间至少间隔 1ms）
   * 这解决了 VOLUME/ZOOM/BRIGHTNESS 模式连续发送多报告导致的
   * 主机处理顺序错误或报告丢失问题。 */
  encoder_report_queue_send_one();
  encoder_event_t event;
  while (encoder_processor_get_event(&event))
  {
    int32_t delta = (event.direction == ENCODER_DIR_CW) ? 1 : -1;
    int32_t total_delta = delta * event.multiplier;
    switch (event.mode)
    {
      case ENCODER_MODE_SCROLL:
        /* 滚动模式：累积到 g_wheel，由 mouse_hid_task 发送 */
        g_wheel += total_delta;
        break;
      case ENCODER_MODE_VOLUME:
      {
        /* 音量模式：Consumer Control 报告，分两步入队（按下 → 释放）
         * 每步间隔 1ms，确保操作系统正确识别按键事件 */
        uint16_t usage = (delta > 0) ? 0x00E9 /* 音量+ */ : 0x00EA /* 音量- */;
        uint8_t press_report[2] = { (uint8_t)(usage & 0xFF), (uint8_t)((usage >> 8) & 0xFF) };
        uint8_t release_report[2] = {0, 0};
        encoder_report_queue_push_generic(ENC_REPORT_CONSUMER, press_report, 2);
        encoder_report_queue_push_generic(ENC_REPORT_CONSUMER, release_report, 2);
        break;
      }
      case ENCODER_MODE_ZOOM:
      {
        /* 缩放模式：Ctrl按下 → 滚轮 → Ctrl释放，分三步入队
         * 每步间隔 1ms，确保 Ctrl 修饰键在滚轮事件期间保持按下状态 */
        int8_t wheel = (int8_t)((total_delta > 127) ? 127 : ((total_delta < -127) ? -127 : total_delta));
        /* 1. Ctrl 按下（Report ID + Modifier=LeftCtrl + 6个空键码） */
        uint8_t ctrl_down[8] = {0};
        ctrl_down[0] = REPORT_ID_KEYBOARD;
        ctrl_down[1] = 0x01;  /* Left Ctrl */
        encoder_report_queue_push_generic(ENC_REPORT_KEYBOARD, ctrl_down, 8);
        /* 2. 鼠标滚轮 */
        encoder_report_queue_push_mouse_wheel(wheel);
        /* 3. Ctrl 释放（全零键盘报告） */
        uint8_t ctrl_up[8] = {0};
        ctrl_up[0] = REPORT_ID_KEYBOARD;
        encoder_report_queue_push_generic(ENC_REPORT_KEYBOARD, ctrl_up, 8);
        break;
      }
      case ENCODER_MODE_BRIGHTNESS:
      {
        /* 亮度模式：Consumer Control 报告，分两步入队（按下 → 释放） */
        uint16_t usage = (delta > 0) ? 0x006F /* 亮度+ */ : 0x0070 /* 亮度- */;
        uint8_t press_report[2] = { (uint8_t)(usage & 0xFF), (uint8_t)((usage >> 8) & 0xFF) };
        uint8_t release_report[2] = {0, 0};
        encoder_report_queue_push_generic(ENC_REPORT_CONSUMER, press_report, 2);
        encoder_report_queue_push_generic(ENC_REPORT_CONSUMER, release_report, 2);
        break;
      }
      case ENCODER_MODE_CUSTOM:
      {
        /* 自定义模式：按键按下 → 释放，分两步入队 */
        uint8_t cw_key = 0, ccw_key = 0;
        encoder_processor_get_custom_keys(&cw_key, &ccw_key);
        uint8_t key = (delta > 0) ? cw_key : ccw_key;
        if (key != 0)
        {
          /* 1. 按键按下 */
          uint8_t key_down[8] = {0};
          key_down[0] = REPORT_ID_KEYBOARD;
          key_down[3] = key;  /* 第一个键码位置 */
          encoder_report_queue_push_generic(ENC_REPORT_KEYBOARD, key_down, 8);
          /* 2. 按键释放 */
          uint8_t key_up[8] = {0};
          key_up[0] = REPORT_ID_KEYBOARD;
          encoder_report_queue_push_generic(ENC_REPORT_KEYBOARD, key_up, 8);
        }
        break;
      }
      default:
        break;
    }
  }
}
//--------------------------------------------------------------------+
// 鼠标HID统一发送任务 - 所有鼠标相关数据统一在这里发送
// 硬件扫描在 Core1，这里从共享数据读取
//--------------------------------------------------------------------+
static void mouse_hid_task(void)
{
  static uint8_t last_buttons = 0;
  // 从 Core1 共享数据读取累积的位移（滚轮由编码器处理器任务处理）
  int32_t new_dx = 0, new_dy = 0;
  shared_hw_take_motion(&new_dx, &new_dy);
  g_mouse_dx += new_dx;
  g_mouse_dy += new_dy;
  /* 滚轮不再从共享数据获取，由 encoder_processor_task 累积到 g_wheel */
  // 读取鼠标按键状态（物理按钮 + 宏按钮合并）
  g_mouse_buttons = shared_hw_get_mouse_buttons() | macro_get_mouse_buttons();
  // 没有数据且按键无变化就不发
  if (g_mouse_dx == 0 && g_mouse_dy == 0 && g_wheel == 0 && g_mouse_buttons == last_buttons)
  {
    return;
  }
  if (!tud_hid_ready())
  {
    return;
  }
  // 限制每帧位移范围（-127 ~ 127）
  int8_t dx = (int8_t)((g_mouse_dx > 127) ? 127 : ((g_mouse_dx < -127) ? -127 : g_mouse_dx));
  int8_t dy = (int8_t)((g_mouse_dy > 127) ? 127 : ((g_mouse_dy < -127) ? -127 : g_mouse_dy));
  int8_t wheel = (int8_t)((g_wheel > 127) ? 127 : ((g_wheel < -127) ? -127 : g_wheel));
  // 发送鼠标报告
  tud_hid_mouse_report(REPORT_ID_MOUSE, g_mouse_buttons, dx, dy, wheel, 0);
  // 减去已发送的部分
  g_mouse_dx -= dx;
  g_mouse_dy -= dy;
  g_wheel -= wheel;
  last_buttons = g_mouse_buttons;
}
//--------------------------------------------------------------------+
// PS2摇杆任务 - 游戏手柄（Core0 业务处理：从共享数据读取并发送HID）
//--------------------------------------------------------------------+
static void joystick_task(void)
{
  static int8_t last_x = 0;
  static int8_t last_y = 0;
  static uint32_t last_btn = 0;
  // 从 Core1 共享数据读取摇杆状态（硬件读取和死区处理在 Core1 完成）
  int16_t joy_x_16 = 0, joy_y_16 = 0;
  bool btn_pressed = false;
  shared_hw_get_joystick(&joy_x_16, &joy_y_16, &btn_pressed);
  int8_t joy_x = (int8_t)joy_x_16;
  int8_t joy_y = (int8_t)joy_y_16;
  uint32_t buttons = btn_pressed ? 0x00000001UL : 0x00000000UL;
  // 只有变化时才发送，减少USB流量
  if (joy_x == last_x && joy_y == last_y && buttons == last_btn)
  {
    return;
  }
  if (!tud_hid_ready())
  {
    return;
  }
  // 发送游戏手柄报告：X, Y, Z, Rz, Rx, Ry, Hat, Buttons
  tud_hid_gamepad_report(REPORT_ID_GAMEPAD, joy_x, joy_y, 0, 0, 0, 0, 0, buttons);
  last_x = joy_x;
  last_y = joy_y;
  last_btn = buttons;
}
//--------------------------------------------------------------------+
// 64键键盘任务子函数（按功能模块划分）
//--------------------------------------------------------------------+

/* 处理按键状态变化（Fn键检测、DPI切换键检测、按键统计、宏触发） */
static void process_key_state_changes(uint64_t stable_keys, uint64_t last_stable, bool *fn_pressed)
{
  // 检测Fn键状态变化
  bool fn_now = ((stable_keys >> keymap_get_fn_key()) & 1ULL) == 0ULL;
  if (fn_now != *fn_pressed) {
    *fn_pressed = fn_now;
  }

  // 检测DPI切换键的按下上升沿
  uint8_t dpi_key = config_get()->dpi_switch_key;
  if (dpi_key == 0xFF) dpi_key = DPI_SWITCH_KEY_INDEX;
  if (dpi_key < 64) {
    bool key_now = ((stable_keys >> dpi_key) & 1ULL) == 0ULL;
    bool key_last = ((last_stable >> dpi_key) & 1ULL) == 0ULL;
    if (key_now && !key_last) {
      dpi_cycle_next();
    }
  }

  // 按键统计：检测按下上升沿，增加计数（使用 XOR+ctz 优化）
  uint64_t changed_keys = stable_keys ^ last_stable;
  uint64_t pressed = ~stable_keys & changed_keys;
  while (pressed) {
#if FEATURE_KEY_STATS
    int i = __builtin_ctzll(pressed);
    key_stats_increment(i);
#endif
    power_manager_notify_activity();
    pressed &= pressed - 1;
  }

  // 宏触发检测：只检查状态变化的键（上升沿触发，下降沿停止）
  changed_keys = stable_keys ^ last_stable;
  while (changed_keys) {
    int i = __builtin_ctzll(changed_keys);
    changed_keys &= changed_keys - 1;
    bool now = ((stable_keys >> i) & 1ULL) == 0ULL;
    bool last = ((last_stable >> i) & 1ULL) == 0ULL;
    uint8_t macro_id = macro_find_by_trigger_key((uint8_t)i);
    if (macro_id != 0xFF) {
      if (now && !last) {
        macro_trigger(macro_id);
      } else if (!now && last) {
        macro_stop(macro_id);
      }
    }
  }
}

/* 构建并发送 HID 键盘报告（包括普通按键、修饰键、多媒体键、层切换） */
static void build_and_send_hid_report(uint64_t stable_keys, bool fn_pressed, uint16_t *last_consumer)
{
  if (!tud_hid_ready()) return;

  uint8_t keycode[6] = { 0 };
  uint8_t key_count = 0;
  uint8_t modifier = 0;
  uint16_t consumer_code = 0;
  bool current_layer_switch_pressed = false;

  // 遍历64个键，收集按下的键（最多6个，标准6KRO）
  for (int i = 0; i < 64; i++) {
    if (((stable_keys >> i) & 1ULL) != 0ULL) continue; // bit=1表示释放
    if (macro_find_by_trigger_key((uint8_t)i) != 0xFF) continue; // 宏触发键不作为普通按键

    keymap_result_t result;
    if (!keymap_lookup(i, fn_pressed, &result)) continue; // 空键跳过

    switch (result.type) {
      case KEYMAP_TYPE_MODIFIER:
        modifier |= result.code;
        break;
      case KEYMAP_TYPE_NORMAL:
        if (key_count < 6) keycode[key_count++] = result.code;
        break;
      case KEYMAP_TYPE_FN:
        break; // Fn键本身不输出
      case KEYMAP_TYPE_CONSUMER:
        if (consumer_code == 0) consumer_code = result.code; // 多媒体键只取第一个
        break;
      case KEYMAP_TYPE_LAYER_SWITCH:
        current_layer_switch_pressed = true;
        if (!s_layer_switch_was_pressed) {
          s_layer_switch_was_pressed = true;
          switch (result.code) {
            case KEYMAP_KEY_LAYER_0: keymap_set_active_layer(KEYMAP_LAYER_0); printf("[KEYMAP] 切换到 Layer 0\n"); break;
            case KEYMAP_KEY_LAYER_1: keymap_set_active_layer(KEYMAP_LAYER_1); printf("[KEYMAP] 切换到 Layer 1\n"); break;
            case KEYMAP_KEY_LAYER_2: keymap_set_active_layer(KEYMAP_LAYER_2); printf("[KEYMAP] 切换到 Layer 2\n"); break;
            case KEYMAP_KEY_LAYER_3: keymap_set_active_layer(KEYMAP_LAYER_3); printf("[KEYMAP] 切换到 Layer 3\n"); break;
            case KEYMAP_KEY_LAYER_NEXT: keymap_cycle_layer(); printf("[KEYMAP] 循环切换层\n"); break;
          }
        }
        break;
      default: break;
    }
  }

  // 更新层切换键状态（边沿检测）
  s_layer_switch_was_pressed = current_layer_switch_pressed;

  // 合并宏的按键状态
  uint8_t macro_mod = 0;
  uint8_t macro_keys[6] = {0};
  macro_get_keyboard_state(&macro_mod, macro_keys);
  modifier |= macro_mod;
  for (int i = 0; i < 6; i++) {
    if (macro_keys[i] == 0) continue;
    bool exists = false;
    for (int j = 0; j < key_count; j++) {
      if (keycode[j] == macro_keys[i]) { exists = true; break; }
    }
    if (!exists && key_count < 6) keycode[key_count++] = macro_keys[i];
  }

  // 发送 HID 键盘报告
  tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycode);
  keyboard_send_nkro_report(stable_keys, modifier);

  // 发送 Consumer 多媒体报告（状态变化时）
  if (consumer_code != *last_consumer) {
    uint8_t consumer_report[2];
    consumer_report[0] = (uint8_t)(consumer_code & 0xFF);
    consumer_report[1] = (uint8_t)((consumer_code >> 8) & 0xFF);
    tud_hid_report(REPORT_ID_CONSUMER_CONTROL, consumer_report, 2);
    *last_consumer = consumer_code;
  }
}

//--------------------------------------------------------------------+
// 64键键盘任务（Core0 业务处理：从共享数据读取，处理Fn/DPI/统计/HID发送）
//--------------------------------------------------------------------+
static void keypad_task(void)
{
  static uint64_t last_stable = 0xFFFFFFFFFFFFFFFFULL;
  static bool fn_pressed = false;
  static uint16_t last_consumer = 0;

  // 从 Core1 共享数据读取稳定按键状态（硬件扫描和消抖在 Core1 完成）
  uint64_t stable_keys = shared_hw_get_keys();
  keyboard_event_update(stable_keys, false);
  g_stable_keys = stable_keys;

  // 按键状态变化或宏按键状态变化时
  bool phy_changed = (stable_keys != last_stable);
  bool macro_changed = macro_kb_has_changed();
  if (phy_changed || macro_changed) {
    if (phy_changed) {
      process_key_state_changes(stable_keys, last_stable, &fn_pressed);
    }
    build_and_send_hid_report(stable_keys, fn_pressed, &last_consumer);
    if (phy_changed) {
      last_stable = stable_keys;
    }
  }
}

/* ==================== 系统初始化子函数（按功能模块划分） ==================== */

/* 板级和串口初始化 */
static void init_board_and_serial(void)
{
  board_init();
#if FEATURE_DEBUG_UART
  stdio_init_all();
#endif
  printf("\n=== Pico2 HID 复合设备启动 ===\n");
#if FEATURE_DEBUG_UART
  print_reset_reason();
#endif
  printf("USB 设备栈初始化中...\n");
}

/* 核心服务初始化（Flash、故障、按键事件、统计、性能、低功耗） */
static void init_core_services(void)
{
  flash_service_init();
  printf("初始化板级硬件...\n");
  bsp_init();
#if FEATURE_FAULT_LOG
  fault_init();
#endif
  keyboard_event_init();
#if FEATURE_KEY_STATS
  key_stats_init();
  printf("按键统计模块初始化完成，总按键数: %lu\n", (unsigned long)key_stats_get_total());
#endif

#if FEATURE_PERF_MONITOR
  perf_init();
  perf_register_task(0, "tud_task");
  perf_register_task(1, "hid_config");
  perf_register_task(2, "macro_task");
  perf_register_task(3, "scheduler");
#endif
  power_manager_init();
#if FEATURE_PERF_MONITOR
  /* 从配置加载性能监控阈值（持久化，重启后保留用户设置）
   * device_config_t 为 packed，直接对 perf_thresholds 成员取地址会得到
   * 潜在非对齐指针（-Waddress-of-packed-member），先拷贝到本地对齐缓冲再加载 */
  const device_config_t* perf_cfg = config_get();
  if (perf_cfg != NULL) {
    uint16_t perf_thresholds_aligned[8];
    memcpy(perf_thresholds_aligned, perf_cfg->perf_thresholds,
           sizeof(perf_thresholds_aligned));
    perf_load_thresholds(perf_thresholds_aligned, 8);
  }
  perf_set_enabled(true);
#endif
}

/* 按键映射和宏模块初始化 */
static void init_keymap_and_macro(void)
{
  keymap_init();
  printf("按键映射初始化完成，Fn键索引: %d\n", keymap_get_fn_key());
  keyboard_processor_init();
  encoder_report_queue_init();
  led_manager_init();
  macro_init();
  const device_config_t* cfg = config_get();
  if (cfg != NULL) {
    macro_load_from_config(cfg->macro_data, CONFIG_MACRO_DATA_SIZE);
  }
}

/* 64键SPI键盘初始化和工厂测试模式检测 */
static void init_keypad_and_factory_test(void)
{
  printf("初始化64键SPI键盘...\n");
  keypad_cfg = board_get_keypad_spi_cfg();
  keypad_spi_init(keypad_cfg);
  debounce_64key_init(&keypad_debounce, 5);
  printf("键盘初始化完成\n");

#if FEATURE_FACTORY_TEST
  printf("检测工厂测试模式...\n");
  uint64_t keys = 0;
  int fn_press_count = 0;
  uint8_t fn_key = keymap_get_fn_key();
  for (int i = 0; i < 50; i++) {
    keypad_spi_read_u64(keypad_cfg, &keys);
    if (((keys >> fn_key) & 1ULL) == 0ULL) {
      fn_press_count++;
    }
    if (i % 10 == 0) {
      gpio_xor_mask(1u << LED_PIN);
    }
    sleep_ms(10);
  }
  if (fn_press_count > 40) {
    printf("\n========================================\n");
    printf("  检测到Fn键长按，进入工厂测试模式！\n");
    printf("========================================\n\n");
    factory_test_enter();
  }
#endif /* FEATURE_FACTORY_TEST */
  printf("正常启动模式\n");
}

/* 光学传感器、编码器、摇杆初始化 */
static void init_sensors_and_encoders(void)
{
  printf("初始化OPTICAL_SENSOR光学传感器...\n");
  optical_sensor_cfg = board_get_optical_sensor_cfg();
  int paw_ret = optical_sensor_init(optical_sensor_cfg);
  if (paw_ret == 0) {
    uint8_t pid = 0, rid = 0;
    optical_sensor_reg_read(optical_sensor_cfg, 0x00, &pid);
    optical_sensor_reg_read(optical_sensor_cfg, 0x01, &rid);
    printf("OPTICAL_SENSOR初始化成功\n");
    printf("  产品ID: 0x%02X\n", pid);
    printf("  修订ID: 0x%02X\n", rid);
    uint16_t dpi_val = config_get()->dpi;
    if (dpi_val == 400 || dpi_val == 800 || dpi_val == 1600 || dpi_val == 3200) {
      optical_sensor_dpi_e dpi_enum = dpi_val_to_enum(dpi_val);
      optical_sensor_set_dpi(optical_sensor_cfg, dpi_enum);
    } else {
      optical_sensor_set_dpi_raw(optical_sensor_cfg, dpi_val);
    }
    printf("  DPI设置为: %d (从配置加载)\n", dpi_val);
  } else {
    printf("OPTICAL_SENSOR初始化失败，错误码: %d\n", paw_ret);
  }

  printf("初始化滚轮编码器...\n");
  encoder_cfg = board_get_encoder_cfg();
  encoder_init(encoder_cfg);
  encoder_state_init(&encoder_state);
  printf("编码器初始化完成\n");

  printf("初始化PS2摇杆...\n");
  joystick_cfg = board_get_joystick_cfg();
  joystick_init(joystick_cfg);
  printf("摇杆初始化完成\n");
}

/* USB设备栈、调度器、共享数据初始化 */
static void init_usb_and_scheduler(void)
{
  const tusb_rhport_init_t rh_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUD_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL
  };
  if (!tud_rhport_init(BOARD_TUD_RHPORT, &rh_init)) {
    printf("USB root port init failed!\n");
  }
  board_init_after_tusb();
  sched_init();
  shared_hw_data_init();
  printf("共享数据初始化完成\n");
  usb_callbacks_init();

  const device_config_t *cfg = config_get();
  shared_hw_set_joystick_calibration_data(cfg->joystick_center_x,
                                            cfg->joystick_center_y,
                                            cfg->joystick_calibrated);
  if (cfg->joystick_calibrated) {
    printf("[JOYSTICK] 校准数据已加载: center_x=%d, center_y=%d\n",
           cfg->joystick_center_x, cfg->joystick_center_y);
  }
}

/* Core1启动、看门狗初始化、复位原因记录 */
static void init_core1_and_watchdog(void)
{
  multicore_launch_core1(core1_scanner_main);
  printf("Core1 硬件扫描已启动\n");

  watchdog_init(WATCHDOG_INIT_TIMEOUT_MS);
  watchdog_feed_layer(WDG_LAYER_BOARD);
  watchdog_feed_layer(WDG_LAYER_DEVICE);
  watchdog_feed_layer(WDG_LAYER_APP);
  printf("看门狗启动完成（逻辑超时%dms，硬件超时2秒）\n", WATCHDOG_INIT_TIMEOUT_MS);

  extern bool watchdog_caused_reboot(void);
  if (watchdog_caused_reboot()) {
    wdg_layer_t last_layer = watchdog_get_last_fault_layer();
    const char* layer_name = "unknown";
    switch (last_layer) {
      case WDG_LAYER_BOARD:  layer_name = "BOARD"; break;
      case WDG_LAYER_DEVICE: layer_name = "DEVICE"; break;
      case WDG_LAYER_APP:    layer_name = "APP"; break;
      default:
        if (last_layer == (wdg_layer_t)0xFF) layer_name = "MANUAL";
        break;
    }
    fault_record(FAULT_LEVEL_WARN, "watchdog", "system recovered from watchdog reset");
    printf("[WATCHDOG] 系统从看门狗复位恢复，最后故障层: %s\n", layer_name);
  }
}

/* ==================== 系统初始化函数 ==================== */
/* 从 main 提取，包含所有板级、硬件、协议栈、调度器、Core1、看门狗初始化 */
static void system_init_all(void)
{
  init_board_and_serial();
  init_core_services();
  init_keymap_and_macro();
  init_keypad_and_factory_test();
  init_sensors_and_encoders();
  init_usb_and_scheduler();
  init_core1_and_watchdog();
}
