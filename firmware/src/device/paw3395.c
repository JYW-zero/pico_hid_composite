/*
 * src/device/paw3395.c
 * PAW3395 光学鼠标传感器驱动实现
 * 严格遵循 MISRA-like 约束：单返回点、显式类型转换、禁止动态内存
 * 驱动无内部状态，完全可重入
 */

#include "device/paw3395.h"
#include "middleware/fault.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/time.h"

/* 错误码定义 */
enum
{
    PAW3395_OK = 0,
    PAW3395_ERR_INVALID_PARAM = -1,
    PAW3395_ERR_HW = -2,
    PAW3395_ERR_TIMEOUT = -3
};

/* SPI读写超时阈值（微秒）
 * 正常单字节读写约50us，设为500us足够
 */
#define PAW3395_SPI_TIMEOUT_US  500U

/* PAW3395 寄存器地址定义 */
#define PAW3395_REG_PRODUCT_ID   (0x00u)
#define PAW3395_REG_REVISION_ID  (0x01u)
#define PAW3395_REG_MOTION       (0x02u)
#define PAW3395_REG_DELTA_X_L    (0x03u)
#define PAW3395_REG_DELTA_X_H    (0x04u)
#define PAW3395_REG_DELTA_Y_L    (0x05u)
#define PAW3395_REG_DELTA_Y_H    (0x06u)
#define PAW3395_REG_CONFIG1      (0x0Du)  /* DPI配置寄存器 */
#define PAW3395_REG_POWER_UP_RESET (0x3Au) /* 上电复位寄存器，写0x5A触发软复位 */

/* PAW3395 标准 Product ID */
#define PAW3395_PRODUCT_ID       (0x51u)

/* DPI 寄存器值对照表
 * 公式: CPI = (reg_value + 1) × 25
 */
static const uint8_t s_dpi_reg_table[PAW3395_DPI_MAX] =
{
    0x0Fu,  /* 400 CPI:  (15+1)×25 = 400 */
    0x1Fu,  /* 800 CPI:  (31+1)×25 = 800 */
    0x3Fu,  /* 1600 CPI: (63+1)×25 = 1600 */
    0x7Fu   /* 3200 CPI: (127+1)×25 = 3200 */
};

/* 内部函数：CS 拉低延时 */
static void paw3395_cs_low(const paw3395_cfg_t *cfg)
{
    gpio_put((uint)cfg->cs_pin, 0);
    busy_wait_us((uint)cfg->cs_delay_us);
}

/* 内部函数：CS 拉高延时 */
static void paw3395_cs_high(const paw3395_cfg_t *cfg)
{
    gpio_put((uint)cfg->cs_pin, 1);
    busy_wait_us((uint)cfg->cs_delay_us);
}

int paw3395_reg_read(const paw3395_cfg_t *cfg, uint8_t addr, uint8_t *out_val)
{
    int status = PAW3395_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "reg_read null cfg");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else if (cfg->spi == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "reg_read null spi");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else if (out_val == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "reg_read null output");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t tx_buf = (uint8_t)(addr & 0x7Fu);  /* 读操作：最高位为0 */
        uint8_t rx_buf = 0u;
        uint32_t start_us = time_us_32();

        paw3395_cs_low(cfg);
        (void)spi_write_blocking(cfg->spi, &tx_buf, 1u);
        busy_wait_us((uint)cfg->reg_delay_us);
        (void)spi_read_blocking(cfg->spi, 0x00u, &rx_buf, 1u);
        paw3395_cs_high(cfg);

        /* 超时检测 */
        uint32_t elapsed_us = time_us_32() - start_us;
        if (elapsed_us > PAW3395_SPI_TIMEOUT_US)
        {
            fault_record(FAULT_LEVEL_ERROR, "paw3395", "spi read timeout");
            status = PAW3395_ERR_TIMEOUT;
        }
        else
        {
            *out_val = rx_buf;
            status = PAW3395_OK;
        }
    }

    return status;
}

int paw3395_reg_write(const paw3395_cfg_t *cfg, uint8_t addr, uint8_t val)
{
    int status = PAW3395_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "reg_write null cfg");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else if (cfg->spi == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "reg_write null spi");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t tx_buf[2u];
        tx_buf[0u] = (uint8_t)(addr | 0x80u);  /* 写操作：最高位为1 */
        tx_buf[1u] = val;
        uint32_t start_us = time_us_32();

        paw3395_cs_low(cfg);
        (void)spi_write_blocking(cfg->spi, tx_buf, 2u);
        busy_wait_us((uint)cfg->reg_delay_us);
        paw3395_cs_high(cfg);

        /* 超时检测 */
        uint32_t elapsed_us = time_us_32() - start_us;
        if (elapsed_us > PAW3395_SPI_TIMEOUT_US)
        {
            fault_record(FAULT_LEVEL_ERROR, "paw3395", "spi write timeout");
            status = PAW3395_ERR_TIMEOUT;
        }
        else
        {
            status = PAW3395_OK;
        }
    }

    return status;
}

int paw3395_reset(const paw3395_cfg_t *cfg)
{
    int status = PAW3395_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "reset null cfg");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else
    {
        /* 硬件复位：拉低1ms，拉高后等待50ms稳定 */
        gpio_put((uint)cfg->rst_pin, 0);
        busy_wait_ms(1u);
        gpio_put((uint)cfg->rst_pin, 1);
        busy_wait_ms(50u);

        status = PAW3395_OK;
    }

    return status;
}

int paw3395_init(const paw3395_cfg_t *cfg)
{
    int status = PAW3395_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "init null cfg");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else if (cfg->spi == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "init null spi");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t pid = 0u;
        uint8_t dummy = 0u;

        /* 执行硬件复位 */
        status = paw3395_reset(cfg);
        if (status != PAW3395_OK)
        {
            fault_record(FAULT_LEVEL_ERROR, "paw3395", "reset failed");
        }
        else
        {
            /* 读取产品ID验证 */
            status = paw3395_reg_read(cfg, PAW3395_REG_PRODUCT_ID, &pid);
            if (status != PAW3395_OK)
            {
                fault_record(FAULT_LEVEL_ERROR, "paw3395", "read pid failed");
            }
            else if (pid != PAW3395_PRODUCT_ID)
            {
                fault_record(FAULT_LEVEL_ERROR, "paw3395", "unexpected product ID");
                status = PAW3395_ERR_HW;
            }
            else
            {
                /* 软复位：确保寄存器回到已知状态 */
                status = paw3395_reg_write(cfg, PAW3395_REG_POWER_UP_RESET, 0x5Au);
                if (status != PAW3395_OK)
                {
                    fault_record(FAULT_LEVEL_ERROR, "paw3395", "soft reset failed");
                }
                else
                {
                    /* 等待软复位完成（≥1ms） */
                    busy_wait_ms(2);

                    /* 清除运动标志（读一次Motion寄存器） */
                    (void)paw3395_reg_read(cfg, PAW3395_REG_MOTION, &dummy);

                    /* 设置默认DPI: 800 */
                    status = paw3395_set_dpi(cfg, PAW3395_DPI_800);
                    if (status != PAW3395_OK)
                    {
                        fault_record(FAULT_LEVEL_ERROR, "paw3395", "set default dpi failed");
                    }
                    else
                    {
                        fault_record(FAULT_LEVEL_INFO, "paw3395", "init complete");
                    }
                }
            }
        }
    }

    return status;
}

int paw3395_set_dpi(const paw3395_cfg_t *cfg, paw3395_dpi_e dpi)
{
    int status = PAW3395_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "set_dpi null cfg");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else if (dpi >= PAW3395_DPI_MAX)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "set_dpi invalid dpi");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else
    {
        status = paw3395_reg_write(cfg, PAW3395_REG_CONFIG1, s_dpi_reg_table[dpi]);
    }

    return status;
}

int paw3395_set_dpi_raw(const paw3395_cfg_t *cfg, uint16_t cpi)
{
    int status = PAW3395_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "set_dpi_raw null cfg");
        return PAW3395_ERR_INVALID_PARAM;
    }

    /* 限制范围：100-6400 CPI，对齐到25的倍数 */
    if (cpi < 100) cpi = 100;
    if (cpi > 6400) cpi = 6400;
    cpi = (cpi / 25) * 25;  /* 对齐到25的倍数 */

    /* 寄存器值 = CPI/25 - 1，范围0-255 */
    uint8_t reg_val = (uint8_t)(cpi / 25 - 1);
    status = paw3395_reg_write(cfg, PAW3395_REG_CONFIG1, reg_val);

    return status;
}

int paw3395_read_motion(const paw3395_cfg_t *cfg, paw3395_motion_t *motion)
{
    int status = PAW3395_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "read_motion null cfg");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else if (cfg->spi == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "read_motion null spi");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else if (motion == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "paw3395", "read_motion null output");
        status = PAW3395_ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t mot_reg = 0u;
        uint8_t dx_l = 0u;
        uint8_t dx_h = 0u;
        uint8_t dy_l = 0u;
        uint8_t dy_h = 0u;

        /* 读取运动状态寄存器 */
        status = paw3395_reg_read(cfg, PAW3395_REG_MOTION, &mot_reg);
        if (status != PAW3395_OK)
        {
            fault_record(FAULT_LEVEL_ERROR, "paw3395", "read motion reg failed");
        }
        else
        {
            /* 检查运动位 bit7 */
            if ((mot_reg & 0x80u) == 0u)
            {
                /* 无运动 */
                motion->has_motion = false;
                motion->dx = 0;
                motion->dy = 0;
                status = PAW3395_OK;
            }
            else
            {
                /* 有运动，读取位移寄存器 */
                status = paw3395_reg_read(cfg, PAW3395_REG_DELTA_X_L, &dx_l);
                if (status == PAW3395_OK)
                {
                    status = paw3395_reg_read(cfg, PAW3395_REG_DELTA_X_H, &dx_h);
                }
                if (status == PAW3395_OK)
                {
                    status = paw3395_reg_read(cfg, PAW3395_REG_DELTA_Y_L, &dy_l);
                }
                if (status == PAW3395_OK)
                {
                    status = paw3395_reg_read(cfg, PAW3395_REG_DELTA_Y_H, &dy_h);
                }

                if (status != PAW3395_OK)
                {
                    fault_record(FAULT_LEVEL_ERROR, "paw3395", "read delta reg failed");
                }
                else
                {
                    /* 组合16位有符号位移值（PAW3395原生16位） */
                    int16_t dx = (int16_t)(((uint16_t)dx_h << 8) | (uint16_t)dx_l);
                    int16_t dy = (int16_t)(((uint16_t)dy_h << 8) | (uint16_t)dy_l);

                    motion->has_motion = true;
                    motion->dx = dx;
                    motion->dy = dy;
                }
            }
        }
    }

    return status;
}

