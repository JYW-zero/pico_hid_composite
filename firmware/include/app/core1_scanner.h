/*
 * include/app/core1_scanner.h
 * Core1 硬件扫描模块
 * 负责所有硬件外设的周期性扫描，结果写入共享数据
 */
#ifndef APP_CORE1_SCANNER_H
#define APP_CORE1_SCANNER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Core1 主入口函数
 * 由 multicore_launch_core1 调用
 * 注意：所有硬件必须在 Core0 上先初始化好
 */
void core1_scanner_main(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CORE1_SCANNER_H */
