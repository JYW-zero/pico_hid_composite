/*
 * tests/test_config_block_receiver.c
 * 配置块接收状态管理模块单元测试
 * 测试 config_block_receiver.c 中的所有函数
 *
 * 覆盖范围：
 * - 初始化/重置（状态清零）
 * - 块标记（单个块、多个块、重复标记）
 * - 完成检查（部分完成、全部完成、未完成）
 * - 状态查询（get_status）
 * - 块信息查询（get_block_info，3个有效report_id + 无效report_id）
 * - 完整接收流程（端到端测试）
 */

#include "unity.h"
#include "protocol/config_block_receiver.h"
#include "protocol/usb_descriptors.h"
#include <stdint.h>
#include <stdbool.h>

/* ==================== 辅助函数 ==================== */

/* 每个测试前重置接收状态 */
static void reset_receiver(void)
{
    config_block_receiver_reset();
}

/* ==================== 测试用例 ==================== */

/* 测试1：初始化后状态为0 */
void test_config_block_receiver_init_zero(void)
{
    config_block_receiver_init();
    TEST_ASSERT_EQUAL_UINT8(0, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());
}

/* 测试2：重置后状态为0 */
void test_config_block_receiver_reset_zero(void)
{
    /* 先标记一些块，然后重置 */
    config_block_receiver_mark_block(0x01);
    config_block_receiver_mark_block(0x02);
    TEST_ASSERT_NOT_EQUAL_UINT8(0, config_block_receiver_get_status());

    /* 重置后应该为0 */
    reset_receiver();
    TEST_ASSERT_EQUAL_UINT8(0, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());
}

/* 测试3：标记BLOCK0后状态为0x01 */
void test_config_block_receiver_mark_block0(void)
{
    reset_receiver();
    config_block_receiver_mark_block(0x01);
    TEST_ASSERT_EQUAL_UINT8(0x01, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());
}

/* 测试4：标记BLOCK1后状态为0x02 */
void test_config_block_receiver_mark_block1(void)
{
    reset_receiver();
    config_block_receiver_mark_block(0x02);
    TEST_ASSERT_EQUAL_UINT8(0x02, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());
}

/* 测试5：标记BLOCK2后状态为0x04 */
void test_config_block_receiver_mark_block2(void)
{
    reset_receiver();
    config_block_receiver_mark_block(0x04);
    TEST_ASSERT_EQUAL_UINT8(0x04, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());
}

/* 测试6：标记多个块后状态正确（位或操作） */
void test_config_block_receiver_mark_multiple(void)
{
    reset_receiver();
    config_block_receiver_mark_block(0x01);
    config_block_receiver_mark_block(0x02);
    TEST_ASSERT_EQUAL_UINT8(0x03, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());

    config_block_receiver_mark_block(0x04);
    TEST_ASSERT_EQUAL_UINT8(0x07, config_block_receiver_get_status());
    TEST_ASSERT_TRUE(config_block_receiver_is_complete());
}

/* 测试7：重复标记同一个块不会改变状态 */
void test_config_block_receiver_mark_duplicate(void)
{
    reset_receiver();
    config_block_receiver_mark_block(0x01);
    TEST_ASSERT_EQUAL_UINT8(0x01, config_block_receiver_get_status());

    /* 重复标记同一个块，状态不应改变 */
    config_block_receiver_mark_block(0x01);
    TEST_ASSERT_EQUAL_UINT8(0x01, config_block_receiver_get_status());

    config_block_receiver_mark_block(0x01);
    TEST_ASSERT_EQUAL_UINT8(0x01, config_block_receiver_get_status());
}

/* 测试8：标记所有块后is_complete返回true */
void test_config_block_receiver_is_complete_all(void)
{
    reset_receiver();
    config_block_receiver_mark_block(0x01);
    config_block_receiver_mark_block(0x02);
    config_block_receiver_mark_block(0x04);
    TEST_ASSERT_TRUE(config_block_receiver_is_complete());
}

/* 测试9：只标记部分块时is_complete返回false */
void test_config_block_receiver_is_complete_partial(void)
{
    reset_receiver();
    config_block_receiver_mark_block(0x01);
    config_block_receiver_mark_block(0x02);
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());
}

/* 测试10：没有标记任何块时is_complete返回false */
void test_config_block_receiver_is_complete_none(void)
{
    reset_receiver();
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());
}

/* 测试11：get_status返回正确的状态位图 */
void test_config_block_receiver_get_status(void)
{
    reset_receiver();
    TEST_ASSERT_EQUAL_UINT8(0, config_block_receiver_get_status());

    config_block_receiver_mark_block(0x01);
    TEST_ASSERT_EQUAL_UINT8(0x01, config_block_receiver_get_status());

    config_block_receiver_mark_block(0x04);
    TEST_ASSERT_EQUAL_UINT8(0x05, config_block_receiver_get_status());

    config_block_receiver_mark_block(0x02);
    TEST_ASSERT_EQUAL_UINT8(0x07, config_block_receiver_get_status());
}

/* 测试12：get_block_info对BLOCK0返回偏移0和位0x01 */
void test_config_block_receiver_get_block_info_block0(void)
{
    uint16_t offset = 0xFFFF;
    uint8_t block_bit = 0xFF;

    config_block_receiver_get_block_info(REPORT_ID_CONFIG_BLOCK0, &offset, &block_bit);

    TEST_ASSERT_EQUAL_UINT16(0, offset);
    TEST_ASSERT_EQUAL_UINT8(0x01, block_bit);
}

/* 测试13：get_block_info对BLOCK1返回偏移512和位0x02 */
void test_config_block_receiver_get_block_info_block1(void)
{
    uint16_t offset = 0xFFFF;
    uint8_t block_bit = 0xFF;

    config_block_receiver_get_block_info(REPORT_ID_CONFIG_BLOCK1, &offset, &block_bit);

    TEST_ASSERT_EQUAL_UINT16(CONFIG_RECEIVER_BLOCK_SIZE, offset);
    TEST_ASSERT_EQUAL_UINT8(0x02, block_bit);
}

/* 测试14：get_block_info对BLOCK2返回偏移1024和位0x04 */
void test_config_block_receiver_get_block_info_block2(void)
{
    uint16_t offset = 0xFFFF;
    uint8_t block_bit = 0xFF;

    config_block_receiver_get_block_info(REPORT_ID_CONFIG_BLOCK2, &offset, &block_bit);

    TEST_ASSERT_EQUAL_UINT16(CONFIG_RECEIVER_BLOCK_SIZE * 2, offset);
    TEST_ASSERT_EQUAL_UINT8(0x04, block_bit);
}

/* 测试15：get_block_info对无效report_id返回偏移0和位0 */
void test_config_block_receiver_get_block_info_invalid(void)
{
    uint16_t offset = 0xFFFF;
    uint8_t block_bit = 0xFF;

    /* 使用一个无效的report_id（比如0或100） */
    config_block_receiver_get_block_info(0, &offset, &block_bit);
    TEST_ASSERT_EQUAL_UINT16(0, offset);
    TEST_ASSERT_EQUAL_UINT8(0, block_bit);

    /* 重置后再测试另一个无效值 */
    offset = 0xFFFF;
    block_bit = 0xFF;
    config_block_receiver_get_block_info(100, &offset, &block_bit);
    TEST_ASSERT_EQUAL_UINT16(0, offset);
    TEST_ASSERT_EQUAL_UINT8(0, block_bit);
}

/* 测试16：完整接收流程（端到端测试） */
void test_config_block_receiver_full_flow(void)
{
    /* 模拟完整的配置块接收流程 */
    reset_receiver();
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());

    /* 接收块0 */
    uint16_t offset;
    uint8_t block_bit;
    config_block_receiver_get_block_info(REPORT_ID_CONFIG_BLOCK0, &offset, &block_bit);
    config_block_receiver_mark_block(block_bit);
    TEST_ASSERT_EQUAL_UINT8(0x01, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());

    /* 接收块1 */
    config_block_receiver_get_block_info(REPORT_ID_CONFIG_BLOCK1, &offset, &block_bit);
    config_block_receiver_mark_block(block_bit);
    TEST_ASSERT_EQUAL_UINT8(0x03, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());

    /* 接收块2 */
    config_block_receiver_get_block_info(REPORT_ID_CONFIG_BLOCK2, &offset, &block_bit);
    config_block_receiver_mark_block(block_bit);
    TEST_ASSERT_EQUAL_UINT8(0x07, config_block_receiver_get_status());
    TEST_ASSERT_TRUE(config_block_receiver_is_complete());

    /* 重置后可以开始新一轮接收 */
    reset_receiver();
    TEST_ASSERT_EQUAL_UINT8(0, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());
}

/* 测试17：乱序接收块（块2 -> 块0 -> 块1） */
void test_config_block_receiver_out_of_order(void)
{
    reset_receiver();

    /* 先接收块2 */
    config_block_receiver_mark_block(0x04);
    TEST_ASSERT_EQUAL_UINT8(0x04, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());

    /* 再接收块0 */
    config_block_receiver_mark_block(0x01);
    TEST_ASSERT_EQUAL_UINT8(0x05, config_block_receiver_get_status());
    TEST_ASSERT_FALSE(config_block_receiver_is_complete());

    /* 最后接收块1 */
    config_block_receiver_mark_block(0x02);
    TEST_ASSERT_EQUAL_UINT8(0x07, config_block_receiver_get_status());
    TEST_ASSERT_TRUE(config_block_receiver_is_complete());
}

/* 测试18：CONFIG_BLOCKS_ALL常量值正确 */
void test_config_block_receiver_constant_all(void)
{
    /* 验证 CONFIG_BLOCKS_ALL = 0x01 | 0x02 | 0x04 = 0x07 */
    TEST_ASSERT_EQUAL_UINT8(0x07, CONFIG_BLOCKS_ALL);
    TEST_ASSERT_EQUAL_UINT8(0x01 | 0x02 | 0x04, CONFIG_BLOCKS_ALL);
}

/* 测试19：CONFIG_RECEIVER_BLOCK_SIZE常量值正确 */
void test_config_block_receiver_constant_block_size(void)
{
    TEST_ASSERT_EQUAL_UINT16(512, CONFIG_RECEIVER_BLOCK_SIZE);
}

/* 测试20：标记0（无操作）不会改变状态 */
void test_config_block_receiver_mark_zero(void)
{
    reset_receiver();
    config_block_receiver_mark_block(0x01);
    TEST_ASSERT_EQUAL_UINT8(0x01, config_block_receiver_get_status());

    /* 标记0应该不改变状态 */
    config_block_receiver_mark_block(0);
    TEST_ASSERT_EQUAL_UINT8(0x01, config_block_receiver_get_status());
}
