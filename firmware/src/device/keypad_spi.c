/*
 * src/device/keypad_spi.c
 * keypad_spi 驱动实现（device 层）
 * 驱动仅实现读写时序，不做底层 SPI/GPIO 的 pin-mode 初始化（由 board 层负责）
 * 严格遵循 MISRA-like 约束：单返回点、显式类型转换、禁止动态内存
 */

#include "device/keypad_spi.h"
#include "middleware/fault.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/time.h"

/* 错误码定义（局部） */
enum
{
    KEYPAD_SPI_OK = 0,
    KEYPAD_SPI_ERR_INVALID_PARAM = -1,
    KEYPAD_SPI_ERR_HW = -2,
    KEYPAD_SPI_ERR_TIMEOUT = -3
};

/* SPI读取超时阈值（微秒）
 * 正常读取8字节约650us，设为1ms足够
 */
#define KEYPAD_SPI_TIMEOUT_US  1000U

int keypad_spi_init(const keypad_spi_cfg_t *cfg)
{
    int status = KEYPAD_SPI_OK;

    /* 参数校验 */
    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "keypad_spi", "init null pointer");
        status = KEYPAD_SPI_ERR_INVALID_PARAM;
    }
    else if (cfg->spi == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "keypad_spi", "init null spi instance");
        status = KEYPAD_SPI_ERR_INVALID_PARAM;
    }
    else
    {
        /* 驱动遵循无状态原则，cfg 中仅为只读参数；此函数仅做合法性校验 */
        status = KEYPAD_SPI_OK;
    }

    return status;
}

int keypad_spi_read_u64(const keypad_spi_cfg_t *cfg, uint64_t *out_val)
{
    int status = KEYPAD_SPI_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "keypad_spi", "read null cfg");
        status = KEYPAD_SPI_ERR_INVALID_PARAM;
    }
    else if (cfg->spi == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "keypad_spi", "read null spi instance");
        status = KEYPAD_SPI_ERR_INVALID_PARAM;
    }
    else if (out_val == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "keypad_spi", "read null output pointer");
        status = KEYPAD_SPI_ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t data[8u] = {0u};
        uint32_t start_us = time_us_32();

        /* CS 拉低 -> 延时 -> CS 拉高 -> 延时 -> 通过 SPI 读取 8 字节（大端）
         * 注意：底层 SPI、GPIO 模式应由 board 层完成初始化
         */
        gpio_put((uint)cfg->cs_pin, 0);
        busy_wait_us((uint)cfg->cs_delay_us);

        gpio_put((uint)cfg->cs_pin, 1);
        busy_wait_us((uint)cfg->cs_delay_us);

        /* 从 SPI 读取 8 字节，大端序
         * 使用 spi_read_blocking，写入 0x00 以产生时钟
         */
        size_t const len = 8u;
        int32_t const read_len = (int32_t)spi_read_blocking(cfg->spi, 0x00u, data, len);

        /* 超时检测：正常读取约650us，超过1ms视为异常 */
        uint32_t elapsed_us = time_us_32() - start_us;
        if (elapsed_us > KEYPAD_SPI_TIMEOUT_US)
        {
            fault_record(FAULT_LEVEL_ERROR, "keypad_spi", "spi read timeout");
            status = KEYPAD_SPI_ERR_TIMEOUT;
        }
        else if (read_len != (int32_t)len)
        {
            /* 读取长度不对视作硬件错误 */
            fault_record(FAULT_LEVEL_ERROR, "keypad_spi", "spi read length error");
            status = KEYPAD_SPI_ERR_HW;
        }
        else
        {
            uint64_t val = 0ull;
            for (uint32_t i = 0u; i < len; ++i)
            {
                val <<= 8u;
                val |= (uint64_t)data[i];
            }

            *out_val = val;
            status = KEYPAD_SPI_OK;
        }
    }

    return status;
}
