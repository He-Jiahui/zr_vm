#include "zr_vm_core/function_exec_ir.h"

#include "function_ir_owners_internal.h"

#include <string.h>

TZrBool ZrCore_Function_SnapshotExecIr(
        SZrFunction *function, const SZrExecIrModule *source,
        SZrExecIrDiagnostic *diagnostic) {
    if (function == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            memset(diagnostic, 0, sizeof(*diagnostic));
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    return ZrCore_ExecIr_CloneOwnedModule(source, &function->ownedExecIr, diagnostic);
}

const SZrExecIrModule *ZrCore_Function_GetExecIr(const SZrFunction *function) {
    return function == ZR_NULL ? ZR_NULL
            : ZrCore_ExecIr_OwnedModuleView(function->ownedExecIr);
}

void ZrCore_Function_ReleaseExecIrOwner(SZrFunction *function) {
    if (function != ZR_NULL) {
        ZrCore_ExecIr_OwnedModuleFree(&function->ownedExecIr);
    }
}

void ZrCore_Function_ForgetExecIrOwnerAfterMove(SZrFunction *function) {
    if (function != ZR_NULL) {
        function->ownedExecIr = ZR_NULL;
    }
}

void ZrCore_Function_ForgetCopiedExecIrOwnerAliases(
        SZrFunction *copiedChildren, TZrUInt32 count) {
    if (copiedChildren == ZR_NULL) {
        return;
    }
    for (TZrUInt32 index = 0u; index < count; index++) {
        copiedChildren[index].ownedExecIr = ZR_NULL;
    }
}
