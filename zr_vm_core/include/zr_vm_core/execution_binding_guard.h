#ifndef ZR_VM_CORE_EXECUTION_BINDING_GUARD_H
#define ZR_VM_CORE_EXECUTION_BINDING_GUARD_H

#include "zr_vm_core/call_binding.h"

struct SZrFunctionCallSiteCacheEntry;
struct SZrObjectPrototype;

typedef enum EZrExecutionBindingGuardResult {
    ZR_EXECUTION_BINDING_GUARD_OK = 0,
    ZR_EXECUTION_BINDING_GUARD_STALE_GENERATION,
    ZR_EXECUTION_BINDING_GUARD_CONTRACT_MISMATCH,
    ZR_EXECUTION_BINDING_GUARD_RECEIVER_TYPE_MISMATCH,
    ZR_EXECUTION_BINDING_GUARD_SHAPE_MISS,
    ZR_EXECUTION_BINDING_GUARD_SLOT_FALLBACK,
    ZR_EXECUTION_BINDING_GUARD_INVALID_SLOT,
    ZR_EXECUTION_BINDING_GUARD_TARGET_MISSING
} EZrExecutionBindingGuardResult;

typedef struct SZrExecutionBindingGuardInput {
    const SZrCallBinding *binding;
    struct SZrFunctionCallSiteCacheEntry *cacheEntry;
    TZrUInt64 activeGeneration;
    TZrUInt64 expectedModuleSignatureHash;
    TZrUInt64 expectedSignatureHash;
    TZrUInt32 expectedLayoutVersion;
    TZrUInt64 expectedLayoutHash;
    const struct SZrObjectPrototype *receiverPrototype;
    TZrUInt64 receiverShapeId;
    TZrUInt64 receiverShapeGeneration;
    TZrBool allowSlotFallback;
} SZrExecutionBindingGuardInput;

typedef struct SZrExecutionBindingGuardDiagnostic {
    EZrExecutionBindingGuardResult result;
    EZrCallBindingStatus bindingStatus;
    TZrUInt32 targetKind;
    TZrUInt32 dispatchSlot;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrExecutionBindingGuardDiagnostic;

ZR_CORE_API EZrExecutionBindingGuardResult ZrCore_Execution_CheckBindingGuard(
        const SZrExecutionBindingGuardInput *input,
        SZrExecutionBindingGuardDiagnostic *diagnostic);

/* Drop dynamic witnesses while retaining the persistent binding contract and
 * relocation location. This is the only reset operation intended for a PIC
 * entry after reload or an untrusted shape miss. */
ZR_CORE_API void ZrCore_Execution_ResetBindingCache(
        struct SZrFunctionCallSiteCacheEntry *cacheEntry);

ZR_CORE_API const char *ZrCore_Execution_BindingGuardResultName(
        EZrExecutionBindingGuardResult result);

#endif
