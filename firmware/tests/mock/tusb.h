/*
 * tests/mock/tusb.h
 * Mock版TinyUSB头文件，仅定义keymap模块需要的常量
 * 用于单元测试
 */

#ifndef _TUSB_H_
#define _TUSB_H_

#include <stdint.h>

/* HID 修饰键位掩码
 * 与 TinyUSB 定义完全一致
 */
#define KEYBOARD_MODIFIER_LEFTCTRL   (1 << 0)
#define KEYBOARD_MODIFIER_LEFTSHIFT  (1 << 1)
#define KEYBOARD_MODIFIER_LEFTALT    (1 << 2)
#define KEYBOARD_MODIFIER_LEFTGUI    (1 << 3)
#define KEYBOARD_MODIFIER_RIGHTCTRL  (1 << 4)
#define KEYBOARD_MODIFIER_RIGHTSHIFT (1 << 5)
#define KEYBOARD_MODIFIER_RIGHTALT   (1 << 6)
#define KEYBOARD_MODIFIER_RIGHTGUI   (1 << 7)

#endif /* _TUSB_H_ */
