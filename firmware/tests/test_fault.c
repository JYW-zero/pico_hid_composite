/*
 * tests/test_fault.c
 * fault 模块单元测试
 */
#include "unity.h"
#include "middleware/fault.h"

/* 测试初始状态计数为0 */
void test_fault_initial_count(void)
{
    fault_clear();
    TEST_ASSERT_EQUAL_UINT32(0, fault_get_count());
}

/* 测试记录1条故障 */
void test_fault_record_one(void)
{
    fault_clear();
    fault_record(FAULT_LEVEL_INFO, "test", "test message");
    TEST_ASSERT_EQUAL_UINT32(1, fault_get_count());
}

/* 测试记录多条故障 */
void test_fault_record_multiple(void)
{
    fault_clear();
    fault_record(FAULT_LEVEL_INFO, "test", "msg1");
    fault_record(FAULT_LEVEL_WARN, "test", "msg2");
    fault_record(FAULT_LEVEL_ERROR, "test", "msg3");
    TEST_ASSERT_EQUAL_UINT32(3, fault_get_count());
}

/* 测试清除故障记录 */
void test_fault_clear(void)
{
    fault_clear();
    fault_record(FAULT_LEVEL_INFO, "test", "msg1");
    fault_record(FAULT_LEVEL_WARN, "test", "msg2");
    TEST_ASSERT_EQUAL_UINT32(2, fault_get_count());

    fault_clear();
    TEST_ASSERT_EQUAL_UINT32(0, fault_get_count());
}

/* 测试所有故障级别都能正常记录 */
void test_fault_all_levels(void)
{
    fault_clear();
    fault_record(FAULT_LEVEL_INFO, "mod", "info msg");
    fault_record(FAULT_LEVEL_WARN, "mod", "warn msg");
    fault_record(FAULT_LEVEL_ERROR, "mod", "error msg");
    fault_record(FAULT_LEVEL_FATAL, "mod", "fatal msg");
    TEST_ASSERT_EQUAL_UINT32(4, fault_get_count());
}

