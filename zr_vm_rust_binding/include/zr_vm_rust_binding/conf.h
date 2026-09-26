#ifndef ZR_VM_RUST_BINDING_CONF_H
#define ZR_VM_RUST_BINDING_CONF_H

#include "zr_vm_common.h"

/* C 共享库和 Rust sys 声明共用这一导出边界；错误文案容量在两侧 ABI 中必须一致。 */
#define ZR_RUST_BINDING_API ZR_API
#define ZR_RUST_BINDING_ERROR_MESSAGE_CAPACITY 512U

#endif
