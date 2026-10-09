#include "zr_vm_parser/exec_ir_source_frame.h"

#include <string.h>

TZrBool ZrParser_ExecIr_ValidatePrimitiveSourceFrame(
        const SZrExecIrFunction *function, const SZrSemanticContext *context,
        const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
        TZrUInt32 frameByteLimit, SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    (void)frameByteLimit;
    if (function == ZR_NULL || context == ZR_NULL ||
        (layoutCount != 0u && layouts == ZR_NULL)) {
        if (diagnostic != ZR_NULL)
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        return ZR_FALSE;
    }

    /* RED staging: the const validation producer chain is not implemented. */
    if (diagnostic != ZR_NULL) {
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        diagnostic->functionToken = function->functionToken;
    }
    return ZR_FALSE;
}
