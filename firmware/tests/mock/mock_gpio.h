/*
 * tests/mock/mock_gpio.h
 * Mock版GPIO头文件，用于单元测试
 */

#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H

#include <stdint.h>
#include <stdbool.h>

typedef unsigned int uint;

/* ==================== 模拟Pico的GPIO函数 ==================== */

#define GPIO_IN  0
#define GPIO_OUT 1

void gpio_init(uint pin);
void gpio_set_dir(uint pin, bool out);
void gpio_pull_up(uint pin);
bool gpio_get(uint pin);

/* ==================== 测试专用接口 ==================== */

/* 设置GPIO引脚电平 */
void mock_gpio_set(uint pin, bool value);

/* 重置所有GPIO状态 */
void mock_gpio_reset(void);

#endif /* MOCK_GPIO_H */
