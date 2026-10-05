#include "zr_vm_parser/exec_ir_source_frame.h"

#include <string.h>

/* Callable RED boundary. Production admission and publication follow only
 * after the real source prerequisites and feature failures are observed. */
TZrBool ZrParser_ExecIr_AttachPrimitiveSourceFrame(
        SZrExecIrFunction *function, const SZrSemanticContext *context,
        const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
        TZrUInt32 frameByteLimit, SZrExecIrDiagnostic *diagnostic) {
    (void)context;
    (void)layouts;
    (void)layoutCount;
    (void)frameByteLimit;
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    }
    return ZR_FALSE;
}
