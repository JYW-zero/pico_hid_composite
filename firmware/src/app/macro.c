/*
 * src/app/macro.c
 * 宏功能模块实现 - 完整版本
 */

#include "app/macro.h"
#include "protocol/usb_descriptors.h"
#include "tusb.h"
#include "pico/time.h"
#include <string.h>
#include <stdio.h>

/* ==================== 私有变量 ==================== */

/* 宏定义存储（先存在RAM里，后续加Flash持久化） */
static macro_def_t s_macros[MACRO_MAX_COUNT];

/* 当前运行的宏 */
static uint8_t s_current_macro = 0xFF;  /* 0xFF表示无 */
static macro_state_t s_state = MACRO_STATE_IDLE;
static uint16_t s_current_action = 0;
static uint32_t s_delay_end_ms = 0;     /* 延迟结束的时间戳（ms） */
static uint8_t s_repeat_remaining = 0;

/* ==================== 状态管理 ==================== */

/* 键盘状态：宏按下的HID按键码（6KRO） */
static uint8_t s_kb_keys[6] = {0};
static uint8_t s_kb_modifier = 0;
static volatile bool s_kb_changed = false;

/* 鼠标状态：宏按下的鼠标按钮 */
static uint8_t s_mouse_buttons = 0;
static volatile bool s_mouse_changed = false;

/* ==================== 内部函数 ==================== */

/* 加载默认宏（空宏） */
static void load_default_macros(void)
{
    memset(s_macros, 0, sizeof(s_macros));
    for (int i = 0; i < MACRO_MAX_COUNT; i++)
    {
        s_macros[i].id = i;
        s_macros[i].trigger_key = 0xFF;  /* 无触发键 */
        s_macros[i].repeat_count = 1;
        s_macros[i].action_count = 0;
        snprintf(s_macros[i].name, MACRO_NAME_MAX_LEN, "宏%d", i + 1);
    }
}

/* 添加一个按下的键 */
static bool add_key(uint8_t keycode)
{
    /* 修饰键 */
    if (keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT)
    {
        uint8_t bit = 0;
        switch (keycode)
        {
            case HID_KEY_CONTROL_LEFT:  bit = 0; break;
            case HID_KEY_SHIFT_LEFT:    bit = 1; break;
            case HID_KEY_ALT_LEFT:      bit = 2; break;
            case HID_KEY_GUI_LEFT:      bit = 3; break;
            case HID_KEY_CONTROL_RIGHT: bit = 4; break;
            case HID_KEY_SHIFT_RIGHT:   bit = 5; break;
            case HID_KEY_ALT_RIGHT:     bit = 6; break;
            case HID_KEY_GUI_RIGHT:     bit = 7; break;
            default: return false;
        }
        if (!(s_kb_modifier & (1 << bit)))
        {
            s_kb_modifier |= (1 << bit);
            s_kb_changed = true;
        }
        return true;
    }

    /* 普通按键：检查是否已经按下 */
    for (int i = 0; i < 6; i++)
    {
        if (s_kb_keys[i] == keycode)
            return true;  /* 已经按下了 */
    }

    /* 找空槽位 */
    for (int i = 0; i < 6; i++)
    {
        if (s_kb_keys[i] == 0)
        {
            s_kb_keys[i] = keycode;
            s_kb_changed = true;
            return true;
        }
    }

    return false;  /* 6个槽位都满了 */
}

/* 移除一个按下的键 */
static bool remove_key(uint8_t keycode)
{
    /* 修饰键 */
    if (keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT)
    {
        uint8_t bit = 0;
        switch (keycode)
        {
            case HID_KEY_CONTROL_LEFT:  bit = 0; break;
            case HID_KEY_SHIFT_LEFT:    bit = 1; break;
            case HID_KEY_ALT_LEFT:      bit = 2; break;
            case HID_KEY_GUI_LEFT:      bit = 3; break;
            case HID_KEY_CONTROL_RIGHT: bit = 4; break;
            case HID_KEY_SHIFT_RIGHT:   bit = 5; break;
            case HID_KEY_ALT_RIGHT:     bit = 6; break;
            case HID_KEY_GUI_RIGHT:     bit = 7; break;
            default: return false;
        }
        if (s_kb_modifier & (1 << bit))
        {
            s_kb_modifier &= ~(1 << bit);
            s_kb_changed = true;
        }
        return true;
    }

    /* 普通按键 */
    for (int i = 0; i < 6; i++)
    {
        if (s_kb_keys[i] == keycode)
        {
            s_kb_keys[i] = 0;
            s_kb_changed = true;
            return true;
        }
    }

    return false;  /* 没找到这个键 */
}

/* 释放所有宏按下的键（停止宏时调用） */
static void release_all_keys(void)
{
    if (s_kb_modifier != 0 || s_kb_keys[0] != 0 || s_kb_keys[1] != 0 ||
        s_kb_keys[2] != 0 || s_kb_keys[3] != 0 || s_kb_keys[4] != 0 || s_kb_keys[5] != 0)
    {
        s_kb_modifier = 0;
        memset(s_kb_keys, 0, sizeof(s_kb_keys));
        s_kb_changed = true;
    }

    if (s_mouse_buttons != 0)
    {
        s_mouse_buttons = 0;
        s_mouse_changed = true;
    }
}

/* 执行单个动作 */
static bool execute_action(const macro_action_t* action)
{
    switch (action->type)
    {
        case MACRO_ACTION_KEY_DOWN:
        {
            uint8_t keycode = action->param1;
            add_key(keycode);
            printf("[宏] 按键按下: 0x%02X\n", keycode);
            break;
        }

        case MACRO_ACTION_KEY_UP:
        {
            uint8_t keycode = action->param1;
            remove_key(keycode);
            printf("[宏] 按键释放: 0x%02X\n", keycode);
            break;
        }

        case MACRO_ACTION_DELAY:
        {
            /* 延迟动作：计算结束时间戳，进入延迟状态 */
            uint16_t delay = action->param2 | (action->param3 << 8);
            s_delay_end_ms = to_ms_since_boot(get_absolute_time()) + delay;
            s_state = MACRO_STATE_DELAY;
            return false;  /* 返回false，表示需要等待 */
        }

        case MACRO_ACTION_MOUSE_MOVE:
        {
            /* 鼠标移动：相对移动，直接发送报告 */
            int8_t dx = (int8_t)action->param2;
            int8_t dy = (int8_t)action->param3;
            if (tud_hid_ready())
            {
                tud_hid_mouse_report(REPORT_ID_MOUSE, s_mouse_buttons, dx, dy, 0, 0);
            }
            printf("[宏] 鼠标移动: dx=%d, dy=%d\n", dx, dy);
            break;
        }

        case MACRO_ACTION_MOUSE_CLICK:
        {
            /* 鼠标点击：按下+释放 */
            uint8_t button = action->param1;
            s_mouse_buttons |= button;
            s_mouse_changed = true;
            /* 注意：点击需要按下和释放两个动作分开，这里只处理按下或释放，由param2决定 */
            if (action->param2 != 0)
            {
                /* 按下 */
                printf("[宏] 鼠标按下: 0x%02X\n", button);
            }
            else
            {
                /* 释放 */
                s_mouse_buttons &= ~button;
                printf("[宏] 鼠标释放: 0x%02X\n", button);
            }
            break;
        }

        case MACRO_ACTION_MOUSE_SCROLL:
        {
            /* 鼠标滚轮 */
            int8_t scroll = (int8_t)action->param1;
            if (tud_hid_ready())
            {
                tud_hid_mouse_report(REPORT_ID_MOUSE, s_mouse_buttons, 0, 0, scroll, 0);
            }
            printf("[宏] 鼠标滚轮: %d\n", scroll);
            break;
        }

        default:
            break;
    }

    return true;  /* 返回true，表示动作执行完成，可以继续下一个 */
}

/* ==================== 对外接口 ==================== */

void macro_init(void)
{
    /* TODO: 从Flash加载宏配置 */
    load_default_macros();
    printf("[宏] 模块初始化完成，共 %d 个宏槽位\n", MACRO_MAX_COUNT);
}

void macro_task(void)
{
    if (s_state == MACRO_STATE_IDLE)
    {
        return;
    }

    if (s_current_macro >= MACRO_MAX_COUNT)
    {
        s_state = MACRO_STATE_IDLE;
        return;
    }

    const macro_def_t* macro = &s_macros[s_current_macro];

    if (s_state == MACRO_STATE_DELAY)
    {
        /* 延迟中：检查时间是否到了 */
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now >= s_delay_end_ms)
        {
            /* 延迟结束，继续下一个动作 */
            s_state = MACRO_STATE_RUNNING;
            s_current_action++;
        }
        else
        {
            return;
        }
    }

    if (s_state == MACRO_STATE_RUNNING)
    {
        /* 执行当前动作 */
        if (s_current_action < macro->action_count)
        {
            const macro_action_t* action = &macro->actions[s_current_action];
            bool done = execute_action(action);

            if (done)
            {
                /* 动作执行完成，继续下一个 */
                s_current_action++;
            }
            /* 如果没执行完（比如延迟），就等下一次调用 */
        }
        else
        {
            /* 所有动作执行完了 */
            if (macro->repeat_count == 0)
            {
                /* 无限循环：重新开始 */
                s_current_action = 0;
            }
            else if (s_repeat_remaining > 1)
            {
                /* 还有循环次数：重新开始 */
                s_repeat_remaining--;
                s_current_action = 0;
            }
            else
            {
                /* 循环结束，停止 */
                printf("[宏] 宏 %d 执行完成\n", s_current_macro);
                release_all_keys();
                s_state = MACRO_STATE_IDLE;
                s_current_macro = 0xFF;
            }
        }
    }
}

const macro_def_t* macro_get(uint8_t macro_id)
{
    if (macro_id >= MACRO_MAX_COUNT)
    {
        return NULL;
    }
    return &s_macros[macro_id];
}

int macro_set(uint8_t macro_id, const macro_def_t* macro)
{
    if (macro_id >= MACRO_MAX_COUNT || macro == NULL)
    {
        return -1;
    }

    /* 如果正在运行这个宏，先停止 */
    if (s_current_macro == macro_id)
    {
        macro_stop_all();
    }

    /* 复制宏定义，确保ID正确 */
    memcpy(&s_macros[macro_id], macro, sizeof(macro_def_t));
    s_macros[macro_id].id = macro_id;

    /* 限制动作数量 */
    if (s_macros[macro_id].action_count > MACRO_MAX_ACTIONS)
    {
        s_macros[macro_id].action_count = MACRO_MAX_ACTIONS;
    }

    /* TODO: 保存到Flash */

    printf("[宏] 保存宏 %d: %s, %d 个动作\n",
           macro_id, s_macros[macro_id].name,
           s_macros[macro_id].action_count);

    return 0;
}

bool macro_trigger(uint8_t macro_id)
{
    if (macro_id >= MACRO_MAX_COUNT)
    {
        return false;
    }

    if (s_state != MACRO_STATE_IDLE)
    {
        /* 已经有宏在运行，先停止 */
        macro_stop_all();
    }

    if (s_macros[macro_id].action_count == 0)
    {
        return false;  /* 空宏，不执行 */
    }

    s_current_macro = macro_id;
    s_state = MACRO_STATE_RUNNING;
    s_current_action = 0;
    s_repeat_remaining = s_macros[macro_id].repeat_count;

    printf("[宏] 触发宏 %d: %s\n", macro_id, s_macros[macro_id].name);

    return true;
}

void macro_stop(uint8_t macro_id)
{
    if (s_current_macro == macro_id)
    {
        macro_stop_all();
    }
}

void macro_stop_all(void)
{
    if (s_state != MACRO_STATE_IDLE)
    {
        printf("[宏] 停止所有宏\n");
        release_all_keys();
    }

    s_state = MACRO_STATE_IDLE;
    s_current_macro = 0xFF;
    s_current_action = 0;
    s_delay_end_ms = 0;
    s_repeat_remaining = 0;
}

bool macro_is_running(void)
{
    return s_state != MACRO_STATE_IDLE;
}

/* ==================== 状态查询接口 ==================== */

/* 获取宏的键盘状态（用于和物理按键合并） */
void macro_get_keyboard_state(uint8_t* out_modifier, uint8_t out_keys[6])
{
    if (out_modifier) *out_modifier = s_kb_modifier;
    if (out_keys) memcpy(out_keys, s_kb_keys, 6);
}

/* 获取宏的鼠标按钮状态 */
uint8_t macro_get_mouse_buttons(void)
{
    return s_mouse_buttons;
}

/* 检查并清除键盘状态变化标志 */
bool macro_kb_has_changed(void)
{
    bool changed = s_kb_changed;
    s_kb_changed = false;
    return changed;
}

/* 检查并清除鼠标状态变化标志 */
bool macro_mouse_has_changed(void)
{
    bool changed = s_mouse_changed;
    s_mouse_changed = false;
    return changed;
}

/* ==================== 配置持久化接口 ==================== */

int macro_load_from_config(const uint8_t* data, uint32_t len)
{
    if (data == NULL || len < sizeof(macro_def_t))
    {
        return -1;
    }

    /* 计算可以加载的宏数量 */
    uint32_t macro_count = len / sizeof(macro_def_t);
    if (macro_count > MACRO_MAX_COUNT)
    {
        macro_count = MACRO_MAX_COUNT;
    }

    /* 逐个加载 */
    for (uint32_t i = 0; i < macro_count; i++)
    {
        const macro_def_t* src = (const macro_def_t*)(data + i * sizeof(macro_def_t));

        /* 基本验证：动作数量不能超过最大值 */
        if (src->action_count > MACRO_MAX_ACTIONS)
        {
            /* 无效，跳过，保持默认值 */
            continue;
        }

        /* 复制 */
        memcpy(&s_macros[i], src, sizeof(macro_def_t));
        s_macros[i].id = (uint8_t)i;  /* 确保ID正确 */
    }

    printf("[宏] 从配置加载了 %d 个宏\n", macro_count);
    return 0;
}

int macro_save_to_config(uint8_t* data, uint32_t len)
{
    if (data == NULL || len < sizeof(macro_def_t))
    {
        return -1;
    }

    /* 计算可以保存的宏数量 */
    uint32_t macro_count = len / sizeof(macro_def_t);
    if (macro_count > MACRO_MAX_COUNT)
    {
        macro_count = MACRO_MAX_COUNT;
    }

    /* 逐个保存 */
    for (uint32_t i = 0; i < macro_count; i++)
    {
        macro_def_t* dst = (macro_def_t*)(data + i * sizeof(macro_def_t));
        memcpy(dst, &s_macros[i], sizeof(macro_def_t));
    }

    return 0;
}
