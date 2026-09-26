#ifndef ZR_VM_RUST_BINDING_NATIVE_CALL_CONTEXT_INTERNAL_H
#define ZR_VM_RUST_BINDING_NATIVE_CALL_CONTEXT_INTERNAL_H

#include "zr_vm_rust_binding.h"
#include "zr_vm_library/native_binding.h"

/* 仅在 VM 调用 native callback 的栈帧内投影底层 context；visitor 和 Rust 包装不得保存此指针。 */
struct ZrRustBindingNativeCallContext {
    ZrLibCallContext *context;
};

#endif
