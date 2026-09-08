#ifndef ZR_VM_CORE_MODULE_CALL_BINDING_H
#define ZR_VM_CORE_MODULE_CALL_BINDING_H

#include "zr_vm_core/call_binding.h"

struct SZrObjectModule;

ZR_CORE_API struct SZrFunction *ZrCore_CallBinding_GetModuleFunction(struct SZrState *state,
        struct SZrObjectModule *module);

ZR_CORE_API TZrBool ZrCore_CallBinding_ModuleConstantContract(const struct SZrFunction *provider,
        TZrUInt32 constantIndex, SZrCallBindingContract *contract);
ZR_CORE_API TZrBool ZrCore_CallBinding_LinkImportedModule(struct SZrState *state,
        struct SZrFunction *consumer, struct SZrObjectModule *module,
        SZrCallBindingDiagnostic *diagnostic);

#endif
