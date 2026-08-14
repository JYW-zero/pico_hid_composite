/*
 * src/middleware/flash_service.c
 * Flash安全写入服务实现（双核同步）
 *
 * 使用Pico SDK官方的flash_safe_execute()机制，
 * 内部自动处理：暂停Core1 → 禁用中断 → 执行擦写 → 恢复中断 → 恢复Core1
 *
 * 注意：本模块是底层模块，禁止调用fault_record()，
 * 因为fault_record在ERROR级别会写Flash，会导致无限递归。
 */

#include "middleware/flash_service.h"

#include "pico/flash.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ==================== 内部类型 ==================== */

typedef struct {
    uint32_t offset;
    const uint8_t *data;
    uint32_t size;
} flash_op_ctx_t;

/* ==================== RAM驻留回调函数 ==================== */

/* 擦除回调：必须在RAM中执行，禁止访问Flash */
static void __no_inline_not_in_flash_func(flash_erase_callback)(void *param)
{
    flash_op_ctx_t *ctx = (flash_op_ctx_t *)param;
    flash_range_erase(ctx->offset, ctx->size);
    flash_flush_cache();
}

/* 编程回调：必须在RAM中执行，禁止访问Flash */
static void __no_inline_not_in_flash_func(flash_program_callback)(void *param)
{
    flash_op_ctx_t *ctx = (flash_op_ctx_t *)param;
    uint32_t offset = ctx->offset;
    const uint8_t *data = ctx->data;
    uint32_t remaining = ctx->size;

    /* 按页写入，每页256字节 */
    while (remaining > 0)
    {
        uint32_t chunk = (remaining > FLASH_PAGE_SIZE) ? FLASH_PAGE_SIZE : remaining;
        flash_range_program(offset, data, chunk);
        offset += chunk;
        data += chunk;
        remaining -= chunk;

        /* 每页写完喂一次狗，防止看门狗超时 */
        watchdog_update();
    }

    flash_flush_cache();
}

/* 擦除+编程回调：先擦除整个扇区，再写入 */
static void __no_inline_not_in_flash_func(flash_write_sector_callback)(void *param)
{
    flash_op_ctx_t *ctx = (flash_op_ctx_t *)param;

    /* 先擦除整个扇区 */
    flash_range_erase(ctx->offset, FLASH_SECTOR_SIZE);

    /* 按页写入 */
    uint32_t offset = ctx->offset;
    const uint8_t *data = ctx->data;
    uint32_t remaining = FLASH_SECTOR_SIZE;

    while (remaining > 0)
    {
        uint32_t chunk = (remaining > FLASH_PAGE_SIZE) ? FLASH_PAGE_SIZE : remaining;
        flash_range_program(offset, data, chunk);
        offset += chunk;
        data += chunk;
        remaining -= chunk;

        watchdog_update();
    }

    flash_flush_cache();
}

/* ==================== 对外接口 ==================== */

void flash_service_init(void)
{
    /* Core0端不需要特殊初始化，flash_safe_execute会自动处理 */
}

void flash_service_core1_init(void)
{
    /* 在Core1中注册，允许被flash_safe_execute锁定 */
    flash_safe_execute_core_init();
}

bool flash_service_erase(uint32_t flash_offset, uint32_t size)
{
    /* 参数校验 */
    if ((flash_offset & (FLASH_SECTOR_SIZE - 1)) != 0)
    {
        return false;
    }
    if ((size & (FLASH_SECTOR_SIZE - 1)) != 0 || size == 0)
    {
        return false;
    }

    flash_op_ctx_t ctx = {
        .offset = flash_offset,
        .data = NULL,
        .size = size
    };

    int rc = flash_safe_execute(flash_erase_callback, &ctx, 5000);
    return (rc == PICO_OK);
}

bool flash_service_program(uint32_t flash_offset, const uint8_t *data, uint32_t size)
{
    /* 参数校验 */
    if (data == NULL)
    {
        return false;
    }
    if ((flash_offset & (FLASH_PAGE_SIZE - 1)) != 0)
    {
        return false;
    }
    if ((size & (FLASH_PAGE_SIZE - 1)) != 0 || size == 0)
    {
        return false;
    }

    flash_op_ctx_t ctx = {
        .offset = flash_offset,
        .data = data,
        .size = size
    };

    int rc = flash_safe_execute(flash_program_callback, &ctx, 5000);
    return (rc == PICO_OK);
}

bool flash_service_write_sector(uint32_t flash_offset, const uint8_t *data)
{
    if (data == NULL)
    {
        return false;
    }
    if ((flash_offset & (FLASH_SECTOR_SIZE - 1)) != 0)
    {
        return false;
    }

    flash_op_ctx_t ctx = {
        .offset = flash_offset,
        .data = data,
        .size = FLASH_SECTOR_SIZE
    };

    int rc = flash_safe_execute(flash_write_sector_callback, &ctx, 5000);
    return (rc == PICO_OK);
}
