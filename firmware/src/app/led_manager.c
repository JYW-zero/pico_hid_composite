/*
 * src/app/led_manager.c
 * LED 管理模块实现
 * 从 main.c 提取，包含 LED 闪烁任务和模式切换反馈
 */
#include "app/led_manager.h"
#include "app/usb_callbacks.h"
#include "bsp/board_api.h"

/* ==================== 私有数据 ==================== */

/* 模式切换 LED 反馈状态 */
static volatile uint8_t  s_led_feedback_count = 0;    /* 剩余闪烁次数 */
static volatile uint32_t s_led_feedback_next_ms = 0;  /* 下次闪烁时间 */
static volatile bool     s_led_feedback_state = false; /* 当前 LED 状态 */

/* 正常闪烁状态 */
static uint32_t s_blink_start_ms = 0;
static bool     s_blink_led_state = false;

/* ==================== 初始化 ==================== */

void led_manager_init(void)
{
  s_led_feedback_count = 0;
  s_led_feedback_next_ms = 0;
  s_led_feedback_state = false;
  s_blink_start_ms = 0;
  s_blink_led_state = false;
}

/* ==================== LED 闪烁任务 ==================== */

void led_manager_blinking_task(void)
{
  /* 模式切换反馈：快速闪烁（100ms间隔），优先级高于正常闪烁 */
  if (s_led_feedback_count > 0)
  {
    uint32_t now = board_millis();
    if (now >= s_led_feedback_next_ms)
    {
      s_led_feedback_state = !s_led_feedback_state;
      board_led_write(s_led_feedback_state);
      s_led_feedback_next_ms = now + 100;  /* 100ms 快速闪烁 */
      if (!s_led_feedback_state)
      {
        /* 完成一次完整的亮灭，计数减1 */
        s_led_feedback_count--;
        if (s_led_feedback_count == 0)
        {
          /* 反馈结束，关闭 LED */
          board_led_write(false);
        }
      }
    }
    return;
  }

  /* 正常闪烁：根据 USB 状态以不同间隔闪烁 */
  uint32_t blink_interval = usb_callbacks_get_blink_interval();
  if (!blink_interval) return;

  if (board_millis() - s_blink_start_ms < blink_interval) return;
  s_blink_start_ms += blink_interval;

  board_led_write(s_blink_led_state);
  s_blink_led_state = !s_blink_led_state;
}

/* ==================== 模式切换 LED 反馈 ==================== */

void led_manager_trigger_mode_feedback(encoder_mode_t mode)
{
  s_led_feedback_count = (uint8_t)mode + 1;
  s_led_feedback_next_ms = board_millis();
  s_led_feedback_state = false;
}
