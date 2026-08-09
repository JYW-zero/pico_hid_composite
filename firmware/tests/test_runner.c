/*
 * tests/test_runner.c
 * 单元测试主入口
 */

#include "unity.h"

/* ==================== debounce 测试函数声明 ==================== */
void test_debounce_initial_state(void);
void test_debounce_single_key_press(void);
void test_debounce_single_key_release(void);
void test_debounce_multiple_keys(void);
void test_debounce_bounce(void);
void test_debounce_threshold_1(void);

/* ==================== keymap 测试函数声明 ==================== */
void test_keymap_normal_key(void);
void test_keymap_modifier_key(void);
void test_keymap_fn_key(void);
void test_keymap_none_key(void);
void test_keymap_fn_layer_fkey(void);
void test_keymap_fn_layer_consumer(void);
void test_keymap_fn_layer_same_as_normal(void);
void test_keymap_set_fn_key(void);
void test_keymap_default_fn_key(void);
void test_keymap_index_0(void);
void test_keymap_index_63(void);
void test_keymap_consumer_volume_down(void);
void test_keymap_consumer_mute(void);
void test_keymap_consumer_play_pause(void);

/* ==================== encoder 测试函数声明 ==================== */
void test_encoder_init_state(void);
void test_encoder_clockwise_full(void);
void test_encoder_counter_clockwise_full(void);
void test_encoder_bounce(void);
void test_encoder_steps_per_tick_4(void);
void test_encoder_direction_reverse(void);
void test_encoder_no_change(void);
void test_encoder_switch(void);

/* ==================== scheduler 测试函数声明 ==================== */
void test_scheduler_init(void);
void test_scheduler_single_task(void);
void test_scheduler_multiple_tasks(void);
void test_scheduler_priority_order(void);
void test_scheduler_null_task(void);
void test_scheduler_zero_count(void);
void test_scheduler_overflow(void);
void test_scheduler_first_run(void);
void test_scheduler_updates_last_run(void);

/* ==================== fault 测试函数声明 ==================== */
void test_fault_initial_count(void);
void test_fault_record_one(void);
void test_fault_record_multiple(void);
void test_fault_clear(void);
void test_fault_all_levels(void);

/* ==================== config 测试函数声明 ==================== */
void test_config_init_empty_flash(void);
void test_config_save_and_load(void);
void test_config_save_seq_increments(void);
void test_config_alternate_sectors(void);
void test_config_one_sector_corrupted(void);
void test_config_both_sectors_corrupted(void);
void test_config_reset_default(void);
void test_config_get_default(void);
void test_config_save_null(void);
void test_config_power_failure_protection(void);
void test_config_size_fits_sector(void);
void test_config_default_keymap_not_empty(void);

/* 全局setUp和tearDown，Unity要求必须有
 * 每个测试函数前后都会调用
 * 各模块自己的初始化在测试函数内部完成
 */
void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();

    /* ==================== debounce 模块测试 ==================== */
    RUN_TEST(test_debounce_initial_state);
    RUN_TEST(test_debounce_single_key_press);
    RUN_TEST(test_debounce_single_key_release);
    RUN_TEST(test_debounce_multiple_keys);
    RUN_TEST(test_debounce_bounce);
    RUN_TEST(test_debounce_threshold_1);

    /* ==================== keymap 模块测试 ==================== */
    RUN_TEST(test_keymap_normal_key);
    RUN_TEST(test_keymap_modifier_key);
    RUN_TEST(test_keymap_fn_key);
    RUN_TEST(test_keymap_none_key);
    RUN_TEST(test_keymap_fn_layer_fkey);
    RUN_TEST(test_keymap_fn_layer_consumer);
    RUN_TEST(test_keymap_fn_layer_same_as_normal);
    RUN_TEST(test_keymap_set_fn_key);
    RUN_TEST(test_keymap_default_fn_key);
    RUN_TEST(test_keymap_index_0);
    RUN_TEST(test_keymap_index_63);
    RUN_TEST(test_keymap_consumer_volume_down);
    RUN_TEST(test_keymap_consumer_mute);
    RUN_TEST(test_keymap_consumer_play_pause);

    /* ==================== encoder 模块测试 ==================== */
    RUN_TEST(test_encoder_init_state);
    RUN_TEST(test_encoder_clockwise_full);
    RUN_TEST(test_encoder_counter_clockwise_full);
    RUN_TEST(test_encoder_bounce);
    RUN_TEST(test_encoder_steps_per_tick_4);
    RUN_TEST(test_encoder_direction_reverse);
    RUN_TEST(test_encoder_no_change);
    RUN_TEST(test_encoder_switch);

    /* ==================== scheduler 模块测试 ==================== */
    RUN_TEST(test_scheduler_init);
    RUN_TEST(test_scheduler_single_task);
    RUN_TEST(test_scheduler_multiple_tasks);
    RUN_TEST(test_scheduler_priority_order);
    RUN_TEST(test_scheduler_null_task);
    RUN_TEST(test_scheduler_zero_count);
    RUN_TEST(test_scheduler_overflow);
    RUN_TEST(test_scheduler_first_run);
    RUN_TEST(test_scheduler_updates_last_run);

    /* ==================== fault 模块测试 ==================== */
    RUN_TEST(test_fault_initial_count);
    RUN_TEST(test_fault_record_one);
    RUN_TEST(test_fault_record_multiple);
    RUN_TEST(test_fault_clear);
    RUN_TEST(test_fault_all_levels);

    /* ==================== config 模块测试 ==================== */
    RUN_TEST(test_config_init_empty_flash);
    RUN_TEST(test_config_save_and_load);
    RUN_TEST(test_config_save_seq_increments);
    RUN_TEST(test_config_alternate_sectors);
    RUN_TEST(test_config_one_sector_corrupted);
    RUN_TEST(test_config_both_sectors_corrupted);
    RUN_TEST(test_config_reset_default);
    RUN_TEST(test_config_get_default);
    RUN_TEST(test_config_save_null);
    RUN_TEST(test_config_power_failure_protection);
    RUN_TEST(test_config_size_fits_sector);
    RUN_TEST(test_config_default_keymap_not_empty);

    return UNITY_END();
}













