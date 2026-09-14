#include "unity.h"

#include <string.h>

#include "zr_vm_parser/exec_ir_send_sync.h"

static SZrExecIrFunction function;
static SZrExecIrValue values[2];

void setUp(void) {
    memset(&function, 0, sizeof(function));
    memset(values, 0, sizeof(values));
    function.values = values;
    function.valueCapacity = 2u;
}

void tearDown(void) {}

static void test_immutable_values_are_send_and_sync(void) {
    SZrExecIrSendSyncSummary summary;
    values[0].id = 1u;
    values[0].ownership = ZR_EXEC_IR_OWNERSHIP_SHARED;
    function.valueCount = 1u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_InferSendSync(&function, &summary, ZR_NULL));
    TEST_ASSERT_TRUE(summary.send);
    TEST_ASSERT_TRUE(summary.sync);
}

static void test_unique_mutable_value_is_send_only(void) {
    SZrExecIrSendSyncSummary summary;
    values[0].id = 7u;
    values[0].ownership = ZR_EXEC_IR_OWNERSHIP_UNIQUE;
    function.valueCount = 1u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_InferSendSync(&function, &summary, ZR_NULL));
    TEST_ASSERT_TRUE(summary.send);
    TEST_ASSERT_FALSE(summary.sync);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_SEND_SYNC_REASON_UNIQUE_MUTABLE, summary.reason);
    TEST_ASSERT_EQUAL_UINT32(7u, summary.reasonValueId);
}

static void test_borrowed_alias_is_rejected_with_reason(void) {
    SZrExecIrSendSyncSummary summary;
    values[0].id = 9u;
    values[0].ownership = ZR_EXEC_IR_OWNERSHIP_BORROWED;
    function.valueCount = 1u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_InferSendSync(&function, &summary, ZR_NULL));
    TEST_ASSERT_FALSE(summary.send);
    TEST_ASSERT_FALSE(summary.sync);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_SEND_SYNC_REASON_BORROWED_ALIAS, summary.reason);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_immutable_values_are_send_and_sync);
    RUN_TEST(test_unique_mutable_value_is_send_only);
    RUN_TEST(test_borrowed_alias_is_rejected_with_reason);
    return UNITY_END();
}
