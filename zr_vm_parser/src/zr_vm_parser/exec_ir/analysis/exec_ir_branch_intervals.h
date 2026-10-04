#ifndef ZR_VM_PARSER_EXEC_IR_BRANCH_INTERVALS_H
#define ZR_VM_PARSER_EXEC_IR_BRANCH_INTERVALS_H

#include "zr_vm_parser/exec_ir_branch_facts.h"
#include <stdint.h>

/* Private checked endpoint math. The bounds on the right are representable
 * in the sign-guarded branches; never evaluate a possibly overflowing result. */
static TZrBool branch_interval_add(TZrInt64 a, TZrInt64 b, TZrInt64 *out) {
    if ((b > 0 && a > INT64_MAX - b) ||
        (b < 0 && a < INT64_MIN - b)) return ZR_FALSE;
    *out = a + b; return ZR_TRUE;
}
static TZrBool branch_interval_sub(TZrInt64 a, TZrInt64 b, TZrInt64 *out) {
    if ((b < 0 && a > INT64_MAX + b) ||
        (b > 0 && a < INT64_MIN + b)) return ZR_FALSE;
    *out = a - b; return ZR_TRUE;
}
static TZrBool branch_interval_complete(const SZrExecIrBranchValueFact *a) {
    return (TZrBool)(a->available && a->signedInteger && !a->overflowed &&
            a->hasLower && a->hasUpper && a->lower <= a->upper);
}

/* out starts as an available, independently witnessed result with no bounds.
 * Missing proof is UNKNOWN. Known overflow possibility is sticky poison, not
 * an unreachable edge and not an execution of the checked operation. */
static void branch_interval_arithmetic(EZrExecIrOpcode opcode,
        const SZrExecIrBranchValueFact *left,
        const SZrExecIrBranchValueFact *right, SZrExecIrBranchValueFact *out) {
    TZrInt64 lower, upper;
    TZrBool safe;
    if (!out->signedInteger || !left->available || !right->available ||
        !left->signedInteger || !right->signedInteger) return;
    if (left->overflowed || right->overflowed) {
        out->overflowed = ZR_TRUE; return;
    }
    if (!branch_interval_complete(left) || !branch_interval_complete(right)) return;
    if (opcode == ZR_EXEC_IR_OPCODE_ADD) {
        safe = (TZrBool)(branch_interval_add(left->lower, right->lower, &lower) &&
                        branch_interval_add(left->upper, right->upper, &upper));
    } else {
        safe = (TZrBool)(branch_interval_sub(left->lower, right->upper, &lower) &&
                        branch_interval_sub(left->upper, right->lower, &upper));
    }
    if (!safe) { out->overflowed = ZR_TRUE; return; }
    out->hasLower = out->hasUpper = ZR_TRUE;
    out->lower = lower; out->upper = upper;
}

static void branch_interval_lower(SZrExecIrBranchValueFact *a, TZrInt64 bound) {
    if (!a->hasLower || bound > a->lower) a->lower = bound;
    a->hasLower = ZR_TRUE;
}
static void branch_interval_upper(SZrExecIrBranchValueFact *a, TZrInt64 bound) {
    if (!a->hasUpper || bound < a->upper) a->upper = bound;
    a->hasUpper = ZR_TRUE;
}
static TZrBool branch_interval_nonempty(const SZrExecIrBranchValueFact *a) {
    return (TZrBool)(!a->hasLower || !a->hasUpper || a->lower <= a->upper);
}

/* Both projections use original snapshots. This interval domain records only
 * endpoint constraints; it does not retain an A<B relation or interior holes.
 * Caller checks domains/effects and handles unequal singleton exclusions. */
static TZrBool branch_interval_relation(SZrExecIrBranchValueFact *left,
        SZrExecIrBranchValueFact *right, TZrUInt32 predicate) {
    SZrExecIrBranchValueFact a = *left, b = *right;
    TZrBool strict;
    if (predicate == 0u) {
        if (a.hasLower) branch_interval_lower(right, a.lower);
        if (b.hasLower) branch_interval_lower(left, b.lower);
        if (a.hasUpper) branch_interval_upper(right, a.upper);
        if (b.hasUpper) branch_interval_upper(left, b.upper);
        return (TZrBool)(branch_interval_nonempty(left) && branch_interval_nonempty(right));
    }
    if (predicate == 3u || predicate == 4u) {
        SZrExecIrBranchValueFact *swap = left;
        left = right; right = swap;
        a = *left; b = *right;
        predicate = predicate == 3u ? 1u : 2u;
    }
    strict = (TZrBool)(predicate == 1u);
    if (b.hasUpper) {
        if (strict && b.upper == INT64_MIN) return ZR_FALSE;
        branch_interval_upper(left, strict ? b.upper - 1 : b.upper);
    }
    if (a.hasLower) {
        if (strict && a.lower == INT64_MAX) return ZR_FALSE;
        branch_interval_lower(right, strict ? a.lower + 1 : a.lower);
    }
    return (TZrBool)(branch_interval_nonempty(left) && branch_interval_nonempty(right));
}

#endif /* ZR_VM_PARSER_EXEC_IR_BRANCH_INTERVALS_H */
