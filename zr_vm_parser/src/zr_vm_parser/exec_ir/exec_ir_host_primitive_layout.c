#include "zr_vm_parser/exec_ir_host_primitive_layout.h"

#include <string.h>

TZrBool ZrParser_ExecIr_MakeHostPrimitiveLayout(
        const SZrSemanticContext *context, TZrTypeId typeId, TZrUInt32 layoutId,
        SZrExecIrLayout *output, SZrExecIrDiagnostic *diagnostic) {
    (void)context;
    (void)typeId;
    (void)layoutId;
    (void)output;
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
    }
    return ZR_FALSE;
}
