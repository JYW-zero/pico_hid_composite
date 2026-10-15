/**
 * @file dfu_metadata.h
 * @brief DFU 升级元数据结构定义
 *
 * 存储在 Flash 的独立扇区（0x10008000），Bootloader 和应用程序都可访问。
 * 用于协调升级流程、校验固件完整性、支持回滚。
 */
#ifndef DFU_METADATA_H
#define DFU_METADATA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Flash 分区定义（RP2350, 2MB Flash, 基地址 0x10000000）
// ============================================================================

#define FLASH_BASE_ADDR         0x10000000U
#define FLASH_SIZE              (2 * 1024 * 1024)  // 2MB（当前硬件，后续可升级到4MB）

#define BOOTLOADER_SIZE         (32 * 1024)         // 32KB
#define BOOTLOADER_START        FLASH_BASE_ADDR
#define BOOTLOADER_END          (BOOTLOADER_START + BOOTLOADER_SIZE)

#define DFU_METADATA_SIZE       (4 * 1024)          // 4KB（一个扇区）
#define DFU_METADATA_START      BOOTLOADER_END
#define DFU_METADATA_END        (DFU_METADATA_START + DFU_METADATA_SIZE)

#define APP_START_ADDR          DFU_METADATA_END    // 0x10009000
#define APP_MAX_SIZE            (FLASH_SIZE - BOOTLOADER_SIZE - DFU_METADATA_SIZE - CONFIG_AREA_SIZE)

#define CONFIG_AREA_SIZE        (64 * 1024)         // 64KB 用户配置区
#define CONFIG_AREA_START       (FLASH_BASE_ADDR + FLASH_SIZE - CONFIG_AREA_SIZE)

// 应用程序向量表偏移（用于 VTOR 设置）
#define APP_VECTOR_TABLE_OFFSET (APP_START_ADDR - FLASH_BASE_ADDR)

// ============================================================================
// DFU 元数据魔数和版本
// ============================================================================

#define DFU_METADATA_MAGIC      0x44465531U  // "DFU1"
#define DFU_METADATA_VERSION    1

// 升级状态
typedef enum {
    DFU_STATE_IDLE          = 0x00,  // 正常状态，无待升级固件
    DFU_STATE_READY         = 0x01,  // 固件已写入，待校验
    DFU_STATE_VERIFIED      = 0x02,  // 固件校验通过，待切换
    DFU_STATE_SWITCHED      = 0x03,  // 已切换到新固件（首次启动）
    DFU_STATE_ROLLBACK      = 0x04,  // 回滚中
    DFU_STATE_ERROR         = 0xFF,  // 错误状态
} dfu_state_t;

// 升级命令（Bootloader 和应用共用）
typedef enum {
    // 应用 → Bootloader（通过元数据区传递）
    DFU_CMD_ENTER_DFU       = 0x01,  // 进入 DFU 模式
    DFU_CMD_RESET           = 0x02,  // 软重启

    // Bootloader → 上位机（HID 报告）
    DFU_CMD_GET_STATUS      = 0x10,  // 获取 DFU 状态
    DFU_CMD_ERASE           = 0x11,  // 擦除应用程序区
    DFU_CMD_WRITE           = 0x12,  // 写入数据块
    DFU_CMD_VERIFY          = 0x13,  // 校验 CRC
    DFU_CMD_FINISH          = 0x14,  // 完成升级，重启
    DFU_CMD_ABORT           = 0x15,  // 中止升级
} dfu_command_t;

// ============================================================================
// DFU 元数据结构（存储在 Flash 中，64 字节，对齐到 4 字节）
// ============================================================================

typedef struct __attribute__((packed)) {
    uint32_t magic;              // 魔数 0x44465531
    uint32_t version;            // 元数据结构版本
    uint32_t state;              // 当前状态 dfu_state_t
    uint32_t command;            // 待执行命令 dfu_command_t

    // 新固件信息
    uint32_t new_fw_size;        // 新固件大小（字节）
    uint32_t new_fw_crc32;       // 新固件 CRC32
    uint32_t new_fw_version;     // 新固件版本号（BCD: 0x0100 = v1.0.0）
    uint32_t new_fw_addr;        // 新固件存储地址（通常 = APP_START_ADDR）

    // 旧固件信息（用于回滚）
    uint32_t old_fw_size;        // 旧固件大小
    uint32_t old_fw_crc32;       // 旧固件 CRC32
    uint32_t old_fw_version;     // 旧固件版本号

    // 升级进度
    uint32_t write_offset;       // 当前写入偏移
    uint32_t boot_count;         // 新固件启动计数（超过阈值自动回滚）

    // 保留
    uint32_t reserved[4];
} dfu_metadata_t;

// 编译时检查大小
_Static_assert(sizeof(dfu_metadata_t) == 68, "dfu_metadata_t must be 68 bytes");

// ============================================================================
// 固件头部结构（应用程序开头，用于版本和大小信息）
// ============================================================================

typedef struct __attribute__((packed)) {
    uint32_t magic;              // 0x46574D31 "FWM1"
    uint32_t version;            // 固件版本 BCD
    uint32_t size;               // 固件总大小（含头部）
    uint32_t crc32;              // 固件 CRC32（从头部后开始计算）
    uint32_t entry_point;        // 入口点（Reset_Handler 地址）
    uint32_t reserved[3];
} firmware_header_t;

#define FIRMWARE_HEADER_MAGIC   0x46574D31U  // "FWM1"
#define FIRMWARE_HEADER_SIZE    sizeof(firmware_header_t)

// ============================================================================
// 固件签名验证（HMAC-SHA256）
// ============================================================================

/* 固件签名大小（HMAC-SHA256，32字节） */
#define FIRMWARE_SIGNATURE_SIZE     32

/* 固件签名魔数（用于识别已签名固件，存储在签名前4字节） */
#define FIRMWARE_SIGNATURE_MAGIC    0x53494731U  /* "SIG1" */

/* 固件签名密钥长度（字节） */
#define FIRMWARE_SIGNATURE_KEY_LEN  32

/* 固件签名状态 */
typedef enum {
    FW_SIGNATURE_UNCHECKED = 0,  /* 未检查 */
    FW_SIGNATURE_VALID     = 1,  /* 签名有效 */
    FW_SIGNATURE_INVALID   = 2,  /* 签名无效 */
    FW_SIGNATURE_UNSIGNED  = 3,  /* 未签名（无签名魔数） */
} firmware_signature_status_t;

// ============================================================================
// 函数声明
// ============================================================================

/**
 * @brief 读取 DFU 元数据
 * @return 指向元数据的指针（Flash 中的只读数据）
 */
const dfu_metadata_t* dfu_metadata_read(void);

/**
 * @brief 写入 DFU 元数据（擦除并写入整个扇区）
 * @param meta 元数据指针
 */
void dfu_metadata_write(const dfu_metadata_t* meta);

/**
 * @brief 设置升级命令（应用程序调用，触发 Bootloader）
 * @param cmd 命令
 */
void dfu_set_command(dfu_command_t cmd);

/**
 * @brief 校验应用程序固件 CRC
 * @param app_addr 应用程序起始地址
 * @param size 固件大小
 * @param expected_crc 期望的 CRC32
 * @return true=校验通过
 */
bool dfu_verify_firmware(uint32_t app_addr, uint32_t size, uint32_t expected_crc);

/**
 * @brief 跳转到应用程序
 * @param app_addr 应用程序起始地址
 */
void dfu_jump_to_app(uint32_t app_addr);

/**
 * @brief 计算 CRC32（与 zlib 兼容）
 * @param data 数据指针
 * @param length 数据长度
 * @return CRC32 值
 */
uint32_t dfu_crc32(const uint8_t* data, uint32_t length);

/**
 * @brief 验证固件签名（HMAC-SHA256）
 *
 * 签名格式：固件数据末尾追加 36 字节：
 *   - 4 字节魔数 (FIRMWARE_SIGNATURE_MAGIC)
 *   - 32 字节 HMAC-SHA256 签名
 *
 * 签名计算范围：整个固件（包括头部，不包括末尾的36字节签名）
 *
 * @param fw_addr 固件起始地址（Flash XIP 地址）
 * @param fw_size 固件总大小（包括签名，字节）
 * @return 签名状态（FW_SIGNATURE_VALID/FW_SIGNATURE_INVALID/FW_SIGNATURE_UNSIGNED）
 */
firmware_signature_status_t dfu_verify_firmware_signature(uint32_t fw_addr, uint32_t fw_size);

/**
 * @brief 获取固件签名密钥（编译时嵌入，用于 Bootloader 验签）
 * @return 指向32字节密钥的指针
 */
const uint8_t* dfu_get_signature_key(void);

/**
 * @brief 设置固件签名验证开关（用于调试，默认开启）
 * @param enabled true=开启签名验证，false=关闭（仅CRC校验）
 */
void dfu_set_signature_verify_enabled(bool enabled);

/**
 * @brief 获取固件签名验证开关状态
 * @return true=开启，false=关闭
 */
bool dfu_is_signature_verify_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // DFU_METADATA_H
