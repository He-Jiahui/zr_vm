#include "exec_ir_source_finalized_module_internal.h"

#include <string.h>

TZrBool source_snapshot_finalized_primitive_module(
        const SZrSemanticIrFunction *semantic, const SZrSemanticContext *context, const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *sourceIdentity, const SZrArray *compilerConstants,
        const SZrTypeValue *functionConstants, TZrUInt32 functionConstantCount, const SZrExecIrModule *original,
        const SZrExecIrFunction *finalized, SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL)
        memset(diagnostic, 0, sizeof(*diagnostic));
    if (semantic == ZR_NULL || context == ZR_NULL || script == ZR_NULL || sourceIdentity == ZR_NULL ||
        compilerConstants == ZR_NULL || original == ZR_NULL || finalized == ZR_NULL || destination == ZR_NULL)
        return source_finalized_module_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    (void) functionConstants;
    (void) functionConstantCount;

    /* RED staging: observe only a safely bounded original selected header. */
    return source_finalized_module_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                        source_finalized_module_read_selected_token(original));
}
