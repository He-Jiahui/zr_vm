#ifndef ZR_VM_REFLECTION_TYPE_RESOLVE_NATIVE_INTERNAL_H
#define ZR_VM_REFLECTION_TYPE_RESOLVE_NATIVE_INTERNAL_H

#include "zr_vm_core/reflection.h"

TZrInt64 ZrCore_Reflection_ResolveTypeIdNativeEntryInternal(struct SZrState *state);

struct SZrClosureNative *ZrCore_Reflection_CreateResolveTypeIdNativeClosureInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule);

#endif
