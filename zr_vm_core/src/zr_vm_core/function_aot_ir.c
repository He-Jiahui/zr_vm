#include "zr_vm_core/function_aot_ir.h"
#include "zr_vm_core/aot_ir_owned_module.h"

#include "function_aot_ir_internal.h"

#include <string.h>

EZrAotIrStatus ZrCore_Function_SnapshotAotIr(
        SZrFunction *function, const SZrAotIrModule *source,
        SZrAotIrDiagnostic *diagnostic) {
    /* Safe RED scaffolding: no allocation, attachment or replacement yet. */
    EZrAotIrStatus status = (function == ZR_NULL || source == ZR_NULL)
            ? ZR_AOT_IR_INVALID_ARGUMENT : ZR_AOT_IR_UNSUPPORTED;
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
    }
    return status;
}

const SZrAotIrModule *ZrCore_Function_GetAotIr(const SZrFunction *function) {
    return function == ZR_NULL ? ZR_NULL
            : ZrCore_AotIr_OwnedModuleView(function->ownedAotIr);
}

void ZrCore_Function_ReleaseAotIrOwner(SZrFunction *function) {
    if (function != ZR_NULL) {
        ZrCore_AotIr_OwnedModuleFree(&function->ownedAotIr);
    }
}

void ZrCore_Function_ForgetAotIrOwnerAfterMove(SZrFunction *function) {
    if (function != ZR_NULL) {
        function->ownedAotIr = ZR_NULL;
    }
}

void ZrCore_Function_ForgetCopiedAotIrOwnerAliases(
        SZrFunction *copiedChildren, TZrUInt32 count) {
    if (copiedChildren == ZR_NULL) {
        return;
    }
    for (TZrUInt32 index = 0; index < count; index++) {
        copiedChildren[index].ownedAotIr = ZR_NULL;
    }
}
