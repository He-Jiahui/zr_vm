#include "unity.h"
#include <string.h>
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_ssa_construction_rejects_missing_semantic_facts(void) {
    SZrExecIrFunction out; SZrExecIrDiagnostic d;
    ZrCore_ExecIr_FunctionInit(&out);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_Build(NULL, NULL, &out, &d));
    TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, d.code);
    ZrCore_ExecIr_FreeFunction(&out);
}

void test_ssa_construction_empty_semir_is_safe(void) {
    SZrSemanticIrFunction sem; SZrExecIrFunction out; SZrExecIrDiagnostic d;
    memset(&sem, 0, sizeof(sem));
    ZrCore_ExecIr_FunctionInit(&out);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_Build(&sem, NULL, &out, &d));
    ZrCore_ExecIr_FreeFunction(&out);
}

void test_ssa_construction_dominator_linear_cfg(void) {
    SZrExecIrFunction f; SZrExecIrDiagnostic d; TZrExecIrBlockId a, b;
    ZrCore_ExecIr_FunctionInit(&f);
    a = ZrCore_ExecIr_FunctionAddBlock(&f, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    b = ZrCore_ExecIr_FunctionAddBlock(&f, 0u);
    f.entryBlockId = a;
    ZrCore_ExecIr_FunctionAppendSuccessors(&f, &b, 1u, &f.blocks[a - 1u].successorRange);
    ZrCore_ExecIr_FunctionAppendPredecessors(&f, &a, 1u, &f.blocks[b - 1u].predecessorRange);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_ComputeDominators(&f, &d));
    TEST_ASSERT_EQUAL(a, f.blocks[b - 1u].immediateDominator);
    ZrCore_ExecIr_FreeFunction(&f);
}

void test_ssa_construction_dominator_diamond_cfg(void) {
    SZrExecIrFunction f;
    SZrExecIrDiagnostic d;
    TZrExecIrBlockId entry;
    TZrExecIrBlockId left;
    TZrExecIrBlockId right;
    TZrExecIrBlockId merge;
    TZrExecIrBlockId entrySuccessors[2];
    TZrExecIrBlockId mergePredecessors[2];

    ZrCore_ExecIr_FunctionInit(&f);
    entry = ZrCore_ExecIr_FunctionAddBlock(&f, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    left = ZrCore_ExecIr_FunctionAddBlock(&f, 0u);
    right = ZrCore_ExecIr_FunctionAddBlock(&f, 0u);
    merge = ZrCore_ExecIr_FunctionAddBlock(&f, 0u);
    f.entryBlockId = entry;
    entrySuccessors[0] = left;
    entrySuccessors[1] = right;
    mergePredecessors[0] = left;
    mergePredecessors[1] = right;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &f, entrySuccessors, 2u, &f.blocks[entry - 1u].successorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &f, &merge, 1u, &f.blocks[left - 1u].successorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &f, &merge, 1u, &f.blocks[right - 1u].successorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPredecessors(
            &f, &entry, 1u, &f.blocks[left - 1u].predecessorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPredecessors(
            &f, &entry, 1u, &f.blocks[right - 1u].predecessorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPredecessors(
            &f, mergePredecessors, 2u, &f.blocks[merge - 1u].predecessorRange));

    TEST_ASSERT_TRUE(ZrParser_ExecIr_ComputeDominators(&f, &d));
    TEST_ASSERT_EQUAL(entry, f.blocks[left - 1u].immediateDominator);
    TEST_ASSERT_EQUAL(entry, f.blocks[right - 1u].immediateDominator);
    TEST_ASSERT_EQUAL(entry, f.blocks[merge - 1u].immediateDominator);
    ZrCore_ExecIr_FreeFunction(&f);
}

void test_ssa_construction_dominator_rejects_invalid_successor(void) {
    SZrExecIrFunction f;
    SZrExecIrDiagnostic d;
    TZrExecIrBlockId entry;
    TZrExecIrBlockId invalidSuccessor = 2u;

    ZrCore_ExecIr_FunctionInit(&f);
    entry = ZrCore_ExecIr_FunctionAddBlock(&f, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    f.entryBlockId = entry;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &f, &invalidSuccessor, 1u,
            &f.blocks[entry - 1u].successorRange));

    TEST_ASSERT_FALSE(ZrParser_ExecIr_ComputeDominators(&f, &d));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, d.code);
    TEST_ASSERT_EQUAL(entry, d.blockId);
    TEST_ASSERT_EQUAL(1u, d.expectedVersion);
    TEST_ASSERT_EQUAL(invalidSuccessor, d.actualVersion);
    ZrCore_ExecIr_FreeFunction(&f);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ssa_construction_rejects_missing_semantic_facts);
    RUN_TEST(test_ssa_construction_empty_semir_is_safe);
    RUN_TEST(test_ssa_construction_dominator_linear_cfg);
    RUN_TEST(test_ssa_construction_dominator_diamond_cfg);
    RUN_TEST(test_ssa_construction_dominator_rejects_invalid_successor);
    return UNITY_END();
}
