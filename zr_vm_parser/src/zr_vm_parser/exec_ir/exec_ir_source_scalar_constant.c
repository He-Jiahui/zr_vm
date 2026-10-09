#include "zr_vm_parser/exec_ir_source_scalar_constant.h"

#include <string.h>

/* Safe linked refusal stub for the finite source-constant RED gate. */
TZrBool ZrParser_ExecIr_MakeSourceScalarConstant(
        const struct SZrCompilerState *compiler,
        SZrExecIrConstant *output, SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (compiler == ZR_NULL || output == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    if (diagnostic != ZR_NULL) {
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
    }
    return ZR_FALSE;
}
