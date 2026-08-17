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

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"
#include "protocol/config_hid.h"
#include "board/board.h"
#include "board/config.h"
#include "device/keypad_spi.h"
#include "device/paw3395.h"
#include "device/encoder.h"
#include "device/joystick.h"
#include "middleware/debounce.h"
#include "middleware/watchdog.h"
#include "middleware/scheduler.h"
#include "middleware/fault.h"
#include "middleware/perf_monitor.h"
#include "middleware/flash_service.h"
#include "middleware/power_manager.h"
#include "app/keymap.h"
#include "app/macro.h"
#include "app/key_stats.h"
#include "app/factory_test.h"
#include "middleware/shared_hw_data.h"
#include "middleware/ipc.h"
#include "app/core1_scanner.h"
#include "pico/multicore.h"

/* 简单日志分级控制：可在编译时定义 LOG_LEVEL（0=NONE,1=ERROR,2=INFO,3=DEBUG） */
#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif

#define LOG_NONE 0
#define LOG_ERROR 1
#define LOG_INFO 2
#define LOG_DEBUG 3

#define LOG_ERROR_PRINT(fmt, ...) do { if (LOG_LEVEL >= LOG_ERROR) printf("[ERR] " fmt, ##__VA_ARGS__); } while(0)
#define LOG_INFO_PRINT(fmt, ...)  do { if (LOG_LEVEL >= LOG_INFO)  printf("[INFO] " fmt, ##__VA_ARGS__); } while(0)
#define LOG_DEBUG_PRINT(fmt, ...) do { if (LOG_LEVEL >= LOG_DEBUG) printf("[DBG] " fmt, ##__VA_ARGS__); } while(0)

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

/* ==================== 64键键盘相关 ==================== */
static const keypad_spi_cfg_t *keypad_cfg;
static debounce_64key_t keypad_debounce;
static uint64_t g_stable_keys = 0xFFFFFFFFFFFFFFFFULL;

/* ==================== PAW3395鼠标传感器相关 ==================== */
static const paw3395_cfg_t *paw3395_cfg;
static int32_t g_mouse_dx = 0;  /* 累积的X位移（int32防止溢出） */
static int32_t g_mouse_dy = 0;  /* 累积的Y位移（int32防止溢出） */
static uint8_t g_mouse_buttons = 0; /* 鼠标按键位掩码 */

/* ==================== 滚轮编码器相关 ==================== */
static const encoder_cfg_t *encoder_cfg;
static encoder_state_t encoder_state;
static int16_t g_wheel = 0;  /* 累积的滚轮步数（int16防止溢出） */

/* ==================== PS2摇杆相关 ==================== */
static const joystick_cfg_t *joystick_cfg;
static int16_t g_joy_x = 0; /* 摇杆X轴 (-127 ~ 127) */
static int16_t g_joy_y = 0; /* 摇杆Y轴 (-127 ~ 127) */
static bool g_joy_btn = false; /* 摇杆按键 */

/* ==================== 64键键盘映射表 ====================
 * 索引0~63 对应 物理键1~64
 * 值为HID按键码，0xFF表示修饰键（单独处理）

void led_blinking_task(void);
void keypad_task(void);
void mouse_hid_task(void);
void joystick_task(void);

/* ==================== DPI切换相关 ==================== */
/* DPI切换键：数字3键（索引3） */
#define DPI_SWITCH_KEY_INDEX  3

/* DPI档位列表（实际数值）：400 / 800 / 1600 / 3200 */
static const uint16_t s_dpi_list[] = {400, 800, 1600, 3200};
#define DPI_COUNT (sizeof(s_dpi_list) / sizeof(s_dpi_list[0]))

/* DPI数值转枚举 */
static paw3395_dpi_e dpi_val_to_enum(uint16_t dpi_val)
{
    switch (dpi_val)
    {
        case 400:  return PAW3395_DPI_400;
        case 800:  return PAW3395_DPI_800;
        case 1600: return PAW3395_DPI_1600;
        case 3200: return PAW3395_DPI_3200;
        default:   return PAW3395_DPI_1600; /* 默认1600 */
    }
}

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
    paw3395_dpi_e new_dpi_enum = dpi_val_to_enum(new_dpi_val);

    /* 通过IPC发送给Core1执行DPI切换，避免Core0直接操作SPI1与Core1竞争 */
    uint32_t cmd = IPC_MAKE_CMD(IPC_CMD_SET_DPI, (uint32_t)new_dpi_enum);
    multicore_fifo_push_blocking(cmd);
    uint32_t ack = multicore_fifo_pop_blocking();

    if (ack == IPC_ACK_OK)
    {
        /* 保存配置到Flash（使用安全写入服务，双核同步） */
        device_config_t cfg = *config_get();
        cfg.dpi = new_dpi_val;
        config_save(&cfg);
    }
}

/* ==================== 任务函数声明 ==================== */
static void led_blinking_task(void);
static void keypad_task(void);
static void mouse_hid_task(void);
static void joystick_task(void);

/* ==================== 调度器任务列表 ==================== */
/* 任务列表：按优先级排序（高优先级在前）
 * 注意：任务函数内部不再做时间判断，统一由调度器管理
 */
static sched_task_t g_task_list[] =
{
    /* 看门狗巡检：1ms，最高优先级 */
    {.interval_us = 1000,  .last_run_us = 0, .priority = SCHED_PRIORITY_HIGHEST, .task_func = watchdog_tick},

    /* 鼠标HID发送：1ms（1000Hz回报率），高优先级 */
    {.interval_us = 1000,  .last_run_us = 0, .priority = SCHED_PRIORITY_HIGH,    .task_func = mouse_hid_task},

    /* 键盘业务处理：5ms（200Hz）- 硬件扫描在 Core1，普通优先级 */
    {.interval_us = 5000,  .last_run_us = 0, .priority = SCHED_PRIORITY_NORMAL,  .task_func = keypad_task},

    /* 摇杆业务处理：10ms（100Hz）- 硬件扫描在 Core1，普通优先级 */
    {.interval_us = 10000, .last_run_us = 0, .priority = SCHED_PRIORITY_NORMAL,  .task_func = joystick_task},

    /* LED闪烁：10ms（内部有自己的时间判断），低优先级 */
    {.interval_us = 10000, .last_run_us = 0, .priority = SCHED_PRIORITY_LOW,     .task_func = led_blinking_task},
};

#define TASK_COUNT (sizeof(g_task_list) / sizeof(g_task_list[0]))

/*------------- MAIN -------------*/

// 函数前向声明

int main(void)
{
  board_init();

  // 初始化UART串口（默认GP0=TX, GP1=RX，波特率115200）
  stdio_init_all();
  printf("\n=== Pico2 HID 复合设备启动 ===\n");
  printf("USB 设备栈初始化中...\n");

  // 初始化Flash安全写入服务（双核同步，必须在bsp_init/config_init之前）
  flash_service_init();

  // 初始化板级硬件（内部会调用config_init读取Flash配置）
  printf("初始化板级硬件...\n");
  bsp_init();

  // 初始化故障记录模块（Flash持久化错误日志）
  fault_init();

  // 初始化按键统计模块（Flash持久化，磨损均衡）
  key_stats_init();
  printf("按键统计模块初始化完成，总按键数: %lu\n", (unsigned long)key_stats_get_total());

  // 初始化性能监控模块
  perf_init();
  perf_register_task(0, "tud_task");
  perf_register_task(1, "hid_config");
  perf_register_task(2, "macro_task");
  perf_register_task(3, "scheduler");

  // 初始化低功耗管理模块
  power_manager_init();

  // 设置默认超时阈值（微秒）
  perf_set_threshold(0, 1000);   // tud_task: 1ms
  perf_set_threshold(1, 2000);   // hid_config: 2ms
  perf_set_threshold(2, 500);    // macro_task: 0.5ms
  perf_set_threshold(3, 60000);  // scheduler: 60ms（写Flash时约50ms，需留余量）

  // 启用性能监控（默认关闭，必须显式启用）
  perf_set_enabled(true);

  // 初始化按键映射
  keymap_init();
  printf("按键映射初始化完成，Fn键索引: %d\n", keymap_get_fn_key());

  // 初始化宏模块
  macro_init();

  // 从配置加载宏配置
  {
    const device_config_t* cfg = config_get();
    if (cfg != NULL)
    {
      macro_load_from_config(cfg->macro_data, CONFIG_MACRO_DATA_SIZE);
    }
  }

  // 初始化64键键盘
  printf("初始化64键SPI键盘...\n");
  keypad_cfg = board_get_keypad_spi_cfg();
  keypad_spi_init(keypad_cfg);
  debounce_64key_init(&keypad_debounce, 5);
  printf("键盘初始化完成\n");

  // 检查是否进入工厂测试模式（按住Fn键启动）
  printf("检测工厂测试模式...\n");
  {
    /* 工厂测试模式进入条件：按住Fn键持续约300ms
     * 检测30次，每次间隔10ms，超过25次（>83%）按下才进入
     * 防止用户无意中按住Fn键上电误入工厂模式
     */
    uint64_t keys = 0;
    int fn_press_count = 0;
    uint8_t fn_key = keymap_get_fn_key();
    for (int i = 0; i < 30; i++)
    {
      keypad_spi_read_u64(keypad_cfg, &keys);
      if (((keys >> fn_key) & 1ULL) == 0ULL)  // 低电平有效
      {
        fn_press_count++;
      }
      sleep_ms(10);
    }
    // 如果超过25次检测到Fn键按下，就进入工厂测试模式
    if (fn_press_count > 25)
    {
      printf("\n========================================\n");
      printf("  检测到Fn键长按，进入工厂测试模式！\n");
      printf("========================================\n\n");
      factory_test_enter();
      // factory_test_enter() 不会返回
    }
  }
  printf("正常启动模式\n");

  // 初始化PAW3395鼠标传感器
  printf("初始化PAW3395光学传感器...\n");
  paw3395_cfg = board_get_paw3395_cfg();
  int paw_ret = paw3395_init(paw3395_cfg);
  if (paw_ret == 0)
  {
    uint8_t pid = 0, rid = 0;
    paw3395_reg_read(paw3395_cfg, 0x00, &pid);
    paw3395_reg_read(paw3395_cfg, 0x01, &rid);
    printf("PAW3395初始化成功\n");
    printf("  产品ID: 0x%02X\n", pid);
    printf("  修订ID: 0x%02X\n", rid);

    /* 从配置加载DPI（支持任意DPI值） */
    uint16_t dpi_val = config_get()->dpi;
    if (dpi_val == 400 || dpi_val == 800 || dpi_val == 1600 || dpi_val == 3200)
    {
        paw3395_dpi_e dpi_enum = dpi_val_to_enum(dpi_val);
        paw3395_set_dpi(paw3395_cfg, dpi_enum);
    }
    else
    {
        paw3395_set_dpi_raw(paw3395_cfg, dpi_val);
    }
    printf("  DPI设置为: %d (从配置加载)\n", dpi_val);
  }
  else
  {
    printf("PAW3395初始化失败，错误码: %d\n", paw_ret);
  }

  // 初始化滚轮编码器
  printf("初始化滚轮编码器...\n");
  encoder_cfg = board_get_encoder_cfg();
  encoder_init(encoder_cfg);
  encoder_state_init(&encoder_state);
  printf("编码器初始化完成\n");

  // 初始化PS2摇杆
  printf("初始化PS2摇杆...\n");
  joystick_cfg = board_get_joystick_cfg();
  joystick_init(joystick_cfg);
  printf("摇杆初始化完成\n");

  // init device stack on configured roothub port
  const tusb_rhport_init_t rh_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUD_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL
  };
  TU_ASSERT(tud_rhport_init(BOARD_TUD_RHPORT, &rh_init));
  board_init_after_tusb();

  /* 初始化调度器 */
  sched_init();

  /* 初始化双核共享数据（必须在启动 Core1 之前） */
  shared_hw_data_init();
  printf("共享数据初始化完成\n");

  /* 启动 Core1 硬件扫描（双核架构：Core1 扫描硬件，Core0 处理业务） */
  multicore_launch_core1(core1_scanner_main);
  printf("Core1 硬件扫描已启动\n");

  /* 所有初始化完成，启动看门狗（逻辑超时500ms，硬件超时1000ms） */
  watchdog_init(500);
  watchdog_feed_layer(WDG_LAYER_BOARD);
  watchdog_feed_layer(WDG_LAYER_DEVICE);
  watchdog_feed_layer(WDG_LAYER_APP);
  printf("看门狗启动完成\n");

  while (1)
  {
    /* 性能监控：主循环tick */
    perf_loop_tick();

    /* 按键统计：自动保存计时 */
    key_stats_tick();

    /* 低功耗管理：检查是否需要进入休眠 */
    power_manager_tick();

    /* TinyUSB设备任务：每轮都调用，保证USB响应及时 */
    perf_start(0);
    tud_task();
    perf_end(0);

    perf_start(1);
    hid_config_task();
    perf_end(1);

    perf_start(2);
    macro_task();
    perf_end(2);

    /* 调度器运行所有任务 */
    perf_start(3);
    sched_run(g_task_list, TASK_COUNT);
    perf_end(3);

    /* 主循环正常运行，喂BOARD层和APP层（DEVICE层由Core1喂） */
    watchdog_feed_layer(WDG_LAYER_BOARD);
    watchdog_feed_layer(WDG_LAYER_APP);
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
  printf("USB 挂载成功！\n");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
  power_manager_on_usb_suspend(remote_wakeup_en);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
  power_manager_on_usb_resume();
}

//--------------------------------------------------------------------+
// USB HID 回调（已移除 TinyUSB 示例残留的 send_hid_report 链式发送）
// 键盘/鼠标/消费者报告由各自的任务函数直接发送
//--------------------------------------------------------------------+

#if TUSB_VERSION_NUMBER > 1800
// board_millis has been removed from tinyusb. Use tusb_time_millis_api instead
#define board_millis tusb_time_millis_api
#endif

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
static void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

//--------------------------------------------------------------------+
// 鼠标HID统一发送任务 - 所有鼠标相关数据统一在这里发送
// 硬件扫描在 Core1，这里从共享数据读取
//--------------------------------------------------------------------+
static void mouse_hid_task(void)
{
  static uint8_t last_buttons = 0;

  // 从 Core1 共享数据读取累积的位移和滚轮
  int32_t new_dx = 0, new_dy = 0;
  shared_hw_take_motion(&new_dx, &new_dy);
  g_mouse_dx += new_dx;
  g_mouse_dy += new_dy;
  g_wheel += shared_hw_take_wheel();

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
// 64键键盘任务（Core0 业务处理：从共享数据读取，处理Fn/DPI/统计/HID发送）
//--------------------------------------------------------------------+
static void keypad_task(void)
{
  static uint64_t last_stable = 0xFFFFFFFFFFFFFFFFULL;
  static bool fn_pressed = false;
  static uint16_t last_consumer = 0;

  // 从 Core1 共享数据读取稳定按键状态（硬件扫描和消抖在 Core1 完成）
  uint64_t stable_keys = shared_hw_get_keys();

  // 更新全局稳定按键状态
  g_stable_keys = stable_keys;

  // 按键状态变化或宏按键状态变化时
  bool phy_changed = (stable_keys != last_stable);
  bool macro_changed = macro_kb_has_changed();

  if (phy_changed || macro_changed)
  {
    if (phy_changed)
    {
      // 检测Fn键状态变化
      bool fn_now = ((stable_keys >> keymap_get_fn_key()) & 1ULL) == 0ULL;
      if (fn_now != fn_pressed)
      {
        fn_pressed = fn_now;
      }

      // 检测DPI切换键（数字3键，索引3）的按下上升沿
      bool key3_now = ((stable_keys >> DPI_SWITCH_KEY_INDEX) & 1ULL) == 0ULL;
      bool key3_last = ((last_stable >> DPI_SWITCH_KEY_INDEX) & 1ULL) == 0ULL;
      if (key3_now && !key3_last)
      {
        dpi_cycle_next();
      }

      // 按键统计：检测所有键的按下上升沿，增加计数
      for (int i = 0; i < 64; i++)
      {
        bool now = ((stable_keys >> i) & 1ULL) == 0ULL;
        bool last = ((last_stable >> i) & 1ULL) == 0ULL;
        if (now && !last)
        {
          key_stats_increment(i);
          power_manager_notify_activity();
        }
      }

      // 宏触发检测：只检查状态变化的键（上升沿触发，下降沿停止）
      {
        uint64_t changed_keys = stable_keys ^ last_stable;
        while (changed_keys)
        {
          int i = __builtin_ctzll(changed_keys);  // 找到最低位变化的键
          changed_keys &= changed_keys - 1;       // 清除最低位

          bool now = ((stable_keys >> i) & 1ULL) == 0ULL;
          bool last = ((last_stable >> i) & 1ULL) == 0ULL;
          uint8_t macro_id = macro_find_by_trigger_key((uint8_t)i);
          if (macro_id != 0xFF)
          {
            if (now && !last)
            {
              macro_trigger(macro_id);
            }
            else if (!now && last)
            {
              macro_stop(macro_id);
            }
          }
        }
      }

      // 串口打印：按下的键数量
      uint8_t pressed_count = 0;
      for (int i = 0; i < 64; i++)
      {
        if (((stable_keys >> i) & 1ULL) == 0ULL)
        {
          pressed_count++;
        }
      }
      /* 临时关闭频繁打印
      printf("[键盘] 按下 %d 个键 (Fn=%s) stable=0x%016llX\n", pressed_count, fn_pressed ? "是" : "否", (unsigned long long)stable_keys);
      */
    }

    // 发送HID键盘报告
    if (tud_hid_ready())
    {
      uint8_t keycode[6] = { 0 };
      uint8_t key_count = 0;
      uint8_t modifier = 0;
      uint16_t consumer_code = 0;

      // 遍历64个键，收集按下的键（最多6个，标准6KRO）
      for (int i = 0; i < 64; i++)
      {
        if (((stable_keys >> i) & 1ULL) == 0ULL) // bit=0表示按下
        {
          // 宏触发键不作为普通按键发送
          if (macro_find_by_trigger_key((uint8_t)i) != 0xFF)
          {
            continue;
          }

          keymap_result_t result;
          if (!keymap_lookup(i, fn_pressed, &result))
          {
            continue; // 空键跳过
          }

          switch (result.type)
          {
            case KEYMAP_TYPE_MODIFIER:
              modifier |= result.code;
              break;

            case KEYMAP_TYPE_NORMAL:
              if (key_count < 6)
              {
                keycode[key_count++] = result.code;
              }
              break;

            case KEYMAP_TYPE_FN:
              // Fn键本身不输出
              break;

            case KEYMAP_TYPE_CONSUMER:
              // 多媒体键：只取第一个按下的
              if (consumer_code == 0)
              {
                consumer_code = result.code;
              }
              break;

            default:
              break;
          }
        }
      }

      // 合并宏的按键状态
      uint8_t macro_mod = 0;
      uint8_t macro_keys[6] = {0};
      macro_get_keyboard_state(&macro_mod, macro_keys);

      // 合并修饰键
      modifier |= macro_mod;

      // 合并普通按键（去重）
      for (int i = 0; i < 6; i++)
      {
        if (macro_keys[i] == 0) continue;

        // 检查是否已经存在
        bool exists = false;
        for (int j = 0; j < key_count; j++)
        {
          if (keycode[j] == macro_keys[i])
          {
            exists = true;
            break;
          }
        }

        // 不存在且还有槽位，就添加
        if (!exists && key_count < 6)
        {
          keycode[key_count++] = macro_keys[i];
        }
      }

      tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycode);

      // 发送Consumer多媒体报告（状态变化时）
      if (consumer_code != last_consumer)
      {
        uint8_t consumer_report[2];
        consumer_report[0] = (uint8_t)(consumer_code & 0xFF);
        consumer_report[1] = (uint8_t)((consumer_code >> 8) & 0xFF);
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, consumer_report, 2);
        last_consumer = consumer_code;
      }
    }

    if (phy_changed)
    {
      last_stable = stable_keys;
    }
  }
}









































