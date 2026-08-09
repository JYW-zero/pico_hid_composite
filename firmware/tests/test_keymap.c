/*
 * tests/test_keymap.c
 * 按键映射模块单元测试
 */

#include "unity.h"
#include "app/keymap.h"
#include "board/config.h"
#include "mock/mock_flash.h"
#include <string.h>

/* 测试辅助函数：每个keymap测试前调用 */
static void keymap_test_setup(void)
{
    mock_flash_reset();
    config_init();
    keymap_init();
}

/* ==================== 普通层测试 ==================== */

/* 测试1：普通层 - 普通键（A键，索引29，键码0x04） */
void test_keymap_normal_key(void)
{
    keymap_test_setup();
    keymap_result_t result;
    bool valid = keymap_lookup(29, false, &result);

    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_NORMAL, result.type);
    TEST_ASSERT_EQUAL_INT(0x04, result.code);  /* HID_KEY_A */
}

/* 测试2：普通层 - 修饰键（左Shift，索引41，值0xFF） */
void test_keymap_modifier_key(void)
{
    keymap_test_setup();
    keymap_result_t result;
    bool valid = keymap_lookup(41, false, &result);

    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_MODIFIER, result.type);
}

/* 测试3：普通层 - Fn键本身（索引59，值0xFE） */
void test_keymap_fn_key(void)
{
    keymap_test_setup();
    keymap_result_t result;
    bool valid = keymap_lookup(59, false, &result);

    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_FN, result.type);
}

/* 测试4：普通层 - 空键（值0x00） */
void test_keymap_none_key(void)
{
    keymap_test_setup();
    /* 设置一个空键：修改配置并保存 */
    device_config_t cfg = *config_get();
    memset(cfg.keymap, 0, 64);
    config_save(&cfg);
    keymap_init();  /* 重新加载 */

    keymap_result_t result;
    bool valid = keymap_lookup(0, false, &result);

    TEST_ASSERT_FALSE(valid);
    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_NONE, result.type);
}

/* ==================== Fn层测试 ==================== */

/* 测试5：Fn层 - F1键（数字1键，索引1，Fn层是F1=0x3A） */
void test_keymap_fn_layer_fkey(void)
{
    keymap_test_setup();
    keymap_result_t result;
    bool valid = keymap_lookup(1, true, &result);

    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_NORMAL, result.type);
    TEST_ASSERT_EQUAL_INT(0x3A, result.code);  /* HID_KEY_F1 */
}

/* 测试6：Fn层 - 多媒体键（Q键，索引15，Fn层是音量+，0xF0） */
void test_keymap_fn_layer_consumer(void)
{
    keymap_test_setup();
    keymap_result_t result;
    bool valid = keymap_lookup(15, true, &result);

    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_CONSUMER, result.type);
    TEST_ASSERT_EQUAL_INT(CONSUMER_VOLUME_UP, result.code);
}

/* 测试7：Fn层 - 保持普通层的键（比如字母U，索引21，Fn层和普通层一样） */
void test_keymap_fn_layer_same_as_normal(void)
{
    keymap_test_setup();
    keymap_result_t result_normal, result_fn;

    keymap_lookup(21, false, &result_normal);
    keymap_lookup(21, true, &result_fn);

    TEST_ASSERT_EQUAL_INT(result_normal.type, result_fn.type);
    TEST_ASSERT_EQUAL_INT(result_normal.code, result_fn.code);
}

/* ==================== Fn键设置测试 ==================== */

/* 测试8：设置Fn键索引 */
void test_keymap_set_fn_key(void)
{
    keymap_test_setup();
    keymap_set_fn_key(10);
    TEST_ASSERT_EQUAL_UINT8(10, keymap_get_fn_key());
    TEST_ASSERT_TRUE(keymap_is_fn_key(10));
    TEST_ASSERT_FALSE(keymap_is_fn_key(59));
}

/* 测试9：默认Fn键索引是59 */
void test_keymap_default_fn_key(void)
{
    keymap_test_setup();
    TEST_ASSERT_EQUAL_UINT8(KEYMAP_DEFAULT_FN_INDEX, keymap_get_fn_key());
    TEST_ASSERT_TRUE(keymap_is_fn_key(KEYMAP_DEFAULT_FN_INDEX));
}

/* ==================== 边界测试 ==================== */

/* 测试10：边界 - 索引0 */
void test_keymap_index_0(void)
{
    keymap_test_setup();
    keymap_result_t result;
    bool valid = keymap_lookup(0, false, &result);

    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_NORMAL, result.type);
    TEST_ASSERT_EQUAL_INT(0x29, result.code);  /* HID_KEY_ESCAPE */
}

/* 测试11：边界 - 索引63 */
void test_keymap_index_63(void)
{
    keymap_test_setup();
    keymap_result_t result;
    bool valid = keymap_lookup(63, false, &result);

    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_NORMAL, result.type);
    TEST_ASSERT_EQUAL_INT(0x4C, result.code);  /* HID_KEY_DELETE */
}

/* 测试12：多媒体键 - 音量-（W键，索引16，Fn层） */
void test_keymap_consumer_volume_down(void)
{
    keymap_test_setup();
    keymap_result_t result;
    keymap_lookup(16, true, &result);

    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_CONSUMER, result.type);
    TEST_ASSERT_EQUAL_INT(CONSUMER_VOLUME_DOWN, result.code);
}

/* 测试13：多媒体键 - 静音（E键，索引17，Fn层） */
void test_keymap_consumer_mute(void)
{
    keymap_test_setup();
    keymap_result_t result;
    keymap_lookup(17, true, &result);

    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_CONSUMER, result.type);
    TEST_ASSERT_EQUAL_INT(CONSUMER_MUTE, result.code);
}

/* 测试14：多媒体键 - 播放/暂停（R键，索引18，Fn层） */
void test_keymap_consumer_play_pause(void)
{
    keymap_test_setup();
    keymap_result_t result;
    keymap_lookup(18, true, &result);

    TEST_ASSERT_EQUAL_INT(KEYMAP_TYPE_CONSUMER, result.type);
    TEST_ASSERT_EQUAL_INT(CONSUMER_PLAY_PAUSE, result.code);
}








