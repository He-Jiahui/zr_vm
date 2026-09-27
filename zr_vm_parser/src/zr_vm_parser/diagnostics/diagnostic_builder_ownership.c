#include "zr_vm_parser/diagnostic_builder.h"

/* nullable owner 的解包策略取决于用户控制流，诊断可给出建议，但不能提供自动编辑。 */
TZrBool ZrParser_DiagnosticBuilder_BuildNullableOwnershipIntrinsicOperand(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "nullable_ownership_intrinsic_operand",
                "Ownership transition requires a live owner",
                "The ownership intrinsic received a nullable owner that may not contain a live handle.",
                "Handle or unwrap the nullable owner before applying another ownership transition.")) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                out, ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* 所有权流分析报告移动后的读取；修复需要重新安排转移，故明确记录用户决策原因。 */
TZrBool ZrParser_DiagnosticBuilder_BuildUseAfterMove(SZrState *state,
                                                     SZrStructuredDiagnostic *out,
                                                     SZrFileRange location) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "use_after_move",
                "Unique value is used after it was moved",
                "Ownership-flow analysis found a path where this Unique value was moved before the current read.",
                "Use the value before moving it, or transfer a shared or borrowed value instead.")) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                out, ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
