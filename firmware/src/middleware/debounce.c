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
    db->debounce_count = 0;
    db->debounce_threshold = threshold;
}

uint64_t debounce_64key_update(debounce_64key_t *db, uint64_t raw)
{
    if (db == NULL)
    {
        fault_record(FAULT_LEVEL_ERROR, "debounce", "update null pointer");
        return 0xFFFFFFFFFFFFFFFFULL;
    }

    if (raw == db->last_raw)
    {
        if (db->debounce_count < db->debounce_threshold)
        {
            db->debounce_count++;
        }
        else
        {
            db->stable_state = raw;
        }
    }
    else
    {
        db->debounce_count = 0;
        db->last_raw = raw;
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
