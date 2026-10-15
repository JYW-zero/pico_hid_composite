/*
 * include/middleware/fault.h
 * Unified Fault Handling Middleware
 * Full version: graded handling + Flash persistent storage + ring buffer
 * Non-volatile across power cycles, facilitates post-mortem troubleshooting
 */

#ifndef MIDDLEWARE_FAULT_H
#define MIDDLEWARE_FAULT_H

#include <stdint.h>
#include <stdbool.h>
#include "board/flash_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Constant Definitions ==================== */

/* Fault level */
typedef enum
{
    FAULT_LEVEL_INFO = 0,   /* Info level, log only */
    FAULT_LEVEL_WARN,       /* Warning level, log and notify */
    FAULT_LEVEL_ERROR,      /* Error level, log and attempt recovery */
    FAULT_LEVEL_FATAL       /* Fatal level, log then reset */
} fault_level_e;

/* Fault log magic: "FLT0" (Fault Log entry 0) */
#define FAULT_LOG_MAGIC     0x30544C46U

/* Single log entry size: 256 bytes (one Flash page, ensures page alignment)
 * Slightly wasteful but reliable, avoids issues from unaligned writes
 */
#define FAULT_LOG_ENTRY_SIZE    FLASH_PAGE_SIZE  /* 256 bytes */

/* Flash log area size: 4KB (one sector) */
#define FAULT_LOG_FLASH_SIZE    FLASH_LAYOUT_SECTOR_SIZE  /* 4096 bytes */

/* Maximum log entries: 4KB / 256 bytes = 16 */
#define FAULT_LOG_MAX_COUNT     (FAULT_LOG_FLASH_SIZE / FAULT_LOG_ENTRY_SIZE)

/*
 * Note: Flash addresses no longer use hardcoded macros!
 * Please use flash_layout_fault_offset() / flash_layout_fault_addr()
 *
 * Layout (from end of Flash backwards):
 *   Last sector 1: Config B area
 *   Last sector 2: Config A area
 *   Last sector 3: Fault log area
 *   Last sector 4: Key statistics area
 */

/* Macros for backward compatibility (deprecated, use functions instead) */
#define FAULT_LOG_FLASH_OFFSET  flash_layout_fault_offset()
#define FAULT_LOG_FLASH_ADDR    flash_layout_fault_addr()

/* ==================== Log Entry Structure ==================== */

/* Fault log entry (full profile, dev)
 * Packed struct size: 252 bytes (header 20B + module 32B + message 200B).
 * Each record is ADDRESSED on a 256-byte Flash page boundary
 * (FAULT_LOG_ENTRY_SIZE = FLASH_PAGE_SIZE = 256), leaving 4 bytes of per-page
 * padding. NEVER index Flash as fault_log_entry_t[] — the 252-byte struct stride
 * would drift against the 256-byte addressing; use base + i*FAULT_LOG_ENTRY_SIZE.
 */
typedef struct __attribute__((packed))
{
    uint32_t magic;          /* Magic 0x30544C46, identifies valid record */
    uint32_t timestamp_ms;   /* Timestamp (milliseconds since boot) */
    uint8_t  level;          /* Fault level fault_level_e */
    uint8_t  module_len;     /* Module name length */
    uint16_t msg_len;        /* Message length */
    uint8_t  reserved[8];    /* Reserved bytes, for alignment */
    char     module[32];     /* Module name (max 31 chars + null terminator) */
    char     msg[200];       /* Error message (max 199 chars + null terminator) */
} fault_log_entry_t;

/* Persisted ABI: packed struct is 252B but each record occupies a 256B page. */
_Static_assert(sizeof(fault_log_entry_t) == 252U,
               "packed fault_log_entry_t must stay 252 bytes (persisted ABI)");
_Static_assert(sizeof(fault_log_entry_t) <= FAULT_LOG_ENTRY_SIZE,
               "fault_log_entry_t must fit one 256-byte Flash page");

/* ==================== Public API ==================== */

/**
 * @brief Initialize fault handling module
 * Load historical logs from Flash, initialize ring buffer
 */
void fault_init(void);

/**
 * @brief Record a fault
 * @param level Fault level
 * @param module Module name (max 15 chars)
 * @param msg Fault message (max 35 chars)
 */
void fault_record(fault_level_e level, const char *module, const char *msg);

/**
 * @brief Get total fault count
 * @return Total fault count
 */
uint32_t fault_get_count(void);

/**
 * @brief Get current log entry count
 * @return Log entry count
 */
uint32_t fault_get_log_count(void);

/**
 * @brief Read log at specified index
 * @param index Index (0=oldest, count-1=newest)
 * @param out_entry Output log entry
 * @return true=success, false=index out of bounds
 */
bool fault_get_log(uint32_t index, fault_log_entry_t* out_entry);

/**
 * @brief Get latest N log entries
 * @param out_entries Output buffer
 * @param max_count Maximum count
 * @return Actual number of entries read
 */
uint32_t fault_get_latest_logs(fault_log_entry_t* out_entries, uint32_t max_count);

/**
 * @brief Clear all fault records (including Flash)
 */
void fault_clear(void);

/**
 * @brief Get level name string
 * @param level Level
 * @return Level name
 */
const char* fault_level_name(fault_level_e level);

/**
 * @brief Thread-safe serial output (multi-core mutex protection)
 * All serial output should prefer this function to avoid Core0/Core1 concurrent
 * printf calls causing interleaved garbled characters.
 * @param fmt Format string
 */
void log_printf(const char* fmt, ...);

/**
 * @brief Lock serial output (for scenarios requiring continuous multi-line output)
 * Must be paired with log_unlock, otherwise deadlock will occur.
 * @return Interrupt state, used for log_unlock restoration
 */
uint32_t log_lock(void);

/**
 * @brief Unlock serial output
 * @param irq_state Interrupt state returned by log_lock
 */
void log_unlock(uint32_t irq_state);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_FAULT_H */
