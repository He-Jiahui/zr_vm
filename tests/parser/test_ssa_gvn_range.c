#include "zr_vm_parser/exec_ir_alias.h"

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

int main(void) {
    test_identical_locations_must_alias();
    test_distinct_stable_allocations_are_disjoint();
    test_unknown_external_alias_is_conservative();
    test_escaped_allocations_are_not_proven_disjoint();
    test_disjoint_field_projection_requires_layout_proof();
    return 0;
}
