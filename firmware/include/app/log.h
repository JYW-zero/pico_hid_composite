/*
 * include/app/log.h
 * 统一日志宏定义（从 main.c 和 config_hid.c 提取，避免代码重复）
 *
 * 编译时可通过 -DLOG_LEVEL=N 控制日志输出级别：
 *   0 = NONE  (无输出)
 *   1 = ERROR (仅错误)
 *   2 = INFO  (错误+信息，默认)
 *   3 = DEBUG (错误+信息+调试)
 */

#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdio.h>

/* 日志级别定义 */
#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif

#define LOG_NONE  0
#define LOG_ERROR 1
#define LOG_WARN  2
#define LOG_INFO  2
#define LOG_DEBUG 3

/* 日志输出宏（使用 do-while(0) 确保宏在 if/else 中行为正确） */
#define LOG_ERROR_PRINT(fmt, ...) do { if (LOG_LEVEL >= LOG_ERROR) printf("[ERR] " fmt, ##__VA_ARGS__); } while(0)
#define LOG_WARN_PRINT(fmt, ...)  do { if (LOG_LEVEL >= LOG_WARN)  printf("[WARN] " fmt, ##__VA_ARGS__); } while(0)
#define LOG_INFO_PRINT(fmt, ...)  do { if (LOG_LEVEL >= LOG_INFO)  printf("[INFO] " fmt, ##__VA_ARGS__); } while(0)
#define LOG_DEBUG_PRINT(fmt, ...) do { if (LOG_LEVEL >= LOG_DEBUG) printf("[DBG] " fmt, ##__VA_ARGS__); } while(0)

#endif /* APP_LOG_H */
