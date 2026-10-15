/**
 * @file command_gate.h
 * @brief HID 命令 / 报告的固件侧授权闸门（功能角色分离，协议 v0.2）
 *
 * 三级命令模型（与 config_lock 的“配置写锁”正交，两层都保留）：
 *   - PUBLIC : 任何连接者可用（消费者功能、版本/能力查询）；
 *   - DIAG   : 诊断命令，需对应诊断能力位；清除 Fault 等售后操作还需会话解锁；
 *   - MFG    : 制造命令，需制造能力且未出厂锁定（阶段3接入实际命令）。
 *
 * 只在上位机隐藏按钮不够：任何人都能直接发 raw HID，固件必须强校验。
 * 详见 docs/FEATURE_ROLE_SPLIT_PLAN.md §5/§8。
 *
 * 本模块为纯逻辑（仅依赖 capability 位图），便于 host 单元测试。
 */
#ifndef COMMAND_GATE_H
#define COMMAND_GATE_H

#include <stdint.h>
#include "protocol/config_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 命令授权级别 */
typedef enum {
    CMD_LEVEL_PUBLIC = 0, /* 消费者公开 */
    CMD_LEVEL_DIAG,       /* 诊断（需能力位，部分需售后会话解锁） */
    CMD_LEVEL_MFG         /* 制造（需制造能力且未出厂锁定） */
} cmd_level_t;

/**
 * @brief 校验 SET_REPORT 控制命令是否允许执行
 * @param cmd 命令码（config_hid_command_t）
 * @return CONFIG_HID_OK 放行；否则为 config_hid_error_t：
 *         NOT_SUPPORTED（能力位缺失/未编入）、DIAG_LOCKED、MFG_LOCKED。
 *         未知命令返回 OK，交由分发 switch 的 default 保持原有“静默”行为。
 */
uint8_t command_gate_check_command(uint8_t cmd);

/**
 * @brief 查询某 GET_REPORT report_id 所需能力位
 * @param report_id HID 报告 ID
 * @return 所需能力位（单 bit）；0 表示无限制（消费者公开或非配置接口报告）
 *
 * 缺能力时上位机本不会读取；固件 get_report 回调据此返回空报告，防止 raw 绕过。
 */
uint64_t command_gate_report_required_cap(uint8_t report_id);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_GATE_H */
