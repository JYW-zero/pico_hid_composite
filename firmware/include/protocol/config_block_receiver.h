/*
 * include/protocol/config_block_receiver.h
 * 配置块接收状态管理接口
 * 从 config_hid.c 提取，包含配置块接收状态跟踪、标记和完成检查
 */
#ifndef PROTOCOL_CONFIG_BLOCK_RECEIVER_H
#define PROTOCOL_CONFIG_BLOCK_RECEIVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量 ==================== */

/* 所有块接收完成的位图掩码（0x01 | 0x02 | 0x04 = 0x07） */
#define CONFIG_BLOCKS_ALL   0x07u

/* 配置结构块大小（字节，用于计算块偏移量）
 * 注意：与 usb_descriptors.h 中的 CONFIG_BLOCK_SIZE（USB报告大小=62）含义不同
 */
#define CONFIG_RECEIVER_BLOCK_SIZE  512u

/* ==================== 初始化 ==================== */

/* 初始化配置块接收状态（清零接收位图） */
void config_block_receiver_init(void);

/* ==================== 状态管理 ==================== */

/* 重置配置块接收状态（清零接收位图）
 * 替代原 config_hid_reset_blocks_received()
 */
void config_block_receiver_reset(void);

/* 标记指定块已接收
 * @param block_bit 块位图（0x01=BLOCK0, 0x02=BLOCK1, 0x04=BLOCK2）
 */
void config_block_receiver_mark_block(uint8_t block_bit);

/* 检查所有配置块是否都已接收
 * @return true=所有块已接收，false=还有块未接收
 */
bool config_block_receiver_is_complete(void);

/* 获取当前接收状态位图（用于调试日志）
 * @return 接收状态位图
 */
uint8_t config_block_receiver_get_status(void);

/* ==================== 块信息查询 ==================== */

/* 根据 Report ID 获取块信息
 * @param report_id HID 报告 ID
 * @param offset 输出参数：块在配置结构中的字节偏移量
 * @param block_bit 输出参数：块位图（0x01/0x02/0x04，0=无效报告ID）
 */
void config_block_receiver_get_block_info(uint8_t report_id, uint16_t *offset, uint8_t *block_bit);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_CONFIG_BLOCK_RECEIVER_H */
