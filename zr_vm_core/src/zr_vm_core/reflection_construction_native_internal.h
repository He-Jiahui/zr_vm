#ifndef ZR_VM_REFLECTION_CONSTRUCTION_NATIVE_INTERNAL_H
#define ZR_VM_REFLECTION_CONSTRUCTION_NATIVE_INTERNAL_H

#include "zr_vm_core/reflection.h"

TZrInt64 ZrCore_Reflection_RequireConstructibleNativeEntryInternal(
        struct SZrState *state);

TZrInt64 ZrCore_Reflection_CreateInstanceNativeEntryInternal(
        struct SZrState *state);

struct SZrClosureNative *
ZrCore_Reflection_CreateRequireConstructibleNativeClosureInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule);

struct SZrClosureNative *
ZrCore_Reflection_CreateInstanceNativeClosureInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule);

#endif
