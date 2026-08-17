/*
 * include/device/optical_sensor.h
 * optical_sensor 鍏夊榧犳爣浼犳劅鍣ㄩ┍鍔ㄥご鏂囦欢
 * 椹卞姩鏃犲唴閮ㄧ姸鎬侊紝閰嶇疆涓庤繍琛岀姸鎬佸垎绂伙紝瀹屽叏鍙噸鍏?
 */

#ifndef DEVICE_OPTICAL_SENSOR_H
#define DEVICE_OPTICAL_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 鍓嶇疆绫诲瀷澹版槑 */
struct spi_inst;

/* DPI 妗ｄ綅鏋氫妇 */
typedef enum
{
    optical_sensor_DPI_400 = 0,
    optical_sensor_DPI_800,
    optical_sensor_DPI_1600,
    optical_sensor_DPI_3200,
    optical_sensor_DPI_MAX
} optical_sensor_dpi_e;

/* 杩愬姩鏁版嵁缁撴瀯浣?*/
typedef struct
{
    int16_t dx;   /* X杞翠綅绉?*/
    int16_t dy;   /* Y杞翠綅绉?*/
    bool has_motion; /* 鏄惁鏈夎繍鍔?*/
} optical_sensor_motion_t;

/* optical_sensor 閰嶇疆缁撴瀯浣擄細浠呭寘鍚彧璇荤‖浠跺弬鏁?*/
typedef struct
{
    struct spi_inst *spi;      /* SPI澶栬瀹炰緥 */
    uint32_t cs_pin;           /* CS寮曡剼鍙?*/
    uint32_t mot_pin;          /* MOT杩愬姩涓柇寮曡剼 */
    uint32_t rst_pin;          /* 澶嶄綅寮曡剼 */
    uint32_t baud_hz;          /* SPI娉㈢壒鐜?*/
    uint32_t cs_delay_us;      /* CS寤舵椂 */
    uint32_t reg_delay_us;     /* 瀵勫瓨鍣ㄨ鍐欏欢鏃?*/
} optical_sensor_cfg_t;

/* 鍒濆鍖栦紶鎰熷櫒 */
int optical_sensor_init(const optical_sensor_cfg_t *cfg);

/* 杞欢澶嶄綅浼犳劅鍣?*/
int optical_sensor_reset(const optical_sensor_cfg_t *cfg);

/* 璁剧疆DPI锛堝浐瀹氭。浣嶏級 */
int optical_sensor_set_dpi(const optical_sensor_cfg_t *cfg, optical_sensor_dpi_e dpi);

/* 璁剧疆浠绘剰DPI锛圕PI鍊硷紝鑼冨洿100-6400锛屼細鑷姩瀵归綈鍒?5鐨勫€嶆暟锛?*/
int optical_sensor_set_dpi_raw(const optical_sensor_cfg_t *cfg, uint16_t cpi);

/* 璇诲彇杩愬姩鏁版嵁 */
int optical_sensor_read_motion(const optical_sensor_cfg_t *cfg, optical_sensor_motion_t *motion);

/* 璇诲彇瀵勫瓨鍣紙璋冭瘯鐢級 */
int optical_sensor_reg_read(const optical_sensor_cfg_t *cfg, uint8_t addr, uint8_t *out_val);

/* 鍐欏叆瀵勫瓨鍣紙璋冭瘯鐢級 */
int optical_sensor_reg_write(const optical_sensor_cfg_t *cfg, uint8_t addr, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_optical_sensor_H */
