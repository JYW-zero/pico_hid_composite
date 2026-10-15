/*
 * protocol/config_hid.c
 * HID 閰嶇疆鍗忚妯″潡锛堜粠 main.c 鎻愬彇锛?
 */

#include "project_features.h"
#include "protocol/config_hid.h"
#include "protocol/config_hid_helpers.h"
#include "protocol/config_lock.h"
#include "protocol/config_block_receiver.h"
#include "protocol/config_validator.h"
#include "protocol/command_confirm.h"
#include "app/log.h"
#include <string.h>
#include <stdio.h>
#include "usb_descriptors.h"
#include "protocol/capability.h"
#include "protocol/command_gate.h"
#include "board/config.h"
#include "board/board.h"
#include "app/macro.h"
#include "app/keymap.h"
#include "app/key_stats.h"
#include "middleware/crc32.h"
#include "middleware/fault.h"
#include "middleware/perf_monitor.h"
#include "middleware/shared_hw_data.h"
#include "middleware/watchdog.h"
#include "middleware/ipc.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "device/optical_sensor.h"
#include "device/dfu/dfu_metadata.h"
#include "pico/time.h"
#include "pico/bootrom.h"

/* ==================== 甯搁噺瀹氫箟 ==================== */
/* 瀹忛厤缃欢杩熶繚瀛樻椂闂达紙寰锛?
 * 瀹忛厤缃慨鏀瑰悗绛夊緟姝ゆ椂闂村啀鍐欏叆 Flash锛屽悎骞剁煭鏃堕棿鍐呯殑澶氭淇敼
 * 200ms 骞宠　浜嗗啓鍏ユ鏁帮紙Flash瀵垮懡锛夊拰鏁版嵁鎸佷箙鎬э紙鏂數涓㈠け绐楀彛锛?
 * 鍘?绉掑欢杩熷湪楂橀瀹忕紪杈戝満鏅笅涓㈠け绐楀彛杩囧ぇ
 */
#define MACRO_DIRTY_SAVE_DELAY_US  (200000U)

/* IPC 鍛戒护 ACK 瓒呮椂锛堝井绉掞級
 * Core0 鍙戦€佸懡浠ょ粰 Core1 鍚庯紝绛夊緟 ACK 鐨勮秴鏃舵椂闂?
 * 10ms 瓒冲瑕嗙洊 Core1 鏈€鍧忔儏鍐典笅鐨勫懡浠ゅ鐞嗗欢杩?
 */
#define CONFIG_IPC_ACK_TIMEOUT_US   (10000U)

/* 閰嶇疆澶嶄綅/閲嶅惎寤惰繜锛堟绉掞級
 * 瑙﹀彂鐪嬮棬鐙楀浣嶅墠鐨勫欢杩燂紝纭繚 Flash 鍐欏叆鍜屾棩蹇楄緭鍑哄畬鎴?
 * 100ms 瓒冲瑕嗙洊 Flash 鎵囧尯鎿﹂櫎锛垀50ms锛夊拰椤电紪绋嬶紙~1ms锛?
 */
#define CONFIG_RESET_DELAY_MS       (100U)

/* DPI 閰嶇疆鑼冨洿锛圖PI 鍊硷級
 * 鍏夊浼犳劅鍣ㄦ敮鎸佺殑 DPI 鑼冨洿锛岃秴鍑烘鑼冨洿鐨勯厤缃涓烘棤鏁?
 */
#define CONFIG_DPI_MIN              (100U)
#define CONFIG_DPI_MAX              (6400U)

/* 瀹忛厤缃潡瀛楁瀹氫箟锛歜lock_raw 瀛楄妭鐨勬渶楂樹綅琛ㄧず璇?鍐欙紝浣?浣嶈〃绀哄潡鍙?*/
#define MACRO_BLOCK_READ_FLAG       (0x80U)  /* 璇诲懡浠ゆ爣蹇椾綅锛堟渶楂樹綅=1琛ㄧず璇伙級 */
#define MACRO_BLOCK_INDEX_MASK      (0x7FU)  /* 鍧楀彿鎺╃爜锛堜綆7浣嶏級 */

/* CRC32 璁＄畻浣跨敤鍏叡妯″潡 middleware/crc32.h */
/* 鏃ュ織瀹忎娇鐢ㄥ叕鍏辨ā鍧?app/log.h */

/* 涓存椂閰嶇疆缂撳啿鍖?*/
static device_config_t g_tmp_config = {0};
static volatile bool g_config_pending = false;
static volatile uint8_t g_pending_cmd = 0;
static volatile uint16_t g_pending_cmd_param = 0;

/* v3.1 鍗忚锛氬懡浠ゅ搷搴旂紦鍐插尯锛堝悓姝ヨ姹?鍝嶅簲妯″紡锛?
 * 鏇夸唬鍘熷紓姝?ACK Input Report锛岄伩鍏?ACK 涓㈠け瀵艰嚧鐨勫懡浠よ秴鏃堕棶棰?
 * 鏍煎紡: [status, cmd, sw_id, device_idx, param_low, param_high, reserved[2]]
 *   status=0 琛ㄧず鎴愬姛锛岄潪0涓洪敊璇爜锛坈onfig_hid_error_t锛?
 *   sw_id 鍥炴樉璇锋眰鐨?Software ID锛岀敤浜庡尮閰嶈姹傚搷搴?
 *   device_idx 鍥炴樉璇锋眰鐨?Device Index
 * 涓婁綅鏈哄彂閫?Output 鍛戒护鍚庯紝绔嬪嵆 GET_REPORT(24) 璇诲彇姝ょ紦鍐插尯
 * 濡傛灉鍛戒护灏氭湭澶勭悊瀹屾垚锛岃繑鍥?CONFIG_HID_ERR_BUSY锛屼笂浣嶆満搴旈噸璇?
 */
static uint8_t g_control_response[8] = {0};
static volatile bool g_control_response_valid = false;

/* 璁剧疆鍛戒护鍝嶅簲锛堟浛浠?config_hid_set_response锛?
 * 鍛戒护澶勭悊瀹屾垚鍚庤皟鐢ㄦ鍑芥暟璁剧疆鍝嶅簲锛屼笂浣嶆満閫氳繃 GET_REPORT(24) 璇诲彇
 */
/* v3.1 鍗忚锛氬綋鍓嶅懡浠ょ殑 Device Index 鍜?Software ID锛堜粠鍛戒护瑙ｆ瀽鏃剁紦瀛橈紝鍝嶅簲鏃跺洖鏄撅級 */
static volatile uint8_t g_current_device_idx = DEVICE_INDEX_DIRECT;
static volatile uint8_t g_current_sw_id = SW_ID_BROADCAST;

static void config_hid_set_response(uint8_t cmd, bool success, uint8_t error_code, uint16_t param)
{
    /* v3.1 鍝嶅簲鏍煎紡: [status, cmd, sw_id, device_idx, param_low, param_high, reserved[2]] */
    g_control_response[RESP_OFFSET_STATUS] = success ? 0 : error_code;
    g_control_response[RESP_OFFSET_CMD] = cmd;
    g_control_response[RESP_OFFSET_SW_ID] = g_current_sw_id;        /* 鍥炴樉 SW ID */
    g_control_response[RESP_OFFSET_DEVICE_IDX] = g_current_device_idx; /* 鍥炴樉 Device Index */
    g_control_response[RESP_OFFSET_PARAM_LOW] = (uint8_t)(param & 0xFF);
    g_control_response[RESP_OFFSET_PARAM_HIGH] = (uint8_t)((param >> 8) & 0xFF);
    g_control_response[6] = 0;
    g_control_response[7] = 0;
    g_control_response_valid = true;
}

/* 閰嶇疆鍧楁帴鏀剁姸鎬佺鐞嗗凡绉昏嚦 protocol/config_block_receiver.c */

/* 閲嶇疆閰嶇疆鍧楁帴鏀剁姸鎬侊紙USB 鏂紑鏃惰皟鐢紝闃叉娈嬬暀鍧椾笌鏂颁細璇濈粍鍚堝鑷撮厤缃崯鍧忥級 */
void config_hid_reset_blocks_received(void)
{
    config_block_receiver_reset();
}

/* 閫氳繃 IPC 鍛戒护灏?DPI 璁剧疆璇锋眰鍙戦€佺粰 Core1 鎵ц
 * 閬垮厤 Core0 鍦ㄤ腑鏂笂涓嬫枃涓洿鎺ユ搷浣?SPI 鎬荤嚎涓?Core1 绔炰簤瀵艰嚧姝婚攣
 * 杩斿洖锛?=鎴愬姛锛岄潪0=澶辫触锛團IFO婊℃垨ACK瓒呮椂锛?
 */
static int config_hid_set_dpi_via_ipc(uint16_t dpi_val, bool is_raw)
{
    uint32_t cmd;
    if (is_raw) {
        cmd = IPC_MAKE_CMD(IPC_CMD_SET_DPI_RAW, dpi_val);
    } else {
        cmd = IPC_MAKE_CMD(IPC_CMD_SET_DPI, dpi_val);
    }

    /* 闈為樆濉炲彂閫侊細FIFO 婊℃椂绔嬪嵆澶辫触 */
    if (!multicore_fifo_push_timeout_us(cmd, 0)) {
        LOG_ERROR_PRINT("[CONFIG] 鉂?DPI璁剧疆澶辫触锛欶IFO婊n");
        return -1;
    }

    /* 璇诲彇骞朵涪寮?Core1 鍥炲鐨?ACK锛堝甫瓒呮椂锛岄伩鍏嶉樆濉烇級 */
    uint32_t ack_val = 0;
    if (!multicore_fifo_pop_timeout_us(CONFIG_IPC_ACK_TIMEOUT_US, &ack_val)) {
        LOG_WARN_PRINT("[CONFIG] 鈿狅笍 DPI璁剧疆ACK瓒呮椂锛圕ore1鍙兘蹇欙級\n");
        /* 瓒呮椂涓嶈涓哄け璐ワ紝DPI浼氬湪Core1涓嬫澶勭悊鍛戒护鏃剁敓鏁?*/
    }

    return 0;
}

/* 瀹忛厤缃鍙栫储寮?*/
static uint8_t s_macro_read_id = 0;
static uint8_t s_macro_read_block = 0;

/* 閿欒鏃ュ織璇诲彇绱㈠紩 */
static uint8_t s_fault_read_index = 0;

#if FEATURE_PERF_MONITOR
/* 鎬ц兘鐩戞帶 - 浠诲姟璇诲彇绱㈠紩 */
static uint8_t s_perf_task_index = 0;
#endif

/* CONFIG_EXT2 褰撳墠瀛愬潡绱㈠紩锛圫ET_REPORT璁剧疆锛孏ET_REPORT杩斿洖瀵瑰簲瀛愬潡鏁版嵁锛?*/
static uint8_t s_config_ext2_subblock = 0;

/* 瀹忛厤缃剰鏍囧織锛堥渶瑕佷繚瀛樺埌Flash锛?*/
static bool s_macro_dirty = false;
static uint32_t s_macro_dirty_time_us = 0;

/* 鎺у埗鍛戒护鐮侊紙涓?config_hid.h 淇濇寔涓€鑷达級 */
#define CMD_SAVE_CONFIG   0x01
#define CMD_RESET_CONFIG  0x02
#define CMD_REBOOT        0x03
#define CMD_ENTER_DFU     0x04
#define CMD_APPLY_CONFIG  0x05
#define CMD_RESET_STATS   0x06
#define CMD_CLEAR_FAULT   0x07
#define CMD_RESET_PERF    0x08
#define CMD_MACRO_PLAY    0x09
#define CMD_MACRO_STOP    0x0A
#define CMD_SET_PERF_ENABLE 0x0B  /* 璁剧疆鎬ц兘鐩戞帶寮€鍏筹細data[1]=1寮€鍚?0鍏抽棴 */
#define CMD_SET_JOYSTICK_DZ_RT 0x0C  /* 瀹炴椂璁剧疆鎽囨潌姝诲尯锛堜笉鍐橣lash锛夛細data[1-2]=姝诲尯鍊?灏忕) */
#define CMD_UNLOCK_CONFIG   0x0D  /* 瑙ｉ攣閰嶇疆鍐欏叆锛氶渶杩炵画鍙戦€?娆?5绉掑唴)鎵嶈兘瑙ｉ攣 */
#define CMD_JOYSTICK_CALIBRATE 0x0E  /* 瑙﹀彂鎽囨潌涓績鐐规牎鍑嗭細data[1-2]=閲囨牱娆℃暟(灏忕锛岄粯璁?0) */
#define CMD_SWITCH_DPI        0x0F  /* 寰幆鍒囨崲 DPI 妗ｄ綅锛歞ata[1]=鐩爣妗ｄ綅(0-3, 0xFF=寰幆涓嬩竴妗? */
#define CMD_GET_VERSION       0x10  /* 鑾峰彇鐗堟湰淇℃伅锛堣繑鍥?ACK 涓惡甯︾増鏈級 */
#define CMD_NEGOTIATE_VERSION 0x11  /* 鐗堟湰鍗忓晢锛堝弬鏁帮細涓婁綅鏈哄崗璁増鏈級 */
#define CMD_SWITCH_LAYER      0x12  /* 鍒囨崲鎸夐敭灞傦紙鍙傛暟锛?-3 灞傚彿锛?*/

/* ACK/NACK 鍙戦€佸拰 DPI 杞崲宸茬Щ鑷?protocol/config_hid_helpers.c */
/* 閰嶇疆閿佸畾鏈哄埗宸茬Щ鑷?protocol/config_lock.c */
/* DFU/Reboot 鍛戒护纭鏈哄埗宸茬Щ鑷?protocol/command_confirm.c */

/* DPI鏁板€艰浆鏋氫妇宸茬Щ鑷?protocol/config_hid_helpers.c */

void config_hid_init(void)
{
    /* 鍒濆鍖栭厤缃攣瀹氭満鍒讹紙榛樿閿佸畾鐘舵€侊級 */
    config_lock_init();
    /* 鍒濆鍖栭厤缃潡鎺ユ敹鐘舵€侊紙娓呴浂鎺ユ敹浣嶅浘锛?*/
    config_block_receiver_init();
    /* 鍒濆鍖栧懡浠ょ‘璁ゆ満鍒讹紙娓呴浂 DFU/Reboot 纭璁℃暟锛?*/
    command_confirm_init();
}

/* ============================================================================
 * GET_REPORT 澶勭悊鍑芥暟锛堜粠 tud_hid_get_report_cb 鎷嗗垎锛屾瘡涓嚱鏁拌亴璐ｅ崟涓€锛?
 * ========================================================================== */

/* 澶勭悊閰嶇疆鍧楄鍙栵紙REPORT_ID_CONFIG_BLOCK0/1/2锛?*/
static uint16_t get_report_config_block(uint8_t report_id, uint8_t* buffer, uint16_t reqlen)
{
    const device_config_t* cfg = config_get();
    const uint8_t* cfg_bytes = (const uint8_t*)cfg;
    uint16_t cfg_size = sizeof(device_config_t);

    uint16_t offset;
    switch (report_id) {
        case REPORT_ID_CONFIG_BLOCK0: offset = 0; break;
        case REPORT_ID_CONFIG_BLOCK1: offset = CONFIG_BLOCK_SIZE; break;
        case REPORT_ID_CONFIG_BLOCK2: offset = CONFIG_BLOCK_SIZE * 2; break;
        default: return 0;
    }

    if (reqlen == 0) return 0;

    if (offset >= cfg_size) {
        /* 鍋忕Щ瓒呭嚭锛岃繑鍥炵┖鏁版嵁锛堜絾涓嶈秺鐣岋級 */
        memset(buffer, 0, reqlen);
        return (reqlen < CONFIG_BLOCK_SIZE) ? reqlen : CONFIG_BLOCK_SIZE;
    }

    uint16_t remaining = cfg_size - offset;
    uint16_t copy_len = (reqlen < remaining) ? reqlen : remaining;
    if (copy_len > CONFIG_BLOCK_SIZE) copy_len = CONFIG_BLOCK_SIZE;

    /* 浠呭啓鍏ヤ笉瓒呰繃 reqlen 鐨勬暟鎹?*/
    if (copy_len > 0) memcpy(buffer, cfg_bytes + offset, copy_len);

    if (copy_len < CONFIG_BLOCK_SIZE && reqlen > copy_len) {
        uint16_t fill_len = (reqlen < CONFIG_BLOCK_SIZE) ? reqlen : CONFIG_BLOCK_SIZE;
        memset(buffer + copy_len, 0, fill_len - copy_len);
        copy_len = fill_len;
    }

    return copy_len;
}

/* 澶勭悊璁惧淇℃伅璇诲彇锛圧EPORT_ID_DEVICE_INFO锛?*/
static uint16_t get_report_device_info(uint8_t* buffer, uint16_t reqlen)
{
    /* 璁惧淇℃伅鎶ュ憡锛?2瀛楄妭锛屽惈 report_id锛?
     * byte[0] = Report ID
     * byte[1-2] = 鍥轰欢鐗堟湰锛坆cdDevice锛?
     * byte[3-4] = 閰嶇疆鐗堟湰
     * byte[5-6] = 鍗忚鐗堟湰锛堥珮瀛楄妭=涓荤増鏈紝浣庡瓧鑺?娆＄増鏈級
     * byte[7-8] = 閰嶇疆缁撴瀯鎬诲ぇ灏?
     * byte[9] = 鏀寔鐨勬寜閿眰鏁?
     * byte[10] = 鏀寔鐨勫畯鏁伴噺
     * byte[11] = 鏈€澶ф敮鎸?DPI
     * byte[12] = 鏀寔鐨勮兘鍔涙暟閲?
     * byte[13-20] = 鑳藉姏浣嶅浘锛?4浣嶏紝姣忎綅浠ｈ〃涓€涓兘鍔涙槸鍚﹀瓨鍦級
     * byte[21] = device_flags锛坴0.2 鏂板锛氬埗閫?浼氳瘽鐘舵€佷綅 DEVICE_FLAG_*锛?
     * byte[22-31] = 棰勭暀锛堣€佷笂浣嶆満鎸夌煭鎶ュ憡瑙ｆ瀽锛岃嚜鍔ㄥ拷鐣ワ級
     */
    config_hid_device_info_t info;
    memset(&info, 0, sizeof(info));
    info.report_id = REPORT_ID_DEVICE_INFO;
    info.firmware_version = FIRMWARE_VERSION_BCD;  /* 鍥轰欢鐗堟湰锛堢粺涓€浠?version.h 寮曠敤锛?*/
    info.config_version = CONFIG_VERSION;
    info.protocol_version = CONFIG_HID_PROTOCOL_VERSION;
    info.config_size = (uint16_t)sizeof(device_config_t);
    info.layer_count = 4;  /* 鏀寔 4 灞傛寜閿槧灏?*/
    info.macro_count = MACRO_MAX_COUNT;
    info.max_dpi = 64;  /* 鏈€澶?DPI 妗ｄ綅绱㈠紩锛堝疄闄呭€?6400锛?*/

    /* 鑳藉姏浣嶅浘锛氭寜缂栬瘧鏈?FEATURE_* 寮€鍏?+ 鍒堕€犵姸鎬佽繍琛屾椂鎷艰锛堝姛鑳借鑹插垎绂伙級銆?
     * 鑳藉姏鍗忓晢鐨勬牳蹇冿細涓婁綅鏈烘嵁姝や綅鍥惧垽鏂姛鑳芥槸鍚﹀彲鐢紝涓嶅啀闈犵増鏈彿鎺ㄦ柇锛?
     * 鍥轰欢鍛戒护/鎶ュ憡闂搁棬涔熶互姝や负鍒ゆ嵁銆傚疄鐜拌 capability.c銆?
     */
    info.capability_count = CAP_COUNT;
    info.capability_bitmap = capability_bitmap_runtime();
    info.device_flags = capability_device_flags();

    uint16_t to_write = (reqlen < (uint16_t)(sizeof(info) - 1)) ? reqlen : (uint16_t)(sizeof(info) - 1);
    /* 娉ㄦ剰锛歵ud_hid_get_report_cb 鐨?buffer 涓嶅簲鍖呭惈 Report ID锛堝崗璁眰鑷姩娣诲姞锛夛紝
     * 浠?firmware_version 瀛楁寮€濮嬪鍒讹紝璺宠繃 report_id锛?瀛楄妭锛?*/
    memcpy(buffer, (const uint8_t*)&info + 1, to_write);
    return to_write;
}

/* 澶勭悊鎺у埗鍛戒护璇诲彇锛圧EPORT_ID_CONTROL锛屽凡搴熷純锛?*/
static uint16_t get_report_control(uint8_t* buffer, uint16_t reqlen)
{
    /* v3.0 鍗忚锛歊EPORT_ID_CONTROL(7) 宸叉敼涓?Output Report锛堜粎鐢ㄤ簬涓绘満鍙戦€佸懡浠わ級
     * GET_REPORT 涓嶅啀杩斿洖鏈夋晥鏁版嵁锛屽懡浠ゅ搷搴旈€氳繃 REPORT_ID_ACK(24) 璇诲彇
     */
    if (reqlen < 1) return 0;
    buffer[0] = 0;
    return 1;
}

/* 澶勭悊鍛戒护鍝嶅簲璇诲彇锛圧EPORT_ID_ACK锛?*/
static uint16_t get_report_ack(uint8_t* buffer, uint16_t reqlen)
{
    /* v3.1 鍗忚锛氬懡浠ゅ搷搴?Feature Report锛堝悓姝ヨ姹?鍝嶅簲妯″紡锛?
     * 涓婁綅鏈哄彂閫?Output 鍛戒护鍚庯紝绔嬪嵆 GET_REPORT(24) 璇诲彇鎵ц缁撴灉
     * 鏍煎紡: [status, cmd, sw_id, device_idx, param_low, param_high, reserved[2]]
     *   status=0 琛ㄧず鎴愬姛锛岄潪0涓洪敊璇爜
     * 璇诲彇鍚庢竻闄ゆ湁鏁堟爣蹇楋紝闃叉閲嶅璇诲彇鏃у搷搴?
     *
     * 绔炴€佹潯浠堕槻鎶わ細濡傛灉鍛戒护灏氭湭澶勭悊瀹屾垚锛坓_control_response_valid=false锛夛紝
     * 杩斿洖 BUSY 鐘舵€侊紝涓婁綅鏈哄簲閲嶈瘯 GET_REPORT锛岄伩鍏嶈鍒版棫鍝嶅簲鎴栧叏闆跺垵濮嬪搷搴斻€?
     */
    uint16_t to_write = (reqlen < 8) ? reqlen : 8;

    if (!g_control_response_valid) {
        /* 鍝嶅簲灏氭湭鍑嗗濂斤紝杩斿洖 BUSY 鐘舵€侊紙CONFIG_HID_ERR_BUSY=8锛?
         * cmd 瀛楁鍥炴樉褰撳墠寰呭鐞嗗懡浠わ紝sw_id/device_idx 鍥炴樉褰撳墠缂撳瓨鍊?*/
        buffer[RESP_OFFSET_STATUS] = CONFIG_HID_ERR_BUSY;
        buffer[RESP_OFFSET_CMD] = g_pending_cmd;
        buffer[RESP_OFFSET_SW_ID] = g_current_sw_id;
        buffer[RESP_OFFSET_DEVICE_IDX] = g_current_device_idx;
        buffer[RESP_OFFSET_PARAM_LOW] = 0;
        buffer[RESP_OFFSET_PARAM_HIGH] = 0;
        buffer[6] = 0;
        buffer[7] = 0;
        return to_write;
    }

    memcpy(buffer, g_control_response, to_write);
    g_control_response_valid = false;
    return to_write;
}

#if FEATURE_KEY_STATS
/* 澶勭悊鎸夐敭缁熻璇诲彇锛圧EPORT_ID_KEY_STATS0-3锛?*/
static uint16_t get_report_key_stats(uint8_t report_id, uint8_t* buffer, uint16_t reqlen)
{
    int start_idx = (report_id - REPORT_ID_KEY_STATS0) * 16;
    int count = 16;
    if (start_idx + count > 64) count = 64 - start_idx;

    int data_len = count * 2;
    uint16_t to_write = (reqlen < (uint16_t)data_len) ? (uint16_t)reqlen : (uint16_t)data_len;

    for (int i = 0; i < count; i++) {
        int off = i * 2;
        if ((uint16_t)off >= to_write) break;
        uint32_t cnt = key_stats_get_count(start_idx + i);
        uint16_t val = (uint16_t)(cnt & 0xFFFF);
        buffer[off] = (uint8_t)(val & 0xFF);
        if ((uint16_t)(off + 1) < to_write) buffer[off + 1] = (uint8_t)((val >> 8) & 0xFF);
    }
    return to_write;
}
#endif /* FEATURE_KEY_STATS */

/* 澶勭悊瀹忛厤缃鍙栵紙REPORT_ID_MACRO_CONFIG锛?*/
static uint16_t get_report_macro_config(uint8_t* buffer, uint16_t reqlen)
{
    if (reqlen < 2) return 0;
    uint8_t macro_id = s_macro_read_id;
    uint8_t block = s_macro_read_block;
    if (macro_id >= MACRO_MAX_COUNT) macro_id = 0;
    if (block > 2) block = 0;
    const macro_def_t* macro = macro_get(macro_id);
    buffer[0] = macro_id;
    buffer[1] = block;
    if (macro == NULL) {
        if (reqlen > 2) memset(&buffer[2], 0, (size_t)(reqlen - 2));
        return (reqlen > 62) ? 62 : reqlen;
    }
    uint16_t offset = block * 60;
    const uint8_t* macro_bytes = (const uint8_t*)macro;
    uint16_t copy_len = 60;
    if (offset + copy_len > sizeof(macro_def_t)) copy_len = (uint16_t)(sizeof(macro_def_t) - offset);
    uint16_t avail = (reqlen > 2) ? (uint16_t)(reqlen - 2) : 0;
    uint16_t write_len = (avail < copy_len) ? avail : copy_len;
    if (write_len > 0) memcpy(&buffer[2], macro_bytes + offset, write_len);
    if (avail > write_len) memset(&buffer[2 + write_len], 0, (size_t)(avail - write_len));
    LOG_DEBUG_PRINT("[瀹廬 璇诲彇瀹?%d 鍧?%d锛屾湡鏈?%d锛屽疄闄呭啓鍏?%d\n", macro_id, block, copy_len, (write_len + 2));
    return (reqlen > 62) ? 62 : reqlen;
}

#if FEATURE_PERF_MONITOR
/* 澶勭悊鎬ц兘绯荤粺缁熻璇诲彇锛圧EPORT_ID_PERF_SYSTEM锛?*/
static uint16_t get_report_perf_system(uint8_t* buffer, uint16_t reqlen)
{
    perf_system_stat_t sys_stat;
    perf_get_system_stat(&sys_stat);
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);
    if (reqlen > 0) buffer[0] = (uint8_t)(sys_stat.cpu_usage & 0xFF);
    if (reqlen > 1) buffer[1] = (uint8_t)(sys_stat.loop_freq_hz & 0xFF);
    if (reqlen > 2) buffer[2] = (uint8_t)((sys_stat.loop_freq_hz >> 8) & 0xFF);
    if (reqlen > 3) buffer[3] = (uint8_t)(sys_stat.uptime_s & 0xFF);
    if (reqlen > 4) buffer[4] = (uint8_t)((sys_stat.uptime_s >> 8) & 0xFF);
    if (reqlen > 5) buffer[5] = (uint8_t)((sys_stat.uptime_s >> 16) & 0xFF);
    if (reqlen > 6) buffer[6] = (uint8_t)((sys_stat.uptime_s >> 24) & 0xFF);
    if (reqlen > 7) buffer[7] = perf_get_task_count();
    if (reqlen > 8) buffer[8] = sys_stat.cpu_usage_avg_10s;
    if (reqlen > 9) buffer[9] = sys_stat.cpu_usage_avg_30s;
    if (reqlen > 10) buffer[10] = (uint8_t)(sys_stat.loop_freq_avg_10s & 0xFF);
    if (reqlen > 11) buffer[11] = (uint8_t)((sys_stat.loop_freq_avg_10s >> 8) & 0xFF);
    return (reqlen > 62) ? 62 : reqlen;
}

/* 澶勭悊鎬ц兘浠诲姟缁熻璇诲彇锛圧EPORT_ID_PERF_TASK锛?*/
static uint16_t get_report_perf_task(uint8_t* buffer, uint16_t reqlen)
{
    uint8_t index = s_perf_task_index;
    perf_task_stat_t task_stat;
    bool valid = perf_get_task_stat(index, &task_stat);
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);
    if (reqlen > 0) buffer[0] = index;
    if (reqlen > 1) buffer[1] = valid ? 1 : 0;
    if (valid) {
        uint32_t avg_us = task_stat.count > 0 ? (task_stat.total_us / task_stat.count) : 0;
        if (reqlen > 2) buffer[2] = (uint8_t)(task_stat.count & 0xFF);
        if (reqlen > 3) buffer[3] = (uint8_t)((task_stat.count >> 8) & 0xFF);
        if (reqlen > 4) buffer[4] = (uint8_t)((task_stat.count >> 16) & 0xFF);
        if (reqlen > 5) buffer[5] = (uint8_t)((task_stat.count >> 24) & 0xFF);
        if (reqlen > 6) buffer[6] = (uint8_t)(task_stat.min_us & 0xFF);
        if (reqlen > 7) buffer[7] = (uint8_t)((task_stat.min_us >> 8) & 0xFF);
        if (reqlen > 8) buffer[8] = (uint8_t)((task_stat.min_us >> 16) & 0xFF);
        if (reqlen > 9) buffer[9] = (uint8_t)((task_stat.min_us >> 24) & 0xFF);
        if (reqlen > 10) buffer[10] = (uint8_t)(task_stat.max_us & 0xFF);
        if (reqlen > 11) buffer[11] = (uint8_t)((task_stat.max_us >> 8) & 0xFF);
        if (reqlen > 12) buffer[12] = (uint8_t)((task_stat.max_us >> 16) & 0xFF);
        if (reqlen > 13) buffer[13] = (uint8_t)((task_stat.max_us >> 24) & 0xFF);
        if (reqlen > 14) buffer[14] = (uint8_t)(avg_us & 0xFF);
        if (reqlen > 15) buffer[15] = (uint8_t)((avg_us >> 8) & 0xFF);
        if (reqlen > 16) buffer[16] = (uint8_t)((avg_us >> 16) & 0xFF);
        if (reqlen > 17) buffer[17] = (uint8_t)((avg_us >> 24) & 0xFF);
        if (reqlen > 18) buffer[18] = (uint8_t)(task_stat.last_us & 0xFF);
        if (reqlen > 19) buffer[19] = (uint8_t)((task_stat.last_us >> 8) & 0xFF);
        if (reqlen > 20) buffer[20] = (uint8_t)((task_stat.last_us >> 16) & 0xFF);
        if (reqlen > 21) buffer[21] = (uint8_t)((task_stat.last_us >> 24) & 0xFF);
        if (reqlen > 22) buffer[22] = task_stat.cpu_percent;
        if (reqlen > 23) buffer[23] = (uint8_t)(task_stat.overrun_count & 0xFF);
        if (reqlen > 24) buffer[24] = (uint8_t)((task_stat.overrun_count >> 8) & 0xFF);
        if (reqlen > 25) buffer[25] = (uint8_t)((task_stat.overrun_count >> 16) & 0xFF);
        if (reqlen > 26) buffer[26] = (uint8_t)((task_stat.overrun_count >> 24) & 0xFF);
        if (reqlen > 27) buffer[27] = (uint8_t)(task_stat.threshold_us & 0xFF);
        if (reqlen > 28) buffer[28] = (uint8_t)((task_stat.threshold_us >> 8) & 0xFF);
        if (reqlen > 29) buffer[29] = (uint8_t)((task_stat.threshold_us >> 16) & 0xFF);
        if (reqlen > 30) buffer[30] = (uint8_t)((task_stat.threshold_us >> 24) & 0xFF);
        if (task_stat.name != NULL && reqlen > 31) {
            uint16_t max_name = (uint16_t)(reqlen - 31);
            if (max_name > 31) max_name = 31;
            uint8_t name_len = 0;
            while (task_stat.name[name_len] != '\0' && name_len < (int)max_name - 1) {
                buffer[31 + name_len] = (uint8_t)task_stat.name[name_len];
                name_len++;
            }
            buffer[31 + name_len] = 0;
        } else if (reqlen > 31) {
            buffer[31] = 0;
        }
    }
    return (reqlen > 62) ? 62 : reqlen;
}
#endif /* FEATURE_PERF_MONITOR */

/* 澶勭悊閿欒鏃ュ織淇℃伅璇诲彇锛圧EPORT_ID_FAULT_INFO锛?*/
static uint16_t get_report_fault_info(uint8_t* buffer, uint16_t reqlen)
{
    uint32_t log_count = fault_get_log_count();
    uint32_t total_count = fault_get_count();
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);
    if (reqlen > 0) buffer[0] = (uint8_t)(log_count & 0xFF);
    if (reqlen > 1) buffer[1] = (uint8_t)((log_count >> 8) & 0xFF);
    if (reqlen > 2) buffer[2] = (uint8_t)((log_count >> 16) & 0xFF);
    if (reqlen > 3) buffer[3] = (uint8_t)((log_count >> 24) & 0xFF);
    if (reqlen > 4) buffer[4] = (uint8_t)(total_count & 0xFF);
    if (reqlen > 5) buffer[5] = (uint8_t)((total_count >> 8) & 0xFF);
    if (reqlen > 6) buffer[6] = (uint8_t)((total_count >> 16) & 0xFF);
    if (reqlen > 7) buffer[7] = (uint8_t)((total_count >> 24) & 0xFF);
    return (reqlen > 62) ? 62 : reqlen;
}

/* 澶勭悊閿欒鏃ュ織璇诲彇锛圧EPORT_ID_FAULT_LOG锛?*/
static uint16_t get_report_fault_log(uint8_t* buffer, uint16_t reqlen)
{
    uint8_t index = s_fault_read_index;
    fault_log_entry_t entry;
    bool valid = fault_get_log(index, &entry);
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);
    if (reqlen > 0) buffer[0] = index;
    if (reqlen > 1) buffer[1] = valid ? 1 : 0;
    if (valid) {
        if (reqlen > 2) buffer[2] = (uint8_t)(entry.timestamp_ms & 0xFF);
        if (reqlen > 3) buffer[3] = (uint8_t)((entry.timestamp_ms >> 8) & 0xFF);
        if (reqlen > 4) buffer[4] = (uint8_t)((entry.timestamp_ms >> 16) & 0xFF);
        if (reqlen > 5) buffer[5] = (uint8_t)((entry.timestamp_ms >> 24) & 0xFF);
        if (reqlen > 6) buffer[6] = (uint8_t)entry.level;
        if (reqlen > 7) buffer[7] = entry.module_len;
        if (reqlen > 8) {
            uint16_t max_mod = (uint16_t)(reqlen - 8);
            if (max_mod > 32) max_mod = 32;
            int module_len = entry.module_len;
            if (module_len > (int)max_mod - 1) module_len = (int)max_mod - 1;
            if (module_len < 0) module_len = 0;
            if (module_len > 0) memcpy(&buffer[8], entry.module, (size_t)module_len);
            if ((uint16_t)(8 + module_len) < reqlen) buffer[8 + module_len] = 0;
        }
        if (reqlen > 40) {
            uint16_t max_msg = (uint16_t)(reqlen - 40);
            if (max_msg > 22) max_msg = 22;
            int msg_len = entry.msg_len;
            if (msg_len > (int)max_msg - 1) msg_len = (int)max_msg - 1;
            if (msg_len < 0) msg_len = 0;
            if (msg_len > 0) memcpy(&buffer[40], entry.msg, (size_t)msg_len);
            if ((uint16_t)(40 + msg_len) < reqlen) buffer[40 + msg_len] = 0;
        }
    }
    return (reqlen > 62) ? 62 : reqlen;
}

#if FEATURE_RAW_DIAG
/* 澶勭悊瀹炴椂鎸夐敭鐘舵€佽鍙栵紙REPORT_ID_KEY_STATE锛?*/
static uint16_t get_report_key_state(uint8_t* buffer, uint16_t reqlen)
{
    /* 瀹炴椂鎸夐敭鐘舵€侊細64浣峛itmap锛宐it=1琛ㄧず鎸変笅 */
    uint64_t keys = shared_hw_get_keys();
    /* keypad_spi璇诲彇鐨勬槸bit=0琛ㄧず鎸変笅锛岃繖閲岃浆鎹负bit=1琛ㄧず鎸変笅 */
    uint64_t pressed = ~keys;
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);
    uint8_t max_bytes = (reqlen > 8) ? 8 : (uint8_t)reqlen;
    for (uint8_t i = 0; i < max_bytes; i++) {
        buffer[i] = (uint8_t)((pressed >> (i * 8)) & 0xFF);
    }
    return (reqlen > 8) ? 8 : reqlen;
}

/* 澶勭悊瀹炴椂鎽囨潌鐘舵€佽鍙栵紙REPORT_ID_JOYSTICK_STATE锛?*/
static uint16_t get_report_joystick_state(uint8_t* buffer, uint16_t reqlen)
{
    /* 瀹炴椂鎽囨潌鐘舵€侊細x, y, btn */
    int16_t x, y;
    bool btn;
    shared_hw_get_joystick(&x, &y, &btn);
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);
    if (reqlen > 0) buffer[0] = (uint8_t)(int8_t)x;   /* 鏈夌鍙峰瓧鑺?*/
    if (reqlen > 1) buffer[1] = (uint8_t)(int8_t)y;   /* 鏈夌鍙峰瓧鑺?*/
    if (reqlen > 2) buffer[2] = btn ? 1 : 0;
    return (reqlen > 8) ? 8 : reqlen;
}
#endif /* FEATURE_RAW_DIAG */

/* 澶勭悊鎽囨潌鏍″噯鐘舵€佽鍙栵紙REPORT_ID_JOYSTICK_CALIB锛?*/
static uint16_t get_report_joystick_calib(uint8_t* buffer, uint16_t reqlen)
{
    /* 鎽囨潌鏍″噯鐘舵€佹煡璇細
     * byte[0] = 鏍″噯鐘舵€侊紙0=绌洪棽,1=鏍″噯涓?2=瀹屾垚锛?
     * byte[1-2] = center_x锛堝皬绔紝ADC鍊硷級
     * byte[3-4] = center_y锛堝皬绔紝ADC鍊硷級
     * byte[5] = calibrated锛?=鏈牎鍑?1=宸叉牎鍑嗭級
     * byte[6-7] = 姝诲尯鍊硷紙灏忕锛孉DC鍘熷鍊硷級
     * byte[8-9] = 鐏垫晱搴︼紙灏忕锛屽畾鐐规暟1.0=1000锛?
     */
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);

    uint8_t calib_state = shared_hw_get_joystick_calibration_state();
    uint16_t center_x = 2048, center_y = 2048;
    uint8_t calibrated = 0;
    shared_hw_get_joystick_calibration_data(&center_x, &center_y, &calibrated);

    const device_config_t *cfg = config_get();

    if (reqlen > 0) buffer[0] = calib_state;
    if (reqlen > 1) buffer[1] = (uint8_t)(center_x & 0xFF);
    if (reqlen > 2) buffer[2] = (uint8_t)((center_x >> 8) & 0xFF);
    if (reqlen > 3) buffer[3] = (uint8_t)(center_y & 0xFF);
    if (reqlen > 4) buffer[4] = (uint8_t)((center_y >> 8) & 0xFF);
    if (reqlen > 5) buffer[5] = calibrated;
    if (reqlen > 6) buffer[6] = (uint8_t)(cfg->joystick_deadzone & 0xFF);
    if (reqlen > 7) buffer[7] = (uint8_t)((cfg->joystick_deadzone >> 8) & 0xFF);
    if (reqlen > 8) buffer[8] = (uint8_t)(cfg->joystick_sensitivity & 0xFF);
    if (reqlen > 9) buffer[9] = (uint8_t)((cfg->joystick_sensitivity >> 8) & 0xFF);

    return (reqlen > 10) ? 10 : reqlen;
}

/* 澶勭悊閰嶇疆鎵╁睍瀛楁璇诲彇锛圧EPORT_ID_CONFIG_EXT锛?*/
static uint16_t get_report_config_ext(uint8_t* buffer, uint16_t reqlen)
{
    /* v3/v4/v5 閰嶇疆鎵╁睍瀛楁锛氬亸绉?326+锛屽叡18瀛楄妭
     * 涓?usb_descriptors.c 涓?REPORT_ID_CONFIG_EXT 鎻忚堪绗﹀０鏄庣殑 18 瀛楄妭涓€鑷达紝
     * 涓庝笂浣嶆満 DeviceService.cs 涓鍐欑殑 18 瀛楄妭涓€鑷淬€?
     * 鍖呭惈: joystick_invert_x(1), joystick_invert_y(1), encoder_steps(1),
     *       encoder_scroll_speed(1), joystick_sensitivity(2), reserved2(2),
     *       length(2), write_count(4), joystick_center_x(2), joystick_center_y(2),
     *       joystick_calibrated(1), reserved3(1) = 18 瀛楄妭
     */
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);
    uint8_t* ext_bytes = (uint8_t*)&g_tmp_config + 1326;
    uint16_t copy_len = (reqlen < 18) ? reqlen : 18;
    memcpy(buffer, ext_bytes, copy_len);
    return (reqlen > 18) ? 18 : reqlen;
}

/* 澶勭悊閰嶇疆鎵╁睍鍧?璇诲彇锛圧EPORT_ID_CONFIG_EXT2锛?*/
static uint16_t get_report_config_ext2(uint8_t* buffer, uint16_t reqlen)
{
    /* v6/v7閰嶇疆鎵╁睍鍧?锛堝瓙鍧楃储寮曟満鍒讹級
     * GET_REPORT: 杩斿洖褰撳墠瀛愬潡鐨勬暟鎹紙瀛愬潡绱㈠紩鐢⊿ET_REPORT璁剧疆锛?
     * SET_REPORT: byte[0]=瀛愬潡绱㈠紩锛岃缃綋鍓嶅瓙鍧楀苟鍙啓鍏ユ暟鎹?
     * 瀛愬潡瀹氫箟:
     *   0 = v6瀛楁 (鍋忕Щ1346, 14瀛楄妭)
     *   1 = layer2_keymap[0..31] (鍋忕Щ1360, 32瀛楄妭)
     *   2 = layer2_keymap[32..63] (鍋忕Щ1392, 32瀛楄妭)
     *   3 = layer3_keymap[0..31] (鍋忕Щ1424, 32瀛楄妭)
     *   4 = layer3_keymap[32..63] (鍋忕Щ1456, 32瀛楄妭)
     */
    if (reqlen == 0) return 0;
    memset(buffer, 0, reqlen);

    uint16_t offset = 0;
    uint16_t data_len = 0;

    switch (s_config_ext2_subblock)
    {
        case 0: offset = 1346; data_len = 14; break;  // v6瀛楁
        case 1: offset = 1360; data_len = 32; break;  // layer2_keymap[0..31]
        case 2: offset = 1392; data_len = 32; break;  // layer2_keymap[32..63]
        case 3: offset = 1424; data_len = 32; break;  // layer3_keymap[0..31]
        case 4: offset = 1456; data_len = 32; break;  // layer3_keymap[32..63]
        default: return 0;  // 鏃犳晥瀛愬潡绱㈠紩
    }

    uint16_t copy_len = (reqlen < data_len) ? reqlen : data_len;
    if (offset + copy_len <= sizeof(device_config_t))
    {
        memcpy(buffer, (uint8_t*)&g_tmp_config + offset, copy_len);
    }
    LOG_INFO_PRINT("[CONFIG] GET CONFIG_EXT2 sub_block=%d offset=%d len=%d\n", s_config_ext2_subblock, offset, copy_len);
    return (reqlen > copy_len) ? copy_len : reqlen;
}

/* GET_REPORT 鍥炶皟锛氬彧澶勭悊 Feature 鎶ュ憡锛堥厤缃帴鍙ｏ級 */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    /* 鍙鐞嗛厤缃帴鍙ｏ紙instance 1锛夌殑 Feature 鎶ュ憡 */
    if (instance != ITF_NUM_HID_CONFIG) return 0;
    if (report_type != HID_REPORT_TYPE_FEATURE) return 0;

    /* 鎶ュ憡鑳藉姏闂搁棬锛氱己灏戝搴旇瘖鏂兘鍔涗綅锛坮elease 鏈紪鍏ワ級鏃惰繑鍥炵┖鎶ュ憡锛?
     * 闃叉缁曡繃涓婁綅鏈虹洿鎺?raw GET_REPORT 璇诲彇璇婃柇鏁版嵁 */
    {
        uint64_t required_cap = command_gate_report_required_cap(report_id);
        if (required_cap != 0 && (capability_bitmap_runtime() & required_cap) == 0) {
            return 0;
        }
    }

    /* 涓存椂鍏抽棴棰戠箒鎵撳嵃锛岄伩鍏嶅共鎵拌皟璇?
    LOG_INFO_PRINT("[CONFIG] 涓绘満璇诲彇 Feature 鎶ュ憡: Report ID=%d, 璇锋眰闀垮害=%d\n", report_id, reqlen);
    */

    /* 鍒嗗彂鍒板搴旂殑澶勭悊鍑芥暟锛堟瘡涓嚱鏁拌亴璐ｅ崟涓€锛屼究浜庣淮鎶ゅ拰娴嬭瘯锛?*/
    switch (report_id)
    {
        case REPORT_ID_CONFIG_BLOCK0:
        case REPORT_ID_CONFIG_BLOCK1:
        case REPORT_ID_CONFIG_BLOCK2:
            return get_report_config_block(report_id, buffer, reqlen);

        case REPORT_ID_DEVICE_INFO:
            return get_report_device_info(buffer, reqlen);

        case REPORT_ID_CONTROL:
            return get_report_control(buffer, reqlen);

        case REPORT_ID_ACK:
            return get_report_ack(buffer, reqlen);

#if FEATURE_KEY_STATS
        case REPORT_ID_KEY_STATS0:
        case REPORT_ID_KEY_STATS1:
        case REPORT_ID_KEY_STATS2:
        case REPORT_ID_KEY_STATS3:
            return get_report_key_stats(report_id, buffer, reqlen);
#endif

        case REPORT_ID_MACRO_CONFIG:
            return get_report_macro_config(buffer, reqlen);

#if FEATURE_PERF_MONITOR
        case REPORT_ID_PERF_SYSTEM:
            return get_report_perf_system(buffer, reqlen);

        case REPORT_ID_PERF_TASK:
            return get_report_perf_task(buffer, reqlen);
#endif

        case REPORT_ID_FAULT_INFO:
            return get_report_fault_info(buffer, reqlen);

        case REPORT_ID_FAULT_LOG:
            return get_report_fault_log(buffer, reqlen);

#if FEATURE_RAW_DIAG
        case REPORT_ID_KEY_STATE:
            return get_report_key_state(buffer, reqlen);

        case REPORT_ID_JOYSTICK_STATE:
            return get_report_joystick_state(buffer, reqlen);
#endif /* FEATURE_RAW_DIAG */

        case REPORT_ID_JOYSTICK_CALIB:
            return get_report_joystick_calib(buffer, reqlen);

        case REPORT_ID_CONFIG_EXT:
            return get_report_config_ext(buffer, reqlen);

        case REPORT_ID_CONFIG_EXT2:
            return get_report_config_ext2(buffer, reqlen);

        default:
            return 0;
    }
}

/* ============================================================================
 * SET_REPORT 澶勭悊鍑芥暟锛堜粠 tud_hid_set_report_cb 鎷嗗垎锛屾瘡涓嚱鏁拌亴璐ｅ崟涓€锛?
 * ========================================================================== */

/* 澶勭悊閰嶇疆鍧楀啓鍏ワ紙REPORT_ID_CONFIG_BLOCK0/1/2锛?*/
static void set_report_config_block(uint8_t report_id, uint8_t const* buffer, uint16_t bufsize)
{
    if (config_lock_is_locked()) {
        LOG_ERROR_PRINT("[CONFIG] 鉂?閰嶇疆宸查攣瀹氾紝鎷掔粷鍐欏叆閰嶇疆鍧梊n");
        return;
    }
    uint8_t* cfg_bytes = (uint8_t*)&g_tmp_config;
    uint16_t cfg_size = sizeof(device_config_t);
    uint16_t offset;
    uint8_t block_bit;
    config_block_receiver_get_block_info(report_id, &offset, &block_bit);
    LOG_INFO_PRINT("[CONFIG] 鏀跺埌閰嶇疆鍧?%d (Report ID=%d, 鍋忕Щ=%d, 闀垮害=%d)\n", report_id - 5, report_id, offset, bufsize);
    if (offset < cfg_size) {
        uint16_t remaining = cfg_size - offset;
        uint16_t copy_len = (bufsize < remaining) ? bufsize : remaining;
        if (copy_len > CONFIG_BLOCK_SIZE) copy_len = CONFIG_BLOCK_SIZE;
        if (copy_len > 0) {
            memcpy(cfg_bytes + offset, buffer, copy_len);
            g_config_pending = true;
            config_block_receiver_mark_block(block_bit);
        }
        LOG_DEBUG_PRINT("[CONFIG] 閰嶇疆鍧?%d 鍐欏叆鎴愬姛锛屽鍒?%d 瀛楄妭\n", report_id - 5, copy_len);
    }
}

/* 澶勭悊鎺у埗鍛戒护锛圧EPORT_ID_CONTROL锛?*/
static void set_report_control(uint8_t const* buffer, uint16_t bufsize)
{
    if (bufsize < 3) return;
    g_current_device_idx = buffer[CMD_OFFSET_DEVICE_IDX];
    g_current_sw_id = buffer[CMD_OFFSET_SW_ID];
    uint8_t cmd = buffer[CMD_OFFSET_CMD];

    if (cmd == CMD_UNLOCK_CONFIG) {
        uint8_t progress = config_lock_attempt_unlock();
        if (progress >= CONFIG_LOCK_UNLOCK_REQUIRED_ATTEMPTS) {
            LOG_INFO_PRINT("[SECURITY] 鉁?閰嶇疆宸茶В閿侊紙30绉掑悗鑷姩閿佸畾锛塡n");
            config_hid_set_response(CMD_UNLOCK_CONFIG, true, CONFIG_HID_OK, 1);
        } else {
            LOG_INFO_PRINT("[SECURITY] 瑙ｉ攣杩涘害: %d/%d\n", progress, CONFIG_LOCK_UNLOCK_REQUIRED_ATTEMPTS);
            config_hid_set_response(CMD_UNLOCK_CONFIG, true, CONFIG_HID_OK, progress);
        }
        return;
    }

    if (config_lock_is_locked() && !config_lock_is_command_allowed(cmd)) {
        LOG_ERROR_PRINT("[SECURITY] 鉂?閰嶇疆宸查攣瀹氾紝鎷掔粷鍛戒护 0x%02X\n", cmd);
        return;
    }

    config_lock_notify_write();
    g_pending_cmd = cmd;
    if (bufsize >= 5) {
        g_pending_cmd_param = (uint16_t)buffer[CMD_OFFSET_PARAM_LOW] | ((uint16_t)buffer[CMD_OFFSET_PARAM_HIGH] << 8);
    } else if (bufsize >= 4) {
        g_pending_cmd_param = buffer[CMD_OFFSET_PARAM_LOW];
    } else {
        g_pending_cmd_param = 0;
    }

    if (cmd == CMD_GET_CAPABILITY_INFO) {
        LOG_DEBUG_PRINT("[CONFIG] cmd: 0x%02X, param: 0x%04X, dev_idx: 0x%02X, sw_id: %d\n",
                         cmd, g_pending_cmd_param, g_current_device_idx, g_current_sw_id);
    } else {
        LOG_INFO_PRINT("[CONFIG] cmd: 0x%02X, param: 0x%04X, dev_idx: 0x%02X, sw_id: %d\n",
                       cmd, g_pending_cmd_param, g_current_device_idx, g_current_sw_id);
    }
}

/* 澶勭悊瀹忛厤缃啓鍏ワ紙REPORT_ID_MACRO_CONFIG锛?*/
static void set_report_macro_config(uint8_t const* buffer, uint16_t bufsize)
{
    if (bufsize < 2) return;
    uint8_t macro_id = buffer[0];
    uint8_t block_raw = buffer[1];
    bool is_read_cmd = (block_raw & MACRO_BLOCK_READ_FLAG) != 0;
    uint8_t block = block_raw & MACRO_BLOCK_INDEX_MASK;

    if (!is_read_cmd && config_lock_is_locked()) {
        LOG_ERROR_PRINT("[SECURITY] 鉂?閰嶇疆宸查攣瀹氾紝鎷掔粷鍐欏叆瀹忛厤缃甛n");
        return;
    }
    if (!is_read_cmd) config_lock_notify_write();

    LOG_INFO_PRINT("[瀹廬 鏀跺埌瀹忛厤缃? 瀹廔D=%d, 鍧?%d, 闀垮害=%d, 璇诲懡浠?%d\n", macro_id, block, bufsize, is_read_cmd);
    if (macro_id >= MACRO_MAX_COUNT || block > 2) return;
    s_macro_read_id = macro_id;
    s_macro_read_block = block;
    if (!is_read_cmd && bufsize > 2) {
        macro_def_t tmp_macro;
        const macro_def_t* current = macro_get(macro_id);
        if (current != NULL) memcpy(&tmp_macro, current, sizeof(macro_def_t));
        else { memset(&tmp_macro, 0, sizeof(macro_def_t)); tmp_macro.id = macro_id; }
        uint16_t offset = block * 60;
        uint8_t* macro_bytes = (uint8_t*)&tmp_macro;
        uint16_t copy_len = bufsize - 2;
        if (offset + copy_len > sizeof(macro_def_t)) copy_len = sizeof(macro_def_t) - offset;
        if (copy_len > 0) memcpy(macro_bytes + offset, &buffer[2], copy_len);
        tmp_macro.id = macro_id;
        if (tmp_macro.action_count > MACRO_MAX_ACTIONS) tmp_macro.action_count = MACRO_MAX_ACTIONS;
        macro_set(macro_id, &tmp_macro);
        s_macro_dirty = true;
        s_macro_dirty_time_us = time_us_32();
        LOG_DEBUG_PRINT("[瀹廬 鍐欏叆瀹忛厤缃? 瀹廔D=%d, 鍧?%d, 闀垮害=%d\n", macro_id, block, copy_len);
    }
}

/* 澶勭悊閿欒鏃ュ織绱㈠紩璁剧疆锛圧EPORT_ID_FAULT_LOG锛?*/
static void set_report_fault_log(uint8_t const* buffer, uint16_t bufsize)
{
    /* 鑳藉姏闂搁棬锛氭湭缂栧叆 Fault 妯″潡鏃跺拷鐣ョ储寮曢€夋嫨 */
    if ((capability_bitmap_runtime() & (1ULL << CAP_FAULT_LOG)) == 0) return;
    if (bufsize >= 1) s_fault_read_index = buffer[0];
}

#if FEATURE_PERF_MONITOR
/* 澶勭悊鎬ц兘浠诲姟绱㈠紩璁剧疆锛圧EPORT_ID_PERF_TASK锛?*/
static void set_report_perf_task(uint8_t const* buffer, uint16_t bufsize)
{
    /* 鑳藉姏闂搁棬锛氭湭缂栧叆鎬ц兘鐩戞帶鏃跺拷鐣ョ储寮曢€夋嫨 */
    if ((capability_bitmap_runtime() & (1ULL << CAP_PERF_MONITOR)) == 0) return;
    if (bufsize >= 1) s_perf_task_index = buffer[0];
}
#endif /* FEATURE_PERF_MONITOR */

/* 澶勭悊閰嶇疆鎵╁睍瀛楁鍐欏叆锛圧EPORT_ID_CONFIG_EXT锛?*/
static void set_report_config_ext(uint8_t const* buffer, uint16_t bufsize)
{
    if (config_lock_is_locked()) {
        LOG_ERROR_PRINT("[SECURITY] 鉂?閰嶇疆宸查攣瀹氾紝鎷掔粷鍐欏叆鎵╁睍瀛楁\n");
        return;
    }
    if (bufsize > 0 && buffer != NULL) {
        uint8_t* ext_bytes = (uint8_t*)&g_tmp_config + 1326;
        uint16_t copy_len = (bufsize < 18) ? bufsize : 18;
        memcpy(ext_bytes, buffer, copy_len);
        LOG_INFO_PRINT("[CONFIG] 鏀跺埌閰嶇疆鎵╁睍瀛楁 (%d瀛楄妭)\n", copy_len);
    }
}

/* 澶勭悊閰嶇疆鎵╁睍鍧?鍐欏叆锛圧EPORT_ID_CONFIG_EXT2锛?*/
static void set_report_config_ext2(uint8_t const* buffer, uint16_t bufsize)
{
    if (bufsize < 1 || buffer == NULL) return;
    uint8_t sub_block = buffer[0];
    if (sub_block > 4) {
        LOG_ERROR_PRINT("[CONFIG] 鉂?CONFIG_EXT2 鏃犳晥瀛愬潡绱㈠紩: %d\n", sub_block);
        return;
    }
    s_config_ext2_subblock = sub_block;

    if (bufsize > 1) {
        if (config_lock_is_locked()) {
            LOG_ERROR_PRINT("[SECURITY] 鉂?閰嶇疆宸查攣瀹氾紝鎷掔粷鍐欏叆 CONFIG_EXT2 瀛愬潡 %d\n", sub_block);
            return;
        }
        uint16_t offset = 0, data_len = 0;
        switch (sub_block) {
            case 0: offset = 1346; data_len = 14; break;
            case 1: offset = 1360; data_len = 32; break;
            case 2: offset = 1392; data_len = 32; break;
            case 3: offset = 1424; data_len = 32; break;
            case 4: offset = 1456; data_len = 32; break;
        }
        uint16_t copy_len = (bufsize - 1 < data_len) ? (bufsize - 1) : data_len;
        if (offset + copy_len <= sizeof(device_config_t)) {
            memcpy((uint8_t*)&g_tmp_config + offset, buffer + 1, copy_len);
            LOG_INFO_PRINT("[CONFIG] 鏀跺埌 CONFIG_EXT2 瀛愬潡 %d (鍋忕Щ=%d, 鍐欏叆=%d瀛楄妭)\n", sub_block, offset, copy_len);
        }
    } else {
        LOG_INFO_PRINT("[CONFIG] 璁剧疆 CONFIG_EXT2 瀛愬潡绱㈠紩: %d\n", sub_block);
    }
}

/* SET_REPORT 鍥炶皟锛團eature 鍐欏叆/Output 鍛戒护澶勭悊锛?*/
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    /* 鍙鐞嗛厤缃帴鍙ｏ紙instance 1锛?*/
    if (instance != ITF_NUM_HID_CONFIG) return;

    /* v3.0 鍗忚锛氭敮鎸?Feature Report锛堥厤缃啓鍏ワ級鍜?Output Report锛堝懡浠ゅ彂閫侊紝涓柇浼犺緭锛?
     * Output Report(Report ID=7) 鐢ㄤ簬楂橀€熷懡浠ゅ彂閫侊紝璧?USB 涓柇浼犺緭
     */
    if (report_type != HID_REPORT_TYPE_FEATURE && report_type != HID_REPORT_TYPE_OUTPUT) return;

    /* 闃插尽鎬ф牎楠岋細Feature Report 鏈€澶?64 瀛楄妭锛堝惈 Report ID锛夛紝寮傚父澶х殑 bufsize 鐩存帴鎷掔粷 */
    if (bufsize > 64) {
        LOG_ERROR_PRINT("[SECURITY] 鉂?寮傚父鐨?Feature Report 闀垮害: %d锛堟渶澶?4锛塡n", bufsize);
        return;
    }
    if (buffer == NULL) return;

    /* 鍒嗗彂鍒板搴旂殑澶勭悊鍑芥暟锛堟瘡涓嚱鏁拌亴璐ｅ崟涓€锛屼究浜庣淮鎶ゅ拰娴嬭瘯锛?*/
    switch (report_id)
    {
        case REPORT_ID_CONFIG_BLOCK0:
        case REPORT_ID_CONFIG_BLOCK1:
        case REPORT_ID_CONFIG_BLOCK2:
            set_report_config_block(report_id, buffer, bufsize);
            break;

        case REPORT_ID_CONTROL:
            set_report_control(buffer, bufsize);
            break;

        case REPORT_ID_MACRO_CONFIG:
            set_report_macro_config(buffer, bufsize);
            break;

        case REPORT_ID_FAULT_LOG:
            set_report_fault_log(buffer, bufsize);
            break;

#if FEATURE_PERF_MONITOR
        case REPORT_ID_PERF_TASK:
            set_report_perf_task(buffer, bufsize);
            break;
#endif

        case REPORT_ID_CONFIG_EXT:
            set_report_config_ext(buffer, bufsize);
            break;

        case REPORT_ID_CONFIG_EXT2:
            set_report_config_ext2(buffer, bufsize);
            break;

        default:
            LOG_DEBUG_PRINT("[CONFIG] 鏈煡 Report ID: %d\n", report_id);
            break;
    }
}

/* ============================================================================
 * 鍛戒护澶勭悊鍑芥暟锛堜粠 hid_config_task 鎷嗗垎锛屾瘡涓嚱鏁拌亴璐ｅ崟涓€锛?
 * ========================================================================== */

/* 澶勭悊淇濆瓨閰嶇疆鍛戒护 */
static void handle_cmd_save_config(void)
{
    if (config_validator_check_magic(&g_tmp_config)) {
        int ret = config_save(&g_tmp_config);
        config_hid_set_response(CMD_SAVE_CONFIG, ret == 0,
                            ret == 0 ? CONFIG_HID_OK : CONFIG_HID_ERR_FLASH_WRITE, 0);
    } else {
        config_hid_set_response(CMD_SAVE_CONFIG, false, CONFIG_HID_ERR_CRC_MISMATCH, 0);
    }
}

/* 澶勭悊鎭㈠鍑哄巶閰嶇疆鍛戒护 */
static void handle_cmd_reset_config(void)
{
    config_reset_default();
    LOG_INFO_PRINT("[CONFIG] 宸叉仮澶嶅嚭鍘傞粯璁ら厤缃紝鍗冲皢閲嶅惎...\n");
    fault_record(FAULT_LEVEL_WARN, "config", "factory reset triggered by command");
    uint32_t reset_start = to_ms_since_boot(get_absolute_time());
    while ((to_ms_since_boot(get_absolute_time()) - reset_start) < CONFIG_RESET_DELAY_MS) {
        watchdog_update();
    }
    watchdog_reboot(0, 0, 1);
    while (1) {};
}

/* 澶勭悊閲嶅惎璁惧鍛戒护 */
static void handle_cmd_reboot(void)
{
    uint8_t progress = command_confirm_attempt_reboot();
    if (progress >= COMMAND_CONFIRM_REBOOT_REQUIRED) {
        LOG_INFO_PRINT("[CMD] 閲嶅惎璁惧锛堝凡纭%d娆★級...\n", COMMAND_CONFIRM_REBOOT_REQUIRED);
        sleep_ms(10);
        watchdog_reboot(0, 0, 1);
        while (1) {};
    } else {
        LOG_INFO_PRINT("[CMD] 閲嶅惎纭: %d/%d锛?绉掑唴杩炵画鍙戦€侊級\n", progress, COMMAND_CONFIRM_REBOOT_REQUIRED);
    }
}

/* 澶勭悊杩涘叆DFU妯″紡鍛戒护 */
static void handle_cmd_enter_dfu(void)
{
    uint8_t progress = command_confirm_attempt_dfu();
    if (progress >= COMMAND_CONFIRM_DFU_REQUIRED) {
        LOG_INFO_PRINT("[CMD] 杩涘叆 DFU 鍗囩骇妯″紡锛堝凡纭%d娆★級...\n", COMMAND_CONFIRM_DFU_REQUIRED);
        sleep_ms(10);
        dfu_set_command(DFU_CMD_ENTER_DFU);
        watchdog_reboot(0, 0, CONFIG_RESET_DELAY_MS);
        while (1) {}
    } else {
        LOG_INFO_PRINT("[CMD] DFU纭: %d/%d锛?绉掑唴杩炵画鍙戦€侊級\n", progress, COMMAND_CONFIRM_DFU_REQUIRED);
    }
}

/* 澶勭悊搴旂敤閰嶇疆鍛戒护锛堟渶澶嶆潅鐨勫懡浠わ紝鍖呭惈閰嶇疆楠岃瘉銆佸悎骞躲€佷繚瀛樸€丏PI搴旂敤锛?*/
static void handle_cmd_apply_config(void)
{
    LOG_INFO_PRINT("[CONFIG] 鏀跺埌搴旂敤閰嶇疆鍛戒护\n");
    config_hid_error_t apply_err = CONFIG_HID_OK;

    if (!config_block_receiver_is_complete()) {
        LOG_ERROR_PRINT("[CONFIG] 鉂?閰嶇疆鍧椾笉瀹屾暣: 0x%02X (鏈熸湜 0x%02X)\n",
                        config_block_receiver_get_status(), CONFIG_BLOCKS_ALL);
        apply_err = CONFIG_HID_ERR_INCOMPLETE;
    } else {
        config_block_receiver_reset();
        if (!config_validator_check_magic(&g_tmp_config)) {
            LOG_ERROR_PRINT("[CONFIG] 鉂?榄旀暟閿欒: 0x%08X (鏈熸湜 0x%08X)\n",
                            g_tmp_config.magic, CONFIG_MAGIC);
            apply_err = CONFIG_HID_ERR_CRC_MISMATCH;
        } else if (g_tmp_config.dpi < CONFIG_DPI_MIN || g_tmp_config.dpi > CONFIG_DPI_MAX) {
            LOG_ERROR_PRINT("[CONFIG] 鉂?DPI 瓒呭嚭鑼冨洿: %d (鍏佽%d-%d)\n",
                            g_tmp_config.dpi, CONFIG_DPI_MIN, CONFIG_DPI_MAX);
            apply_err = CONFIG_HID_ERR_INVALID_PARAM;
        } else {
            g_tmp_config.dpi = (g_tmp_config.dpi / 25) * 25;
            const device_config_t* current_cfg = config_get();
            device_config_t merged;
            if (current_cfg != NULL) {
                memcpy(&merged, current_cfg, sizeof(device_config_t));
            } else {
                memset(&merged, 0, sizeof(device_config_t));
            }
            config_validator_merge_config(&merged, &g_tmp_config);
            watchdog_feed_layer(WDG_LAYER_DEVICE);
            int save_ret = config_save(&merged);
            if (save_ret == 0) {
                LOG_INFO_PRINT("[CONFIG] 鉁?閰嶇疆宸蹭繚瀛樺埌 Flash (鐏垫晱搴?%d, 鍙嶈浆X=%d, 鍙嶈浆Y=%d)\n",
                               merged.joystick_sensitivity, merged.joystick_invert_x, merged.joystick_invert_y);
                const optical_sensor_cfg_t *paw_cfg = board_get_optical_sensor_cfg();
                if (paw_cfg != NULL) {
                    int ret;
                    uint8_t slot = merged.current_dpi_slot;
                    if (slot > 3) slot = 0;
                    uint16_t dpi_val = merged.dpi_slot[slot];
                    if (dpi_val == 0) dpi_val = merged.dpi;
                    if (dpi_val == 400 || dpi_val == 800 || dpi_val == 1600 || dpi_val == 3200) {
                        optical_sensor_dpi_e dpi_enum = config_hid_dpi_val_to_enum(dpi_val);
                        ret = config_hid_set_dpi_via_ipc((uint16_t)dpi_enum, false);
                    } else {
                        ret = config_hid_set_dpi_via_ipc(dpi_val, true);
                    }
                    if (ret == 0) LOG_INFO_PRINT("[CONFIG] 鉁?DPI 宸插簲鐢? %d (妗ｄ綅%d)\n", dpi_val, slot);
                    else LOG_ERROR_PRINT("[CONFIG] 鉂?DPI 搴旂敤澶辫触锛岄敊璇爜: %d\n", ret);
                }
                apply_err = CONFIG_HID_OK;
            } else {
                LOG_ERROR_PRINT("[CONFIG] 鉂?閰嶇疆淇濆瓨鍒?Flash 澶辫触\n");
                apply_err = CONFIG_HID_ERR_FLASH_WRITE;
            }
            watchdog_feed_layer(WDG_LAYER_DEVICE);
        }
    }

    /* 澶辫触鏃堕噸缃复鏃堕厤缃紝纭繚 g_tmp_config 濮嬬粓涓?Flash 涓€鑷?
     * 閬垮厤閮ㄥ垎鏇存柊鐨勯厤缃鍚庣画鍛戒护璇诲彇锛屽鑷存暟鎹笉涓€鑷?
     */
    if (apply_err != CONFIG_HID_OK) {
        const device_config_t* current = config_get();
        if (current != NULL) {
            memcpy(&g_tmp_config, current, sizeof(device_config_t));
        }
        config_block_receiver_reset();
        LOG_WARN_PRINT("[CONFIG] 鈿狅笍 搴旂敤閰嶇疆澶辫触(閿欒鐮?%d)锛屽凡閲嶇疆涓存椂閰嶇疆\n", apply_err);
    }

    config_hid_set_response(CMD_APPLY_CONFIG, apply_err == CONFIG_HID_OK, apply_err, 0);
}

#if FEATURE_KEY_STATS
/* 澶勭悊閲嶇疆鎸夐敭缁熻鍛戒护 */
static void handle_cmd_reset_stats(void)
{
    key_stats_reset();
    LOG_INFO_PRINT("[STATS] 鎸夐敭缁熻宸叉竻闆禱n");
    config_hid_set_response(CMD_RESET_STATS, true, CONFIG_HID_OK, 0);
}
#endif /* FEATURE_KEY_STATS */

#if FEATURE_FAULT_LOG
/* 澶勭悊娓呴櫎閿欒鏃ュ織鍛戒护 */
static void handle_cmd_clear_fault(void)
{
    fault_clear();
    LOG_INFO_PRINT("[FAULT] 閿欒鏃ュ織宸叉竻闄n");
    config_hid_set_response(CMD_CLEAR_FAULT, true, CONFIG_HID_OK, 0);
}
#endif /* FEATURE_FAULT_LOG */

#if FEATURE_PERF_MONITOR
/* 澶勭悊閲嶇疆鎬ц兘缁熻鍛戒护 */
static void handle_cmd_reset_perf(void)
{
    perf_reset();
    LOG_INFO_PRINT("[PERF] 鎬ц兘缁熻宸查噸缃甛n");
    config_hid_set_response(CMD_RESET_PERF, true, CONFIG_HID_OK, 0);
}
#endif /* FEATURE_PERF_MONITOR */

/* 澶勭悊鎾斁瀹忓懡浠?*/
static void handle_cmd_macro_play(void)
{
    uint8_t macro_id = g_pending_cmd_param;
    if (macro_id < MACRO_MAX_COUNT) {
        bool ok = macro_trigger(macro_id);
        LOG_INFO_PRINT("[MACRO] 鎾斁瀹?%d: %s\n", macro_id, ok ? "鎴愬姛" : "澶辫触");
        config_hid_set_response(CMD_MACRO_PLAY, ok,
                            ok ? CONFIG_HID_OK : CONFIG_HID_ERR_BUSY, macro_id);
    } else {
        LOG_ERROR_PRINT("[MACRO] 鏃犳晥鐨勫畯ID: %d\n", macro_id);
        config_hid_set_response(CMD_MACRO_PLAY, false, CONFIG_HID_ERR_INVALID_PARAM, macro_id);
    }
}

/* 澶勭悊鍋滄瀹忓懡浠?*/
static void handle_cmd_macro_stop(void)
{
    uint8_t macro_id = g_pending_cmd_param;
    if (macro_id < MACRO_MAX_COUNT) {
        macro_stop(macro_id);
        LOG_INFO_PRINT("[MACRO] 鍋滄瀹?%d\n", macro_id);
    } else {
        macro_stop_all();
        LOG_INFO_PRINT("[MACRO] 鍋滄鎵€鏈夊畯\n");
    }
    config_hid_set_response(CMD_MACRO_STOP, true, CONFIG_HID_OK, 0);
}

#if FEATURE_PERF_MONITOR
/* 澶勭悊璁剧疆鎬ц兘鐩戞帶寮€鍏冲懡浠?*/
static void handle_cmd_set_perf_enable(void)
{
    bool enable = (g_pending_cmd_param != 0);
    perf_set_enabled(enable);
    if (enable) perf_reset();
    LOG_INFO_PRINT("[PERF] monitor: %s\n", enable ? "on" : "off");
    config_hid_set_response(CMD_SET_PERF_ENABLE, true, CONFIG_HID_OK, enable ? 1 : 0);
}

/* 澶勭悊璁剧疆鎬ц兘鐩戞帶浠诲姟瓒呮椂闃堝€煎懡浠?
 * 鍙傛暟鏍煎紡锛?6浣嶅皬绔級锛?
 *   楂樺瓧鑺傦紙bit 8-15锛夛細浠诲姟绱㈠紩锛?-15锛屽搴?perf_monitor 涓殑浠诲姟锛?
 *   浣庡瓧鑺傦紙bit 0-7锛夛細闃堝€硷紙0-255锛屽崟浣?00寰锛?=绂佺敤鍛婅锛?
 *   鏈€澶ч槇鍊硷細255 脳 100us = 25500us = 25.5ms
 * 鍝嶅簲鍙傛暟锛氶珮瀛楄妭=浠诲姟绱㈠紩锛屼綆瀛楄妭=瀹為檯璁剧疆鐨勯槇鍊?100
 */
static void handle_cmd_set_perf_threshold(void)
{
    uint8_t task_index = (uint8_t)((g_pending_cmd_param >> 8) & 0x0F);  /* 鍙敤浣?浣嶏紝0-15 */
    uint8_t threshold_raw = (uint8_t)(g_pending_cmd_param & 0xFF);        /* 0-255锛屽崟浣?00us */
    uint32_t threshold_us = (uint32_t)threshold_raw * 100U;                /* 杞崲涓哄井绉?*/

    /* 楠岃瘉浠诲姟绱㈠紩鏈夋晥鎬?*/
    if (task_index >= PERF_MAX_TASKS) {
        LOG_ERROR_PRINT("[PERF] 鉂?鏃犳晥浠诲姟绱㈠紩: %d (鏈€澶?d)\n", task_index, PERF_MAX_TASKS - 1);
        config_hid_set_response(CMD_SET_PERF_THRESHOLD, false, CONFIG_HID_ERR_INVALID_PARAM, 0);
        return;
    }

    /* 璁剧疆闃堝€硷紙0=绂佺敤鍛婅锛?*/
    perf_set_threshold(task_index, threshold_us);

    /* Persist threshold to config (only first 8 tasks) */
    if (task_index < 8) {
        const device_config_t* current_cfg = config_get();
        if (current_cfg != NULL) {
            device_config_t new_cfg;
            memcpy(&new_cfg, current_cfg, sizeof(device_config_t));
            new_cfg.perf_thresholds[task_index] = threshold_raw;
            int save_ret = config_save(&new_cfg);
            if (save_ret != 0) {
                LOG_ERROR_PRINT("[PERF] threshold save failed: %d\n", save_ret);
            } else {
                LOG_INFO_PRINT("[PERF] task%d threshold saved to config\n", task_index);
            }
        }
    }

    if (threshold_us == 0) {
        LOG_INFO_PRINT("[PERF] 浠诲姟%d 瓒呮椂鍛婅宸茬鐢╘n", task_index);
    } else {
        LOG_INFO_PRINT("[PERF] 浠诲姟%d 瓒呮椂闃堝€艰缃负: %lu us (%lu ms)\n",
                       task_index, (unsigned long)threshold_us, (unsigned long)(threshold_us / 1000U));
    }

    /* 鍝嶅簲鍙傛暟锛氶珮瀛楄妭=浠诲姟绱㈠紩锛屼綆瀛楄妭=闃堝€?100 */
    uint16_t resp_param = (uint16_t)((task_index << 8) | threshold_raw);
    config_hid_set_response(CMD_SET_PERF_THRESHOLD, true, CONFIG_HID_OK, resp_param);
}
#endif /* FEATURE_PERF_MONITOR */

/* 澶勭悊瀹炴椂璁剧疆鎽囨潌姝诲尯鍛戒护 */
static void handle_cmd_set_joystick_dz_rt(void)
{
    uint16_t deadzone = (uint16_t)g_pending_cmd_param;
    if (deadzone > 4095) deadzone = 4095;
    uint32_t cmd = IPC_MAKE_CMD(IPC_CMD_SET_JOYSTICK_DZ, deadzone);
    if (!multicore_fifo_push_timeout_us(cmd, 0)) {
        LOG_ERROR_PRINT("[JOYSTICK] FIFO 婊★紝璺宠繃姝诲尯璁剧疆\n");
        config_hid_set_response(CMD_SET_JOYSTICK_DZ_RT, false, CONFIG_HID_ERR_BUSY, deadzone);
        return;
    }
    uint32_t ack_val = 0;
    bool ack_received = multicore_fifo_pop_timeout_us(CONFIG_IPC_ACK_TIMEOUT_US, &ack_val);
    if (!ack_received) {
        LOG_WARN_PRINT("[JOYSTICK] 姝诲尯璁剧疆ACK瓒呮椂锛圕ore1鍙兘蹇欙級\n");
    }
    LOG_INFO_PRINT("[JOYSTICK] 瀹炴椂姝诲尯璁剧疆: %u (宸插彂閫?\n", deadzone);
    config_hid_set_response(CMD_SET_JOYSTICK_DZ_RT, true, CONFIG_HID_OK, deadzone);
}

/* 澶勭悊鎽囨潌鏍″噯鍛戒护 */
static void handle_cmd_joystick_calibrate(void)
{
    uint16_t sample_count = (uint16_t)g_pending_cmd_param;
    if (sample_count == 0) sample_count = 50;
    if (sample_count > 200) sample_count = 200;
    if (shared_hw_trigger_joystick_calibration(sample_count)) {
        LOG_INFO_PRINT("[JOYSTICK] 鏍″噯瑙﹀彂鎴愬姛锛岄噰鏍锋鏁? %u\n", sample_count);
        config_hid_set_response(CMD_JOYSTICK_CALIBRATE, true, CONFIG_HID_OK, sample_count);
    } else {
        LOG_ERROR_PRINT("[JOYSTICK] 鏍″噯瑙﹀彂澶辫触锛堝彲鑳芥鍦ㄦ牎鍑嗕腑锛塡n");
        config_hid_set_response(CMD_JOYSTICK_CALIBRATE, false, CONFIG_HID_ERR_BUSY, sample_count);
    }
}

/* 澶勭悊鍒囨崲DPI妗ｄ綅鍛戒护 */
static void handle_cmd_switch_dpi(void)
{
    uint8_t target_slot = (uint8_t)(g_pending_cmd_param & 0xFF);
    const device_config_t *current_cfg = config_get();
    uint8_t new_slot = current_cfg->current_dpi_slot;
    if (target_slot == 0xFF) {
        new_slot = (current_cfg->current_dpi_slot + 1) % 4;
    } else if (target_slot < 4) {
        new_slot = target_slot;
    }
    if (new_slot != current_cfg->current_dpi_slot) {
        device_config_t new_cfg;
        memcpy(&new_cfg, current_cfg, sizeof(device_config_t));
        new_cfg.current_dpi_slot = new_slot;
        config_save(&new_cfg);
        const optical_sensor_cfg_t *opt_cfg = board_get_optical_sensor_cfg();
        if (opt_cfg != NULL) {
            uint16_t dpi_val = new_cfg.dpi_slot[new_slot];
            if (dpi_val == 0) dpi_val = new_cfg.dpi;
            int ret;
            if (dpi_val == 400 || dpi_val == 800 || dpi_val == 1600 || dpi_val == 3200) {
                optical_sensor_dpi_e dpi_enum = config_hid_dpi_val_to_enum(dpi_val);
                ret = config_hid_set_dpi_via_ipc((uint16_t)dpi_enum, false);
            } else {
                ret = config_hid_set_dpi_via_ipc(dpi_val, true);
            }
            if (ret == 0)
                LOG_INFO_PRINT("[CONFIG] 鉁?DPI 鍒囨崲鍒版。浣?d: %d DPI\n", new_slot, dpi_val);
            else
                LOG_ERROR_PRINT("[CONFIG] 鉂?DPI 搴旂敤澶辫触锛岄敊璇爜: %d\n", ret);
        }
    } else {
        LOG_INFO_PRINT("[CONFIG] DPI 宸插湪妗ｄ綅%d\n", new_slot);
    }
    config_hid_set_response(CMD_SWITCH_DPI, true, CONFIG_HID_OK, new_slot);
}

/* 澶勭悊鑾峰彇鐗堟湰淇℃伅鍛戒护 */
static void handle_cmd_get_version(void)
{
    LOG_INFO_PRINT("[CONFIG] 鐗堟湰鏌ヨ: 鍗忚=%d.%d, 閰嶇疆=0x%04X\n",
                   CONFIG_HID_PROTOCOL_VERSION_MAJOR,
                   CONFIG_HID_PROTOCOL_VERSION_MINOR,
                   CONFIG_VERSION);
    config_hid_set_response(CMD_GET_VERSION, true, CONFIG_HID_OK,
                        CONFIG_HID_PROTOCOL_VERSION);
}

/* 澶勭悊鐗堟湰鍗忓晢鍛戒护 */
static void handle_cmd_negotiate_version(void)
{
    uint16_t host_version = g_pending_cmd_param;
    uint8_t host_major = (uint8_t)(host_version >> 8);
    uint8_t host_minor = (uint8_t)(host_version & 0xFF);
    LOG_INFO_PRINT("[CONFIG] 鐗堟湰鍗忓晢: 涓婁綅鏈?%d.%d, 鍥轰欢=%d.%d\n",
                   host_major, host_minor,
                   CONFIG_HID_PROTOCOL_VERSION_MAJOR,
                   CONFIG_HID_PROTOCOL_VERSION_MINOR);
    if (host_major != CONFIG_HID_PROTOCOL_VERSION_MAJOR) {
        LOG_ERROR_PRINT("[CONFIG] 鉂?鐗堟湰涓嶅吋瀹? 涓婁綅鏈轰富鐗堟湰=%d, 鍥轰欢涓荤増鏈?%d\n",
                        host_major, CONFIG_HID_PROTOCOL_VERSION_MAJOR);
        config_hid_set_response(CMD_NEGOTIATE_VERSION, false,
                            CONFIG_HID_ERR_VERSION_MISMATCH,
                            CONFIG_HID_PROTOCOL_VERSION);
    } else {
        config_hid_set_response(CMD_NEGOTIATE_VERSION, true, CONFIG_HID_OK,
                            CONFIG_HID_PROTOCOL_VERSION);
    }
}

/* 澶勭悊鍒囨崲鎸夐敭灞傚懡浠?*/
static void handle_cmd_switch_layer(void)
{
    uint8_t target_layer = (uint8_t)(g_pending_cmd_param & 0xFF);
    if (target_layer >= 4) {
        LOG_ERROR_PRINT("[CONFIG] 鉂?鏃犳晥灞傚彿: %d\n", target_layer);
        config_hid_set_response(CMD_SWITCH_LAYER, false,
                            CONFIG_HID_ERR_INVALID_PARAM, 0);
    } else {
        keymap_set_active_layer(target_layer);
        LOG_INFO_PRINT("[CONFIG] 鉁?鍒囨崲鍒板眰 %d\n", target_layer);
        config_hid_set_response(CMD_SWITCH_LAYER, true, CONFIG_HID_OK, target_layer);
    }
}

/* 澶勭悊鑾峰彇鑳藉姏淇℃伅鍛戒护 */
static void handle_cmd_get_capability_info(void)
{
    uint8_t cap_id = (uint8_t)(g_pending_cmd_param & 0xFF);
    static const uint16_t capability_versions[CAP_COUNT] = {
        [CAP_KEYBOARD_BASIC]   = (1 << 8) | 0,
        [CAP_MOUSE_BASIC]      = (1 << 8) | 0,
        [CAP_JOYSTICK]         = (1 << 8) | 0,
        [CAP_ENCODER]          = (1 << 8) | 0,
        [CAP_KEYMAP]           = (2 << 8) | 0,
        [CAP_MACRO]            = (1 << 8) | 0,
        [CAP_CONFIG_STORAGE]   = (2 << 8) | 0,
        [CAP_PERF_MONITOR]     = (1 << 8) | 0,
        [CAP_FAULT_LOG]        = (1 << 8) | 0,
        [CAP_KEY_STATS]        = (1 << 8) | 0,
        [CAP_DFU_UPGRADE]      = (1 << 8) | 0,
        [CAP_NKRO]             = (1 << 8) | 0,
        [CAP_CONSUMER]         = (1 << 8) | 0,
        [CAP_GAMEPAD]          = (1 << 8) | 0,
        [CAP_DPI_SLOTS]        = (1 << 8) | 0,
        [CAP_JOYSTICK_CALIB]   = (1 << 8) | 0,
        [CAP_DIAG_RAW_STATE]   = (1 << 8) | 0,
        [CAP_DIAG_DEBUG_UART]  = (1 << 8) | 0,
        [CAP_MFG_FACTORY_TEST] = (1 << 8) | 0,
        [CAP_MFG_PROVISION]    = (1 << 8) | 0,
        /* bit18-23 棰勭暀锛岀増鏈繚鎸?0 */
    };
    if (cap_id >= CAP_COUNT) {
        LOG_ERROR_PRINT("[CAP] invalid cap_id: %d\n", cap_id);
        config_hid_set_response(CMD_GET_CAPABILITY_INFO, false,
                            CONFIG_HID_ERR_INVALID_PARAM, 0);
    } else {
        uint16_t version = capability_versions[cap_id];
        uint8_t major = (uint8_t)(version >> 8);
        uint8_t minor = (uint8_t)(version & 0xFF);
        LOG_DEBUG_PRINT("[CAP] cap%d version: v%d.%d\n", cap_id, major, minor);
        config_hid_set_response(CMD_GET_CAPABILITY_INFO, true,
                            CONFIG_HID_OK, version);
    }
}

/* 澶勭悊杩涘叆BOOTSEL妯″紡鍛戒护 */
static void handle_cmd_enter_bootsel(void)
{
    uint8_t progress = command_confirm_attempt_bootsel();
    if (progress >= COMMAND_CONFIRM_BOOTSEL_REQUIRED) {
        LOG_INFO_PRINT("[CMD] 杩涘叆瀹樻柟 BOOTSEL UF2 妯″紡锛堝凡纭%d娆★級...\n", COMMAND_CONFIRM_BOOTSEL_REQUIRED);
        sleep_ms(10);
        rom_reset_usb_boot(0, 0);
        while (1) { }
    } else {
        LOG_INFO_PRINT("[CMD] BOOTSEL纭: %d/%d锛?绉掑唴杩炵画鍙戦€侊級\n", progress, COMMAND_CONFIRM_BOOTSEL_REQUIRED);
        config_hid_set_response(CMD_ENTER_BOOTSEL, true, CONFIG_HID_OK, progress);
    }
}

/* 鍛ㄦ湡鎬у鐞嗗嚱鏁帮紙鏇夸唬鍘?hid_config_task锛?*/
void hid_config_task(void)
{
    /* 閰嶇疆閿佸畾锛氳В閿佸悗30绉掓棤鎿嶄綔鑷姩閲嶆柊閿佸畾 */
    if (config_lock_check_auto_relock()) {
        LOG_INFO_PRINT("[SECURITY] 馃敀 閰嶇疆宸茶嚜鍔ㄩ攣瀹氾紙瓒呮椂锛塡n");
    }

    /* 澶勭悊瀹忛厤缃殑寤惰繜淇濆瓨锛堜娇鐢ㄥ畨鍏ㄥ啓鍏ユ湇鍔★紝鍙屾牳鍚屾锛?*/
    if (s_macro_dirty) {
        uint32_t now = time_us_32();
        if (now - s_macro_dirty_time_us > MACRO_DIRTY_SAVE_DELAY_US) {
            const device_config_t* current_cfg = config_get();
            if (current_cfg != NULL) {
                /* 浣跨敤 static 缂撳啿鍖洪伩鍏嶆爤婧㈠嚭锛坉evice_config_t 绾?330瀛楄妭锛?*/
                static device_config_t s_macro_save_buf;
                memcpy(&s_macro_save_buf, current_cfg, sizeof(device_config_t));
                macro_save_to_config(s_macro_save_buf.macro_data, CONFIG_MACRO_DATA_SIZE);
                config_save(&s_macro_save_buf);
                LOG_INFO_PRINT("[瀹廬 閰嶇疆宸蹭繚瀛樺埌Flash\n");
            }
            s_macro_dirty = false;
        }
    }

    /* 澶勭悊寰呭啓鍏ョ殑涓存椂閰嶇疆 */
    if (g_config_pending) {
        g_config_pending = false;
        if (config_validator_check_magic(&g_tmp_config)) {
            /* 鏈潵锛氬簲鐢ㄥ埌妯″潡鎴栨牎楠?*/
        }
    }

    /* 澶勭悊寰呮墽琛岀殑鍛戒护锛堝垎鍙戝埌瀵瑰簲鐨勫鐞嗗嚱鏁帮級 */
    if (g_pending_cmd != 0) {
        uint8_t cmd = g_pending_cmd;
        g_pending_cmd = 0;

        /* 鍥轰欢渚у懡浠ら椄闂細鑳藉姏浣?/ 鎺堟潈绾у埆鏍￠獙銆?
         * 涓?config_lock 鐨勨€滈厤缃啓閿佲€濇浜わ紙鍐欓攣鍦?set_report 鍏ュ彛宸叉鏌ワ級銆?
         * 鎷掔粷鏃跺洖鏄庣‘ NACK锛岄伩鍏嶅彧闈犱笂浣嶆満闅愯棌鎸夐挳琚?raw HID 缁曡繃銆?*/
        uint8_t gate_err = command_gate_check_command(cmd);
        if (gate_err != CONFIG_HID_OK) {
            LOG_INFO_PRINT("[GATE] 鍛戒护 0x%02X 琚嫆缁濓紝閿欒鐮?%d\n", cmd, gate_err);
            config_hid_set_response(cmd, false, gate_err, 0);
        } else {
            switch (cmd) {
            case CMD_SAVE_CONFIG:        handle_cmd_save_config(); break;
            case CMD_RESET_CONFIG:       handle_cmd_reset_config(); break;
            case CMD_REBOOT:             handle_cmd_reboot(); break;
            case CMD_ENTER_DFU:          handle_cmd_enter_dfu(); break;
            case CMD_APPLY_CONFIG:       handle_cmd_apply_config(); break;
#if FEATURE_KEY_STATS
            case CMD_RESET_STATS:        handle_cmd_reset_stats(); break;
#endif
#if FEATURE_FAULT_LOG
            case CMD_CLEAR_FAULT:        handle_cmd_clear_fault(); break;
#endif
#if FEATURE_PERF_MONITOR
            case CMD_RESET_PERF:         handle_cmd_reset_perf(); break;
#endif
            case CMD_MACRO_PLAY:         handle_cmd_macro_play(); break;
            case CMD_MACRO_STOP:         handle_cmd_macro_stop(); break;
#if FEATURE_PERF_MONITOR
            case CMD_SET_PERF_ENABLE:    handle_cmd_set_perf_enable(); break;
#endif
            case CMD_SET_JOYSTICK_DZ_RT: handle_cmd_set_joystick_dz_rt(); break;
            case CMD_JOYSTICK_CALIBRATE: handle_cmd_joystick_calibrate(); break;
            case CMD_SWITCH_DPI:         handle_cmd_switch_dpi(); break;
            case CMD_GET_VERSION:        handle_cmd_get_version(); break;
            case CMD_NEGOTIATE_VERSION:  handle_cmd_negotiate_version(); break;
            case CMD_SWITCH_LAYER:       handle_cmd_switch_layer(); break;
            case CMD_GET_CAPABILITY_INFO: handle_cmd_get_capability_info(); break;
            case CMD_ENTER_BOOTSEL:      handle_cmd_enter_bootsel(); break;
#if FEATURE_PERF_MONITOR
            case CMD_SET_PERF_THRESHOLD: handle_cmd_set_perf_threshold(); break;
#endif
            default: break;
            }
        }
    }
}

