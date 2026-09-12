#include "zr_vm_parser/exec_ir_projections.h"

TZrBool ZrParser_ExecIr_LowerExecBc(const SZrExecIrFunction *function,
                                    SZrExecBcProjection *output,
                                    SZrExecIrDiagnostic *diagnostic) {
    SZrExecBcProjection prepared;
    TZrBool ok;
    if (output == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    prepared = (SZrExecBcProjection){0};
    ok = ZrParser_ExecIr_BuildProjection(function, &prepared, diagnostic);
    if (!ok) return ZR_FALSE;
    ZrParser_ExecBcProjection_Free(output);
    *output = prepared;
    return ZR_TRUE;
}
