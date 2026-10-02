#include "zr_vm_parser/exec_ir_alias.h"
#include "zr_vm_parser/exec_ir_ranges.h"
#include "zr_vm_parser/exec_ir_gvn.h"

#include <assert.h>
#include <stdio.h>
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

static void test_zero_generation_cannot_prove_alias_relation(void) {
    SZrExecIrAliasLocation left[3];
    SZrExecIrAliasLocation right[3];
    const char *caseName[3] = {"same projection", "distinct allocation",
                               "disjoint projection"};
    TZrUInt32 caseIndex;
    TZrUInt32 zeroMask;
    TZrUInt32 unsafeProofs = 0u;

    left[0] = location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 7u, 3u);
    right[0] = left[0];
    left[1] = location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 7u, 0u);
    right[1] = location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 8u, 0u);
    left[2] = location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 7u, 3u);
    right[2] = location(ZR_EXEC_IR_ALIAS_BASE_ALLOCATION, 7u, 4u);
    left[2].projectionDisjoint = ZR_TRUE;
    right[2].projectionDisjoint = ZR_TRUE;

    for (caseIndex = 0u; caseIndex < 3u; ++caseIndex) {
        for (zeroMask = 1u; zeroMask <= 3u; ++zeroMask) {
            SZrExecIrAliasLocation queryLeft = left[caseIndex];
            SZrExecIrAliasLocation queryRight = right[caseIndex];
            EZrExecIrAliasRelation relation;
            if ((zeroMask & 1u) != 0u) queryLeft.generation = 0u;
            if ((zeroMask & 2u) != 0u) queryRight.generation = 0u;
            relation = ZrParser_ExecIr_AliasQuery(&queryLeft, &queryRight);
            if (relation != ZR_EXEC_IR_ALIAS_UNKNOWN) {
                fprintf(stderr, "%s, zero mask %u: %s\n", caseName[caseIndex],
                        (unsigned int)zeroMask,
                        ZrParser_ExecIr_AliasRelationName(relation));
                ++unsafeProofs;
            }
        }
    }
    assert(unsafeProofs == 0u);
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
    length.lengthMutable = ZR_TRUE;
    assert(!ZrParser_ExecIr_RangeProvesBounds(&index, &length));
    length.lengthMutable = ZR_FALSE;
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
    length.lengthMutable = ZR_TRUE;
    assert(!ZrParser_ExecIr_CanElideBoundsCheck(&index, &length, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE);
    length.lengthMutable = ZR_FALSE;
    index.upper = 4;
    assert(!ZrParser_ExecIr_CanElideBoundsCheck(&index, &length, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE);
}

static void test_direct_bounds_proof_rejects_inverted_intervals(void) {
    SZrExecIrRangeFact index = {1u, 0, 2, 1u, ZR_TRUE, ZR_TRUE,
                                ZR_FALSE, ZR_FALSE};
    SZrExecIrRangeFact length = {2u, 3, 3, 1u, ZR_TRUE, ZR_TRUE,
                                 ZR_FALSE, ZR_FALSE};
    SZrExecIrDiagnostic diagnostic;
    index.lower = 4;
    assert(!ZrParser_ExecIr_RangeProvesBounds(&index, &length));
    assert(!ZrParser_ExecIr_CanElideBoundsCheck(&index, &length, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE);
    index.lower = 0;
    length.upper = 1;
    assert(!ZrParser_ExecIr_RangeProvesBounds(&index, &length));
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

static void test_gvn_reuses_definition_from_dominating_block(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId left, right, first, repeated;
    SZrExecIrRange operands, result, successors, returned;
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id;
    TZrExecIrBlockId entry, child;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 77u;
    entry = ZrCore_ExecIr_FunctionAddBlock(&function,
                                            ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    child = ZrCore_ExecIr_FunctionAddBlock(&function, 0u);
    function.entryBlockId = entry;
    left = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    right = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    first = ZrCore_ExecIr_FunctionAddValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    repeated = ZrCore_ExecIr_FunctionAddValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(left != 0u && right != 0u && first != 0u && repeated != 0u);
    {
        TZrExecIrValueId addOperands[2] = {left, right};
        assert(ZrCore_ExecIr_FunctionAppendOperands(
                &function, addOperands, 2u, &operands));
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD;
    instruction.operandRange = operands;
    instruction.typeToken = ZR_VALUE_TYPE_INT64;
    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &first, 1u, &result));
    instruction.resultRange = result;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(
            &function, &child, 1u, &successors));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_BRANCH;
    instruction.successorRange = successors;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &repeated, 1u, &result));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD;
    instruction.operandRange = operands;
    instruction.resultRange = result;
    instruction.typeToken = ZR_VALUE_TYPE_INT64;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &repeated, 1u, &returned));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operandRange = returned;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));
    function.blocks[entry - 1u].instructionRange.start = 0u;
    function.blocks[entry - 1u].instructionRange.count = 2u;
    function.blocks[entry - 1u].successorRange = successors;
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(
            &function, &entry, 1u,
            &function.blocks[child - 1u].predecessorRange));
    function.blocks[child - 1u].instructionRange.start = 2u;
    function.blocks[child - 1u].instructionRange.count = 2u;
    assert(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic));
    assert(function.blocks[child - 1u].immediateDominator == entry);
    assert(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    assert(ZrParser_ExecIr_RunGvnCse(
            &function, ZR_NULL, ZR_NULL, &diagnostic));
    assert(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_COPY);
    assert(function.operands[function.instructions[2].operandRange.start] == first);
    assert(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_gvn_does_not_reuse_definition_from_sibling_block(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId condition, leftOperand, rightOperand;
    TZrExecIrValueId leftResult, rightResult, joinResult;
    SZrExecIrRange conditionOperands, operands, result, returnOperands;
    SZrExecIrInstruction instruction;
    TZrExecIrBlockId entry, left, right, join;
    TZrExecIrBlockId entrySuccessors[2];
    TZrExecIrBlockId joinPredecessors[2];
    TZrExecIrInstructionId id;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 78u;
    entry = ZrCore_ExecIr_FunctionAddBlock(
            &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    left = ZrCore_ExecIr_FunctionAddBlock(&function, 0u);
    right = ZrCore_ExecIr_FunctionAddBlock(&function, 0u);
    join = ZrCore_ExecIr_FunctionAddBlock(&function, 0u);
    function.entryBlockId = entry;
    condition = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, ZR_VALUE_TYPE_BOOL, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    leftOperand = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    rightOperand = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    leftResult = ZrCore_ExecIr_FunctionAddValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    rightResult = ZrCore_ExecIr_FunctionAddValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    joinResult = ZrCore_ExecIr_FunctionAddValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(entry == 1u && left == 2u && right == 3u && join == 4u);
    assert(condition != ZR_EXEC_IR_VALUE_ID_INVALID &&
           leftOperand != ZR_EXEC_IR_VALUE_ID_INVALID &&
           rightOperand != ZR_EXEC_IR_VALUE_ID_INVALID &&
           leftResult != ZR_EXEC_IR_VALUE_ID_INVALID &&
           rightResult != ZR_EXEC_IR_VALUE_ID_INVALID &&
           joinResult != ZR_EXEC_IR_VALUE_ID_INVALID);

    entrySuccessors[0] = left;
    entrySuccessors[1] = right;
    joinPredecessors[0] = left;
    joinPredecessors[1] = right;
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(
            &function, entrySuccessors, 2u,
            &function.blocks[entry - 1u].successorRange));
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(
            &function, &join, 1u,
            &function.blocks[left - 1u].successorRange));
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(
            &function, &join, 1u,
            &function.blocks[right - 1u].successorRange));
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(
            &function, &entry, 1u,
            &function.blocks[left - 1u].predecessorRange));
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(
            &function, &entry, 1u,
            &function.blocks[right - 1u].predecessorRange));
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(
            &function, joinPredecessors, 2u,
            &function.blocks[join - 1u].predecessorRange));

    assert(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &condition, 1u, &conditionOperands));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH;
    instruction.operandRange = conditionOperands;
    instruction.successorRange = function.blocks[entry - 1u].successorRange;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));

    {
        TZrExecIrValueId addOperands[2] = {leftOperand, rightOperand};
        assert(ZrCore_ExecIr_FunctionAppendOperands(
                &function, addOperands, 2u, &operands));
    }
    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &leftResult, 1u, &result));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD;
    instruction.operandRange = operands;
    instruction.resultRange = result;
    instruction.typeToken = ZR_VALUE_TYPE_INT64;
    instruction.layoutId = 9u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_BRANCH;
    instruction.successorRange = function.blocks[left - 1u].successorRange;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));

    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &rightResult, 1u, &result));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD;
    instruction.operandRange = operands;
    instruction.resultRange = result;
    instruction.typeToken = ZR_VALUE_TYPE_INT64;
    instruction.layoutId = 9u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_BRANCH;
    instruction.successorRange = function.blocks[right - 1u].successorRange;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));

    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &joinResult, 1u, &result));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD;
    instruction.operandRange = operands;
    instruction.resultRange = result;
    instruction.typeToken = ZR_VALUE_TYPE_INT64;
    instruction.layoutId = 9u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &joinResult, 1u, &returnOperands));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operandRange = returnOperands;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));

    function.blocks[entry - 1u].instructionRange.start = 0u;
    function.blocks[entry - 1u].instructionRange.count = 1u;
    function.blocks[left - 1u].instructionRange.start = 1u;
    function.blocks[left - 1u].instructionRange.count = 2u;
    function.blocks[right - 1u].instructionRange.start = 3u;
    function.blocks[right - 1u].instructionRange.count = 2u;
    function.blocks[join - 1u].instructionRange.start = 5u;
    function.blocks[join - 1u].instructionRange.count = 2u;

    assert(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic));
    assert(function.blocks[left - 1u].immediateDominator == entry);
    assert(function.blocks[right - 1u].immediateDominator == entry);
    assert(function.blocks[join - 1u].immediateDominator == entry);
    assert(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    assert(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_ADD);
    assert(function.instructions[3].opcode == ZR_EXEC_IR_OPCODE_ADD);
    assert(function.instructions[5].opcode == ZR_EXEC_IR_OPCODE_ADD);
    assert(ZrParser_ExecIr_RunGvnCse(
            &function, ZR_NULL, ZR_NULL, &diagnostic));
    assert(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_ADD);
    assert(function.instructions[3].opcode == ZR_EXEC_IR_OPCODE_ADD);
    assert(function.instructions[5].opcode == ZR_EXEC_IR_OPCODE_ADD);
    assert(function.resultPool[function.instructions[1].resultRange.start] ==
           leftResult);
    assert(function.resultPool[function.instructions[3].resultRange.start] ==
           rightResult);
    assert(function.resultPool[function.instructions[5].resultRange.start] ==
           joinResult);
    assert(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
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

static void test_gvn_preserves_conversion_result_type_when_instruction_type_is_implicit(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId source, integer, floating, repeated;
    SZrExecIrRange operands, result, returned;
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id;
    TZrExecIrBlockId block;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 1u;
    block = ZrCore_ExecIr_FunctionAddBlock(
            &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    function.entryBlockId = block;
    source = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    integer = ZrCore_ExecIr_FunctionAddValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    floating = ZrCore_ExecIr_FunctionAddValue(
            &function, ZR_VALUE_TYPE_DOUBLE, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    repeated = ZrCore_ExecIr_FunctionAddValue(
            &function, ZR_VALUE_TYPE_DOUBLE, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(source != 0u && integer != 0u && floating != 0u && repeated != 0u);
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &source, 1u, &operands));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CONVERT;
    instruction.operands = operands;
    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &integer, 1u, &result));
    instruction.results = result;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &floating, 1u, &result));
    instruction.results = result;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &repeated, 1u, &result));
    instruction.results = result;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &repeated, 1u, &returned));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returned;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    function.blocks[0].instructionRange.start = 0u;
    function.blocks[0].instructionRange.count = function.instructionCount;
    assert(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    assert(ZrParser_ExecIr_RunGvnCse(
            &function, ZR_NULL, ZR_NULL, &diagnostic));
    assert(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_CONVERT);
    assert(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_COPY);
    assert(function.operands[function.instructions[2].operands.start] == floating);
    assert(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_gvn_rejects_missing_value_storage(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 originalCount;
    ZrCore_ExecIr_FunctionInit(&function);
    function.valueCount = function.valueCapacity = 1u;
    assert(!ZrParser_ExecIr_RunGvnCse(
            &function, ZR_NULL, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    function.valueCount = function.valueCapacity = 0u;
    assert(ZrCore_ExecIr_FunctionAddValue(
            &function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN) != 0u);
    originalCount = function.valueCount;
    function.valueCount = function.valueCapacity + 1u;
    assert(!ZrParser_ExecIr_RunGvnCse(
            &function, ZR_NULL, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    function.valueCount = originalCount;
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_identical_locations_must_alias();
    test_distinct_stable_allocations_are_disjoint();
    test_unknown_external_alias_is_conservative();
    test_escaped_allocations_are_not_proven_disjoint();
    test_disjoint_field_projection_requires_layout_proof();
    test_zero_generation_cannot_prove_alias_relation();
    test_range_facts_require_both_bounds();
    test_range_facts_reject_overflow_and_stale_generation();
    test_shape_fact_invalidates_on_generation_change();
    test_nullability_fact_is_generation_scoped();
    test_bounds_check_api_is_conservative();
    test_direct_bounds_proof_rejects_inverted_intervals();
    test_gvn_rewrites_only_duplicate_pure_definitions();
    test_gvn_reuses_definition_from_dominating_block();
    test_gvn_does_not_reuse_definition_from_sibling_block();
    test_gvn_keys_type_tests_by_canonical_match_type();
    test_gvn_preserves_conversion_result_type_when_instruction_type_is_implicit();
    test_gvn_rejects_missing_value_storage();
    return 0;
}
