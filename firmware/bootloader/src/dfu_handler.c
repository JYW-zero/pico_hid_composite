/**
 * @file dfu_handler.c
 * @brief Bootloader DFU 协议处理实现
 *
 * 自定义 HID 升级协议：
 * - Report ID 0x30: 状态查询（GET）/ 状态响应
 * - Report ID 0x31: 擦除应用程序区（SET，4字节大小）
 * - Report ID 0x32: 写入数据块（SET，4字节偏移 + 58字节数据）
 * - Report ID 0x33: 校验CRC（SET，4字节大小 + 4字节CRC）
 * - Report ID 0x34: 完成升级（SET，4字节版本号）→ 重启
 * - Report ID 0x35: 中止升级
 */
#include "dfu_handler.h"
#include "device/dfu/dfu_metadata.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include <string.h>

#define BOOTLOADER_VERSION_MAJOR  1
#define BOOTLOADER_VERSION_MINOR  0

// 每次传输的数据大小（58字节，加上4字节偏移=62字节报告）
#define DFU_DATA_CHUNK_SIZE  58

// 状态
static volatile dfu_state_t s_state = DFU_STATE_IDLE;
static volatile dfu_error_t s_error = DFU_ERR_NONE;
static volatile uint32_t s_write_offset = 0;
static volatile uint32_t s_expected_size = 0;
static volatile uint32_t s_received_crc = 0;
static volatile bool s_upgrade_active = false;
static volatile uint32_t s_last_activity = 0;

// 临时缓冲区（用于 CRC 计算）
static uint8_t s_crc_buffer[DFU_DATA_CHUNK_SIZE];

// 前向声明
static void handle_erase(const uint8_t* buffer, uint16_t bufsize);
static void handle_data(const uint8_t* buffer, uint16_t bufsize);
static void handle_verify(const uint8_t* buffer, uint16_t bufsize);
static void handle_finish(const uint8_t* buffer, uint16_t bufsize);
static void handle_abort(void);
static void handle_boot_app(void);

// ============================================================================
// 初始化
// ============================================================================

void dfu_handler_init(void)
{
    s_state = DFU_STATE_IDLE;
    s_error = DFU_ERR_NONE;
    s_write_offset = 0;
    s_expected_size = 0;
    s_received_crc = 0;
    s_upgrade_active = false;
    s_last_activity = to_ms_since_boot(get_absolute_time());
}

void dfu_handler_task(void)
{
    // 超时检测：5分钟无活动则重置状态
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (s_upgrade_active && (now - s_last_activity) > 300000) {
        s_state = DFU_STATE_IDLE;
        s_upgrade_active = false;
        s_error = DFU_ERR_NONE;
    }
}

bool dfu_handler_is_active(void)
{
    return s_upgrade_active;
}

// ============================================================================
// GET_REPORT 处理
// ============================================================================

uint16_t dfu_get_report(uint8_t report_id, uint8_t* buffer, uint16_t reqlen)
{
    switch (report_id) {
        case DFU_REPORT_ID_STATUS: {
            dfu_status_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.state = s_state;
            resp.error = s_error;
            resp.write_offset = s_write_offset;
            resp.expected_size = s_expected_size;
            resp.received_crc = s_received_crc;
            resp.version_major = BOOTLOADER_VERSION_MAJOR;
            resp.version_minor = BOOTLOADER_VERSION_MINOR;
            resp.app_start = APP_START_ADDR;
            resp.app_max_size = APP_MAX_SIZE;

            uint16_t len = (reqlen < sizeof(resp)) ? reqlen : sizeof(resp);
            memcpy(buffer, &resp, len);
            return len;
        }
        default:
            return 0;
    }
}

// ============================================================================
// SET_REPORT 处理
// ============================================================================

void dfu_set_report(uint8_t report_id, const uint8_t* buffer, uint16_t bufsize)
{
    s_last_activity = to_ms_since_boot(get_absolute_time());

    switch (report_id) {
        case DFU_REPORT_ID_ERASE:
            handle_erase(buffer, bufsize);
            break;
        case DFU_REPORT_ID_DATA:
            handle_data(buffer, bufsize);
            break;
        case DFU_REPORT_ID_VERIFY:
            handle_verify(buffer, bufsize);
            break;
        case DFU_REPORT_ID_FINISH:
            handle_finish(buffer, bufsize);
            break;
        case DFU_REPORT_ID_ABORT:
            handle_abort();
            break;
        case DFU_REPORT_ID_BOOT_APP:
            handle_boot_app();
            break;
        default:
            s_error = DFU_ERR_INVALID_CMD;
            break;
    }
}

// ============================================================================
// 擦除命令
// ============================================================================

static void handle_erase(const uint8_t* buffer, uint16_t bufsize)
{
    if (bufsize < 4) {
        s_error = DFU_ERR_INVALID_CMD;
        return;
    }

    uint32_t size;
    memcpy(&size, buffer, 4);

    if (size == 0 || size > APP_MAX_SIZE) {
        s_error = DFU_ERR_INVALID_SIZE;
        return;
    }

    // 保存旧固件信息（用于回滚）
    const dfu_metadata_t* old_meta = dfu_metadata_read();
    dfu_metadata_t new_meta;
    memset(&new_meta, 0, sizeof(new_meta));
    new_meta.magic = DFU_METADATA_MAGIC;
    new_meta.version = DFU_METADATA_VERSION;

    if (old_meta->magic == DFU_METADATA_MAGIC) {
        new_meta.old_fw_size = old_meta->new_fw_size;
        new_meta.old_fw_crc32 = old_meta->new_fw_crc32;
        new_meta.old_fw_version = old_meta->new_fw_version;
    }

    new_meta.state = DFU_STATE_READY;
    new_meta.new_fw_size = size;
    new_meta.write_offset = 0;
    dfu_metadata_write(&new_meta);

    // 擦除 Flash（按扇区擦除，4KB 一个扇区）
    uint32_t sectors = (size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    uint32_t erase_size = sectors * FLASH_SECTOR_SIZE;
    uint32_t flash_offset = APP_START_ADDR - FLASH_BASE_ADDR;

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_offset, erase_size);
    restore_interrupts(ints);

    s_state = DFU_STATE_READY;
    s_error = DFU_ERR_NONE;
    s_write_offset = 0;
    s_expected_size = size;
    s_received_crc = 0;
    s_upgrade_active = true;
}

// ============================================================================
// 数据写入
// ============================================================================

static void handle_data(const uint8_t* buffer, uint16_t bufsize)
{
    if (s_state != DFU_STATE_READY && s_state != DFU_STATE_IDLE) {
        s_error = DFU_ERR_INVALID_CMD;
        return;
    }

    if (bufsize < 4) {
        s_error = DFU_ERR_INVALID_CMD;
        return;
    }

    uint32_t offset;
    memcpy(&offset, buffer, 4);

    uint16_t data_len = bufsize - 4;
    if (data_len > DFU_DATA_CHUNK_SIZE) {
        data_len = DFU_DATA_CHUNK_SIZE;
    }

    if (offset + data_len > s_expected_size) {
        s_error = DFU_ERR_INVALID_SIZE;
        return;
    }

    // 写入 Flash（必须按 256 字节对齐写入）
    // 这里简化处理：先缓存，凑够 256 字节再写入
    // 实际实现中，上位机应保证偏移和大小对齐
    uint32_t flash_offset = (APP_START_ADDR - FLASH_BASE_ADDR) + offset;

    uint32_t ints = save_and_disable_interrupts();

    // 如果偏移是 256 的倍数且数据长度是 256，直接写入
    if ((offset % 256 == 0) && (data_len == 256)) {
        flash_range_program(flash_offset, buffer + 4, 256);
    } else {
        // 非对齐写入：读取当前扇区，修改后写回
        // 简化：使用 256 字节缓冲区
        static uint8_t write_buf[256];
        uint32_t sector_base = offset & ~0xFF;
        uint32_t sector_offset = offset & 0xFF;

        // 读取当前 256 字节
        memcpy(write_buf, (const void*)(APP_START_ADDR + sector_base), 256);

        // 修改数据
        memcpy(write_buf + sector_offset, buffer + 4, data_len);

        // 写回（注意：需要先擦除，但我们假设整个区域已在 erase 阶段擦除）
        flash_range_program((APP_START_ADDR - FLASH_BASE_ADDR) + sector_base, write_buf, 256);
    }

    restore_interrupts(ints);

    // 更新写入偏移
    if (offset + data_len > s_write_offset) {
        s_write_offset = offset + data_len;
    }

    s_error = DFU_ERR_NONE;
    s_upgrade_active = true;
}

// ============================================================================
// 校验命令
// ============================================================================

static void handle_verify(const uint8_t* buffer, uint16_t bufsize)
{
    if (bufsize < 8) {
        s_error = DFU_ERR_INVALID_CMD;
        return;
    }

    uint32_t size, expected_crc;
    memcpy(&size, buffer, 4);
    memcpy(&expected_crc, buffer + 4, 4);

    if (size != s_write_offset || size == 0) {
        s_error = DFU_ERR_INVALID_SIZE;
        return;
    }

    // 计算 CRC
    uint32_t calc_crc = dfu_crc32((const uint8_t*)APP_START_ADDR, size);

    if (calc_crc != expected_crc) {
        s_error = DFU_ERR_VERIFY_FAILED;
        s_state = DFU_STATE_ERROR;
        return;
    }

    // 校验通过，更新元数据
    const dfu_metadata_t* meta = dfu_metadata_read();
    dfu_metadata_t new_meta;
    if (meta->magic == DFU_METADATA_MAGIC) {
        memcpy(&new_meta, meta, sizeof(dfu_metadata_t));
    } else {
        memset(&new_meta, 0, sizeof(new_meta));
        new_meta.magic = DFU_METADATA_MAGIC;
        new_meta.version = DFU_METADATA_VERSION;
    }

    new_meta.state = DFU_STATE_VERIFIED;
    new_meta.new_fw_size = size;
    new_meta.new_fw_crc32 = expected_crc;
    new_meta.write_offset = size;
    dfu_metadata_write(&new_meta);

    s_state = DFU_STATE_VERIFIED;
    s_error = DFU_ERR_NONE;
    s_received_crc = expected_crc;
    s_upgrade_active = true;
}

// ============================================================================
// 完成升级
// ============================================================================

static void handle_finish(const uint8_t* buffer, uint16_t bufsize)
{
    if (s_state != DFU_STATE_VERIFIED) {
        s_error = DFU_ERR_INVALID_CMD;
        return;
    }

    uint32_t version = 0;
    if (bufsize >= 4) {
        memcpy(&version, buffer, 4);
    }

    // 更新版本号
    const dfu_metadata_t* meta = dfu_metadata_read();
    dfu_metadata_t new_meta;
    memcpy(&new_meta, meta, sizeof(dfu_metadata_t));
    new_meta.new_fw_version = version;
    new_meta.state = DFU_STATE_VERIFIED;
    new_meta.command = 0;
    dfu_metadata_write(&new_meta);

    // 延迟 100ms 后重启
    sleep_ms(100);
    watchdog_reboot(0, 0, 100);
    while (1) {}
}

// ============================================================================
// 中止升级
// ============================================================================

static void handle_abort(void)
{
    s_state = DFU_STATE_IDLE;
    s_error = DFU_ERR_NONE;
    s_write_offset = 0;
    s_expected_size = 0;
    s_received_crc = 0;
    s_upgrade_active = false;

    // 重置元数据
    const dfu_metadata_t* meta = dfu_metadata_read();
    if (meta->magic == DFU_METADATA_MAGIC) {
        dfu_metadata_t new_meta;
        memcpy(&new_meta, meta, sizeof(dfu_metadata_t));
        new_meta.state = DFU_STATE_IDLE;
        new_meta.command = 0;
        dfu_metadata_write(&new_meta);
    }
}

// ============================================================================
// 启动应用程序
// ============================================================================

static void handle_boot_app(void)
{
    // 检查应用程序是否有效（基本检查：栈指针和 Reset_Handler）
    const uint32_t* vectors = (const uint32_t*)APP_START_ADDR;
    uint32_t sp = vectors[0];
    uint32_t reset_handler = vectors[1] & ~1U;

    // 栈指针应在 RAM 范围内
    if (sp < 0x20000000 || sp > 0x20090000) {
        s_error = DFU_ERR_INVALID_CMD;
        return;
    }

    // Reset_Handler 应在 Flash 应用区范围内
    if (reset_handler < APP_START_ADDR || reset_handler >= APP_START_ADDR + APP_MAX_SIZE) {
        s_error = DFU_ERR_INVALID_CMD;
        return;
    }

    // 应用程序有效，跳转到应用程序
    // 此函数不会返回
    dfu_jump_to_app(APP_START_ADDR);
    while (1) { }  // 不应到达这里
}
