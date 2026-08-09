/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

enum
{
  REPORT_ID_KEYBOARD = 1,
  REPORT_ID_MOUSE,
  REPORT_ID_CONSUMER_CONTROL,
  REPORT_ID_GAMEPAD,
  REPORT_ID_CONFIG_BLOCK0 = 5,   /* 配置块 0 (偏移 0-62, 63 字节) */
  REPORT_ID_DEVICE_INFO = 6,     /* 设备信息 */
  REPORT_ID_CONTROL = 7,         /* 控制命令 */
  REPORT_ID_CONFIG_BLOCK1 = 8,   /* 配置块 1 (偏移 63-125, 63 字节) */
  REPORT_ID_CONFIG_BLOCK2 = 9,   /* 配置块 2 (偏移 126-188, 63 字节) */
  REPORT_ID_KEY_STATS0 = 10,     /* 按键统计块 0 (键 0~15) */
  REPORT_ID_KEY_STATS1 = 11,     /* 按键统计块 1 (键 16~31) */
  REPORT_ID_KEY_STATS2 = 12,     /* 按键统计块 2 (键 32~47) */
  REPORT_ID_KEY_STATS3 = 13,     /* 按键统计块 3 (键 48~63) */
  REPORT_ID_MACRO_CONFIG = 14,   /* 宏配置读写 */
  REPORT_ID_PERF_SYSTEM = 15,    /* 性能监控 - 系统状态 */
  REPORT_ID_PERF_TASK = 16,      /* 性能监控 - 任务统计 */
  REPORT_ID_FAULT_INFO = 17,     /* 错误日志 - 信息 */
  REPORT_ID_FAULT_LOG = 18,      /* 错误日志 - 读取日志 */
  REPORT_ID_COUNT
};

/* 配置块大小 */
#define CONFIG_BLOCK_SIZE 62
/* 配置总大小 */
#define CONFIG_TOTAL_SIZE 146

#endif /* USB_DESCRIPTORS_H_ */


