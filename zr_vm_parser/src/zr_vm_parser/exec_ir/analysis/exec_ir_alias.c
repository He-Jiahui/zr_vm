#include "zr_vm_parser/exec_ir_alias.h"

#include <stddef.h>

static TZrBool alias_base_kind_valid(EZrExecIrAliasBaseKind kind) {
    return (TZrBool)(kind >= ZR_EXEC_IR_ALIAS_BASE_UNKNOWN &&
                     kind <= ZR_EXEC_IR_ALIAS_BASE_EXTERNAL);
}

static TZrBool alias_base_is_external(EZrExecIrAliasBaseKind kind) {
    return (TZrBool)(kind == ZR_EXEC_IR_ALIAS_BASE_EXTERNAL ||
                     kind == ZR_EXEC_IR_ALIAS_BASE_PARAMETER);
}

static TZrBool alias_generation_conflicts(const SZrExecIrAliasLocation *left,
                                          const SZrExecIrAliasLocation *right) {
    return (TZrBool)(left->generation != 0u && right->generation != 0u &&
                     left->generation != right->generation);
}

EZrExecIrAliasRelation ZrParser_ExecIr_AliasQuery(
        const SZrExecIrAliasLocation *left,
        const SZrExecIrAliasLocation *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !alias_base_kind_valid(left->baseKind) ||
        !alias_base_kind_valid(right->baseKind) ||
        left->baseKind == ZR_EXEC_IR_ALIAS_BASE_UNKNOWN ||
        right->baseKind == ZR_EXEC_IR_ALIAS_BASE_UNKNOWN ||
        left->baseId == 0u || right->baseId == 0u) {
        return ZR_EXEC_IR_ALIAS_UNKNOWN;
    }
    /* A generation mismatch means that a previously valid shape/layout
     * identity is stale.  Never turn stale evidence into a disjoint proof. */
    if (alias_generation_conflicts(left, right)) {
        return ZR_EXEC_IR_ALIAS_UNKNOWN;
    }
    if (left->unknownWrite || right->unknownWrite) {
        return ZR_EXEC_IR_ALIAS_UNKNOWN;
    }
    if ((left->escaped && alias_base_is_external(left->baseKind)) ||
        (right->escaped && alias_base_is_external(right->baseKind))) {
        return ZR_EXEC_IR_ALIAS_UNKNOWN;
    }

    if (left->baseKind == right->baseKind && left->baseId == right->baseId &&
        left->hasStableBase && right->hasStableBase) {
        if (left->projectionId == right->projectionId) {
            return ZR_EXEC_IR_ALIAS_MUST_ALIAS;
        }
        if (left->projectionId != 0u && right->projectionId != 0u &&
            left->projectionDisjoint && right->projectionDisjoint &&
            left->layoutId != 0u && left->layoutId == right->layoutId) {
            return ZR_EXEC_IR_ALIAS_DISJOINT;
        }
        return ZR_EXEC_IR_ALIAS_MAY_ALIAS;
    }

    /* Distinct, non-escaped allocation/stack bases are the only bases for
     * which identity itself is a disjointness proof.  Parameters and external
     * handles can be aliases supplied by the caller/FFI. */
    if (left->hasStableBase && right->hasStableBase &&
        !left->escaped && !right->escaped &&
        (left->baseKind == ZR_EXEC_IR_ALIAS_BASE_ALLOCATION ||
         left->baseKind == ZR_EXEC_IR_ALIAS_BASE_STACK) &&
        (right->baseKind == ZR_EXEC_IR_ALIAS_BASE_ALLOCATION ||
         right->baseKind == ZR_EXEC_IR_ALIAS_BASE_STACK)) {
        return ZR_EXEC_IR_ALIAS_DISJOINT;
    }
    return (EZrExecIrAliasRelation)(alias_base_is_external(left->baseKind) ||
                                    alias_base_is_external(right->baseKind)
                                    ? ZR_EXEC_IR_ALIAS_UNKNOWN
                                    : ZR_EXEC_IR_ALIAS_MAY_ALIAS);
}

const TZrChar *ZrParser_ExecIr_AliasRelationName(
        EZrExecIrAliasRelation relation) {
    switch (relation) {
        case ZR_EXEC_IR_ALIAS_DISJOINT:
            return "disjoint";
        case ZR_EXEC_IR_ALIAS_MAY_ALIAS:
            return "may-alias";
        case ZR_EXEC_IR_ALIAS_MUST_ALIAS:
            return "must-alias";
        case ZR_EXEC_IR_ALIAS_UNKNOWN:
        default:
            return "unknown";
    }
}
