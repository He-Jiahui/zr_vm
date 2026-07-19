#include "zr_vm_parser/diagnostic_builder.h"

TZrBool ZrParser_DiagnosticBuilder_BuildUseAfterMove(SZrState *state,
                                                     SZrStructuredDiagnostic *out,
                                                     SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "use_after_move",
            "Unique value is used after it was moved",
            "Ownership-flow analysis found a path where this %unique value was moved before the current read.",
            "Use the value before moving it, or transfer a shared or borrowed value instead.");
}
