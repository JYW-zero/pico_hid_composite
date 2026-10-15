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
void test_debounce_set_get_threshold(void);
void test_debounce_all_keys_press_release(void);
void test_debounce_get_does_not_change_state(void);
void test_debounce_threshold_change_runtime(void);

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
void test_config_migrate_v1_to_latest(void);
void test_config_migrate_v3_to_latest(void);
void test_config_migrate_v5_to_latest(void);
void test_config_invalid_version_handled(void);

/* ==================== crc32 测试函数声明 ==================== */
void test_crc32_empty_data(void);
void test_crc32_single_byte(void);
void test_crc32_known_string(void);
void test_crc32_hello_world(void);
void test_crc32_consistency(void);
void test_crc32_different_data(void);
void test_crc32_single_bit_change(void);
void test_crc32_large_data(void);
void test_crc32_nibble_matches_full(void);
void test_crc32_nibble_known_string(void);
void test_crc32_table_init(void);
void test_crc32_incremental(void);
void test_crc32_all_zeros(void);
void test_crc32_all_ones(void);

/* ==================== macro 测试函数声明 ==================== */
void test_macro_init(void);
void test_macro_set_and_get(void);
void test_macro_set_invalid_id(void);
void test_macro_get_invalid_id(void);
void test_macro_set_null(void);
void test_macro_trigger_and_stop(void);
void test_macro_trigger_invalid_id(void);
void test_macro_stop_all(void);
void test_macro_find_by_trigger_key(void);
void test_macro_find_by_trigger_key_not_found(void);
void test_macro_max_actions(void);
void test_macro_name_max_len(void);
void test_macro_keyboard_state(void);
void test_macro_mouse_buttons(void);
void test_macro_kb_changed(void);
void test_macro_mouse_changed(void);
void test_macro_save_and_load_config(void);
void test_macro_load_null_data(void);
void test_macro_save_null_buffer(void);
void test_macro_load_zero_length(void);
void test_macro_action_types(void);
void test_macro_repeat_count(void);
void test_macro_infinite_repeat(void);

/* ==================== config_validator 测试函数声明 ==================== */
void test_config_validator_check_magic_null(void);
void test_config_validator_check_magic_valid(void);
void test_config_validator_check_magic_invalid(void);
void test_config_validator_calc_crc_null(void);
void test_config_validator_calc_crc_valid(void);
void test_config_validator_calc_crc_consistent(void);
void test_config_validator_check_crc_null(void);
void test_config_validator_check_crc_valid(void);
void test_config_validator_check_crc_invalid(void);
void test_config_validator_check_crc_data_modified(void);
void test_config_validator_validate_null(void);
void test_config_validator_validate_valid(void);
void test_config_validator_validate_bad_magic(void);
void test_config_validator_validate_bad_crc(void);
void test_config_validator_validate_dpi_too_low(void);
void test_config_validator_validate_dpi_too_high(void);
void test_config_validator_validate_dpi_min_boundary(void);
void test_config_validator_validate_dpi_max_boundary(void);
void test_config_validator_validate_deadzone_too_high(void);
void test_config_validator_validate_deadzone_max_boundary(void);
void test_config_validator_validate_dpi_slot_too_high(void);
void test_config_validator_validate_dpi_slot_max_boundary(void);
void test_config_validator_validate_lod_too_high(void);
void test_config_validator_validate_lod_max_boundary(void);
void test_config_validator_merge_null_current(void);
void test_config_validator_merge_null_new(void);
void test_config_validator_merge_both_null(void);
void test_config_validator_merge_dpi_overwritten(void);
void test_config_validator_merge_magic_forced(void);
void test_config_validator_merge_version_forced(void);
void test_config_validator_merge_crc_recalculated(void);
void test_config_validator_merge_result_valid(void);
void test_config_validator_merge_joystick_invert_x(void);
void test_config_validator_merge_encoder_steps(void);
void test_config_validator_merge_joystick_sensitivity(void);
void test_config_validator_merge_dpi_slots(void);
void test_config_validator_merge_v6_scalars(void);
void test_config_validator_merge_layer2_keymap(void);
void test_config_validator_merge_layer3_keymap(void);
void test_config_validator_merge_keeps_calibration(void);
void test_config_validator_merge_keeps_perf_thresholds(void);

/* ==================== config_block_receiver 测试函数声明 ==================== */
void test_config_block_receiver_init_zero(void);
void test_config_block_receiver_reset_zero(void);
void test_config_block_receiver_mark_block0(void);
void test_config_block_receiver_mark_block1(void);
void test_config_block_receiver_mark_block2(void);
void test_config_block_receiver_mark_multiple(void);
void test_config_block_receiver_mark_duplicate(void);
void test_config_block_receiver_is_complete_all(void);
void test_config_block_receiver_is_complete_partial(void);
void test_config_block_receiver_is_complete_none(void);
void test_config_block_receiver_get_status(void);
void test_config_block_receiver_get_block_info_block0(void);
void test_config_block_receiver_get_block_info_block1(void);
void test_config_block_receiver_get_block_info_block2(void);
void test_config_block_receiver_get_block_info_invalid(void);
void test_config_block_receiver_full_flow(void);
void test_config_block_receiver_out_of_order(void);
void test_config_block_receiver_constant_all(void);
void test_config_block_receiver_constant_block_size(void);
void test_config_block_receiver_mark_zero(void);

/* ==================== config_hid_helpers 测试函数声明 ==================== */
void test_config_hid_dpi_400(void);
void test_config_hid_dpi_800(void);
void test_config_hid_dpi_1600(void);
void test_config_hid_dpi_3200(void);
void test_config_hid_dpi_0_default(void);
void test_config_hid_dpi_100_default(void);
void test_config_hid_dpi_6400_default(void);
void test_config_hid_dpi_ffff_default(void);
void test_config_hid_dpi_399_default(void);
void test_config_hid_dpi_401_default(void);
void test_config_hid_dpi_799_default(void);
void test_config_hid_dpi_801_default(void);
void test_config_hid_dpi_1599_default(void);
void test_config_hid_dpi_1601_default(void);
void test_config_hid_dpi_3199_default(void);
void test_config_hid_dpi_3201_default(void);

/* ==================== config_lock 测试函数声明 ==================== */
void test_config_lock_init_locked(void);
void test_config_lock_attempt_1(void);
void test_config_lock_attempt_2(void);
void test_config_lock_attempt_3_unlocked(void);
void test_config_lock_window_timeout_reset(void);
void test_config_lock_window_within_success(void);
void test_config_lock_auto_relock_timeout(void);
void test_config_lock_auto_relock_not_yet(void);
void test_config_lock_auto_relock_write_resets(void);
void test_config_lock_auto_relock_already_locked(void);
void test_config_lock_notify_write_no_state_change(void);
void test_config_lock_cmd_get_version_allowed(void);
void test_config_lock_cmd_enter_dfu_allowed(void);
void test_config_lock_cmd_reboot_allowed(void);
void test_config_lock_cmd_switch_dpi_allowed(void);
void test_config_lock_cmd_save_config_not_allowed(void);
void test_config_lock_cmd_unknown_not_allowed(void);
void test_config_lock_unlock_allows_sensitive_cmds(void);

/* ==================== command_confirm 测试函数声明 ==================== */
void test_command_confirm_reboot_init_not_confirmed(void);
void test_command_confirm_reboot_attempt_1(void);
void test_command_confirm_reboot_attempt_2(void);
void test_command_confirm_reboot_attempt_3_confirmed(void);
void test_command_confirm_reboot_window_timeout_reset(void);
void test_command_confirm_reboot_window_within_success(void);
void test_command_confirm_dfu_init_not_confirmed(void);
void test_command_confirm_dfu_attempt_1(void);
void test_command_confirm_dfu_attempt_3_confirmed(void);
void test_command_confirm_dfu_window_timeout_reset(void);
void test_command_confirm_bootsel_init_not_confirmed(void);
void test_command_confirm_bootsel_attempt_1(void);
void test_command_confirm_bootsel_attempt_3_confirmed(void);
void test_command_confirm_bootsel_window_timeout_reset(void);
void test_command_confirm_reset_reboot(void);
void test_command_confirm_reset_dfu(void);
void test_command_confirm_reset_bootsel(void);
void test_command_confirm_independent(void);

/* ==================== capability 测试函数声明 ==================== */
void test_capability_count_extended_to_26(void);
void test_capability_bitmap_contains_consumer_base(void);
void test_capability_bitmap_contains_diag_bits(void);
void test_capability_bitmap_contains_mfg_bits(void);
void test_capability_device_flags_zero_in_engineering(void);
void test_capability_reserved_holes_clear(void);
void test_capability_dfu_follows_bootloader_flag(void);

/* ==================== command_gate 测试函数声明 ==================== */
void test_gate_consumer_commands_allowed(void);
void test_gate_dfu_and_bootsel_allowed_when_bootloader_compiled(void);
void test_gate_get_capability_info_is_public(void);
void test_gate_diag_commands_allowed_when_compiled_in(void);
void test_gate_clear_fault_allowed_engineering(void);
void test_gate_unknown_command_passes_through(void);
void test_gate_report_cap_keystats(void);
void test_gate_report_cap_perf(void);
void test_gate_report_cap_raw_state(void);
void test_gate_report_cap_fault(void);
void test_gate_report_cap_public_reports_zero(void);

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
    RUN_TEST(test_debounce_set_get_threshold);
    RUN_TEST(test_debounce_all_keys_press_release);
    RUN_TEST(test_debounce_get_does_not_change_state);
    RUN_TEST(test_debounce_threshold_change_runtime);

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
    RUN_TEST(test_config_migrate_v1_to_latest);
    RUN_TEST(test_config_migrate_v3_to_latest);
    RUN_TEST(test_config_migrate_v5_to_latest);
    RUN_TEST(test_config_invalid_version_handled);

    /* ==================== crc32 模块测试 ==================== */
    RUN_TEST(test_crc32_empty_data);
    RUN_TEST(test_crc32_single_byte);
    RUN_TEST(test_crc32_known_string);
    RUN_TEST(test_crc32_hello_world);
    RUN_TEST(test_crc32_consistency);
    RUN_TEST(test_crc32_different_data);
    RUN_TEST(test_crc32_single_bit_change);
    RUN_TEST(test_crc32_large_data);
    RUN_TEST(test_crc32_nibble_matches_full);
    RUN_TEST(test_crc32_nibble_known_string);
    RUN_TEST(test_crc32_table_init);
    RUN_TEST(test_crc32_incremental);
    RUN_TEST(test_crc32_all_zeros);
    RUN_TEST(test_crc32_all_ones);

    /* ==================== macro 模块测试 ==================== */
    RUN_TEST(test_macro_init);
    RUN_TEST(test_macro_set_and_get);
    RUN_TEST(test_macro_set_invalid_id);
    RUN_TEST(test_macro_get_invalid_id);
    RUN_TEST(test_macro_set_null);
    RUN_TEST(test_macro_trigger_and_stop);
    RUN_TEST(test_macro_trigger_invalid_id);
    RUN_TEST(test_macro_stop_all);
    RUN_TEST(test_macro_find_by_trigger_key);
    RUN_TEST(test_macro_find_by_trigger_key_not_found);
    RUN_TEST(test_macro_max_actions);
    RUN_TEST(test_macro_name_max_len);
    RUN_TEST(test_macro_keyboard_state);
    RUN_TEST(test_macro_mouse_buttons);
    RUN_TEST(test_macro_kb_changed);
    RUN_TEST(test_macro_mouse_changed);
    RUN_TEST(test_macro_save_and_load_config);
    RUN_TEST(test_macro_load_null_data);
    RUN_TEST(test_macro_save_null_buffer);
    RUN_TEST(test_macro_load_zero_length);
    RUN_TEST(test_macro_action_types);
    RUN_TEST(test_macro_repeat_count);
    RUN_TEST(test_macro_infinite_repeat);

    /* ==================== config_validator 测试 ==================== */
    RUN_TEST(test_config_validator_check_magic_null);
    RUN_TEST(test_config_validator_check_magic_valid);
    RUN_TEST(test_config_validator_check_magic_invalid);
    RUN_TEST(test_config_validator_calc_crc_null);
    RUN_TEST(test_config_validator_calc_crc_valid);
    RUN_TEST(test_config_validator_calc_crc_consistent);
    RUN_TEST(test_config_validator_check_crc_null);
    RUN_TEST(test_config_validator_check_crc_valid);
    RUN_TEST(test_config_validator_check_crc_invalid);
    RUN_TEST(test_config_validator_check_crc_data_modified);
    RUN_TEST(test_config_validator_validate_null);
    RUN_TEST(test_config_validator_validate_valid);
    RUN_TEST(test_config_validator_validate_bad_magic);
    RUN_TEST(test_config_validator_validate_bad_crc);
    RUN_TEST(test_config_validator_validate_dpi_too_low);
    RUN_TEST(test_config_validator_validate_dpi_too_high);
    RUN_TEST(test_config_validator_validate_dpi_min_boundary);
    RUN_TEST(test_config_validator_validate_dpi_max_boundary);
    RUN_TEST(test_config_validator_validate_deadzone_too_high);
    RUN_TEST(test_config_validator_validate_deadzone_max_boundary);
    RUN_TEST(test_config_validator_validate_dpi_slot_too_high);
    RUN_TEST(test_config_validator_validate_dpi_slot_max_boundary);
    RUN_TEST(test_config_validator_validate_lod_too_high);
    RUN_TEST(test_config_validator_validate_lod_max_boundary);
    RUN_TEST(test_config_validator_merge_null_current);
    RUN_TEST(test_config_validator_merge_null_new);
    RUN_TEST(test_config_validator_merge_both_null);
    RUN_TEST(test_config_validator_merge_dpi_overwritten);
    RUN_TEST(test_config_validator_merge_magic_forced);
    RUN_TEST(test_config_validator_merge_version_forced);
    RUN_TEST(test_config_validator_merge_crc_recalculated);
    RUN_TEST(test_config_validator_merge_result_valid);
    RUN_TEST(test_config_validator_merge_joystick_invert_x);
    RUN_TEST(test_config_validator_merge_encoder_steps);
    RUN_TEST(test_config_validator_merge_joystick_sensitivity);
    RUN_TEST(test_config_validator_merge_dpi_slots);
    RUN_TEST(test_config_validator_merge_v6_scalars);
    RUN_TEST(test_config_validator_merge_layer2_keymap);
    RUN_TEST(test_config_validator_merge_layer3_keymap);
    RUN_TEST(test_config_validator_merge_keeps_calibration);
    RUN_TEST(test_config_validator_merge_keeps_perf_thresholds);

    /* ==================== config_block_receiver 测试 ==================== */
    RUN_TEST(test_config_block_receiver_init_zero);
    RUN_TEST(test_config_block_receiver_reset_zero);
    RUN_TEST(test_config_block_receiver_mark_block0);
    RUN_TEST(test_config_block_receiver_mark_block1);
    RUN_TEST(test_config_block_receiver_mark_block2);
    RUN_TEST(test_config_block_receiver_mark_multiple);
    RUN_TEST(test_config_block_receiver_mark_duplicate);
    RUN_TEST(test_config_block_receiver_is_complete_all);
    RUN_TEST(test_config_block_receiver_is_complete_partial);
    RUN_TEST(test_config_block_receiver_is_complete_none);
    RUN_TEST(test_config_block_receiver_get_status);
    RUN_TEST(test_config_block_receiver_get_block_info_block0);
    RUN_TEST(test_config_block_receiver_get_block_info_block1);
    RUN_TEST(test_config_block_receiver_get_block_info_block2);
    RUN_TEST(test_config_block_receiver_get_block_info_invalid);
    RUN_TEST(test_config_block_receiver_full_flow);
    RUN_TEST(test_config_block_receiver_out_of_order);
    RUN_TEST(test_config_block_receiver_constant_all);
    RUN_TEST(test_config_block_receiver_constant_block_size);
    RUN_TEST(test_config_block_receiver_mark_zero);

    /* ==================== config_hid_helpers 测试 ==================== */
    RUN_TEST(test_config_hid_dpi_400);
    RUN_TEST(test_config_hid_dpi_800);
    RUN_TEST(test_config_hid_dpi_1600);
    RUN_TEST(test_config_hid_dpi_3200);
    RUN_TEST(test_config_hid_dpi_0_default);
    RUN_TEST(test_config_hid_dpi_100_default);
    RUN_TEST(test_config_hid_dpi_6400_default);
    RUN_TEST(test_config_hid_dpi_ffff_default);
    RUN_TEST(test_config_hid_dpi_399_default);
    RUN_TEST(test_config_hid_dpi_401_default);
    RUN_TEST(test_config_hid_dpi_799_default);
    RUN_TEST(test_config_hid_dpi_801_default);
    RUN_TEST(test_config_hid_dpi_1599_default);
    RUN_TEST(test_config_hid_dpi_1601_default);
    RUN_TEST(test_config_hid_dpi_3199_default);
    RUN_TEST(test_config_hid_dpi_3201_default);

    /* ==================== config_lock 测试 ==================== */
    RUN_TEST(test_config_lock_init_locked);
    RUN_TEST(test_config_lock_attempt_1);
    RUN_TEST(test_config_lock_attempt_2);
    RUN_TEST(test_config_lock_attempt_3_unlocked);
    RUN_TEST(test_config_lock_window_timeout_reset);
    RUN_TEST(test_config_lock_window_within_success);
    RUN_TEST(test_config_lock_auto_relock_timeout);
    RUN_TEST(test_config_lock_auto_relock_not_yet);
    RUN_TEST(test_config_lock_auto_relock_write_resets);
    RUN_TEST(test_config_lock_auto_relock_already_locked);
    RUN_TEST(test_config_lock_notify_write_no_state_change);
    RUN_TEST(test_config_lock_cmd_get_version_allowed);
    RUN_TEST(test_config_lock_cmd_enter_dfu_allowed);
    RUN_TEST(test_config_lock_cmd_reboot_allowed);
    RUN_TEST(test_config_lock_cmd_switch_dpi_allowed);
    RUN_TEST(test_config_lock_cmd_save_config_not_allowed);
    RUN_TEST(test_config_lock_cmd_unknown_not_allowed);
    RUN_TEST(test_config_lock_unlock_allows_sensitive_cmds);

    /* ==================== command_confirm 测试 ==================== */
    RUN_TEST(test_command_confirm_reboot_init_not_confirmed);
    RUN_TEST(test_command_confirm_reboot_attempt_1);
    RUN_TEST(test_command_confirm_reboot_attempt_2);
    RUN_TEST(test_command_confirm_reboot_attempt_3_confirmed);
    RUN_TEST(test_command_confirm_reboot_window_timeout_reset);
    RUN_TEST(test_command_confirm_reboot_window_within_success);
    RUN_TEST(test_command_confirm_dfu_init_not_confirmed);
    RUN_TEST(test_command_confirm_dfu_attempt_1);
    RUN_TEST(test_command_confirm_dfu_attempt_3_confirmed);
    RUN_TEST(test_command_confirm_dfu_window_timeout_reset);
    RUN_TEST(test_command_confirm_bootsel_init_not_confirmed);
    RUN_TEST(test_command_confirm_bootsel_attempt_1);
    RUN_TEST(test_command_confirm_bootsel_attempt_3_confirmed);
    RUN_TEST(test_command_confirm_bootsel_window_timeout_reset);
    RUN_TEST(test_command_confirm_reset_reboot);
    RUN_TEST(test_command_confirm_reset_dfu);
    RUN_TEST(test_command_confirm_reset_bootsel);
    RUN_TEST(test_command_confirm_independent);

    /* ==================== capability 测试 ==================== */
    RUN_TEST(test_capability_count_extended_to_26);
    RUN_TEST(test_capability_bitmap_contains_consumer_base);
    RUN_TEST(test_capability_bitmap_contains_diag_bits);
    RUN_TEST(test_capability_bitmap_contains_mfg_bits);
    RUN_TEST(test_capability_device_flags_zero_in_engineering);
    RUN_TEST(test_capability_reserved_holes_clear);
    RUN_TEST(test_capability_dfu_follows_bootloader_flag);

    /* ==================== command_gate 测试 ==================== */
    RUN_TEST(test_gate_consumer_commands_allowed);
    RUN_TEST(test_gate_dfu_and_bootsel_allowed_when_bootloader_compiled);
    RUN_TEST(test_gate_get_capability_info_is_public);
    RUN_TEST(test_gate_diag_commands_allowed_when_compiled_in);
    RUN_TEST(test_gate_clear_fault_allowed_engineering);
    RUN_TEST(test_gate_unknown_command_passes_through);
    RUN_TEST(test_gate_report_cap_keystats);
    RUN_TEST(test_gate_report_cap_perf);
    RUN_TEST(test_gate_report_cap_raw_state);
    RUN_TEST(test_gate_report_cap_fault);
    RUN_TEST(test_gate_report_cap_public_reports_zero);

    return UNITY_END();
}













