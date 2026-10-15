#ifndef PROTOCOL_CONFIG_HID_H
#define PROTOCOL_CONFIG_HID_H

#include <stdint.h>
#include "tusb.h"
#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 协议版本（统一从 version.h 引用） ==================== */

/* 配置 HID 协议版本
 * 上位机应读取此版本并与自身支持的版本比较，
 * v0.1 初始版本：
 *   - Device Index 字段（为无线接收器多设备寻址预留，有线版本固定 0xFF）
 *   - Software ID 字段（0~15循环，用于匹配请求响应，防止读到残留响应）
 *   - 标准化错误码（参考 HID++/Logitech 设计）
 * 如果主版本号不同，则不兼容，应提示用户升级。
 */
#define CONFIG_HID_PROTOCOL_VERSION_MAJOR  PROTOCOL_VERSION_MAJOR
#define CONFIG_HID_PROTOCOL_VERSION_MINOR  PROTOCOL_VERSION_MINOR
#define CONFIG_HID_PROTOCOL_VERSION        PROTOCOL_VERSION

/* ==================== 协议字段定义（统一从 version.h 引用） ==================== */

/* Device Index（设备索引）
 * 参考 HID++ 设计：0xFF = 直连设备（USB/蓝牙），1~6 = 接收器插槽1~6
 * 有线版本固定使用 0xFF，无线版本由接收器转发时填入对应插槽号
 *
 * 注意：DEVICE_INDEX_DIRECT 和 DEVICE_INDEX_BROADCAST 已在 version.h 中统一定义
 */

/* Software ID（软件 ID）
 * 0~15 循环使用，上位机每次命令分配一个递增的 SW ID
 * 固件响应时回显相同 SW ID，上位机据此匹配请求响应
 * SW ID=0 表示广播/通知（不需要匹配）
 *
 * 注意：SW_ID_BROADCAST、SW_ID_MIN、SW_ID_MAX 已在 version.h 中统一定义
 */

/* v3.1 命令格式（Output Report，8字节，不含 Report ID）
 * byte[0] = Device Index
 * byte[1] = Command ID
 * byte[2] = Software ID
 * byte[3] = Param Low
 * byte[4] = Param High
 * byte[5..7] = Reserved
 */
#define CMD_OFFSET_DEVICE_IDX  0
#define CMD_OFFSET_CMD         1
#define CMD_OFFSET_SW_ID       2
#define CMD_OFFSET_PARAM_LOW   3
#define CMD_OFFSET_PARAM_HIGH  4

/* v3.1 响应格式（Feature Report，8字节，不含 Report ID）
 * byte[0] = Status (0=成功，非0=错误码)
 * byte[1] = Command ID（回显）
 * byte[2] = Software ID（回显）
 * byte[3] = Device Index（回显）
 * byte[4] = Param Low
 * byte[5] = Param High
 * byte[6..7] = Reserved
 */
#define RESP_OFFSET_STATUS      0
#define RESP_OFFSET_CMD         1
#define RESP_OFFSET_SW_ID       2
#define RESP_OFFSET_DEVICE_IDX  3
#define RESP_OFFSET_PARAM_LOW   4
#define RESP_OFFSET_PARAM_HIGH  5

/* ==================== 错误码定义 ==================== */

typedef enum
{
    CONFIG_HID_OK = 0,              /* 成功 */
    CONFIG_HID_ERR_INVALID_CMD = 1,  /* 无效命令 */
    CONFIG_HID_ERR_INVALID_PARAM = 2, /* 无效参数 */
    CONFIG_HID_ERR_CONFIG_LOCKED = 3, /* 配置已锁定 */
    CONFIG_HID_ERR_CRC_MISMATCH = 4,  /* CRC 校验失败 */
    CONFIG_HID_ERR_INCOMPLETE = 5,    /* 配置块不完整 */
    CONFIG_HID_ERR_FLASH_WRITE = 6,    /* Flash 写入失败 */
    CONFIG_HID_ERR_TIMEOUT = 7,        /* 操作超时 */
    CONFIG_HID_ERR_BUSY = 8,           /* 设备忙 */
    CONFIG_HID_ERR_NOT_SUPPORTED = 9,  /* 功能不支持 */
    CONFIG_HID_ERR_VERSION_MISMATCH = 10, /* 版本不兼容 */
    CONFIG_HID_ERR_DIAG_LOCKED = 11, /* 诊断功能当前会话未授权（如已出厂锁定后清除 Fault，需售后解锁） */
    CONFIG_HID_ERR_MFG_LOCKED = 12,  /* 制造功能已出厂锁定 / 未授权 */
    CONFIG_HID_ERR_UNKNOWN = 0xFF      /* 未知错误 */
} config_hid_error_t;

/* ==================== ACK/NACK 响应结构 ==================== */

/* ACK/NACK 报告（Report ID = REPORT_ID_ACK）
 * 固件在收到控制命令后，通过此报告返回执行结果。
 * 上位机应等待此报告后再发送下一个命令。
 */
typedef struct __attribute__((packed))
{
    uint8_t report_id;      /* 报告 ID = REPORT_ID_ACK */
    uint8_t cmd;            /* 对应的命令码 */
    uint8_t status;         /* 0=ACK(成功), 1=NACK(失败) */
    uint8_t error_code;     /* 错误码（config_hid_error_t），成功时为 0 */
    uint16_t param;         /* 附加参数（如当前 DPI、层号等） */
    uint8_t reserved[2];    /* 保留对齐 */
} config_hid_ack_t;

/* ==================== 控制命令码 ==================== */

#define CMD_SAVE_CONFIG          0x01  /* 保存配置到 Flash */
#define CMD_RESET_CONFIG         0x02  /* 恢复出厂默认配置 */
#define CMD_REBOOT               0x03  /* 重启设备（需3次确认） */
#define CMD_ENTER_DFU            0x04  /* 进入 DFU 模式（需3次确认） */
#define CMD_APPLY_CONFIG         0x05  /* 应用临时配置（写入 Flash 并生效） */
#define CMD_RESET_STATS          0x06  /* 清零按键统计 */
#define CMD_CLEAR_FAULT          0x07  /* 清除错误日志 */
#define CMD_RESET_PERF           0x08  /* 重置性能统计 */
#define CMD_MACRO_PLAY           0x09  /* 播放宏（参数：宏ID） */
#define CMD_MACRO_STOP           0x0A  /* 停止宏（参数：宏ID，0xFF=停止所有） */
#define CMD_SET_PERF_ENABLE      0x0B  /* 设置性能监控开关（参数：1=开启，0=关闭） */
#define CMD_SET_JOYSTICK_DZ_RT   0x0C  /* 实时设置摇杆死区（参数：2字节小端，不写Flash） */
#define CMD_UNLOCK_CONFIG        0x0D  /* 解锁配置写入（需连续3次，5秒内） */
#define CMD_JOYSTICK_CALIBRATE   0x0E  /* 触发摇杆中心点校准（参数：采样次数） */
#define CMD_SWITCH_DPI           0x0F  /* 循环切换 DPI 档位（参数：0xFF=循环，0-3=指定档位） */
#define CMD_GET_VERSION          0x10  /* 获取版本信息（返回固件/配置/协议版本） */
#define CMD_NEGOTIATE_VERSION    0x11  /* 版本协商（参数：上位机协议版本，返回固件支持的版本） */
#define CMD_SWITCH_LAYER         0x12  /* 切换按键层（参数：0-3 层号） */
#define CMD_GET_CAPABILITY_INFO  0x13  /* 获取能力信息（参数：能力ID索引，ACK返回能力版本高字节=主版本低字节=次版本） */
#define CMD_ENTER_BOOTSEL        0x14  /* 进入官方 BOOTSEL UF2 模式（软件触发，相当于按住BOOTSEL键上电） */
#define CMD_SET_PERF_THRESHOLD   0x15  /* 设置性能监控任务超时阈值（参数：高字节=任务索引0-15，低字节=阈值/100us，0=禁用） */

/* ==================== 配置块传输 ==================== */

/* 配置块大小（定义在 usb_descriptors.h 中）
 * #define CONFIG_BLOCK_SIZE  62u
 */

/* 注意：CONFIG_BLOCK_COUNT、CONFIG_BLOCK_SIZE、CONFIG_TOTAL_SIZE 已在 version.h 中统一定义 */

/* 配置块 Report ID 范围
 * REPORT_ID_CONFIG_BLOCK0 = 5
 * REPORT_ID_CONFIG_BLOCK1 = 8
 * REPORT_ID_CONFIG_BLOCK2 = 9
 * 扩展块使用 REPORT_ID_CONFIG_EXT (21) + 块索引
 */

/* ==================== 设备信息结构 ==================== */

/* ==================== 能力协商（Capability Negotiation） ==================== */

/* 能力ID定义（Capability IDs）
 * 每个能力独立版本化，上位机根据能力位图判断功能兼容性
 * 能力位图中每个 bit 代表一个能力是否存在（1=支持，0=不支持）
 */
typedef enum {
    CAP_KEYBOARD_BASIC      = 0,  /* 键盘基础：64键矩阵、标准键盘报告 */
    CAP_MOUSE_BASIC         = 1,  /* 鼠标基础：PAW3395、标准鼠标报告 */
    CAP_JOYSTICK            = 2,  /* 摇杆：ADC摇杆、死区/灵敏度 */
    CAP_ENCODER             = 3,  /* 编码器：旋转编码器、滚轮 */
    CAP_KEYMAP              = 4,  /* 按键映射：4层按键映射、Fn键 */
    CAP_MACRO               = 5,  /* 宏引擎：8个宏、32个动作 */
    CAP_CONFIG_STORAGE      = 6,  /* 配置存储：1510字节、CRC32、双备份 */
    CAP_PERF_MONITOR        = 7,  /* 性能监控：CPU使用率、任务统计 */
    CAP_FAULT_LOG           = 8,  /* 错误日志：Flash持久化错误日志 */
    CAP_KEY_STATS           = 9,  /* 按键统计：64键使用次数 */
    CAP_DFU_UPGRADE         = 10, /* 自定义 in-app DFU：CMD_ENTER_DFU + HID report 0x30~0x36
                                  * 专用 bootloader，随 FEATURE_DFU_BOOTLOADER 置位；
                                  * 不含 RP2350 原生 BOOTSEL UF2（CMD_ENTER_BOOTSEL，始终可用） */
    CAP_NKRO                = 11, /* NKRO：NKRO位图键盘 */
    CAP_CONSUMER            = 12, /* 多媒体键：音量、播放控制 */
    CAP_GAMEPAD             = 13, /* 游戏手柄：标准HID Gamepad */
    CAP_DPI_SLOTS           = 14, /* DPI档位：4档DPI切换、LOD */
    CAP_JOYSTICK_CALIB      = 15, /* 摇杆校准：中心点校准、实时调节 */

    /* ==== 诊断能力段 bit16-23（协议 v0.2 新增） ==== */
    CAP_DIAG_RAW_STATE      = 16, /* 实时 raw 诊断报告（KeyState / JoystickState），FEATURE_RAW_DIAG */
    CAP_DIAG_DEBUG_UART     = 17, /* 调试串口 / 详细日志，FEATURE_DEBUG_UART */
    /* bit18-23 预留 */

    /* ==== 制造能力段 bit24-31（协议 v0.2 新增） ==== */
    CAP_MFG_FACTORY_TEST    = 24, /* 工厂自检（FEATURE_FACTORY_TEST 且 manufacturing flag 未出厂锁定） */
    CAP_MFG_PROVISION       = 25, /* 制造数据烧录 / 出厂锁定（序列号、校准、ship lock） */
    /* bit26-31 预留 */

    CAP_COUNT               = 26  /* 当前支持的能力总数（含预留空洞 18-23） */
} capability_id_t;

/* Device Info 报告 device_flags 位定义（协议 v0.2 新增）
 * bit0 = 已出厂锁定（manufacturing shipped）；bit1 = 本会话售后已解锁；bit2-7 预留 */
#define DEVICE_FLAG_MFG_SHIPPED_LOCKED   (1u << 0)
#define DEVICE_FLAG_MFG_SESSION_UNLOCKED (1u << 1)

/* 设备信息报告（Report ID = REPORT_ID_DEVICE_INFO）
 * 上位机通过 GET_REPORT 读取此信息，用于识别设备、版本协商和能力发现。
 */
typedef struct __attribute__((packed))
{
    uint8_t report_id;           /* 报告 ID = REPORT_ID_DEVICE_INFO */
    uint16_t firmware_version;    /* 固件版本（bcdDevice，如 0x0400 = 4.0.0） */
    uint16_t config_version;      /* 配置结构版本（如 0x0007 = v7） */
    uint16_t protocol_version;    /* 配置 HID 协议版本（高字节=主版本，低字节=次版本） */
    uint16_t config_size;         /* 配置结构总大小（字节） */
    uint8_t  layer_count;         /* 支持的按键层数 */
    uint8_t  macro_count;         /* 支持的宏数量 */
    uint8_t  max_dpi;             /* 最大支持 DPI */
    uint8_t  capability_count;    /* 支持的能力数量 */
    uint64_t capability_bitmap;   /* 能力位图（64位，每位代表一个能力是否存在） */
    uint8_t  device_flags;        /* v0.2 新增：制造/会话状态位（DEVICE_FLAG_*），老上位机按短报告解析忽略 */
    uint8_t  reserved[10];        /* 对齐预留（报告数据 ≤ 32B，含 report_id 共 32B） */
} config_hid_device_info_t;

/* ==================== 对外接口 ==================== */

/* Initialize config HID module */
void config_hid_init(void);

/* Periodic task to be called from main loop */
void hid_config_task(void);

/* 发送 ACK 响应
 * cmd: 对应的命令码
 * success: true=成功(ACK), false=失败(NACK)
 * error_code: 错误码（成功时为 CONFIG_HID_OK）
 * param: 附加参数
 */
void config_hid_send_ack(uint8_t cmd, bool success, uint8_t error_code, uint16_t param);

/* 重置配置块接收状态（USB 断开时调用，防止残留块与新会话组合导致配置损坏） */
void config_hid_reset_blocks_received(void);

/* TinyUSB callbacks */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_CONFIG_HID_H */
