/**
 * @file dfu_metadata.c
 * @brief DFU 元数据读写、CRC 校验、跳转应用程序
 */
#include "device/dfu/dfu_metadata.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include <string.h>
#include "middleware/hmac_sha256.h"

// Cortex-M33 寄存器地址（不依赖 CMSIS）
#define SCB_VTOR_ADDR       (*(volatile uint32_t*)0xE000ED08)
#define SYSTICK_CTRL_ADDR   (*(volatile uint32_t*)0xE000E010)
#define SYSTICK_LOAD_ADDR   (*(volatile uint32_t*)0xE000E014)
#define SYSTICK_VAL_ADDR    (*(volatile uint32_t*)0xE000E018)
#define NVIC_ICER0_ADDR     (*(volatile uint32_t*)0xE000E180)
#define NVIC_ICPR0_ADDR     (*(volatile uint32_t*)0xE000E280)

// ============================================================================
// CRC32 实现（与 zlib 兼容，多项式 0xEDB88320）
// ============================================================================

uint32_t dfu_crc32(const uint8_t* data, uint32_t length)
{
    // 标准逐位 CRC32 算法（与 zlib、上位机一致）
    // 多项式 0xEDB88320，初始值 0xFFFFFFFF，取反输出
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return ~crc;
}

// ============================================================================
// 元数据读写
// ============================================================================

const dfu_metadata_t* dfu_metadata_read(void)
{
    return (const dfu_metadata_t*)DFU_METADATA_START;
}

void dfu_metadata_write(const dfu_metadata_t* meta)
{
    if (meta == NULL) return;

    // 计算 Flash 偏移（相对于 Flash 基地址）
    uint32_t offset = DFU_METADATA_START - FLASH_BASE_ADDR;

    // 关闭中断，防止 Flash 写入期间被打断
    uint32_t ints = save_and_disable_interrupts();

    // 擦除扇区（4KB），期间喂看门狗防止复位
    watchdog_update();
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    watchdog_update();

    // 写入数据（必须是 256 字节的倍数，使用静态 buffer 避免栈溢出）
    // DFU 元数据只有 68 字节，写入 256 字节（一个 Flash 页）足够
    static uint8_t buffer[256];
    memset(buffer, 0xFF, sizeof(buffer));
    memcpy(buffer, meta, sizeof(dfu_metadata_t));
    flash_range_program(offset, buffer, sizeof(buffer));
    watchdog_update();

    restore_interrupts(ints);
}

void dfu_set_command(dfu_command_t cmd)
{
    dfu_metadata_t meta;
    const dfu_metadata_t* current = dfu_metadata_read();

    // 复制现有元数据
    if (current->magic == DFU_METADATA_MAGIC) {
        memcpy(&meta, current, sizeof(dfu_metadata_t));
    } else {
        memset(&meta, 0, sizeof(dfu_metadata_t));
        meta.magic = DFU_METADATA_MAGIC;
        meta.version = DFU_METADATA_VERSION;
        meta.state = DFU_STATE_IDLE;
    }

    meta.command = cmd;
    dfu_metadata_write(&meta);
}

// ============================================================================
// 固件校验
// ============================================================================

bool dfu_verify_firmware(uint32_t app_addr, uint32_t size, uint32_t expected_crc)
{
    if (size == 0 || size > APP_MAX_SIZE) return false;

    // 检查向量表栈指针是否在 RAM 范围内（RP2350 RAM 520KB，范围 0x20000000 - 0x20081FFF）
    // 栈指针初始值通常是 RAM 末尾 + 1（栈向下增长），所以允许 0x20000000 - 0x20090000
    const uint32_t* vectors = (const uint32_t*)app_addr;
    uint32_t sp = vectors[0];
    if (sp < 0x20000000 || sp > 0x20090000) {
        return false;  // 栈指针无效
    }

    // 检查 Reset_Handler 是否在 Flash 范围内
    uint32_t reset_handler = vectors[1] & ~1U;  // 清除 Thumb 位
    if (reset_handler < app_addr || reset_handler >= app_addr + size) {
        return false;  // 入口点无效
    }

    // CRC32 校验（仅当 expected_crc 非零时执行）
    // 首次烧录或元数据无效时 expected_crc 可能为0，此时跳过CRC校验只做基本检查
    if (expected_crc != 0) {
        uint32_t calc_crc = dfu_crc32((const uint8_t*)app_addr, size);
        if (calc_crc != expected_crc) {
            return false;  // CRC 不匹配
        }
    }

    return true;  // 所有检查通过
}

// ============================================================================
// 跳转到应用程序
// ============================================================================

void dfu_jump_to_app(uint32_t app_addr)
{
    // 最简单的跳转方法：直接设置 MSP 并跳转到 Reset_Handler
    const uint32_t* vectors = (const uint32_t*)app_addr;

    // 关闭中断
    __asm volatile ("cpsid i" ::: "memory");

    // 设置向量表偏移
    SCB_VTOR_ADDR = app_addr;

    // 设置栈指针
    __asm volatile ("msr msp, %0" :: "r" (vectors[0]) : "memory");

    // 数据同步屏障
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    // 跳转到 Reset_Handler（使用 bx 指令，自动处理 Thumb 位）
    __asm volatile (
        "mov r0, %0\n"
        "ldr r1, [r0, #4]\n"  // 读取 Reset_Handler 地址
        "bx r1\n"              // 跳转
        :: "r" (app_addr) : "r0", "r1"
    );

    // 不应到达这里
    while (1) {
        __asm volatile ("nop");
    }
}


// ============================================================================
// 固件签名验证（HMAC-SHA256）
// ============================================================================

/* 固件签名密钥（32字节，编译时嵌入 Bootloader）
 *
 * 安全说明：
 * - 此密钥用于 HMAC-SHA256 固件签名验证
 * - 生产环境应使用随机生成的密钥，并妥善保管
 * - 密钥泄露将导致攻击者可以伪造固件签名
 * - 建议在量产前使用 scripts/generate_signing_key.py 生成新密钥
 *
 * 当前密钥为演示用途，生产环境必须替换！
 */
static const uint8_t s_firmware_signature_key[FIRMWARE_SIGNATURE_KEY_LEN] = {
    0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x7A, 0x8B,
    0x9C, 0xAD, 0xBE, 0xCF, 0x01, 0x12, 0x23, 0x34,
    0x45, 0x56, 0x67, 0x78, 0x89, 0x9A, 0xAB, 0xBC,
    0xCD, 0xDE, 0xEF, 0xF0, 0x0A, 0x1B, 0x2C
};

/* 签名验证开关（默认开启） */
static bool s_signature_verify_enabled = true;

const uint8_t* dfu_get_signature_key(void)
{
    return s_firmware_signature_key;
}

void dfu_set_signature_verify_enabled(bool enabled)
{
    s_signature_verify_enabled = enabled;
}

bool dfu_is_signature_verify_enabled(void)
{
    return s_signature_verify_enabled;
}

firmware_signature_status_t dfu_verify_firmware_signature(uint32_t fw_addr, uint32_t fw_size)
{
    /* 签名格式：固件末尾追加 36 字节
     *   - 4 字节魔数 (FIRMWARE_SIGNATURE_MAGIC)
     *   - 32 字节 HMAC-SHA256 签名
     *
     * 签名计算范围：整个固件（不包括末尾的36字节签名）
     */

    /* 检查固件大小是否足够包含签名 */
    if (fw_size < (FIRMWARE_SIGNATURE_SIZE + 4)) {
        return FW_SIGNATURE_UNSIGNED;  /* 太小，不可能有签名 */
    }

    /* 读取签名魔数（固件末尾前36字节的前4字节） */
    uint32_t sig_offset = fw_size - FIRMWARE_SIGNATURE_SIZE - 4;
    const uint8_t* fw_data = (const uint8_t*)fw_addr;
    uint32_t magic = ((uint32_t)fw_data[sig_offset] << 24) |
                      ((uint32_t)fw_data[sig_offset + 1] << 16) |
                      ((uint32_t)fw_data[sig_offset + 2] << 8) |
                      ((uint32_t)fw_data[sig_offset + 3]);

    if (magic != FIRMWARE_SIGNATURE_MAGIC) {
        return FW_SIGNATURE_UNSIGNED;  /* 无签名魔数，未签名固件 */
    }

    /* 如果签名验证被禁用，直接返回有效（用于调试） */
    if (!s_signature_verify_enabled) {
        return FW_SIGNATURE_VALID;
    }

    /* 读取签名（固件末尾32字节） */
    const uint8_t* signature = fw_data + sig_offset + 4;

    /* 计算 HMAC-SHA256（签名范围：整个固件，不包括末尾的36字节签名） */
    uint8_t computed[HMAC_SHA256_DIGEST_SIZE];
    hmac_sha256(s_firmware_signature_key, FIRMWARE_SIGNATURE_KEY_LEN,
                fw_data, sig_offset, computed);

    /* 恒定时间比较（防止时序攻击） */
    uint8_t diff = 0;
    for (int i = 0; i < FIRMWARE_SIGNATURE_SIZE; i++) {
        diff |= computed[i] ^ signature[i];
    }

    return (diff == 0) ? FW_SIGNATURE_VALID : FW_SIGNATURE_INVALID;
}
