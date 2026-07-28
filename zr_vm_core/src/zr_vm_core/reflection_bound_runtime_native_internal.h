#ifndef ZR_VM_REFLECTION_BOUND_RUNTIME_NATIVE_INTERNAL_H
#define ZR_VM_REFLECTION_BOUND_RUNTIME_NATIVE_INTERNAL_H

#include "zr_vm_core/reflection.h"
#include "zr_vm_core/value.h"

struct SZrClosureNative *ZrCore_Reflection_CreateBoundRuntimeNativeClosureInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        FZrNativeFunction nativeFunction,
        TZrBool pinRuntimeModule);

TZrBool ZrCore_Reflection_BoundRuntimeNativeClosureIsValidInternal(
        struct SZrState *state,
        struct SZrClosureNative *closure,
        FZrNativeFunction expectedFunction,
        struct SZrObjectModule *expectedRuntimeModule);

struct SZrMetadataRuntime *ZrCore_Reflection_GetBoundRuntimeFromCallInternal(
        struct SZrState *state,
        FZrNativeFunction expectedFunction);

#endif
