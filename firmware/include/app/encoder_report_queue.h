/*
 * include/app/encoder_report_queue.h
 * 编码器待发送报告环形队列接口
 * 从 main.c 提取，解决编码器多报告连续发送导致的 TinyUSB 异步缓冲区顺序错误问题
 */
#ifndef APP_ENCODER_REPORT_QUEUE_H
#define APP_ENCODER_REPORT_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 报告类型 ==================== */

/* 编码器待发送报告类型 */
typedef enum {
  ENC_REPORT_KEYBOARD = 0,  /* 键盘报告（8字节: ReportID + Modifier + Reserved + 6Keys） */
  ENC_REPORT_CONSUMER,      /* Consumer Control 报告（2字节 usage） */
  ENC_REPORT_MOUSE,         /* 鼠标报告（使用 tud_hid_mouse_report） */
} enc_report_type_t;

/* ==================== 队列操作 ==================== */

/* 初始化编码器报告队列（清零环形缓冲区） */
void encoder_report_queue_init(void);

/* 入队：键盘/Consumer 通用报告
 * @param type 报告类型（ENC_REPORT_KEYBOARD 或 ENC_REPORT_CONSUMER）
 * @param data 报告数据指针
 * @param length 报告数据长度（最大8字节）
 * @return true=入队成功，false=队列已满或数据过长
 */
bool encoder_report_queue_push_generic(enc_report_type_t type, const uint8_t *data, uint8_t length);

/* 入队：鼠标滚轮报告
 * @param wheel 滚轮值（int8_t）
 * @return true=入队成功，false=队列已满
 */
bool encoder_report_queue_push_mouse_wheel(int8_t wheel);

/* 出队并发送一个报告（每帧调用一次，确保报告间至少间隔1ms）
 * @return true=发送了一个报告，false=队列为空或 USB 未就绪
 */
bool encoder_report_queue_send_one(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_ENCODER_REPORT_QUEUE_H */
