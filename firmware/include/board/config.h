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
#include <stddef.h>
#include "board/flash_layout.h"
#include "version.h"

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

/* 配置魔数和版本号（统一从 version.h 引用，避免重复定义） */
/* CONFIG_MAGIC 和 CONFIG_VERSION 已在 version.h 中定义 */

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
    /* v4 新增字段 */
    uint16_t length;              /* 配置结构总长度（字节），用于完整性校验 */
    uint32_t write_count;         /* 总写入次数，用于磨损均衡统计 */
    /* v5 新增字段：摇杆校准数据 */
    uint16_t joystick_center_x;   /* 摇杆 X 轴中心点（ADC 值，0-4095，默认2048） */
    uint16_t joystick_center_y;   /* 摇杆 Y 轴中心点（ADC 值，0-4095，默认2048） */
    uint8_t  joystick_calibrated; /* 摇杆是否已校准：0=未校准(用默认),1=已校准 */
    uint8_t  reserved3[1];        /* 保留对齐；reserved3[0] 复用于存储编码器模式 (encoder_mode_e)，0=KEYBOARD,1=ZOOM,2=VOLUME,3=BRIGHTNESS,4=CUSTOM */
    /* v6 新增字段：光学传感器高级配置 */
    uint16_t dpi_slot[4];         /* 4档 DPI 值（每档2字节，默认400/800/1600/3200） */
    uint8_t  current_dpi_slot;    /* 当前 DPI 档位（0-3，默认2=1600） */
    uint8_t  lod_level;           /* LOD 静默高度等级（0-3，默认1） */
    uint8_t  angle_snapping;      /* 角度捕捉开关：0=关闭，1=开启 */
    uint8_t  dpi_switch_key;      /* DPI 切换按键索引（0xFF=默认按键） */
    uint8_t  reserved4[2];        /* 保留对齐 */
    /* v7 新增字段：4层按键映射（Layer2/Layer3） */
    uint8_t  layer2_keymap[64];   /* 第3层按键映射（扩展层1） */
    uint8_t  layer3_keymap[64];   /* 第4层按键映射（扩展层2） */
    /* v8 新增字段：性能监控阈值持久化 */
    uint16_t perf_thresholds[8];   /* 8个核心任务超时阈值（单位100us，0=禁用告警） */
    uint8_t  reserved5[2];         /* 保留对齐 */
    uint32_t crc32;              /* CRC32校验值（计算前面所有字段） */
} device_config_t;

/* ---- 持久化 ABI 红线（功能角色分离，见 docs/FEATURE_ROLE_SPLIT_PLAN.md）----
 * device_config_t 同时是 Flash 落盘格式与 HID 分块传输载荷，release/dev 双
 * Profile 编译同一份定义，必须二进制一致：字段只增不删，新字段只能追加在
 * crc32 之前并提升 CONFIG_VERSION + 提供迁移。下列断言锁定总大小与关键偏移
 * （数值为首次实测）；任何 ABI 改动都必须显式更新这些断言并验证旧配置可读。 */
_Static_assert(sizeof(device_config_t) == 1510U,
               "device_config_t ABI size must stay 1510 (append-only before crc32)");
_Static_assert(sizeof(device_config_t) == CONFIG_TOTAL_SIZE,
               "device_config_t size must match CONFIG_TOTAL_SIZE");
_Static_assert(offsetof(device_config_t, magic) == 0U, "magic must be at offset 0");
_Static_assert(offsetof(device_config_t, version) == 4U, "version offset drift");
_Static_assert(offsetof(device_config_t, perf_thresholds) == 1488U,
               "perf_thresholds offset drift (v8 field)");
_Static_assert(offsetof(device_config_t, crc32) == 1506U,
               "crc32 must remain the last field at offset 1506 (1510-4)");

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

/* 强制加载默认配置（不保存到Flash，仅内存中生效）
 * 与 config_reset_default 的区别：本函数不写入Flash
 */
void config_load_default(void);

/* 获取默认配置 */
const device_config_t* config_get_default(void);

/* 获取总写入次数（用于磨损均衡监控） */
uint32_t config_get_write_count(void);

/* 获取最后一次保存的结果状态（0=成功，其他=失败） */
int config_get_last_save_status(void);

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

