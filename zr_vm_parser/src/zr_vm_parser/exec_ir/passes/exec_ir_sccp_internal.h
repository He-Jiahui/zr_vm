#ifndef ZR_EXEC_IR_SCCP_INTERNAL_H
#define ZR_EXEC_IR_SCCP_INTERNAL_H

#include "exec_ir/exec_ir_pass_internal.h"
#include "zr_vm_common/zr_type_conf.h"
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/* Private facts shared by legacy analysis, typed owner analysis and rewrite.
 * Owner CONSTANTs have exact canonical type; strictInt64 is never inferred
 * from legacy numeric bits or the public cache. */
typedef struct SZrSccpLattice {
    EZrExecIrSccpLatticeKind kind;
    TZrUInt64 bits;
    TZrExecIrTypeToken typeToken;
    /* Type annotations can differ from the runner's actual value kind. */
    TZrBool signedRepresentation;
    TZrBool strictInt64;
} SZrSccpLattice;

static inline TZrBool zr_sccp_size_mul_overflow(TZrUInt64 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     count > (TZrUInt64)(SIZE_MAX / elementSize));
}

static inline TZrBool zr_sccp_add(TZrInt64 left, TZrInt64 right, TZrInt64 *out) {
    if ((right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) return ZR_FALSE;
    *out = left + right;
    return ZR_TRUE;
}

static inline TZrBool zr_sccp_sub(TZrInt64 left, TZrInt64 right, TZrInt64 *out) {
    if ((right < 0 && left > INT64_MAX + right) ||
        (right > 0 && left < INT64_MIN + right)) return ZR_FALSE;
    *out = left - right;
    return ZR_TRUE;
}

static inline TZrBool zr_sccp_mul(TZrInt64 left, TZrInt64 right, TZrInt64 *out) {
    if (left != 0 && right != 0 &&
        ((left == INT64_MIN && right == -1) ||
         (right == INT64_MIN && left == -1) ||
         (left > 0 && right > 0 && left > INT64_MAX / right) ||
         (left > 0 && right < 0 && right < INT64_MIN / left) ||
         (left < 0 && right > 0 && left < INT64_MIN / right) ||
         (left < 0 && right < 0 && left < INT64_MAX / right))) return ZR_FALSE;
    *out = left * right;
    return ZR_TRUE;
}

static inline TZrBool zr_sccp_div(TZrInt64 left, TZrInt64 right, TZrInt64 *out) {
    if (right == 0 || (left == INT64_MIN && right == -1)) return ZR_FALSE;
    *out = left / right;
    return ZR_TRUE;
}

/* TRUE with complete=FALSE is conservative interruption, not a proof.
 * The caller owns scratch arrays and must skip cache/rewrite in that case. */
TZrBool ZrParser_ExecIr_ComputeOwnerSccp(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, SZrSccpLattice *lattice,
        TZrUInt8 *blocks, TZrUInt32 *edges, TZrBool *complete,
        SZrExecIrDiagnostic *diagnostic);

#endif
