/*
 * include/app/keyboard_processor.h
 * 键盘处理模块接口
 * 从 main.c 提取，包含 NKRO 模式控制和报告生成
 */
#ifndef APP_KEYBOARD_PROCESSOR_H
#define APP_KEYBOARD_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== NKRO 报告常量 ==================== */

/* NKRO 报告大小（含 Report ID） */
#define KEYBOARD_NKRO_REPORT_SIZE       35u

/* NKRO 报告数据大小（不含 Report ID，发送时使用此大小） */
#define KEYBOARD_NKRO_REPORT_DATA_SIZE  (KEYBOARD_NKRO_REPORT_SIZE - 1u)

/* ==================== 初始化 ==================== */

/* 初始化键盘处理模块（清零 NKRO 报告缓冲区） */
void keyboard_processor_init(void);

/* ==================== NKRO 模式控制 ==================== */

/* 设置 NKRO 模式开关
 * @param enabled true=启用 NKRO，false=使用标准 6KRO
 */
void keyboard_set_nkro_enabled(bool enabled);

/* 获取 NKRO 模式状态
 * @return true=NKRO 已启用，false=标准 6KRO
 */
bool keyboard_get_nkro_enabled(void);

/* ==================== NKRO 报告生成与发送 ==================== */

/* 生成 NKRO 位图报告并通过 USB 发送
 * @param stable_keys 稳定按键状态位图（bit=0 表示按下）
 * @param modifier 修饰键位掩码
 *
 * 内部流程：
 * 1. 清零报告缓冲区
 * 2. 设置 Report ID 和修饰键
 * 3. 遍历64个物理键，查找 HID 键码，设置位图
 * 4. 通过 tud_hid_report 发送
 */
void keyboard_send_nkro_report(uint64_t stable_keys, uint8_t modifier);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEYBOARD_PROCESSOR_H */
