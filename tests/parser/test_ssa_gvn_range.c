#include "zr_vm_parser/exec_ir_alias.h"
#include "zr_vm_parser/exec_ir_ranges.h"
#include "zr_vm_parser/exec_ir_gvn.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static SZrExecIrAliasLocation location(EZrExecIrAliasBaseKind kind,
                                       TZrUInt64 baseId,
                                       TZrUInt64 projectionId) {
    SZrExecIrAliasLocation value;
    memset(&value, 0, sizeof(value));
    value.baseKind = kind;
    value.baseId = baseId;
    value.projectionId = projectionId;
    value.layoutId = 1u;
    value.generation = 1u;
    value.hasStableBase = ZR_TRUE;
    return value;
}

static void test_identical_locations_must_alias(void) {
    const SZrExecIrAliasLocation left =
            location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 7u, 3u);
    const SZrExecIrAliasLocation right = left;
    assert(ZrParser_ExecIr_AliasQuery(&left, &right) ==
           ZR_EXEC_IR_ALIAS_MUST_ALIAS);
}

static void test_distinct_stable_allocations_are_disjoint(void) {
    const SZrExecIrAliasLocation left =
            location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 7u, 0u);
    const SZrExecIrAliasLocation right =
            location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 8u, 0u);
    assert(ZrParser_ExecIr_AliasQuery(&left, &right) ==
           ZR_EXEC_IR_ALIAS_DISJOINT);
}

static void test_unknown_external_alias_is_conservative(void) {
    SZrExecIrAliasLocation left =
            location(ZR_EXEC_IR_ALIAS_BASE_PARAMETER, 1u, 0u);
    SZrExecIrAliasLocation right =
            location(ZR_EXEC_IR_ALIAS_BASE_PARAMETER, 2u, 0u);
    left.hasStableBase = ZR_FALSE;
    right.hasStableBase = ZR_FALSE;
    assert(ZrParser_ExecIr_AliasQuery(&left, &right) ==
           ZR_EXEC_IR_ALIAS_UNKNOWN);
}

static void test_escaped_allocations_are_not_proven_disjoint(void) {
    SZrExecIrAliasLocation left =
            location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 7u, 0u);
    SZrExecIrAliasLocation right =
            location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 8u, 0u);
    left.escaped = ZR_TRUE;
    assert(ZrParser_ExecIr_AliasQuery(&left, &right) ==
           ZR_EXEC_IR_ALIAS_MAY_ALIAS);
}

static void test_disjoint_field_projection_requires_layout_proof(void) {
    SZrExecIrAliasLocation left =
            location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 7u, 3u);
    SZrExecIrAliasLocation right =
            location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 7u, 4u);
    left.projectionDisjoint = ZR_TRUE;
    right.projectionDisjoint = ZR_TRUE;
    assert(ZrParser_ExecIr_AliasQuery(&left, &right) ==
           ZR_EXEC_IR_ALIAS_DISJOINT);
}

static void test_range_facts_require_both_bounds(void) {
    SZrExecIrAnalysisFacts facts;
    SZrExecIrRangeFact index = {0};
    SZrExecIrRangeFact length = {0};
    ZrParser_ExecIr_AnalysisFactsInit(&facts);
    index.valueId = 1u;
    index.hasLower = ZR_TRUE;
    index.hasUpper = ZR_TRUE;
    index.lower = 0;
    index.upper = 3;
    index.generation = 1u;
    length.valueId = 2u;
    length.hasLower = ZR_TRUE;
    length.hasUpper = ZR_TRUE;
    length.lower = 4;
    length.upper = 4;
    length.generation = 1u;
    assert(ZrParser_ExecIr_AnalysisFacts_AddRange(&facts, &index));
    assert(ZrParser_ExecIr_AnalysisFacts_AddRange(&facts, &length));
    assert(ZrParser_ExecIr_RangeProvesBounds(&index, &length));
    index.hasLower = ZR_FALSE;
    assert(!ZrParser_ExecIr_RangeProvesBounds(&index, &length));
    ZrParser_ExecIr_AnalysisFactsFree(&facts);
}

static void test_range_facts_reject_overflow_and_stale_generation(void) {
    SZrExecIrAnalysisFacts facts;
    SZrExecIrRangeFact index = {0};
    SZrExecIrRangeFact length = {0};
    ZrParser_ExecIr_AnalysisFactsInit(&facts);
    index.valueId = 1u;
    index.hasLower = index.hasUpper = ZR_TRUE;
    index.lower = 0;
    index.upper = 3;
    index.overflowed = ZR_TRUE;
    index.generation = 1u;
    length.valueId = 2u;
    length.hasLower = length.hasUpper = ZR_TRUE;
    length.lower = length.upper = 4;
    length.generation = 1u;
    assert(!ZrParser_ExecIr_RangeProvesBounds(&index, &length));
    index.overflowed = ZR_FALSE;
    assert(ZrParser_ExecIr_RangeProvesBounds(&index, &length));
    ZrParser_ExecIr_AnalysisFacts_InvalidateGeneration(&facts, 2u);
    assert(!ZrParser_ExecIr_AnalysisFacts_AddRange(&facts, &index));
    ZrParser_ExecIr_AnalysisFactsFree(&facts);
}

static void test_shape_fact_invalidates_on_generation_change(void) {
    SZrExecIrAnalysisFacts facts;
    SZrExecIrShapeFact shape = {0};
    ZrParser_ExecIr_AnalysisFactsInit(&facts);
    shape.valueId = 7u;
    shape.typeToken = 2u;
    shape.layoutId = 3u;
    shape.shapeId = 9u;
    shape.generation = 1u;
    assert(ZrParser_ExecIr_AnalysisFacts_AddShape(&facts, &shape));
    assert(ZrParser_ExecIr_AnalysisFacts_FindShape(&facts, 7u) != ZR_NULL);
    ZrParser_ExecIr_AnalysisFacts_InvalidateGeneration(&facts, 6u);
    assert(ZrParser_ExecIr_AnalysisFacts_FindShape(&facts, 7u) == ZR_NULL);
    ZrParser_ExecIr_AnalysisFactsFree(&facts);
}

static void test_nullability_fact_is_generation_scoped(void) {
    SZrExecIrAnalysisFacts facts;
    SZrExecIrNullabilityFact nullability = {1u, ZR_EXEC_IR_NULL_FACT_NONNULL, 1u};
    ZrParser_ExecIr_AnalysisFactsInit(&facts);
    assert(ZrParser_ExecIr_AnalysisFacts_AddNullability(&facts, &nullability));
    assert(ZrParser_ExecIr_AnalysisFacts_FindNullability(&facts, 1u) != ZR_NULL);
    ZrParser_ExecIr_AnalysisFacts_InvalidateGeneration(&facts, 2u);
    assert(ZrParser_ExecIr_AnalysisFacts_FindNullability(&facts, 1u) == ZR_NULL);
    ZrParser_ExecIr_AnalysisFactsFree(&facts);
}

static void test_bounds_check_api_is_conservative(void) {
    SZrExecIrRangeFact index = {1u, 0, 2, 1u, ZR_TRUE, ZR_TRUE,
                                ZR_FALSE, ZR_FALSE};
    SZrExecIrRangeFact length = {2u, 3, 3, 1u, ZR_TRUE, ZR_TRUE,
                                 ZR_FALSE, ZR_FALSE};
    SZrExecIrDiagnostic diagnostic;
    assert(ZrParser_ExecIr_CanElideBoundsCheck(&index, &length, &diagnostic));
    index.upper = 4;
    assert(!ZrParser_ExecIr_CanElideBoundsCheck(&index, &length, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE);
}

static void test_gvn_rewrites_only_duplicate_pure_definitions(void) {
    SZrExecIrFunction function;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId values[4];
    SZrExecIrRange leftRange, rightRange, addOperands, resultRange;
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id;
    TZrExecIrBlockId block;
    ZrCore_ExecIr_FunctionInit(&function);
    block = ZrCore_ExecIr_FunctionAddBlock(&function,
                                            ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    function.entryBlockId = block;
    values[0] = ZrCore_ExecIr_FunctionAddValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    values[1] = ZrCore_ExecIr_FunctionAddValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    values[2] = ZrCore_ExecIr_FunctionAddValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    values[3] = ZrCore_ExecIr_FunctionAddValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(values[3] != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(&function, &values[0], 1u,
                                               &leftRange));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.resultRange = leftRange;
    instruction.layoutId = 1u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendResults(&function, &values[1], 1u,
                                               &rightRange));
    instruction.resultRange = rightRange;
    instruction.layoutId = 2u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendOperands(&function, values, 2u,
                                                &addOperands));
    assert(ZrCore_ExecIr_FunctionAppendResults(&function, &values[2], 1u,
                                               &resultRange));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD;
    instruction.operandRange = addOperands;
    instruction.resultRange = resultRange;
    instruction.typeToken = 1u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendResults(&function, &values[3], 1u,
                                               &resultRange));
    instruction.resultRange = resultRange;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));
    function.blocks[0].instructionRange.start = 0u;
    function.blocks[0].instructionRange.count = function.instructionCount;
    memset(&remarks, 0, sizeof(remarks));
    assert(ZrParser_ExecIr_RunGvnCse(&function, ZR_NULL, &remarks, &diagnostic));
    assert(function.instructions[3].opcode == ZR_EXEC_IR_OPCODE_COPY);
    assert(function.resultPool[function.instructions[3].resultRange.start] == values[3]);
    assert(remarks.count >= 1u && remarks.items[remarks.count - 1u].sourceId == 0u);
    free(remarks.items);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_gvn_keys_type_tests_by_canonical_match_type(void) {
    SZrExecIrFunction function;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId values[4];
    SZrExecIrRange sourceResult, operandRange, resultRange, returnOperands;
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id;
    TZrExecIrBlockId block;
    TZrUInt32 index;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 1u;
    block = ZrCore_ExecIr_FunctionAddBlock(
            &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    function.entryBlockId = block;
    for (index = 0u; index < 4u; ++index) {
        values[index] = ZrCore_ExecIr_FunctionAddValue(
                &function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        assert(values[index] != 0u);
    }
    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &values[0], 1u, &sourceResult));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = sourceResult;
    instruction.layoutId = 1u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &values[0], 1u, &operandRange));
    for (index = 1u; index < 4u; ++index) {
        assert(ZrCore_ExecIr_FunctionAppendResults(
                &function, &values[index], 1u, &resultRange));
        memset(&instruction, 0, sizeof(instruction));
        instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_TYPE_TEST;
        instruction.operands = operandRange;
        instruction.results = resultRange;
        instruction.typeToken = 1u;
        instruction.matchTypeToken = index == 2u ? 8u : 7u;
        assert(ZrCore_ExecIr_FunctionAppendInstruction(
                &function, &instruction, &id));
    }
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &values[3], 1u, &returnOperands));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperands;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    function.blocks[0].instructionRange.start = 0u;
    function.blocks[0].instructionRange.count = function.instructionCount;

    memset(&remarks, 0, sizeof(remarks));
    assert(ZrParser_ExecIr_RunGvnCse(
            &function, ZR_NULL, &remarks, &diagnostic));
    assert(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST);
    assert(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST);
    assert(function.instructions[3].opcode == ZR_EXEC_IR_OPCODE_COPY);
    assert(function.instructions[3].matchTypeToken == 0u);
    assert(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    free(remarks.items);
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_identical_locations_must_alias();
    test_distinct_stable_allocations_are_disjoint();
    test_unknown_external_alias_is_conservative();
    test_escaped_allocations_are_not_proven_disjoint();
    test_disjoint_field_projection_requires_layout_proof();
    test_range_facts_require_both_bounds();
    test_range_facts_reject_overflow_and_stale_generation();
    test_shape_fact_invalidates_on_generation_change();
    test_nullability_fact_is_generation_scoped();
    test_bounds_check_api_is_conservative();
    test_gvn_rewrites_only_duplicate_pure_definitions();
    test_gvn_keys_type_tests_by_canonical_match_type();
    return 0;
}
