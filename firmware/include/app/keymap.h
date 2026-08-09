/*
 * include/app/keymap.h
 * 按键映射模块
 * 支持普通层 + Fn层 两层映射
 * 映射表从Flash配置加载，支持掉电保存
 */

#ifndef APP_KEYMAP_H
#define APP_KEYMAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/* 按键数量 */
#define KEYMAP_KEY_COUNT  64

/* 特殊键码标记 */
#define KEYMAP_KEY_NONE     0x00  /* 空键 */
#define KEYMAP_KEY_MODIFIER 0xFF  /* 修饰键标记（具体修饰键查表） */
#define KEYMAP_KEY_FN       0xFE  /* Fn键标记 */

/* Fn键索引（默认值，可配置） */
#define KEYMAP_DEFAULT_FN_INDEX  59  /* 第60号键，原F12位置 */

/* ==================== 映射结果类型 ==================== */

typedef enum
{
    KEYMAP_TYPE_NONE = 0,     /* 空键 */
    KEYMAP_TYPE_NORMAL,       /* 普通HID键码 */
    KEYMAP_TYPE_MODIFIER,     /* 修饰键 */
    KEYMAP_TYPE_FN,           /* Fn键本身 */
    KEYMAP_TYPE_CONSUMER,     /* 多媒体键（Consumer Report） */
} keymap_type_e;

/* ==================== 常用多媒体键码（Consumer Usage ID） ==================== */
/* 注意：这些是USB HID Consumer Page的Usage ID，16位 */
#define CONSUMER_VOLUME_UP     0x00E9
#define CONSUMER_VOLUME_DOWN   0x00EA
#define CONSUMER_MUTE          0x00E2
#define CONSUMER_PLAY_PAUSE    0x00CD
#define CONSUMER_SCAN_NEXT     0x00B5
#define CONSUMER_SCAN_PREV     0x00B6
#define CONSUMER_STOP          0x00B7
#define CONSUMER_EJECT         0x00B8

/* 配置存储中的特殊标记（uint8_t，0xF0-0xF7对应8个常用多媒体键） */
#define KEYMAP_CONSUMER_BASE   0xF0
#define KEYMAP_CONSUMER_COUNT  8

typedef struct
{
    keymap_type_e type;       /* 键类型 */
    uint16_t code;            /* 键码（普通键8位，修饰键8位，Consumer键16位） */
} keymap_result_t;

/* ==================== 对外接口 ==================== */

/* 初始化按键映射：从Flash配置加载 */
void keymap_init(void);

/* 查询按键映射
 * key_index: 按键索引 (0-63)
 * fn_pressed: Fn键是否按下
 * out_result: 输出结果
 * 返回值：true=有效，false=空键
 */
bool keymap_lookup(uint8_t key_index, bool fn_pressed, keymap_result_t* out_result);

/* 设置Fn键索引 */
void keymap_set_fn_key(uint8_t key_index);

/* 获取Fn键索引 */
uint8_t keymap_get_fn_key(void);

/* 检查是否是Fn键 */
bool keymap_is_fn_key(uint8_t key_index);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEYMAP_H */
