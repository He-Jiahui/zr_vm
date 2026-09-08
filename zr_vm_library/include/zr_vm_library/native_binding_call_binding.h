#ifndef ZR_VM_LIBRARY_NATIVE_BINDING_CALL_BINDING_H
#define ZR_VM_LIBRARY_NATIVE_BINDING_CALL_BINDING_H

#include "zr_vm_library/native_binding.h"
#include "zr_vm_core/call_binding.h"

typedef enum EZrNativeCallBindingDescriptorKind {
    ZR_NATIVE_CALL_BINDING_FUNCTION = 0,
    ZR_NATIVE_CALL_BINDING_METHOD = 1,
    ZR_NATIVE_CALL_BINDING_META_METHOD = 2
} EZrNativeCallBindingDescriptorKind;

ZR_LIBRARY_API TZrBool ZrLibrary_NativeCallBinding_GetDescriptorContract(
        const ZrLibModuleDescriptor *module,
        const ZrLibTypeDescriptor *type,
        EZrNativeCallBindingDescriptorKind kind,
        const void *descriptor,
        SZrCallBindingContract *outContract);

#endif
