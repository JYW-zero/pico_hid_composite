/*
 * include/middleware/joystick_processor.h
 * 摇杆信号处理中间件
 * 功能：中心点校准、死区处理、灵敏度曲线、轴反转、漂移抑制
 * 与硬件解耦，输入原始 ADC 值，输出处理后的 -127~127 值
 */

#ifndef MIDDLEWARE_JOYSTICK_PROCESSOR_H
#define MIDDLEWARE_JOYSTICK_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/* ADC 最大值（12位 ADC） */
#define JOYSTICK_ADC_MAX        4095

/* 默认中心点 */
#define JOYSTICK_DEFAULT_CENTER  2048

/* 输出范围（HID 游戏手柄标准） */
#define JOYSTICK_OUTPUT_MIN     -127
#define JOYSTICK_OUTPUT_MAX     127

/* 灵敏度曲线类型 */
typedef enum
{
    JOYSTICK_CURVE_LINEAR = 0,    /* 线性（默认） */
    JOYSTICK_CURVE_EXPONENTIAL,   /* 指数曲线（小值更灵敏，大值更平缓） */
    JOYSTICK_CURVE_LOGARITHMIC,   /* 对数曲线（小值更平缓，大值更灵敏） */
    JOYSTICK_CURVE_COUNT
} joystick_curve_type_t;

/* 校准状态 */
typedef enum
{
    JOYSTICK_CALIB_IDLE = 0,      /* 空闲 */
    JOYSTICK_CALIB_CENTERING,     /* 正在校准中心点 */
    JOYSTICK_CALIB_DONE           /* 校准完成 */
} joystick_calib_state_t;

/* ==================== 配置结构体 ==================== */

/* 摇杆处理配置
 * 从 device_config_t 中提取相关字段，避免直接依赖完整配置结构
 */
typedef struct
{
    uint16_t deadzone;             /* 死区（ADC 原始值，0-2048） */
    uint16_t sensitivity;          /* 灵敏度（定点数，1.0=1000） */
    uint8_t  invert_x;             /* X 轴反转：0=正常，1=反转 */
    uint8_t  invert_y;             /* Y 轴反转：0=正常，1=反转 */
    uint8_t  curve_type;           /* 曲线类型（joystick_curve_type_t） */
} joystick_config_t;

/* 校准数据 */
typedef struct
{
    uint16_t center_x;             /* X 轴中心点（ADC 值，0-4095） */
    uint16_t center_y;             /* Y 轴中心点（ADC 值，0-4095） */
    bool     calibrated;           /* 是否已校准 */
} joystick_calibration_t;

/* 处理结果 */
typedef struct
{
    int16_t x;                     /* 处理后的 X 值（-127~127） */
    int16_t y;                     /* 处理后的 Y 值（-127~127） */
    bool    in_deadzone;           /* 是否在死区内（漂移抑制） */
} joystick_result_t;

/* ==================== 接口函数 ==================== */

/**
 * @brief 初始化摇杆处理器
 * 加载默认中心点和配置
 */
void joystick_processor_init(void);

/**
 * @brief 设置处理配置
 * @param config 配置参数（从 device_config_t 提取）
 */
void joystick_processor_set_config(const joystick_config_t *config);

/**
 * @brief 获取当前处理配置
 * @return 当前配置
 */
joystick_config_t joystick_processor_get_config(void);

/**
 * @brief 处理原始 ADC 值
 * @param raw_x 原始 X 轴 ADC 值（0-4095）
 * @param raw_y 原始 Y 轴 ADC 值（0-4095）
 * @param result 输出处理结果
 */
void joystick_processor_process(uint16_t raw_x, uint16_t raw_y, joystick_result_t *result);

/**
 * @brief 开始中心点自动校准
 * 校准过程中应保持摇杆在中心位置，持续采样取平均值
 * @param sample_count 采样次数（建议 50-100 次）
 * @return true=开始成功，false=校准进行中
 */
bool joystick_processor_start_calibration(uint16_t sample_count);

/**
 * @brief 输入校准采样值（在校准过程中调用）
 * @param raw_x X 轴 ADC 值
 * @param raw_y Y 轴 ADC 值
 * @return 当前已采样次数，0 表示校准未开始或已完成
 */
uint16_t joystick_processor_calibration_sample(uint16_t raw_x, uint16_t raw_y);

/**
 * @brief 获取校准状态
 * @return 校准状态
 */
joystick_calib_state_t joystick_processor_get_calibration_state(void);

/**
 * @brief 获取校准数据
 * @return 校准数据（中心点等）
 */
joystick_calibration_t joystick_processor_get_calibration(void);

/**
 * @brief 设置校准数据（从 Flash 加载时调用）
 * @param calib 校准数据
 */
void joystick_processor_set_calibration(const joystick_calibration_t *calib);

/**
 * @brief 重置校准数据为默认值（中心点 2048）
 */
void joystick_processor_reset_calibration(void);

/**
 * @brief 应用灵敏度曲线
 * @param value 输入值（-127~127）
 * @param curve_type 曲线类型
 * @return 曲线处理后的值（-127~127）
 */
int16_t joystick_processor_apply_curve(int16_t value, uint8_t curve_type);

/**
 * @brief 设置自动漂移补偿开关
 * @param enabled true=启用，false=禁用
 */
void joystick_processor_set_drift_compensation(bool enabled);

/**
 * @brief 获取自动漂移补偿状态
 * @return true=启用，false=禁用
 */
bool joystick_processor_get_drift_compensation(void);

/**
 * @brief 重置自动漂移补偿状态（清除漂移计数器）
 */
void joystick_processor_reset_drift_compensation(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_JOYSTICK_PROCESSOR_H */
