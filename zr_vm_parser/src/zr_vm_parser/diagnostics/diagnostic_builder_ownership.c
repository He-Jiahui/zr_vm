#include "zr_vm_parser/diagnostic_builder.h"

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
