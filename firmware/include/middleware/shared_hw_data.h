/*
 * include/middleware/shared_hw_data.h
 * 双核共享硬件数据模块
 * 用官方自旋锁保护，线程安全
 * Core1（生产者）写入扫描结果
 * Core0（消费者）读取并处理
 */
#ifndef MIDDLEWARE_SHARED_HW_DATA_H
#define MIDDLEWARE_SHARED_HW_DATA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 初始化 ==================== */

/* 初始化共享数据和自旋锁
 * 必须在启动 Core1 之前调用
 */
void shared_hw_data_init(void);

/* ==================== Core1 写入接口（生产者） ==================== */

/* 添加鼠标位移（累积） */
void shared_hw_add_motion(int32_t dx, int32_t dy);

/* 添加滚轮步数（累积） */
void shared_hw_add_wheel(int32_t delta);

/* 更新键盘稳定按键状态（最新值） */
void shared_hw_set_keys(uint64_t keys);

/* 设置鼠标按键（OR合并：只设置指定位，不清除其他位）
 * 多个来源（编码器中键、OPTICAL_SENSOR侧键等）的按钮状态通过OR合并
 */
void shared_hw_set_mouse_buttons(uint8_t buttons);

/* 清除鼠标按键（清除指定位） */
void shared_hw_clear_mouse_buttons(uint8_t buttons);

/* 更新摇杆数据（最新值） */
void shared_hw_set_joystick(int16_t x, int16_t y, bool btn);

/* ==================== 心跳监控 ==================== */

/* Core1 递增心跳计数器（每次主循环调用一次） */
void shared_hw_increment_heartbeat(void);

/* Core0 读取心跳计数器值 */
uint32_t shared_hw_get_heartbeat(void);

/* ==================== Core0 读取接口（消费者） ==================== */

/* 读取并清零鼠标位移累积值 */
void shared_hw_take_motion(int32_t *out_dx, int32_t *out_dy);

/* 读取并清零滚轮步数累积值 */
int32_t shared_hw_take_wheel(void);

/* 读取键盘稳定按键状态（最新值） */
uint64_t shared_hw_get_keys(void);

/* 读取鼠标按键状态（最新值） */
uint8_t shared_hw_get_mouse_buttons(void);

/* 读取摇杆数据（最新值） */
void shared_hw_get_joystick(int16_t *out_x, int16_t *out_y, bool *out_btn);

/* ==================== 状态统计 ==================== */
/* Core1 运行状态统计，Core1 更新，Core0 读取 */

uint32_t shared_hw_get_keypad_scan_count(void);
uint32_t shared_hw_get_optical_sensor_read_count(void);
uint32_t shared_hw_get_encoder_scan_count(void);
uint32_t shared_hw_get_joystick_read_count(void);
uint32_t shared_hw_get_error_count(void);
void shared_hw_reset_stats(void);

/* ==================== Core1 专用递增函数 ==================== */
void shared_hw_inc_keypad_scan(void);
void shared_hw_inc_optical_sensor_read(void);
void shared_hw_inc_encoder_scan(void);
void shared_hw_inc_joystick_read(void);
void shared_hw_inc_error(void);

/* ==================== 去抖阈值配置（Core0 设置，Core1 读取） ==================== */

/* 设置键盘去抖阈值（采样次数）
 * Core0 调用，Core1 在下次扫描时应用
 * 实际去抖时间 = 阈值 × 扫描周期(5ms)
 * 例如：阈值=2 → 10ms，阈值=4 → 20ms
 */
void shared_hw_set_keypad_debounce_threshold(uint8_t threshold);

/* 获取键盘去抖阈值（采样次数）
 * Core1 调用，检查是否需要更新阈值
 */
uint8_t shared_hw_get_keypad_debounce_threshold(void);

/* 检查去抖阈值是否已更新（Core1 调用，读取后自动清除标志）
 * 返回 true 表示阈值已更新，需要重新设置去抖实例
 */
bool shared_hw_check_and_clear_debounce_updated(void);

/* ==================== 摇杆校准（Core0 触发，Core1 执行） ==================== */

/* 触发摇杆中心点校准（Core0 调用）
 * sample_count: 采样次数（建议 50-100）
 * 返回 true=触发成功，false=校准进行中
 */
bool shared_hw_trigger_joystick_calibration(uint16_t sample_count);

/* 检查摇杆校准是否触发（Core1 调用，读取后自动清除标志）
 * 返回 true=需要开始校准，sample_count 输出采样次数
 */
bool shared_hw_check_and_clear_joystick_calib_trigger(uint16_t *sample_count);

/* 获取摇杆校准状态（Core0 调用，用于查询校准进度）
 * 返回: 0=空闲, 1=校准中, 2=完成
 */
uint8_t shared_hw_get_joystick_calibration_state(void);

/* 设置摇杆校准状态（Core1 调用）
 * state: 0=空闲, 1=校准中, 2=完成
 */
void shared_hw_set_joystick_calibration_state(uint8_t state);

/* 获取摇杆校准数据（Core0 调用，用于保存到配置）
 * out_center_x, out_center_y: 输出中心点（ADC 值）
 * out_calibrated: 输出是否已校准
 */
void shared_hw_get_joystick_calibration_data(uint16_t *out_center_x,
                                                uint16_t *out_center_y,
                                                uint8_t *out_calibrated);

/* 设置摇杆校准数据（Core0 调用，启动时从配置加载）
 * center_x, center_y: 中心点（ADC 值）
 * calibrated: 是否已校准
 */
void shared_hw_set_joystick_calibration_data(uint16_t center_x,
                                                uint16_t center_y,
                                                uint8_t calibrated);

/* 检查摇杆校准数据是否已更新（Core0 调用，读取后自动清除标志）
 * 返回 true 表示校准数据已更新，需要保存到配置
 */
bool shared_hw_check_and_clear_joystick_calib_updated(void);

/* 通知摇杆校准数据已更新（Core1 调用，校准完成后设置标志）
 */
void shared_hw_notify_joystick_calib_updated(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_SHARED_HW_DATA_H */





