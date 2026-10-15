/*
 * include/app/key_stats.h
 * 鎸夐敭缁熻妯″潡
 * 瀹屾暣鐗堟湰锛歊AM缁熻 + Flash鎸佷箙鍖?+ 纾ㄦ崯鍧囪　
 * 鏅鸿兘淇濆瓨绛栫暐锛屽噺灏慒lash鎿﹀啓娆℃暟
 */

#ifndef APP_KEY_STATS_H
#define APP_KEY_STATS_H

#include <stdint.h>
#include <stdbool.h>
#include "board/flash_layout.h"
#include "project_features.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 甯搁噺瀹氫箟 ==================== */

/* 鏈€澶ф寜閿暟閲?*/
#define KEY_STATS_MAX_KEYS    64

/* Flash瀛樺偍鍖哄ぇ灏忥細4KB锛堜竴涓墖鍖猴級 */
#define KEY_STATS_FLASH_SIZE    FLASH_LAYOUT_SECTOR_SIZE  /* 4096 bytes */

/* 鍗曟潯璁板綍澶у皬锛?12瀛楄妭锛?涓狥lash椤碉級
 * 娉ㄦ剰锛氬繀椤绘槸Flash椤靛ぇ灏忥紙256瀛楄妭锛夌殑鏁存暟鍊嶏紝
 * 鍥犱负flash_range_program瑕佹眰椤靛榻愬啓鍏? * 缁撴瀯浣撳疄闄呭ぇ灏?68瀛楄妭锛?12瀛楄妭鐣欐湁鎵╁睍绌洪棿
 */
#define KEY_STATS_RECORD_SIZE   (FLASH_PAGE_SIZE * 2)  /* 512 bytes */

/* 鏈€澶ц褰曟暟锛?KB / 512瀛楄妭 = 8鏉? * 椤哄簭鍐欏叆锛屽啓婊?娆℃墠鎿﹂櫎涓€娆★紝澶уぇ寤堕暱Flash瀵垮懡
 */
#define KEY_STATS_MAX_RECORDS   (KEY_STATS_FLASH_SIZE / KEY_STATS_RECORD_SIZE)

/*
 * 娉ㄦ剰锛欶lash鍦板潃涓嶅啀浣跨敤纭紪鐮佸畯锛? * 璇蜂娇鐢?flash_layout_key_stats_offset() / flash_layout_key_stats_addr()
 *
 * 甯冨眬锛堜粠Flash鏈熬鍚戝墠锛夛細
 *   鏈€鍚庣1涓墖鍖? 閰嶇疆B鍖? *   鏈€鍚庣2涓墖鍖? 閰嶇疆A鍖? *   鏈€鍚庣3涓墖鍖? 閿欒鏃ュ織鍖? *   鏈€鍚庣4涓墖鍖? 鎸夐敭缁熻鍖? */

/* 鍏煎鏃т唬鐮佺殑瀹忥紙涓嶆帹鑽愪娇鐢紝璇锋敼鐢ㄥ嚱鏁帮級 */
#define KEY_STATS_FLASH_OFFSET  flash_layout_key_stats_offset()
#define KEY_STATS_FLASH_ADDR    flash_layout_key_stats_addr()

/* 鑷姩淇濆瓨闂撮殧锛?0鍒嗛挓锛?800000姣锛?*/
#define KEY_STATS_AUTO_SAVE_INTERVAL_MS    1800000U  /* 30鍒嗛挓 */

/* 璁板綍榄旀暟锛?KEY0" */
#define KEY_STATS_MAGIC    0x3059454BU

/* ==================== 鏁版嵁缁撴瀯 ==================== */

/* 鎸夐敭缁熻璁板綍锛團lash瀛樺偍鏍煎紡锛?*/
typedef struct __attribute__((packed))
{
    uint32_t magic;                      /* 榄旀暟 0x3059454B */
    uint32_t timestamp_s;                /* 鏃堕棿鎴筹紙绉掞級 */
    uint32_t total_keystrokes;           /* 鎬绘寜閿暟 */
    uint32_t counts[KEY_STATS_MAX_KEYS]; /* 姣忎釜閿殑璁℃暟 */
    uint32_t crc32;                      /* CRC32鏍￠獙鍊硷紙璁＄畻鍓嶉潰鎵€鏈夊瓧娈碉級 */
} key_stats_record_t;

/* ==================== 瀵瑰鎺ュ彛 ==================== */

#if FEATURE_KEY_STATS

/**
 * @brief 鍒濆鍖栨寜閿粺璁℃ā鍧? * 浠嶧lash鍔犺浇鏈€杩戠殑缁熻鏁版嵁
 */
void key_stats_init(void);

/**
 * @brief 璁板綍涓€娆℃寜閿? * @param key_index 鎸夐敭绱㈠紩锛?~63锛? */
void key_stats_increment(uint8_t key_index);

/**
 * @brief 鑾峰彇鎸囧畾鎸夐敭鐨勮鏁? * @param key_index 鎸夐敭绱㈠紩
 * @return 鎸夐敭娆℃暟
 */
uint32_t key_stats_get_count(uint8_t key_index);

/**
 * @brief 鑾峰彇鎬绘寜閿暟
 * @return 鎬绘寜閿鏁? */
uint32_t key_stats_get_total(void);

/**
 * @brief 娓呴浂鎵€鏈夌粺璁? */
void key_stats_reset(void);

/**
 * @brief 淇濆瓨缁熻鏁版嵁鍒癋lash
 * 鑷姩鎵句笅涓€涓┖闂蹭綅缃啓鍏ワ紝鍐欐弧浜嗘墠鎿﹂櫎
 */
void key_stats_save_to_flash(void);

/**
 * @brief 浠嶧lash鍔犺浇鏈€杩戠殑缁熻鏁版嵁
 * @return true=鍔犺浇鎴愬姛锛宖alse=娌℃湁鏈夋晥鏁版嵁
 */
bool key_stats_load_from_flash(void);

/**
 * @brief 涓诲惊鐜痶ick锛堢敤浜庤嚜鍔ㄤ繚瀛樿鏃讹級
 */
void key_stats_tick(void);

/**
 * @brief 鑾峰彇Flash淇濆瓨娆℃暟锛堢敤浜庣（鎹熺洃鎺э級
 * @return 鏈鍛ㄦ湡鍐呯殑淇濆瓨娆℃暟
 */
uint32_t key_stats_get_save_count(void);

#else  /* FEATURE_KEY_STATS == 0锛歳elease 绌烘々锛岀紪璇戞湡娑堥櫎銆侀浂杩愯鏃跺紑閿€ */

static inline void key_stats_init(void) {}
static inline void key_stats_increment(uint8_t key_index) { (void)key_index; }
static inline uint32_t key_stats_get_count(uint8_t key_index) { (void)key_index; return 0; }
static inline uint32_t key_stats_get_total(void) { return 0; }
static inline void key_stats_reset(void) {}
static inline void key_stats_save_to_flash(void) {}
static inline bool key_stats_load_from_flash(void) { return false; }
static inline void key_stats_tick(void) {}
static inline uint32_t key_stats_get_save_count(void) { return 0; }

#endif /* FEATURE_KEY_STATS */

#ifdef __cplusplus
}
#endif

#endif /* APP_KEY_STATS_H */
