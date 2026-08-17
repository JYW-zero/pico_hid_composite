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

#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
// #define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
//                            _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

#define USB_VID   0xCafe
#define USB_PID   0x4004  // 固定 PID，不随接口数量变化
#define USB_BCD   0x0200

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(REPORT_ID_KEYBOARD         )),
  TUD_HID_REPORT_DESC_MOUSE   ( HID_REPORT_ID(REPORT_ID_MOUSE            )),
  TUD_HID_REPORT_DESC_CONSUMER( HID_REPORT_ID(REPORT_ID_CONSUMER_CONTROL )),
  TUD_HID_REPORT_DESC_GAMEPAD ( HID_REPORT_ID(REPORT_ID_GAMEPAD          )),
};

// 配置接口的 HID 报告描述符（第二个 HID 接口）
uint8_t const desc_hid_config_report[] =
{
  // === 配置接口 Feature 报告（Vendor 定义） ===
  0x06, 0x00, 0xFF,        // Usage Page (Vendor Defined 0xFF00)
  0x09, 0x01,              // Usage (0x01)
  0xA1, 0x01,              // Collection (Application)

  // 配置块 0 Feature 报告 (Report ID 5, 62 字节, 偏移 0-61)
  0x85, 5,                 //   Report ID (5)
  0x09, 0x02,              //   Usage (0x02)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 62,                //   Report Count (62)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 配置块 1 Feature 报告 (Report ID 8, 62 字节, 偏移 62-123)
  0x85, 8,                 //   Report ID (8)
  0x09, 0x05,              //   Usage (0x05)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 62,                //   Report Count (62)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 配置块 2 Feature 报告 (Report ID 9, 62 字节, 偏移 124-185)
  0x85, 9,                 //   Report ID (9)
  0x09, 0x06,              //   Usage (0x06)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 62,                //   Report Count (62)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 设备信息 Feature 报告 (Report ID 6, 32 字节)
  0x85, 6,                 //   Report ID (6)
  0x09, 0x03,              //   Usage (0x03)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 32,                //   Report Count (32)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 控制命令 Feature 报告 (Report ID 7, 1 字节)
  0x85, 7,                 //   Report ID (7)
  0x09, 0x04,              //   Usage (0x04)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 1,                 //   Report Count (1)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 按键统计 0 Feature 报告 (Report ID 10, 32 字节, 键0-15)
  0x85, 10,                //   Report ID (10)
  0x09, 0x0C,              //   Usage (0x0C)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 32,                //   Report Count (32)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 按键统计 1 Feature 报告 (Report ID 11, 32 字节, 键16-31)
  0x85, 11,                //   Report ID (11)
  0x09, 0x0D,              //   Usage (0x0D)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 32,                //   Report Count (32)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 按键统计 2 Feature 报告 (Report ID 12, 32 字节, 键32-47)
  0x85, 12,                //   Report ID (12)
  0x09, 0x0E,              //   Usage (0x0E)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 32,                //   Report Count (32)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 按键统计 3 Feature 报告 (Report ID 13, 32 字节, 键48-63)
  0x85, 13,                //   Report ID (13)
  0x09, 0x0F,              //   Usage (0x0F)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 32,                //   Report Count (32)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 宏配置 Feature 报告 (Report ID 14, 62 字节)
  0x85, 14,                //   Report ID (14)
  0x09, 0x07,              //   Usage (0x07)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 62,                //   Report Count (62)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 性能监控 - 系统状态 Feature 报告 (Report ID 15, 62 字节)
  0x85, 15,                //   Report ID (15)
  0x09, 0x08,              //   Usage (0x08)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 62,                //   Report Count (62)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 性能监控 - 任务统计 Feature 报告 (Report ID 16, 62 字节)
  0x85, 16,                //   Report ID (16)
  0x09, 0x09,              //   Usage (0x09)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 62,                //   Report Count (62)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 错误日志 - 信息 Feature 报告 (Report ID 17, 62 字节)
  0x85, 17,                //   Report ID (17)
  0x09, 0x0A,              //   Usage (0x0A)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 62,                //   Report Count (62)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 错误日志 - 读取日志 Feature 报告 (Report ID 18, 62 字节)
  0x85, 18,                //   Report ID (18)
  0x09, 0x0B,              //   Usage (0x0B)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 62,                //   Report Count (62)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 实时按键状态 Feature 报告 (Report ID 19, 8 字节, 64位bitmap)
  0x85, 19,                //   Report ID (19)
  0x09, 0x10,              //   Usage (0x10) - 独立Usage，避免与Key Stats 0冲突
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 8,                 //   Report Count (8)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 实时摇杆状态 Feature 报告 (Report ID 20, 8 字节)
  0x85, 20,                //   Report ID (20)
  0x09, 0x11,              //   Usage (0x11) - 独立Usage，避免与Key Stats 1冲突
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 8,                 //   Report Count (8)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // v3 配置扩展字段 Feature 报告 (Report ID 21, 12 字节)
  // 包含: joystick_invert_x, joystick_invert_y, encoder_steps,
  //       encoder_scroll_speed, joystick_sensitivity(2), reserved(6)
  0x85, 21,                //   Report ID (21)
  0x09, 0x12,              //   Usage (0x12) - 独立Usage
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 12,                //   Report Count (12)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  0xC0                     // End Collection
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
  if (instance == 0)
  {
    // 接口 0：标准 HID 功能（键盘、鼠标、消费者控制、游戏手柄）
    return desc_hid_report;
  }
  else
  {
    // 接口 1：配置接口（Vendor 定义）
    return desc_hid_config_report;
  }
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

#define  CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_HID          0x81
#define EPNUM_HID_CONFIG   0x82   // 配置接口的端点

uint8_t const desc_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // 接口 0：标准 HID 功能（键盘、鼠标、消费者控制、游戏手柄）
  // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
  TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1),

  // 接口 1：配置接口（Vendor 定义）
  TUD_HID_DESCRIPTOR(ITF_NUM_HID_CONFIG, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_config_report), EPNUM_HID_CONFIG, CFG_TUD_HID_EP_BUFSIZE, 10)
};

#if TUD_OPT_HIGH_SPEED
// Per USB specs: high speed capable device must report device_qualifier and other_speed_configuration

// other speed configuration
uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
tusb_desc_device_qualifier_t const desc_device_qualifier =
{
  .bLength            = sizeof(tusb_desc_device_qualifier_t),
  .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
  .bcdUSB             = USB_BCD,

  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,

  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .bNumConfigurations = 0x01,
  .bReserved          = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
  return (uint8_t const*) &desc_device_qualifier;
}

// Invoked when received GET OTHER SEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  // other speed config is basically configuration with type = OHER_SPEED_CONFIG
  memcpy(desc_other_speed_config, desc_configuration, CONFIG_TOTAL_LEN);
  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

  // this example use the same configuration for both high and full speed mode
  return desc_other_speed_config;
}

#endif // highspeed

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  // This example use the same configuration for both high and full speed mode
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// String Descriptor Index
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
};

// array of pointer to string descriptors
char const *string_desc_arr[] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "JYW",                          // 1: Manufacturer
  "Pico HID Composite",           // 2: Product
  NULL,                          // 3: Serials will use unique ID if possible
};

static uint16_t _desc_str[32 + 1];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;

  switch ( index ) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
      // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

      if ( !(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) ) return NULL;

      const char *str = string_desc_arr[index];

      // Cap at max char
      chr_count = strlen(str);
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type
      if ( chr_count > max_count ) chr_count = max_count;

      // Convert ASCII string into UTF-16
      for ( size_t i = 0; i < chr_count; i++ ) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}








