/*
 * include/app/key_stats.h
 * 按键统计模块
 * 完整版本：RAM统计 + Flash持久化 + 磨损均衡
 * 智能保存策略，减少Flash擦写次数
 */

#ifndef APP_KEY_STATS_H
#define APP_KEY_STATS_H

#include <stdint.h>
#include <stdbool.h>
#include "board/flash_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/* 最大按键数量 */
#define KEY_STATS_MAX_KEYS    64

/* Flash存储区大小：4KB（一个扇区） */
#define KEY_STATS_FLASH_SIZE    FLASH_LAYOUT_SECTOR_SIZE  /* 4096 bytes */

/* 单条记录大小：512字节（2个Flash页）
 * 注意：必须是Flash页大小（256字节）的整数倍，
 * 因为flash_range_program要求页对齐写入
 * 结构体实际大小268字节，512字节留有扩展空间
 */
#define KEY_STATS_RECORD_SIZE   (FLASH_PAGE_SIZE * 2)  /* 512 bytes */

/* 最大记录数：4KB / 512字节 = 8条
 * 顺序写入，写满8次才擦除一次，大大延长Flash寿命
 */
#define KEY_STATS_MAX_RECORDS   (KEY_STATS_FLASH_SIZE / KEY_STATS_RECORD_SIZE)

/*
 * 注意：Flash地址不再使用硬编码宏！
 * 请使用 flash_layout_key_stats_offset() / flash_layout_key_stats_addr()
 *
 * 布局（从Flash末尾向前）：
 *   最后第1个扇区: 配置B区
 *   最后第2个扇区: 配置A区
 *   最后第3个扇区: 错误日志区
 *   最后第4个扇区: 按键统计区
 */

/* 兼容旧代码的宏（不推荐使用，请改用函数） */
#define KEY_STATS_FLASH_OFFSET  flash_layout_key_stats_offset()
#define KEY_STATS_FLASH_ADDR    flash_layout_key_stats_addr()

/* 自动保存间隔：30分钟（1800000毫秒） */
#define KEY_STATS_AUTO_SAVE_INTERVAL_MS    1800000U  /* 30分钟 */

/* 记录魔数："KEY0" */
#define KEY_STATS_MAGIC    0x3059454BU

/* ==================== 数据结构 ==================== */

/* 按键统计记录（Flash存储格式） */
typedef struct __attribute__((packed))
{
    uint32_t magic;                      /* 魔数 0x3059454B */
    uint32_t timestamp_s;                /* 时间戳（秒） */
    uint32_t total_keystrokes;           /* 总按键数 */
    uint32_t counts[KEY_STATS_MAX_KEYS]; /* 每个键的计数 */
    uint32_t crc32;                      /* CRC32校验值（计算前面所有字段） */
} key_stats_record_t;

/* ==================== 对外接口 ==================== */

/**
 * @brief 初始化按键统计模块
 * 从Flash加载最近的统计数据
 */
void key_stats_init(void);

/**
 * @brief 记录一次按键
 * @param key_index 按键索引（0~63）
 */
void key_stats_increment(uint8_t key_index);

/**
 * @brief 获取指定按键的计数
 * @param key_index 按键索引
 * @return 按键次数
 */
uint32_t key_stats_get_count(uint8_t key_index);

/**
 * @brief 获取总按键数
 * @return 总按键次数
 */
uint32_t key_stats_get_total(void);

/**
 * @brief 清零所有统计
 */
void key_stats_reset(void);

/**
 * @brief 保存统计数据到Flash
 * 自动找下一个空闲位置写入，写满了才擦除
 */
void key_stats_save_to_flash(void);

/**
 * @brief 从Flash加载最近的统计数据
 * @return true=加载成功，false=没有有效数据
 */
bool key_stats_load_from_flash(void);

/**
 * @brief 主循环tick（用于自动保存计时）
 */
void key_stats_tick(void);

/**
 * @brief 获取Flash保存次数（用于磨损监控）
 * @return 本次周期内的保存次数
 */
uint32_t key_stats_get_save_count(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEY_STATS_H */
