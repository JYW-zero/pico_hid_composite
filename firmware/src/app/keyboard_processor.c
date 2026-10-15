/*
 * src/app/keyboard_processor.c
 * 键盘处理模块实现
 * 从 main.c 提取，包含 NKRO 模式控制和报告生成
 */
#include "app/keyboard_processor.h"
#include "board/config.h"
#include "app/keymap.h"
#include "protocol/usb_descriptors.h"
#include "tusb.h"
#include <string.h>
#include <stdio.h>

/* ==================== 私有数据 ==================== */

/* NKRO 模式开关（默认关闭，使用标准 6KRO） */
static bool s_nkro_enabled = false;

/* NKRO 报告缓冲区（35字节: Report ID + Modifier + Reserved + 32字节位图） */
static uint8_t s_nkro_report[KEYBOARD_NKRO_REPORT_SIZE];

/* ==================== 初始化 ==================== */

void keyboard_processor_init(void)
{
  memset(s_nkro_report, 0, sizeof(s_nkro_report));
  s_nkro_enabled = false;
}

/* ==================== NKRO 模式控制 ==================== */

void keyboard_set_nkro_enabled(bool enabled)
{
  s_nkro_enabled = enabled;
  printf("[KEYBOARD] NKRO 模式: %s\n", enabled ? "启用" : "禁用");
}

bool keyboard_get_nkro_enabled(void)
{
  return s_nkro_enabled;
}

/* ==================== NKRO 报告生成与发送 ==================== */

/* 生成 NKRO 位图报告
 * @param stable_keys 稳定按键状态位图（bit=0 表示按下）
 * @param modifier 修饰键位掩码
 */
static void build_nkro_report(uint64_t stable_keys, uint8_t modifier)
{
  /* 清空报告缓冲区 */
  memset(s_nkro_report, 0, sizeof(s_nkro_report));

  /* Report ID */
  s_nkro_report[0] = REPORT_ID_NKRO;

  /* 修饰键字节 */
  s_nkro_report[1] = modifier;

  /* 保留字节（已清零） */

  /* 键码位图：遍历64个物理键，查找对应的 HID 键码，设置对应 bit */
  const device_config_t *cfg = config_get();
  bool fn_pressed = ((stable_keys >> keymap_get_fn_key()) & 1ULL) == 0ULL;

  for (int i = 0; i < 64; i++)
  {
    if (((stable_keys >> i) & 1ULL) == 0ULL)  /* bit=0 表示按下 */
    {
      /* 跳过 Fn 键本身 */
      if (i == keymap_get_fn_key()) continue;

      /* 获取 HID 键码（根据 Fn 状态选择标准层或 Fn 层） */
      uint8_t keycode = fn_pressed ? cfg->fn_keymap[i] : cfg->keymap[i];

      /* 跳过无效键码和修饰键（修饰键已在 modifier 中处理） */
      if (keycode == 0xFF || keycode == 0x00) continue;
      if (keycode >= 0xE0 && keycode <= 0xE7) continue;  /* 修饰键 */

      /* 设置位图中对应的 bit（keycode 0-255 对应 32 字节位图） */
      if (keycode < 256)
      {
        uint8_t byte_index = 2 + (keycode / 8);  /* 偏移2: Report ID + Modifier + Reserved */
        uint8_t bit_index = keycode % 8;
        if (byte_index < sizeof(s_nkro_report))
        {
          s_nkro_report[byte_index] |= (1U << bit_index);
        }
      }
    }
  }
}

void keyboard_send_nkro_report(uint64_t stable_keys, uint8_t modifier)
{
  if (!s_nkro_enabled) return;

  build_nkro_report(stable_keys, modifier);
  tud_hid_report(REPORT_ID_NKRO, s_nkro_report + 1, KEYBOARD_NKRO_REPORT_DATA_SIZE);
}
