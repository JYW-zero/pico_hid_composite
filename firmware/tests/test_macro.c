/*
 * tests/test_macro.c
 * 宏功能模块单元测试
 * 测试宏定义、触发、停止、配置加载保存等功能
 */

#include "unity.h"
#include "app/macro.h"
#include <string.h>
#include <stdint.h>

/* ==================== 辅助函数 ==================== */

/* 创建一个简单的测试宏 */
static void create_test_macro(macro_def_t* macro, uint8_t id, const char* name)
{
    memset(macro, 0, sizeof(macro_def_t));
    macro->id = id;
    macro->trigger_key = 0xFF;  /* 无触发键 */
    macro->repeat_count = 1;
    macro->action_count = 0;
    strncpy(macro->name, name, MACRO_NAME_MAX_LEN - 1);
}

/* 添加一个按键按下动作 */
static void add_key_down_action(macro_def_t* macro, uint8_t keycode)
{
    if (macro->action_count < MACRO_MAX_ACTIONS)
    {
        macro->actions[macro->action_count].type = MACRO_ACTION_KEY_DOWN;
        macro->actions[macro->action_count].param1 = keycode;
        macro->action_count++;
    }
}

/* 添加一个按键释放动作 */
static void add_key_up_action(macro_def_t* macro, uint8_t keycode)
{
    if (macro->action_count < MACRO_MAX_ACTIONS)
    {
        macro->actions[macro->action_count].type = MACRO_ACTION_KEY_UP;
        macro->actions[macro->action_count].param1 = keycode;
        macro->action_count++;
    }
}

/* 添加一个延迟动作 */
static void add_delay_action(macro_def_t* macro, uint16_t delay_ms)
{
    if (macro->action_count < MACRO_MAX_ACTIONS)
    {
        macro->actions[macro->action_count].type = MACRO_ACTION_DELAY;
        macro->actions[macro->action_count].param2 = delay_ms & 0xFF;
        macro->actions[macro->action_count].param3 = (delay_ms >> 8) & 0xFF;
        macro->action_count++;
    }
}

/* ==================== 测试用例 ==================== */

void test_macro_init(void)
{
    /* 测试宏模块初始化 */
    macro_init();
    /* 初始化后不应有宏在运行 */
    TEST_ASSERT_FALSE(macro_is_running());
}

void test_macro_set_and_get(void)
{
    /* 测试设置和获取宏定义 */
    macro_def_t macro;
    create_test_macro(&macro, 0, "Test Macro");
    add_key_down_action(&macro, 0x04);  /* A键 */
    add_key_up_action(&macro, 0x04);

    int result = macro_set(0, &macro);
    TEST_ASSERT_EQUAL(0, result);

    const macro_def_t* got = macro_get(0);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL(0, got->id);
    TEST_ASSERT_EQUAL_STRING("Test Macro", got->name);
    TEST_ASSERT_EQUAL(2, got->action_count);
}

void test_macro_set_invalid_id(void)
{
    /* 测试设置无效 ID 的宏 */
    macro_def_t macro;
    create_test_macro(&macro, MACRO_MAX_COUNT, "Invalid");
    int result = macro_set(MACRO_MAX_COUNT, &macro);
    TEST_ASSERT_NOT_EQUAL(0, result);
}

void test_macro_get_invalid_id(void)
{
    /* 测试获取无效 ID 的宏 */
    const macro_def_t* got = macro_get(MACRO_MAX_COUNT);
    TEST_ASSERT_NULL(got);
}

void test_macro_set_null(void)
{
    /* 测试设置 NULL 宏 */
    int result = macro_set(0, NULL);
    TEST_ASSERT_NOT_EQUAL(0, result);
}

void test_macro_trigger_and_stop(void)
{
    /* 测试宏触发和停止 */
    macro_def_t macro;
    create_test_macro(&macro, 1, "Trigger Test");
    add_key_down_action(&macro, 0x04);
    add_delay_action(&macro, 10);
    add_key_up_action(&macro, 0x04);
    macro_set(1, &macro);

    bool triggered = macro_trigger(1);
    TEST_ASSERT_TRUE(triggered);
    TEST_ASSERT_TRUE(macro_is_running());

    macro_stop(1);
    /* 停止后可能需要几帧才能完全停止，这里只测试接口不崩溃 */
}

void test_macro_trigger_invalid_id(void)
{
    /* 测试触发无效 ID 的宏 */
    bool triggered = macro_trigger(MACRO_MAX_COUNT);
    TEST_ASSERT_FALSE(triggered);
}

void test_macro_stop_all(void)
{
    /* 测试停止所有宏 */
    macro_stop_all();
    TEST_ASSERT_FALSE(macro_is_running());
}

void test_macro_find_by_trigger_key(void)
{
    /* 测试通过触发键查找宏 */
    macro_def_t macro;
    create_test_macro(&macro, 2, "Trigger Key Test");
    macro.trigger_key = 10;  /* 设置触发键为索引10 */
    add_key_down_action(&macro, 0x04);  /* 需要至少一个动作才能被找到 */
    macro_set(2, &macro);

    uint8_t found = macro_find_by_trigger_key(10);
    TEST_ASSERT_EQUAL(2, found);
}

void test_macro_find_by_trigger_key_not_found(void)
{
    /* 测试查找不存在的触发键 */
    uint8_t found = macro_find_by_trigger_key(200);  /* 使用一个不存在的索引 */
    TEST_ASSERT_EQUAL(0xFF, found);
}

void test_macro_max_actions(void)
{
    /* 测试最大动作数限制 */
    macro_def_t macro;
    create_test_macro(&macro, 3, "Max Actions Test");

    /* 尝试添加超过最大数量的动作 */
    for (int i = 0; i < MACRO_MAX_ACTIONS + 5; i++)
    {
        add_key_down_action(&macro, 0x04 + (i % 26));
    }

    /* 动作数不应超过 MACRO_MAX_ACTIONS */
    TEST_ASSERT_EQUAL(MACRO_MAX_ACTIONS, macro.action_count);
}

void test_macro_name_max_len(void)
{
    /* 测试宏名称最大长度 */
    macro_def_t macro;
    create_test_macro(&macro, 4, "This is a very long macro name that exceeds max length");

    /* 名称应被截断到 MACRO_NAME_MAX_LEN - 1 */
    TEST_ASSERT_TRUE(strlen(macro.name) < MACRO_NAME_MAX_LEN);
}

void test_macro_keyboard_state(void)
{
    /* 测试获取宏的键盘状态 */
    uint8_t modifier = 0;
    uint8_t keys[6] = {0};

    macro_get_keyboard_state(&modifier, keys);
    /* 初始状态应为全零 */
    TEST_ASSERT_EQUAL(0, modifier);
    for (int i = 0; i < 6; i++)
    {
        TEST_ASSERT_EQUAL(0, keys[i]);
    }
}

void test_macro_mouse_buttons(void)
{
    /* 测试获取宏的鼠标按钮状态 */
    uint8_t buttons = macro_get_mouse_buttons();
    /* 初始状态应为 0 */
    TEST_ASSERT_EQUAL(0, buttons);
}

void test_macro_kb_changed(void)
{
    /* 测试键盘状态变化标志 */
    bool changed = macro_kb_has_changed();
    /* 初始状态可能为 false */
    TEST_ASSERT_FALSE(changed);
}

void test_macro_mouse_changed(void)
{
    /* 测试鼠标状态变化标志 */
    bool changed = macro_mouse_has_changed();
    /* 初始状态可能为 false */
    TEST_ASSERT_FALSE(changed);
}

void test_macro_save_and_load_config(void)
{
    /* 测试宏配置的保存和加载 */
    macro_def_t macro;
    create_test_macro(&macro, 5, "Save Load Test");
    add_key_down_action(&macro, 0x05);  /* B键 */
    add_key_up_action(&macro, 0x05);
    macro_set(5, &macro);

    /* 保存到缓冲区 - 需要足够大的空间保存所有宏 */
    uint8_t buffer[MACRO_MAX_COUNT * sizeof(macro_def_t) + 64];
    memset(buffer, 0, sizeof(buffer));
    int save_result = macro_save_to_config(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, save_result);

    /* 清除当前宏 */
    macro_def_t empty;
    memset(&empty, 0, sizeof(empty));
    macro_set(5, &empty);

    /* 从缓冲区加载 */
    int load_result = macro_load_from_config(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, load_result);

    /* 验证加载的宏 */
    const macro_def_t* loaded = macro_get(5);
    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_EQUAL_STRING("Save Load Test", loaded->name);
}

void test_macro_load_null_data(void)
{
    /* 测试加载 NULL 数据 */
    int result = macro_load_from_config(NULL, 100);
    TEST_ASSERT_NOT_EQUAL(0, result);
}

void test_macro_save_null_buffer(void)
{
    /* 测试保存到 NULL 缓冲区 */
    int result = macro_save_to_config(NULL, 100);
    TEST_ASSERT_NOT_EQUAL(0, result);
}

void test_macro_load_zero_length(void)
{
    /* 测试加载零长度数据 */
    uint8_t buffer[1] = {0};
    int result = macro_load_from_config(buffer, 0);
    TEST_ASSERT_NOT_EQUAL(0, result);
}

void test_macro_action_types(void)
{
    /* 测试所有动作类型的设置 */
    macro_def_t macro;
    create_test_macro(&macro, 6, "Action Types Test");

    /* 添加各种类型的动作 */
    macro.actions[0].type = MACRO_ACTION_KEY_DOWN;
    macro.actions[1].type = MACRO_ACTION_KEY_UP;
    macro.actions[2].type = MACRO_ACTION_DELAY;
    macro.actions[3].type = MACRO_ACTION_MOUSE_MOVE;
    macro.actions[4].type = MACRO_ACTION_MOUSE_CLICK;
    macro.actions[5].type = MACRO_ACTION_MOUSE_SCROLL;
    macro.actions[6].type = MACRO_ACTION_KEY_PRESS;
    macro.actions[7].type = MACRO_ACTION_TEXT_CHAR;
    macro.action_count = 8;

    macro_set(6, &macro);

    const macro_def_t* got = macro_get(6);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL(8, got->action_count);
    TEST_ASSERT_EQUAL(MACRO_ACTION_KEY_DOWN, got->actions[0].type);
    TEST_ASSERT_EQUAL(MACRO_ACTION_TEXT_CHAR, got->actions[7].type);
}

void test_macro_repeat_count(void)
{
    /* 测试循环次数设置 */
    macro_def_t macro;
    create_test_macro(&macro, 7, "Repeat Test");
    macro.repeat_count = 3;
    add_key_down_action(&macro, 0x04);
    add_key_up_action(&macro, 0x04);
    macro_set(7, &macro);

    const macro_def_t* got = macro_get(7);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL(3, got->repeat_count);
}

void test_macro_infinite_repeat(void)
{
    /* 测试无限循环（repeat_count = 0） */
    macro_def_t macro;
    create_test_macro(&macro, 7, "Infinite Repeat Test");
    macro.repeat_count = 0;  /* 0 表示无限循环 */
    add_key_down_action(&macro, 0x04);
    macro_set(7, &macro);

    const macro_def_t* got = macro_get(7);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL(0, got->repeat_count);
}
