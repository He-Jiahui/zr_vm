#include "zr_vm_lib_iteration/module.h"

#include "zr_vm_library/native_registry.h"

/**
 * ## Summary
 * 描述符定义集中在 descriptor.c，使内建注册和共享插件入口返回同一静态对象。
 * ## Returns
 * @return descriptor.c 中唯一静态描述符的借用只读地址；不分配或注册。
 * ## Constraints and lifecycle
 * 此跨翻译单元访问器只在本模块内部使用；描述符地址由 provider 静态存储期持有。
 * ## References
 * @ref ZrVmLibIteration_Runtime_GetModuleDescriptor; @ref g_iteration_module_descriptor; @ref ZrVmLibIteration_GetModuleDescriptor.
 */
const ZrLibModuleDescriptor *ZrVmLibIteration_Runtime_GetModuleDescriptor(void);

const ZrLibModuleDescriptor *ZrVmLibIteration_GetModuleDescriptor(void) {
    return ZrVmLibIteration_Runtime_GetModuleDescriptor();
}

TZrBool ZrVmLibIteration_Register(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    /*
     * ## Summary
     * 先检查并附着全局原生注册表，再提交本模块描述符。
     * ## Constraints and lifecycle
     * RegisterModule 的记录路径也会 Attach；已附着时 Attach 返回成功。若后续注册失败，此处不撤销已建立的注册表。
     * 保留显式 Attach 的既有返回检查，不将它解释为独立的重复注册语义。
     * ## References
     * ZrLibrary_NativeRegistry_Attach; ZrLibrary_NativeRegistry_RegisterModule.
     */
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
