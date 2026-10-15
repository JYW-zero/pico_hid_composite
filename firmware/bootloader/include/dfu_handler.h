/**
 * @file dfu_handler.h
 * @brief Bootloader DFU 协议处理头文件
 */
#ifndef DFU_HANDLER_H
#define DFU_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// DFU HID Report ID
#define DFU_REPORT_ID_STATUS     0x30  // 状态查询/响应（62字节）
#define DFU_REPORT_ID_ERASE      0x31  // 擦除命令（4字节：大小）
#define DFU_REPORT_ID_DATA       0x32  // 数据写入（62字节：4字节偏移 + 58字节数据）
#define DFU_REPORT_ID_VERIFY     0x33  // 校验命令（8字节：大小+CRC）
#define DFU_REPORT_ID_FINISH     0x34  // 完成/重启（4字节：版本号）
#define DFU_REPORT_ID_ABORT      0x35  // 中止升级
#define DFU_REPORT_ID_BOOT_APP   0x36  // 启动应用程序（1字节）

// DFU 状态响应结构（Report ID 0x30）
typedef struct __attribute__((packed)) {
    uint8_t  state;          // 当前状态 dfu_state_t
    uint8_t  error;          // 错误码（0=无错误）
    uint16_t reserved;
    uint32_t write_offset;   // 当前写入偏移
    uint32_t expected_size;  // 期望的固件大小
    uint32_t received_crc;   // 已接收数据的 CRC
    uint8_t  version_major;  // Bootloader 版本
    uint8_t  version_minor;
    uint16_t reserved2;
    uint32_t app_start;      // 应用程序起始地址
    uint32_t app_max_size;   // 应用程序最大大小
} dfu_status_response_t;

// 错误码
typedef enum {
    DFU_ERR_NONE            = 0x00,
    DFU_ERR_INVALID_SIZE    = 0x01,
    DFU_ERR_ERASE_FAILED    = 0x02,
    DFU_ERR_WRITE_FAILED    = 0x03,
    DFU_ERR_VERIFY_FAILED   = 0x04,
    DFU_ERR_INVALID_CMD     = 0x05,
    DFU_ERR_FLASH_LOCKED    = 0x06,
} dfu_error_t;

/**
 * @brief 初始化 DFU 处理器
 */
void dfu_handler_init(void);

/**
 * @brief DFU 处理器任务（在主循环中调用）
 */
void dfu_handler_task(void);

/**
 * @brief 检查是否有升级活动
 * @return true=正在升级
 */
bool dfu_handler_is_active(void);

/**
 * @brief 处理 HID GET_REPORT 请求
 */
uint16_t dfu_get_report(uint8_t report_id, uint8_t* buffer, uint16_t reqlen);

/**
 * @brief 处理 HID SET_REPORT 请求
 */
void dfu_set_report(uint8_t report_id, const uint8_t* buffer, uint16_t bufsize);

#ifdef __cplusplus
}
#endif

#endif // DFU_HANDLER_H
