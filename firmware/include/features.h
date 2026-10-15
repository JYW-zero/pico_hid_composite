/*
 * include/features.h
 * 功能裁剪开关（Feature Flags）统一入口
 *
 * 每个开关取值 0 / 1，代码中以 #if FEATURE_xxx 包裹对应模块的入口
 * （初始化、周期任务、HID 报告/命令分发等）。
 *
 * 约定：
 *   - 本文件中的默认值全部为 1（全开），保证不带任何参数构建时，
 *     固件行为与历史版本完全一致。
 *   - CMake 的 option(FEATURE_xxx ON/OFF) 会通过 -DFEATURE_xxx=1/0 注入，
 *     由于这里用 #ifndef 保护，CMake 传入的值优先，缺省时回退到默认 1。
 *   - 固件单元测试独立于 CMake 构建，未注入开关时同样走默认全开。
 *
 * release / dev 两套构建目标的取值矩阵：
 *   see docs/FEATURE_ROLE_SPLIT_PLAN.md §8.2
 *
 * 注意：本文件只控制“功能是否编入/入口是否启用”，
 *       不得改变 device_config_t 的字段布局（配置二进制只增不删）。
 */

#ifndef FEATURES_H
#define FEATURES_H

/* 工厂自检（factory_test.c）
 * release/dev 均编入：release 中由 Flash manufacturing flag 在运行时门控
 * （产线态可进入、出厂锁定、售后受控解锁）。 */
#ifndef FEATURE_FACTORY_TEST
#define FEATURE_FACTORY_TEST 1
#endif

/* 任务级性能监控（perf_monitor.c：CPU 占用、任务统计、超时阈值）
 * release 关闭（0），dev 开启（1）。 */
#ifndef FEATURE_PERF_MONITOR
#define FEATURE_PERF_MONITOR 1
#endif

/* Fault / 错误日志（fault.c + Flash 持久化）
 * dev 全量；release 保留精简只读版（售后回收日志需要）。 */
#ifndef FEATURE_FAULT_LOG
#define FEATURE_FAULT_LOG 1
#endif

/* 按键使用次数统计（key_stats.c + Flash 持久化）
 * 属诊断/耐久分析，release 关闭，dev 开启。 */
#ifndef FEATURE_KEY_STATS
#define FEATURE_KEY_STATS 1
#endif

/* 调试串口与详细日志（stdio UART，GP0/GP1 @115200）
 * release 关闭，dev 开启。关闭时由构建系统同时降低 LOG_LEVEL。 */
#ifndef FEATURE_DEBUG_UART
#define FEATURE_DEBUG_UART 1
#endif

/* 实时 raw 诊断报告（KeyState / JoystickState 等实时状态 HID 报告）
 * release 关闭，dev 开启。 */
#ifndef FEATURE_RAW_DIAG
#define FEATURE_RAW_DIAG 1
#endif

/* Fault 紧凑存储后端（阶段2）
 * 1 = release 精简档：16B 定长记录 + 按页攒批环形 + A/B 磨损均衡，
 *     公共 API/HID 报告不变（读出时还原为 fault_log_entry_t）；
 * 0 = dev 全量档：256B/条文本日志（历史行为）。
 * 默认 0，由 release 构建目标置 1。 */
#ifndef FEATURE_FAULT_COMPACT
#define FEATURE_FAULT_COMPACT 0
#endif

/* 制造能力上电默认锁定（阶段2过渡门控）
 * 1 = release：撤销 CAP_MFG_*、Device Info 上报 SHIPPED_LOCKED，
 *     HID 制造能力默认关闭（正式掉电保持锁与售后解锁随阶段3 Provisioning 落地）；
 * 0 = dev/工程态：制造能力可用。
 * 注意：本地“开机长按 Fn 自检”入口在阶段2仍保留（物理门槛，不走 HID）。 */
#ifndef FEATURE_MFG_DEFAULT_LOCKED
#define FEATURE_MFG_DEFAULT_LOCKED 0
#endif

/* 自定义 in-app USB DFU bootloader（CMD_ENTER_DFU=0x04 + HID report 0x30~0x36
 * + 专用 BOOTLOADER_VID/PID 的一键/在线升级）。
 *
 * 该开关只表示“自定义 DFU bootloader 是否随固件布局编入”：
 *   1 = bootloader 占用 Flash 高地址段、应用链接到 0x10009000，CAP_DFU_UPGRADE(bit10) 置位；
 *   0 = 无独立 bootloader（应用链接到 0x10000000），bit10 必须上报 0，
 *       上位机据此隐藏“一键/在线 DFU”，避免消费者点击后找不到 bootloader 必失败。
 *
 * 注意：RP2350 原生 BOOTSEL UF2 升级（CMD_ENTER_BOOTSEL=0x14，RPI-RP2 盘符拖放）
 *       是芯片固化能力，与本开关无关，始终可用，上位机应始终保留该手动升级入口。
 *
 * 当前 bootloader 被 CMakeLists 临时禁用（见其 [临时禁用 DFU/Bootloader] 注释），
 * 故 release/dev 两个构建目标都显式注入 0；host 单元测试不带 CMake 参数，
 * 按本文件默认 1（工程全开形态）验证“编入即置位”。
 * 恢复 bootloader 时：CMake 改为 1 + 链接 0x10009000 + linker override
 * （详见 CMakeLists.txt 末尾恢复步骤）。 */
#ifndef FEATURE_DFU_BOOTLOADER
#define FEATURE_DFU_BOOTLOADER 1
#endif

#endif /* FEATURES_H */
