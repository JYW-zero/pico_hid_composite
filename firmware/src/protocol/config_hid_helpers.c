/*
 * src/protocol/config_hid_helpers.c
 * 配置 HID 辅助函数实现
 * 从 config_hid.c 提取，包含 ACK 发送和 DPI 转换等独立辅助函数
 */
#include "protocol/config_hid_helpers.h"
#include "protocol/config_hid.h"
#include "protocol/usb_descriptors.h"
#include "app/log.h"
#include "tusb.h"
#include "pico/time.h"

/* ==================== ACK 响应发送 ==================== */

/* ACK 发送最大重试次数 */
#define ACK_SEND_MAX_RETRIES  5u
/* ACK 发送重试间隔（微秒） */
#define ACK_SEND_RETRY_US     1000u

void config_hid_send_ack(uint8_t cmd, bool success, uint8_t error_code, uint16_t param)
{
  config_hid_ack_t ack;
  ack.report_id = REPORT_ID_ACK;
  ack.cmd = cmd;
  ack.status = success ? 0 : 1;
  ack.error_code = error_code;
  ack.param = param;
  ack.reserved[0] = 0;
  ack.reserved[1] = 0;

  /* 通过配置 HID 接口（instance 1）发送 Input 报告
   * 注意：报告描述符中 ACK 定义了 Report ID=24，
   * TinyUSB 的 tud_hid_n_report 期望 buffer 包含 Report ID 作为第一个字节，
   * 不会自动添加 Report ID，所以直接发送完整的 ack 结构体
   *
   * 可靠性：tud_hid_n_report 在端点忙时会返回 false，
   * 必须重试，否则 ACK 静默丢失导致上位机超时（这是之前解锁失败的根本原因）
   */
  if (!tud_hid_n_ready(1))
  {
    LOG_ERROR_PRINT("[ACK] ❌ HID接口未就绪，无法发送ACK: cmd=0x%02X\n", cmd);
    return;
  }

  for (uint8_t retry = 0; retry < ACK_SEND_MAX_RETRIES; retry++)
  {
    if (tud_hid_n_report(1, REPORT_ID_ACK, &ack, sizeof(ack)))
    {
      if (retry > 0)
      {
        LOG_DEBUG_PRINT("[ACK] 发送成功（重试%d次）: cmd=0x%02X\n", retry, cmd);
      }
      return;
    }
    /* 端点忙，短暂等待后重试 */
    sleep_us(ACK_SEND_RETRY_US);
  }

  /* 所有重试都失败 */
  LOG_ERROR_PRINT("[ACK] ❌ 发送失败（重试%d次均失败）: cmd=0x%02X, success=%d\n",
                  ACK_SEND_MAX_RETRIES, cmd, success ? 1 : 0);
}

/* ==================== DPI 转换 ==================== */

optical_sensor_dpi_e config_hid_dpi_val_to_enum(uint16_t dpi_val)
{
  switch (dpi_val)
  {
    case 400:  return optical_sensor_DPI_400;
    case 800:  return optical_sensor_DPI_800;
    case 1600: return optical_sensor_DPI_1600;
    case 3200: return optical_sensor_DPI_3200;
    default:   return optical_sensor_DPI_1600;
  }
}
