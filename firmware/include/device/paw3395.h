/*
 * include/device/paw3395.h
 * PAW3395 光学鼠标传感器驱动头文件
 * 驱动无内部状态，配置与运行状态分离，完全可重入
 */

#ifndef DEVICE_PAW3395_H
#define DEVICE_PAW3395_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前置类型声明 */
struct spi_inst;

/* DPI 档位枚举 */
typedef enum
{
    PAW3395_DPI_400 = 0,
    PAW3395_DPI_800,
    PAW3395_DPI_1600,
    PAW3395_DPI_3200,
    PAW3395_DPI_MAX
} paw3395_dpi_e;

/* 运动数据结构体 */
typedef struct
{
    int16_t dx;   /* X轴位移 */
    int16_t dy;   /* Y轴位移 */
    bool has_motion; /* 是否有运动 */
} paw3395_motion_t;

/* PAW3395 配置结构体：仅包含只读硬件参数 */
typedef struct
{
    struct spi_inst *spi;      /* SPI外设实例 */
    uint32_t cs_pin;           /* CS引脚号 */
    uint32_t mot_pin;          /* MOT运动中断引脚 */
    uint32_t rst_pin;          /* 复位引脚 */
    uint32_t baud_hz;          /* SPI波特率 */
    uint32_t cs_delay_us;      /* CS延时 */
    uint32_t reg_delay_us;     /* 寄存器读写延时 */
} paw3395_cfg_t;

/* 初始化传感器 */
int paw3395_init(const paw3395_cfg_t *cfg);

/* 软件复位传感器 */
int paw3395_reset(const paw3395_cfg_t *cfg);

/* 设置DPI（固定档位） */
int paw3395_set_dpi(const paw3395_cfg_t *cfg, paw3395_dpi_e dpi);

/* 设置任意DPI（CPI值，范围100-6400，会自动对齐到25的倍数） */
int paw3395_set_dpi_raw(const paw3395_cfg_t *cfg, uint16_t cpi);

/* 读取运动数据 */
int paw3395_read_motion(const paw3395_cfg_t *cfg, paw3395_motion_t *motion);

/* 读取寄存器（调试用） */
int paw3395_reg_read(const paw3395_cfg_t *cfg, uint8_t addr, uint8_t *out_val);

/* 写入寄存器（调试用） */
int paw3395_reg_write(const paw3395_cfg_t *cfg, uint8_t addr, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_PAW3395_H */
