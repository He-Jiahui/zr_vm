#include "zr_vm_lib_iteration/module.h"

#include "zr_vm_library/native_registry.h"

const ZrLibModuleDescriptor *ZrVmLibIteration_Runtime_GetModuleDescriptor(void);

const ZrLibModuleDescriptor *ZrVmLibIteration_GetModuleDescriptor(void) {
    return ZrVmLibIteration_Runtime_GetModuleDescriptor();
}

TZrBool ZrVmLibIteration_Register(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    // TODO: RegisterModule 内部也执行 Attach；需核对这里的提前附着是否承载独立的生命周期约束。
    // 核查入口：native_binding_support.c 的注册流程及容器重复注册测试。
    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        return ZR_FALSE;
    }

    return ZrLibrary_NativeRegistry_RegisterModule(global, ZrVmLibIteration_GetModuleDescriptor());
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return ZrVmLibIteration_GetModuleDescriptor();
}
#endif
