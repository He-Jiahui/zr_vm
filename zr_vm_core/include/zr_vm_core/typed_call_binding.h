#ifndef ZR_VM_CORE_TYPED_CALL_BINDING_H
#define ZR_VM_CORE_TYPED_CALL_BINDING_H

#include "zr_vm_core/function.h"

ZR_CORE_API TZrBool ZrCore_CallBinding_LinkTypedSignature(
        SZrFunction *function, SZrFunctionCallSiteCacheEntry *entry,
        SZrCallBindingDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_CallBinding_PrepareTypedCall(
        struct SZrState *state, SZrFunction *function, SZrFunctionCallSiteCacheEntry *entry,
        const SZrTypeValue *callable, SZrCallBindingDiagnostic *diagnostic);

#endif
