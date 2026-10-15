/*
 * tests/test_config_validator.c
 * 配置验证模块单元测试
 * 测试 config_validator.c 中的所有验证函数
 *
 * 覆盖范围：
 * - 魔数验证（空指针、正确、错误）
 * - CRC计算和验证（空指针、正确、错误）
 * - 完整验证（魔数、CRC、参数范围：DPI、死区、DPI档位、LOD等级）
 * - 配置合并（字段覆盖、magic/version强制设置、CRC重新计算、空指针）
 */

#include "unity.h"
#include "protocol/config_validator.h"
#include "board/config.h"
#include "middleware/crc32.h"
#include <string.h>
#include <stdint.h>

/* ==================== 辅助函数 ==================== */

/* 构造一个有效的配置 */
static device_config_t make_valid_config(void)
{
    device_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = CONFIG_MAGIC;
    cfg.version = CONFIG_VERSION;
    cfg.dpi = 1600;
    cfg.joystick_deadzone = 200;
    cfg.current_dpi_slot = 2;
    cfg.lod_level = 1;
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    return cfg;
}

/* ==================== 测试用例 ==================== */

/* 测试1：魔数验证 - 空指针返回false */
void test_config_validator_check_magic_null(void)
{
    TEST_ASSERT_FALSE(config_validator_check_magic(NULL));
}

/* 测试2：魔数验证 - 正确魔数返回true */
void test_config_validator_check_magic_valid(void)
{
    device_config_t cfg = make_valid_config();
    TEST_ASSERT_TRUE(config_validator_check_magic(&cfg));
}

/* 测试3：魔数验证 - 错误魔数返回false */
void test_config_validator_check_magic_invalid(void)
{
    device_config_t cfg = make_valid_config();
    cfg.magic = 0xDEADBEEF;
    TEST_ASSERT_FALSE(config_validator_check_magic(&cfg));
}

/* 测试4：CRC计算 - 空指针返回0 */
void test_config_validator_calc_crc_null(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, config_validator_calc_crc(NULL));
}

/* 测试5：CRC计算 - 有效配置返回非0 */
void test_config_validator_calc_crc_valid(void)
{
    device_config_t cfg = make_valid_config();
    uint32_t crc = config_validator_calc_crc(&cfg);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, crc);
}

/* 测试6：CRC计算 - 与直接计算结果一致 */
void test_config_validator_calc_crc_consistent(void)
{
    device_config_t cfg = make_valid_config();
    uint32_t crc1 = config_validator_calc_crc(&cfg);
    uint32_t crc2 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL_UINT32(crc2, crc1);
}

/* 测试7：CRC验证 - 空指针返回false */
void test_config_validator_check_crc_null(void)
{
    TEST_ASSERT_FALSE(config_validator_check_crc(NULL));
}

/* 测试8：CRC验证 - 正确CRC返回true */
void test_config_validator_check_crc_valid(void)
{
    device_config_t cfg = make_valid_config();
    TEST_ASSERT_TRUE(config_validator_check_crc(&cfg));
}

/* 测试9：CRC验证 - 错误CRC返回false */
void test_config_validator_check_crc_invalid(void)
{
    device_config_t cfg = make_valid_config();
    cfg.crc32 = 0xDEADBEEF;
    TEST_ASSERT_FALSE(config_validator_check_crc(&cfg));
}

/* 测试10：CRC验证 - 修改数据后CRC失效 */
void test_config_validator_check_crc_data_modified(void)
{
    device_config_t cfg = make_valid_config();
    cfg.dpi = 3200;  /* 修改数据但不更新CRC */
    TEST_ASSERT_FALSE(config_validator_check_crc(&cfg));
}

/* 测试11：完整验证 - 空指针返回参数错误 */
void test_config_validator_validate_null(void)
{
    TEST_ASSERT_EQUAL(CONFIG_HID_ERR_INVALID_PARAM, config_validator_validate(NULL));
}

/* 测试12：完整验证 - 有效配置返回OK */
void test_config_validator_validate_valid(void)
{
    device_config_t cfg = make_valid_config();
    TEST_ASSERT_EQUAL(CONFIG_HID_OK, config_validator_validate(&cfg));
}

/* 测试13：完整验证 - 错误魔数返回CRC错误 */
void test_config_validator_validate_bad_magic(void)
{
    device_config_t cfg = make_valid_config();
    cfg.magic = 0xDEADBEEF;
    TEST_ASSERT_EQUAL(CONFIG_HID_ERR_CRC_MISMATCH, config_validator_validate(&cfg));
}

/* 测试14：完整验证 - 错误CRC返回CRC错误 */
void test_config_validator_validate_bad_crc(void)
{
    device_config_t cfg = make_valid_config();
    cfg.crc32 = 0xDEADBEEF;
    TEST_ASSERT_EQUAL(CONFIG_HID_ERR_CRC_MISMATCH, config_validator_validate(&cfg));
}

/* 测试15：完整验证 - DPI过低返回参数错误 */
void test_config_validator_validate_dpi_too_low(void)
{
    device_config_t cfg = make_valid_config();
    cfg.dpi = 50;  /* 低于最小值100 */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL(CONFIG_HID_ERR_INVALID_PARAM, config_validator_validate(&cfg));
}

/* 测试16：完整验证 - DPI过高返回参数错误 */
void test_config_validator_validate_dpi_too_high(void)
{
    device_config_t cfg = make_valid_config();
    cfg.dpi = 10000;  /* 高于最大值6400 */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL(CONFIG_HID_ERR_INVALID_PARAM, config_validator_validate(&cfg));
}

/* 测试17：完整验证 - DPI边界值（最小值）通过 */
void test_config_validator_validate_dpi_min_boundary(void)
{
    device_config_t cfg = make_valid_config();
    cfg.dpi = 100;  /* 最小值 */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL(CONFIG_HID_OK, config_validator_validate(&cfg));
}

/* 测试18：完整验证 - DPI边界值（最大值）通过 */
void test_config_validator_validate_dpi_max_boundary(void)
{
    device_config_t cfg = make_valid_config();
    cfg.dpi = 6400;  /* 最大值 */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL(CONFIG_HID_OK, config_validator_validate(&cfg));
}

/* 测试19：完整验证 - 摇杆死区过高返回参数错误 */
void test_config_validator_validate_deadzone_too_high(void)
{
    device_config_t cfg = make_valid_config();
    cfg.joystick_deadzone = 5000;  /* 高于最大值4095 */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL(CONFIG_HID_ERR_INVALID_PARAM, config_validator_validate(&cfg));
}

/* 测试20：完整验证 - 摇杆死区边界值（最大值）通过 */
void test_config_validator_validate_deadzone_max_boundary(void)
{
    device_config_t cfg = make_valid_config();
    cfg.joystick_deadzone = 4095;  /* 最大值 */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL(CONFIG_HID_OK, config_validator_validate(&cfg));
}

/* 测试21：完整验证 - DPI档位过高返回参数错误 */
void test_config_validator_validate_dpi_slot_too_high(void)
{
    device_config_t cfg = make_valid_config();
    cfg.current_dpi_slot = 5;  /* 高于最大值3 */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL(CONFIG_HID_ERR_INVALID_PARAM, config_validator_validate(&cfg));
}

/* 测试22：完整验证 - DPI档位边界值（最大值）通过 */
void test_config_validator_validate_dpi_slot_max_boundary(void)
{
    device_config_t cfg = make_valid_config();
    cfg.current_dpi_slot = 3;  /* 最大值 */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL(CONFIG_HID_OK, config_validator_validate(&cfg));
}

/* 测试23：完整验证 - LOD等级过高返回参数错误 */
void test_config_validator_validate_lod_too_high(void)
{
    device_config_t cfg = make_valid_config();
    cfg.lod_level = 5;  /* 高于最大值3 */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL(CONFIG_HID_ERR_INVALID_PARAM, config_validator_validate(&cfg));
}

/* 测试24：完整验证 - LOD等级边界值（最大值）通过 */
void test_config_validator_validate_lod_max_boundary(void)
{
    device_config_t cfg = make_valid_config();
    cfg.lod_level = 3;  /* 最大值 */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t*)&cfg, sizeof(cfg) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL(CONFIG_HID_OK, config_validator_validate(&cfg));
}

/* 测试25：配置合并 - 空指针current不崩溃 */
void test_config_validator_merge_null_current(void)
{
    device_config_t new_cfg = make_valid_config();
    config_validator_merge_config(NULL, &new_cfg);
    /* 不崩溃即通过 */
    TEST_PASS();
}

/* 测试26：配置合并 - 空指针new_config不崩溃 */
void test_config_validator_merge_null_new(void)
{
    device_config_t current = make_valid_config();
    config_validator_merge_config(&current, NULL);
    /* 不崩溃即通过 */
    TEST_PASS();
}

/* 测试27：配置合并 - 两个都为空不崩溃 */
void test_config_validator_merge_both_null(void)
{
    config_validator_merge_config(NULL, NULL);
    /* 不崩溃即通过 */
    TEST_PASS();
}

/* 测试28：配置合并 - DPI字段被覆盖 */
void test_config_validator_merge_dpi_overwritten(void)
{
    device_config_t current = make_valid_config();
    current.dpi = 800;
    device_config_t new_cfg = make_valid_config();
    new_cfg.dpi = 3200;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT16(3200, current.dpi);
}

/* 测试29：配置合并 - magic被强制设置为正确值 */
void test_config_validator_merge_magic_forced(void)
{
    device_config_t current = make_valid_config();
    device_config_t new_cfg = make_valid_config();
    new_cfg.magic = 0xDEADBEEF;  /* 错误的magic */

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT32(CONFIG_MAGIC, current.magic);
}

/* 测试30：配置合并 - version被强制设置为正确值 */
void test_config_validator_merge_version_forced(void)
{
    device_config_t current = make_valid_config();
    device_config_t new_cfg = make_valid_config();
    new_cfg.version = 0x0001;  /* 旧版本 */

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, current.version);
}

/* 测试31：配置合并 - CRC被重新计算 */
void test_config_validator_merge_crc_recalculated(void)
{
    device_config_t current = make_valid_config();
    device_config_t new_cfg = make_valid_config();
    new_cfg.dpi = 3200;
    new_cfg.crc32 = 0xDEADBEEF;  /* 错误的CRC */

    config_validator_merge_config(&current, &new_cfg);

    /* 验证CRC被重新计算且正确 */
    uint32_t expected_crc = crc32_calc((const uint8_t*)&current, sizeof(current) - sizeof(uint32_t));
    TEST_ASSERT_EQUAL_UINT32(expected_crc, current.crc32);
}

/* 测试32：配置合并 - 合并后配置验证通过 */
void test_config_validator_merge_result_valid(void)
{
    device_config_t current = make_valid_config();
    device_config_t new_cfg = make_valid_config();
    new_cfg.dpi = 2400;
    new_cfg.joystick_deadzone = 300;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL(CONFIG_HID_OK, config_validator_validate(&current));
}

/* 测试33：配置合并 - v3新增字段被覆盖（摇杆反转X） */
void test_config_validator_merge_joystick_invert_x(void)
{
    device_config_t current = make_valid_config();
    current.joystick_invert_x = 0;
    device_config_t new_cfg = make_valid_config();
    new_cfg.joystick_invert_x = 1;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT8(1, current.joystick_invert_x);
}

/* 测试34：配置合并 - v3新增字段被覆盖（编码器步数） */
void test_config_validator_merge_encoder_steps(void)
{
    device_config_t current = make_valid_config();
    current.encoder_steps = 1;
    device_config_t new_cfg = make_valid_config();
    new_cfg.encoder_steps = 4;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT16(4, current.encoder_steps);
}

/* 测试35：配置合并 - 摇杆灵敏度被覆盖 */
void test_config_validator_merge_joystick_sensitivity(void)
{
    device_config_t current = make_valid_config();
    current.joystick_sensitivity = 100;
    device_config_t new_cfg = make_valid_config();
    new_cfg.joystick_sensitivity = 200;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT16(200, current.joystick_sensitivity);
}

/* 测试36：配置合并 - v6 四档 DPI 数组被覆盖（回归：此前漏合并导致保存后不生效） */
void test_config_validator_merge_dpi_slots(void)
{
    device_config_t current = make_valid_config();
    device_config_t new_cfg = make_valid_config();
    current.dpi_slot[0] = 400; current.dpi_slot[3] = 3200;
    new_cfg.dpi_slot[0] = 500; new_cfg.dpi_slot[1] = 900;
    new_cfg.dpi_slot[2] = 1700; new_cfg.dpi_slot[3] = 3300;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT16(500, current.dpi_slot[0]);
    TEST_ASSERT_EQUAL_UINT16(900, current.dpi_slot[1]);
    TEST_ASSERT_EQUAL_UINT16(1700, current.dpi_slot[2]);
    TEST_ASSERT_EQUAL_UINT16(3300, current.dpi_slot[3]);
}

/* 测试37：配置合并 - v6 光学标量字段（当前档/LOD/角度捕捉/切换键）被覆盖 */
void test_config_validator_merge_v6_scalars(void)
{
    device_config_t current = make_valid_config();
    device_config_t new_cfg = make_valid_config();
    new_cfg.current_dpi_slot = 3;
    new_cfg.lod_level = 2;
    new_cfg.angle_snapping = 1;
    new_cfg.dpi_switch_key = 0x42;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT8(3, current.current_dpi_slot);
    TEST_ASSERT_EQUAL_UINT8(2, current.lod_level);
    TEST_ASSERT_EQUAL_UINT8(1, current.angle_snapping);
    TEST_ASSERT_EQUAL_UINT8(0x42, current.dpi_switch_key);
}

/* 测试38：配置合并 - v7 Layer2 键映射被覆盖（回归：此前漏合并导致第3层丢失） */
void test_config_validator_merge_layer2_keymap(void)
{
    device_config_t current = make_valid_config();
    device_config_t new_cfg = make_valid_config();
    new_cfg.layer2_keymap[0] = 0x11;
    new_cfg.layer2_keymap[63] = 0x22;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT8(0x11, current.layer2_keymap[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22, current.layer2_keymap[63]);
}

/* 测试39：配置合并 - v7 Layer3 键映射被覆盖（回归：此前漏合并导致第4层丢失） */
void test_config_validator_merge_layer3_keymap(void)
{
    device_config_t current = make_valid_config();
    device_config_t new_cfg = make_valid_config();
    new_cfg.layer3_keymap[5] = 0x33;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT8(0x33, current.layer3_keymap[5]);
}

/* 测试40：配置合并 - v5 摇杆校准状态刻意保留 Flash 现值，不被下发零值清除 */
void test_config_validator_merge_keeps_calibration(void)
{
    device_config_t current = make_valid_config();
    device_config_t new_cfg = make_valid_config();
    current.joystick_calibrated = 1;
    current.joystick_center_x = 2010;
    current.joystick_center_y = 2090;
    new_cfg.joystick_calibrated = 0;  /* 上位机配置块不下发校准，保持零 */
    new_cfg.joystick_center_x = 0;
    new_cfg.joystick_center_y = 0;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT8(1, current.joystick_calibrated);
    TEST_ASSERT_EQUAL_UINT16(2010, current.joystick_center_x);
    TEST_ASSERT_EQUAL_UINT16(2090, current.joystick_center_y);
}

/* 测试41：配置合并 - v8 性能阈值刻意保留 Flash 现值，不被下发零值清除 */
void test_config_validator_merge_keeps_perf_thresholds(void)
{
    device_config_t current = make_valid_config();
    device_config_t new_cfg = make_valid_config();
    current.perf_thresholds[0] = 100;
    current.perf_thresholds[7] = 250;
    new_cfg.perf_thresholds[0] = 0;
    new_cfg.perf_thresholds[7] = 0;

    config_validator_merge_config(&current, &new_cfg);
    TEST_ASSERT_EQUAL_UINT16(100, current.perf_thresholds[0]);
    TEST_ASSERT_EQUAL_UINT16(250, current.perf_thresholds[7]);
}
