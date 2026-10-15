/**
 * @file usb_descriptors.c
 * @brief Bootloader USB 描述符
 *
 * Bootloader 只提供一个 HID 接口，用于 DFU 固件升级。
 * VID/PID 与应用程序不同，以便上位机区分 Bootloader 模式。
 */
#include "tusb.h"
#include "dfu_handler.h"

#define USB_VID   0x1209  // pid.codes VID（与应用程序相同，详见 docs/USB_COMPLIANCE.md）
#define USB_PID   0x4005  // Bootloader 专用 PID（应用是 0x4004，需在 pid.codes 注册）
#define USB_BCD   0x0200

// ============================================================================
// Device Descriptor
// ============================================================================
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

uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

// ============================================================================
// HID Report Descriptor（DFU 升级接口）
// ============================================================================
uint8_t const desc_hid_report[] =
{
    // Vendor 定义的报告描述符，包含所有 DFU 报告 ID
    0x06, 0x00, 0xFF,        // Usage Page (Vendor Defined)
    0x09, 0x01,              // Usage (0x01)
    0xA1, 0x01,              // Collection (Application)

    // 状态报告 (Report ID 0x30, 62字节, Feature)
    0x85, DFU_REPORT_ID_STATUS,
    0x09, 0x02,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 62, 0xB1, 0x02,

    // 擦除命令 (Report ID 0x31, 4字节, Feature)
    0x85, DFU_REPORT_ID_ERASE,
    0x09, 0x03,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 4, 0xB1, 0x02,

    // 数据写入 (Report ID 0x32, 62字节, Feature)
    0x85, DFU_REPORT_ID_DATA,
    0x09, 0x04,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 62, 0xB1, 0x02,

    // 校验命令 (Report ID 0x33, 8字节, Feature)
    0x85, DFU_REPORT_ID_VERIFY,
    0x09, 0x05,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 8, 0xB1, 0x02,

    // 完成命令 (Report ID 0x34, 4字节, Feature)
    0x85, DFU_REPORT_ID_FINISH,
    0x09, 0x06,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 4, 0xB1, 0x02,

    // 中止命令 (Report ID 0x35, 1字节, Feature)
    0x85, DFU_REPORT_ID_ABORT,
    0x09, 0x07,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 1, 0xB1, 0x02,

    // 启动应用程序 (Report ID 0x36, 1字节, Feature)
    0x85, DFU_REPORT_ID_BOOT_APP,
    0x09, 0x08,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 1, 0xB1, 0x02,

    0xC0                     // End Collection
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return desc_hid_report;
}

// ============================================================================
// Configuration Descriptor
// ============================================================================
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_HID  0x81

uint8_t const desc_configuration[] =
{
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10)
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

// ============================================================================
// String Descriptors
// ============================================================================
char const *string_desc_arr[] =
{
    (const char[]) { 0x09, 0x04 }, // 0: English
    "指动科技",                      // 1: Manufacturer（与应用程序一致）
    "指动 HID Bootloader (DFU)",    // 2: Product
    NULL,                           // 3: Serial
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    size_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
        const char *str = string_desc_arr[index];

        // UTF-8 到 UTF-16LE 转换（支持中文）
        chr_count = 0;
        size_t i = 0;
        size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;

        while (str[i] != '\0' && chr_count < max_count) {
            uint16_t utf16_char;
            uint8_t c = (uint8_t)str[i];

            if (c < 0x80) {
                utf16_char = c;
                i += 1;
            } else if ((c & 0xE0) == 0xC0) {
                utf16_char = ((c & 0x1F) << 6) | ((uint8_t)str[i+1] & 0x3F);
                i += 2;
            } else if ((c & 0xF0) == 0xE0) {
                utf16_char = ((c & 0x0F) << 12) |
                              (((uint8_t)str[i+1] & 0x3F) << 6) |
                              ((uint8_t)str[i+2] & 0x3F);
                i += 3;
            } else if ((c & 0xF8) == 0xF0) {
                uint32_t codepoint = ((c & 0x07) << 18) |
                                      (((uint8_t)str[i+1] & 0x3F) << 12) |
                                      (((uint8_t)str[i+2] & 0x3F) << 6) |
                                      ((uint8_t)str[i+3] & 0x3F);
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
                i += 1;
                continue;
            }

            _desc_str[1 + chr_count] = utf16_char;
            chr_count++;
        }
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}

// ============================================================================
// HID 回调
// ============================================================================

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_type;
    return dfu_get_report(report_id, buffer, reqlen);
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_type;
    dfu_set_report(report_id, buffer, bufsize);
}
