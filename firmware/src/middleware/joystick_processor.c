/*
 * src/middleware/joystick_processor.c
 * 摇杆信号处理中间件实现
 * 功能：中心点校准、死区处理、灵敏度曲线、轴反转、漂移抑制
 */

#include "middleware/joystick_processor.h"
#include "middleware/fault.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

/* ==================== 静态变量 ==================== */

static joystick_config_t s_config = {
    .deadzone = 100,
    .sensitivity = 1000,
    .invert_x = 0,
    .invert_y = 1,  /* 默认 Y 轴反转（物理方向与 HID 相反） */
    .curve_type = JOYSTICK_CURVE_LINEAR
};

static joystick_calibration_t s_calibration = {
    .center_x = JOYSTICK_DEFAULT_CENTER,
    .center_y = JOYSTICK_DEFAULT_CENTER,
    .calibrated = false
};

/* 校准状态 */
static joystick_calib_state_t s_calib_state = JOYSTICK_CALIB_IDLE;
static uint16_t s_calib_sample_count = 0;
static uint16_t s_calib_target_count = 0;
static uint32_t s_calib_sum_x = 0;
static uint32_t s_calib_sum_y = 0;

/* 自动漂移补偿 */
static bool s_drift_comp_enabled = true;       /* 默认启用 */
static uint32_t s_drift_counter = 0;           /* 微小输出持续计数 */
static uint32_t s_drift_threshold = 100;       /* 持续 100 次（1秒@10ms）后触发微调 */
static int32_t s_drift_max_offset = 50;        /* 最大微调偏移（ADC 值） */
static int32_t s_drift_offset_x = 0;            /* 当前 X 轴微调偏移 */
static int32_t s_drift_offset_y = 0;            /* 当前 Y 轴微调偏移 */

/* ==================== 内部函数 ==================== */

/* 限制值在范围内 */
static int32_t clamp_int32(int32_t value, int32_t min, int32_t max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/* ==================== 接口实现 ==================== */

void joystick_processor_init(void)
{
    s_config.deadzone = 100;
    s_config.sensitivity = 1000;
    s_config.invert_x = 0;
    s_config.invert_y = 1;
    s_config.curve_type = JOYSTICK_CURVE_LINEAR;

    s_calibration.center_x = JOYSTICK_DEFAULT_CENTER;
    s_calibration.center_y = JOYSTICK_DEFAULT_CENTER;
    s_calibration.calibrated = false;

    s_calib_state = JOYSTICK_CALIB_IDLE;
    s_calib_sample_count = 0;
    s_calib_target_count = 0;
    s_calib_sum_x = 0;
    s_calib_sum_y = 0;

    fault_record(FAULT_LEVEL_INFO, "joystick_proc", "init complete");
}

void joystick_processor_set_config(const joystick_config_t *config)
{
    if (config == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "joystick_proc", "set_config null");
        return;
    }
    s_config = *config;

    /* 合法性检查 */
    if (s_config.sensitivity == 0) s_config.sensitivity = 1000;
    if (s_config.curve_type >= JOYSTICK_CURVE_COUNT)
        s_config.curve_type = JOYSTICK_CURVE_LINEAR;
}

joystick_config_t joystick_processor_get_config(void)
{
    return s_config;
}

void joystick_processor_process(uint16_t raw_x, uint16_t raw_y, joystick_result_t *result)
{
    if (result == NULL) return;

    /* 1. 减去中心点（校准中心点 + 漂移微调偏移）
     * 漂移微调是临时的，不保存到校准数据
     */
    int32_t effective_center_x = (int32_t)s_calibration.center_x + s_drift_offset_x;
    int32_t effective_center_y = (int32_t)s_calibration.center_y + s_drift_offset_y;
    int32_t x = (int32_t)raw_x - effective_center_x;
    int32_t y = (int32_t)raw_y - effective_center_y;

    /* 自动漂移补偿检测：
     * 如果摇杆长时间处于微小输出状态（在死区附近但不在死区内），
     * 自动微调中心点，使输出变为 0
     */
    if (s_drift_comp_enabled && s_calib_state == JOYSTICK_CALIB_IDLE)
    {
        int32_t drift_threshold = (int32_t)s_config.deadzone + (s_config.deadzone / 2);
        if (drift_threshold < 10) drift_threshold = 10;  /* 最小阈值 */

        if (x > -drift_threshold && x < drift_threshold &&
            y > -drift_threshold && y < drift_threshold)
        {
            s_drift_counter++;
            if (s_drift_counter >= s_drift_threshold)
            {
                /* 微调中心点：每次 1 个 ADC 值，方向朝向当前输出 */
                if (x > 0 && s_drift_offset_x < s_drift_max_offset)
                    s_drift_offset_x++;
                else if (x < 0 && s_drift_offset_x > -s_drift_max_offset)
                    s_drift_offset_x--;

                if (y > 0 && s_drift_offset_y < s_drift_max_offset)
                    s_drift_offset_y++;
                else if (y < 0 && s_drift_offset_y > -s_drift_max_offset)
                    s_drift_offset_y--;

                s_drift_counter = 0;  /* 重置计数器，继续检测 */
            }
        }
        else
        {
            s_drift_counter = 0;  /* 输出较大，重置计数器 */
        }
    }

    /* 2. 圆形死区处理（使用欧几里得距离判断，避免方形死区的对角问题）
     * 如果 x² + y² < deadzone²，则在死区内，输出 0
     * 否则，按比例缩放：scale = (distance - deadzone) / distance
     */
    int32_t deadzone = (int32_t)s_config.deadzone;
    bool in_deadzone = false;

    if (deadzone > 0)
    {
        int64_t dist_sq = (int64_t)x * x + (int64_t)y * y;
        int64_t deadzone_sq = (int64_t)deadzone * deadzone;

        if (dist_sq < deadzone_sq)
        {
            /* 在死区内，输出 0 */
            x = 0;
            y = 0;
            in_deadzone = true;
        }
        else
        {
            /* 在死区外，按比例缩放到边缘（消除死区造成的台阶）
             * scale = (distance - deadzone) / distance
             * 使用整数运算避免浮点：x' = x * (distance - deadzone) / distance
             */
            int32_t distance = (int32_t)sqrtf((float)dist_sq);
            if (distance > deadzone)
            {
                int32_t scale_num = distance - deadzone;
                x = (x * scale_num) / distance;
                y = (y * scale_num) / distance;
            }
        }
    }

    /* 3. 计算有效范围（中心点到边缘的距离）
     * 注意：圆形死区已经在步骤2中做了缩放，这里不再减去死区
     * 使用有效中心点（校准中心点 + 漂移微调偏移）
     */
    int32_t range_pos = (int32_t)JOYSTICK_ADC_MAX - effective_center_x;
    int32_t range_neg = effective_center_x;
    int32_t range = (range_pos < range_neg) ? range_pos : range_neg;
    if (range < 100) range = 100;

    /* 4. 缩放至 -127~127 */
    if (x > 0)
        x = (x * JOYSTICK_OUTPUT_MAX) / range_pos;
    else
        x = (x * JOYSTICK_OUTPUT_MAX) / range_neg;

    if (y > 0)
        y = (y * JOYSTICK_OUTPUT_MAX) / range_pos;
    else
        y = (y * JOYSTICK_OUTPUT_MAX) / range_neg;

    /* 5. 应用灵敏度（定点数，1.0=1000） */
    int32_t sens = (int32_t)s_config.sensitivity;
    x = (x * sens) / 1000;
    y = (y * sens) / 1000;

    /* 6. 应用灵敏度曲线 */
    x = joystick_processor_apply_curve((int16_t)x, s_config.curve_type);
    y = joystick_processor_apply_curve((int16_t)y, s_config.curve_type);

    /* 7. 限制范围 */
    x = clamp_int32(x, JOYSTICK_OUTPUT_MIN, JOYSTICK_OUTPUT_MAX);
    y = clamp_int32(y, JOYSTICK_OUTPUT_MIN, JOYSTICK_OUTPUT_MAX);

    /* 8. 轴反转 */
    if (s_config.invert_x) x = -x;
    if (s_config.invert_y) y = -y;

    /* 输出结果 */
    result->x = (int16_t)x;
    result->y = (int16_t)y;
    result->in_deadzone = in_deadzone;
}

bool joystick_processor_start_calibration(uint16_t sample_count)
{
    if (s_calib_state != JOYSTICK_CALIB_IDLE)
    {
        return false;
    }
    if (sample_count == 0) sample_count = 50;

    s_calib_state = JOYSTICK_CALIB_CENTERING;
    s_calib_sample_count = 0;
    s_calib_target_count = sample_count;
    s_calib_sum_x = 0;
    s_calib_sum_y = 0;

    fault_record(FAULT_LEVEL_INFO, "joystick_proc", "calibration started");
    return true;
}

uint16_t joystick_processor_calibration_sample(uint16_t raw_x, uint16_t raw_y)
{
    if (s_calib_state != JOYSTICK_CALIB_CENTERING)
    {
        return 0;
    }

    s_calib_sum_x += raw_x;
    s_calib_sum_y += raw_y;
    s_calib_sample_count++;

    if (s_calib_sample_count >= s_calib_target_count)
    {
        /* 计算平均值作为中心点 */
        s_calibration.center_x = (uint16_t)(s_calib_sum_x / s_calib_sample_count);
        s_calibration.center_y = (uint16_t)(s_calib_sum_y / s_calib_sample_count);
        s_calibration.calibrated = true;
        s_calib_state = JOYSTICK_CALIB_DONE;

        fault_record(FAULT_LEVEL_INFO, "joystick_proc", "calibration done");
    }

    return s_calib_sample_count;
}

joystick_calib_state_t joystick_processor_get_calibration_state(void)
{
    return s_calib_state;
}

joystick_calibration_t joystick_processor_get_calibration(void)
{
    return s_calibration;
}

void joystick_processor_set_calibration(const joystick_calibration_t *calib)
{
    if (calib == NULL) return;
    s_calibration = *calib;
}

void joystick_processor_reset_calibration(void)
{
    s_calibration.center_x = JOYSTICK_DEFAULT_CENTER;
    s_calibration.center_y = JOYSTICK_DEFAULT_CENTER;
    s_calibration.calibrated = false;
    s_calib_state = JOYSTICK_CALIB_IDLE;
    fault_record(FAULT_LEVEL_INFO, "joystick_proc", "calibration reset to default");
}

int16_t joystick_processor_apply_curve(int16_t value, uint8_t curve_type)
{
    if (value == 0) return 0;

    int32_t result = value;
    bool negative = (value < 0);
    int32_t abs_value = negative ? -value : value;

    /* 归一化到 0.0~1.0 */
    float normalized = (float)abs_value / 127.0f;
    float curved;

    switch (curve_type)
    {
        case JOYSTICK_CURVE_EXPONENTIAL:
            /* 指数曲线：小值更灵敏，大值更平缓
             * f(x) = 1 - (1-x)^2
             */
            curved = 1.0f - (1.0f - normalized) * (1.0f - normalized);
            break;

        case JOYSTICK_CURVE_LOGARITHMIC:
            /* 对数曲线：小值更平缓，大值更灵敏
             * f(x) = sqrt(x)
             */
            curved = sqrtf(normalized);
            break;

        case JOYSTICK_CURVE_LINEAR:
        default:
            curved = normalized;
            break;
    }

    /* 反归一化到 0~127 */
    result = (int32_t)(curved * 127.0f + 0.5f);
    result = clamp_int32(result, 0, 127);

    return negative ? (int16_t)(-result) : (int16_t)result;
}

/* ==================== 自动漂移补偿 ==================== */

void joystick_processor_set_drift_compensation(bool enabled)
{
    s_drift_comp_enabled = enabled;
    if (!enabled)
    {
        /* 禁用时重置漂移状态 */
        s_drift_counter = 0;
        s_drift_offset_x = 0;
        s_drift_offset_y = 0;
    }
}

bool joystick_processor_get_drift_compensation(void)
{
    return s_drift_comp_enabled;
}

void joystick_processor_reset_drift_compensation(void)
{
    s_drift_counter = 0;
    s_drift_offset_x = 0;
    s_drift_offset_y = 0;
}
