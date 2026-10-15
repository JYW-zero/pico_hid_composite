/*
 * src/middleware/keyboard_event.c
 * 按键事件回调机制实现
 */

#include "middleware/keyboard_event.h"
#include "middleware/fault.h"

#include <stddef.h>
#include <string.h>

#include "pico/time.h"

/* ==================== 静态变量 ==================== */

/* 当前按键状态（bit=0 表示按下） */
static uint64_t s_current_state = 0xFFFFFFFFFFFFFFFFULL;

/* 回调注册表 */
typedef struct
{
    bool used;
    keyboard_event_callback_t callback;
    void *user_data;
} callback_entry_t;

static callback_entry_t s_callbacks[KEYBOARD_MAX_CALLBACKS];

/* 模块是否已初始化 */
static bool s_initialized = false;

/* ==================== 内部函数 ==================== */

/* 触发所有回调 */
static void dispatch_event(const keyboard_event_t *event)
{
    for (int i = 0; i < KEYBOARD_MAX_CALLBACKS; i++)
    {
        if (s_callbacks[i].used && s_callbacks[i].callback != NULL)
        {
            s_callbacks[i].callback(event, s_callbacks[i].user_data);
        }
    }
}

/* ==================== 接口实现 ==================== */

void keyboard_event_init(void)
{
    s_current_state = 0xFFFFFFFFFFFFFFFFULL;
    memset(s_callbacks, 0, sizeof(s_callbacks));
    s_initialized = true;
}

int keyboard_event_register(keyboard_event_callback_t callback, void *user_data)
{
    if (!s_initialized)
    {
        keyboard_event_init();
    }

    if (callback == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "keyboard_event", "register null callback");
        return -1;
    }

    /* 查找空闲槽位 */
    for (int i = 0; i < KEYBOARD_MAX_CALLBACKS; i++)
    {
        if (!s_callbacks[i].used)
        {
            s_callbacks[i].used = true;
            s_callbacks[i].callback = callback;
            s_callbacks[i].user_data = user_data;
            return i;
        }
    }

    fault_record(FAULT_LEVEL_ERROR, "keyboard_event", "register failed: callback table full");
    return -2;
}

bool keyboard_event_unregister(int callback_id)
{
    if (callback_id < 0 || callback_id >= KEYBOARD_MAX_CALLBACKS)
    {
        return false;
    }

    if (!s_callbacks[callback_id].used)
    {
        return false;
    }

    s_callbacks[callback_id].used = false;
    s_callbacks[callback_id].callback = NULL;
    s_callbacks[callback_id].user_data = NULL;
    return true;
}

uint32_t keyboard_event_update(uint64_t new_state, bool trigger_hold)
{
    if (!s_initialized)
    {
        keyboard_event_init();
    }

    uint32_t event_count = 0;
    uint64_t timestamp = (uint64_t)to_ms_since_boot(get_absolute_time());

    /* 检测变化的位 */
    uint64_t changed = new_state ^ s_current_state;

    /* 处理按下和释放事件 */
    while (changed)
    {
        /* 找到最低位变化的键 */
        int key_index = __builtin_ctzll(changed);
        changed &= changed - 1;  /* 清除最低位 */

        bool now_pressed = ((new_state >> key_index) & 1ULL) == 0ULL;

        keyboard_event_t event;
        event.key_index = (uint8_t)key_index;
        event.timestamp_ms = timestamp;
        event.current_state = new_state;
        event.type = now_pressed ? KEYBOARD_EVENT_PRESS : KEYBOARD_EVENT_RELEASE;

        dispatch_event(&event);
        event_count++;
    }

    /* 处理保持事件（对所有当前按下的键） */
    if (trigger_hold)
    {
        for (int i = 0; i < KEYBOARD_MAX_KEYS; i++)
        {
            if (((new_state >> i) & 1ULL) == 0ULL)
            {
                keyboard_event_t event;
                event.key_index = (uint8_t)i;
                event.timestamp_ms = timestamp;
                event.current_state = new_state;
                event.type = KEYBOARD_EVENT_HOLD;

                dispatch_event(&event);
                event_count++;
            }
        }
    }

    /* 更新当前状态 */
    s_current_state = new_state;

    return event_count;
}

uint64_t keyboard_event_get_state(void)
{
    return s_current_state;
}

bool keyboard_event_is_pressed(uint8_t key_index)
{
    if (key_index >= KEYBOARD_MAX_KEYS)
    {
        return false;
    }
    return ((s_current_state >> key_index) & 1ULL) == 0ULL;
}

uint8_t keyboard_event_get_pressed_count(void)
{
    /* 计算 bit=0 的数量（按下的键） */
    uint64_t pressed = ~s_current_state;
    return (uint8_t)__builtin_popcountll(pressed);
}
