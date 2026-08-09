/*
 * src/app/macro.h
 * 宏功能模块
 * 支持录制和执行按键序列、鼠标动作等
 */

#ifndef MACRO_H
#define MACRO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

#define MACRO_MAX_COUNT       8     /* 最大宏数量 */
#define MACRO_MAX_ACTIONS     32    /* 每个宏最多动作数 */
#define MACRO_NAME_MAX_LEN    16    /* 宏名称最大长度 */

/* ==================== 数据结构 ==================== */

/* 宏动作类型 */
typedef enum {
    MACRO_ACTION_KEY_DOWN = 0,    /* 按键按下 */
    MACRO_ACTION_KEY_UP = 1,      /* 按键释放 */
    MACRO_ACTION_DELAY = 2,       /* 延迟 (ms) */
    MACRO_ACTION_MOUSE_MOVE = 3,  /* 鼠标移动 */
    MACRO_ACTION_MOUSE_CLICK = 4, /* 鼠标点击 */
    MACRO_ACTION_MOUSE_SCROLL = 5 /* 鼠标滚轮 */
} macro_action_type_t;

/* 宏动作 */
typedef struct {
    uint8_t type;       /* 动作类型 macro_action_type_t */
    uint8_t param1;     /* 参数1：键码/鼠标按钮 */
    uint8_t param2;     /* 参数2：延迟低字节 / X偏移低字节 */
    uint8_t param3;     /* 参数3：延迟高字节 / X偏移高字节 / Y偏移 */
} macro_action_t;

/* 宏定义 */
typedef struct {
    uint8_t id;                       /* 宏ID (0 ~ MACRO_MAX_COUNT-1) */
    uint8_t trigger_key;              /* 触发键索引 (0-63, 0xFF表示无触发键) */
    uint8_t repeat_count;             /* 循环次数 (0=无限循环，直到松开触发键) */
    uint8_t action_count;             /* 动作数量 */
    char name[MACRO_NAME_MAX_LEN];    /* 宏名称 */
    macro_action_t actions[MACRO_MAX_ACTIONS]; /* 动作列表 */
} macro_def_t;

/* 宏运行状态 */
typedef enum {
    MACRO_STATE_IDLE = 0,      /* 空闲 */
    MACRO_STATE_RUNNING = 1,   /* 运行中 */
    MACRO_STATE_DELAY = 2      /* 延迟中 */
} macro_state_t;

/* ==================== 对外接口 ==================== */

/* 初始化宏模块 */
void macro_init(void);

/* 宏任务（在主循环中调用） */
void macro_task(void);

/* 获取宏定义 */
const macro_def_t* macro_get(uint8_t macro_id);

/* 设置宏定义 */
int macro_set(uint8_t macro_id, const macro_def_t* macro);

/* 触发宏（按下触发键时调用） */
bool macro_trigger(uint8_t macro_id);

/* 停止宏（松开触发键时调用） */
void macro_stop(uint8_t macro_id);

/* 停止所有宏 */
void macro_stop_all(void);

/* 是否有宏正在运行 */
bool macro_is_running(void);

/* ==================== 状态查询接口 ==================== */

/**
 * @brief 获取宏的键盘状态（用于和物理按键合并）
 * @param out_modifier 输出修饰键状态（可为NULL）
 * @param out_keys 输出6个按键码（可为NULL）
 */
void macro_get_keyboard_state(uint8_t* out_modifier, uint8_t out_keys[6]);

/**
 * @brief 获取宏的鼠标按钮状态
 * @return 鼠标按钮位图
 */
uint8_t macro_get_mouse_buttons(void);

/**
 * @brief 检查并清除键盘状态变化标志
 * @return 是否有变化
 */
bool macro_kb_has_changed(void);

/**
 * @brief 检查并清除鼠标状态变化标志
 * @return 是否有变化
 */
bool macro_mouse_has_changed(void);

/* ==================== 配置持久化接口 ==================== */

/**
 * @brief 从配置字节数组加载宏配置
 * @param data 配置数据指针
 * @param len 数据长度
 * @return 0=成功，其他=失败
 */
int macro_load_from_config(const uint8_t* data, uint32_t len);

/**
 * @brief 保存宏配置到字节数组
 * @param data 输出数据缓冲区指针
 * @param len 缓冲区大小
 * @return 0=成功，其他=失败
 */
int macro_save_to_config(uint8_t* data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* MACRO_H */
