#include "zr_vm_parser/exec_ir_dead_source_places.h"

#include <string.h>

/* Callable RED boundary. Root establishes behavioral failure before the
 * implementation replaces this finite unsupported stub. */
TZrBool ZrParser_ExecIr_EliminateDeadSourcePlaces(
        const SZrSemanticIrFunction *semantic,
        const SZrSemanticContext *context,
        const SZrExecIrFunction *input,
        SZrExecIrFunction *output,
        SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = semantic == ZR_NULL || context == ZR_NULL ||
                input == ZR_NULL || output == ZR_NULL || input == output
                ? ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT
                : ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        if (input != ZR_NULL) diagnostic->functionToken = input->functionToken;
    }
    return ZR_FALSE;
}
