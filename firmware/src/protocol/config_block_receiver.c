/*
 * src/protocol/config_block_receiver.c
 * 配置块接收状态管理实现
 * 从 config_hid.c 提取，包含配置块接收状态跟踪、标记和完成检查
 */
#include "protocol/config_block_receiver.h"
#include "protocol/usb_descriptors.h"

/* ==================== 私有数据 ==================== */

/* 已接收配置块位图（bit0=BLOCK0, bit1=BLOCK1, bit2=BLOCK2） */
static volatile uint8_t s_blocks_received = 0;

/* ==================== 初始化 ==================== */

void config_block_receiver_init(void)
{
  s_blocks_received = 0;
}

/* ==================== 状态管理 ==================== */

void config_block_receiver_reset(void)
{
  s_blocks_received = 0;
}

void config_block_receiver_mark_block(uint8_t block_bit)
{
  s_blocks_received |= block_bit;
}

bool config_block_receiver_is_complete(void)
{
  return (s_blocks_received == CONFIG_BLOCKS_ALL);
}

uint8_t config_block_receiver_get_status(void)
{
  return s_blocks_received;
}

/* ==================== 块信息查询 ==================== */

void config_block_receiver_get_block_info(uint8_t report_id, uint16_t *offset, uint8_t *block_bit)
{
  switch (report_id)
  {
    case REPORT_ID_CONFIG_BLOCK0:
      *offset = 0;
      *block_bit = 0x01;
      break;

    case REPORT_ID_CONFIG_BLOCK1:
      *offset = CONFIG_RECEIVER_BLOCK_SIZE;
      *block_bit = 0x02;
      break;

    case REPORT_ID_CONFIG_BLOCK2:
      *offset = CONFIG_RECEIVER_BLOCK_SIZE * 2;
      *block_bit = 0x04;
      break;

    default:
      *offset = 0;
      *block_bit = 0;
      break;
  }
}
