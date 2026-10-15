/**
 * @file capability.h
 * @brief 运行时能力位图与设备标志（功能角色分离，协议 v0.2）
 *
 * 能力来源不再是“协议版本推断”，而是固件按编译期 FEATURE_* 开关 + 制造状态
 * 直接拼出 64 位 capability_bitmap，随 Device Info 上报；上位机据此显隐功能，
 * 固件命令/报告闸门也以此为判据。详见 docs/FEATURE_ROLE_SPLIT_PLAN.md §6/§8。
 *
 * 本模块零硬件依赖，只依赖 features.h 与能力枚举，便于 host 单元测试。
 */
#ifndef CAPABILITY_H
#define CAPABILITY_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol/config_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取运行时能力位图
 * @return 64 位能力位图，bit n 对应 capability_id_t 的 CAP_xxx
 *
 * - 消费者基础能力（bit0-6、10-15）始终编入；
 * - 诊断能力（PERF_MONITOR/FAULT_LOG/KEY_STATS/RAW_STATE/DEBUG_UART）随 FEATURE_*；
 * - 制造能力（MFG_FACTORY_TEST/MFG_PROVISION）需 FEATURE_FACTORY_TEST 且未出厂锁定。
 */
uint64_t capability_bitmap_runtime(void);

/**
 * @brief 获取 Device Info 的 device_flags（DEVICE_FLAG_*）
 * @return 标志字节；阶段1工程态恒为 0，阶段2/3 接入 Provisioning 分区后返回真实状态
 */
uint8_t capability_device_flags(void);

/**
 * @brief 制造/工厂能力当前是否可用（未出厂锁定，或本会话已售后解锁）
 * @return true 可用；false 已出厂锁定且未解锁
 *
 * 阶段1为弱桩，工程态恒 true；阶段2/3 改为读取 Provisioning 分区 mfg_state。
 */
bool capability_mfg_factory_available(void);

#ifdef __cplusplus
}
#endif

#endif /* CAPABILITY_H */
