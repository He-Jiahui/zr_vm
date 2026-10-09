#include "zr_vm_core/exec_ir_module_copy.h"

#include <string.h>

TZrBool ZrCore_ExecIr_CloneModuleReplacingFunction(
        const SZrExecIrModule *source, TZrExecIrFunctionId selectedId,
        const SZrExecIrFunction *replacement, SZrExecIrModule *destination,
        SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (source == ZR_NULL || replacement == ZR_NULL || destination == ZR_NULL ||
        source == destination) {
        if (diagnostic != ZR_NULL)
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        return ZR_FALSE;
    }
    if (selectedId == 0u || selectedId > source->functionCount) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
            diagnostic->expectedVersion = source->functionCount;
            diagnostic->actualVersion = selectedId;
        }
        return ZR_FALSE;
    }
    /* RED staging: no candidate cloning or destination publication yet. */
    if (diagnostic != ZR_NULL)
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
    return ZR_FALSE;
}
