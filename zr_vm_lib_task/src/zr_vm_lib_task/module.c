#include "zr_vm_lib_task/module.h"

#include "zr_vm_lib_task/runtime.h"
#include "zr_vm_library/native_registry.h"

const ZrLibModuleDescriptor *ZrVmTask_GetModuleDescriptor(void) { return ZrVmTask_Runtime_GetModuleDescriptor(); }

TZrBool ZrVmTask_Register(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        return ZR_FALSE;
    }

    return ZrLibrary_NativeRegistry_RegisterModule(global, ZrVmTask_Runtime_GetModuleDescriptor());
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) { return ZrVmTask_Runtime_GetModuleDescriptor(); }
#endif
