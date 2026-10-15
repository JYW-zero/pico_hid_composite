/*
 * src/app/key_stats.c
 * 鎸夐敭缁熻妯″潡瀹炵幇
 * 瀹屾暣鐗堟湰锛歊AM缁熻 + Flash鎸佷箙鍖?+ 纾ㄦ崯鍧囪　
 */

#include "project_features.h"

#if FEATURE_KEY_STATS

#include "app/key_stats.h"
#include "board/flash_layout.h"
#include "middleware/fault.h"
#include "middleware/flash_service.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include <string.h>
#include <stdio.h>

/* ==================== CRC32 瀹炵幇 ==================== */

static const uint32_t s_crc32_table[16] =
{
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

static uint32_t key_stats_crc32(const uint8_t* data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++)
    {
        uint8_t byte = data[i];
        crc = (crc >> 4) ^ s_crc32_table[(crc & 0x0Fu) ^ (byte & 0x0Fu)];
        crc = (crc >> 4) ^ s_crc32_table[(crc & 0x0Fu) ^ (byte >> 4)];
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ==================== 闈欐€佸彉閲?==================== */

static uint32_t s_counts[KEY_STATS_MAX_KEYS];  /* 褰撳墠鎸夐敭璁℃暟 */
static uint32_t s_total = 0;                    /* 鎬绘寜閿暟 */
static bool s_initialized = false;

static uint32_t s_next_save_index = 0;          /* 涓嬩竴涓繚瀛樹綅缃?*/
static uint32_t s_save_count = 0;               /* 鏈鍛ㄦ湡淇濆瓨娆℃暟 */
static uint32_t s_last_save_ms = 0;             /* 涓婃淇濆瓨鏃堕棿 */
static uint32_t s_uptime_s = 0;                 /* 杩愯鏃堕棿锛堢锛?*/

/* ==================== 鍐呴儴鍑芥暟 ==================== */

/* 鎵弿Flash锛屾壘鍒版渶鏂扮殑鏈夋晥璁板綍鍜屼笅涓€涓啓鍏ヤ綅缃?*/
static void scan_flash_records(void)
{
    const key_stats_record_t* records = (const key_stats_record_t*)KEY_STATS_FLASH_ADDR;

    s_next_save_index = 0;
    s_save_count = 0;

    /* 鎵惧埌鏈€鍚庝竴鏉℃湁鏁堣褰?*/
    for (uint32_t i = 0; i < KEY_STATS_MAX_RECORDS; i++)
    {
        if (records[i].magic == KEY_STATS_MAGIC)
        {
            s_next_save_index = i + 1;
            s_save_count++;
        }
        else
        {
            /* 閬囧埌鏃犳晥鐨勶紝鍚庨潰鐨勯兘鏃犳晥浜嗭紙椤哄簭鍐欏叆锛?*/
            break;
        }
    }

    /* 濡傛灉鍐欐弧浜嗭紝涓嬫浠?寮€濮嬶紙鍐欏叆鍓嶄細鍏堟摝闄わ級 */
    if (s_next_save_index >= KEY_STATS_MAX_RECORDS)
    {
        s_next_save_index = 0;
    }
}

/* 鎿﹂櫎鏁翠釜缁熻鎵囧尯 */
static void erase_stats_flash(void)
{
    flash_service_erase(KEY_STATS_FLASH_OFFSET, KEY_STATS_FLASH_SIZE);
}

/* 鍐欏叆涓€鏉¤褰曞埌Flash鎸囧畾浣嶇疆 */
static void write_record_to_flash(uint32_t index, const key_stats_record_t* record)
{
    if (index >= KEY_STATS_MAX_RECORDS)
    {
        return;
    }

    /* 浣跨敤闈欐€佺紦鍐插尯锛屽厛娓呴浂鍐嶅鍒剁粨鏋勪綋锛岄伩鍏嶈秺鐣岃鍙栨爤鏁版嵁 */
    static uint8_t write_buf[KEY_STATS_RECORD_SIZE];
    memset(write_buf, 0, sizeof(write_buf));
    memcpy(write_buf, record, sizeof(key_stats_record_t));

    uint32_t offset = KEY_STATS_FLASH_OFFSET + index * KEY_STATS_RECORD_SIZE;
    flash_service_program(offset, write_buf, KEY_STATS_RECORD_SIZE);
}

/* ==================== 瀵瑰鎺ュ彛 ==================== */

void key_stats_init(void)
{
    /* 纭繚Flash甯冨眬宸插垵濮嬪寲锛堟娴婩lash澶у皬锛?*/
    flash_layout_init();

    memset(s_counts, 0, sizeof(s_counts));
    s_total = 0;
    s_last_save_ms = 0;
    s_uptime_s = 0;

    /* 鎵弿Flash */
    scan_flash_records();

    /* 灏濊瘯鍔犺浇鏈€杩戠殑璁板綍 */
    if (s_save_count > 0)
    {
        key_stats_load_from_flash();
    }

    s_initialized = true;
}

void key_stats_increment(uint8_t key_index)
{
    if (!s_initialized || key_index >= KEY_STATS_MAX_KEYS)
    {
        return;
    }

    s_counts[key_index]++;
    s_total++;
}

uint32_t key_stats_get_count(uint8_t key_index)
{
    if (key_index >= KEY_STATS_MAX_KEYS)
    {
        return 0;
    }
    return s_counts[key_index];
}

uint32_t key_stats_get_total(void)
{
    return s_total;
}

void key_stats_reset(void)
{
    memset(s_counts, 0, sizeof(s_counts));
    s_total = 0;
}

void key_stats_save_to_flash(void)
{
    if (!s_initialized)
    {
        return;
    }

    /* 濡傛灉鍐欐弧浜嗭紝鍏堟摝闄ゆ暣涓墖鍖猴紝鐒跺悗浠庡ご寮€濮嬪啓 */
    if (s_next_save_index == 0 && s_save_count >= KEY_STATS_MAX_RECORDS)
    {
        erase_stats_flash();
        s_save_count = 0;
    }

    /* 鏋勯€犺褰?*/
    key_stats_record_t record;
    memset(&record, 0, sizeof(record));
    record.magic = KEY_STATS_MAGIC;
    record.timestamp_s = s_uptime_s;
    record.total_keystrokes = s_total;
    memcpy(record.counts, s_counts, sizeof(s_counts));

    /* 璁＄畻CRC32锛堥櫎crc32瀛楁澶栫殑鎵€鏈夊唴瀹癸級 */
    uint32_t crc = key_stats_crc32((const uint8_t*)&record,
                                   sizeof(key_stats_record_t) - sizeof(uint32_t));
    record.crc32 = crc;

    /* 鍐欏叆Flash */
    write_record_to_flash(s_next_save_index, &record);

    /* 鏇存柊绱㈠紩 */
    s_next_save_index++;
    s_save_count++;
    s_last_save_ms = (uint32_t)to_ms_since_boot(get_absolute_time());

    if (s_next_save_index >= KEY_STATS_MAX_RECORDS)
    {
        s_next_save_index = 0;
    }
}

bool key_stats_load_from_flash(void)
{
    const key_stats_record_t* records = (const key_stats_record_t*)KEY_STATS_FLASH_ADDR;

    /* 鎵惧埌鏈€鍚庝竴鏉℃湁鏁堣褰曪紙magic + CRC鍙岄噸鏍￠獙锛?*/
    int last_index = -1;
    for (uint32_t i = 0; i < KEY_STATS_MAX_RECORDS; i++)
    {
        if (records[i].magic == KEY_STATS_MAGIC)
        {
            /* 楠岃瘉CRC32 */
            uint32_t calc_crc = key_stats_crc32((const uint8_t*)&records[i],
                                                 sizeof(key_stats_record_t) - sizeof(uint32_t));
            if (calc_crc == records[i].crc32)
            {
                last_index = (int)i;
            }
            else
            {
                /* CRC涓嶅尮閰嶏紝璁板綍鎹熷潖锛屽仠姝㈡煡鎵撅紙鍚庣画璁板綍涔熶笉鍙俊锛?*/
                break;
            }
        }
        else
        {
            break;
        }
    }

    if (last_index < 0)
    {
        return false;
    }

    /* 鍔犺浇鏁版嵁 */
    s_total = records[last_index].total_keystrokes;
    memcpy(s_counts, records[last_index].counts, sizeof(s_counts));
    s_uptime_s = records[last_index].timestamp_s;

    return true;
}

void key_stats_tick(void)
{
    if (!s_initialized)
    {
        return;
    }

    static uint32_t last_tick_ms = 0;
    uint32_t now = (uint32_t)to_ms_since_boot(get_absolute_time());

    /* 姣忕鏇存柊杩愯鏃堕棿 */
    if (now - last_tick_ms >= 1000)
    {
        s_uptime_s += (now - last_tick_ms) / 1000;
        last_tick_ms = now;
    }

    /* 鑷姩淇濆瓨锛氭瘡闅?0鍒嗛挓淇濆瓨涓€娆★紙浣跨敤瀹夊叏鍐欏叆鏈嶅姟锛屽弻鏍稿悓姝ワ級 */
    if (now - s_last_save_ms >= KEY_STATS_AUTO_SAVE_INTERVAL_MS)
    {
        if (s_total > 0)
        {
            key_stats_save_to_flash();
        }
        else
        {
            s_last_save_ms = now;
        }
    }
}

uint32_t key_stats_get_save_count(void)
{
    return s_save_count;
}

#endif /* FEATURE_KEY_STATS */
