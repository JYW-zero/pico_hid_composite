/*
 * tests/mock/mock_tusb.c
 * Mock版TinyUSB函数实现
 */

#include "tusb.h"
#include <stdbool.h>

/* HID 就绪检查 - mock 始终返回 true */
bool tud_hid_ready(void)
{
    return true;
}

/* 发送鼠标 HID 报表 - mock 实现 */
bool tud_hid_mouse_report(uint8_t report_id, uint8_t buttons, int8_t x, int8_t y, int8_t vertical, int8_t horizontal)
{
    (void)report_id;
    (void)buttons;
    (void)x;
    (void)y;
    (void)vertical;
    (void)horizontal;
    return true;
}

/* 发送键盘 HID 报表 - mock 实现 */
bool tud_hid_keyboard_report(uint8_t report_id, uint8_t modifier, uint8_t keycode[6])
{
    (void)report_id;
    (void)modifier;
    (void)keycode;
    return true;
}

/* 多接口 HID 就绪检查 - mock 始终返回 true */
bool tud_hid_n_ready(uint8_t instance)
{
    (void)instance;
    return true;
}

/* 多接口 HID 发送报告 - mock 始终返回 true */
bool tud_hid_n_report(uint8_t instance, uint8_t report_id, void const *report, uint16_t len)
{
    (void)instance;
    (void)report_id;
    (void)report;
    (void)len;
    return true;
}
