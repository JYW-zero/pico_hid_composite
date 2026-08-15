/*
 * src/app/keymap.c
 * 按键映射模块实现
 * 支持普通层 + Fn层 两层映射
 */

#include "app/keymap.h"
#include "board/config.h"
#include "middleware/fault.h"
#include "tusb.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ==================== 静态变量 ==================== */

static uint8_t s_fn_key_index = KEYMAP_DEFAULT_FN_INDEX;
static bool s_initialized = false;

/* 修饰键映射表：索引 -> 修饰键位掩码
 * 索引41 -> 左Shift (bit1)
 * 索引52 -> 右Shift (bit5)
 * 索引54 -> 左Ctrl (bit0)
 * 索引55 -> 左Win (bit3)
 * 索引56 -> 左Alt (bit2)
 * 索引58 -> 右Alt (bit6)
 */
static const uint8_t s_modifier_map[64] =
{
    /* 0-40 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 41: 左Shift */
    KEYBOARD_MODIFIER_LEFTSHIFT,
    /* 42-51 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 52: 右Shift */
    KEYBOARD_MODIFIER_RIGHTSHIFT,
    /* 53 */
    0,
    /* 54: 左Ctrl */
    KEYBOARD_MODIFIER_LEFTCTRL,
    /* 55: 左Win */
    KEYBOARD_MODIFIER_LEFTGUI,
    /* 56: 左Alt */
    KEYBOARD_MODIFIER_LEFTALT,
    /* 57 */
    0,
    /* 58: 右Alt */
    KEYBOARD_MODIFIER_RIGHTALT,
    /* 59-63 */
    0, 0, 0, 0, 0
};

/* 多媒体键映射表：配置值(0xF0-0xF7) -> Consumer Usage ID
 * 索引0对应0xF0，索引1对应0xF1，以此类推
 */
static const uint16_t s_consumer_map[KEYMAP_CONSUMER_COUNT] =
{
    CONSUMER_VOLUME_UP,     /* 0xF0: 音量+ */
    CONSUMER_VOLUME_DOWN,   /* 0xF1: 音量- */
    CONSUMER_MUTE,          /* 0xF2: 静音 */
    CONSUMER_PLAY_PAUSE,    /* 0xF3: 播放/暂停 */
    CONSUMER_SCAN_NEXT,     /* 0xF4: 下一曲 */
    CONSUMER_SCAN_PREV,     /* 0xF5: 上一曲 */
    CONSUMER_STOP,          /* 0xF6: 停止 */
    CONSUMER_EJECT,         /* 0xF7: 弹出 */
};

/* ==================== 对外接口 ==================== */

void keymap_init(void)
{
    /* 从配置加载Fn键索引（暂时用默认值，以后可以配置） */
    s_fn_key_index = KEYMAP_DEFAULT_FN_INDEX;

    s_initialized = true;
    fault_record(FAULT_LEVEL_INFO, "keymap", "init complete");
}

bool keymap_lookup(uint8_t key_index, bool fn_pressed, keymap_result_t* out_result)
{
    const device_config_t* cfg = config_get();
    uint8_t keycode;

    if (!s_initialized || out_result == NULL || key_index >= KEYMAP_KEY_COUNT)
    {
        return false;
    }

    /* 检查是否是Fn键本身 */
    if (key_index == s_fn_key_index)
    {
        out_result->type = KEYMAP_TYPE_FN;
        out_result->code = 0;
        return true;
    }

    /* 根据Fn状态选择映射层 */
    if (fn_pressed)
    {
        keycode = cfg->fn_keymap[key_index];
    }
    else
    {
        keycode = cfg->keymap[key_index];
    }

    /* 空键 */
    if (keycode == KEYMAP_KEY_NONE)
    {
        out_result->type = KEYMAP_TYPE_NONE;
        out_result->code = 0;
        return false;
    }

    /* 修饰键（新编码：0xE0~0xE7 直接编码修饰键类型） */
    if (keycode >= KEYMAP_MOD_BASE && keycode < (KEYMAP_MOD_BASE + KEYMAP_MOD_COUNT))
    {
        static const uint8_t modifier_bits[KEYMAP_MOD_COUNT] = {
            KEYBOARD_MODIFIER_LEFTCTRL,   /* 0xE0 */
            KEYBOARD_MODIFIER_LEFTSHIFT,  /* 0xE1 */
            KEYBOARD_MODIFIER_LEFTALT,    /* 0xE2 */
            KEYBOARD_MODIFIER_LEFTGUI,    /* 0xE3 */
            KEYBOARD_MODIFIER_RIGHTCTRL,  /* 0xE4 */
            KEYBOARD_MODIFIER_RIGHTSHIFT, /* 0xE5 */
            KEYBOARD_MODIFIER_RIGHTALT,   /* 0xE6 */
            KEYBOARD_MODIFIER_RIGHTGUI    /* 0xE7 */
        };
        out_result->type = KEYMAP_TYPE_MODIFIER;
        out_result->code = modifier_bits[keycode - KEYMAP_MOD_BASE];
        return true;
    }

    /* 修饰键（旧编码：0xFF + 物理位置表，向后兼容） */
    if (keycode == KEYMAP_KEY_MODIFIER)
    {
        out_result->type = KEYMAP_TYPE_MODIFIER;
        out_result->code = s_modifier_map[key_index];
        return true;
    }

    /* 多媒体键（Consumer） */
    if (keycode >= KEYMAP_CONSUMER_BASE && keycode < (KEYMAP_CONSUMER_BASE + KEYMAP_CONSUMER_COUNT))
    {
        uint8_t idx = (uint8_t)(keycode - KEYMAP_CONSUMER_BASE);
        out_result->type = KEYMAP_TYPE_CONSUMER;
        out_result->code = s_consumer_map[idx];
        return true;
    }

    /* 普通HID键 */
    out_result->type = KEYMAP_TYPE_NORMAL;
    out_result->code = keycode;
    return true;
}

void keymap_set_fn_key(uint8_t key_index)
{
    if (key_index < KEYMAP_KEY_COUNT)
    {
        s_fn_key_index = key_index;
    }
}

uint8_t keymap_get_fn_key(void)
{
    return s_fn_key_index;
}

bool keymap_is_fn_key(uint8_t key_index)
{
    return (key_index == s_fn_key_index);
}
