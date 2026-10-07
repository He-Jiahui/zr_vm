#include "zr_vm_core/aot_ir_owned_module.h"

#include <stdlib.h>
#include <string.h>

struct SZrAotIrOwnedModule {
    SZrAotIrModule module;
};

static EZrAotIrStatus owned_module_fail(
        EZrAotIrStatus status, SZrAotIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
    }
    return status;
}

EZrAotIrStatus ZrCore_AotIr_CloneOwnedModule(
        const SZrAotIrModule *source, SZrAotIrOwnedModule **destination,
        SZrAotIrDiagnostic *diagnostic) {
    EZrAotIrStatus status;
    if (source == ZR_NULL || destination == ZR_NULL)
        return owned_module_fail(ZR_AOT_IR_INVALID_ARGUMENT, diagnostic);
    status = ZrCore_AotIr_ValidateModule(source, diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    /* 合法描述符尚缺独立复制实现；目的槽保持不变，形成真实特性 RED。 */
    return owned_module_fail(ZR_AOT_IR_UNSUPPORTED, diagnostic);
}

const SZrAotIrModule *ZrCore_AotIr_OwnedModuleView(const SZrAotIrOwnedModule *owner) {
    return owner == ZR_NULL ? ZR_NULL : &owner->module;
}

void ZrCore_AotIr_OwnedModuleFree(SZrAotIrOwnedModule **owner) {
    if (owner == ZR_NULL || *owner == ZR_NULL) return;
    free(*owner);
    *owner = ZR_NULL;
}
