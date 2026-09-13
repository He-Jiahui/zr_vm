#include "unity.h"

#include "zr_vm_core/execution_call_transfer.h"
#include "zr_vm_parser/exec_ir_call_transfer.h"

void setUp(void) {}
void tearDown(void) {}

static void test_return_forwarding_requires_commit_and_layout(void) {
    SZrExecutionReturnTransfer transfer = { ZR_TRUE, ZR_TRUE, ZR_TRUE, ZR_FALSE };
    TEST_ASSERT_TRUE(ZrCore_Execution_CanForwardReturn(&transfer));
    transfer.requiresWriteback = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrCore_Execution_CanForwardReturn(&transfer));
    transfer.requiresWriteback = ZR_FALSE;
    transfer.compatibleLayout = ZR_FALSE;
    TEST_ASSERT_FALSE(ZrCore_Execution_CanForwardReturn(&transfer));
}

static void test_tail_reuse_rejects_cleanup_or_escaping_alias(void) {
    SZrExecutionTailEligibility eligibility = { ZR_TRUE, ZR_TRUE, ZR_TRUE, ZR_TRUE };
    TEST_ASSERT_TRUE(ZrCore_Execution_CanReuseTailFrame(&eligibility));
    eligibility.noPendingCleanup = ZR_FALSE;
    TEST_ASSERT_FALSE(ZrCore_Execution_CanReuseTailFrame(&eligibility));
    eligibility.noPendingCleanup = ZR_TRUE;
    eligibility.noEscapingFrameAlias = ZR_FALSE;
    TEST_ASSERT_FALSE(ZrCore_Execution_CanReuseTailFrame(&eligibility));
}

static void test_parser_transfer_classification_is_transactional(void) {
    SZrExecIrPackedFrameLayout source;
    SZrExecIrPackedFrameLayout target;
    SZrExecIrCallTransferValue value;
    SZrExecIrCallTransferRequest request;
    SZrExecIrCallTransferPlan plan;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_ExecIr_PackedFrameLayoutInit(&source);
    ZrParser_ExecIr_PackedFrameLayoutInit(&target);
    ZrParser_ExecIr_CallTransferPlanInit(&plan);
    source.frame.parameterPrefixCount = 1u;
    target.frame.parameterPrefixCount = 1u;
    source.frame.storageSlotCount = 4u;
    target.frame.storageSlotCount = 4u;
    value.valueId = 1u;
    value.ownership = ZR_EXEC_IR_OWNERSHIP_UNIQUE;
    value.slotClass = ZR_EXEC_IR_PACKED_SLOT_SCALAR;
    value.sourceSlot = 2u;
    value.targetSlot = 3u;
    request.sourceLayout = &source;
    request.targetLayout = &target;
    request.values = &value;
    request.valueCount = 1u;
    request.returnMayAlias = ZR_FALSE;
    request.requiresWriteback = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_TRANSFER_MOVE, plan.kinds[0]);
    value.ownership = ZR_EXEC_IR_OWNERSHIP_COUNT;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_TRANSFER_MOVE, plan.kinds[0]);
    ZrParser_ExecIr_CallTransferPlanFree(&plan);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_return_forwarding_requires_commit_and_layout);
    RUN_TEST(test_tail_reuse_rejects_cleanup_or_escaping_alias);
    RUN_TEST(test_parser_transfer_classification_is_transactional);
    return UNITY_END();
}
