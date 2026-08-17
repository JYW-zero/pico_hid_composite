/*
 * src/device/optical_sensor.c
 * optical_sensor 鍏夊榧犳爣浼犳劅鍣ㄩ┍鍔ㄥ疄鐜?
 * 涓ユ牸閬靛惊 MISRA-like 绾︽潫锛氬崟杩斿洖鐐广€佹樉寮忕被鍨嬭浆鎹€佺姝㈠姩鎬佸唴瀛?
 * 椹卞姩鏃犲唴閮ㄧ姸鎬侊紝瀹屽叏鍙噸鍏?
 */

#include "device/optical_sensor.h"
#include "middleware/fault.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/time.h"

/* 閿欒鐮佸畾涔?*/
enum
{
    optical_sensor_OK = 0,
    optical_sensor_ERR_INVALID_PARAM = -1,
    optical_sensor_ERR_HW = -2,
    optical_sensor_ERR_TIMEOUT = -3
};

/* SPI璇诲啓瓒呮椂闃堝€硷紙寰锛?
 * 姝ｅ父鍗曞瓧鑺傝鍐欑害50us锛岃涓?00us瓒冲
 */
#define optical_sensor_SPI_TIMEOUT_US  500U

/* optical_sensor 瀵勫瓨鍣ㄥ湴鍧€瀹氫箟 */
#define optical_sensor_REG_PRODUCT_ID   (0x00u)
#define optical_sensor_REG_REVISION_ID  (0x01u)
#define optical_sensor_REG_MOTION       (0x02u)
#define optical_sensor_REG_DELTA_X_L    (0x03u)
#define optical_sensor_REG_DELTA_X_H    (0x04u)
#define optical_sensor_REG_DELTA_Y_L    (0x05u)
#define optical_sensor_REG_DELTA_Y_H    (0x06u)
#define optical_sensor_REG_CONFIG1      (0x0Du)  /* DPI閰嶇疆瀵勫瓨鍣?*/
#define optical_sensor_REG_POWER_UP_RESET (0x3Au) /* 涓婄數澶嶄綅瀵勫瓨鍣紝鍐?x5A瑙﹀彂杞浣?*/

/* optical_sensor 鏍囧噯 Product ID */
#define optical_sensor_PRODUCT_ID       (0x51u)

/* DPI 瀵勫瓨鍣ㄥ€煎鐓ц〃
 * 鍏紡: CPI = (reg_value + 1) 脳 25
 */
static const uint8_t s_dpi_reg_table[optical_sensor_DPI_MAX] =
{
    0x0Fu,  /* 400 CPI:  (15+1)脳25 = 400 */
    0x1Fu,  /* 800 CPI:  (31+1)脳25 = 800 */
    0x3Fu,  /* 1600 CPI: (63+1)脳25 = 1600 */
    0x7Fu   /* 3200 CPI: (127+1)脳25 = 3200 */
};

/* 鍐呴儴鍑芥暟锛欳S 鎷変綆寤舵椂 */
static void optical_sensor_cs_low(const optical_sensor_cfg_t *cfg)
{
    gpio_put((uint)cfg->cs_pin, 0);
    busy_wait_us((uint)cfg->cs_delay_us);
}

/* 鍐呴儴鍑芥暟锛欳S 鎷夐珮寤舵椂 */
static void optical_sensor_cs_high(const optical_sensor_cfg_t *cfg)
{
    gpio_put((uint)cfg->cs_pin, 1);
    busy_wait_us((uint)cfg->cs_delay_us);
}

int optical_sensor_reg_read(const optical_sensor_cfg_t *cfg, uint8_t addr, uint8_t *out_val)
{
    int status = optical_sensor_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "reg_read null cfg");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else if (cfg->spi == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "reg_read null spi");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else if (out_val == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "reg_read null output");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t tx_buf = (uint8_t)(addr & 0x7Fu);  /* 璇绘搷浣滐細鏈€楂樹綅涓? */
        uint8_t rx_buf = 0u;
        uint32_t start_us = time_us_32();

        optical_sensor_cs_low(cfg);
        (void)spi_write_blocking(cfg->spi, &tx_buf, 1u);
        busy_wait_us((uint)cfg->reg_delay_us);
        (void)spi_read_blocking(cfg->spi, 0x00u, &rx_buf, 1u);
        optical_sensor_cs_high(cfg);

        /* 瓒呮椂妫€娴?*/
        uint32_t elapsed_us = time_us_32() - start_us;
        if (elapsed_us > optical_sensor_SPI_TIMEOUT_US)
        {
            fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "spi read timeout");
            status = optical_sensor_ERR_TIMEOUT;
        }
        else
        {
            *out_val = rx_buf;
            status = optical_sensor_OK;
        }
    }

    return status;
}

int optical_sensor_reg_write(const optical_sensor_cfg_t *cfg, uint8_t addr, uint8_t val)
{
    int status = optical_sensor_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "reg_write null cfg");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else if (cfg->spi == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "reg_write null spi");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t tx_buf[2u];
        tx_buf[0u] = (uint8_t)(addr | 0x80u);  /* 鍐欐搷浣滐細鏈€楂樹綅涓? */
        tx_buf[1u] = val;
        uint32_t start_us = time_us_32();

        optical_sensor_cs_low(cfg);
        (void)spi_write_blocking(cfg->spi, tx_buf, 2u);
        busy_wait_us((uint)cfg->reg_delay_us);
        optical_sensor_cs_high(cfg);

        /* 瓒呮椂妫€娴?*/
        uint32_t elapsed_us = time_us_32() - start_us;
        if (elapsed_us > optical_sensor_SPI_TIMEOUT_US)
        {
            fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "spi write timeout");
            status = optical_sensor_ERR_TIMEOUT;
        }
        else
        {
            status = optical_sensor_OK;
        }
    }

    return status;
}

int optical_sensor_reset(const optical_sensor_cfg_t *cfg)
{
    int status = optical_sensor_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "reset null cfg");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else
    {
        /* 纭欢澶嶄綅锛氭媺浣?ms锛屾媺楂樺悗绛夊緟50ms绋冲畾 */
        gpio_put((uint)cfg->rst_pin, 0);
        busy_wait_ms(1u);
        gpio_put((uint)cfg->rst_pin, 1);
        busy_wait_ms(50u);

        status = optical_sensor_OK;
    }

    return status;
}

int optical_sensor_init(const optical_sensor_cfg_t *cfg)
{
    int status = optical_sensor_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "init null cfg");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else if (cfg->spi == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "init null spi");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t pid = 0u;
        uint8_t dummy = 0u;

        /* 鎵ц纭欢澶嶄綅 */
        status = optical_sensor_reset(cfg);
        if (status != optical_sensor_OK)
        {
            fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "reset failed");
        }
        else
        {
            /* 璇诲彇浜у搧ID楠岃瘉 */
            status = optical_sensor_reg_read(cfg, optical_sensor_REG_PRODUCT_ID, &pid);
            if (status != optical_sensor_OK)
            {
                fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "read pid failed");
            }
            else if (pid != optical_sensor_PRODUCT_ID)
            {
                fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "unexpected product ID");
                status = optical_sensor_ERR_HW;
            }
            else
            {
                /* 杞浣嶏細纭繚瀵勫瓨鍣ㄥ洖鍒板凡鐭ョ姸鎬?*/
                status = optical_sensor_reg_write(cfg, optical_sensor_REG_POWER_UP_RESET, 0x5Au);
                if (status != optical_sensor_OK)
                {
                    fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "soft reset failed");
                }
                else
                {
                    /* 绛夊緟杞浣嶅畬鎴愶紙鈮?ms锛?*/
                    busy_wait_ms(2);

                    /* 娓呴櫎杩愬姩鏍囧織锛堣涓€娆otion瀵勫瓨鍣級 */
                    (void)optical_sensor_reg_read(cfg, optical_sensor_REG_MOTION, &dummy);

                    /* 璁剧疆榛樿DPI: 800 */
                    status = optical_sensor_set_dpi(cfg, optical_sensor_DPI_800);
                    if (status != optical_sensor_OK)
                    {
                        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "set default dpi failed");
                    }
                    else
                    {
                        fault_record(FAULT_LEVEL_INFO, "optical_sensor", "init complete");
                    }
                }
            }
        }
    }

    return status;
}

int optical_sensor_set_dpi(const optical_sensor_cfg_t *cfg, optical_sensor_dpi_e dpi)
{
    int status = optical_sensor_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "set_dpi null cfg");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else if (dpi >= optical_sensor_DPI_MAX)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "set_dpi invalid dpi");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else
    {
        status = optical_sensor_reg_write(cfg, optical_sensor_REG_CONFIG1, s_dpi_reg_table[dpi]);
    }

    return status;
}

int optical_sensor_set_dpi_raw(const optical_sensor_cfg_t *cfg, uint16_t cpi)
{
    int status = optical_sensor_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "set_dpi_raw null cfg");
        return optical_sensor_ERR_INVALID_PARAM;
    }

    /* 闄愬埗鑼冨洿锛?00-6400 CPI锛屽榻愬埌25鐨勫€嶆暟 */
    if (cpi < 100) cpi = 100;
    if (cpi > 6400) cpi = 6400;
    cpi = (cpi / 25) * 25;  /* 瀵归綈鍒?5鐨勫€嶆暟 */

    /* 瀵勫瓨鍣ㄥ€?= CPI/25 - 1锛岃寖鍥?-255 */
    uint8_t reg_val = (uint8_t)(cpi / 25 - 1);
    status = optical_sensor_reg_write(cfg, optical_sensor_REG_CONFIG1, reg_val);

    return status;
}

int optical_sensor_read_motion(const optical_sensor_cfg_t *cfg, optical_sensor_motion_t *motion)
{
    int status = optical_sensor_OK;

    if (cfg == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "read_motion null cfg");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else if (cfg->spi == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "read_motion null spi");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else if (motion == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "read_motion null output");
        status = optical_sensor_ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t mot_reg = 0u;
        uint8_t dx_l = 0u;
        uint8_t dx_h = 0u;
        uint8_t dy_l = 0u;
        uint8_t dy_h = 0u;

        /* 璇诲彇杩愬姩鐘舵€佸瘎瀛樺櫒 */
        status = optical_sensor_reg_read(cfg, optical_sensor_REG_MOTION, &mot_reg);
        if (status != optical_sensor_OK)
        {
            fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "read motion reg failed");
        }
        else
        {
            /* 妫€鏌ヨ繍鍔ㄤ綅 bit7 */
            if ((mot_reg & 0x80u) == 0u)
            {
                /* 鏃犺繍鍔?*/
                motion->has_motion = false;
                motion->dx = 0;
                motion->dy = 0;
                status = optical_sensor_OK;
            }
            else
            {
                /* 鏈夎繍鍔紝璇诲彇浣嶇Щ瀵勫瓨鍣?*/
                status = optical_sensor_reg_read(cfg, optical_sensor_REG_DELTA_X_L, &dx_l);
                if (status == optical_sensor_OK)
                {
                    status = optical_sensor_reg_read(cfg, optical_sensor_REG_DELTA_X_H, &dx_h);
                }
                if (status == optical_sensor_OK)
                {
                    status = optical_sensor_reg_read(cfg, optical_sensor_REG_DELTA_Y_L, &dy_l);
                }
                if (status == optical_sensor_OK)
                {
                    status = optical_sensor_reg_read(cfg, optical_sensor_REG_DELTA_Y_H, &dy_h);
                }

                if (status != optical_sensor_OK)
                {
                    fault_record(FAULT_LEVEL_ERROR, "optical_sensor", "read delta reg failed");
                }
                else
                {
                    /* 缁勫悎16浣嶆湁绗﹀彿浣嶇Щ鍊硷紙optical_sensor鍘熺敓16浣嶏級 */
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

