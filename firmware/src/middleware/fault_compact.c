/*
 * src/middleware/fault_compact.c
 * Fault 鎸佷箙鍖栧悗绔?- 绮剧畝妗ｏ紙release / FEATURE_FAULT_COMPACT=1锛? *
 * 璁捐锛堣 docs/FEATURE_ROLE_SPLIT_PLAN.md 搂8.3锛夛細
 *   - 瀹氶暱 16B 绱у噾璁板綍锛歝ode/level/module_id/boot_count/timestamp/occurrences/crc16
 *   - 鎸夐〉鏀掓壒锛氫竴椤?256B 鏀?16 鏉★紝鏀掓弧鎵嶇紪绋嬶紱姣忛〉鍙紪绋嬩竴娆★紙涓嶅仛鍚岄〉澶氭缂栫▼锛? *   - FATAL / 寮傚父澶嶄綅鍓?seal锛氱珛鍗崇紪绋嬪綋鍓嶉〉锛岄〉鍐呯┖浣嶄互 PAD 璁板綍濉厖淇濇寔鐗╃悊杩炵画
 *   - A/B 鍙屾墖鍖虹幆褰細姣忔墖鍖?256 鏉★紝鍐欐弧鍚庢妸鏈€杩?128 鏉℃惉鍒板彟涓€鎵囧尯鍐嶅垏鎹? *   - 姣忔潯 CRC16锛屾壂鎻忛亣绌烘Ы(0xFFFF)/鎹熷潖鍗虫锛屾帀鐢靛彧鍙兘涓㈡湭婊￠〉鐨勯潪鑷村懡璁板綍
 *   - 寮€鏈鸿拷鍔犱竴鏉?BOOT 浜嬩欢鎵胯浇 boot_count锛堟棤 RTC 鏃堕棿鍩哄噯锛変笌澶嶄綅鍘熷洜
 *   - 瀵瑰鍙毚闇叉渶杩?COMPACT_VIEW_COUNT(32) 鏉★紱璇诲嚭鏃惰繕鍘熶负鍏叡 fault_log_entry_t锛? *     鍥犳 HID 鎶ュ憡涓庝笂浣嶆満 ErrorLog 椤垫棤闇€鏀瑰姩銆? *
 * 瀹归噺/瀵垮懡锛堢浉瀵瑰叏閲忔。锛夛細鍗曟墖鍖?16 -> 256 鏉★紝鍗曚綅鎵囧尯鎿﹂櫎闂撮殧绾︽彁鍗?16 鍊嶃€? */

#include "project_features.h"

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

/* ==================== 甯搁噺 ==================== */

#define COMPACT_REC_SIZE       16U
#define COMPACT_PER_PAGE       (FLASH_PAGE_SIZE / COMPACT_REC_SIZE)   /* 16 鏉?椤?*/
#define COMPACT_PAGES          (FAULT_LOG_FLASH_SIZE / FLASH_PAGE_SIZE) /* 16 椤?鎵囧尯 */
#define COMPACT_PER_SECTOR     (COMPACT_PER_PAGE * COMPACT_PAGES)     /* 256 鏉?鎵囧尯 */
#define COMPACT_KEEP_ON_SWITCH (COMPACT_PER_SECTOR / 2U)              /* 鍒囨崲淇濈暀 128 鏉?*/
#define COMPACT_VIEW_COUNT     32U                                   /* 瀵瑰鏈€杩戠獥鍙?*/
#define COMPACT_CRC_LEN        14U                                   /* CRC 瑕嗙洊鍓?14 瀛楄妭 */

/* 绌烘Ы鏍囪锛堟摝闄ゆ€?code 瀛楁涓?0xFFFF锛夛紱PAD 涓?seal 椤靛～鍏呭崰浣?*/
#define CODE_BLANK             0xFFFFU
#define CODE_PAD               0xFFFEU

/* 鏁呴殰鐮?/ 鍚姩浜嬩欢鐮?*/
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

/* 妯″潡 ID锛堢揣鍑戞。鍙瓨 ID锛岃繕鍘熸椂鏌ラ潤鎬佸瓧绗︿覆琛級 */
enum
{
    MOD_SYSTEM = 0, MOD_FLASH, MOD_WATCHDOG, MOD_CORE1, MOD_USB,
    MOD_CONFIG, MOD_KEYPAD, MOD_OPTICAL, MOD_JOYSTICK, MOD_ENCODER,
    MOD_FACTORY, MOD_MACRO, MOD_POWER, MOD_SCHEDULER, MOD_IPC, MOD_OTHER
};

/* ==================== 绱у噾璁板綍锛?6B锛宲acked锛?==================== */

typedef struct __attribute__((packed))
{
    uint16_t code;         /* 鏁呴殰鐮?鍚姩浜嬩欢鐮?*/
    uint8_t  level;        /* fault_level_e */
    uint8_t  module_id;    /* 妯″潡鏋氫妇 ID */
    uint32_t boot_count;   /* 涓婄數搴忓彿 */
    uint32_t timestamp_ms; /* 鏈涓婄數鍐呮绉?*/
    uint16_t occurrences;  /* 鍚岀被鍚堝苟璁℃暟 */
    uint16_t crc16;        /* CRC16-CCITT锛岃鐩栧墠 14 瀛楄妭 */
} compact_rec_t;

_Static_assert(sizeof(compact_rec_t) == COMPACT_REC_SIZE, "compact record must be 16 bytes");

/* ==================== 闈欐€佺姸鎬?==================== */

static uint8_t  s_active = 0;      /* 娲诲姩鎵囧尯 0=A 1=B */
static uint32_t s_committed = 0;   /* 宸叉暣椤电紪绋嬬殑璁板綍妲芥暟锛堝惈 PAD锛屾亽涓洪〉鐣岋級 */
static uint32_t s_valid = 0;       /* 鏈夋晥浜嬩欢鎬绘暟锛堜笉鍚?PAD锛屽惈褰撳墠 RAM 椤碉級 */
static uint8_t  s_page_slot = 0;   /* 褰撳墠 RAM 椤靛唴宸茬紦鍐茬殑鏈夋晥鏉℃暟 0..15 */
static uint32_t s_boot = 1;        /* 鏈涓婄數搴忓彿 */
static uint8_t  s_page_buf[FLASH_PAGE_SIZE]; /* 褰撳墠椤?RAM 闀滃儚锛?56B锛?*/

/* 鎵囧尯鍒囨崲鏃舵惉杩愮紦鍐诧細128 鏉?脳 16B = 2KB锛宻tatic 閬垮厤鍗犵敤鏍?*/
static compact_rec_t s_keep[COMPACT_KEEP_ON_SWITCH];

/* ==================== 鎵囧尯鍦板潃 ==================== */

static uint32_t active_offset(void)
{
    return (s_active == 0) ? flash_layout_fault_a_offset() : flash_layout_fault_b_offset();
}
/* 鍙鍩烘寚閽堬細鍥轰欢涓?XIP 鍦板潃锛宧ost 娴嬭瘯鐢?mock 閲嶅畾鍚戝埌 RAM 妯℃嫙 Flash */
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

/* PAD 鍗犱綅璁板綍锛坰eal 椤靛～鍏咃紝闈炵湡瀹炰簨浠讹級 */
static void rec_make_pad(compact_rec_t* r)
{
    memset(r, 0, sizeof(*r));
    r->code = CODE_PAD;
    rec_finalize(r);
}

/* 鏈夋晥锛堢湡瀹炰簨浠讹紝CRC 姝ｇ‘锛夛紱PAD 杩斿洖 false 浣嗕篃瑕佹眰 CRC 姝ｇ‘ */
static bool rec_is_event(const compact_rec_t* r)
{
    if (r->code == CODE_BLANK || r->code == CODE_PAD) return false;
    return r->crc16 == crc16_ccitt((const uint8_t*)r, COMPACT_CRC_LEN);
}
static bool rec_is_pad_ok(const compact_rec_t* r)
{
    return r->code == CODE_PAD && r->crc16 == crc16_ccitt((const uint8_t*)r, COMPACT_CRC_LEN);
}

/* ==================== 鍒嗙被琛紙鏂囨湰 -> code / module_id锛?==================== */

static const char* const s_module_names[16] =
{
    "system", "flash", "watchdog", "core1", "usb",
    "config", "keypad", "optical", "joystick", "encoder",
    "factory", "macro", "power", "scheduler", "ipc", "other"
};

/* 澶у皬鍐欎笉鏁忔劅瀛愪覆锛坣eedle 蹇呴』涓哄皬鍐?ASCII锛沨aystack 鍙惈 UTF-8 涓枃锛?*/
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

/* ==================== 鎵弿鎭㈠ ==================== */

typedef struct
{
    uint32_t committed;   /* 椤哄簭璁板綍妲芥暟锛堝惈 PAD锛夛紝椤电晫 */
    uint32_t valid;       /* 鏈夋晥浜嬩欢鏁帮紙涓嶅惈 PAD锛?*/
    uint32_t newest_boot; /* 鏈€鏂版湁鏁堣褰曠殑 boot_count */
    uint32_t newest_ts;   /* 鏈€鏂版湁鏁堣褰曠殑 timestamp_ms */
    uint32_t last_boot;   /* 鏈€鏂?BOOT 浜嬩欢鐨?boot_count */
} scan_result_t;

static scan_result_t scan_sector(uint32_t offset)
{
    scan_result_t s;
    memset(&s, 0, sizeof(s));

    const uint8_t* sector = (const uint8_t*)(uintptr_t)(flash_layout_xip_base() + offset);

    /* 鎸夐〉鎵弿锛氭瘡椤佃涔堟暣椤电紪绋嬶紙鍏ㄤ簨浠讹紝鎴栦簨浠?+ PAD锛夛紝瑕佷箞浠庢湭缂栫▼銆?     * 椤电紪绋嬫槸鍘熷瓙鐨勶紝姝ｅ父鎺夌數涓嶄細鍑虹幇鍗婇〉锛涢〉鍐呬竴鏃﹀嚭鐜?blank/CRC 鍧忥紝
     * 璇存槑璇ラ〉宸叉崯鍧忥紝鏁撮〉鍒ゅ簾骞朵粠涓嬩竴鐗╃悊椤电户缁紝淇濊瘉缁濅笉浜屾缂栫▼宸插啓椤点€?*/
    for (uint32_t page = 0; page < COMPACT_PAGES; page++)
    {
        uint32_t page_base = page * COMPACT_PER_PAGE;
        const compact_rec_t* r0 = (const compact_rec_t*)(sector + page_base * COMPACT_REC_SIZE);

        if (rec_is_blank(r0))
        {
            s.committed = page_base;   /* 璇ラ〉浠庢湭缂栫▼锛氶€昏緫鏈熬锛堥〉鐣岋級 */
            return s;
        }

        uint32_t page_valid = 0, p_boot = 0, p_ts = 0, p_lastboot = 0;
        bool page_ok = true;
        for (uint32_t k = 0; k < COMPACT_PER_PAGE; k++)
        {
            const compact_rec_t* r = (const compact_rec_t*)(sector + (page_base + k) * COMPACT_REC_SIZE);
            if (rec_is_pad_ok(r)) continue;
            if (!rec_is_event(r)) { page_ok = false; break; }  /* 椤典腑 blank/鎹熷潖锛氭暣椤靛垽搴?*/
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
            s.committed = (page + 1U) * COMPACT_PER_PAGE;  /* 璺宠繃鎹熷潖椤碉紙妲芥暟锛岄〉鐣岋級 */
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

/* ==================== 椤电紪绋?/ 鎵囧尯鍒囨崲 ==================== */

static void switch_sectors(void);

/* 缂栫▼褰撳墠 RAM 椤碉紙鏁撮〉 256B锛岃皟鐢ㄦ椂 s_page_slot 蹇呴』 > 0锛?*/
static void program_current_page(bool seal)
{
    if (s_page_slot == 0) return;

    if (seal)
    {
        /* 鐢?PAD 濉弧椤靛唴绌轰綅锛屼繚璇佺墿鐞嗚褰曡繛缁€佹壂鎻忎笉琚┖妲芥埅鏂?*/
        for (uint8_t k = s_page_slot; k < COMPACT_PER_PAGE; k++)
        {
            rec_make_pad((compact_rec_t*)(s_page_buf + k * COMPACT_REC_SIZE));
        }
    }

    /* s_committed 涓哄凡缂栫▼璁板綍妲芥暟锛堥〉鐣岋級锛屾崲绠椾负瀛楄妭鍋忕Щ */
    uint32_t offset = active_offset() + s_committed * COMPACT_REC_SIZE;
    flash_service_program(offset, s_page_buf, FLASH_PAGE_SIZE);

    s_committed += COMPACT_PER_PAGE;
    s_page_slot = 0;

    if (s_committed >= COMPACT_PER_SECTOR)
    {
        switch_sectors();
    }
}

/* 娲诲姩鎵囧尯鍐欐弧锛氬弽鍚戞敹闆嗘渶杩?KEEP 鏉℃湁鏁堜簨浠讹紝鎼埌鍙︿竴鎵囧尯锛? 鏁撮〉锛?*/
static void switch_sectors(void)
{
    const uint8_t* src = active_ptr();
    int32_t got = 0;

    /* 浠庢渶鍚庝竴涓Ы鍙嶅悜鏀堕泦锛屾渶鏂扮殑鏀惧叆楂樼储寮?*/
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
        /* 鏈€鍚庝竴椤佃嫢涓嶆弧锛坘eep 闈?16 鍊嶆暟锛夛紝鐢?PAD 濉弧锛屼繚鎸佹暣椤靛彲鍐?*/
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

/* ==================== 杩藉姞璁板綍 ==================== */

static void append_raw(uint16_t code, fault_level_e level, uint8_t module_id)
{
    /* 鍚岀被鍚堝苟锛氫粎鍚堝苟褰撳墠 RAM 椤垫渶鍚庝竴鏉★紙宸茬紪绋嬮〉姘镐笉鍥炴敼锛岄伩鍏?0->1锛?*/
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
        program_current_page(false);  /* 婊￠〉锛氭暣椤电紪绋嬩竴娆?*/
    }
}

/* ==================== 璇诲彇涓庤繕鍘?==================== */

/* 鍙栭€昏緫绗?k 鏉℃湁鏁堜簨浠讹紙0=鏈€鏃э級锛岃烦杩?PAD锛涘惈褰撳墠 RAM 椤?*/
static bool get_kth_valid(uint32_t k, compact_rec_t* out)
{
    uint32_t flash_valid = s_valid - s_page_slot;

    if (k >= flash_valid)
    {
        /* 钀藉湪褰撳墠 RAM 椤?*/
        uint32_t idx = k - flash_valid;
        if (idx >= s_page_slot) return false;
        *out = *(const compact_rec_t*)(s_page_buf + idx * COMPACT_REC_SIZE);
        return true;
    }

    /* 椤哄簭鎵弿娲诲姩鎵囧尯 Flash锛岃烦杩?PAD */
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
        /* 涓ゆ墖鍖哄潎鏃犳湁鏁堣褰曪紙鍏ㄦ柊锛屾垨浠呭墿鎹熷潖/PAD 椤碉級銆?         * 閫夌墿鐞嗗啓鍏ヨ繘搴︽洿闈犲悗鐨勬墖鍖哄苟娌跨敤鍏?committed锛岄伩鍏嶅湪宸茬紪绋嬮〉涓婁簩娆＄紪绋嬨€?*/
        if (b.committed > a.committed) { s_active = 1; s_committed = b.committed; }
        else                            { s_active = 0; s_committed = a.committed; }
        s_valid = 0;
    }
    s_page_slot = 0;

    uint32_t last_boot = (a.last_boot > b.last_boot) ? a.last_boot : b.last_boot;
    s_boot = last_boot + 1U;

    /* 杩藉姞鏈鍚姩浜嬩欢锛堟壙杞?boot_count 涓庡浣嶅師鍥狅級銆?     * 寮傚父澶嶄綅锛堢湅闂ㄧ嫍/HardFault 鏈€缁堥兘璧扮湅闂ㄧ嫍锛夌珛鍗?seal 钀界洏锛涙甯镐笂鐢甸殢浜嬩欢鏀掓壒銆?*/
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
    /* FATAL 澶嶄綅鍓嶅己鍒惰惤鐩樺綋鍓嶉〉锛圥AD 濉綈锛屾瘡椤典粛鍙紪绋嬩竴娆★級 */
    if (s_page_slot > 0)
    {
        program_current_page(true);
    }
}

#endif /* FEATURE_FAULT_COMPACT */
