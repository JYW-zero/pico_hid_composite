/*
 * tests/test_config_hid_helpers.c
 * 配置 HID 辅助函数模块单元测试
 * 测试 config_hid_helpers.c 中的纯函数
 *
 * 覆盖范围：
 * - config_hid_dpi_val_to_enum：DPI数值转枚举
 *   - 4个有效值（400/800/1600/3200）
 *   - 8个无效值（0/100/6400/0xFFFF等）
 *   - 8个边界值（399/401/799/801/1599/1601/3199/3201）
 *
 * 注意：config_hid_send_ack 依赖 TinyUSB，需要复杂的 mock，
 *       本测试文件暂不覆盖该函数，后续可集成到硬件在环测试中。
 */

#include "unity.h"
#include "protocol/config_hid_helpers.h"
#include "device/optical_sensor.h"
#include <stdint.h>

/* ==================== 测试用例 ==================== */

/* 测试1：DPI=400 返回 optical_sensor_DPI_400 */
void test_config_hid_dpi_400(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_400, config_hid_dpi_val_to_enum(400));
}

/* 测试2：DPI=800 返回 optical_sensor_DPI_800 */
void test_config_hid_dpi_800(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_800, config_hid_dpi_val_to_enum(800));
}

/* 测试3：DPI=1600 返回 optical_sensor_DPI_1600 */
void test_config_hid_dpi_1600(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(1600));
}

/* 测试4：DPI=3200 返回 optical_sensor_DPI_3200 */
void test_config_hid_dpi_3200(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_3200, config_hid_dpi_val_to_enum(3200));
}

/* 测试5：DPI=0（无效）返回默认 optical_sensor_DPI_1600 */
void test_config_hid_dpi_0_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(0));
}

/* 测试6：DPI=100（无效，低于最小值）返回默认 optical_sensor_DPI_1600 */
void test_config_hid_dpi_100_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(100));
}

/* 测试7：DPI=6400（无效，高于最大值）返回默认 optical_sensor_DPI_1600 */
void test_config_hid_dpi_6400_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(6400));
}

/* 测试8：DPI=0xFFFF（无效，最大值）返回默认 optical_sensor_DPI_1600 */
void test_config_hid_dpi_ffff_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(0xFFFF));
}

/* 测试9：边界值 DPI=399（无效，低于400）返回默认 */
void test_config_hid_dpi_399_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(399));
}

/* 测试10：边界值 DPI=401（无效，高于400但不是标准值）返回默认 */
void test_config_hid_dpi_401_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(401));
}

/* 测试11：边界值 DPI=799（无效，低于800）返回默认 */
void test_config_hid_dpi_799_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(799));
}

/* 测试12：边界值 DPI=801（无效，高于800但不是标准值）返回默认 */
void test_config_hid_dpi_801_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(801));
}

/* 测试13：边界值 DPI=1599（无效，低于1600）返回默认 */
void test_config_hid_dpi_1599_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(1599));
}

/* 测试14：边界值 DPI=1601（无效，高于1600但不是标准值）返回默认 */
void test_config_hid_dpi_1601_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(1601));
}

/* 测试15：边界值 DPI=3199（无效，低于3200）返回默认 */
void test_config_hid_dpi_3199_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(3199));
}

/* 测试16：边界值 DPI=3201（无效，高于3200）返回默认 */
void test_config_hid_dpi_3201_default(void)
{
    TEST_ASSERT_EQUAL_INT(optical_sensor_DPI_1600, config_hid_dpi_val_to_enum(3201));
}
