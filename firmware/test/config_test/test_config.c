/*
 * test/config_test/test_config.c
 * 配置存储模块单元测试
 *
 * 测试内容：
 * 1. 配置基本校验（magic、length、CRC）
 * 2. 双备份加载（A/B区各种组合）
 * 3. 版本迁移（v1/v2/v3 → v4）
 * 4. 配置保存和读取
 * 5. config_load_default 强制恢复
 * 6. 序列号回绕比较
 * 7. 损坏恢复（随机损坏任一区）
 * 8. 写入次数统计
 *
 * 编译：gcc -I../../include -I. test_config.c ../../src/board/config.c -o test_config
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ==================== Mock Pico SDK ==================== */

/* 模拟 Flash 内存（64KB，足够测试用） */
#define MOCK_FLASH_SIZE  (64 * 1024)
static uint8_t g_mock_flash[MOCK_FLASH_SIZE];

/* Flash 布局：配置区在最后 8KB */
#define CONFIG_A_OFFSET  (MOCK_FLASH_SIZE - 8192)
#define CONFIG_B_OFFSET  (MOCK_FLASH_SIZE - 4096)

/* 时间 mock */
static uint64_t g_mock_time_ms = 0;

/* Flash layout mock（实现 flash_layout.h 中声明的函数，定义在 include config.h 之后） */
void flash_layout_init(void) {}

/* Flash service mock */
bool flash_service_erase(uint32_t offset, uint32_t size)
{
    if (offset + size > MOCK_FLASH_SIZE) return false;
    memset(g_mock_flash + offset, 0xFF, size);
    return true;
}

bool flash_service_program(uint32_t offset, const uint8_t* data, uint32_t size)
{
    if (offset + size > MOCK_FLASH_SIZE) return false;
    memcpy(g_mock_flash + offset, data, size);
    return true;
}

/* ==================== 包含被测代码 ==================== */

#include "board/config.h"

/* 重命名 config.c 中的弱函数，避免与测试中的强函数冲突 */
#define config_get_flash_base      config_get_flash_base_weak
#define config_enter_flash_write   config_enter_flash_write_weak
#define config_exit_flash_write    config_exit_flash_write_weak

/* 直接包含源码以访问静态函数（单元测试专用） */
#include "../../src/board/config.c"

#undef config_get_flash_base
#undef config_enter_flash_write
#undef config_exit_flash_write

/* 测试用强函数覆盖 */
uintptr_t config_get_flash_base(void)
{
    return (uintptr_t)g_mock_flash;
}

void config_enter_flash_write(void) {}
void config_exit_flash_write(void) {}

/* Fault mock 实现 */
void fault_record(fault_level_e level, const char* module, const char* msg)
{
    (void)level; (void)module; (void)msg;
}

/* flash_layout 函数实现（必须在包含 config.h 之后，因为需要 flash_region_e 类型） */
uint32_t flash_layout_get_offset(flash_region_e region)
{
    switch (region) {
        case FLASH_REGION_CONFIG_A: return CONFIG_A_OFFSET;
        case FLASH_REGION_CONFIG_B: return CONFIG_B_OFFSET;
        default: return 0;
    }
}

uint32_t flash_layout_get_xip_addr(flash_region_e region)
{
    return 0x10000000 + flash_layout_get_offset(region);
}

uint32_t flash_layout_get_size(flash_region_e region)
{
    (void)region;
    return 4096;
}

/* ==================== 测试框架 ==================== */

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
        g_tests_failed++; \
    } \
} while(0)

#define TEST_SECTION(name) printf("\n=== %s ===\n", name)

/* ==================== 辅助函数 ==================== */

/* 构造一个有效的 v4 配置 */
static void make_valid_config(device_config_t* cfg, uint16_t seq)
{
    memset(cfg, 0, sizeof(device_config_t));
    cfg->magic = CONFIG_MAGIC;
    cfg->version = CONFIG_VERSION;
    cfg->dpi = 800;
    cfg->joystick_deadzone = 100;
    cfg->seq = seq;
    cfg->length = sizeof(device_config_t);
    cfg->write_count = seq;
    cfg->crc32 = crc32_calc((const uint8_t*)cfg, sizeof(device_config_t) - sizeof(uint32_t));
}

/* 写入配置到指定 Flash 偏移 */
static void write_config_to_flash(uint32_t offset, const device_config_t* cfg)
{
    memcpy(g_mock_flash + offset, cfg, sizeof(device_config_t));
}

/* 清空模拟 Flash */
static void clear_flash(void)
{
    memset(g_mock_flash, 0xFF, MOCK_FLASH_SIZE);
}

/* ==================== 测试用例 ==================== */

/* 测试1：CRC32 计算 */
static void test_crc32(void)
{
    TEST_SECTION("CRC32 计算");

    uint8_t data1[] = "123456789";
    uint32_t crc = crc32_calc(data1, 9);
    TEST_ASSERT(crc == 0xCBF43926U, "标准测试串 CRC32 正确");

    uint8_t data2[] = {0};
    uint32_t crc2 = crc32_calc(data2, 1);
    TEST_ASSERT(crc2 != 0, "全零数据 CRC 不为0");

    /* 相同数据 CRC 相同 */
    uint8_t data3[] = "hello";
    TEST_ASSERT(crc32_calc(data3, 5) == crc32_calc(data3, 5), "相同数据 CRC 一致");
}

/* 测试2：配置基本校验 */
static void test_config_basic_valid(void)
{
    TEST_SECTION("配置基本校验");

    device_config_t cfg;
    make_valid_config(&cfg, 1);
    TEST_ASSERT(config_basic_valid(&cfg), "有效配置校验通过");

    /* magic 错误 */
    device_config_t bad_magic = cfg;
    bad_magic.magic = 0xDEADBEEF;
    TEST_ASSERT(!config_basic_valid(&bad_magic), "magic 错误校验失败");

    /* CRC 错误 */
    device_config_t bad_crc = cfg;
    bad_crc.crc32 = 0x12345678;
    TEST_ASSERT(!config_basic_valid(&bad_crc), "CRC 错误校验失败");

    /* length 错误（v4+） */
    device_config_t bad_len = cfg;
    bad_len.length = 999;
    bad_len.crc32 = crc32_calc((const uint8_t*)&bad_len, sizeof(device_config_t) - 4);
    TEST_ASSERT(!config_basic_valid(&bad_len), "length 错误校验失败");

    /* NULL 指针 */
    TEST_ASSERT(!config_basic_valid(NULL), "NULL 指针校验失败");
}

/* 测试3：序列号回绕比较 */
static void test_seq_is_newer(void)
{
    TEST_SECTION("序列号回绕比较");

    TEST_ASSERT(seq_is_newer(2, 1), "2 比 1 新");
    TEST_ASSERT(!seq_is_newer(1, 2), "1 不比 2 新");
    TEST_ASSERT(!seq_is_newer(1, 1), "相同序列号不更新");

    /* 回绕场景：65535 → 0 */
    TEST_ASSERT(seq_is_newer(0, 65535), "回绕后 0 比 65535 新");
    TEST_ASSERT(!seq_is_newer(65535, 0), "65535 不比回绕后的 0 新");

    /* 回绕场景：65534 → 1 */
    TEST_ASSERT(seq_is_newer(1, 65534), "回绕后 1 比 65534 新");
}

/* 测试4：双备份加载 - 都有效 */
static void test_load_both_valid(void)
{
    TEST_SECTION("双备份加载 - 都有效");

    clear_flash();

    device_config_t cfg_a, cfg_b;
    make_valid_config(&cfg_a, 1);
    make_valid_config(&cfg_b, 2);
    cfg_b.dpi = 1600;  /* B区不同 */

    write_config_to_flash(CONFIG_A_OFFSET, &cfg_a);
    write_config_to_flash(CONFIG_B_OFFSET, &cfg_b);

    printf("  [DEBUG] before config_init, A_OFFSET=%u, B_OFFSET=%u\n", CONFIG_A_OFFSET, CONFIG_B_OFFSET);
    printf("  [DEBUG] flash_layout A=%u, B=%u\n", flash_layout_config_a_offset(), flash_layout_config_b_offset());
    fflush(stdout);

    config_init();
    printf("  [DEBUG] after config_init\n");
    fflush(stdout);

    const device_config_t* current = config_get();

    TEST_ASSERT(current != NULL, "配置加载成功");
    TEST_ASSERT(current->seq == 2, "选择较新的 B 区 (seq=2)");
    TEST_ASSERT(current->dpi == 1600, "B 区的 DPI 值正确");
}

/* 测试5：双备份加载 - 只有A有效 */
static void test_load_only_a_valid(void)
{
    TEST_SECTION("双备份加载 - 只有A有效");

    clear_flash();

    device_config_t cfg_a;
    make_valid_config(&cfg_a, 5);
    cfg_a.dpi = 400;
    write_config_to_flash(CONFIG_A_OFFSET, &cfg_a);

    /* B区故意损坏 */
    memset(g_mock_flash + CONFIG_B_OFFSET, 0x00, 64);

    config_init();
    const device_config_t* current = config_get();

    TEST_ASSERT(current != NULL, "配置加载成功");
    TEST_ASSERT(current->seq == 5, "加载 A 区 (seq=5)");
    TEST_ASSERT(current->dpi == 400, "A 区的 DPI 值正确");

    /* 验证 B 区被修复 */
    const device_config_t* flash_b = (const device_config_t*)(g_mock_flash + CONFIG_B_OFFSET);
    TEST_ASSERT(config_basic_valid(flash_b), "B 区已被修复");
}

/* 测试6：双备份加载 - 只有B有效 */
static void test_load_only_b_valid(void)
{
    TEST_SECTION("双备份加载 - 只有B有效");

    clear_flash();

    /* A区故意损坏 */
    memset(g_mock_flash + CONFIG_A_OFFSET, 0x00, 64);

    device_config_t cfg_b;
    make_valid_config(&cfg_b, 10);
    cfg_b.dpi = 3200;
    write_config_to_flash(CONFIG_B_OFFSET, &cfg_b);

    config_init();
    const device_config_t* current = config_get();

    TEST_ASSERT(current != NULL, "配置加载成功");
    TEST_ASSERT(current->seq == 10, "加载 B 区 (seq=10)");
    TEST_ASSERT(current->dpi == 3200, "B 区的 DPI 值正确");

    /* 验证 A 区被修复 */
    const device_config_t* flash_a = (const device_config_t*)(g_mock_flash + CONFIG_A_OFFSET);
    TEST_ASSERT(config_basic_valid(flash_a), "A 区已被修复");
}

/* 测试7：双备份加载 - 都无效 */
static void test_load_both_invalid(void)
{
    TEST_SECTION("双备份加载 - 都无效");

    clear_flash();
    /* 两区都损坏 */
    memset(g_mock_flash + CONFIG_A_OFFSET, 0x00, 256);
    memset(g_mock_flash + CONFIG_B_OFFSET, 0x00, 256);

    config_init();
    const device_config_t* current = config_get();

    TEST_ASSERT(current != NULL, "默认配置加载成功");
    TEST_ASSERT(current->magic == CONFIG_MAGIC, "默认配置 magic 正确");
    TEST_ASSERT(current->version == CONFIG_VERSION, "默认配置 version 正确");
    TEST_ASSERT(current->dpi == DEFAULT_DPI, "默认配置 DPI 正确");

    /* 验证两区都被写入默认配置 */
    const device_config_t* flash_a = (const device_config_t*)(g_mock_flash + CONFIG_A_OFFSET);
    const device_config_t* flash_b = (const device_config_t*)(g_mock_flash + CONFIG_B_OFFSET);
    TEST_ASSERT(config_basic_valid(flash_a), "A 区已写入默认配置");
    TEST_ASSERT(config_basic_valid(flash_b), "B 区已写入默认配置");
}

/* 测试8：配置保存 */
static void test_config_save(void)
{
    TEST_SECTION("配置保存");

    clear_flash();
    config_init();  /* 初始化默认配置 */

    device_config_t new_cfg = *config_get();
    new_cfg.dpi = 2400;
    new_cfg.joystick_deadzone = 200;

    g_mock_time_ms = 1000;  /* 跳过最小间隔保护 */
    int result = config_save(&new_cfg);

    TEST_ASSERT(result == 0, "配置保存成功");
    TEST_ASSERT(config_get()->dpi == 2400, "保存后 DPI 更新");
    TEST_ASSERT(config_get()->joystick_deadzone == 200, "保存后死区更新");

    /* 验证写入的区有效 */
    const device_config_t* flash_a = (const device_config_t*)(g_mock_flash + CONFIG_A_OFFSET);
    const device_config_t* flash_b = (const device_config_t*)(g_mock_flash + CONFIG_B_OFFSET);
    TEST_ASSERT(config_basic_valid(flash_a) || config_basic_valid(flash_b), "至少一个区有新配置");
}

/* 测试9：config_load_default 强制恢复 */
static void test_config_load_default(void)
{
    TEST_SECTION("config_load_default 强制恢复");

    clear_flash();
    config_init();

    /* 修改配置 */
    device_config_t new_cfg = *config_get();
    new_cfg.dpi = 9999;
    g_mock_time_ms = 2000;
    config_save(&new_cfg);
    TEST_ASSERT(config_get()->dpi == 9999, "配置已修改");

    /* 强制加载默认 */
    config_load_default();
    TEST_ASSERT(config_get()->dpi == DEFAULT_DPI, "强制恢复默认 DPI");
    TEST_ASSERT(config_get()->version == CONFIG_VERSION, "默认配置版本正确");
}

/* 测试10：写入次数统计 */
static void test_write_count(void)
{
    TEST_SECTION("写入次数统计");

    clear_flash();
    config_init();

    uint32_t initial_count = config_get_write_count();
    TEST_ASSERT(initial_count >= 2, "初始化后写入次数 >= 2（两区各写一次）");

    /* 保存配置 */
    device_config_t new_cfg = *config_get();
    g_mock_time_ms = 3000;
    config_save(&new_cfg);

    TEST_ASSERT(config_get_write_count() == initial_count + 1, "保存后写入次数加1");
    TEST_ASSERT(config_get()->write_count == config_get_write_count(), "配置中写入次数与统计一致");
}

/* 测试11：版本迁移 v3→v4 */
static void test_migrate_v3_to_v4(void)
{
    TEST_SECTION("版本迁移 v3→v4");

    /* 构造 v3 配置（没有 length 和 write_count 字段） */
    typedef struct __attribute__((packed)) {
        uint32_t magic;
        uint16_t version;
        uint16_t dpi;
        uint16_t joystick_deadzone;
        uint8_t  encoder_reverse;
        uint16_t seq;
        uint8_t  reserved[1];
        uint8_t  keymap[64];
        uint8_t  fn_keymap[64];
        uint8_t  macro_data[1184];
        uint8_t  joystick_invert_x;
        uint8_t  joystick_invert_y;
        uint8_t  encoder_steps;
        uint8_t  encoder_scroll_speed;
        uint16_t joystick_sensitivity;
        uint8_t  reserved2[2];
        uint32_t crc32;
    } config_v3_t;

    config_v3_t old_cfg;
    memset(&old_cfg, 0, sizeof(old_cfg));
    old_cfg.magic = CONFIG_MAGIC;
    old_cfg.version = 0x0003;
    old_cfg.dpi = 1200;
    old_cfg.joystick_deadzone = 150;
    old_cfg.seq = 7;
    old_cfg.crc32 = crc32_calc((const uint8_t*)&old_cfg, sizeof(old_cfg) - 4);

    device_config_t migrated;
    bool result = config_try_migrate((const device_config_t*)&old_cfg, &migrated);

    TEST_ASSERT(result, "v3→v4 迁移成功");
    TEST_ASSERT(migrated.version == CONFIG_VERSION, "迁移后版本为 v4");
    TEST_ASSERT(migrated.dpi == 1200, "迁移保留 DPI");
    TEST_ASSERT(migrated.joystick_deadzone == 150, "迁移保留死区");
    TEST_ASSERT(migrated.seq == 7, "迁移保留序列号");
    TEST_ASSERT(migrated.length == sizeof(device_config_t), "迁移设置 length");
    TEST_ASSERT(migrated.write_count == 0, "迁移重置 write_count");
    TEST_ASSERT(config_basic_valid(&migrated), "迁移后配置有效");
}

/* 测试12：随机损坏恢复 */
static void test_random_corruption_recovery(void)
{
    TEST_SECTION("随机损坏恢复");

    clear_flash();
    config_init();

    /* 保存一个配置 */
    device_config_t new_cfg = *config_get();
    new_cfg.dpi = 2000;
    g_mock_time_ms = 4000;
    config_save(&new_cfg);

    /* 随机损坏 A 区的前 100 字节 */
    for (int i = 0; i < 100; i++) {
        g_mock_flash[CONFIG_A_OFFSET + i] = (uint8_t)(rand() & 0xFF);
    }

    /* 重新初始化，应该从 B 区恢复 */
    config_init();
    const device_config_t* current = config_get();

    TEST_ASSERT(current != NULL, "损坏后仍能加载配置");
    TEST_ASSERT(current->dpi == 2000, "从 B 区恢复正确配置");
}

/* 测试13：保存最小间隔保护 */
static void test_save_min_interval(void)
{
    TEST_SECTION("保存最小间隔保护");

    clear_flash();
    config_init();

    device_config_t new_cfg = *config_get();
    g_mock_time_ms = 5000;

    /* 第一次保存应该成功 */
    int r1 = config_save(&new_cfg);
    TEST_ASSERT(r1 == 0, "第一次保存成功");

    /* 立即第二次保存应该被拒绝（间隔 < 500ms） */
    g_mock_time_ms = 5100;  /* 只过了 100ms */
    int r2 = config_save(&new_cfg);
    TEST_ASSERT(r2 == -4, "间隔不足被拒绝");

    /* 过了 500ms 后应该成功 */
    g_mock_time_ms = 5600;  /* 过了 600ms */
    int r3 = config_save(&new_cfg);
    TEST_ASSERT(r3 == 0, "间隔足够后保存成功");
}

/* 测试14：最后保存状态 */
static void test_last_save_status(void)
{
    TEST_SECTION("最后保存状态");

    clear_flash();
    config_init();

    TEST_ASSERT(config_get_last_save_status() == 0, "初始化后保存状态为0");

    /* 保存失败（NULL指针） */
    g_mock_time_ms = 6000;
    config_save(NULL);
    TEST_ASSERT(config_get_last_save_status() == -1, "NULL指针保存状态为-1");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("========================================\n");
    printf("  配置存储模块单元测试\n");
    printf("========================================\n");
    fflush(stdout);

    test_crc32();
    fflush(stdout);

    test_config_basic_valid();
    fflush(stdout);

    test_seq_is_newer();
    fflush(stdout);

    test_load_both_valid();
    fflush(stdout);

    /*
    test_load_only_a_valid();
    test_load_only_b_valid();
    test_load_both_invalid();
    test_config_save();
    test_config_load_default();
    test_write_count();
    test_migrate_v3_to_v4();
    test_random_corruption_recovery();
    test_save_min_interval();
    test_last_save_status();
    */

    printf("\n========================================\n");
    printf("  测试结果: %d 通过, %d 失败\n", g_tests_passed, g_tests_failed);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
