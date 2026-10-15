/*
 * include/protocol/config_lock.h
 * 配置锁定机制接口
 * 从 config_hid.c 提取，包含配置锁定/解锁、自动超时锁定、命令白名单检查
 */
#ifndef PROTOCOL_CONFIG_LOCK_H
#define PROTOCOL_CONFIG_LOCK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量 ==================== */

/* 解锁所需尝试次数（3次） */
#define CONFIG_LOCK_UNLOCK_REQUIRED_ATTEMPTS  3u

/* 解锁时间窗口（5秒内连续发送才算） */
#define CONFIG_LOCK_UNLOCK_WINDOW_MS          5000u

/* 解锁后自动锁定超时（30秒无操作） */
#define CONFIG_LOCK_UNLOCK_TIMEOUT_MS         30000u

/* ==================== 初始化 ==================== */

/* 初始化配置锁定机制（默认锁定状态） */
void config_lock_init(void);

/* ==================== 锁定状态查询 ==================== */

/* 检查配置是否已锁定
 * @return true=已锁定，false=已解锁
 */
bool config_lock_is_locked(void);

/* ==================== 解锁操作 ==================== */

/* 尝试解锁配置（需连续3次在5秒内发送）
 * @return 当前解锁进度（0-2=还需继续，3=已解锁）
 */
uint8_t config_lock_attempt_unlock(void);

/* ==================== 写入通知 ==================== */

/* 通知配置写入操作，刷新自动锁定计时器 */
void config_lock_notify_write(void);

/* ==================== 自动锁定检查 ==================== */

/* 检查是否需要自动重新锁定（解锁后30秒无操作）
 * @return true=已自动重新锁定，false=仍保持解锁或原本就锁定
 */
bool config_lock_check_auto_relock(void);

/* ==================== 命令白名单检查 ==================== */

/* 检查命令在锁定状态下是否被允许
 * 白名单包括：DFU/Reboot（有独立确认机制）、实时设置、只读查询、宏播放等
 * @param cmd 命令字节
 * @return true=允许执行，false=拒绝执行
 */
bool config_lock_is_command_allowed(uint8_t cmd);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_CONFIG_LOCK_H */
