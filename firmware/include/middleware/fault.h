/*
 * include/middleware/fault.h
 * 统一故障处理中间件
 * 完整版本：分级处理 + Flash持久化存储 + 环形缓冲区
 * 掉电不丢失，方便事后排查问题
 */

#ifndef MIDDLEWARE_FAULT_H
#define MIDDLEWARE_FAULT_H

#include <stdint.h>
#include <stdbool.h>
#include "board/flash_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/* 故障级别 */
typedef enum
{
    FAULT_LEVEL_INFO = 0,   /* 信息级，仅记录 */
    FAULT_LEVEL_WARN,       /* 警告级，记录并提示 */
    FAULT_LEVEL_ERROR,      /* 错误级，记录并尝试恢复 */
    FAULT_LEVEL_FATAL       /* 致命级，记录后复位 */
} fault_level_e;

/* 错误日志魔数："FLT0" (Fault Log entry 0) */
#define FAULT_LOG_MAGIC     0x30544C46U

/* 单条日志大小：256字节（Flash一页，确保页对齐）
 * 虽然有点浪费，但是可靠，避免不对齐写入导致的问题
 */
#define FAULT_LOG_ENTRY_SIZE    FLASH_PAGE_SIZE  /* 256 bytes */

/* Flash日志区大小：4KB（一个扇区） */
#define FAULT_LOG_FLASH_SIZE    FLASH_LAYOUT_SECTOR_SIZE  /* 4096 bytes */

/* 最大日志条数：4KB / 256字节 = 16条 */
#define FAULT_LOG_MAX_COUNT     (FAULT_LOG_FLASH_SIZE / FAULT_LOG_ENTRY_SIZE)

/*
 * 注意：Flash地址不再使用硬编码宏！
 * 请使用 flash_layout_fault_offset() / flash_layout_fault_addr()
 *
 * 布局（从Flash末尾向前）：
 *   最后第1个扇区: 配置B区
 *   最后第2个扇区: 配置A区
 *   最后第3个扇区: 错误日志区
 *   最后第4个扇区: 按键统计区
 */

/* 兼容旧代码的宏（不推荐使用，请改用函数） */
#define FAULT_LOG_FLASH_OFFSET  flash_layout_fault_offset()
#define FAULT_LOG_FLASH_ADDR    flash_layout_fault_addr()

/* ==================== 日志条目结构体 ==================== */

/* 错误日志条目
 * 大小：256字节（Flash一页，确保页对齐）
 * 布局：头部(16字节) + 模块名(32字节) + 消息(208字节) = 256字节
 */
typedef struct __attribute__((packed))
{
    uint32_t magic;          /* 魔数 0x30544C46，标识有效记录 */
    uint32_t timestamp_ms;   /* 时间戳（启动后毫秒数） */
    uint8_t  level;          /* 错误级别 fault_level_e */
    uint8_t  module_len;     /* 模块名长度 */
    uint16_t msg_len;        /* 消息长度 */
    uint8_t  reserved[8];    /* 保留字节，对齐用 */
    char     module[32];     /* 模块名（最多31字符 + 结束符） */
    char     msg[200];       /* 错误消息（最多199字符 + 结束符） */
} fault_log_entry_t;

/* ==================== 对外接口 ==================== */

/**
 * @brief 初始化故障处理模块
 * 从Flash加载历史日志，初始化环形缓冲区
 */
void fault_init(void);

/**
 * @brief 记录一条故障
 * @param level 故障级别
 * @param module 模块名（最多15字符）
 * @param msg 故障消息（最多35字符）
 */
void fault_record(fault_level_e level, const char *module, const char *msg);

/**
 * @brief 获取总故障计数
 * @return 故障总数
 */
uint32_t fault_get_count(void);

/**
 * @brief 获取当前日志条数
 * @return 日志条数
 */
uint32_t fault_get_log_count(void);

/**
 * @brief 读取指定索引的日志
 * @param index 索引（0=最旧，count-1=最新）
 * @param out_entry 输出日志条目
 * @return true=成功，false=索引越界
 */
bool fault_get_log(uint32_t index, fault_log_entry_t* out_entry);

/**
 * @brief 获取最新的N条日志
 * @param out_entries 输出缓冲区
 * @param max_count 最大条数
 * @return 实际读取的条数
 */
uint32_t fault_get_latest_logs(fault_log_entry_t* out_entries, uint32_t max_count);

/**
 * @brief 清除所有故障记录（包括Flash）
 */
void fault_clear(void);

/**
 * @brief 获取级别名称字符串
 * @param level 级别
 * @return 级别名称
 */
const char* fault_level_name(fault_level_e level);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_FAULT_H */
