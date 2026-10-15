/*
 * include/middleware/fault_backend.h
 * Fault 持久化后端内部接口（不对外暴露给上位机/应用层）
 *
 * fault.c 只保留分级处理、串口输出、限频与公共 API；具体“如何落 Flash”
 * 由两套互斥实现提供：
 *   - fault_full.c    ：FEATURE_FAULT_COMPACT=0（dev），256B/条文本日志（历史行为）
 *   - fault_compact.c ：FEATURE_FAULT_COMPACT=1（release），16B/条紧凑环形 + 磨损均衡
 * 两个实现文件都参与编译，但各自用 FEATURE 宏整体守护，每个构建目标只有一份生效。
 */

#ifndef MIDDLEWARE_FAULT_BACKEND_H
#define MIDDLEWARE_FAULT_BACKEND_H

#include "middleware/fault.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化持久化后端：扫描 A/B 扇区恢复历史；紧凑档还会追加本次启动事件。
 * fault_init 在完成串口锁与 flash_layout_init 之后调用一次。 */
void fault_backend_init(void);

/* 持久化一条故障（fault.c 保证：仅 Core0、level>=ERROR、且已过外层限频；
 * module/msg 已保证非 NULL）。全量档立即写盘；紧凑档按页攒批。 */
void fault_backend_record(fault_level_e level, const char *module, const char *msg);

/* 当前对外可见的日志条数（紧凑档为最近窗口，<= FAULT_COMPACT_VIEW_COUNT） */
uint32_t fault_backend_get_log_count(void);

/* 读取第 index 条可见日志（0=窗口最旧），由后端还原为公共 fault_log_entry_t。
 * 紧凑档在此把 16B 记录（code/module_id/occurrences/boot_count）还原为文本。 */
bool fault_backend_get_log(uint32_t index, fault_log_entry_t *out_entry);

/* 清空 A/B 两个扇区并复位后端游标 */
void fault_backend_clear(void);

/* 强制把尚未落盘的记录刷到 Flash（FATAL 复位前调用，保证致命故障不丢）。
 * 全量档为立即写入，此函数为空操作；紧凑档会编程当前页（页内空位以 PAD 填充）。 */
void fault_backend_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_FAULT_BACKEND_H */
