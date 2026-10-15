/*
 * src/protocol/config_validator.c
 * 配置验证实现
 * 从 config_hid.c 提取，包含配置魔数验证、CRC校验、参数验证
 */
#include "protocol/config_validator.h"
#include "middleware/crc32.h"
#include <string.h>

/* ==================== 验证函数 ==================== */

bool config_validator_check_magic(const device_config_t *config)
{
  if (config == NULL) return false;
  return (config->magic == CONFIG_MAGIC);
}

uint32_t config_validator_calc_crc(const device_config_t *config)
{
  if (config == NULL) return 0;
  /* CRC 计算忽略 crc32 字段本身（最后4字节） */
  return crc32_calc((const uint8_t*)config, sizeof(device_config_t) - sizeof(uint32_t));
}

bool config_validator_check_crc(const device_config_t *config)
{
  if (config == NULL) return false;
  uint32_t calc_crc = config_validator_calc_crc(config);
  return (calc_crc == config->crc32);
}

config_hid_error_t config_validator_validate(const device_config_t *config)
{
  if (config == NULL) return CONFIG_HID_ERR_INVALID_PARAM;

  /* 验证魔数 */
  if (!config_validator_check_magic(config))
  {
    return CONFIG_HID_ERR_CRC_MISMATCH;
  }

  /* 验证 CRC */
  if (!config_validator_check_crc(config))
  {
    return CONFIG_HID_ERR_CRC_MISMATCH;
  }

  /* 验证基本参数范围 */
  if (config->dpi < 100 || config->dpi > 6400)
  {
    return CONFIG_HID_ERR_INVALID_PARAM;
  }

  if (config->joystick_deadzone > 4095)
  {
    return CONFIG_HID_ERR_INVALID_PARAM;
  }

  if (config->current_dpi_slot > 3)
  {
    return CONFIG_HID_ERR_INVALID_PARAM;
  }

  if (config->lod_level > 3)
  {
    return CONFIG_HID_ERR_INVALID_PARAM;
  }

  return CONFIG_HID_OK;
}

/* ==================== 配置合并 ==================== */

void config_validator_merge_config(device_config_t *current, const device_config_t *new_config)
{
  if (current == NULL || new_config == NULL) return;

  /* 只覆盖前142字节（magic ~ fn_keymap 结束，不含 macro_data） */
  memcpy(current, new_config, 142);

  /* 覆盖v3新增字段（偏移1326+，在macro_data之后）
   * 这些字段不在前142字节内，必须单独复制 */
  current->joystick_invert_x = new_config->joystick_invert_x;
  current->joystick_invert_y = new_config->joystick_invert_y;
  current->encoder_steps = new_config->encoder_steps;
  current->encoder_scroll_speed = new_config->encoder_scroll_speed;
  current->joystick_sensitivity = new_config->joystick_sensitivity;

  /* v6 光学传感器高级配置（上位机经 CONFIG_EXT2 子块0下发）。
   * 修复：此前遗漏会导致 GUI 修改的 4 档 DPI 值 / 当前档 / LOD / 角度捕捉 /
   * DPI 切换键在 APPLY 时沿用 Flash 旧值，表现为“保存后不生效”。 */
  memcpy(current->dpi_slot, new_config->dpi_slot, sizeof(current->dpi_slot));
  current->current_dpi_slot = new_config->current_dpi_slot;
  current->lod_level        = new_config->lod_level;
  current->angle_snapping   = new_config->angle_snapping;
  current->dpi_switch_key   = new_config->dpi_switch_key;

  /* v7 四层键映射 Layer2/Layer3（CONFIG_EXT2 子块1-4）。
   * 修复：此前遗漏会导致第 3/4 层键映射保存后丢失。 */
  memcpy(current->layer2_keymap, new_config->layer2_keymap, sizeof(current->layer2_keymap));
  memcpy(current->layer3_keymap, new_config->layer3_keymap, sizeof(current->layer3_keymap));

  /* 以下字段刻意不在此合并，保留 Flash 现值：
   *  - v5 摇杆校准(center_x/center_y/calibrated)：由 CMD_JOYSTICK_CALIBRATE
   *    单独采样并持久化，上位机配置块不下发 calibrated，合并零值反而会清除校准；
   *  - v8 perf_thresholds：仅 dev，由 CMD_SET_PERF_THRESHOLD 单独持久化；
   *  - macro_data：由宏命令单独读写；length/write_count/seq 由 config_save 重设。 */

  /* 强制设置正确的 magic 和 version */
  current->magic = CONFIG_MAGIC;
  current->version = CONFIG_VERSION;

  /* 重新计算 CRC（忽略上位机传来的 CRC） */
  current->crc32 = config_validator_calc_crc(current);
}
