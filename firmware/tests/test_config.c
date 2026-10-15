/*
 * tests/test_config.c
 * 配置存储模块单元测试
 * 测试对外接口行为
 */

#include "unity.h"
#include "mock/mock_flash.h"
#include "board/config.h"
#include "middleware/crc32.h"
#include <stdint.h>
#include <string.h>

/* ==================== 辅助函数 ==================== */

/* 构造一个有效的配置（通过config_save来构造） */
static void make_saved_config(uint16_t seq, uint16_t dpi)
{
    /* 先加载默认配置 */
    config_init();

    /* 修改配置 */
    device_config_t cfg = *config_get();
    cfg.dpi = dpi;
    cfg.seq = seq;  /* seq会被config_save自动加1，所以这里先减1 */
    /* 不对，config_save会自动加1，所以我们直接保存，让它自己管理seq */

    /* 保存几次，让seq达到想要的值 */
    for (uint16_t i = 0; i < seq; i++)
    {
        device_config_t tmp = *config_get();
        config_save(&tmp);
    }

    /* 最后一次保存，设置dpi */
    device_config_t final = *config_get();
    final.dpi = dpi;
    config_save(&final);
}

/* 直接往Flash里写一个损坏的配置（魔数错误） */
static void write_corrupted_config_a(void)
{
    device_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = 0xDEADBEEF;  /* 错误的魔数 */
    mock_flash_direct_write(CONFIG_FLASH_OFFSET_A,
                            (const uint8_t*)&cfg,
                            sizeof(cfg));
}

static void write_corrupted_config_b(void)
{
    device_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = 0xDEADBEEF;  /* 错误的魔数 */
    mock_flash_direct_write(CONFIG_FLASH_OFFSET_B,
                            (const uint8_t*)&cfg,
                            sizeof(cfg));
}

/* 每个测试前重置Flash和config状态
 * 注意：config模块的内部状态是static的，我们没法直接重置，
 * 所以通过重置Flash，然后重新init来模拟。
 */
static void config_test_setup(void)
{
    mock_flash_reset();
    /* config模块的s_initialized等静态变量，我们没法直接重置，
     * 但是没关系，每次测试都调用config_init重新加载就可以了
     */
}

/* ==================== 测试用例 ==================== */

/* 测试1：首次启动 - 空Flash，加载默认配置 */
void test_config_init_empty_flash(void)
{
    config_test_setup();

    config_init();

    const device_config_t* cfg = config_get();
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_UINT32(CONFIG_MAGIC, cfg->magic);
    TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, cfg->version);
    TEST_ASSERT_EQUAL_UINT16(DEFAULT_DPI, cfg->dpi);
    TEST_ASSERT_EQUAL_UINT16(DEFAULT_DEADZONE, cfg->joystick_deadzone);
    TEST_ASSERT_EQUAL_UINT8(DEFAULT_ENCODER_REV, cfg->encoder_reverse);
}

/* 测试2：保存配置 - 保存后可以读回来 */
void test_config_save_and_load(void)
{
    config_test_setup();

    config_init();

    /* 修改DPI并保存 */
    device_config_t cfg = *config_get();
    cfg.dpi = 800;
    int status = config_save(&cfg);
    TEST_ASSERT_EQUAL_INT(0, status);

    /* 验证当前配置已经更新 */
    TEST_ASSERT_EQUAL_UINT16(800, config_get()->dpi);

    /* 重新初始化，模拟重启，验证配置从Flash加载回来 */
    config_init();
    TEST_ASSERT_EQUAL_UINT16(800, config_get()->dpi);
}

/* 测试3：保存配置 - seq递增 */
void test_config_save_seq_increments(void)
{
    config_test_setup();

    config_init();
    uint16_t seq1 = config_get()->seq;

    /* 保存一次 */
    device_config_t cfg = *config_get();
    config_save(&cfg);
    uint16_t seq2 = config_get()->seq;

    TEST_ASSERT_EQUAL_UINT16(seq1 + 1, seq2);

    /* 再保存一次 */
    cfg = *config_get();
    config_save(&cfg);
    uint16_t seq3 = config_get()->seq;

    TEST_ASSERT_EQUAL_UINT16(seq2 + 1, seq3);
}

/* 测试4：双备份 - 保存两次，应该在两个扇区交替写入 */
void test_config_alternate_sectors(void)
{
    config_test_setup();

    config_init();
    /* config_init 时空Flash会写入A区(seq=1)和B区(seq=2)，当前seq=2 */

    /* 第一次保存：seq=3 */
    device_config_t cfg = *config_get();
    cfg.dpi = 400;
    config_save(&cfg);
    TEST_ASSERT_EQUAL_UINT16(400, config_get()->dpi);

    /* 第二次保存：seq=4 */
    cfg = *config_get();
    cfg.dpi = 800;
    config_save(&cfg);
    TEST_ASSERT_EQUAL_UINT16(800, config_get()->dpi);

    /* 第三次保存：seq=5 */
    cfg = *config_get();
    cfg.dpi = 1600;
    config_save(&cfg);
    TEST_ASSERT_EQUAL_UINT16(1600, config_get()->dpi);

    /* 重新初始化，应该加载最新的（seq=5） */
    config_init();
    TEST_ASSERT_EQUAL_UINT16(1600, config_get()->dpi);
    TEST_ASSERT_EQUAL_UINT16(5, config_get()->seq);
}

/* 测试5：双备份 - 一个扇区损坏，加载另一个 */
void test_config_one_sector_corrupted(void)
{
    config_test_setup();

    /* 先保存两次，让A和B都有有效配置 */
    config_init();
    /* config_init 后 seq=2 */
    device_config_t cfg = *config_get();
    cfg.dpi = 400;
    config_save(&cfg);  /* seq=3 */
    cfg = *config_get();
    cfg.dpi = 800;
    config_save(&cfg);  /* seq=4 */

    /* 损坏A区 */
    write_corrupted_config_a();

    /* 重新初始化，应该加载B区（seq=4） */
    config_init();
    TEST_ASSERT_EQUAL_UINT16(800, config_get()->dpi);
    TEST_ASSERT_EQUAL_UINT16(4, config_get()->seq);
}

/* 测试6：双备份 - 两个扇区都损坏，加载默认配置 */
void test_config_both_sectors_corrupted(void)
{
    config_test_setup();

    /* 两个扇区都损坏 */
    write_corrupted_config_a();
    write_corrupted_config_b();

    /* 初始化，应该加载默认配置 */
    config_init();
    TEST_ASSERT_EQUAL_UINT16(DEFAULT_DPI, config_get()->dpi);
}

/* 测试7：重置默认配置 */
void test_config_reset_default(void)
{
    config_test_setup();

    config_init();

    /* 修改配置并保存 */
    device_config_t cfg = *config_get();
    cfg.dpi = 3200;
    cfg.joystick_deadzone = 200;
    config_save(&cfg);
    TEST_ASSERT_EQUAL_UINT16(3200, config_get()->dpi);

    /* 重置为默认配置 */
    config_reset_default();

    /* 验证已经重置 */
    TEST_ASSERT_EQUAL_UINT16(DEFAULT_DPI, config_get()->dpi);
    TEST_ASSERT_EQUAL_UINT16(DEFAULT_DEADZONE, config_get()->joystick_deadzone);

    /* 重新初始化，验证Flash里也已经是默认值 */
    config_init();
    TEST_ASSERT_EQUAL_UINT16(DEFAULT_DPI, config_get()->dpi);
}

/* 测试8：config_get_default - 返回默认配置 */
void test_config_get_default(void)
{
    config_test_setup();

    const device_config_t* def = config_get_default();
    TEST_ASSERT_NOT_NULL(def);
    TEST_ASSERT_EQUAL_UINT16(DEFAULT_DPI, def->dpi);
    TEST_ASSERT_EQUAL_UINT16(DEFAULT_DEADZONE, def->joystick_deadzone);
}

/* 测试9：config_save - NULL指针返回错误 */
void test_config_save_null(void)
{
    config_test_setup();

    config_init();
    int status = config_save(NULL);
    TEST_ASSERT_NOT_EQUAL(0, status);
}

/* 测试10：掉电保护 - 写入过程中断电，旧配置还在
 * 模拟：写入B区时断电，B区损坏，但A区完好
 */
void test_config_power_failure_protection(void)
{
    config_test_setup();

    /* 先保存一次，让A区有有效配置 */
    config_init();
    /* config_init 后 seq=2，A区seq=1，B区seq=2 */
    device_config_t cfg = *config_get();
    cfg.dpi = 400;
    config_save(&cfg);  /* seq=3 */

    /* 再保存一次，应该写另一个区（seq=4，dpi=800） */
    cfg = *config_get();
    cfg.dpi = 800;
    config_save(&cfg);  /* seq=4 */

    /* 模拟最新的区写入时断电（损坏最新的区） */
    /* 需要确定哪个区是最新的（seq=4），损坏它 */
    /* 由于config_init时A=seq1, B=seq2，然后save1→seq3(写A), save2→seq4(写B) */
    /* 所以B区是最新的(seq4)，损坏B区 */
    write_corrupted_config_b();

    /* 重新初始化，应该加载A区的旧配置（seq=3，dpi=400） */
    config_init();
    TEST_ASSERT_EQUAL_UINT16(400, config_get()->dpi);
    TEST_ASSERT_EQUAL_UINT16(3, config_get()->seq);
}

/* 测试11：配置大小不超过Flash扇区大小 */
void test_config_size_fits_sector(void)
{
    /* 配置结构体大小应该小于等于Flash扇区大小 */
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(CONFIG_FLASH_SIZE, sizeof(device_config_t));
    /* 不对，应该是配置大小 <= 扇区大小 */
    TEST_ASSERT_TRUE(sizeof(device_config_t) <= CONFIG_FLASH_SIZE);
}

/* 测试12：默认配置的keymap不为空 */
void test_config_default_keymap_not_empty(void)
{
    config_test_setup();

    config_init();
    const device_config_t* cfg = config_get();

    /* 检查keymap里至少有一些非零值 */
    int non_zero = 0;
    for (int i = 0; i < 64; i++)
    {
        if (cfg->keymap[i] != 0)
        {
            non_zero++;
        }
    }
    TEST_ASSERT_GREATER_THAN_INT(0, non_zero);
}

/* ==================== 版本迁移测试 ==================== */

/* 辅助：构造一个指定版本的配置并写入Flash A区 */
static void write_old_version_config(uint16_t version, uint16_t dpi, uint16_t deadzone)
{
    device_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* 先加载默认配置作为基底 */
    config_init();
    memcpy(&cfg, config_get(), sizeof(cfg));

    /* 修改字段 */
    cfg.dpi = dpi;
    cfg.joystick_deadzone = deadzone;
    cfg.version = version;  /* 设置为旧版本 */
    cfg.magic = CONFIG_MAGIC;
    /* 关键：设置序列号为3，确保比 config_init() 写入的 B区(序列号2)新，
     * 这样 config_init() 会选择 A区旧配置，触发版本迁移 */
    cfg.seq = 3;
    cfg.write_count = 3;

    /* 使用固件的 crc32_calc 函数重新计算CRC（确保与固件验证一致） */
    cfg.crc32 = 0;
    cfg.crc32 = crc32_calc((const uint8_t *)&cfg, sizeof(cfg) - sizeof(uint32_t));

    /* 写入Flash A区 */
    mock_flash_direct_write(CONFIG_FLASH_OFFSET_A,
                            (const uint8_t *)&cfg,
                            sizeof(cfg));
}

/* 测试13：v1配置迁移到最新版本 */
void test_config_migrate_v1_to_latest(void)
{
    config_test_setup();

    /* 写入一个v1配置，DPI=1200，死区=200 */
    write_old_version_config(1, 1200, 200);

    /* 重新初始化，触发迁移 */
    config_init();

    const device_config_t *cfg = config_get();

    /* 验证迁移到最新版本 */
    TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, cfg->version);
    TEST_ASSERT_EQUAL_UINT32(CONFIG_MAGIC, cfg->magic);

    /* 验证旧字段保留 */
    TEST_ASSERT_EQUAL_UINT16(1200, cfg->dpi);
    TEST_ASSERT_EQUAL_UINT16(200, cfg->joystick_deadzone);
}

/* 测试14：v3配置迁移到最新版本 */
void test_config_migrate_v3_to_latest(void)
{
    config_test_setup();

    /* 写入一个v3配置，DPI=1600，死区=300 */
    write_old_version_config(3, 1600, 300);

    /* 重新初始化，触发迁移 */
    config_init();

    const device_config_t *cfg = config_get();

    /* 验证迁移到最新版本 */
    TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, cfg->version);

    /* 验证旧字段保留 */
    TEST_ASSERT_EQUAL_UINT16(1600, cfg->dpi);
    TEST_ASSERT_EQUAL_UINT16(300, cfg->joystick_deadzone);

    /* 验证v4新增字段有默认值 */
    TEST_ASSERT_EQUAL_UINT16(sizeof(device_config_t), cfg->length);
}

/* 测试15：v5配置迁移到最新版本 */
void test_config_migrate_v5_to_latest(void)
{
    config_test_setup();

    /* 写入一个v5配置 */
    write_old_version_config(5, 800, 150);

    /* 重新初始化，触发迁移 */
    config_init();

    const device_config_t *cfg = config_get();

    /* 验证迁移到最新版本 */
    TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, cfg->version);

    /* 验证旧字段保留 */
    TEST_ASSERT_EQUAL_UINT16(800, cfg->dpi);

    /* 验证v6新增字段有默认值（DPI档位） */
    TEST_ASSERT_GREATER_THAN_UINT16(0, cfg->dpi_slot[0]);
}

/* 测试16：无效版本号不崩溃 */
void test_config_invalid_version_handled(void)
{
    config_test_setup();

    /* 写入一个版本号为0的配置（无效） */
    write_old_version_config(0, 800, 100);

    /* 重新初始化，应该安全处理（加载默认配置或迁移） */
    config_init();

    const device_config_t *cfg = config_get();
    TEST_ASSERT_NOT_NULL(cfg);
    /* 版本应该是有效的（最新版本或默认） */
    TEST_ASSERT_GREATER_THAN_UINT16(0, cfg->version);
}
