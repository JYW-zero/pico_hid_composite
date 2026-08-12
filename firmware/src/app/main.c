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
#include "app/keymap.h"
#include "app/macro.h"
#include "app/key_stats.h"
#include "app/factory_test.h"
#include "middleware/shared_hw_data.h"
#include "middleware/ipc.h"
#include "app/core1_scanner.h"
#include "pico/multicore.h"

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
static int16_t g_mouse_dx = 0;  /* 累积的X位移 */
static int16_t g_mouse_dy = 0;  /* 累积的Y位移 */
static uint8_t g_mouse_buttons = 0; /* 鼠标按键位掩码 */

/* ==================== 滚轮编码器相关 ==================== */
static const encoder_cfg_t *encoder_cfg;
static encoder_state_t encoder_state;
static int8_t g_wheel = 0;  /* 累积的滚轮步数 */

/* ==================== PS2摇杆相关 ==================== */
static const joystick_cfg_t *joystick_cfg;
static int16_t g_joy_x = 0; /* 摇杆X轴 (-127 ~ 127) */
static int16_t g_joy_y = 0; /* 摇杆Y轴 (-127 ~ 127) */
static bool g_joy_btn = false; /* 摇杆按键 */

/* ==================== 64键键盘映射表 ====================
 * 索引0~63 对应 物理键1~64
 * 值为HID按键码，0xFF表示修饰键（单独处理）

void led_blinking_task(void);
void hid_task(void);
void keypad_task(void);
void paw3395_task(void);
void encoder_task(void);
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

    int ret = paw3395_set_dpi(paw3395_cfg, new_dpi_enum);
    if (ret == 0)
    {
        printf("[DPI] 切换到: %d DPI\n", new_dpi_val);

        /* 保存到Flash配置 */
        device_config_t cfg = *config_get();
        cfg.dpi = new_dpi_val;
        config_save(&cfg);
    }
    else
    {
        printf("[DPI] 切换失败: %d\n", ret);
    }
}

/* ==================== 任务函数声明 ==================== */
static void led_blinking_task(void);
static void hid_task(void);
static void keypad_task(void);
static void paw3395_task(void);
static void encoder_task(void);
static void mouse_hid_task(void);
static void joystick_task(void);

/* ==================== 调度器任务列表 ==================== */
/* 任务列表：按优先级排序（高优先级在前）
 * 注意：任务函数内部不再做时间判断，统一由调度器管理
 */
static sched_task_t g_task_list[] =
{
    /* 看门狗巡检：1ms */
    {.interval_us = 1000,  .last_run_us = 0, .task_func = watchdog_tick},

    /* 编码器扫描：1ms */
    {.interval_us = 1000,  .last_run_us = 0, .task_func = encoder_task},

    /* 鼠标HID发送：1ms（1000Hz回报率） */
    {.interval_us = 1000,  .last_run_us = 0, .task_func = mouse_hid_task},

    /* PAW3395传感器读取：2ms（500Hz轮询） */
    {.interval_us = 2000,  .last_run_us = 0, .task_func = paw3395_task},

    /* 键盘扫描：5ms（200Hz） */
    {.interval_us = 5000,  .last_run_us = 0, .task_func = keypad_task},

    /* 摇杆读取：10ms（100Hz） */
    {.interval_us = 10000, .last_run_us = 0, .task_func = joystick_task},

    /* LED闪烁：10ms（内部有自己的时间判断） */
    {.interval_us = 10000, .last_run_us = 0, .task_func = led_blinking_task},
};

#define TASK_COUNT (sizeof(g_task_list) / sizeof(g_task_list[0]))

/*------------- MAIN -------------*/

// 函数前向声明
static void hid_config_task(void);

int main(void)
{
  board_init();

  // 初始化UART串口（默认GP0=TX, GP1=RX，波特率115200）
  stdio_init_all();
  printf("\n=== Pico2 HID 复合设备启动 ===\n");
  printf("USB 设备栈初始化中...\n");

  // 初始化板级硬件
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

  // 设置默认超时阈值（微秒）
  perf_set_threshold(0, 1000);   // tud_task: 1ms
  perf_set_threshold(1, 2000);   // hid_config: 2ms
  perf_set_threshold(2, 500);    // macro_task: 0.5ms
  perf_set_threshold(3, 500);    // scheduler: 0.5ms

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
    // 读取几次按键，确保稳定
    uint64_t keys = 0;
    int fn_press_count = 0;
    uint8_t fn_key = keymap_get_fn_key();
    for (int i = 0; i < 20; i++)
    {
      keypad_spi_read_u64(keypad_cfg, &keys);
      if (((keys >> fn_key) & 1ULL) == 0ULL)  // 低电平有效
      {
        fn_press_count++;
      }
      sleep_ms(5);
    }
    // 如果超过15次检测到Fn键按下，就进入工厂测试模式
    if (fn_press_count > 15)
    {
      printf("\n========================================\n");
      printf("  检测到Fn键按下，进入工厂测试模式！\n");
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

    /* 从配置加载DPI */
    uint16_t dpi_val = config_get()->dpi;
    paw3395_dpi_e dpi_enum = dpi_val_to_enum(dpi_val);
    paw3395_set_dpi(paw3395_cfg, dpi_enum);
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

    /* 主循环正常运行，喂BOARD层和APP层 */
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
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
  // skip if hid is not ready yet
  if ( !tud_hid_ready() ) return;

  switch(report_id)
  {
    case REPORT_ID_KEYBOARD:
    {
      /* 键盘报告由 keypad_task() 统一发送（使用 keymap 正确映射 + 宏合并）
       * 此处跳过，避免重复发送硬编码索引映射的错误报告 */
    }
    break;

    case REPORT_ID_MOUSE:
    {
      /* 鼠标报告由 mouse_hid_task() 统一发送，此处跳过 */
    }
    break;

    case REPORT_ID_CONSUMER_CONTROL:
    {
      // use to avoid send multiple consecutive zero report
      static bool has_consumer_key = false;

      if ( btn )
      {
        // volume down
        uint16_t volume_down = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_down, 2);
        has_consumer_key = true;
      }else
      {
        // send empty key report (release key) if previously has key pressed
        uint16_t empty_key = 0;
        if (has_consumer_key) tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &empty_key, 2);
        has_consumer_key = false;
      }
    }
    break;

    case REPORT_ID_GAMEPAD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_gamepad_key = false;

      hid_gamepad_report_t report =
      {
        .x   = 0, .y = 0, .z = 0, .rz = 0, .rx = 0, .ry = 0,
        .hat = 0, .buttons = 0
      };

      if ( btn )
      {
        report.hat = GAMEPAD_HAT_UP;
        report.buttons = GAMEPAD_BUTTON_A;
        tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));

        has_gamepad_key = true;
      }else
      {
        report.hat = GAMEPAD_HAT_CENTERED;
        report.buttons = 0;
        if (has_gamepad_key) tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
        has_gamepad_key = false;
      }
    }
    break;

    default: break;
  }
}

#if TUSB_VERSION_NUMBER > 1800
// board_millis has been removed from tinyusb. Use tusb_time_millis_api instead
#define board_millis tusb_time_millis_api
#endif

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
static void hid_task(void)
{
  // Poll every 10ms
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  uint32_t const btn = board_button_read();

  // Remote wakeup
  if ( tud_suspended() && btn )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }else
  {
    // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
    send_hid_report(REPORT_ID_KEYBOARD, btn);
  }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) len;

  uint8_t next_report_id = report[0] + 1u;

  if (next_report_id < REPORT_ID_COUNT)
  {
    send_hid_report(next_report_id, board_button_read());
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request

// ==================== USB 配置协议 ====================

// 临时配置缓冲区
static device_config_t g_tmp_config;
static volatile bool g_config_pending = false;
static volatile uint8_t g_pending_cmd = 0;

// 按键统计（已改用 key_stats 模块，Flash持久化 + 磨损均衡）
// static uint32_t g_key_stats[64] = {0};

// 宏配置读取索引
static uint8_t s_macro_read_id = 0;
static uint8_t s_macro_read_block = 0;

// 错误日志读取索引
static uint8_t s_fault_read_index = 0;

// 性能监控 - 任务读取索引
static uint8_t s_perf_task_index = 0;

// 宏配置脏标志（需要保存到Flash）
static bool s_macro_dirty = false;
static uint32_t s_macro_dirty_time_us = 0;

// 控制命令码
#define CMD_SAVE_CONFIG   0x01
#define CMD_RESET_CONFIG  0x02
#define CMD_REBOOT        0x03
#define CMD_ENTER_DFU     0x04
#define CMD_APPLY_CONFIG  0x05  /* 应用临时配置（写完所有块后调用） */
#define CMD_RESET_STATS   0x06  /* 清零按键统计 */
#define CMD_CLEAR_FAULT   0x07  /* 清除错误日志 */
#define CMD_RESET_PERF    0x08  /* 重置性能统计 */

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // 只有配置接口（接口 1）处理 Feature 报告
  if (instance != 1) {
    return 0;
  }

  if (report_type != HID_REPORT_TYPE_FEATURE) {
    return 0;
  }

  printf("[CONFIG] 主机读取 Feature 报告: Report ID=%d, 请求长度=%d\n", report_id, reqlen);

  switch (report_id)
  {
    case REPORT_ID_CONFIG_BLOCK0:
    case REPORT_ID_CONFIG_BLOCK1:
    case REPORT_ID_CONFIG_BLOCK2:
    {
      const device_config_t* cfg = config_get();
      const uint8_t* cfg_bytes = (const uint8_t*)cfg;
      uint16_t cfg_size = sizeof(device_config_t);

      // 计算块偏移
      uint16_t offset;
      switch (report_id) {
        case REPORT_ID_CONFIG_BLOCK0: offset = 0; break;
        case REPORT_ID_CONFIG_BLOCK1: offset = CONFIG_BLOCK_SIZE; break;
        case REPORT_ID_CONFIG_BLOCK2: offset = CONFIG_BLOCK_SIZE * 2; break;
        default: return 0;
      }

      if (offset >= cfg_size) {
        // 偏移超出配置大小，返回 0
        memset(buffer, 0, reqlen);
        return (reqlen < CONFIG_BLOCK_SIZE) ? reqlen : CONFIG_BLOCK_SIZE;
      }

      // 计算要复制的长度
      uint16_t remaining = cfg_size - offset;
      uint16_t copy_len = (reqlen < remaining) ? reqlen : remaining;
      if (copy_len > CONFIG_BLOCK_SIZE) copy_len = CONFIG_BLOCK_SIZE;

      if (copy_len > 0) {
        memcpy(buffer, cfg_bytes + offset, copy_len);
      }

      // 如果长度不足块大小，后面填 0
      if (copy_len < CONFIG_BLOCK_SIZE && reqlen > copy_len) {
        uint16_t fill_len = (reqlen < CONFIG_BLOCK_SIZE) ? reqlen : CONFIG_BLOCK_SIZE;
        memset(buffer + copy_len, 0, fill_len - copy_len);
        copy_len = fill_len;
      }

      return copy_len;
    }

    case REPORT_ID_DEVICE_INFO:
    {
      // 设备信息：固件版本 + 硬件版本 + 配置大小等
      buffer[0] = 1;  // 固件版本主版本
      buffer[1] = 0;  // 固件版本次版本
      buffer[2] = 0;  // 固件版本修订版本
      buffer[3] = 1;  // 硬件版本主版本
      buffer[4] = 0;  // 硬件版本次版本
      buffer[5] = (uint8_t)(sizeof(device_config_t) & 0xFF);
      buffer[6] = (uint8_t)((sizeof(device_config_t) >> 8) & 0xFF);
      // 剩下的填 0
      for (int i = 7; i < 32 && i < reqlen; i++) {
        buffer[i] = 0;
      }
      return (reqlen > 32) ? 32 : reqlen;
    }

    case REPORT_ID_CONTROL:
    {
      // 返回当前状态：0=空闲，1=忙
      buffer[0] = 0;
      return 1;
    }

    case REPORT_ID_KEY_STATS0:
    case REPORT_ID_KEY_STATS1:
    case REPORT_ID_KEY_STATS2:
    case REPORT_ID_KEY_STATS3:
    {
      // 计算起始键索引（每个块16个键）
      int start_idx = (report_id - REPORT_ID_KEY_STATS0) * 16;
      int count = 16;
      if (start_idx + count > 64) count = 64 - start_idx;

      // 写入统计数据（uint16_t，低16位）
      for (int i = 0; i < count; i++)
      {
        uint32_t cnt = key_stats_get_count(start_idx + i);
        uint16_t val = (uint16_t)(cnt & 0xFFFF);
        buffer[i * 2] = (uint8_t)(val & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)((val >> 8) & 0xFF);
      }

      int data_len = count * 2;
      return (reqlen > data_len) ? data_len : reqlen;
    }

    case REPORT_ID_MACRO_CONFIG:
    {
      // 返回当前索引对应的宏数据块
      uint8_t macro_id = s_macro_read_id;
      uint8_t block = s_macro_read_block;

      if (macro_id >= MACRO_MAX_COUNT) macro_id = 0;
      if (block > 2) block = 0;

      const macro_def_t* macro = macro_get(macro_id);
      if (macro == NULL)
      {
        memset(buffer, 0, reqlen);
        return 0;
      }

      // 前两个字节：宏ID + 块号
      buffer[0] = macro_id;
      buffer[1] = block;

      // 计算偏移和长度（每块60字节数据）
      uint16_t offset = block * 60;
      const uint8_t* macro_bytes = (const uint8_t*)macro;
      uint16_t copy_len = 60;
      if (offset + copy_len > sizeof(macro_def_t))
      {
        copy_len = sizeof(macro_def_t) - offset;
      }

      // 复制数据
      if (copy_len > 0)
      {
        memcpy(&buffer[2], macro_bytes + offset, copy_len);
      }

      // 剩余填0
      if (copy_len < 60 && reqlen > 2 + copy_len)
      {
        memset(&buffer[2 + copy_len], 0, 60 - copy_len);
      }

      printf("[宏] 读取宏 %d 块 %d，长度 %d\n", macro_id, block, copy_len);

      // 返回62字节（2字节头部 + 60字节数据）
      return (reqlen > 62) ? 62 : reqlen;
    }

    case REPORT_ID_PERF_SYSTEM:
    {
      // 性能监控 - 系统状态
      perf_system_stat_t sys_stat;
      perf_get_system_stat(&sys_stat);

      // 格式：
      // 偏移0: cpu_usage (uint8) - 瞬时CPU使用率
      // 偏移1-2: loop_freq_hz (uint16) - 瞬时主循环频率
      // 偏移3-6: uptime_s (uint32) - 运行时间
      // 偏移7: task_count (uint8) - 任务数量
      // 偏移8: cpu_usage_avg_10s (uint8) - 10秒平均CPU使用率
      // 偏移9: cpu_usage_avg_30s (uint8) - 30秒平均CPU使用率
      // 偏移10-11: loop_freq_avg_10s (uint16) - 10秒平均主循环频率
      buffer[0] = (uint8_t)(sys_stat.cpu_usage & 0xFF);
      buffer[1] = (uint8_t)(sys_stat.loop_freq_hz & 0xFF);
      buffer[2] = (uint8_t)((sys_stat.loop_freq_hz >> 8) & 0xFF);
      buffer[3] = (uint8_t)(sys_stat.uptime_s & 0xFF);
      buffer[4] = (uint8_t)((sys_stat.uptime_s >> 8) & 0xFF);
      buffer[5] = (uint8_t)((sys_stat.uptime_s >> 16) & 0xFF);
      buffer[6] = (uint8_t)((sys_stat.uptime_s >> 24) & 0xFF);
      buffer[7] = perf_get_task_count();
      buffer[8] = sys_stat.cpu_usage_avg_10s;
      buffer[9] = sys_stat.cpu_usage_avg_30s;
      buffer[10] = (uint8_t)(sys_stat.loop_freq_avg_10s & 0xFF);
      buffer[11] = (uint8_t)((sys_stat.loop_freq_avg_10s >> 8) & 0xFF);

      // 剩余填0
      memset(&buffer[12], 0, reqlen > 12 ? reqlen - 12 : 0);

      return (reqlen > 62) ? 62 : reqlen;
    }

    case REPORT_ID_PERF_TASK:
    {
      // 性能监控 - 任务统计
      uint8_t index = s_perf_task_index;
      perf_task_stat_t task_stat;
      bool valid = perf_get_task_stat(index, &task_stat);

      // 格式：
      // 偏移0: index (uint8) - 任务索引
      // 偏移1: valid (uint8) - 0=无效, 1=有效
      // 偏移2-5: count (uint32) - 执行次数
      // 偏移6-9: min_us (uint32) - 最小执行时间（微秒）
      // 偏移10-13: max_us (uint32) - 最大执行时间（微秒）
      // 偏移14-17: avg_us (uint32) - 平均执行时间（微秒）
      // 偏移18-21: last_us (uint32) - 最近执行时间（微秒）
      // 偏移22: cpu_percent (uint8) - CPU占比（0-100）
      // 偏移23-26: overrun_count (uint32) - 超时次数
      // 偏移27-30: threshold_us (uint32) - 超时阈值（微秒）
      // 偏移31-61: name (char[31]) - 任务名称
      buffer[0] = index;
      buffer[1] = valid ? 1 : 0;

      if (valid)
      {
        uint32_t avg_us = task_stat.count > 0 ? (task_stat.total_us / task_stat.count) : 0;

        buffer[2] = (uint8_t)(task_stat.count & 0xFF);
        buffer[3] = (uint8_t)((task_stat.count >> 8) & 0xFF);
        buffer[4] = (uint8_t)((task_stat.count >> 16) & 0xFF);
        buffer[5] = (uint8_t)((task_stat.count >> 24) & 0xFF);

        buffer[6] = (uint8_t)(task_stat.min_us & 0xFF);
        buffer[7] = (uint8_t)((task_stat.min_us >> 8) & 0xFF);
        buffer[8] = (uint8_t)((task_stat.min_us >> 16) & 0xFF);
        buffer[9] = (uint8_t)((task_stat.min_us >> 24) & 0xFF);

        buffer[10] = (uint8_t)(task_stat.max_us & 0xFF);
        buffer[11] = (uint8_t)((task_stat.max_us >> 8) & 0xFF);
        buffer[12] = (uint8_t)((task_stat.max_us >> 16) & 0xFF);
        buffer[13] = (uint8_t)((task_stat.max_us >> 24) & 0xFF);

        buffer[14] = (uint8_t)(avg_us & 0xFF);
        buffer[15] = (uint8_t)((avg_us >> 8) & 0xFF);
        buffer[16] = (uint8_t)((avg_us >> 16) & 0xFF);
        buffer[17] = (uint8_t)((avg_us >> 24) & 0xFF);

        buffer[18] = (uint8_t)(task_stat.last_us & 0xFF);
        buffer[19] = (uint8_t)((task_stat.last_us >> 8) & 0xFF);
        buffer[20] = (uint8_t)((task_stat.last_us >> 16) & 0xFF);
        buffer[21] = (uint8_t)((task_stat.last_us >> 24) & 0xFF);

        buffer[22] = task_stat.cpu_percent;

        buffer[23] = (uint8_t)(task_stat.overrun_count & 0xFF);
        buffer[24] = (uint8_t)((task_stat.overrun_count >> 8) & 0xFF);
        buffer[25] = (uint8_t)((task_stat.overrun_count >> 16) & 0xFF);
        buffer[26] = (uint8_t)((task_stat.overrun_count >> 24) & 0xFF);

        buffer[27] = (uint8_t)(task_stat.threshold_us & 0xFF);
        buffer[28] = (uint8_t)((task_stat.threshold_us >> 8) & 0xFF);
        buffer[29] = (uint8_t)((task_stat.threshold_us >> 16) & 0xFF);
        buffer[30] = (uint8_t)((task_stat.threshold_us >> 24) & 0xFF);

        // 任务名称（最多31字节）
        if (task_stat.name != NULL)
        {
          uint8_t name_len = 0;
          while (task_stat.name[name_len] != '\0' && name_len < 30)
          {
            buffer[31 + name_len] = (uint8_t)task_stat.name[name_len];
            name_len++;
          }
          buffer[31 + name_len] = 0;
        }
        else
        {
          buffer[31] = 0;
        }
      }

      // 剩余填0
      if (reqlen > 31)
      {
        memset(&buffer[31], 0, reqlen - 31);
      }

      return (reqlen > 62) ? 62 : reqlen;
    }

    case REPORT_ID_FAULT_INFO:
    {
      // 错误日志 - 信息
      uint32_t log_count = fault_get_log_count();
      uint32_t total_count = fault_get_count();

      // 格式：
      // 偏移0-3: log_count (uint32) - 当前日志条数
      // 偏移4-7: total_count (uint32) - 总故障计数
      buffer[0] = (uint8_t)(log_count & 0xFF);
      buffer[1] = (uint8_t)((log_count >> 8) & 0xFF);
      buffer[2] = (uint8_t)((log_count >> 16) & 0xFF);
      buffer[3] = (uint8_t)((log_count >> 24) & 0xFF);
      buffer[4] = (uint8_t)(total_count & 0xFF);
      buffer[5] = (uint8_t)((total_count >> 8) & 0xFF);
      buffer[6] = (uint8_t)((total_count >> 16) & 0xFF);
      buffer[7] = (uint8_t)((total_count >> 24) & 0xFF);

      // 剩余填0
      memset(&buffer[8], 0, reqlen > 8 ? reqlen - 8 : 0);

      return (reqlen > 62) ? 62 : reqlen;
    }

    case REPORT_ID_FAULT_LOG:
    {
      // 错误日志 - 读取指定索引的日志
      uint8_t index = s_fault_read_index;
      fault_log_entry_t entry;
      bool valid = fault_get_log(index, &entry);

      // 格式：
      // 偏移0: index (uint8)
      // 偏移1: valid (uint8) - 0=无效, 1=有效
      // 偏移2-5: timestamp_ms (uint32)
      // 偏移6: level (uint8)
      // 偏移7: module_len (uint8)
      // 偏移8-39: module (char[32])
      // 偏移40-61: msg (char[22])
      buffer[0] = index;
      buffer[1] = valid ? 1 : 0;

      if (valid)
      {
        buffer[2] = (uint8_t)(entry.timestamp_ms & 0xFF);
        buffer[3] = (uint8_t)((entry.timestamp_ms >> 8) & 0xFF);
        buffer[4] = (uint8_t)((entry.timestamp_ms >> 16) & 0xFF);
        buffer[5] = (uint8_t)((entry.timestamp_ms >> 24) & 0xFF);
        buffer[6] = (uint8_t)entry.level;
        buffer[7] = entry.module_len;

        // 复制模块名（最多32字节）
        int module_len = entry.module_len;
        if (module_len > 31) module_len = 31;
        memcpy(&buffer[8], entry.module, module_len);
        buffer[8 + module_len] = 0;

        // 复制消息（最多22字节，截断）
        int msg_len = entry.msg_len;
        if (msg_len > 21) msg_len = 21;
        memcpy(&buffer[40], entry.msg, msg_len);
        buffer[40 + msg_len] = 0;
      }
      else
      {
        memset(&buffer[2], 0, reqlen > 2 ? reqlen - 2 : 0);
      }

      return (reqlen > 62) ? 62 : reqlen;
    }

    default:
      return 0;
  }
}


// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  // 接口 0：标准 HID 功能（键盘、鼠标等）
  if (instance == 0)
  {
    // 处理 Output 报告（键盘 LED 等）
    if (report_type == HID_REPORT_TYPE_OUTPUT)
    {
      if (report_id == REPORT_ID_KEYBOARD)
      {
        // TinyUSB 已经去掉了 Report ID，buffer[0] 就是数据
        if ( bufsize < 1 ) return;
        uint8_t const kbd_leds = buffer[0];
        if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
        {
          blink_interval_ms = 0;
          board_led_write(true);
        } else {
          board_led_write(false);
          blink_interval_ms = BLINK_MOUNTED;
        }
      }
    }
    return;
  }

  // 接口 1：配置接口
  if (instance == 1)
  {
    // 处理 Feature 报告（配置协议）
    if (report_type == HID_REPORT_TYPE_FEATURE)
    {
      switch (report_id)
      {
        case REPORT_ID_CONFIG_BLOCK0:
        case REPORT_ID_CONFIG_BLOCK1:
        case REPORT_ID_CONFIG_BLOCK2:
        {
          // 写入配置块到临时缓冲区
          // TinyUSB 已经去掉了 Report ID，buffer[0] 就是数据
          uint8_t* cfg_bytes = (uint8_t*)&g_tmp_config;
          uint16_t cfg_size = sizeof(device_config_t);

          // 计算块偏移
          uint16_t offset;
          switch (report_id) {
            case REPORT_ID_CONFIG_BLOCK0: offset = 0; break;
            case REPORT_ID_CONFIG_BLOCK1: offset = CONFIG_BLOCK_SIZE; break;
            case REPORT_ID_CONFIG_BLOCK2: offset = CONFIG_BLOCK_SIZE * 2; break;
            default: break;
          }

          printf("[CONFIG] 收到配置块 %d (Report ID=%d, 偏移=%d, 长度=%d)\n",
                 report_id - 5, report_id, offset, bufsize);

          if (offset < cfg_size)
          {
            uint16_t remaining = cfg_size - offset;
            uint16_t copy_len = (bufsize < remaining) ? bufsize : remaining;
            if (copy_len > CONFIG_BLOCK_SIZE) copy_len = CONFIG_BLOCK_SIZE;

            if (copy_len > 0)
            {
              memcpy(cfg_bytes + offset, buffer, copy_len);
            }
            printf("[CONFIG] 配置块 %d 写入成功，复制 %d 字节\n", report_id - 5, copy_len);
          }
          break;
        }

        case REPORT_ID_CONTROL:
        {
          // 控制命令
          // TinyUSB 已经去掉了 Report ID，buffer[0] 就是命令
          if (bufsize >= 1)
          {
            g_pending_cmd = buffer[0];
            printf("[CONFIG] 收到控制命令: 0x%02X\n", buffer[0]);
          }
          break;
        }

        case REPORT_ID_MACRO_CONFIG:
        {
          // 宏配置读写
          if (bufsize < 2) break;

          uint8_t macro_id = buffer[0];
          uint8_t block_raw = buffer[1];
          bool is_read_cmd = (block_raw & 0x80) != 0;  // 最高位为1表示设置读取索引
          uint8_t block = block_raw & 0x7F;

          printf("[宏] 收到宏配置: 宏ID=%d, 块=%d, 长度=%d, 读命令=%d\n", 
                 macro_id, block, bufsize, is_read_cmd);

          if (macro_id >= MACRO_MAX_COUNT) break;
          if (block > 2) break;

          // 更新读取索引（方便后续GET_REPORT）
          s_macro_read_id = macro_id;
          s_macro_read_block = block;

          // 如果是写入命令（最高位为0）且数据长度大于2，说明是写入数据
          if (!is_read_cmd && bufsize > 2)
          {
            // 读取当前宏
            macro_def_t tmp_macro;
            const macro_def_t* current = macro_get(macro_id);
            if (current != NULL)
            {
              memcpy(&tmp_macro, current, sizeof(macro_def_t));
            }
            else
            {
              memset(&tmp_macro, 0, sizeof(macro_def_t));
              tmp_macro.id = macro_id;
            }

            // 计算偏移和长度
            uint16_t offset = block * 60;
            uint8_t* macro_bytes = (uint8_t*)&tmp_macro;
            uint16_t copy_len = bufsize - 2;  // 减去前2字节（宏ID+块号）
            if (offset + copy_len > sizeof(macro_def_t))
            {
              copy_len = sizeof(macro_def_t) - offset;
            }

            if (copy_len > 0)
            {
              memcpy(macro_bytes + offset, &buffer[2], copy_len);
            }

            // 确保ID正确
            tmp_macro.id = macro_id;

            // 限制动作数量
            if (tmp_macro.action_count > MACRO_MAX_ACTIONS)
            {
              tmp_macro.action_count = MACRO_MAX_ACTIONS;
            }

            // 保存
            macro_set(macro_id, &tmp_macro);

            // 设置脏标志，延迟保存到Flash
            s_macro_dirty = true;
            s_macro_dirty_time_us = time_us_32();

            printf("[宏] 写入宏配置: 宏ID=%d, 块=%d, 长度=%d\n", macro_id, block, copy_len);
          }

          break;
        }

        case REPORT_ID_FAULT_LOG:
        {
          // 错误日志 - 设置读取索引
          if (bufsize >= 1)
          {
            s_fault_read_index = buffer[0];
            printf("[FAULT] 设置日志读取索引: %d\n", s_fault_read_index);
          }
          break;
        }

        case REPORT_ID_PERF_TASK:
        {
          // 性能监控 - 设置任务读取索引
          if (bufsize >= 1)
          {
            s_perf_task_index = buffer[0];
            printf("[PERF] 设置任务读取索引: %d\n", s_perf_task_index);
          }
          break;
        }

        default:
          printf("[CONFIG] 未知 Report ID: %d\n", report_id);
          break;
      }
    }
  }
}

// 配置协议处理任务（在主循环中调用）
static void hid_config_task(void)
{
  // 处理宏配置的延迟保存（修改后1秒再写入Flash，避免频繁写入）
  if (s_macro_dirty)
  {
    uint32_t now = time_us_32();
    if (now - s_macro_dirty_time_us > 1000000U)  // 1秒延迟
    {
      const device_config_t* current_cfg = config_get();
      if (current_cfg != NULL)
      {
        // 复制当前配置
        device_config_t new_cfg;
        memcpy(&new_cfg, current_cfg, sizeof(device_config_t));

        // 保存宏配置到配置结构体
        macro_save_to_config(new_cfg.macro_data, CONFIG_MACRO_DATA_SIZE);

        // 保存到Flash
        config_save(&new_cfg);
        printf("[宏] 配置已保存到Flash\n");
      }
      s_macro_dirty = false;
    }
  }

  // 处理待写入的配置
  if (g_config_pending)
  {
    g_config_pending = false;
    // 验证魔数
    if (g_tmp_config.magic == CONFIG_MAGIC)
    {
      // TODO: 应用配置到各个模块（DPI、死区、按键映射等）
      // 现在只是保存到临时缓冲区，等保存命令再写入 Flash
    }
  }

  // 处理待执行的命令
  if (g_pending_cmd != 0)
  {
    uint8_t cmd = g_pending_cmd;
    g_pending_cmd = 0;

    switch (cmd)
    {
      case CMD_SAVE_CONFIG:
        // 保存配置到 Flash
        if (g_tmp_config.magic == CONFIG_MAGIC)
        {
          config_save(&g_tmp_config);
        }
        break;

      case CMD_RESET_CONFIG:
        // 恢复默认配置（重新初始化）
        config_init();
        break;

      case CMD_REBOOT:
        // 重启设备
        printf("[CMD] 重启设备...\n");
        sleep_ms(10);
        watchdog_reboot(0, 0, 1);
        while (1) { }  // 等待复位生效
        break;

      case CMD_ENTER_DFU:
        // 进入 BOOTSEL 模式
        printf("[CMD] 进入 BOOTSEL 模式...\n");
        sleep_ms(10);
        // 重启到 USB 启动模式（BOOTSEL）
        rom_reset_usb_boot(0, 0);
        break;

      case CMD_APPLY_CONFIG:
        // 应用临时配置（写完所有配置块后调用）
        printf("[CONFIG] 收到应用配置命令\n");
        if (g_tmp_config.magic == CONFIG_MAGIC)
        {
          printf("[CONFIG] 魔数正确: 0x%08X\n", g_tmp_config.magic);
          printf("[CONFIG] 配置版本: %d\n", g_tmp_config.version);
          printf("[CONFIG] DPI: %d\n", g_tmp_config.dpi);
          printf("[CONFIG] 摇杆死区: %d\n", g_tmp_config.joystick_deadzone);
          printf("[CONFIG] 编码器反转: %d\n", g_tmp_config.encoder_reverse);

          // 保存到 Flash，同时更新当前配置
          int save_ret = config_save(&g_tmp_config);
          if (save_ret == 0)
          {
            printf("[CONFIG] ✅ 配置已保存到 Flash\n");

            // 重新应用配置到各个模块
            printf("[CONFIG] 正在应用配置到各个模块...\n");

            // 1. 应用 DPI 设置到 PAW3395 传感器
            if (paw3395_cfg != NULL)
            {
              paw3395_dpi_e dpi_enum = dpi_val_to_enum(g_tmp_config.dpi);
              int ret = paw3395_set_dpi(paw3395_cfg, dpi_enum);
              if (ret == 0)
              {
                printf("[CONFIG] ✅ DPI 已应用: %d\n", g_tmp_config.dpi);
              }
              else
              {
                printf("[CONFIG] ❌ DPI 应用失败，错误码: %d\n", ret);
              }
            }

            // 2. 按键映射：每次按键都从 config 读取，自动生效，无需额外操作
            // 3. 摇杆死区：每次处理都从 config 读取，自动生效，无需额外操作
            // 4. 编码器反转：每次处理都从 config 读取，自动生效，无需额外操作

            printf("[CONFIG] ✅ 配置应用完成\n");
          }
          else
          {
            printf("[CONFIG] ❌ 配置保存到 Flash 失败\n");
          }
        }
        else
        {
          printf("[CONFIG] ❌ 魔数错误: 0x%08X (期望 0x%08X)\n",
                 g_tmp_config.magic, CONFIG_MAGIC);
        }
        break;

      case CMD_RESET_STATS:
        // 清零按键统计
        key_stats_reset();
        printf("[STATS] 按键统计已清零\n");
        break;

      case CMD_CLEAR_FAULT:
        // 清除错误日志
        fault_clear();
        printf("[FAULT] 错误日志已清除\n");
        break;

      case CMD_RESET_PERF:
        // 重置性能统计
        perf_reset();
        printf("[PERF] 性能统计已重置\n");
        break;

      default:
        break;
    }
  }
}


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
// PAW3395鼠标传感器任务 - 只采集数据，不发送HID
//--------------------------------------------------------------------+
static void paw3395_task(void)
{
  paw3395_motion_t motion;
  if (paw3395_read_motion(paw3395_cfg, &motion) == 0)
  {
    if (motion.has_motion)
    {
      g_mouse_dx += motion.dx;
      g_mouse_dy += motion.dy;
    }
  }

  /* PAW3395任务正常运行，喂DEVICE层 */
  watchdog_feed_layer(WDG_LAYER_DEVICE);
}

//--------------------------------------------------------------------+
// 滚轮编码器任务 - 只采集数据，不发送HID
//--------------------------------------------------------------------+
static void encoder_task(void)
{
  static bool last_sw = false;
  static uint8_t sw_debounce = 0;

  // 更新编码器状态
  encoder_dir_e dir = encoder_update(encoder_cfg, &encoder_state);

  // 根据配置反转方向
  bool reverse = (config_get()->encoder_reverse != 0);
  if (reverse)
  {
    if (dir == ENCODER_DIR_CW)
    {
      dir = ENCODER_DIR_CCW;
    }
    else if (dir == ENCODER_DIR_CCW)
    {
      dir = ENCODER_DIR_CW;
    }
  }

  if (dir == ENCODER_DIR_CW)
  {
    g_wheel++;
    static uint32_t cw_count = 0;
    cw_count++;
    if (cw_count >= 4)  // 每4步打印一次，减少输出
    {
      printf("[滚轮] 顺时针\n");
      cw_count = 0;
    }
  }
  else if (dir == ENCODER_DIR_CCW)
  {
    g_wheel--;
    static uint32_t ccw_count = 0;
    ccw_count++;
    if (ccw_count >= 4)  // 每4步打印一次，减少输出
    {
      printf("[滚轮] 逆时针\n");
      ccw_count = 0;
    }
  }

  // 读取中键状态（带消抖，5次采样=5ms）
  bool sw = encoder_read_switch(encoder_cfg);
  if (sw != last_sw)
  {
    sw_debounce++;
    if (sw_debounce >= 5)
    {
      if (sw)
      {
        g_mouse_buttons |= MOUSE_BUTTON_MIDDLE;
      }
      else
      {
        g_mouse_buttons &= ~MOUSE_BUTTON_MIDDLE;
      }
      last_sw = sw;
      sw_debounce = 0;
    }
  }
  else
  {
    sw_debounce = 0;
  }
}

//--------------------------------------------------------------------+
// 鼠标HID统一发送任务 - 所有鼠标相关数据统一在这里发送
//--------------------------------------------------------------------+
static void mouse_hid_task(void)
{
  static uint8_t last_buttons = 0;

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
// PS2摇杆任务 - 游戏手柄
//--------------------------------------------------------------------+
static void joystick_task(void)
{
  static int8_t last_x = 0;
  static int8_t last_y = 0;
  static uint32_t last_btn = 0;

  joystick_data_t data;
  if (joystick_read(joystick_cfg, &data) != 0)
  {
    return;
  }

  // ADC值 0~4095 转换为 -127~127（中心值2048）
  int32_t x = (int32_t)data.x - 2048;
  int32_t y = (int32_t)data.y - 2048;

  // 从配置读取死区
  uint16_t deadzone = config_get()->joystick_deadzone;
  int32_t range = 2048 - (int32_t)deadzone;
  if (range < 100) range = 100; // 防止除零

  // 死区处理
  if (x > -(int32_t)deadzone && x < (int32_t)deadzone) x = 0;
  if (y > -(int32_t)deadzone && y < (int32_t)deadzone) y = 0;

  // 缩放至 -127~127
  x = (x * 127) / range;
  y = (y * 127) / range;

  // 限制范围
  if (x > 127) x = 127;
  if (x < -127) x = -127;
  if (y > 127) y = 127;
  if (y < -127) y = -127;

  int8_t joy_x = (int8_t)x;
  int8_t joy_y = (int8_t)(-y); // Y轴反转，上推为正
  uint32_t buttons = data.btn_pressed ? 0x00000001UL : 0x00000000UL; // 按键1

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
// 64键键盘任务
//--------------------------------------------------------------------+
static void keypad_task(void)
{
  static uint64_t last_stable = 0xFFFFFFFFFFFFFFFFULL;
  static bool fn_pressed = false;
  static uint16_t last_consumer = 0;

  // 读取原始按键值并消抖
  uint64_t raw_keys = 0;
  keypad_spi_read_u64(keypad_cfg, &raw_keys);
  uint64_t stable_keys = debounce_64key_update(&keypad_debounce, raw_keys);

  // 更新全局稳定按键状态
  g_stable_keys = stable_keys;

  // 按键状态变化时
  if (stable_keys != last_stable)
  {
    // 检测Fn键状态变化
    bool fn_now = ((stable_keys >> keymap_get_fn_key()) & 1ULL) == 0ULL;
    if (fn_now != fn_pressed)
    {
      fn_pressed = fn_now;
      printf("[Fn] %s\n", fn_now ? "按下" : "松开");
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
    printf("[键盘] 按下 %d 个键 (Fn=%s)\n", pressed_count, fn_pressed ? "是" : "否");

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

      // 合并宏的按键状态（与 send_hid_report 原逻辑一致）
      {
        uint8_t macro_mod = 0;
        uint8_t macro_keys[6] = {0};
        macro_get_keyboard_state(&macro_mod, macro_keys);
        modifier |= macro_mod;
        for (int mi = 0; mi < 6; mi++)
        {
          if (macro_keys[mi] == 0) continue;
          bool exists = false;
          for (int mj = 0; mj < key_count; mj++)
          {
            if (keycode[mj] == macro_keys[mi]) { exists = true; break; }
          }
          if (!exists && key_count < 6)
          {
            keycode[key_count++] = macro_keys[mi];
          }
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

        if (consumer_code != 0)
        {
          printf("[多媒体] 按下: 0x%04X\n", consumer_code);
        }
        else
        {
          printf("[多媒体] 松开\n");
        }
      }
    }
    
    last_stable = stable_keys;
  }
}









































