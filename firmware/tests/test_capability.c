/*
 * tests/test_capability.c
 * 杩愯鏃惰兘鍔涗綅鍥?/ 璁惧鏍囧織鍗曞厓娴嬭瘯锛堝姛鑳借鑹插垎绂伙紝鍗忚 v0.2锛? *
 * host 鍗曞厓娴嬭瘯榛樿涓嶈鐩?FEATURE_*锛宖eatures.h 鍏ㄩ儴榛樿涓?1锛坉ev/宸ョ▼鎬佸叏寮€锛夛紝
 * 鍥犳杩欓噷楠岃瘉鈥滃叏寮€褰㈡€佲€濈殑浣嶅浘鏋勬垚銆俽elease 瑁佸壀褰㈡€侊紙缂鸿瘖鏂?鍒堕€犱綅锛夌敱
 * 鍏ㄥ叧鍥轰欢鏋勫缓锛坆uild_off锛変繚璇佸彲缂栬瘧锛屽叾浣嶅浘鏀剁獎閫昏緫涓庢湰琛ㄥ悓涓€濂?#if 鍒嗘敮銆? */

#include "unity.h"
#include "project_features.h"
#include "protocol/capability.h"
#include "protocol/config_hid.h"
#include <stdint.h>

/* 鑳藉姏鎬绘暟闅忚瘖鏂?鍒堕€犳鎵╁睍鍒?26锛堝惈 18-23 棰勭暀绌烘礊锛?*/
void test_capability_count_extended_to_26(void)
{
    TEST_ASSERT_EQUAL_UINT8(26, CAP_COUNT);
}

/* 娑堣垂鑰呭熀纭€鑳藉姏浣嶅缁堢紪鍏ワ紝涓?FEATURE_* 鏃犲叧銆? * 娉ㄦ剰 CAP_DFU_UPGRADE(bit10) 涓嶅湪姝ゅ垪锛屽畠璺熼殢 FEATURE_DFU_BOOTLOADER锛岃涓嬫柟涓撴祴銆?*/
void test_capability_bitmap_contains_consumer_base(void)
{
    uint64_t b = capability_bitmap_runtime();

    TEST_ASSERT_TRUE((b & (1ULL << CAP_KEYBOARD_BASIC)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_MOUSE_BASIC)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_JOYSTICK)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_ENCODER)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_KEYMAP)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_MACRO)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_CONFIG_STORAGE)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_NKRO)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DPI_SLOTS)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_JOYSTICK_CALIB)) != 0);
}

/* 鑷畾涔?DFU bootloader 鑳藉姏浣嶄弗鏍艰窡闅?FEATURE_DFU_BOOTLOADER锛? * host 涓诲浠朵笉甯?CMake 鍙傛暟锛岃蛋 features.h 榛樿鍏ㄥ紑锛?1锛夛紝bit10 搴旂疆浣嶃€? * release 瑁佸壀褰㈡€侊紙=0锛宐ootloader 鏈紪鍏ワ級鐢?build_mfg_lock_tests 濂椾欢楠岃瘉 bit10=0銆? * BOOTSEL UF2 涓鸿姱鐗囧師鐢熷崌绾ц矾寰勶紝涓嶅崰鐢ㄨ兘鍔涗綅锛屼笉鍦ㄦ鏂█銆?*/
void test_capability_dfu_follows_bootloader_flag(void)
{
    uint64_t b = capability_bitmap_runtime();
#if FEATURE_DFU_BOOTLOADER
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DFU_UPGRADE)) != 0);
#else
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DFU_UPGRADE)) == 0);
#endif
}

/* 鍏ㄥ紑褰㈡€侊細璇婃柇鑳藉姏浣嶏紙perf/fault/keystats/raw/uart锛夊叏閮ㄧ疆浣?*/
void test_capability_bitmap_contains_diag_bits(void)
{
    uint64_t b = capability_bitmap_runtime();

    TEST_ASSERT_TRUE((b & (1ULL << CAP_PERF_MONITOR)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_FAULT_LOG)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_KEY_STATS)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DIAG_RAW_STATE)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_DIAG_DEBUG_UART)) != 0);
}

/* 鍏ㄥ紑宸ョ▼鎬侊細宸ュ巶鑳藉姏缂栧叆涓旀湭鍑哄巶閿佸畾锛屽埗閫犱綅缃綅 */
void test_capability_bitmap_contains_mfg_bits(void)
{
    uint64_t b = capability_bitmap_runtime();

    TEST_ASSERT_TRUE(capability_mfg_factory_available());
    TEST_ASSERT_TRUE((b & (1ULL << CAP_MFG_FACTORY_TEST)) != 0);
    TEST_ASSERT_TRUE((b & (1ULL << CAP_MFG_PROVISION)) != 0);
}

/* 闃舵1宸ョ▼鎬侊細鏈帴 Provisioning 鍒嗗尯锛宒evice_flags 鎭掍负 0 */
void test_capability_device_flags_zero_in_engineering(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, capability_device_flags());
}

/* 棰勭暀浣?18-23 涓嶅簲琚疆浣?*/
void test_capability_reserved_holes_clear(void)
{
    uint64_t b = capability_bitmap_runtime();
    int bit;

    for (bit = 18; bit <= 23; bit++) {
        TEST_ASSERT_TRUE((b & (1ULL << bit)) == 0);
    }
}
