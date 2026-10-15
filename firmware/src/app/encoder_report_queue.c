/*
 * src/app/encoder_report_queue.c
 * 编码器待发送报告环形队列实现
 * 从 main.c 提取，解决编码器多报告连续发送导致的 TinyUSB 异步缓冲区顺序错误问题
 *
 * 问题背景：编码器的 VOLUME/ZOOM/BRIGHTNESS/CUSTOM 模式需要发送
 * 多步 HID 报告（如 ZOOM: Ctrl按下 → 滚轮 → Ctrl释放）。
 * 如果在同一个函数调用中连续发送，TinyUSB 的异步缓冲区可能导致
 * 主机处理顺序错误或报告丢失。
 *
 * 解决方案：将多步报告加入环形队列，encoder_processor_task 每帧
 * （1ms）只从队列取出一个报告发送，确保报告间至少间隔一个 USB 轮询周期。
 *
 * 队列容量：16 个报告（足够容纳最复杂的 ZOOM 操作 3 个报告 × 多次旋转）
 */
#include "app/encoder_report_queue.h"
#include "protocol/usb_descriptors.h"
#include "tusb.h"
#include <string.h>

/* ==================== 私有数据 ==================== */

/* 待发送报告结构体 */
typedef struct {
  enc_report_type_t type;
  uint8_t  data[8];   /* 报告数据（最大8字节，鼠标报告单独处理） */
  uint8_t  length;    /* 报告数据长度 */
  int8_t   wheel;     /* 鼠标报告专用：滚轮值 */
} enc_pending_report_t;

/* 队列容量 */
#define ENC_PENDING_QUEUE_SIZE  16

/* 环形队列缓冲区 */
static enc_pending_report_t s_enc_pending_queue[ENC_PENDING_QUEUE_SIZE];

/* 队列指针 */
static uint8_t s_enc_pending_head = 0;   /* 写入位置 */
static uint8_t s_enc_pending_tail = 0;   /* 读取位置 */
static uint8_t s_enc_pending_count = 0;  /* 当前队列中的报告数 */

/* ==================== 初始化 ==================== */

void encoder_report_queue_init(void)
{
  memset(s_enc_pending_queue, 0, sizeof(s_enc_pending_queue));
  s_enc_pending_head = 0;
  s_enc_pending_tail = 0;
  s_enc_pending_count = 0;
}

/* ==================== 入队操作 ==================== */

bool encoder_report_queue_push_generic(enc_report_type_t type, const uint8_t *data, uint8_t length)
{
  if (s_enc_pending_count >= ENC_PENDING_QUEUE_SIZE) return false;
  if (length > sizeof(s_enc_pending_queue[0].data)) return false;

  enc_pending_report_t *r = &s_enc_pending_queue[s_enc_pending_head];
  r->type = type;
  r->length = length;
  r->wheel = 0;
  if (data && length > 0) memcpy(r->data, data, length);

  s_enc_pending_head = (s_enc_pending_head + 1) % ENC_PENDING_QUEUE_SIZE;
  s_enc_pending_count++;
  return true;
}

bool encoder_report_queue_push_mouse_wheel(int8_t wheel)
{
  if (s_enc_pending_count >= ENC_PENDING_QUEUE_SIZE) return false;

  enc_pending_report_t *r = &s_enc_pending_queue[s_enc_pending_head];
  r->type = ENC_REPORT_MOUSE;
  r->length = 0;
  r->wheel = wheel;
  memset(r->data, 0, sizeof(r->data));

  s_enc_pending_head = (s_enc_pending_head + 1) % ENC_PENDING_QUEUE_SIZE;
  s_enc_pending_count++;
  return true;
}

/* ==================== 出队与发送 ==================== */

bool encoder_report_queue_send_one(void)
{
  if (s_enc_pending_count == 0) return false;
  if (!tud_hid_ready()) return false;

  enc_pending_report_t *r = &s_enc_pending_queue[s_enc_pending_tail];

  switch (r->type)
  {
    case ENC_REPORT_KEYBOARD:
      tud_hid_report(REPORT_ID_KEYBOARD, r->data, r->length);
      break;

    case ENC_REPORT_CONSUMER:
      tud_hid_report(REPORT_ID_CONSUMER_CONTROL, r->data, r->length);
      break;

    case ENC_REPORT_MOUSE:
      tud_hid_mouse_report(REPORT_ID_MOUSE, 0, 0, 0, r->wheel, 0);
      break;

    default:
      break;
  }

  s_enc_pending_tail = (s_enc_pending_tail + 1) % ENC_PENDING_QUEUE_SIZE;
  s_enc_pending_count--;
  return true;
}
