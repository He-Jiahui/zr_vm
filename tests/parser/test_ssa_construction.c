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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ssa_construction_rejects_missing_semantic_facts);
    RUN_TEST(test_ssa_construction_empty_semir_is_safe);
    RUN_TEST(test_ssa_construction_dominator_linear_cfg);
    return UNITY_END();
}
