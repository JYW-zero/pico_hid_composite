/**
 * @file capability.c
 * @brief 杩愯鏃惰兘鍔涗綅鍥句笌璁惧鏍囧織瀹炵幇锛堝姛鑳借鑹插垎绂伙紝鍗忚 v0.2锛? */
#include "project_features.h"
#include "protocol/capability.h"

/* 娑堣垂鑰呭熀纭€鑳藉姏浣嶏細鏍稿績 HID 鍔熻兘濮嬬粓缂栧叆锛屼笌 FEATURE_* 寮€鍏虫棤鍏炽€? * 瀵瑰簲 bit0-6 涓?bit11-15锛坆it7-9 涓哄彲瑁佸壀鐨勮瘖鏂兘鍔涳紝瑙佷笅锛夈€? * bit10 CAP_DFU_UPGRADE 涓嶅湪姝ゅ垪锛氬畠琛ㄧず鈥滆嚜瀹氫箟 in-app DFU bootloader鈥濓紝
 * 闅?FEATURE_DFU_BOOTLOADER 缃綅锛堣 CAP_DFU_BOOTLOADER_BITMAP锛夈€? * RP2350 鍘熺敓 BOOTSEL UF2 鍗囩骇濮嬬粓鍙敤锛屼絾涓嶅崰鐢ㄨ兘鍔涗綅銆?*/
#define CAP_BASE_BITMAP ( \
    (1ULL << CAP_KEYBOARD_BASIC) | (1ULL << CAP_MOUSE_BASIC) | \
    (1ULL << CAP_JOYSTICK)      | (1ULL << CAP_ENCODER)      | \
    (1ULL << CAP_KEYMAP)        | (1ULL << CAP_MACRO)        | \
    (1ULL << CAP_CONFIG_STORAGE)| (1ULL << CAP_NKRO)         | \
    (1ULL << CAP_CONSUMER)      | (1ULL << CAP_GAMEPAD)      | \
    (1ULL << CAP_DPI_SLOTS)     | (1ULL << CAP_JOYSTICK_CALIB) )

/* 鑷畾涔?in-app DFU bootloader锛圕MD_ENTER_DFU + HID report 0x30~0x36锛夛細
 * bootloader 闅忓浐浠跺竷灞€缂栧叆锛團EATURE_DFU_BOOTLOADER=1锛夋墠涓婃姤 bit10锛? * 褰撳墠 CMake 涓存椂绂佺敤 bootloader锛屼袱涓?profile 鍧囨敞鍏?0锛宐it10 涓婃姤 0銆?*/
#if FEATURE_DFU_BOOTLOADER
#define CAP_DFU_BOOTLOADER_BITMAP (1ULL << CAP_DFU_UPGRADE)
#else
#define CAP_DFU_BOOTLOADER_BITMAP 0ULL
#endif

bool capability_mfg_factory_available(void)
{
    /* 闃舵2杩囨浮闂ㄦ帶锛氶噺浜у浐浠?FEATURE_MFG_DEFAULT_LOCKED=1锛屼笂鐢甸粯璁ら攣瀹氾紝
     * 涓嶆毚闇?CAP_MFG_FACTORY_TEST/CAP_MFG_PROVISION锛涚墿鐞嗏€滈暱鎸?Fn鈥濇湰鍦拌嚜妫€鍦?     * main.c 鐩存帴杩涘叆锛堜笉璧?HID锛夛紝涓嶅彈姝ら檺銆俤ev 鍥轰欢榛樿瑙ｉ攣銆佸埗閫犺兘鍔涘叏寮€銆?     * TODO(闃舵3): 鏀逛负璇诲彇 Provisioning 鍒嗗尯 mfg_state锛堝嚭鍘傞攣 + 鍞悗浼氳瘽瑙ｉ攣锛夈€?*/
#if FEATURE_MFG_DEFAULT_LOCKED
    return false;
#else
    return true;
#endif
}

uint64_t capability_bitmap_runtime(void)
{
    uint64_t bitmap = CAP_BASE_BITMAP;

#if FEATURE_PERF_MONITOR
    bitmap |= (1ULL << CAP_PERF_MONITOR);
#endif

#if FEATURE_FAULT_LOG
    bitmap |= (1ULL << CAP_FAULT_LOG);
#endif

#if FEATURE_KEY_STATS
    bitmap |= (1ULL << CAP_KEY_STATS);
#endif

#if FEATURE_RAW_DIAG
    bitmap |= (1ULL << CAP_DIAG_RAW_STATE);
#endif

#if FEATURE_DEBUG_UART
    bitmap |= (1ULL << CAP_DIAG_DEBUG_UART);
#endif

#if FEATURE_FACTORY_TEST
    if (capability_mfg_factory_available()) {
        bitmap |= (1ULL << CAP_MFG_FACTORY_TEST);
        bitmap |= (1ULL << CAP_MFG_PROVISION);
    }
#endif

    /* 鑷畾涔?DFU bootloader 璺熼殢缂栬瘧寮€鍏筹紙BOOTSEL UF2 鍘熺敓鍗囩骇涓嶅崰鑳藉姏浣嶏級 */
    return bitmap | CAP_DFU_BOOTLOADER_BITMAP;
}

uint8_t capability_device_flags(void)
{
    uint8_t flags = 0;

    /* 闃舵2锛氶噺浜у浐浠讹紙FEATURE_MFG_DEFAULT_LOCKED=1锛変笂鐢甸粯璁ゆ姤鍛婂嚭鍘傞攣瀹氭爣蹇椼€?     * TODO(闃舵3): 鏀逛负浠?Provisioning 鍒嗗尯璇诲彇鍑哄巶閿佸畾浣嶄笌鏈細璇濆敭鍚庤В閿佷綅锛?     *   宸插嚭鍘傞攣瀹氱疆 DEVICE_FLAG_MFG_SHIPPED_LOCKED锛?     *   鍞悗鎸戞垬-搴旂瓟閫氳繃缃?DEVICE_FLAG_MFG_SESSION_UNLOCKED銆?*/
#if FEATURE_FACTORY_TEST
    if (!capability_mfg_factory_available()) {
        flags |= DEVICE_FLAG_MFG_SHIPPED_LOCKED;
    }
#endif

    return flags;
}
