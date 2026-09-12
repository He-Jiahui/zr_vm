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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ssa_construction_rejects_missing_semantic_facts);
    RUN_TEST(test_ssa_construction_empty_semir_is_safe);
    return UNITY_END();
}
