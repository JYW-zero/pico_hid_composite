/*
 * src/app/factory_test.c
 * 工厂测试模式实现
 * 量产测试：自动检测所有硬件
 */

#include "app/factory_test.h"
#include "board/board.h"
#include "board/config.h"
#include "board/flash_layout.h"
#include "middleware/fault.h"
#include "device/keypad_spi.h"
#include "device/optical_sensor.h"
#include "device/joystick.h"
#include "device/encoder.h"
#include "pico/time.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>

/* ==================== 静态变量 ==================== */

static factory_test_result_t s_result;

/* ==================== 内部函数：各个测试项 ==================== */

/* 测试1：SPI键盘测试 */
static factory_test_status_t test_spi_keypad(char* detail, uint32_t* duration_ms)
{
    uint32_t start = to_ms_since_boot(get_absolute_time());
    const keypad_spi_cfg_t* cfg = board_get_keypad_spi_cfg();

    /* 读取所有按键状态 */
    uint64_t keys = 0;
    int ret = keypad_spi_read_u64(cfg, &keys);
    if (ret != 0)
    {
        sprintf(detail, "SPI键盘: 读取失败 (%d)", ret);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    /* 检查是否有短路（所有按键都按下 = 全0，低电平有效）
     * 正常情况下应该是所有按键都松开 = 全1
     */
    int pressed_count = 0;
    for (int i = 0; i < 64; i++)
    {
        if (((keys >> i) & 1ULL) == 0ULL)
        {
            pressed_count++;
        }
    }

    if (pressed_count >= 60)
    {
        /* 几乎所有键都按下，可能是SPI通信故障或者短路 */
        sprintf(detail, "SPI键盘: 疑似短路，按下%d键", pressed_count);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    sprintf(detail, "SPI键盘: 通信正常，%d键按下", pressed_count);
    *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
    return FACTORY_TEST_PASS;
}

/* 测试2：OPTICAL_SENSOR传感器测试 */
static factory_test_status_t test_optical_sensor(char* detail, uint32_t* duration_ms)
{
    uint32_t start = to_ms_since_boot(get_absolute_time());
    const optical_sensor_cfg_t* cfg = board_get_optical_sensor_cfg();

    /* 初始化OPTICAL_SENSOR */
    int ret = optical_sensor_init(cfg);
    if (ret != 0)
    {
        sprintf(detail, "OPTICAL_SENSOR: 初始化失败 (%d)", ret);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    /* 读取产品ID */
    uint8_t pid = 0, rid = 0;
    ret = optical_sensor_reg_read(cfg, 0x00, &pid);
    if (ret != 0)
    {
        sprintf(detail, "OPTICAL_SENSOR: 读取产品ID失败");
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }
    optical_sensor_reg_read(cfg, 0x01, &rid);

    /* 检查产品ID是否合理（不是0x00也不是0xFF） */
    if (pid == 0x00 || pid == 0xFF)
    {
        sprintf(detail, "OPTICAL_SENSOR: 产品ID异常 (0x%02X)", pid);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    sprintf(detail, "OPTICAL_SENSOR: 产品ID=0x%02X 修订ID=0x%02X", pid, rid);
    *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
    return FACTORY_TEST_PASS;
}

/* 测试3：摇杆ADC测试 */
static factory_test_status_t test_joystick_adc(char* detail, uint32_t* duration_ms)
{
    uint32_t start = to_ms_since_boot(get_absolute_time());
    const joystick_cfg_t* cfg = board_get_joystick_cfg();

    /* 初始化摇杆 */
    int ret = joystick_init(cfg);
    if (ret != 0)
    {
        sprintf(detail, "摇杆ADC: 初始化失败 (%d)", ret);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    /* 读取摇杆数据 */
    joystick_data_t data;
    ret = joystick_read(cfg, &data);
    if (ret != 0)
    {
        sprintf(detail, "摇杆ADC: 读取失败");
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    /* 检查ADC值是否在合理范围内（0-4095） */
    if (data.x > 4095 || data.y > 4095)
    {
        sprintf(detail, "摇杆ADC: 值超出范围 X=%d Y=%d", data.x, data.y);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    /* 检查中间值是否合理（应该在2048左右，允许一定偏差） */
    bool x_ok = (data.x > 500) && (data.x < 3500);
    bool y_ok = (data.y > 500) && (data.y < 3500);
    if (!x_ok || !y_ok)
    {
        sprintf(detail, "摇杆ADC: 中值异常 X=%d Y=%d", data.x, data.y);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    sprintf(detail, "摇杆ADC: X=%d Y=%d 按键=%s",
            data.x, data.y, data.btn_pressed ? "按下" : "松开");
    *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
    return FACTORY_TEST_PASS;
}

/* 测试4：编码器测试 */
static factory_test_status_t test_encoder(char* detail, uint32_t* duration_ms)
{
    uint32_t start = to_ms_since_boot(get_absolute_time());
    const encoder_cfg_t* cfg = board_get_encoder_cfg();

    /* 初始化编码器 */
    int ret = encoder_init(cfg);
    if (ret != 0)
    {
        sprintf(detail, "编码器: 初始化失败 (%d)", ret);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    /* 读取GPIO状态 */
    bool a_state = gpio_get(cfg->a_pin);
    bool b_state = gpio_get(cfg->b_pin);
    bool sw_state = gpio_get(cfg->sw_pin);

    /* 简单检查：A/B相不应该同时是低电平（除非按下了），
     * 这里只检查GPIO读取功能正常
     */
    sprintf(detail, "编码器: A=%d B=%d SW=%d",
            a_state ? 1 : 0, b_state ? 1 : 0, sw_state ? 1 : 0);

    *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
    return FACTORY_TEST_PASS;
}

/* 测试5：Flash读写测试 */
static factory_test_status_t test_flash(char* detail, uint32_t* duration_ms)
{
    uint32_t start = to_ms_since_boot(get_absolute_time());

    /* 确保Flash布局已初始化（检测Flash大小） */
    flash_layout_init();

    /* 获取Flash大小 */
    uint32_t flash_size = flash_layout_get_total_size();
    const char* size_str = flash_layout_get_size_string();

    /* 检查Flash大小是否合理（至少256KB） */
    if (flash_size < 256 * 1024)
    {
        sprintf(detail, "Flash: 大小异常 (%s)", size_str);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    /* 检查Flash布局是否有效 */
    if (!flash_layout_is_valid())
    {
        sprintf(detail, "Flash: 布局无效，大小%s不足", size_str);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }

    /* 测试Flash：读取配置魔数，验证Flash可读 */
    const device_config_t* cfg = config_get();
    bool config_valid = (cfg != NULL && cfg->magic == CONFIG_MAGIC);

    /* Flash写入-回读测试：使用按键统计区作为临时测试区域
     * 工厂测试阶段该区域尚未使用，测试后擦除恢复
     */
    uint32_t test_offset = flash_layout_get_offset(FLASH_REGION_KEY_STATS);
    uint32_t test_addr = flash_layout_get_xip_addr(FLASH_REGION_KEY_STATS);
    bool write_test_pass = false;

    if (test_offset != 0xFFFFFFFF && test_addr != 0xFFFFFFFF)
    {
        /* 擦除测试扇区 */
        flash_range_erase(test_offset, FLASH_LAYOUT_SECTOR_SIZE);

        /* 写入测试数据（256字节，交替模式） */
        uint8_t test_data[256];
        for (int i = 0; i < 256; i++)
        {
            test_data[i] = (uint8_t)(i ^ 0xA5);
        }
        flash_range_program(test_offset, test_data, 256);

        /* 回读验证 */
        const uint8_t* read_ptr = (const uint8_t*)test_addr;
        write_test_pass = true;
        for (int i = 0; i < 256; i++)
        {
            if (read_ptr[i] != test_data[i])
            {
                write_test_pass = false;
                break;
            }
        }

        /* 擦除恢复（写入全0xFF） */
        flash_range_erase(test_offset, FLASH_LAYOUT_SECTOR_SIZE);
    }

    if (write_test_pass)
    {
        if (config_valid)
        {
            sprintf(detail, "Flash: %s, 读写测试通过, 配置魔数正确", size_str);
        }
        else
        {
            sprintf(detail, "Flash: %s, 读写测试通过, 无有效配置（首次启动？）", size_str);
        }
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_PASS;
    }
    else
    {
        sprintf(detail, "Flash: %s, 写入-回读测试失败", size_str);
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_FAIL;
    }
}

/* 测试6：LED测试 */
static factory_test_status_t test_led(char* detail, uint32_t* duration_ms)
{
    uint32_t start = to_ms_since_boot(get_absolute_time());

    /* 点亮LED 500ms，熄灭 500ms，重复 3 次供目视检查 */
    for (int i = 0; i < 3; i++)
    {
        board_led_write(true);
        sleep_ms(500);
        board_led_write(false);
        sleep_ms(500);
    }

    strcpy(detail, "LED: 闪烁 3 次完成，请目视确认");

    *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
    return FACTORY_TEST_PASS;  /* 需要人工确认 */
}

/* 测试7：USB连接测试 */
static factory_test_status_t test_usb(char* detail, uint32_t* duration_ms)
{
    uint32_t start = to_ms_since_boot(get_absolute_time());

    /* 检查USB挂载状态 */
    bool mounted = tud_mounted();
    bool suspended = tud_suspended();

    if (mounted)
    {
        if (suspended)
        {
            strcpy(detail, "USB: 已挂载（挂起状态）");
        }
        else
        {
            strcpy(detail, "USB: 已挂载（正常工作）");
        }
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        return FACTORY_TEST_PASS;
    }
    else
    {
        strcpy(detail, "USB: 未挂载");
        *duration_ms = to_ms_since_boot(get_absolute_time()) - start;
        /* USB未挂载不一定是故障，可能是没插电脑，所以返回警告？
         * 这里先返回FAIL，工厂测试时应该插着USB
         */
        return FACTORY_TEST_FAIL;
    }
}

/* ==================== 对外接口 ==================== */

void factory_test_init(void)
{
    memset(&s_result, 0, sizeof(s_result));
    for (int i = 0; i < FACTORY_TEST_COUNT; i++)
    {
        s_result.items[i].result = FACTORY_TEST_NOT_RUN;
        s_result.items[i].duration_ms = 0;
        s_result.items[i].detail[0] = '\0';
    }
}

factory_test_status_t factory_test_run_item(factory_test_item_t item)
{
    if (item >= FACTORY_TEST_COUNT)
    {
        return FACTORY_TEST_FAIL;
    }

    factory_test_status_t result = FACTORY_TEST_FAIL;
    uint32_t duration_ms = 0;
    char detail[64] = {0};

    switch (item)
    {
        case FACTORY_TEST_SPI_KEYPAD:
            result = test_spi_keypad(detail, &duration_ms);
            break;
        case FACTORY_TEST_OPTICAL_SENSOR:
            result = test_optical_sensor(detail, &duration_ms);
            break;
        case FACTORY_TEST_JOYSTICK_ADC:
            result = test_joystick_adc(detail, &duration_ms);
            break;
        case FACTORY_TEST_ENCODER:
            result = test_encoder(detail, &duration_ms);
            break;
        case FACTORY_TEST_FLASH:
            result = test_flash(detail, &duration_ms);
            break;
        case FACTORY_TEST_LED:
            result = test_led(detail, &duration_ms);
            break;
        case FACTORY_TEST_USB:
            result = test_usb(detail, &duration_ms);
            break;
        default:
            strcpy(detail, "未知测试项");
            break;
    }

    /* 保存结果 */
    s_result.items[item].result = result;
    s_result.items[item].duration_ms = duration_ms;
    strncpy(s_result.items[item].detail, detail, 63);
    s_result.items[item].detail[63] = '\0';

    /* 更新统计 */
    if (result == FACTORY_TEST_PASS)
    {
        s_result.pass_count++;
    }
    else if (result == FACTORY_TEST_FAIL)
    {
        s_result.fail_count++;
    }

    return result;
}

void factory_test_start_all(void)
{
    s_result.running = true;
    s_result.pass_count = 0;
    s_result.fail_count = 0;

    printf("\n========== 工厂测试开始 ==========\n");

    /* 逐个运行测试项 */
    for (int i = 0; i < FACTORY_TEST_COUNT; i++)
    {
        s_result.current_item = i;
        s_result.items[i].result = FACTORY_TEST_RUNNING;

        printf("[%d/%d] 运行测试...\n", i + 1, FACTORY_TEST_COUNT);

        factory_test_status_t res = factory_test_run_item((factory_test_item_t)i);

        const char* res_str = (res == FACTORY_TEST_PASS) ? "PASS" : "FAIL";
        printf("  结果: %s - %s (%lu ms)\n",
               res_str,
               s_result.items[i].detail,
               s_result.items[i].duration_ms);
    }

    s_result.running = false;

    printf("\n========== 工厂测试结束 ==========\n");
    printf("通过: %d / %d\n", s_result.pass_count, FACTORY_TEST_COUNT);
    printf("失败: %d\n", s_result.fail_count);

    if (s_result.fail_count == 0)
    {
        printf("✅ 全部测试通过！\n");
    }
    else
    {
        printf("❌ 有测试失败，请检查！\n");
    }
    printf("==================================\n\n");
}

void factory_test_get_result(factory_test_result_t* out_result)
{
    if (out_result == NULL)
    {
        return;
    }
    memcpy(out_result, &s_result, sizeof(factory_test_result_t));
}

void factory_test_print_result(void)
{
    printf("\n========== 测试结果 ==========\n");
    printf("通过: %d / %d\n", s_result.pass_count, FACTORY_TEST_COUNT);
    printf("失败: %d\n\n", s_result.fail_count);

    for (int i = 0; i < FACTORY_TEST_COUNT; i++)
    {
        const char* res_str;
        switch (s_result.items[i].result)
        {
            case FACTORY_TEST_NOT_RUN: res_str = "NOT RUN"; break;
            case FACTORY_TEST_PASS:    res_str = "PASS"; break;
            case FACTORY_TEST_FAIL:    res_str = "FAIL"; break;
            case FACTORY_TEST_RUNNING: res_str = "RUNNING"; break;
            default:                   res_str = "UNKNOWN"; break;
        }

        printf("[%s] 测试项 %d: %s (%lu ms)\n",
               res_str, i,
               s_result.items[i].detail,
               s_result.items[i].duration_ms);
    }
    printf("==============================\n");
}

bool factory_test_check_entry(void)
{
    /* TODO: 检测是否进入工厂测试模式
     * 可以是：
     * - 按住某个键启动
     * - 某个GPIO拉低
     * - 特殊的USB命令
     */
    return false;  /* 默认不进入 */
}

void factory_test_enter(void)
{
    printf("进入工厂测试模式...\n");
    factory_test_init();
    factory_test_start_all();

    printf("\n工厂测试完成！按任意键或等待30秒自动重启...\n");

    /* 测试完成后，等待按键或超时自动重启 */
    uint32_t wait_start = to_ms_since_boot(get_absolute_time());
    const keypad_spi_cfg_t* keypad_cfg = board_get_keypad_spi_cfg();

    while (1)
    {
        /* 检测键盘：任意键按下即重启 */
        if (keypad_cfg != NULL)
        {
            uint64_t keys = 0;
            keypad_spi_read_u64(keypad_cfg, &keys);
            /* 0xFFFFFFFFFFFFFFFF 表示无键按下（所有位为1，低电平有效） */
            if (keys != 0xFFFFFFFFFFFFFFFFULL)
            {
                printf("检测到按键，重启设备...\n");
                break;
            }
        }

        /* 超时 30 秒自动重启 */
        if (to_ms_since_boot(get_absolute_time()) - wait_start > 30000)
        {
            printf("超时，自动重启设备...\n");
            break;
        }

        sleep_ms(100);
    }

    /* 看门狗复位 */
    watchdog_reboot(0, 0, 100);
    while (1) {}
}
