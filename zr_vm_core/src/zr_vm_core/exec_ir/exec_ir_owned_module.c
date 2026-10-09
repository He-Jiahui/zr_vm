#include "zr_vm_core/exec_ir_owned_module.h"

#include <stdlib.h>
#include <string.h>

struct SZrExecIrOwnedModule {
    SZrExecIrModule module;
};

TZrBool ZrCore_ExecIr_CloneOwnedModule(
        const SZrExecIrModule *source, SZrExecIrOwnedModule **destination,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOwnedModule *candidate;
    SZrExecIrOwnedModule *old;
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (source == ZR_NULL || destination == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    candidate = (SZrExecIrOwnedModule *)calloc(1u, sizeof(*candidate));
    if (candidate == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
            diagnostic->expectedVersion = 1u;
        }
        return ZR_FALSE;
    }
    ZrCore_ExecIr_ModuleInit(&candidate->module);
    if (!ZrCore_ExecIr_CloneModule(source, &candidate->module, diagnostic)) {
        ZrCore_ExecIr_FreeModule(&candidate->module);
        free(candidate);
        return ZR_FALSE;
    }
    old = *destination;
    *destination = candidate;
    ZrCore_ExecIr_OwnedModuleFree(&old);
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    return ZR_TRUE;
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
