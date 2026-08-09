/*
 * tests/mock/mock_gpio.c
 * Mock版GPIO实现，用于单元测试
 */

#include "mock_gpio.h"
#include <string.h>

/* ==================== 内部状态 ==================== */

#define MOCK_GPIO_MAX 32

static uint8_t s_gpio_values[MOCK_GPIO_MAX];
static bool s_gpio_initialized[MOCK_GPIO_MAX];

/* ==================== Pico GPIO 函数实现 ==================== */

void gpio_init(uint pin)
{
    if (pin < MOCK_GPIO_MAX)
    {
        s_gpio_initialized[pin] = true;
        s_gpio_values[pin] = 1;  /* 上拉默认高电平 */
    }
}

void gpio_set_dir(uint pin, bool out)
{
    (void)pin;
    (void)out;
    /* mock实现，什么都不做 */
}

void gpio_pull_up(uint pin)
{
    if (pin < MOCK_GPIO_MAX)
    {
        s_gpio_values[pin] = 1;  /* 上拉，默认高电平 */
    }
}

bool gpio_get(uint pin)
{
    if (pin >= MOCK_GPIO_MAX)
    {
        return false;
    }
    return s_gpio_values[pin] != 0;
}

/* ==================== 测试专用接口 ==================== */

void mock_gpio_set(uint pin, bool value)
{
    if (pin < MOCK_GPIO_MAX)
    {
        s_gpio_values[pin] = value ? 1 : 0;
    }
}

void mock_gpio_reset(void)
{
    memset(s_gpio_values, 0, sizeof(s_gpio_values));
    memset(s_gpio_initialized, 0, sizeof(s_gpio_initialized));
}
