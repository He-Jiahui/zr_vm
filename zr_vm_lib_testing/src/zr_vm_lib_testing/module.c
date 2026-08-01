#include "zr_vm_lib_testing/module.h"

#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/native_registry.h"
#include "runtime/runtime_internal.h"

const ZrLibModuleDescriptor *ZrVmLibTesting_GetModuleDescriptor(void) {
    return ZrVmLibTesting_Runtime_GetModuleDescriptor();
}

TZrBool ZrVmLibTesting_Register(SZrGlobalState *global) {
    return global != ZR_NULL &&
           ZrCore_TaskRuntime_RegisterBuiltins(global) &&
           ZrLibrary_NativeRegistry_Attach(global) &&
           ZrLibrary_NativeRegistry_RegisterModule(
                   global, ZrVmLibTesting_GetModuleDescriptor());
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return ZrVmLibTesting_GetModuleDescriptor();
}
#endif
