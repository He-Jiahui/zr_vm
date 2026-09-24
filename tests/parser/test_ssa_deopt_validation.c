#include "unity.h"

#include <stdlib.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_state_maps.h"

static SZrExecIrFunction function;
static SZrExecIrDiagnostic diagnostic;

void setUp(void) {
    SZrExecIrInstruction instruction = {0};
    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 99u;
    function.signatureHash = 123u;
    function.contract.generation = 7u;
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_ExecIr_FunctionAddExternalValue(
            &function, 1u, ZR_EXEC_IR_OWNERSHIP_GC,
            ZR_EXEC_IR_NULLABILITY_NULLABLE));
    instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_GC;
    instruction.sourceId = 42u;
    instruction.deoptId = 91u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, NULL));
    function.deoptStates = (SZrExecIrDeoptState *)calloc(
            1u, sizeof(*function.deoptStates));
    function.deoptValues = (TZrExecIrValueId *)calloc(
            2u, sizeof(*function.deoptValues));
    TEST_ASSERT_NOT_NULL(function.deoptStates);
    TEST_ASSERT_NOT_NULL(function.deoptValues);
    function.deoptStateCount = function.deoptStateCapacity = 1u;
    function.deoptValueCount = 1u;
    function.deoptValueCapacity = 2u;
    function.deoptValues[0] = function.deoptValues[1] = 1u;
    function.deoptStates[0].id = 91u;
    function.deoptStates[0].sourceId = 42u;
    function.deoptStates[0].resumeId = 701u;
    function.deoptStates[0].valueRange.count = 1u;
}

void tearDown(void) {
    ZrCore_ExecIr_FreeFunction(&function);
}

static void assert_rejected(EZrExecutionDiagnosticCode code,
                            TZrExecIrInstructionId instructionId) {
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_EQUAL(code, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(99u, diagnostic.functionToken);
    TEST_ASSERT_EQUAL_UINT32(42u, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_UINT32(instructionId, diagnostic.instructionId);
}

static void test_deopt_accepts_live_range_and_empty_range_at_end(void) {
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    function.deoptStates[0].valueRange.start = function.deoptValueCount;
    function.deoptStates[0].valueRange.count = 0u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
}

static void test_deopt_rejects_range_into_unused_capacity(void) {
    function.deoptStates[0].valueRange.count = 2u;
    assert_rejected(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 1u);
}

static void test_deopt_rejects_wrapped_range(void) {
    function.deoptStates[0].valueRange.start = UINT32_MAX;
    function.deoptStates[0].valueRange.count = 2u;
    assert_rejected(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 1u);
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_STRUCTURE, NULL));
}

static void test_deopt_rejects_empty_range_past_end(void) {
    function.deoptStates[0].valueRange.start = 2u;
    function.deoptStates[0].valueRange.count = 0u;
    assert_rejected(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 1u);
}

static void test_deopt_rejects_invalid_reconstruction_values(void) {
    function.deoptValueCount = 2u;
    function.deoptStates[0].valueRange.start = 1u;
    function.deoptValues[1] = ZR_EXEC_IR_VALUE_ID_INVALID;
    assert_rejected(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 1u);
    function.deoptValues[1] = function.valueCount + 1u;
    assert_rejected(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 1u);
    TEST_ASSERT_EQUAL_UINT32(function.valueCount, diagnostic.expectedVersion);
    TEST_ASSERT_EQUAL_UINT32(function.valueCount + 1u, diagnostic.actualVersion);
}

static void test_deopt_validates_unreferenced_state(void) {
    function.instructions[0].deoptId = 0u;
    function.deoptStates[0].valueRange.count = 2u;
    assert_rejected(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
    function.deoptStates[0].valueRange.count = 1u;
    function.deoptValues[0] = ZR_EXEC_IR_VALUE_ID_INVALID;
    assert_rejected(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0u);
}

static void test_deopt_builder_keeps_published_map_on_invalid_range(void) {
    SZrExecIrStateMap *published;
    SZrExecIrStateMapEntry *entries;
    TZrUInt32 entryCount;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    published = function.stateMap;
    TEST_ASSERT_NOT_NULL(published);
    entries = published->entries;
    entryCount = published->entryCount;
    /* Physically allocated padding keeps the old implementation's failure
     * deterministic without relying on a heap crash to catch this bug. */
    function.deoptStates[0].valueRange.start = 1u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(42u, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_PTR(published, function.stateMap);
    TEST_ASSERT_EQUAL_PTR(entries, function.stateMap->entries);
    TEST_ASSERT_EQUAL_UINT32(entryCount, function.stateMap->entryCount);
    TEST_ASSERT_EQUAL_UINT32(701u, function.stateMap->entries[0].resumeId);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_deopt_accepts_live_range_and_empty_range_at_end);
    RUN_TEST(test_deopt_rejects_range_into_unused_capacity);
    RUN_TEST(test_deopt_rejects_wrapped_range);
    RUN_TEST(test_deopt_rejects_empty_range_past_end);
    RUN_TEST(test_deopt_rejects_invalid_reconstruction_values);
    RUN_TEST(test_deopt_validates_unreferenced_state);
    RUN_TEST(test_deopt_builder_keeps_published_map_on_invalid_range);
    return UNITY_END();
}
