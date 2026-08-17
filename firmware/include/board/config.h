/*
 * include/board/config.h
 * 设备配置存储模块
 * 使用Flash最后8KB存储用户配置（双备份：A区 + B区）
 * 含CRC32校验，损坏自动恢复默认值
 * 支持版本迁移，固件升级不丢配置
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "board/flash_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置常量定义 ==================== */

/* Flash配置区大小：4KB（一个扇区大小） */
#define CONFIG_FLASH_SIZE     FLASH_LAYOUT_SECTOR_SIZE  /* 4096 bytes */

/*
 * 注意：Flash地址不再使用硬编码宏！
 * 请使用 flash_layout_config_a_offset() / flash_layout_config_b_offset()
 * 或 flash_layout_config_a_addr() / flash_layout_config_b_addr()
 *
 * 这些函数会在运行时根据检测到的Flash大小动态计算地址。
 * 布局（从Flash末尾向前）：
 *   最后第1个扇区: 配置B区
 *   最后第2个扇区: 配置A区
 */

/* 兼容旧代码的宏（不推荐使用，请改用函数） */
#define CONFIG_FLASH_OFFSET_A  flash_layout_config_a_offset()
#define CONFIG_FLASH_OFFSET_B  flash_layout_config_b_offset()
#define CONFIG_FLASH_ADDR_A    flash_layout_config_a_addr()
#define CONFIG_FLASH_ADDR_B    flash_layout_config_b_addr()

/* 配置魔数：用于识别有效配置 */
#define CONFIG_MAGIC          0x5A5A5A5AU

/* 配置版本号：升级时用于兼容旧版本配置 */
#define CONFIG_VERSION        0x0003U

/* 默认DPI值 */
#define DEFAULT_DPI           1600U

/* 默认摇杆死区（ADC原始值，0-4095） */
#define DEFAULT_DEADZONE      100U

/* 默认编码器方向：0=正常，1=反转 */
#define DEFAULT_ENCODER_REV   0U

/* 默认摇杆灵敏度（定点数，1.0=1000，范围100-5000即0.1-5.0） */
#define DEFAULT_JOY_SENS      1000U

/* 默认摇杆X/Y反转：0=正常，1=反转（Y轴默认反转，因为物理方向与HID相反） */
#define DEFAULT_JOY_INV_X     0U
#define DEFAULT_JOY_INV_Y     1U

/* 默认编码器每格步数（1-10） */
#define DEFAULT_ENC_STEPS     1U

/* 默认编码器滚动速度（1-10） */
#define DEFAULT_ENC_SCROLL    3U

/* 宏配置大小：8个宏 × 148字节/宏 = 1184字节 */
#define CONFIG_MACRO_DATA_SIZE  (8U * 148U)

/* ==================== 配置结构体定义 ==================== */

/* 设备配置结构体
 * 注意：字段顺序不要随意改变，否则会导致旧配置失效
 * 新增字段请加在crc32之前，并增加版本号
 */
typedef struct __attribute__((packed))
{
    uint32_t magic;              /* 魔数 0x5A5A5A5A */
    uint16_t version;            /* 配置版本号 */
    uint16_t dpi;                /* OPTICAL_SENSOR DPI值 */
    uint16_t joystick_deadzone;  /* 摇杆死区（ADC原始值） */
    uint8_t  encoder_reverse;    /* 编码器方向：0=正常，1=反转 */
    uint16_t seq;                /* 配置序列号，每次写入加1，用于双备份判断最新 */
    uint8_t  reserved[1];        /* 保留字节，对齐用 */
    uint8_t  keymap[64];         /* 64键映射表（普通层） */
    uint8_t  fn_keymap[64];      /* 64键映射表（Fn层） */
    uint8_t  macro_data[CONFIG_MACRO_DATA_SIZE]; /* 宏配置原始数据 */
    /* v2 新增字段（加在crc32之前，不改变旧字段偏移） */
    uint8_t  joystick_invert_x;   /* 摇杆X轴反转：0=正常，1=反转 */
    uint8_t  joystick_invert_y;   /* 摇杆Y轴反转：0=正常，1=反转 */
    uint8_t  encoder_steps;       /* 编码器每格步数（1-10） */
    uint8_t  encoder_scroll_speed;/* 编码器滚动速度（1-10） */
    uint16_t joystick_sensitivity;/* 摇杆灵敏度（定点数，1.0=1000） */
    uint8_t  reserved2[2];        /* 保留对齐 */
    uint32_t crc32;              /* CRC32校验值（计算前面所有字段） */
} device_config_t;

/* ==================== 对外接口 ==================== */

/* 初始化配置模块：从Flash加载配置，失败则加载默认值 */
void config_init(void);

/* 获取当前配置指针（只读） */
const device_config_t* config_get(void);

/* 保存配置到Flash
 * 返回值：0=成功，其他=失败 */
int config_save(const device_config_t* new_config);

/* 恢复默认配置并保存 */
void config_reset_default(void);

/* 获取默认配置 */
const device_config_t* config_get_default(void);

/* ==================== 写入回调（弱函数） ==================== */
/* 写入 Flash 前后的回调，用于双核同步等
 * 默认空实现，上层可以定义强函数覆盖
 */
void config_enter_flash_write(void);
void config_exit_flash_write(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_CONFIG_H */

