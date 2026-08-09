/*
 * include/app/factory_test.h
 * 工厂测试模式
 * 量产测试：自动测试所有硬件，快速检测不良品
 * 测试项：GPIO/SPI/ADC/Flash/USB/编码器/LED/按键等
 */

#ifndef APP_FACTORY_TEST_H
#define APP_FACTORY_TEST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 测试项定义 ==================== */

typedef enum
{
    FACTORY_TEST_SPI_KEYPAD = 0,    /* SPI键盘测试 */
    FACTORY_TEST_PAW3395,           /* PAW3395传感器测试 */
    FACTORY_TEST_JOYSTICK_ADC,      /* 摇杆ADC测试 */
    FACTORY_TEST_ENCODER,           /* 编码器测试 */
    FACTORY_TEST_FLASH,             /* Flash读写测试 */
    FACTORY_TEST_LED,               /* LED测试 */
    FACTORY_TEST_USB,               /* USB连接测试 */
    FACTORY_TEST_COUNT              /* 测试项总数 */
} factory_test_item_t;

/* 测试结果状态 */
typedef enum
{
    FACTORY_TEST_NOT_RUN = 0,   /* 未运行 */
    FACTORY_TEST_PASS,          /* 通过 */
    FACTORY_TEST_FAIL,          /* 失败 */
    FACTORY_TEST_RUNNING        /* 测试中 */
} factory_test_status_t;

/* 单个测试项的结果 */
typedef struct
{
    factory_test_status_t result;   /* 测试结果 */
    uint32_t duration_ms;           /* 测试耗时（毫秒） */
    char detail[64];                /* 详细信息 */
} factory_test_item_result_t;

/* 整体测试结果 */
typedef struct
{
    bool running;                       /* 是否正在测试 */
    uint8_t current_item;               /* 当前测试项 */
    uint8_t pass_count;                 /* 通过项数 */
    uint8_t fail_count;                 /* 失败项数 */
    factory_test_item_result_t items[FACTORY_TEST_COUNT];  /* 各项结果 */
} factory_test_result_t;

/* ==================== 对外接口 ==================== */

/**
 * @brief 初始化工厂测试模块
 */
void factory_test_init(void);

/**
 * @brief 开始运行所有测试
 */
void factory_test_start_all(void);

/**
 * @brief 运行单个测试项
 * @param item 测试项
 * @return 测试状态
 */
factory_test_status_t factory_test_run_item(factory_test_item_t item);

/**
 * @brief 获取整体测试结果
 * @param out_result 输出结果
 */
void factory_test_get_result(factory_test_result_t* out_result);

/**
 * @brief 打印测试结果到串口
 */
void factory_test_print_result(void);

/**
 * @brief 检查是否应该进入工厂测试模式（启动时检测按键等）
 * @return true=进入工厂测试模式
 */
bool factory_test_check_entry(void);

/**
 * @brief 进入工厂测试模式（阻塞，直到测试完成）
 */
void factory_test_enter(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FACTORY_TEST_H */
