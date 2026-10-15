/*
 * include/protocol/command_confirm.h
 * 命令确认机制接口
 * 从 config_hid.c 提取，包含 DFU/Reboot 命令的多次确认机制
 * 防止恶意程序通过单次命令触发危险操作（如重启、进入 DFU 模式）
 */
#ifndef PROTOCOL_COMMAND_CONFIRM_H
#define PROTOCOL_COMMAND_CONFIRM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量 ==================== */

/* 重启命令所需确认次数（3次） */
#define COMMAND_CONFIRM_REBOOT_REQUIRED  3u

/* 重启命令确认时间窗口（5秒内连续发送才算） */
#define COMMAND_CONFIRM_REBOOT_WINDOW_MS  5000u

/* DFU 命令所需确认次数（3次） */
#define COMMAND_CONFIRM_DFU_REQUIRED  3u

/* DFU 命令确认时间窗口（5秒内连续发送才算） */
#define COMMAND_CONFIRM_DFU_WINDOW_MS  5000u

/* BOOTSEL 命令所需确认次数（3次） */
#define COMMAND_CONFIRM_BOOTSEL_REQUIRED  3u

/* BOOTSEL 命令确认时间窗口（5秒内连续发送才算） */
#define COMMAND_CONFIRM_BOOTSEL_WINDOW_MS  5000u

/* ==================== 初始化 ==================== */

/* 初始化命令确认机制（清零确认计数和时间戳） */
void command_confirm_init(void);

/* ==================== 重启命令确认 ==================== */

/* 尝试确认重启命令（需连续3次在5秒内发送）
 * @return 当前确认进度（0-2=还需继续发送，3=已确认可执行）
 */
uint8_t command_confirm_attempt_reboot(void);

/* 检查重启命令是否已确认
 * @return true=已确认，false=未确认
 */
bool command_confirm_is_reboot_confirmed(void);

/* 重置重启命令确认状态 */
void command_confirm_reset_reboot(void);

/* ==================== DFU 命令确认 ==================== */

/* 尝试确认 DFU 命令（需连续3次在5秒内发送）
 * @return 当前确认进度（0-2=还需继续发送，3=已确认可执行）
 */
uint8_t command_confirm_attempt_dfu(void);

/* 检查 DFU 命令是否已确认
 * @return true=已确认，false=未确认
 */
bool command_confirm_is_dfu_confirmed(void);

/* 重置 DFU 命令确认状态 */
void command_confirm_reset_dfu(void);

/* ==================== BOOTSEL 命令确认 ==================== */

/* 尝试确认 BOOTSEL 命令（需连续3次在5秒内发送）
 * @return 当前确认进度（0-2=还需继续发送，3=已确认可执行）
 */
uint8_t command_confirm_attempt_bootsel(void);

/* 检查 BOOTSEL 命令是否已确认
 * @return true=已确认，false=未确认
 */
bool command_confirm_is_bootsel_confirmed(void);

/* 重置 BOOTSEL 命令确认状态 */
void command_confirm_reset_bootsel(void);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_COMMAND_CONFIRM_H */
