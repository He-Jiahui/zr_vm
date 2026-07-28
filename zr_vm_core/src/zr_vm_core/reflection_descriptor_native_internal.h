#ifndef ZR_VM_REFLECTION_DESCRIPTOR_NATIVE_INTERNAL_H
#define ZR_VM_REFLECTION_DESCRIPTOR_NATIVE_INTERNAL_H

#include "zr_vm_core/reflection.h"

struct SZrObject;
struct SZrState;

TZrBool ZrCore_Reflection_AttachDescriptorNativeMethodsInternal(
        struct SZrState *state,
        struct SZrObject *descriptor,
        EZrReflectionTypeCategory category);

#endif
