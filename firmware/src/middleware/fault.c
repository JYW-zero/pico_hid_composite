/*
 * src/middleware/fault.c
 * Unified Fault Handling Middleware - common framework
 *
 * 本文件只保留与“落盘形态”无关的公共逻辑：
 *   - 多核串口互斥（log_printf / log_lock / log_unlock）
 *   - 分级处理、Core1 串口限频、ERROR Flash 写限频
 *   - 对外公共 API（fault_record / fault_get_log / fault_clear ...）
 * 具体的 Flash 持久化由后端实现，见 fault_backend.h：
 *   - dev  全量档（fault_full.c）   ：256B/条文本日志
 *   - release 紧凑档（fault_compact.c）：16B/条环形 + 磨损均衡
 */

#include "middleware/fault.h"
#include "middleware/fault_backend.h"
#include "board/flash_layout.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>

#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/time.h"
#include "pico/platform.h"

/* ==================== Constant Definitions ==================== */

/* Flash write rate limit interval for ERROR level logs (milliseconds)
 * Prevents frequent errors from causing Flash write storms, FATAL level is not rate-limited
 * 100ms interval limits worst-case write frequency to 10 times/second
 */
#define FAULT_ERROR_RATE_LIMIT_MS  (100U)

/* Core1 serial output rate limit interval (milliseconds) */
#define FAULT_CORE1_LOG_RATE_LIMIT_MS (500U)

/* ==================== Serial Output Mutex ==================== */
/* In multi-core environment, Core0 and Core1 may call printf simultaneously, causing interleaved garbled characters.
 * Use spin_lock to protect all serial output, ensuring each log is output completely.
 */
static spin_lock_t *s_log_spinlock = NULL;
static uint32_t     s_log_spinlock_num = 0;
static bool         s_log_lock_inited = false;

static void log_lock_init(void)
{
    if (s_log_lock_inited) return;
    s_log_spinlock_num = spin_lock_claim_unused(true);
    s_log_spinlock = spin_lock_init(s_log_spinlock_num);
    s_log_lock_inited = true;
}

uint32_t log_lock(void)
{
    if (!s_log_lock_inited) return 0;
    return spin_lock_blocking(s_log_spinlock);
}

void log_unlock(uint32_t irq_state)
{
    if (!s_log_lock_inited) return;
    spin_unlock(s_log_spinlock, irq_state);
}

void log_printf(const char* fmt, ...)
{
    va_list args;
    uint32_t irq = log_lock();
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    log_unlock(irq);
}

/* ==================== Static Variables ==================== */

static uint32_t s_total_count = 0;     /* Total fault count (including non-persisted) */
static bool     s_initialized = false;

/* ==================== Public API ==================== */

void fault_init(void)
{
    /* Initialize serial output mutex first (must be before any printf) */
    log_lock_init();

    /* Ensure Flash layout is initialized (detect Flash size) */
    flash_layout_init();

    /* Initialize persistence backend (scan history; compact backend also appends boot event) */
    fault_backend_init();

    s_initialized = true;
    s_total_count = fault_backend_get_log_count();  /* Initial count equals visible log count */

    log_printf("[INFO] fault: init complete, history logs: %d\n", (int)s_total_count);
}

void fault_record(fault_level_e level, const char *module, const char *msg)
{
    s_total_count++;

    /* Serial output with correct level prefix.
     * Core1 rate limiting: ERROR/WARN at most once per 500ms to avoid serial storm;
     * FATAL is never rate-limited and is always printed.
     */
    const char *level_str = fault_level_name(level);
    if (get_core_num() == 1 && level < FAULT_LEVEL_FATAL)
    {
        static uint32_t s_core1_last_log_ms = 0;
        uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
        if ((now_ms - s_core1_last_log_ms) < FAULT_CORE1_LOG_RATE_LIMIT_MS)
        {
            /* Core1 rate limited: skip serial; Core1 never writes Flash. */
            return;
        }
        s_core1_last_log_ms = now_ms;
    }
    log_printf("[%s] %s: %s\n", level_str, module ? module : "?", msg ? msg : "?");

    /* If not initialized, serial only, no Flash */
    if (!s_initialized)
    {
        return;
    }

    if (module == NULL) module = "unknown";
    if (msg == NULL) msg = "unknown";

    /* Only Core0 writes Flash, avoiding concurrent flash_safe_execute deadlock. */
    if (level >= FAULT_LEVEL_ERROR && get_core_num() == 0)
    {
        /* Global rate limit for ERROR Flash writes; FATAL is never limited. */
        static uint32_t s_last_flash_write_ms = 0;
        uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());

        if (level < FAULT_LEVEL_FATAL && (now_ms - s_last_flash_write_ms) < FAULT_ERROR_RATE_LIMIT_MS)
        {
            return;
        }
        s_last_flash_write_ms = now_ms;

        fault_backend_record(level, module, msg);
    }

    /* Fatal error: force-flush pending records, then watchdog reset (10ms, enough to finish writing) */
    if (level == FAULT_LEVEL_FATAL)
    {
        fault_backend_flush();
        watchdog_reboot(0, 0, 10);
    }
}

uint32_t fault_get_count(void)
{
    return s_total_count;
}

uint32_t fault_get_log_count(void)
{
    return fault_backend_get_log_count();
}

bool fault_get_log(uint32_t index, fault_log_entry_t* out_entry)
{
    if (out_entry == NULL)
    {
        return false;
    }
    return fault_backend_get_log(index, out_entry);
}

uint32_t fault_get_latest_logs(fault_log_entry_t* out_entries, uint32_t max_count)
{
    if (out_entries == NULL || max_count == 0)
    {
        return 0;
    }

    uint32_t count = (max_count < fault_backend_get_log_count()) ? max_count : fault_backend_get_log_count();

    /* Newest entries are at the end, copy in reverse order */
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t log_count = fault_backend_get_log_count();
        uint32_t src_index = log_count - 1 - i;
        fault_backend_get_log(src_index, &out_entries[i]);
    }

    return count;
}

void fault_clear(void)
{
    fault_backend_clear();
    s_total_count = 0;
}

const char* fault_level_name(fault_level_e level)
{
    switch (level)
    {
        case FAULT_LEVEL_INFO:  return "INFO";
        case FAULT_LEVEL_WARN:  return "WARN";
        case FAULT_LEVEL_ERROR: return "ERROR";
        case FAULT_LEVEL_FATAL: return "FATAL";
        default:                return "UNKNOWN";
    }
}
