/**
 * @file capability.c
 * @brief 运行时能力位图与设备标志实现（功能角色分离，协议 v0.2）
 */
#include "features.h"
#include "protocol/capability.h"

/* 消费者基础能力位：核心 HID 功能始终编入，与 FEATURE_* 开关无关。
 * 对应 bit0-6 与 bit11-15（bit7-9 为可裁剪的诊断能力，见下）。
 * bit10 CAP_DFU_UPGRADE 不在此列：它表示“自定义 in-app DFU bootloader”，
 * 随 FEATURE_DFU_BOOTLOADER 置位（见 CAP_DFU_BOOTLOADER_BITMAP）。
 * RP2350 原生 BOOTSEL UF2 升级始终可用，但不占用能力位。 */
#define CAP_BASE_BITMAP ( \
    (1ULL << CAP_KEYBOARD_BASIC) | (1ULL << CAP_MOUSE_BASIC) | \
    (1ULL << CAP_JOYSTICK)      | (1ULL << CAP_ENCODER)      | \
    (1ULL << CAP_KEYMAP)        | (1ULL << CAP_MACRO)        | \
    (1ULL << CAP_CONFIG_STORAGE)| (1ULL << CAP_NKRO)         | \
    (1ULL << CAP_CONSUMER)      | (1ULL << CAP_GAMEPAD)      | \
    (1ULL << CAP_DPI_SLOTS)     | (1ULL << CAP_JOYSTICK_CALIB) )

/* 自定义 in-app DFU bootloader（CMD_ENTER_DFU + HID report 0x30~0x36）：
 * bootloader 随固件布局编入（FEATURE_DFU_BOOTLOADER=1）才上报 bit10；
 * 当前 CMake 临时禁用 bootloader，两个 profile 均注入 0，bit10 上报 0。 */
#if FEATURE_DFU_BOOTLOADER
#define CAP_DFU_BOOTLOADER_BITMAP (1ULL << CAP_DFU_UPGRADE)
#else
#define CAP_DFU_BOOTLOADER_BITMAP 0ULL
#endif

bool capability_mfg_factory_available(void)
{
    /* 阶段2过渡门控：量产固件 FEATURE_MFG_DEFAULT_LOCKED=1，上电默认锁定，
     * 不暴露 CAP_MFG_FACTORY_TEST/CAP_MFG_PROVISION；物理“长按 Fn”本地自检在
     * main.c 直接进入（不走 HID），不受此限。dev 固件默认解锁、制造能力全开。
     * TODO(阶段3): 改为读取 Provisioning 分区 mfg_state（出厂锁 + 售后会话解锁）。 */
#if FEATURE_MFG_DEFAULT_LOCKED
    return false;
#else
    return true;
#endif
}

uint64_t capability_bitmap_runtime(void)
{
    uint64_t bitmap = CAP_BASE_BITMAP;

#if FEATURE_PERF_MONITOR
    bitmap |= (1ULL << CAP_PERF_MONITOR);
#endif

#if FEATURE_FAULT_LOG
    bitmap |= (1ULL << CAP_FAULT_LOG);
#endif

#if FEATURE_KEY_STATS
    bitmap |= (1ULL << CAP_KEY_STATS);
#endif

#if FEATURE_RAW_DIAG
    bitmap |= (1ULL << CAP_DIAG_RAW_STATE);
#endif

#if FEATURE_DEBUG_UART
    bitmap |= (1ULL << CAP_DIAG_DEBUG_UART);
#endif

#if FEATURE_FACTORY_TEST
    if (capability_mfg_factory_available()) {
        bitmap |= (1ULL << CAP_MFG_FACTORY_TEST);
        bitmap |= (1ULL << CAP_MFG_PROVISION);
    }
#endif

    /* 自定义 DFU bootloader 跟随编译开关（BOOTSEL UF2 原生升级不占能力位） */
    return bitmap | CAP_DFU_BOOTLOADER_BITMAP;
}

uint8_t capability_device_flags(void)
{
    uint8_t flags = 0;

    /* 阶段2：量产固件（FEATURE_MFG_DEFAULT_LOCKED=1）上电默认报告出厂锁定标志。
     * TODO(阶段3): 改为从 Provisioning 分区读取出厂锁定位与本会话售后解锁位，
     *   已出厂锁定置 DEVICE_FLAG_MFG_SHIPPED_LOCKED；
     *   售后挑战-应答通过置 DEVICE_FLAG_MFG_SESSION_UNLOCKED。 */
#if FEATURE_FACTORY_TEST
    if (!capability_mfg_factory_available()) {
        flags |= DEVICE_FLAG_MFG_SHIPPED_LOCKED;
    }
#endif

    return flags;
}
