/*
 * tests/mock/tusb.h
 * Mock版TinyUSB头文件，定义keymap和macro模块需要的常量和函数
 * 用于单元测试
 */

#ifndef _TUSB_H_
#define _TUSB_H_

#include <stdint.h>
#include <stdbool.h>

/* ==================== HID 修饰键位掩码 ==================== */
#define KEYBOARD_MODIFIER_LEFTCTRL   (1 << 0)
#define KEYBOARD_MODIFIER_LEFTSHIFT  (1 << 1)
#define KEYBOARD_MODIFIER_LEFTALT    (1 << 2)
#define KEYBOARD_MODIFIER_LEFTGUI    (1 << 3)
#define KEYBOARD_MODIFIER_RIGHTCTRL  (1 << 4)
#define KEYBOARD_MODIFIER_RIGHTSHIFT (1 << 5)
#define KEYBOARD_MODIFIER_RIGHTALT   (1 << 6)
#define KEYBOARD_MODIFIER_RIGHTGUI   (1 << 7)

/* ==================== HID 键码定义（常用部分） ==================== */
#define HID_KEY_NONE               0x00
#define HID_KEY_A                  0x04
#define HID_KEY_B                  0x05
#define HID_KEY_C                  0x06
#define HID_KEY_D                  0x07
#define HID_KEY_E                  0x08
#define HID_KEY_F                  0x09
#define HID_KEY_G                  0x0A
#define HID_KEY_H                  0x0B
#define HID_KEY_I                  0x0C
#define HID_KEY_J                  0x0D
#define HID_KEY_K                  0x0E
#define HID_KEY_L                  0x0F
#define HID_KEY_M                  0x10
#define HID_KEY_N                  0x11
#define HID_KEY_O                  0x12
#define HID_KEY_P                  0x13
#define HID_KEY_Q                  0x14
#define HID_KEY_R                  0x15
#define HID_KEY_S                  0x16
#define HID_KEY_T                  0x17
#define HID_KEY_U                  0x18
#define HID_KEY_V                  0x19
#define HID_KEY_W                  0x1A
#define HID_KEY_X                  0x1B
#define HID_KEY_Y                  0x1C
#define HID_KEY_Z                  0x1D
#define HID_KEY_1                  0x1E
#define HID_KEY_2                  0x1F
#define HID_KEY_3                  0x20
#define HID_KEY_4                  0x21
#define HID_KEY_5                  0x22
#define HID_KEY_6                  0x23
#define HID_KEY_7                  0x24
#define HID_KEY_8                  0x25
#define HID_KEY_9                  0x26
#define HID_KEY_0                  0x27
#define HID_KEY_ENTER              0x28
#define HID_KEY_ESCAPE             0x29
#define HID_KEY_BACKSPACE          0x2A
#define HID_KEY_TAB                0x2B
#define HID_KEY_SPACE              0x2C
#define HID_KEY_MINUS              0x2D
#define HID_KEY_EQUAL              0x2E
#define HID_KEY_BRACKET_LEFT       0x2F
#define HID_KEY_BRACKET_RIGHT      0x30
#define HID_KEY_BACKSLASH          0x31
#define HID_KEY_EUROPE_1           0x32
#define HID_KEY_SEMICOLON          0x33
#define HID_KEY_APOSTROPHE         0x34
#define HID_KEY_GRAVE              0x35
#define HID_KEY_COMMA              0x36
#define HID_KEY_PERIOD             0x37
#define HID_KEY_SLASH              0x38
#define HID_KEY_CAPS_LOCK          0x39

/* 功能键 */
#define HID_KEY_F1                 0x3A
#define HID_KEY_F2                 0x3B
#define HID_KEY_F3                 0x3C
#define HID_KEY_F4                 0x3D
#define HID_KEY_F5                 0x3E
#define HID_KEY_F6                 0x3F
#define HID_KEY_F7                 0x40
#define HID_KEY_F8                 0x41
#define HID_KEY_F9                 0x42
#define HID_KEY_F10                0x43
#define HID_KEY_F11                0x44
#define HID_KEY_F12                0x45

/* 修饰键 */
#define HID_KEY_CONTROL_LEFT       0xE0
#define HID_KEY_SHIFT_LEFT         0xE1
#define HID_KEY_ALT_LEFT           0xE2
#define HID_KEY_GUI_LEFT           0xE3
#define HID_KEY_CONTROL_RIGHT      0xE4
#define HID_KEY_SHIFT_RIGHT        0xE5
#define HID_KEY_ALT_RIGHT          0xE6
#define HID_KEY_GUI_RIGHT          0xE7

/* ==================== 鼠标相关 ==================== */
#define MOUSE_BUTTON_LEFT          (1 << 0)
#define MOUSE_BUTTON_RIGHT         (1 << 1)
#define MOUSE_BUTTON_MIDDLE        (1 << 2)

/* ==================== HID Report 类型 ==================== */
typedef enum
{
  HID_REPORT_TYPE_INVALID = 0,
  HID_REPORT_TYPE_INPUT,
  HID_REPORT_TYPE_OUTPUT,
  HID_REPORT_TYPE_FEATURE,
} hid_report_type_t;

/* ==================== Mock 函数声明 ==================== */
#ifdef __cplusplus
extern "C" {
#endif

/* HID 就绪检查 */
bool tud_hid_ready(void);

/* 发送鼠标 HID 报表 */
bool tud_hid_mouse_report(uint8_t report_id, uint8_t buttons, int8_t x, int8_t y, int8_t vertical, int8_t horizontal);

/* 发送键盘 HID 报表 */
bool tud_hid_keyboard_report(uint8_t report_id, uint8_t modifier, uint8_t keycode[6]);

/* 多接口 HID 就绪检查（mock 始终返回 true） */
bool tud_hid_n_ready(uint8_t instance);

/* 多接口 HID 发送报告（mock 始终返回 true） */
bool tud_hid_n_report(uint8_t instance, uint8_t report_id, void const *report, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_H_ */
