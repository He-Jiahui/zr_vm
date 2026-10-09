#include "zr_vm_core/exec_ir_owned_module.h"

#include <stdlib.h>
#include <string.h>

struct SZrExecIrOwnedModule {
    SZrExecIrModule module;
};

/* RED staging: the constructor refuses storage snapshots until GREEN GO. */
TZrBool ZrCore_ExecIr_CloneOwnedModule(
        const SZrExecIrModule *source, SZrExecIrOwnedModule **destination,
        SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = source == ZR_NULL || destination == ZR_NULL
                ? ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT
                : ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
    }
    return ZR_FALSE;
}

const SZrExecIrModule *ZrCore_ExecIr_OwnedModuleView(
        const SZrExecIrOwnedModule *owner) {
    return owner != ZR_NULL ? &owner->module : ZR_NULL;
}

void ZrCore_ExecIr_OwnedModuleFree(SZrExecIrOwnedModule **owner) {
    SZrExecIrOwnedModule *old;
    if (owner == ZR_NULL || *owner == ZR_NULL) {
        return;
    }
    old = *owner;
    *owner = ZR_NULL;
    ZrCore_ExecIr_FreeModule(&old->module);
    free(old);
}
