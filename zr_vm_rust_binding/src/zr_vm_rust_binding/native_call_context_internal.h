#ifndef ZR_VM_RUST_BINDING_NATIVE_CALL_CONTEXT_INTERNAL_H
#define ZR_VM_RUST_BINDING_NATIVE_CALL_CONTEXT_INTERNAL_H

#include "zr_vm_rust_binding.h"
#include "zr_vm_library/native_binding.h"

struct ZrRustBindingNativeCallContext {
    ZrLibCallContext *context;
};

#endif
