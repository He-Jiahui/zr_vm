#include "zr_vm_parser/exec_ir_alias.h"
#include "zr_vm_parser/exec_ir_ranges.h"

#include <assert.h>
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
    return 0;
}
