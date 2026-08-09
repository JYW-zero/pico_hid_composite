/*
 * include/device/keypad_spi.h
 * keypad_spi 驱动头文件（device 层）
 * 遵循项目规范：cfg 仅包含只读硬件参数，不保存运行态；驱动提供初始化校验和读取原始 uint64_t 接口
 */

#ifndef DEVICE_KEYPAD_SPI_H
#define DEVICE_KEYPAD_SPI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前置类型声明（pico-sdk 类型） */
struct spi_inst; /* spi_inst_t */

/* keypad_spi 配置结构体：只读硬件参数 */
typedef struct
{
    struct spi_inst *spi;      /* 外设实例指针（由 board 层传入并初始化） */
    uint32_t cs_pin;           /* CS 引脚号 */
    uint32_t baud_hz;          /* 波特率 */
    uint32_t cs_delay_us;      /* CS 时序延时，单位微秒 */
} keypad_spi_cfg_t;

/* 返回值约定：0 成功，负值为错误码 */
int keypad_spi_init(const keypad_spi_cfg_t *cfg);

/* 读取 64 位原始按键数据（大端），bit=0 表示按下
 * 输出通过 out_val 返回，函数不修改 cfg
 */
int keypad_spi_read_u64(const keypad_spi_cfg_t *cfg, uint64_t *out_val);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_KEYPAD_SPI_H */
