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
#include "version.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
// #define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
//                            _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

/*
 * USB VID/PID 配置
 *
 * 当前使用 pid.codes 分配的 VID (0x1209)，适合开源项目和小型商业产品。
 * 如需大规模商业销售，建议购买 USB-IF 独立 VID (~$5000)。
 *
 * pid.codes 申请：https://pid.codes/
 *   - 免费，通过 GitHub PR 申请
 *   - 每个项目分配一个唯一 PID
 *   - 申请时需提供项目名称、描述、URL
 *
 * 注意：以下 PID 为占位符，使用前请到 pid.codes 申请正式 PID 并替换。
 * 应用程序和 Bootloader 必须使用不同的 PID，以便上位机区分设备模式。
 */
#define USB_VID   0x1209  // pid.codes VID（免费，允许商业使用，详见 docs/USB_COMPLIANCE.md）
#define USB_PID   0x4004  // 应用程序 PID（需在 pid.codes 注册，项目：指动科技 Pico HID）
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
    .bcdDevice          = FIRMWARE_VERSION_BCD,  // 固件版本（统一从 version.h 引用）

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

  // NKRO 位图键盘报告 (Report ID 22)
  // 报告格式: [Report ID(1)] [Modifier(1)] [Reserved(1)] [KeyBitmap(32)] = 35 字节
  // KeyBitmap: 每个 bit 代表一个 HID 键码(0-255)，bit=1 表示按下
  // 注意: 必须包含完整的 Collection (Application) ... End Collection 包裹，
  //       否则 Windows HID 解析器会因报告描述符语法错误而导致设备不识别。
  0x05, 0x01,              // Usage Page (Desktop)
  0x09, 0x06,              // Usage (Keyboard)
  0xA1, 0x01,              // Collection (Application)
  0x85, REPORT_ID_NKRO,    //   Report ID (22)
  0x05, 0x07,              //   Usage Page (Keyboard)
  0x19, 0xE0,              //   Usage Minimum (224 = Left Control)
  0x29, 0xE7,              //   Usage Maximum (231 = Right GUI)
  0x15, 0x00,              //   Logical Minimum (0)
  0x25, 0x01,              //   Logical Maximum (1)
  0x75, 0x01,              //   Report Size (1 bit)
  0x95, 0x08,              //   Report Count (8)
  0x81, 0x02,              //   Input (Data,Var,Abs) - Modifier byte

  0x95, 0x01,              //   Report Count (1)
  0x75, 0x08,              //   Report Size (8 bits)
  0x81, 0x01,              //   Input (Const,Var,Abs) - Reserved byte

  0x05, 0x07,              //   Usage Page (Keyboard)
  0x19, 0x00,              //   Usage Minimum (0)
  0x29, 0xFF,              //   Usage Maximum (255)
  0x15, 0x00,              //   Logical Minimum (0)
  0x25, 0x01,              //   Logical Maximum (1)
  0x75, 0x01,              //   Report Size (1 bit)
  0x96, 0x00, 0x01,       //   Report Count (256) - 2字节前缀
  0x81, 0x02,              //   Input (Data,Var,Abs) - Key bitmap (32 bytes)
  0xC0,                    // End Collection
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

  // 控制命令 Output 报告 (Report ID 7, 8 字节)
  // v3.1 协议：Output Report 发送命令（中断传输，速度更快，高端游戏外设标准做法）
  // 格式: [device_idx, cmd, sw_id, param_low, param_high, reserved[3]]
  //   device_idx=0xFF 表示直连有线设备（无线版本为接收器插槽号）
  //   sw_id=1~15 循环分配，用于匹配请求响应（0=广播/通知）
  // 上位机通过 WriteFile 发送，走 USB 中断传输（Interrupt Transfer）
  0x85, 7,                 //   Report ID (7)
  0x09, 0x04,              //   Usage (0x04)
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 8,                 //   Report Count (8)
  0x91, 0x02,              //   Output (Data,Var,Abs) - 主机发送给设备（中断传输）

  // 命令响应 Feature 报告 (Report ID 24, 8 字节)
  // v3.1 协议：同步请求-响应模式（QMK/VIA 风格 + HID++ 风格的 SW ID 匹配）
  // 上位机发送 Output 命令后，立即 GET_REPORT(24) 读取执行结果
  // 格式: [status, cmd, sw_id, device_idx, param_low, param_high, reserved[2]]
  //   status=0 表示成功，非0为错误码（CONFIG_HID_ERR_BUSY=8 表示命令尚未处理完成，应重试）
  //   sw_id 回显请求的 Software ID，上位机验证匹配防止读到残留响应
  //   device_idx 回显请求的 Device Index
  // 替代原异步 ACK Input Report，避免 ACK 丢失导致的命令超时问题
  0x85, 24,                //   Report ID (24)
  0x09, 0x13,              //   Usage (0x13) - 独立Usage
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 8,                 //   Report Count (8)
  0xB1, 0x02,              //   Feature (Data,Var,Abs) - 同步读取响应

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

  // v4 配置扩展字段 Feature 报告 (Report ID 21, 18 字节)
  // 包含: joystick_invert_x, joystick_invert_y, encoder_steps,
  //       encoder_scroll_speed, joystick_sensitivity(2), reserved(2),
  //       length(2), write_count(4), crc32(4)
  0x85, 21,                //   Report ID (21)
  0x09, 0x12,              //   Usage (0x12) - 独立Usage
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 18,                //   Report Count (18)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // 摇杆校准状态 Feature 报告 (Report ID 23, 10 字节)
  // byte[0]=校准状态, byte[1-2]=center_x, byte[3-4]=center_y,
  // byte[5]=calibrated, byte[6-7]=死区值, byte[8-9]=灵敏度
  // 注意: 必须在描述符中声明，否则 Windows HID 驱动会拒绝此 Report ID
  0x85, 23,                //   Report ID (23)
  0x09, 0x14,              //   Usage (0x14) - 独立Usage
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 10,                //   Report Count (10)
  0xB1, 0x02,              //   Feature (Data,Var,Abs)

  // v6/v7 配置扩展块2 Feature 报告 (Report ID 25, 63 字节)
  // 使用子块索引机制：byte[0]=ReportID, byte[1]=子块索引, byte[2..]=数据
  // 子块定义：
  //   0 = v6字段 (偏移1346, 14字节): dpi_slot[4], current_dpi_slot, lod_level, angle_snapping, dpi_switch_key, reserved4[2]
  //   1 = layer2_keymap[0..31] (偏移1360, 32字节)
  //   2 = layer2_keymap[32..63] (偏移1392, 32字节)
  //   3 = layer3_keymap[0..31] (偏移1424, 32字节)
  //   4 = layer3_keymap[32..63] (偏移1456, 32字节)
  // 注意：全速USB Feature报告最大64字节（含Report ID），所以keymap需分2个子块传输。
  // Report Count = 63 = 1字节索引 + 62字节数据
  0x85, 25,                //   Report ID (25)
  0x09, 0x15,              //   Usage (0x15) - 独立Usage
  0x15, 0x00,              //   Logical Minimum (0)
  0x26, 0xFF, 0x00,        //   Logical Maximum (255)
  0x75, 0x08,              //   Report Size (8 bits)
  0x95, 63,                //   Report Count (63) = 1字节索引 + 62字节数据
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

/* ==================== USB 字符串多语言支持 ==================== */
/* 语言ID定义（USB-IF 标准） */
#define USB_LANGID_ENGLISH_US   0x0409  /* 英语（美国） */
#define USB_LANGID_CHINESE_SIMPLIFIED 0x0804  /* 中文（简体，中国） */

/* 支持的语言列表（索引0返回此列表） */
static const uint16_t supported_langids[] = {
    USB_LANGID_ENGLISH_US,
    USB_LANGID_CHINESE_SIMPLIFIED,
};
#define SUPPORTED_LANGID_COUNT (sizeof(supported_langids) / sizeof(supported_langids[0]))

/* 英文字符串 */
static char const *string_desc_arr_en[] =
{
  NULL,  /* 0: 语言ID列表（特殊处理） */
  "Zhidong Tech",               /* 1: Manufacturer */
  "Zhidong HID Composite Device", /* 2: Product */
  NULL,                            /* 3: Serial（基于芯片唯一ID） */
};

/* 中文字符串 */
static char const *string_desc_arr_zh[] =
{
  NULL,  /* 0: 语言ID列表（特殊处理） */
  "指动科技",                      /* 1: Manufacturer */
  "指动 HID 复合设备",             /* 2: Product */
  NULL,                            /* 3: Serial（基于芯片唯一ID） */
};

/* 获取指定语言的字符串数组 */
static char const **get_string_array_by_langid(uint16_t langid)
{
  switch (langid)
  {
    case USB_LANGID_CHINESE_SIMPLIFIED:
      return string_desc_arr_zh;
    case USB_LANGID_ENGLISH_US:
    default:
      return string_desc_arr_en;
  }
}

static uint16_t _desc_str[32 + 1];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  size_t chr_count;

  switch ( index ) {
    case STRID_LANGID:
      /* 返回支持的语言ID列表（英文+中文） */
      chr_count = 0;
      for (size_t i = 0; i < SUPPORTED_LANGID_COUNT && chr_count < 32; i++) {
        _desc_str[1 + chr_count] = supported_langids[i];
        chr_count++;
      }
      break;

    case STRID_SERIAL:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
      // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

      /* 根据语言ID选择对应的字符串数组 */
      char const **str_array = get_string_array_by_langid(langid);

      /* 字符串数组大小（所有语言数组大小相同） */
      const size_t str_array_size = sizeof(string_desc_arr_en) / sizeof(string_desc_arr_en[0]);

      if ( !(index < str_array_size) ) return NULL;

      const char *str = str_array[index];

      // UTF-8 到 UTF-16LE 转换（支持中文）
      chr_count = 0;
      size_t i = 0;
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type

      while (str[i] != '\0' && chr_count < max_count) {
        uint16_t utf16_char;
        uint8_t c = (uint8_t)str[i];

        if (c < 0x80) {
          // ASCII 字符 (1 字节)
          utf16_char = c;
          i += 1;
        } else if ((c & 0xE0) == 0xC0) {
          // 2 字节 UTF-8
          utf16_char = ((c & 0x1F) << 6) | ((uint8_t)str[i+1] & 0x3F);
          i += 2;
        } else if ((c & 0xF0) == 0xE0) {
          // 3 字节 UTF-8 (中文常用)
          utf16_char = ((c & 0x0F) << 12) |
                        (((uint8_t)str[i+1] & 0x3F) << 6) |
                        ((uint8_t)str[i+2] & 0x3F);
          i += 3;
        } else if ((c & 0xF8) == 0xF0) {
          // 4 字节 UTF-8 (需要代理对，此处简化处理)
          uint32_t codepoint = ((c & 0x07) << 18) |
                                (((uint8_t)str[i+1] & 0x3F) << 12) |
                                (((uint8_t)str[i+2] & 0x3F) << 6) |
                                ((uint8_t)str[i+3] & 0x3F);
          // 转换为 UTF-16 代理对
          codepoint -= 0x10000;
          _desc_str[1 + chr_count] = (uint16_t)(0xD800 + (codepoint >> 10));
          chr_count++;
          if (chr_count < max_count) {
            _desc_str[1 + chr_count] = (uint16_t)(0xDC00 + (codepoint & 0x3FF));
            chr_count++;
          }
          i += 4;
          continue;
        } else {
          // 无效的 UTF-8 字节，跳过
          i += 1;
          continue;
        }

        _desc_str[1 + chr_count] = utf16_char;
        chr_count++;
      }
      break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}








