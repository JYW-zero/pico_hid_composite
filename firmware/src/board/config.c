/*
 * src/board/config.c
 * 设备配置存储模块实现
 * 使用Flash最后8KB存储用户配置（双备份：A区 + B区）
 * 每次写入切换到另一个扇区，防止断电损坏
 */

#include "board/config.h"
#include "board/flash_layout.h"
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
    .crc32              = 0
};

/* ==================== CRC32 计算 ==================== */

/* CRC32查表法，多项式0xEDB88320 */
static const uint32_t s_crc32_table[16] =
{
    0x00000000U, 0x1DB71064U, 0x3B6E20C8U, 0x26D930ACU,
    0x76DC4190U, 0x6B6B51F4U, 0x4DB26158U, 0x5005713CU,
    0xEDB88320U, 0xF00F9344U, 0xD6D6A3E8U, 0xCB61B38CU,
    0x9B64C2B0U, 0x86D3D2D4U, 0xA00AE278U, 0xBDBDF21CU
};

static uint32_t crc32_calc(const uint8_t* data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;

    for (uint32_t i = 0; i < len; i++)
    {
        uint8_t byte = data[i];
        crc = (crc >> 4) ^ s_crc32_table[(crc & 0x0FU) ^ (byte & 0x0FU)];
        crc = (crc >> 4) ^ s_crc32_table[(crc & 0x0FU) ^ (byte >> 4)];
    }

    return ~crc;
}

/* ==================== 内部函数 ==================== */

/* 加载默认配置 */
static void load_default_config(void)
{
    memcpy(&s_current_config, &s_default_config, sizeof(device_config_t));
    memcpy(s_current_config.keymap, s_default_keymap, 64);
    memcpy(s_current_config.fn_keymap, s_default_fn_keymap, 64);
    memset(s_current_config.macro_data, 0, CONFIG_MACRO_DATA_SIZE);  /* 默认宏配置：全0（空宏） */
    s_current_seq = 0;

    /* 计算CRC */
    uint32_t crc = crc32_calc((const uint8_t*)&s_current_config,
                              sizeof(device_config_t) - sizeof(uint32_t));
    s_current_config.crc32 = crc;
}

/* 校验配置基本完整性（魔数 + CRC，不检查版本号） */
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

    /* 检查CRC */
    uint32_t calc_crc = crc32_calc((const uint8_t*)cfg,
                                   sizeof(device_config_t) - sizeof(uint32_t));
    if (calc_crc != cfg->crc32)
    {
        return false;
    }

    return true;
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
    memcpy(new_cfg->keymap, old_cfg->keymap, 64);
    memcpy(new_cfg->fn_keymap, old_cfg->fn_keymap, 64);
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
        /* 两个都有效，选序列号大的（最新的） */
        if (cfg_a.seq >= cfg_b.seq)
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
        /* 只有A有效 */
        memcpy(&s_current_config, &cfg_a, sizeof(device_config_t));
        s_current_seq = cfg_a.seq;
        fault_record(FAULT_LEVEL_INFO, "config", "load from A");
    }
    else if (valid_b)
    {
        /* 只有B有效 */
        memcpy(&s_current_config, &cfg_b, sizeof(device_config_t));
        s_current_seq = cfg_b.seq;
        fault_record(FAULT_LEVEL_INFO, "config", "load from B");
    }
    else
    {
        /* 都无效，加载默认值 */
        load_default_config();
        fault_record(FAULT_LEVEL_WARN, "config", "both A/B invalid, load default");
    }

    s_initialized = true;
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
        return -1;
    }

    if (new_config == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "config", "save: null pointer");
        return -1;
    }

    /* 构造新配置 */
    device_config_t write_cfg;
    memcpy(&write_cfg, new_config, sizeof(device_config_t));

    /* 序列号加1 */
    write_cfg.seq = (uint16_t)(s_current_seq + 1u);
    write_cfg.magic = CONFIG_MAGIC;
    write_cfg.version = CONFIG_VERSION;

    /* 重新计算CRC */
    uint32_t crc = crc32_calc((const uint8_t*)&write_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    write_cfg.crc32 = crc;

    /* 确定写入哪个扇区：写入另一个扇区
     * 当前序列号是偶数 → 写A区
     * 当前序列号是奇数 → 写B区
     * 这样交替写入
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

    /* 写入前回调（用于双核同步等） */
    config_enter_flash_write();

    /* 写入 */
    status = config_write_to_offset(write_offset, &write_cfg);

    /* 写入后回调 */
    config_exit_flash_write();

    if (status != 0)
    {
        fault_record(FAULT_LEVEL_ERROR, "config", "save: write failed");
        return status;
    }

    /* 写入成功，更新当前配置 */
    memcpy(&s_current_config, &write_cfg, sizeof(device_config_t));
    s_current_seq = write_cfg.seq;

    return 0;
}

void config_reset_default(void)
{
    device_config_t default_cfg;
    load_default_config();
    memcpy(&default_cfg, &s_current_config, sizeof(device_config_t));

    /* 序列号加1 */
    default_cfg.seq = (uint16_t)(s_current_seq + 1u);

    /* 重新计算CRC */
    uint32_t crc = crc32_calc((const uint8_t*)&default_cfg,
                              sizeof(device_config_t) - sizeof(uint32_t));
    default_cfg.crc32 = crc;

    /* 写入 */
    config_save(&default_cfg);
}

const device_config_t* config_get_default(void)
{
    /* 临时加载默认配置到s_current_config，然后返回 */
    /* 注意：这个函数会修改s_current_config，调用后需要调用config_init重新加载 */
    /* 或者，我们用一个静态变量存默认配置 */
    static device_config_t s_default_cache;
    static bool s_default_loaded = false;

    if (!s_default_loaded)
    {
        device_config_t saved = s_current_config;
        uint16_t saved_seq = s_current_seq;

        load_default_config();
        memcpy(&s_default_cache, &s_current_config, sizeof(device_config_t));

        /* 恢复 */
        memcpy(&s_current_config, &saved, sizeof(device_config_t));
        s_current_seq = saved_seq;

        s_default_loaded = true;
    }

    return &s_default_cache;
}










