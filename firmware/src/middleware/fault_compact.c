/*
 * src/middleware/fault_compact.c
 * Fault 持久化后端 - 精简档（release / FEATURE_FAULT_COMPACT=1）
 *
 * 设计（见 docs/FEATURE_ROLE_SPLIT_PLAN.md §8.3）：
 *   - 定长 16B 紧凑记录：code/level/module_id/boot_count/timestamp/occurrences/crc16
 *   - 按页攒批：一页 256B 放 16 条，攒满才编程；每页只编程一次（不做同页多次编程）
 *   - FATAL / 异常复位前 seal：立即编程当前页，页内空位以 PAD 记录填充保持物理连续
 *   - A/B 双扇区环形：每扇区 256 条，写满后把最近 128 条搬到另一扇区再切换
 *   - 每条 CRC16，扫描遇空槽(0xFFFF)/损坏即止，掉电只可能丢未满页的非致命记录
 *   - 开机追加一条 BOOT 事件承载 boot_count（无 RTC 时间基准）与复位原因
 *   - 对外只暴露最近 COMPACT_VIEW_COUNT(32) 条；读出时还原为公共 fault_log_entry_t，
 *     因此 HID 报告与上位机 ErrorLog 页无需改动。
 *
 * 容量/寿命（相对全量档）：单扇区 16 -> 256 条，单位扇区擦除间隔约提升 16 倍。
 */

#include "features.h"

#if FEATURE_FAULT_COMPACT

#include "middleware/fault.h"
#include "middleware/fault_backend.h"
#include "middleware/flash_service.h"
#include "board/flash_layout.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "pico/time.h"

/* ==================== 常量 ==================== */

#define COMPACT_REC_SIZE       16U
#define COMPACT_PER_PAGE       (FLASH_PAGE_SIZE / COMPACT_REC_SIZE)   /* 16 条/页 */
#define COMPACT_PAGES          (FAULT_LOG_FLASH_SIZE / FLASH_PAGE_SIZE) /* 16 页/扇区 */
#define COMPACT_PER_SECTOR     (COMPACT_PER_PAGE * COMPACT_PAGES)     /* 256 条/扇区 */
#define COMPACT_KEEP_ON_SWITCH (COMPACT_PER_SECTOR / 2U)              /* 切换保留 128 条 */
#define COMPACT_VIEW_COUNT     32U                                   /* 对外最近窗口 */
#define COMPACT_CRC_LEN        14U                                   /* CRC 覆盖前 14 字节 */

/* 空槽标记（擦除态 code 字段为 0xFFFF）；PAD 为 seal 页填充占位 */
#define CODE_BLANK             0xFFFFU
#define CODE_PAD               0xFFFEU

/* 故障码 / 启动事件码 */
enum
{
    CODE_BOOT_POWERON  = 0x0000,
    CODE_BOOT_WATCHDOG = 0x0001,
    CODE_HARD_FAULT    = 0x0010,
    CODE_WATCHDOG      = 0x0011,
    CODE_ASSERT        = 0x0012,
    CODE_FLASH_ERROR   = 0x0013,
    CODE_IPC_TIMEOUT   = 0x0014,
    CODE_CRC_ERROR     = 0x0015,
    CODE_USB_ERROR     = 0x0016,
    CODE_GENERIC_WARN  = 0x001E,
    CODE_GENERIC_ERROR = 0x001F,
    CODE_GENERIC_FATAL = 0x002F
};

/* 模块 ID（紧凑档只存 ID，还原时查静态字符串表） */
enum
{
    MOD_SYSTEM = 0, MOD_FLASH, MOD_WATCHDOG, MOD_CORE1, MOD_USB,
    MOD_CONFIG, MOD_KEYPAD, MOD_OPTICAL, MOD_JOYSTICK, MOD_ENCODER,
    MOD_FACTORY, MOD_MACRO, MOD_POWER, MOD_SCHEDULER, MOD_IPC, MOD_OTHER
};

/* ==================== 紧凑记录（16B，packed） ==================== */

typedef struct __attribute__((packed))
{
    uint16_t code;         /* 故障码/启动事件码 */
    uint8_t  level;        /* fault_level_e */
    uint8_t  module_id;    /* 模块枚举 ID */
    uint32_t boot_count;   /* 上电序号 */
    uint32_t timestamp_ms; /* 本次上电内毫秒 */
    uint16_t occurrences;  /* 同类合并计数 */
    uint16_t crc16;        /* CRC16-CCITT，覆盖前 14 字节 */
} compact_rec_t;

_Static_assert(sizeof(compact_rec_t) == COMPACT_REC_SIZE, "compact record must be 16 bytes");

/* ==================== 静态状态 ==================== */

static uint8_t  s_active = 0;      /* 活动扇区 0=A 1=B */
static uint32_t s_committed = 0;   /* 已整页编程的记录槽数（含 PAD，恒为页界） */
static uint32_t s_valid = 0;       /* 有效事件总数（不含 PAD，含当前 RAM 页） */
static uint8_t  s_page_slot = 0;   /* 当前 RAM 页内已缓冲的有效条数 0..15 */
static uint32_t s_boot = 1;        /* 本次上电序号 */
static uint8_t  s_page_buf[FLASH_PAGE_SIZE]; /* 当前页 RAM 镜像（256B） */

/* 扇区切换时搬运缓冲：128 条 × 16B = 2KB，static 避免占用栈 */
static compact_rec_t s_keep[COMPACT_KEEP_ON_SWITCH];

/* ==================== 扇区地址 ==================== */

static uint32_t active_offset(void)
{
    return (s_active == 0) ? flash_layout_fault_a_offset() : flash_layout_fault_b_offset();
}
/* 只读基指针：固件为 XIP 地址，host 测试由 mock 重定向到 RAM 模拟 Flash */
static const uint8_t* active_ptr(void)
{
    return (const uint8_t*)(uintptr_t)(flash_layout_xip_base() + active_offset());
}

/* ==================== CRC16-CCITT ==================== */

static uint16_t crc16_ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFFU;
    while (len--)
    {
        crc ^= (uint16_t)((uint16_t)(*data++) << 8);
        for (uint8_t i = 0; i < 8; i++)
        {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void rec_finalize(compact_rec_t* r)
{
    r->crc16 = crc16_ccitt((const uint8_t*)r, COMPACT_CRC_LEN);
}

static bool rec_is_blank(const compact_rec_t* r)
{
    return r->code == CODE_BLANK;
}

/* PAD 占位记录（seal 页填充，非真实事件） */
static void rec_make_pad(compact_rec_t* r)
{
    memset(r, 0, sizeof(*r));
    r->code = CODE_PAD;
    rec_finalize(r);
}

/* 有效（真实事件，CRC 正确）；PAD 返回 false 但也要求 CRC 正确 */
static bool rec_is_event(const compact_rec_t* r)
{
    if (r->code == CODE_BLANK || r->code == CODE_PAD) return false;
    return r->crc16 == crc16_ccitt((const uint8_t*)r, COMPACT_CRC_LEN);
}
static bool rec_is_pad_ok(const compact_rec_t* r)
{
    return r->code == CODE_PAD && r->crc16 == crc16_ccitt((const uint8_t*)r, COMPACT_CRC_LEN);
}

/* ==================== 分类表（文本 -> code / module_id） ==================== */

static const char* const s_module_names[16] =
{
    "system", "flash", "watchdog", "core1", "usb",
    "config", "keypad", "optical", "joystick", "encoder",
    "factory", "macro", "power", "scheduler", "ipc", "other"
};

/* 大小写不敏感子串（needle 必须为小写 ASCII；haystack 可含 UTF-8 中文） */
static bool contains_ci(const char* hay, const char* needle)
{
    if (hay == NULL || needle == NULL || needle[0] == '\0') return false;
    for (size_t i = 0; hay[i] != '\0'; i++)
    {
        size_t h = i, n = 0;
        while (needle[n] != '\0')
        {
            char ch = hay[h];
            if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
            if (ch != needle[n]) break;
            h++; n++;
        }
        if (needle[n] == '\0') return true;
    }
    return false;
}

static bool hit(const char* module, const char* msg, const char* kw)
{
    return contains_ci(module, kw) || contains_ci(msg, kw);
}

static uint8_t classify_module(const char* module, const char* msg)
{
    static const struct { const char* kw; uint8_t id; } tab[] =
    {
        {"watchdog",  MOD_WATCHDOG}, {"wdt",       MOD_WATCHDOG},
        {"flash",     MOD_FLASH},
        {"multicore", MOD_CORE1},    {"core",      MOD_CORE1},
        {"ipc",       MOD_IPC},
        {"tinyusb",   MOD_USB},      {"usb",       MOD_USB},
        {"config",    MOD_CONFIG},
        {"keypad",    MOD_KEYPAD},   {"keyboard",  MOD_KEYPAD}, {"key", MOD_KEYPAD},
        {"optical",   MOD_OPTICAL},  {"mouse",     MOD_OPTICAL}, {"sensor", MOD_OPTICAL},
        {"joystick",  MOD_JOYSTICK}, {"joy",       MOD_JOYSTICK}, {"adc", MOD_JOYSTICK},
        {"encoder",   MOD_ENCODER},  {"enc",       MOD_ENCODER},
        {"factory",   MOD_FACTORY},
        {"macro",     MOD_MACRO},
        {"sleep",     MOD_POWER},    {"power",     MOD_POWER},
        {"sched",     MOD_SCHEDULER},{"task",      MOD_SCHEDULER}
    };
    for (size_t i = 0; i < sizeof(tab) / sizeof(tab[0]); i++)
    {
        if (hit(module, msg, tab[i].kw)) return tab[i].id;
    }
    return MOD_OTHER;
}

static uint16_t classify_code(fault_level_e level, const char* module, const char* msg)
{
    if (hit(module, msg, "hardfault") || hit(module, msg, "hard fault")) return CODE_HARD_FAULT;
    if (hit(module, msg, "assert"))      return CODE_ASSERT;
    if (hit(module, msg, "watchdog") || hit(module, msg, "wdt")) return CODE_WATCHDOG;
    if (hit(module, msg, "crc") || hit(module, msg, "checksum")) return CODE_CRC_ERROR;
    if (hit(module, msg, "ipc") || hit(module, msg, "timeout"))  return CODE_IPC_TIMEOUT;
    if (hit(module, msg, "flash"))      return CODE_FLASH_ERROR;
    if (hit(module, msg, "usb") || hit(module, msg, "enumer"))  return CODE_USB_ERROR;

    switch (level)
    {
        case FAULT_LEVEL_FATAL: return CODE_GENERIC_FATAL;
        case FAULT_LEVEL_ERROR: return CODE_GENERIC_ERROR;
        case FAULT_LEVEL_WARN:  return CODE_GENERIC_WARN;
        default:                return CODE_GENERIC_WARN;
    }
}

static const char* code_desc(uint16_t code)
{
    switch (code)
    {
        case CODE_BOOT_POWERON:  return "power-on reset";
        case CODE_BOOT_WATCHDOG: return "watchdog/hard-fault reset";
        case CODE_HARD_FAULT:    return "hard fault";
        case CODE_WATCHDOG:      return "watchdog warning";
        case CODE_ASSERT:        return "assertion failed";
        case CODE_FLASH_ERROR:   return "flash error";
        case CODE_IPC_TIMEOUT:   return "IPC timeout";
        case CODE_CRC_ERROR:     return "CRC error";
        case CODE_USB_ERROR:     return "USB error";
        case CODE_GENERIC_WARN:  return "warning";
        case CODE_GENERIC_ERROR: return "error";
        case CODE_GENERIC_FATAL: return "fatal error";
        default:                 return "fault";
    }
}

/* ==================== 扫描恢复 ==================== */

typedef struct
{
    uint32_t committed;   /* 顺序记录槽数（含 PAD），页界 */
    uint32_t valid;       /* 有效事件数（不含 PAD） */
    uint32_t newest_boot; /* 最新有效记录的 boot_count */
    uint32_t newest_ts;   /* 最新有效记录的 timestamp_ms */
    uint32_t last_boot;   /* 最新 BOOT 事件的 boot_count */
} scan_result_t;

static scan_result_t scan_sector(uint32_t offset)
{
    scan_result_t s;
    memset(&s, 0, sizeof(s));

    const uint8_t* sector = (const uint8_t*)(uintptr_t)(flash_layout_xip_base() + offset);

    /* 按页扫描：每页要么整页编程（全事件，或事件 + PAD），要么从未编程。
     * 页编程是原子的，正常掉电不会出现半页；页内一旦出现 blank/CRC 坏，
     * 说明该页已损坏，整页判废并从下一物理页继续，保证绝不二次编程已写页。 */
    for (uint32_t page = 0; page < COMPACT_PAGES; page++)
    {
        uint32_t page_base = page * COMPACT_PER_PAGE;
        const compact_rec_t* r0 = (const compact_rec_t*)(sector + page_base * COMPACT_REC_SIZE);

        if (rec_is_blank(r0))
        {
            s.committed = page_base;   /* 该页从未编程：逻辑末尾（页界） */
            return s;
        }

        uint32_t page_valid = 0, p_boot = 0, p_ts = 0, p_lastboot = 0;
        bool page_ok = true;
        for (uint32_t k = 0; k < COMPACT_PER_PAGE; k++)
        {
            const compact_rec_t* r = (const compact_rec_t*)(sector + (page_base + k) * COMPACT_REC_SIZE);
            if (rec_is_pad_ok(r)) continue;
            if (!rec_is_event(r)) { page_ok = false; break; }  /* 页中 blank/损坏：整页判废 */
            page_valid++;
            p_boot = r->boot_count;
            p_ts = r->timestamp_ms;
            if ((r->code == CODE_BOOT_POWERON || r->code == CODE_BOOT_WATCHDOG) && r->boot_count > p_lastboot)
            {
                p_lastboot = r->boot_count;
            }
        }

        if (!page_ok)
        {
            s.committed = (page + 1U) * COMPACT_PER_PAGE;  /* 跳过损坏页（槽数，页界） */
            return s;
        }

        s.committed = (page + 1U) * COMPACT_PER_PAGE;
        s.valid += page_valid;
        s.newest_boot = p_boot;
        s.newest_ts = p_ts;
        if (p_lastboot > s.last_boot) s.last_boot = p_lastboot;
    }
    return s;
}

/* ==================== 页编程 / 扇区切换 ==================== */

static void switch_sectors(void);

/* 编程当前 RAM 页（整页 256B，调用时 s_page_slot 必须 > 0） */
static void program_current_page(bool seal)
{
    if (s_page_slot == 0) return;

    if (seal)
    {
        /* 用 PAD 填满页内空位，保证物理记录连续、扫描不被空槽截断 */
        for (uint8_t k = s_page_slot; k < COMPACT_PER_PAGE; k++)
        {
            rec_make_pad((compact_rec_t*)(s_page_buf + k * COMPACT_REC_SIZE));
        }
    }

    /* s_committed 为已编程记录槽数（页界），换算为字节偏移 */
    uint32_t offset = active_offset() + s_committed * COMPACT_REC_SIZE;
    flash_service_program(offset, s_page_buf, FLASH_PAGE_SIZE);

    s_committed += COMPACT_PER_PAGE;
    s_page_slot = 0;

    if (s_committed >= COMPACT_PER_SECTOR)
    {
        switch_sectors();
    }
}

/* 活动扇区写满：反向收集最近 KEEP 条有效事件，搬到另一扇区（8 整页） */
static void switch_sectors(void)
{
    const uint8_t* src = active_ptr();
    int32_t got = 0;

    /* 从最后一个槽反向收集，最新的放入高索引 */
    for (int32_t i = (int32_t)COMPACT_PER_SECTOR - 1; i >= 0 && got < (int32_t)COMPACT_KEEP_ON_SWITCH; i--)
    {
        const compact_rec_t* r = (const compact_rec_t*)(src + (uint32_t)i * COMPACT_REC_SIZE);
        if (rec_is_event(r))
        {
            s_keep[COMPACT_KEEP_ON_SWITCH - 1U - (uint32_t)got] = *r;
            got++;
        }
    }

    s_active = (s_active == 0) ? 1 : 0;
    flash_service_erase(active_offset(), FAULT_LOG_FLASH_SIZE);

    uint32_t keep = (uint32_t)got;
    for (uint32_t p = 0; p < (keep + COMPACT_PER_PAGE - 1U) / COMPACT_PER_PAGE; p++)
    {
        memset(s_page_buf, 0xFF, sizeof(s_page_buf));
        uint32_t base = p * COMPACT_PER_PAGE;
        uint32_t n = keep - base;
        if (n > COMPACT_PER_PAGE) n = COMPACT_PER_PAGE;
        memcpy(s_page_buf, &s_keep[base], n * COMPACT_REC_SIZE);
        /* 最后一页若不满（keep 非 16 倍数），用 PAD 填满，保持整页可写 */
        for (uint32_t k = n; k < COMPACT_PER_PAGE; k++)
        {
            rec_make_pad((compact_rec_t*)(s_page_buf + k * COMPACT_REC_SIZE));
        }
        flash_service_program(active_offset() + p * FLASH_PAGE_SIZE, s_page_buf, FLASH_PAGE_SIZE);
    }

    s_committed = ((keep + COMPACT_PER_PAGE - 1U) / COMPACT_PER_PAGE) * COMPACT_PER_PAGE;
    s_valid = keep;
    s_page_slot = 0;
}

/* ==================== 追加记录 ==================== */

static void append_raw(uint16_t code, fault_level_e level, uint8_t module_id)
{
    /* 同类合并：仅合并当前 RAM 页最后一条（已编程页永不回改，避免 0->1） */
    if (s_page_slot > 0)
    {
        compact_rec_t* last = (compact_rec_t*)(s_page_buf + (s_page_slot - 1U) * COMPACT_REC_SIZE);
        if (last->code == code && last->module_id == module_id &&
            (uint8_t)last->level == (uint8_t)level &&
            last->boot_count == s_boot && code != CODE_BOOT_POWERON && code != CODE_BOOT_WATCHDOG)
        {
            if (last->occurrences < 0xFFFFU) last->occurrences++;
            rec_finalize(last);
            return;
        }
    }

    if (s_page_slot == 0)
    {
        memset(s_page_buf, 0xFF, sizeof(s_page_buf));
    }

    compact_rec_t* r = (compact_rec_t*)(s_page_buf + s_page_slot * COMPACT_REC_SIZE);
    memset(r, 0, sizeof(*r));
    r->code = code;
    r->level = (uint8_t)level;
    r->module_id = module_id;
    r->boot_count = s_boot;
    r->timestamp_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    r->occurrences = 1;
    rec_finalize(r);

    s_page_slot++;
    s_valid++;

    if (s_page_slot >= COMPACT_PER_PAGE)
    {
        program_current_page(false);  /* 满页：整页编程一次 */
    }
}

/* ==================== 读取与还原 ==================== */

/* 取逻辑第 k 条有效事件（0=最旧），跳过 PAD；含当前 RAM 页 */
static bool get_kth_valid(uint32_t k, compact_rec_t* out)
{
    uint32_t flash_valid = s_valid - s_page_slot;

    if (k >= flash_valid)
    {
        /* 落在当前 RAM 页 */
        uint32_t idx = k - flash_valid;
        if (idx >= s_page_slot) return false;
        *out = *(const compact_rec_t*)(s_page_buf + idx * COMPACT_REC_SIZE);
        return true;
    }

    /* 顺序扫描活动扇区 Flash，跳过 PAD */
    uint32_t seen = 0;
    const uint8_t* sector = active_ptr();
    for (uint32_t i = 0; i < s_committed; i++)
    {
        const compact_rec_t* r = (const compact_rec_t*)(sector + i * COMPACT_REC_SIZE);
        if (rec_is_event(r))
        {
            if (seen == k)
            {
                *out = *r;
                return true;
            }
            seen++;
        }
    }
    return false;
}

static void fill_entry(const compact_rec_t* r, fault_log_entry_t* e)
{
    memset(e, 0, sizeof(*e));
    e->magic = FAULT_LOG_MAGIC;
    e->timestamp_ms = r->timestamp_ms;
    e->level = r->level;

    const char* mod = (r->module_id <= MOD_OTHER) ? s_module_names[r->module_id] : s_module_names[MOD_OTHER];
    size_t mlen = strlen(mod);
    if (mlen > 31U) mlen = 31U;
    memcpy(e->module, mod, mlen);
    e->module[mlen] = '\0';
    e->module_len = (uint8_t)mlen;

    int n;
    if (r->occurrences > 1U)
    {
        n = snprintf(e->msg, sizeof(e->msg), "%s (x%u) boot#%lu",
                     code_desc(r->code), (unsigned)r->occurrences,
                     (unsigned long)r->boot_count);
    }
    else
    {
        n = snprintf(e->msg, sizeof(e->msg), "%s boot#%lu",
                     code_desc(r->code), (unsigned long)r->boot_count);
    }
    if (n < 0) n = 0;
    if (n >= (int)sizeof(e->msg)) n = (int)sizeof(e->msg) - 1;
    e->msg_len = (uint16_t)n;
}

/* ==================== Backend API ==================== */

void fault_backend_init(void)
{
    scan_result_t a = scan_sector(flash_layout_fault_a_offset());
    scan_result_t b = scan_sector(flash_layout_fault_b_offset());

    if (a.valid > 0 && b.valid == 0)
    {
        s_active = 0; s_committed = a.committed; s_valid = a.valid;
    }
    else if (b.valid > 0 && a.valid == 0)
    {
        s_active = 1; s_committed = b.committed; s_valid = b.valid;
    }
    else if (a.valid > 0 && b.valid > 0)
    {
        bool b_newer = (b.newest_boot > a.newest_boot) ||
                       (b.newest_boot == a.newest_boot && b.newest_ts > a.newest_ts);
        if (b_newer) { s_active = 1; s_committed = b.committed; s_valid = b.valid; }
        else         { s_active = 0; s_committed = a.committed; s_valid = a.valid; }
    }
    else
    {
        /* 两扇区均无有效记录（全新，或仅剩损坏/PAD 页）。
         * 选物理写入进度更靠后的扇区并沿用其 committed，避免在已编程页上二次编程。 */
        if (b.committed > a.committed) { s_active = 1; s_committed = b.committed; }
        else                            { s_active = 0; s_committed = a.committed; }
        s_valid = 0;
    }
    s_page_slot = 0;

    uint32_t last_boot = (a.last_boot > b.last_boot) ? a.last_boot : b.last_boot;
    s_boot = last_boot + 1U;

    /* 追加本次启动事件（承载 boot_count 与复位原因）。
     * 异常复位（看门狗/HardFault 最终都走看门狗）立即 seal 落盘；正常上电随事件攒批。 */
    uint16_t boot_code = watchdog_caused_reboot() ? CODE_BOOT_WATCHDOG : CODE_BOOT_POWERON;
    append_raw(boot_code, FAULT_LEVEL_INFO, MOD_SYSTEM);
    if (boot_code == CODE_BOOT_WATCHDOG)
    {
        program_current_page(true);
    }
}

void fault_backend_record(fault_level_e level, const char *module, const char *msg)
{
    uint16_t code = classify_code(level, module, msg);
    uint8_t mid = classify_module(module, msg);
    append_raw(code, level, mid);
}

uint32_t fault_backend_get_log_count(void)
{
    return (s_valid < COMPACT_VIEW_COUNT) ? s_valid : COMPACT_VIEW_COUNT;
}

bool fault_backend_get_log(uint32_t index, fault_log_entry_t* out_entry)
{
    uint32_t view = fault_backend_get_log_count();
    if (index >= view) return false;

    compact_rec_t r;
    uint32_t k = s_valid - view + index;
    if (!get_kth_valid(k, &r)) return false;

    fill_entry(&r, out_entry);
    return true;
}

void fault_backend_clear(void)
{
    flash_service_erase(flash_layout_fault_a_offset(), FAULT_LOG_FLASH_SIZE);
    flash_service_erase(flash_layout_fault_b_offset(), FAULT_LOG_FLASH_SIZE);
    s_active = 0;
    s_committed = 0;
    s_valid = 0;
    s_page_slot = 0;
}

void fault_backend_flush(void)
{
    /* FATAL 复位前强制落盘当前页（PAD 填齐，每页仍只编程一次） */
    if (s_page_slot > 0)
    {
        program_current_page(true);
    }
}

#endif /* FEATURE_FAULT_COMPACT */
