#include "unity.h"

#include "zr_vm_core/execution_call_transfer.h"
#include "zr_vm_parser/exec_ir_call_transfer.h"
#include <string.h>

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
    SZrExecIrPackedValue sourceValue = { 11u, 7u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u };
    SZrExecIrPackedValue targetValue = { 91u, 7u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u };
    SZrExecIrPackedFrameRequest packed = { 1u, &sourceValue, 1u, 0u, 0u, 0u, 0u };
    SZrExecIrCallTransferValue value = { 11u, ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 0u, 0u, 8u, 0u };
    SZrExecIrCallTransferRequest request = { &source, &target, &value, 1u, ZR_FALSE, ZR_FALSE };
    SZrExecIrCallTransferPlan plan;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_ExecIr_PackedFrameLayoutInit(&source);
    ZrParser_ExecIr_PackedFrameLayoutInit(&target);
    ZrParser_ExecIr_CallTransferPlanInit(&plan);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &source, &diagnostic));
    packed.values = &targetValue;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &target, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_TRANSFER_MOVE, plan.kinds[0]);
    value.ownership = ZR_EXEC_IR_OWNERSHIP_BORROWED;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_TRANSFER_BORROW, plan.kinds[0]);
    value.ownership = ZR_EXEC_IR_OWNERSHIP_SHARED;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_TRANSFER_SCALAR_COPY, plan.kinds[0]);
    value.ownership = ZR_EXEC_IR_OWNERSHIP_GC;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_TRANSFER_SCALAR_COPY, plan.kinds[0]);
    value.ownership = ZR_EXEC_IR_OWNERSHIP_UNIQUE;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    value.ownership = ZR_EXEC_IR_OWNERSHIP_COUNT;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_TRANSFER_MOVE, plan.kinds[0]);
    ZrParser_ExecIr_CallTransferPlanFree(&plan);
    ZrParser_ExecIr_PackedFrameLayoutFree(&source);
    ZrParser_ExecIr_PackedFrameLayoutFree(&target);
}

static void test_parser_transfer_rejects_actual_representation_mismatch(void) {
    SZrExecIrPackedFrameLayout source, target;
    SZrExecIrPackedValue sv = { 11u, 7u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u };
    SZrExecIrPackedValue tv = { 91u, 9u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u };
    SZrExecIrPackedFrameRequest packed = { 1u, &sv, 1u, 0u, 0u, 0u, 0u };
    SZrExecIrCallTransferValue value = { 11u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 0u, 0u, 8u, 0u };
    SZrExecIrCallTransferRequest request = { &source, &target, &value, 1u, ZR_FALSE, ZR_FALSE };
    SZrExecIrCallTransferPlan plan;
    SZrExecIrDiagnostic diagnostic;
    ZrParser_ExecIr_PackedFrameLayoutInit(&source);
    ZrParser_ExecIr_PackedFrameLayoutInit(&target);
    ZrParser_ExecIr_CallTransferPlanInit(&plan);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &source, &diagnostic));
    packed.values = &tv;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &target, &diagnostic));
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(0u, diagnostic.actualVersion);
    ZrParser_ExecIr_CallTransferPlanFree(&plan);
    ZrParser_ExecIr_PackedFrameLayoutFree(&source);
    ZrParser_ExecIr_PackedFrameLayoutFree(&target);
}

static void test_parser_transfer_checks_spans_and_preserves_whole_plan(void) {
    SZrExecIrPackedFrameLayout source, target;
    SZrExecIrPackedValue sv[] = {
        { 12u, 8u, ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN, 16u, 16u, 0u, 5u, 0u },
        { 11u, 7u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 4u, 4u, 0u, 5u, 0u }
    };
    SZrExecIrPackedValue tv[] = {
        { 91u, 7u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 4u, 4u, 0u, 5u, 0u },
        { 92u, 8u, ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN, 16u, 16u, 0u, 5u, 0u }
    };
    SZrExecIrPackedFrameRequest packed = { 1u, sv, 2u, 0u, 0u, 0u, 0u };
    SZrExecIrCallTransferValue values[] = {
        { 11u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 0u, 0u, 4u, 0u },
        { 12u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN, 1u, 1u, 16u, 0u }
    };
    SZrExecIrCallTransferRequest request = { &source, &target, values, 2u, ZR_FALSE, ZR_FALSE };
    SZrExecIrCallTransferPlan plan, old;
    SZrExecIrCallTransferSpan oldSpans[2];
    EZrExecIrTransferKind oldKinds[2];
    TZrUInt32 oldSourceSlots[2], oldTargetSlots[2];
    SZrExecIrDiagnostic diagnostic;
    ZrParser_ExecIr_PackedFrameLayoutInit(&source);
    ZrParser_ExecIr_PackedFrameLayoutInit(&target);
    ZrParser_ExecIr_CallTransferPlanInit(&plan);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &source, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, source.logicalToPhysical[0]);
    TEST_ASSERT_EQUAL_UINT32(0u, source.logicalToPhysical[1]);
    packed.values = tv;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &target, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_TRANSFER_SCALAR_COPY, plan.kinds[0]);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_TRANSFER_SPAN_COPY, plan.kinds[1]);
    TEST_ASSERT_FALSE(plan.forwardReturn);
    TEST_ASSERT_TRUE(plan.compatibleLayout);
    TEST_ASSERT_EQUAL_UINT32(0u, plan.spans[0].sourceByteOffset);
    TEST_ASSERT_EQUAL_UINT32(16u, plan.spans[1].sourceByteOffset);
    TEST_ASSERT_EQUAL_UINT32(16u, plan.spans[1].targetByteOffset);
    TEST_ASSERT_EQUAL_UINT32(16u, plan.spans[1].byteSize);
    TEST_ASSERT_EQUAL_UINT32(16u, plan.spans[1].sourceByteAlign);
    TEST_ASSERT_EQUAL_UINT32(16u, plan.spans[1].targetByteAlign);
    TEST_ASSERT_EQUAL_UINT32(8u, plan.spans[1].typeToken);
    TEST_ASSERT_EQUAL_UINT64(source.frame.layoutHash, plan.sourceLayoutHash);
    TEST_ASSERT_EQUAL_UINT64(target.frame.layoutHash, plan.targetLayoutHash);
    memcpy(oldSpans, plan.spans, sizeof(oldSpans));
    memcpy(oldKinds, plan.kinds, sizeof(oldKinds));
    memcpy(oldSourceSlots, plan.sourceSlots, sizeof(oldSourceSlots));
    memcpy(oldTargetSlots, plan.targetSlots, sizeof(oldTargetSlots));
    old = plan;
    values[1].byteSize = 8u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.actualVersion);
    TEST_ASSERT_EQUAL_MEMORY(&old, &plan, sizeof(plan));
    TEST_ASSERT_EQUAL_MEMORY(oldSpans, plan.spans, sizeof(oldSpans));
    TEST_ASSERT_EQUAL_MEMORY(oldKinds, plan.kinds, sizeof(oldKinds));
    TEST_ASSERT_EQUAL_MEMORY(oldSourceSlots, plan.sourceSlots, sizeof(oldSourceSlots));
    TEST_ASSERT_EQUAL_MEMORY(oldTargetSlots, plan.targetSlots, sizeof(oldTargetSlots));
    TEST_ASSERT_EQUAL_UINT32(0u, plan.sourceSlots[0]);
    TEST_ASSERT_EQUAL_UINT32(1u, plan.targetSlots[1]);
    values[1].byteSize = 16u;
    source.frame.slots[1].typeToken = 0u;
    target.frame.slots[1].typeToken = 0u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.actualVersion);
    TEST_ASSERT_EQUAL_MEMORY(&old, &plan, sizeof(plan));
    TEST_ASSERT_EQUAL_MEMORY(oldSpans, plan.spans, sizeof(oldSpans));
    TEST_ASSERT_EQUAL_MEMORY(oldKinds, plan.kinds, sizeof(oldKinds));
    TEST_ASSERT_EQUAL_MEMORY(oldSourceSlots, plan.sourceSlots, sizeof(oldSourceSlots));
    TEST_ASSERT_EQUAL_MEMORY(oldTargetSlots, plan.targetSlots, sizeof(oldTargetSlots));
    source.frame.slots[1].typeToken = 8u;
    target.frame.slots[1].typeToken = 8u;
    values[1].flags = 1u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    values[1].flags = 0u;
    values[1].ownership = (EZrExecIrOwnership)-1;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    values[1].ownership = ZR_EXEC_IR_OWNERSHIP_UNKNOWN;
    values[1].slotClass = (EZrExecIrPackedSlotClass)-1;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    values[1].slotClass = ZR_EXEC_IR_PACKED_SLOT_SCALAR;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    values[1].slotClass = ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN;
    target.frame.slots[1].byteAlign = 8u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    target.frame.slots[1].byteAlign = 16u;
    target.frame.slots[1].typeToken = 7u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    target.frame.slots[1].typeToken = 8u;
    target.frame.slots[1].byteOffset = 17u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    target.frame.slots[1].byteOffset = 16u;
    target.frame.slots[1].byteSize = 0xffffffffu;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    target.frame.slots[1].byteSize = 16u;
    values[1].valueId = 11u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    values[1].valueId = 12u;
    target.logicalToPhysical[1] = 7u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    target.logicalToPhysical[1] = 1u;
    TEST_ASSERT_EQUAL_MEMORY(&old, &plan, sizeof(plan));
    ZrParser_ExecIr_CallTransferPlanFree(&plan);
    ZrParser_ExecIr_PackedFrameLayoutFree(&source);
    ZrParser_ExecIr_PackedFrameLayoutFree(&target);
}

static void test_parser_transfer_reuse_and_empty_layout_contract(void) {
    SZrExecIrPackedFrameLayout source, target;
    SZrExecIrPackedValue sv[] = {
        { 11u, 7u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 1u, 0u },
        { 12u, 9u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 4u, 4u, 1u, 2u, 0u },
        { 13u, 8u, ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN, 16u, 16u, 0u, 2u, 0u }
    };
    SZrExecIrPackedValue tv = { 91u, 8u, ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN, 16u, 16u, 0u, 2u, 0u };
    SZrExecIrPackedFrameRequest packed = { 1u, sv, 3u, 0u, 0u, 0u, 0u };
    SZrExecIrCallTransferValue value = { 13u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN, 1u, 0u, 16u, 0u };
    SZrExecIrCallTransferRequest request = { &source, &target, &value, 1u, ZR_FALSE, ZR_FALSE };
    SZrExecIrCallTransferPlan plan;
    SZrExecIrDiagnostic diagnostic;
    ZrParser_ExecIr_PackedFrameLayoutInit(&source);
    ZrParser_ExecIr_PackedFrameLayoutInit(&target);
    ZrParser_ExecIr_CallTransferPlanInit(&plan);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &source, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(source.logicalToPhysical[0], source.logicalToPhysical[1]);
    packed.values = &tv;
    packed.valueCount = 1u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &target, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    tv.typeToken = 7u;
    tv.slotClass = ZR_EXEC_IR_PACKED_SLOT_SCALAR;
    tv.byteSize = 8u;
    tv.byteAlign = 8u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &target, &diagnostic));
    value.sourceSlot = 0u;
    value.valueId = 11u;
    value.slotClass = ZR_EXEC_IR_PACKED_SLOT_SCALAR;
    value.byteSize = 8u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    request.valueCount = 0u;
    request.values = ZR_NULL;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0u, plan.valueCount);
    TEST_ASSERT_NULL(plan.kinds);
    TEST_ASSERT_NULL(plan.spans);
    TEST_ASSERT_FALSE(plan.forwardReturn);
    source.frame.slotCount = 0u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    source.frame.slotCount = source.frame.storageSlotCount;
    ZrParser_ExecIr_CallTransferPlanFree(&plan);
    ZrParser_ExecIr_PackedFrameLayoutFree(&source);
    ZrParser_ExecIr_PackedFrameLayoutFree(&target);
}

static void test_parser_transfer_bridge_metadata_and_duplicate_endpoints(void) {
    SZrExecIrPackedFrameLayout source, target;
    SZrExecIrPackedValue sv = { 21u, 9u, ZR_EXEC_IR_PACKED_SLOT_REF, 8u, 8u, 0u, 2u, 0u };
    SZrExecIrPackedValue tv[] = {
        { 31u, 9u, ZR_EXEC_IR_PACKED_SLOT_REF, 8u, 8u, 0u, 2u, 0u },
        { 32u, 9u, ZR_EXEC_IR_PACKED_SLOT_REF, 8u, 8u, 0u, 2u, 0u }
    };
    SZrExecIrPackedFrameRequest packed = { 1u, &sv, 1u, 0u, 0u, 0u, 0u };
    SZrExecIrCallTransferValue values[] = {
        { 21u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_PACKED_SLOT_REF, 0u, 0u, 8u, 0u },
        { 21u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_PACKED_SLOT_REF, 0u, 1u, 8u, 0u }
    };
    SZrExecIrCallTransferRequest request = { &source, &target, values, 2u, ZR_FALSE, ZR_FALSE };
    SZrExecIrCallTransferPlan plan, old;
    SZrExecIrDiagnostic diagnostic;
    ZrParser_ExecIr_PackedFrameLayoutInit(&source);
    ZrParser_ExecIr_PackedFrameLayoutInit(&target);
    ZrParser_ExecIr_CallTransferPlanInit(&plan);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &source, &diagnostic));
    packed.values = tv;
    packed.valueCount = 2u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &target, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_TRANSFER_BOXED_BRIDGE, plan.kinds[0]);
    old = plan;
    values[1].targetSlot = 0u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.actualVersion);
    TEST_ASSERT_EQUAL_MEMORY(&old, &plan, sizeof(plan));
    values[1].targetSlot = 1u;
    values[1].ownership = ZR_EXEC_IR_OWNERSHIP_UNIQUE;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&old, &plan, sizeof(plan));
    ZrParser_ExecIr_CallTransferPlanFree(&plan);
    ZrParser_ExecIr_PackedFrameLayoutFree(&source);
    ZrParser_ExecIr_PackedFrameLayoutFree(&target);
    ZrParser_ExecIr_PackedFrameLayoutInit(&source);
    ZrParser_ExecIr_PackedFrameLayoutInit(&target);
    packed.values = ZR_NULL;
    packed.valueCount = 0u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &source, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &target, &diagnostic));
    request.valueCount = 0u;
    request.values = ZR_NULL;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_FALSE(plan.forwardReturn);
    TEST_ASSERT_NULL(plan.kinds);
    ZrParser_ExecIr_CallTransferPlanFree(&plan);
    ZrParser_ExecIr_PackedFrameLayoutFree(&source);
    ZrParser_ExecIr_PackedFrameLayoutFree(&target);
}

static void test_parser_transfer_requires_selected_nonzero_type_proof(void) {
    SZrExecIrPackedFrameLayout source, target;
    SZrExecIrPackedValue sv[] = {
        { 11u, 7u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u },
        { 12u, 8u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u }
    };
    SZrExecIrPackedValue tv[] = {
        { 91u, 7u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u },
        { 92u, 8u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u }
    };
    SZrExecIrPackedFrameRequest packed = { 1u, sv, 2u, 0u, 0u, 0u, 0u };
    SZrExecIrCallTransferValue value = { 11u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 0u, 0u, 8u, 0u };
    SZrExecIrCallTransferRequest request = { &source, &target, &value, 1u, ZR_FALSE, ZR_FALSE };
    SZrExecIrCallTransferPlan plan, old;
    SZrExecIrCallTransferSpan oldSpan;
    SZrExecIrDiagnostic diagnostic;
    ZrParser_ExecIr_PackedFrameLayoutInit(&source);
    ZrParser_ExecIr_PackedFrameLayoutInit(&target);
    ZrParser_ExecIr_CallTransferPlanInit(&plan);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &source, &diagnostic));
    packed.values = tv;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutPackedFrame(&packed, &target, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    old = plan;
    oldSpan = plan.spans[0];
    source.frame.slots[0].typeToken = 0u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&old, &plan, sizeof(plan));
    source.frame.slots[0].typeToken = 7u;
    target.frame.slots[0].typeToken = 0u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&old, &plan, sizeof(plan));
    source.frame.slots[0].typeToken = 0u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(0u, diagnostic.actualVersion);
    TEST_ASSERT_EQUAL_MEMORY(&old, &plan, sizeof(plan));
    TEST_ASSERT_EQUAL_MEMORY(&oldSpan, plan.spans, sizeof(oldSpan));
    value.valueId = 12u;
    value.sourceSlot = 1u;
    value.targetSlot = 1u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_PrepareCallTransfer(&request, &plan, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(8u, plan.spans[0].typeToken);
    ZrParser_ExecIr_CallTransferPlanFree(&plan);
    ZrParser_ExecIr_PackedFrameLayoutFree(&source);
    ZrParser_ExecIr_PackedFrameLayoutFree(&target);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_return_forwarding_requires_commit_and_layout);
    RUN_TEST(test_tail_reuse_rejects_cleanup_or_escaping_alias);
    RUN_TEST(test_parser_transfer_classification_is_transactional);
    RUN_TEST(test_parser_transfer_rejects_actual_representation_mismatch);
    RUN_TEST(test_parser_transfer_checks_spans_and_preserves_whole_plan);
    RUN_TEST(test_parser_transfer_reuse_and_empty_layout_contract);
    RUN_TEST(test_parser_transfer_bridge_metadata_and_duplicate_endpoints);
    RUN_TEST(test_parser_transfer_requires_selected_nonzero_type_proof);
    return UNITY_END();
}
