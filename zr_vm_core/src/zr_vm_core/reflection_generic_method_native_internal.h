#ifndef ZR_VM_REFLECTION_GENERIC_METHOD_NATIVE_INTERNAL_H
#define ZR_VM_REFLECTION_GENERIC_METHOD_NATIVE_INTERNAL_H

#include "zr_vm_core/reflection.h"

struct SZrClosureNative *ZrCore_Reflection_CreateMakeGenericMethodNativeClosureInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule);

#endif
