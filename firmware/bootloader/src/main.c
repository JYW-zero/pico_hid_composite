/**
 * @file main.c
 * @brief Bootloader 主程序
 *
 * 功能：
 * 1. 上电检查 DFU 元数据，决定进入 DFU 模式还是启动应用
 * 2. DFU 模式下通过 HID 接口接收固件数据
 * 3. 写入 Flash，CRC32 校验
 * 4. 校验通过后更新元数据，重启进入新固件
 * 5. 升级失败可回滚到旧固件
 */
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "tusb.h"
#include "dfu_handler.h"
#include "device/dfu/dfu_metadata.h"
#include <string.h>
#include <stdio.h>

// pico-sdk 的 board_init 在某些配置下未显式声明，手动前向声明
void board_init(void);

// 状态 LED（Pico 2 板载 LED）
#define LED_PIN 25

// 进入 bootrom UF2 模式的触发引脚（可选，接按钮到 GND）
// 不接按钮时，按住 BOOTSEL + 复位仍可进入 bootrom UF2 模式（硬件行为）
#define UF2_TRIGGER_PIN 2

// 等待上位机连接的超时时间（毫秒）
#define DFU_TIMEOUT_MS 5000

// ============================================================================
// 函数前向声明
// ============================================================================
static bool check_should_enter_dfu(void);
static void blink_led(uint32_t interval_ms);
static void dfu_mode_loop(void);

// ============================================================================
// 主函数
// ============================================================================
int main(void)
{
    // 初始化 UART 调试输出（GP0=TX, GP1=RX, 115200）
    stdio_init_all();
    printf("\n=== Bootloader 启动 ===\n");

    // 初始化 LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // 注意：不使用 GPIO 触发 UF2 模式，因为 GPIO 2 可能被其他外设占用导致误触发
    // 用户可通过按住 BOOTSEL 按钮硬件进入 bootrom 的 UF2 模式（硬件行为，与 Bootloader 无关）
    // 如果需要 GPIO 触发，可取消下方注释并选择一个未被占用的 GPIO 引脚
    /*
    // 初始化 UF2 触发引脚（上拉输入，接按钮到 GND 触发）
    gpio_init(UF2_TRIGGER_PIN);
    gpio_set_dir(UF2_TRIGGER_PIN, GPIO_IN);
    gpio_pull_up(UF2_TRIGGER_PIN);

    // 检测是否需要进入 bootrom 的 UF2 模式
    if (gpio_get(UF2_TRIGGER_PIN) == 0) {
        printf("[BOOT] UF2触发引脚被拉低，进入bootrom UF2模式\n");
        rom_reset_usb_boot(0, 0);
        while (1) { }
    }
    */

    // 检查是否应该进入 DFU 模式
    bool enter_dfu = check_should_enter_dfu();

    if (!enter_dfu) {
        // 尝试启动应用程序
        const dfu_metadata_t* meta = dfu_metadata_read();

        // 确定应用程序大小和 CRC
        uint32_t app_size = 0;
        uint32_t app_crc = 0;

        if (meta->magic == DFU_METADATA_MAGIC &&
            (meta->state == DFU_STATE_VERIFIED || meta->state == DFU_STATE_SWITCHED)) {
            // 新固件已验证或已切换，使用元数据中的固件信息
            app_size = meta->new_fw_size;
            app_crc = meta->new_fw_crc32;
        } else {
            // 元数据无效（首次烧录或未经过DFU升级），使用整个应用程序区大小
            // CRC=0 表示跳过CRC校验，只做基本检查（栈指针、Reset_Handler）
            app_size = APP_MAX_SIZE;
            app_crc = 0;
        }

        // 校验应用程序（CRC32）
        if (app_size > 0 && dfu_verify_firmware(APP_START_ADDR, app_size, app_crc)) {
            // 固件签名验证（HMAC-SHA256）
            // 向后兼容：未签名固件（无签名魔数）允许启动，已签名固件必须验证通过
            firmware_signature_status_t sig_status = dfu_verify_firmware_signature(APP_START_ADDR, app_size);
            if (sig_status == FW_SIGNATURE_INVALID) {
                // 签名无效，拒绝启动，进入 DFU 模式
                printf("[BOOT] ❌ 固件签名验证失败，拒绝启动，进入DFU模式\n");
                // 记录错误到元数据（可选）
            } else {
                if (sig_status == FW_SIGNATURE_VALID) {
                    printf("[BOOT] ✅ 固件签名验证通过\n");
                } else {
                    printf("[BOOT] ⚠️  固件未签名，向后兼容模式启动\n");
                }

                // 应用有效，更新启动计数并跳转
                if (meta->magic == DFU_METADATA_MAGIC && meta->state == DFU_STATE_VERIFIED) {
                    // 新固件首次启动，标记为已切换
                    dfu_metadata_t new_meta;
                    memcpy(&new_meta, meta, sizeof(dfu_metadata_t));
                    new_meta.state = DFU_STATE_SWITCHED;
                    new_meta.boot_count = 0;
                    dfu_metadata_write(&new_meta);
                }

                // 跳转到应用程序
                dfu_jump_to_app(APP_START_ADDR);
            }
        }
        // 应用无效，进入 DFU 模式
    }

    // 进入 DFU 模式
    // 初始化板级硬件（USB 引脚、时钟等），必须在 tusb_init 之前调用
    // 注意：board_init() 会初始化 LED 和 UART，与 Bootloader 自己的 LED 初始化不冲突
    board_init();
    dfu_handler_init();
    tusb_init();

    printf("[BOOT] 已进入DFU模式，等待上位机连接...\n");

    // LED 快闪表示 DFU 模式
    uint32_t start_time = to_ms_since_boot(get_absolute_time());

    while (1) {
        tud_task();
        dfu_handler_task();

        // LED 闪烁
        blink_led(200);

        // 如果超时且没有升级活动，尝试启动应用（回滚）
        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_time;
        if (elapsed > DFU_TIMEOUT_MS && !dfu_handler_is_active()) {
            const dfu_metadata_t* meta = dfu_metadata_read();
            if (meta->magic == DFU_METADATA_MAGIC && meta->state == DFU_STATE_SWITCHED) {
                // 新固件启动失败，回滚
                if (meta->old_fw_size > 0 && meta->old_fw_crc32 != 0) {
                    // 标记回滚状态
                    dfu_metadata_t new_meta;
                    memcpy(&new_meta, meta, sizeof(dfu_metadata_t));
                    new_meta.state = DFU_STATE_ROLLBACK;
                    dfu_metadata_write(&new_meta);

                    // 重启，Bootloader 会检测到回滚状态并使用旧固件信息
                    watchdog_reboot(0, 0, 100);
                    while (1) {}
                }
            }
            // 无旧固件可回滚，继续等待
        }
    }

    return 0;
}

// ============================================================================
// 检查是否应该进入 DFU 模式
// ============================================================================
static bool check_should_enter_dfu(void)
{
    const dfu_metadata_t* meta = dfu_metadata_read();

    printf("[BOOT] DFU元数据: magic=0x%08X, version=%lu, state=%lu, command=%lu\n",
           meta->magic, meta->version, meta->state, meta->command);

    // 检查元数据是否有效
    if (meta->magic != DFU_METADATA_MAGIC) {
        printf("[BOOT] magic不匹配，直接启动应用\n");
        return false;  // 无元数据，直接尝试启动应用
    }

    // 检查是否有进入 DFU 的命令
    if (meta->command == DFU_CMD_ENTER_DFU) {
        printf("[BOOT] 收到ENTER_DFU命令，进入DFU模式\n");
        // 清除命令
        dfu_metadata_t new_meta;
        memcpy(&new_meta, meta, sizeof(dfu_metadata_t));
        new_meta.command = 0;
        dfu_metadata_write(&new_meta);
        return true;
    }

    // 检查状态
    switch (meta->state) {
        case DFU_STATE_READY:
            // 固件已写入但未校验，进入 DFU 模式完成校验
            return true;
        case DFU_STATE_VERIFIED:
        case DFU_STATE_ROLLBACK:
        case DFU_STATE_SWITCHED:
            // 固件已校验或已切换，正常启动
            return false;
        default:
            return false;
    }
}

// ============================================================================
// LED 闪烁
// ============================================================================
static void blink_led(uint32_t interval_ms)
{
    static uint32_t last_toggle = 0;
    static bool led_state = false;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_toggle >= interval_ms) {
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);
        last_toggle = now;
    }
}
