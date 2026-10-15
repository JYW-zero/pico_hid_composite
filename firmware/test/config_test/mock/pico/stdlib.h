/* Mock pico/stdlib.h */
#ifndef PICO_STDLIB_H
#define PICO_STDLIB_H

#include <stdint.h>

typedef uint64_t absolute_time_t;

/* 外部变量，由测试文件定义 */
extern uint64_t g_mock_time_ms;

static inline absolute_time_t get_absolute_time(void) { return (absolute_time_t)g_mock_time_ms; }
static inline uint64_t to_ms_since_boot(absolute_time_t t) { (void)t; return g_mock_time_ms; }

#endif
