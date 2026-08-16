/*
 * src/middleware/debounce.c
 * 按键消抖中间件实现
 */

#include "middleware/debounce.h"
#include "middleware/fault.h"
#include <stddef.h>

void debounce_64key_init(debounce_64key_t *db, uint8_t threshold)
{
    if (db == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "debounce", "init null pointer");
        return;
    }

    db->last_raw = 0xFFFFFFFFFFFFFFFFULL;
    db->stable_state = 0xFFFFFFFFFFFFFFFFULL;
    db->debounce_threshold = threshold;

    /* 初始化所有按键计数器为0 */
    for (int i = 0; i < 64; i++)
    {
        db->count[i] = 0;
    }
}

uint64_t debounce_64key_update(debounce_64key_t *db, uint64_t raw)
{
    if (db == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "debounce", "update null pointer");
        return 0xFFFFFFFFFFFFFFFFULL;
    }

    /* 找出哪些位发生了变化 */
    uint64_t changed = raw ^ db->last_raw;
    db->last_raw = raw;

    /* 逐位独立消抖 */
    for (int i = 0; i < 64; i++)
    {
        if ((changed >> i) & 1ULL)
        {
            /* 该位变化，重置计数器为1（当前采样算第一次） */
            db->count[i] = 1;
        }
        else if (db->count[i] < db->debounce_threshold)
        {
            /* 该位未变化但未达到阈值，累加计数器 */
            db->count[i]++;
        }
        else
        {
            /* 该位已稳定，更新稳定状态 */
            uint64_t bit = (raw >> i) & 1ULL;
            if (bit)
            {
                db->stable_state |= (1ULL << i);
            }
            else
            {
                db->stable_state &= ~(1ULL << i);
            }
        }
    }

    return db->stable_state;
}

uint64_t debounce_64key_get(const debounce_64key_t *db)
{
    if (db == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "debounce", "get null pointer");
        return 0xFFFFFFFFFFFFFFFFULL;
    }
    return db->stable_state;
}
