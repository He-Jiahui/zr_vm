#include "zr_vm_lib_thread/module.h"

#include "zr_vm_lib_thread/runtime.h"
#include "zr_vm_library/native_registry.h"

const ZrLibModuleDescriptor *ZrVmThread_GetModuleDescriptor(void) { return ZrVmThread_Runtime_GetModuleDescriptor(); }

TZrBool ZrVmThread_Register(SZrGlobalState *global) {
    /* 注册表先附着到目标全局状态，再登记由 runtime.c 维护的静态模块契约。 */
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        return ZR_FALSE;
    }

    return ZrLibrary_NativeRegistry_RegisterModule(global, ZrVmThread_Runtime_GetModuleDescriptor());
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) { return ZrVmThread_Runtime_GetModuleDescriptor(); }
#endif
