//
// Built-in zr.ffi native module registration.
//

#ifndef ZR_VM_LIB_FFI_MODULE_H
#define ZR_VM_LIB_FFI_MODULE_H

#include "zr_vm_library.h"

ZR_API const ZrLibModuleDescriptor *ZrVmLibFfi_GetModuleDescriptor(void);
ZR_API TZrBool ZrVmLibFfi_Register(SZrGlobalState *global);

#endif // ZR_VM_LIB_FFI_MODULE_H
