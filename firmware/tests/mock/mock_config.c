/*
 * tests/mock/mock_config.c
 * Mock版配置模块，用于单元测试
 * 用内存代替Flash，接口与真实config模块完全一致
 */

#include "board/config.h"
#include "middleware/fault.h"
#include <string.h>
#include <stddef.h>

/* ==================== 默认配置 ==================== */

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
    0xFF, 0xFF, 0xFF, 0x2C, 0xFF, 0xFE, 0x50, 0x51,
    0x4F, 0x4C
};

static const uint8_t s_default_fn_keymap[64] =
{
    /* 第1行（键1-14） */
    0x29, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x2A,
    /* 第2行（键15-28） */
    0x2B, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0x18,
    0x0C, 0x12, 0x13, 0x2F, 0x30, 0x31,
    /* 第3行（键29-41） */
    0x39, 0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D,
    0x0E, 0x0F, 0x33, 0x34, 0x28,
    /* 第4行（键42-54） */
    0xFF, 0x1D, 0x1B, 0x06, 0x19, 0x05, 0x11, 0x10,
    0x36, 0x37, 0x38, 0xFF, 0x4B,
    /* 第5行（键55-64） */
    0xFF, 0xFF, 0xFF, 0x2C, 0xFF, 0xFE, 0x4A, 0x4E,
    0x4D, 0x4C
};

/* ==================== 内部状态 ==================== */

static device_config_t s_current_config;
static bool s_initialized = false;

/* ==================== 辅助函数 ==================== */

static void load_default_config(void)
{
    memset(&s_current_config, 0, sizeof(device_config_t));
    s_current_config.magic = CONFIG_MAGIC;
    s_current_config.version = CONFIG_VERSION;
    s_current_config.dpi = DEFAULT_DPI;
    s_current_config.joystick_deadzone = DEFAULT_DEADZONE;
    s_current_config.encoder_reverse = DEFAULT_ENCODER_REV;
    s_current_config.seq = 0;
    memcpy(s_current_config.keymap, s_default_keymap, 64);
    memcpy(s_current_config.fn_keymap, s_default_fn_keymap, 64);
}

/* ==================== 对外接口 ==================== */

__attribute__((weak)) void config_init(void)
{
    load_default_config();
    s_initialized = true;
    fault_record(FAULT_LEVEL_INFO, "mock_config", "init complete (mock)");
}

__attribute__((weak)) const device_config_t* config_get(void)
{
    if (!s_initialized)
    {
        return NULL;
    }
    return &s_current_config;
}

__attribute__((weak)) int config_save(const device_config_t* new_config)
{
    if (!s_initialized)
    {
        fault_record(FAULT_LEVEL_ERROR, "mock_config", "save: not initialized");
        return -1;
    }
    if (new_config == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "mock_config", "save: null pointer");
        return -1;
    }

    memcpy(&s_current_config, new_config, sizeof(device_config_t));
    s_current_config.seq = (uint16_t)(s_current_config.seq + 1u);
    return 0;
}

__attribute__((weak)) void config_reset_default(void)
{
    load_default_config();
}

__attribute__((weak)) const device_config_t* config_get_default(void)
{
    /* 直接返回当前配置（因为我们的默认就是初始值） */
    /* 注意：真实实现里这个函数返回的是默认配置，不影响当前配置 */
    /* 这里简化处理，测试用足够了 */
    static device_config_t s_default_cache;
    static bool s_cache_valid = false;

    if (!s_cache_valid)
    {
        memset(&s_default_cache, 0, sizeof(device_config_t));
        s_default_cache.magic = CONFIG_MAGIC;
        s_default_cache.version = CONFIG_VERSION;
        s_default_cache.dpi = DEFAULT_DPI;
        s_default_cache.joystick_deadzone = DEFAULT_DEADZONE;
        s_default_cache.encoder_reverse = DEFAULT_ENCODER_REV;
        s_default_cache.seq = 0;
        memcpy(s_default_cache.keymap, s_default_keymap, 64);
        memcpy(s_default_cache.fn_keymap, s_default_fn_keymap, 64);
        s_cache_valid = true;
    }

    return &s_default_cache;
}

/* ==================== 测试专用接口 ==================== */

/* 测试用：直接设置keymap（普通层） */
__attribute__((weak)) void mock_config_set_keymap(const uint8_t keymap[64])
{
    if (!s_initialized)
    {
        config_init();
    }
    memcpy(s_current_config.keymap, keymap, 64);
}

/* 测试用：直接设置fn_keymap（Fn层） */
__attribute__((weak)) void mock_config_set_fn_keymap(const uint8_t fn_keymap[64])
{
    if (!s_initialized)
    {
        config_init();
    }
    memcpy(s_current_config.fn_keymap, fn_keymap, 64);
}

/* 测试用：重置为默认配置 */
__attribute__((weak)) void mock_config_reset(void)
{
    load_default_config();
    s_initialized = true;
}

