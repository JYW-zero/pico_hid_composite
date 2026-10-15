/*
 * src/board/config.c
 * 设备配置存储模块实现
 * 使用Flash最后8KB存储用户配置（双备份：A区 + B区）
 * 每次写入切换到另一个扇区，防止断电损坏
 */

#include "board/config.h"
#include "board/flash_layout.h"
#include "middleware/crc32.h"
#include "middleware/fault.h"
#include "middleware/flash_service.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

/* 获取Flash基地址，弱函数，测试时可以覆盖 */
__attribute__((weak)) uintptr_t config_get_flash_base(void)
{
    return 0x10000000U;
}

/* Flash 写入前后回调，弱函数，默认空实现
 * 上层（如双核同步）可以定义强函数覆盖
 */
__attribute__((weak)) void config_enter_flash_write(void)
{
    /* 默认空实现 */
}

__attribute__((weak)) void config_exit_flash_write(void)
{
    /* 默认空实现 */
}

/* ==================== 静态变量 ==================== */

static device_config_t s_current_config;
static bool s_initialized = false;
static uint16_t s_current_seq = 0;  /* 当前配置序列号 */

/* 默认配置缓存（在 config_init 中预计算，避免 config_get_default 运行时临时修改 s_current_config） */
static device_config_t s_default_cache;
static bool s_default_cache_loaded = false;
static uint32_t s_last_save_ms = 0; /* 上次保存时间，用于最小间隔保护 */
static uint32_t s_write_count = 0;  /* 总写入次数（磨损均衡统计） */
static int s_last_save_status = 0;  /* 最后一次保存状态 */

/* 配置保存最小间隔（毫秒），防止异常情况下频繁写入Flash */
#define CONFIG_SAVE_MIN_INTERVAL_MS  500U

/* ==================== 默认配置 ==================== */

/* 默认64键映射表（普通层）
 * 与main.c中的keymap保持一致
 * 注意：0xFF表示修饰键，具体在应用层处理
 */
static const uint8_t s_default_keymap[64] =
{
    /* 第1行（键1-14） */
    0x29, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2A,
    /* 第2行（键15-28） */
    0x2B, 0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18,
    0x0C, 0x12, 0x13, 0x2F, 0x30, 0x31,
    /* 第3行（键29-41） */
    0x39, 0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D,
    0x0E, 0x0F, 0x33, 0x34, 0x28,
    /* 第4行（键42-54） */
    0xFF, 0x1D, 0x1B, 0x06, 0x19, 0x05, 0x11, 0x10,
    0x36, 0x37, 0x38, 0xFF, 0x52,
    /* 第5行（键55-64） */
    0xFF, 0xFF, 0xFF, 0x2C, 0xFF, 0x45, 0x50, 0x51,
    0x4F, 0x4C
};

/* 默认Fn层映射表
 * Fn层功能：
 * - Fn + 数字键 = F1~F10
 * - Fn + - = F11
 * - Fn + = = F12
 * - Fn + 方向键 = Home/PageUp/End/PageDown
 * - 修饰键保持不变
 */
static const uint8_t s_default_fn_keymap[64] =
{
    /* 第1行（键1-14） */
    /* 0:Esc  1:1→F1  2:2→F2  3:3→F3  4:4→F4  5:5→F5  6:6→F6  7:7→F7 */
    0x29, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    /* 8:8→F8  9:9→F9  10:0→F10  11:-→F11  12:=→F12  13:Backspace */
    0x41, 0x42, 0x43, 0x44, 0x45, 0x2A,
    /* 第2行（键15-28） */
    /* 14:Tab  15:Q→音量+  16:W→音量-  17:E→静音  18:R→播放/暂停  19:T→下一曲  20:Y→上一曲 */
    /* 21-24:U-P 保持  25-27:[]\ 保持 */
    0x2B, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0x18,
    0x0C, 0x12, 0x13, 0x2F, 0x30, 0x31,
    /* 第3行（键29-41） */
    /* 28:CapsLock  29-40:A-L 保持  41:Enter */
    0x39, 0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D,
    0x0E, 0x0F, 0x33, 0x34, 0x28,
    /* 第4行（键42-54） */
    /* 41:左Shift  42-51:Z-/ 保持  52:右Shift  53:↑→PageUp */
    0xFF, 0x1D, 0x1B, 0x06, 0x19, 0x05, 0x11, 0x10,
    0x36, 0x37, 0x38, 0xFF, 0x4B,
    /* 第5行（键55-64） */
    /* 54:左Ctrl  55:左Win  56:左Alt  57:空格  58:右Alt  59:Fn键本身 */
    /* 60:←→Home  61:↓→PageDown  62:→→End  63:Delete */
    0xFF, 0xFF, 0xFF, 0x2C, 0xFF, 0xFE, 0x4A, 0x4E,
    0x4D, 0x4C
};

/* 默认配置 */
static const device_config_t s_default_config =
{
    .magic              = CONFIG_MAGIC,
    .version            = CONFIG_VERSION,
    .dpi                = DEFAULT_DPI,
    .joystick_deadzone  = DEFAULT_DEADZONE,
    .encoder_reverse    = DEFAULT_ENCODER_REV,
    .seq                = 0,
    .reserved           = {0},
    .keymap             = {0},  /* 后面单独初始化 */
    .fn_keymap          = {0},  /* 后面单独初始化 */
    .joystick_invert_x  = DEFAULT_JOY_INV_X,
    .joystick_invert_y  = DEFAULT_JOY_INV_Y,
    .encoder_steps      = DEFAULT_ENC_STEPS,
    .encoder_scroll_speed = DEFAULT_ENC_SCROLL,
    .joystick_sensitivity = DEFAULT_JOY_SENS,
    .reserved2          = {0},
    .length             = sizeof(device_config_t),
    .write_count        = 0,
    /* v5 新增字段：摇杆校准数据 */
    .joystick_center_x  = 2048,  /* 默认中心点 */
    .joystick_center_y  = 2048,
    .joystick_calibrated = 0,    /* 未校准，使用默认值 */
    .reserved3          = {0},
    /* v6 新增字段：光学传感器高级配置 */
    .dpi_slot           = {400, 800, 1600, 3200},
    .current_dpi_slot   = 2,      /* 默认第3档=1600 DPI */
    .lod_level          = 1,      /* 默认 LOD 等级1 */
    .angle_snapping     = 0,      /* 默认关闭角度捕捉 */
    .dpi_switch_key     = 0xFF,   /* 默认按键 */
    .reserved4          = {0},
    /* v7 新增字段：4层按键映射 */
    .layer2_keymap      = {0},    /* Layer2 默认全空 */
    .layer3_keymap      = {0},    /* Layer3 默认全空 */
    /* v8 新增字段：性能监控阈值 */
    .perf_thresholds     = {100, 100, 10, 600, 0, 0, 0, 0},  /* 默认阈值：tud_task=10ms, hid_config=10ms, macro=1ms, scheduler=60ms（单位100us） */
    .reserved5          = {0},
    .crc32              = 0
};

/* CRC32 计算使用公共模块 middleware/crc32.h */

/* ==================== 内部函数 ==================== */

/* 加载默认配置 */
static void load_default_config(void)
{
    memcpy(&s_current_config, &s_default_config, sizeof(device_config_t));
    memcpy(s_current_config.keymap, s_default_keymap, sizeof(s_current_config.keymap));
    memcpy(s_current_config.fn_keymap, s_default_fn_keymap, sizeof(s_current_config.fn_keymap));
    memset(s_current_config.macro_data, 0, CONFIG_MACRO_DATA_SIZE);  /* 默认宏配置：全0（空宏） */
    s_current_seq = 0;

    /* 计算CRC */
    uint32_t crc = crc32_calc((const uint8_t*)&s_current_config,
                              sizeof(device_config_t) - sizeof(uint32_t));
    s_current_config.crc32 = crc;
}

/* 校验配置基本完整性（魔数 + length + CRC，不检查版本号） */
static bool config_basic_valid(const device_config_t* cfg)
{
    if (cfg == NULL)
    {
        return false;
    }

    /* 检查魔数 */
    if (cfg->magic != CONFIG_MAGIC)
    {
        return false;
    }

    /* v4+ 检查 length 字段（旧版本没有 length 字段，值可能是随机的）
     * 只有 version >= 4 时才校验 length
     */
    if (cfg->version >= 0x0004U && cfg->length != sizeof(device_config_t))
    {
        return false;
    }

    /* 检查CRC */
    uint32_t calc_crc = crc32_calc((const uint8_t*)cfg,
                                   sizeof(device_config_t) - sizeof(uint32_t));
    if (calc_crc != cfg->crc32)
    {
        return false;
    }

    return true;
}

/* 序列号安全比较：考虑uint16回绕
 * 返回true表示a比b新
 * 使用有符号减法处理回绕：(int16_t)(a - b) > 0 表示a更新
 */
static bool seq_is_newer(uint16_t a, uint16_t b)
{
    return ((int16_t)(a - b)) > 0;
}

/* 前向声明：config_write_to_offset 在后面定义 */
static int config_write_to_offset(uint32_t flash_offset, const device_config_t* cfg);

/* 修复配置并写入指定Flash偏移（提取重复逻辑）
 * src_cfg: 源配置
 * flash_offset: 目标Flash偏移
 * new_seq: 新序列号
 * new_write_count: 新写入次数
 * 返回: 0成功，负数失败
 */
static int repair_and_write_config(const device_config_t* src_cfg,
                                   uint32_t flash_offset,
                                   uint16_t new_seq,
                                   uint32_t new_write_count)
{
    if (src_cfg == NULL)
    {
        return -1;
    }

    device_config_t repair_cfg;
    memcpy(&repair_cfg, src_cfg, sizeof(device_config_t));
    repair_cfg.seq = new_seq;
    repair_cfg.write_count = new_write_count;
    repair_cfg.magic = CONFIG_MAGIC;
    repair_cfg.version = CONFIG_VERSION;
    repair_cfg.length = sizeof(device_config_t);

    uint32_t crc = crc32_calc((const uint8_t*)&repair_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    repair_cfg.crc32 = crc;

    config_enter_flash_write();
    int ret = config_write_to_offset(flash_offset, &repair_cfg);
    config_exit_flash_write();

    return ret;
}

/* ==================== 版本迁移 ==================== */

/*
 * 版本迁移说明：
 * - 每次升级配置版本时，在这里添加对应的迁移函数
 * - 迁移原则：尽量保留用户配置，新增字段用默认值填充
 * - 只有无法迁移的老版本才重置为默认配置
 *
 * 版本历史：
 * - 0x0001: 初始版本（dpi, joystick_deadzone, encoder_reverse, keymap, fn_keymap）
 */

/* 从v1迁移到当前版本（v2新增了macro_data字段） */
static bool migrate_from_v1(const device_config_t* old_cfg, device_config_t* new_cfg)
{
    if (old_cfg == NULL || new_cfg == NULL)
    {
        return false;
    }

    /* 先加载默认配置作为基底 */
    load_default_config();
    memcpy(new_cfg, &s_current_config, sizeof(device_config_t));

    /* 复制旧版本中存在的字段 */
    new_cfg->dpi = old_cfg->dpi;
    new_cfg->joystick_deadzone = old_cfg->joystick_deadzone;
    new_cfg->encoder_reverse = old_cfg->encoder_reverse;
    memcpy(new_cfg->keymap, old_cfg->keymap, sizeof(new_cfg->keymap));
    memcpy(new_cfg->fn_keymap, old_cfg->fn_keymap, sizeof(new_cfg->fn_keymap));
    new_cfg->seq = old_cfg->seq;  /* 保留序列号 */

    /* 新增字段：macro_data 用默认值（已在load_default_config中初始化为0） */

    /* 重新计算CRC */
    new_cfg->magic = CONFIG_MAGIC;
    new_cfg->version = CONFIG_VERSION;
    uint32_t crc = crc32_calc((const uint8_t*)new_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    new_cfg->crc32 = crc;

    return true;
}

/* 从v2迁移到v3：v2已有macro_data，v3新增摇杆/编码器扩展字段 */
static bool migrate_from_v2(const device_config_t* old_cfg, device_config_t* new_cfg)
{
    if (old_cfg == NULL || new_cfg == NULL)
    {
        return false;
    }

    /* 先加载默认配置作为基底（新字段有默认值） */
    load_default_config();
    memcpy(new_cfg, &s_current_config, sizeof(device_config_t));

    /* 复制v2中存在的所有字段（偏移0到1325，即macro_data结束） */
    memcpy(new_cfg, old_cfg, offsetof(device_config_t, joystick_invert_x));

    /* 重新计算CRC */
    new_cfg->magic = CONFIG_MAGIC;
    new_cfg->version = CONFIG_VERSION;
    uint32_t crc = crc32_calc((const uint8_t*)new_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    new_cfg->crc32 = crc;

    return true;
}

/* 从v3迁移到v4：v3已有摇杆/编码器扩展，v4新增length和write_count字段 */
static bool migrate_from_v3(const device_config_t* old_cfg, device_config_t* new_cfg)
{
    if (old_cfg == NULL || new_cfg == NULL)
    {
        return false;
    }

    /* 先加载默认配置作为基底（新字段有默认值） */
    load_default_config();
    memcpy(new_cfg, &s_current_config, sizeof(device_config_t));

    /* 复制v3中存在的所有字段（偏移0到reserved2结束，即length之前） */
    memcpy(new_cfg, old_cfg, offsetof(device_config_t, length));

    /* 设置新字段默认值 */
    new_cfg->length = sizeof(device_config_t);
    new_cfg->write_count = 0;  /* 迁移时重置写入计数 */

    /* 重新计算CRC */
    new_cfg->magic = CONFIG_MAGIC;
    new_cfg->version = CONFIG_VERSION;
    uint32_t crc = crc32_calc((const uint8_t*)new_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    new_cfg->crc32 = crc;

    return true;
}

/* 从v4迁移到v5：v4已有length/write_count，v5新增摇杆校准数据 */
static bool migrate_from_v4(const device_config_t* old_cfg, device_config_t* new_cfg)
{
    if (old_cfg == NULL || new_cfg == NULL)
    {
        return false;
    }

    /* 先加载默认配置作为基底（新字段有默认值） */
    load_default_config();
    memcpy(new_cfg, &s_current_config, sizeof(device_config_t));

    /* 复制v4中存在的所有字段（偏移0到write_count结束，即joystick_center_x之前） */
    memcpy(new_cfg, old_cfg, offsetof(device_config_t, joystick_center_x));

    /* v5新字段使用默认值（已在load_default_config中设置）
     * joystick_center_x = 2048
     * joystick_center_y = 2048
     * joystick_calibrated = 0（未校准）
     */

    /* 重新计算CRC */
    new_cfg->magic = CONFIG_MAGIC;
    new_cfg->version = CONFIG_VERSION;
    uint32_t crc = crc32_calc((const uint8_t*)new_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    new_cfg->crc32 = crc;

    return true;
}

/* 从v5迁移到v6：v5已有摇杆校准数据，v6新增光学传感器高级配置 */
static bool migrate_from_v5(const device_config_t* old_cfg, device_config_t* new_cfg)
{
    if (old_cfg == NULL || new_cfg == NULL)
    {
        return false;
    }

    /* 先加载默认配置作为基底（新字段有默认值） */
    load_default_config();
    memcpy(new_cfg, &s_current_config, sizeof(device_config_t));

    /* 复制v5中存在的所有字段（偏移0到reserved3结束，即dpi_slot之前） */
    memcpy(new_cfg, old_cfg, offsetof(device_config_t, dpi_slot));

    /* v6新字段使用默认值（已在load_default_config中设置）
     * dpi_slot = {400, 800, 1600, 3200}
     * current_dpi_slot = 2 (1600 DPI)
     * lod_level = 1
     * angle_snapping = 0
     * dpi_switch_key = 0xFF
     */

    /* 重新计算CRC */
    new_cfg->magic = CONFIG_MAGIC;
    new_cfg->version = CONFIG_VERSION;
    uint32_t crc = crc32_calc((const uint8_t*)new_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    new_cfg->crc32 = crc;

    return true;
}

/* 从v6迁移到v7：v6已有光学传感器高级配置，v7新增4层按键映射 */
static bool migrate_from_v6(const device_config_t* old_cfg, device_config_t* new_cfg)
{
    if (old_cfg == NULL || new_cfg == NULL)
    {
        return false;
    }

    /* 先加载默认配置作为基底（新字段有默认值） */
    load_default_config();
    memcpy(new_cfg, &s_current_config, sizeof(device_config_t));

    /* 复制v6中存在的所有字段（偏移0到reserved4结束，即layer2_keymap之前） */
    memcpy(new_cfg, old_cfg, offsetof(device_config_t, layer2_keymap));

    /* v7新字段使用默认值（layer2_keymap/layer3_keymap全0） */

    /* 重新计算CRC */
    new_cfg->magic = CONFIG_MAGIC;
    new_cfg->version = CONFIG_VERSION;
    uint32_t crc = crc32_calc((const uint8_t*)new_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    new_cfg->crc32 = crc;

    return true;
}

/* 从v7迁移到v8：v7已有4层按键映射，v8新增性能监控阈值持久化 */
static bool migrate_from_v7(const device_config_t* old_cfg, device_config_t* new_cfg)
{
    if (old_cfg == NULL || new_cfg == NULL)
    {
        return false;
    }

    /* 先加载默认配置作为基底（新字段有默认值） */
    load_default_config();
    memcpy(new_cfg, &s_current_config, sizeof(device_config_t));

    /* 复制v7中存在的所有字段（偏移0到layer3_keymap结束，即perf_thresholds之前） */
    memcpy(new_cfg, old_cfg, offsetof(device_config_t, perf_thresholds));

    /* v8新字段使用默认值（perf_thresholds全0=禁用告警） */

    /* 重新计算CRC */
    new_cfg->magic = CONFIG_MAGIC;
    new_cfg->version = CONFIG_VERSION;
    uint32_t crc = crc32_calc((const uint8_t*)new_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    new_cfg->crc32 = crc;

    return true;
}

/* 尝试迁移旧版本配置到新版本
 * 返回值：true=迁移成功，false=无法迁移
 */
static bool config_try_migrate(const device_config_t* old_cfg, device_config_t* out_new_cfg)
{
    if (old_cfg == NULL || out_new_cfg == NULL)
    {
        return false;
    }

    /* 魔数不对，根本不是我们的配置，无法迁移 */
    if (old_cfg->magic != CONFIG_MAGIC)
    {
        return false;
    }

    /* 根据旧版本号选择迁移路径 */
    switch (old_cfg->version)
    {
        case 0x0001:
            return migrate_from_v1(old_cfg, out_new_cfg);

        case 0x0002:
            return migrate_from_v2(old_cfg, out_new_cfg);

        case 0x0003:
            return migrate_from_v3(old_cfg, out_new_cfg);

        case 0x0004:
            return migrate_from_v4(old_cfg, out_new_cfg);

        case 0x0005:
            return migrate_from_v5(old_cfg, out_new_cfg);

        case 0x0006:
            return migrate_from_v6(old_cfg, out_new_cfg);

        case 0x0007:
            return migrate_from_v7(old_cfg, out_new_cfg);

        default:
            /* 版本太老或未知，无法迁移 */
            return false;
    }
}

/* ==================== 双备份内部函数 ==================== */

/* 从指定偏移读取配置，并处理版本迁移
 * 返回值：true=成功（配置有效或迁移成功），false=失败
 */
static bool config_load_from_offset(uint32_t flash_offset, device_config_t* out_cfg)
{
    if (out_cfg == NULL)
    {
        return false;
    }

    const device_config_t* flash_cfg = (const device_config_t*)(config_get_flash_base() + flash_offset);

    if (!config_basic_valid(flash_cfg))
    {
        return false;
    }

    if (flash_cfg->version == CONFIG_VERSION)
    {
        /* 版本相同，直接复制 */
        memcpy(out_cfg, flash_cfg, sizeof(device_config_t));
        return true;
    }
    else
    {
        /* 版本不同，尝试迁移 */
        device_config_t migrated;
        if (config_try_migrate(flash_cfg, &migrated))
        {
            memcpy(out_cfg, &migrated, sizeof(device_config_t));
            return true;
        }
        else
        {
            return false;
        }
    }
}

/* 写入配置到指定偏移
 * 返回值：0=成功，其他=失败
 *
 * 注意：pico-sdk要求flash_range_program的大小必须是256字节（一页）的整数倍
 */
static int config_write_to_offset(uint32_t flash_offset, const device_config_t* cfg)
{
    if (cfg == NULL)
    {
        return -1;
    }

    /* 计算对齐到256字节的写入大小（向上取整） */
    const uint32_t config_size = sizeof(device_config_t);
    const uint32_t write_size = (config_size + FLASH_PAGE_SIZE - 1U) & ~(FLASH_PAGE_SIZE - 1U);

    /* 构造写入缓冲区（后面补0xFF，保持擦除状态）
     * 注意：使用 static 缓冲区避免栈溢出（write_size 最大约1.5KB）
     */
    static uint8_t write_buf[1536];  /* 6页 = 1536字节，足够容纳配置 */
    if (write_size > sizeof(write_buf))
    {
        fault_record(FAULT_LEVEL_ERROR, "config", "write buffer too small");
        return -2;
    }

    /* 先全部填充0xFF（擦除状态），再复制配置数据 */
    memset(write_buf, 0xFF, write_size);
    memcpy(write_buf, cfg, config_size);

    /* 使用Flash安全写入服务（内部自动暂停Core1、禁用中断） */
    if (!flash_service_erase(flash_offset, CONFIG_FLASH_SIZE))
    {
        fault_record(FAULT_LEVEL_ERROR, "config", "flash erase failed");
        return -2;
    }

    if (!flash_service_program(flash_offset, write_buf, write_size))
    {
        fault_record(FAULT_LEVEL_ERROR, "config", "flash program failed");
        return -3;
    }

    /* 验证 */
    const device_config_t* verify = (const device_config_t*)(config_get_flash_base() + flash_offset);
    if (!config_basic_valid(verify))
    {
        fault_record(FAULT_LEVEL_ERROR, "config", "write verify failed");
        return -3;
    }

    return 0;
}

/* ==================== 对外接口 ==================== */

void config_init(void)
{
    /* 确保Flash布局已初始化（检测Flash大小） */
    flash_layout_init();

    device_config_t cfg_a;
    device_config_t cfg_b;
    bool valid_a = config_load_from_offset(CONFIG_FLASH_OFFSET_A, &cfg_a);
    bool valid_b = config_load_from_offset(CONFIG_FLASH_OFFSET_B, &cfg_b);

    if (valid_a && valid_b)
    {
        /* 两个都有效，选序列号新的（使用安全比较处理uint16回绕） */
        if (seq_is_newer(cfg_a.seq, cfg_b.seq))
        {
            memcpy(&s_current_config, &cfg_a, sizeof(device_config_t));
            s_current_seq = cfg_a.seq;
            fault_record(FAULT_LEVEL_INFO, "config", "load from A (newer)");
        }
        else
        {
            memcpy(&s_current_config, &cfg_b, sizeof(device_config_t));
            s_current_seq = cfg_b.seq;
            fault_record(FAULT_LEVEL_INFO, "config", "load from B (newer)");
        }
    }
    else if (valid_a)
    {
        /* 只有A有效：加载A，并修复B区 */
        memcpy(&s_current_config, &cfg_a, sizeof(device_config_t));
        s_current_seq = cfg_a.seq;
        fault_record(FAULT_LEVEL_WARN, "config", "B invalid, load from A and repair B");

        s_initialized = true;
        /* 将A的内容写入B区（序列号+1，确保修复后的副本比原副本新） */
        repair_and_write_config(&cfg_a, CONFIG_FLASH_OFFSET_B,
                                cfg_a.seq + 1, cfg_a.write_count + 1);
    }
    else if (valid_b)
    {
        /* 只有B有效：加载B，并修复A区 */
        memcpy(&s_current_config, &cfg_b, sizeof(device_config_t));
        s_current_seq = cfg_b.seq;
        fault_record(FAULT_LEVEL_WARN, "config", "A invalid, load from B and repair A");

        s_initialized = true;
        /* 将B的内容写入A区（序列号+1，确保修复后的副本比原副本新） */
        repair_and_write_config(&cfg_b, CONFIG_FLASH_OFFSET_A,
                                cfg_b.seq + 1, cfg_b.write_count + 1);
    }
    else
    {
        /* 都无效，加载默认值并立即写入两个扇区，确保双备份 */
        load_default_config();
        fault_record(FAULT_LEVEL_WARN, "config", "both A/B invalid, load default and write both");

        /* 先标记已初始化，才能调用 config_save */
        s_initialized = true;

        /* 构造默认配置（序列号从1开始） */
        device_config_t default_cfg;
        memcpy(&default_cfg, &s_current_config, sizeof(device_config_t));

        /* 写入A区（序列号1） */
        repair_and_write_config(&default_cfg, CONFIG_FLASH_OFFSET_A, 1, 1);

        /* 写入B区（序列号2，确保B区更新） */
        repair_and_write_config(&default_cfg, CONFIG_FLASH_OFFSET_B, 2, 2);

        /* 更新当前序列号和写入次数 */
        s_current_seq = 2;
        s_current_config.seq = 2;
        s_write_count = 2;
        s_current_config.write_count = 2;
    }

    /* 防御性修复：字段如果为0或异常，设为默认值（兼容旧版本升级） */
    if (s_current_config.joystick_deadzone == 0 || s_current_config.joystick_deadzone > 4095) {
        s_current_config.joystick_deadzone = DEFAULT_DEADZONE;
        fault_record(FAULT_LEVEL_WARN, "config", "joystick_deadzone invalid, reset to default");
    }
    if (s_current_config.joystick_sensitivity == 0 || s_current_config.joystick_sensitivity > 5000) {
        s_current_config.joystick_sensitivity = DEFAULT_JOY_SENS;
        fault_record(FAULT_LEVEL_WARN, "config", "joystick_sensitivity invalid, reset to default");
    }
    if (s_current_config.encoder_steps == 0) {
        s_current_config.encoder_steps = DEFAULT_ENC_STEPS;
    }
    if (s_current_config.encoder_scroll_speed == 0) {
        s_current_config.encoder_scroll_speed = DEFAULT_ENC_SCROLL;
    }

    /* 恢复写入次数统计（从配置中读取） */
    s_write_count = s_current_config.write_count;

    s_initialized = true;

    /* 预计算默认配置缓存（避免 config_get_default 运行时临时修改 s_current_config 导致多核竞态） */
    if (!s_default_cache_loaded)
    {
        device_config_t saved = s_current_config;
        uint16_t saved_seq = s_current_seq;

        load_default_config();
        memcpy(&s_default_cache, &s_current_config, sizeof(device_config_t));

        /* 恢复当前配置 */
        memcpy(&s_current_config, &saved, sizeof(device_config_t));
        s_current_seq = saved_seq;

        s_default_cache_loaded = true;
    }
}

const device_config_t* config_get(void)
{
    if (!s_initialized)
    {
        return NULL;
    }
    return &s_current_config;
}

int config_save(const device_config_t* new_config)
{
    int status = 0;

    if (!s_initialized)
    {
        fault_record(FAULT_LEVEL_ERROR, "config", "save: not initialized");
        s_last_save_status = -1;
        return -1;
    }

    if (new_config == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "config", "save: null pointer");
        s_last_save_status = -1;
        return -1;
    }

    /* 最小间隔保护：防止异常情况下频繁写入Flash */
    uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    if (s_last_save_ms != 0 && (now_ms - s_last_save_ms) < CONFIG_SAVE_MIN_INTERVAL_MS)
    {
        fault_record(FAULT_LEVEL_WARN, "config", "save: too frequent, skipped");
        s_last_save_status = -4;
        return -4;
    }

    /* 构造新配置（两阶段提交：阶段1 - 准备数据） */
    device_config_t write_cfg;
    memcpy(&write_cfg, new_config, sizeof(device_config_t));

    /* 序列号加1（用于双备份判断最新） */
    write_cfg.seq = (uint16_t)(s_current_seq + 1u);
    write_cfg.magic = CONFIG_MAGIC;
    write_cfg.version = CONFIG_VERSION;
    write_cfg.length = sizeof(device_config_t);
    write_cfg.write_count = s_write_count + 1u;  /* 写入次数加1 */

    /* 重新计算CRC */
    uint32_t crc = crc32_calc((const uint8_t*)&write_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    write_cfg.crc32 = crc;

    /* 确定写入哪个扇区：写入另一个扇区（磨损均衡：交替写入）
     * 当前序列号是偶数 → 写A区
     * 当前序列号是奇数 → 写B区
     */
    uint32_t write_offset;
    if ((s_current_seq & 1u) == 0u)
    {
        write_offset = CONFIG_FLASH_OFFSET_A;
    }
    else
    {
        write_offset = CONFIG_FLASH_OFFSET_B;
    }

    /* 两阶段提交：阶段2 - 写入备份区（非当前激活区） */
    config_enter_flash_write();
    status = config_write_to_offset(write_offset, &write_cfg);
    config_exit_flash_write();

    if (status != 0)
    {
        fault_record(FAULT_LEVEL_ERROR, "config", "save: write failed");
        s_last_save_status = status;
        return status;
    }

    /* 两阶段提交：阶段3 - 校验写入结果（回读验证）
     * config_write_to_offset 内部已经做了校验，这里再次确认
     */
    const device_config_t* verify_cfg = (const device_config_t*)(config_get_flash_base() + write_offset);
    if (!config_basic_valid(verify_cfg) || verify_cfg->seq != write_cfg.seq)
    {
        fault_record(FAULT_LEVEL_ERROR, "config", "save: post-write verify failed");
        s_last_save_status = -5;
        return -5;
    }

    /* 两阶段提交：阶段4 - 切换激活（更新内存中的当前配置）
     * 只有写入和校验都成功后才更新当前配置
     * 如果在此之前断电，旧配置仍然有效
     */
    memcpy(&s_current_config, &write_cfg, sizeof(device_config_t));
    s_current_seq = write_cfg.seq;
    s_write_count = write_cfg.write_count;
    s_last_save_ms = now_ms;
    s_last_save_status = 0;

    fault_record(FAULT_LEVEL_INFO, "config", "save: ok");

    return 0;
}

void config_reset_default(void)
{
    /* 直接构造默认配置到局部变量，不修改s_current_config
     * 避免config_save失败时内存与Flash不一致
     */
    device_config_t default_cfg;
    memcpy(&default_cfg, &s_default_config, sizeof(device_config_t));
    memcpy(default_cfg.keymap, s_default_keymap, sizeof(default_cfg.keymap));
    memcpy(default_cfg.fn_keymap, s_default_fn_keymap, sizeof(default_cfg.fn_keymap));
    memset(default_cfg.macro_data, 0, CONFIG_MACRO_DATA_SIZE);

    /* 序列号加1 */
    default_cfg.seq = (uint16_t)(s_current_seq + 1u);
    default_cfg.length = sizeof(device_config_t);
    default_cfg.write_count = s_write_count + 1u;

    /* 重新计算CRC */
    uint32_t crc = crc32_calc((const uint8_t*)&default_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    default_cfg.crc32 = crc;

    /* 写入 */
    config_save(&default_cfg);
}

/* 强制加载默认配置（仅内存中生效，不写入Flash）
 * 用于配置完全损坏时的紧急恢复
 */
void config_load_default(void)
{
    load_default_config();
    s_current_config.length = sizeof(device_config_t);
    s_current_config.write_count = s_write_count;
    s_initialized = true;

    /* 重新计算CRC */
    uint32_t crc = crc32_calc((const uint8_t*)&s_current_config,
                              sizeof(device_config_t) - sizeof(uint32_t));
    s_current_config.crc32 = crc;

    fault_record(FAULT_LEVEL_WARN, "config", "load_default: forced default config (not saved)");
}

/* 获取总写入次数 */
uint32_t config_get_write_count(void)
{
    return s_write_count;
}

/* 获取最后一次保存状态 */
int config_get_last_save_status(void)
{
    return s_last_save_status;
}

const device_config_t* config_get_default(void)
{
    /* 默认配置在 config_init 中预计算并缓存，这里直接返回，无副作用 */
    if (!s_default_cache_loaded)
    {
        /* 防御性：如果 config_init 未调用，返回 NULL */
        return NULL;
    }
    return &s_default_cache;
}










