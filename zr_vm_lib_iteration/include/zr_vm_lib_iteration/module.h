// zr.iteration 拥有同步/异步协议身份；具体集合的迭代回调由对应 provider 提供。

#ifndef ZR_VM_LIB_ITERATION_MODULE_H
#define ZR_VM_LIB_ITERATION_MODULE_H

#include "zr_vm_library.h"

#define ZR_VM_LIB_ITERATION_API ZR_API

/**
 * ## Summary
 * 返回 zr.iteration 的规范类型与成员描述符，供注册器、反射和编译器共享协议身份；本调用不注册模块。
 * ## Returns
 * @return 模块持有的静态 const 描述符地址；无分配、回调或失败分支。
 * ## Constraints and lifecycle
 * 调用方只借用并只读；不得释放或修改。共享插件返回的地址在插件映像卸载后失效，持有方须先结束使用。
 * ## References
 * @ref ZrVmLibIteration_Runtime_GetModuleDescriptor; @ref ZrLibrary_NativeRegistry_RegisterModule;
 * @ref ZrVm_GetNativeModule_v1.
 */
ZR_VM_LIB_ITERATION_API const ZrLibModuleDescriptor *ZrVmLibIteration_GetModuleDescriptor(void);

/**
 * ## Summary
 * 将迭代协议描述符发布到 global 的原生模块注册表，供容器注册及后续导入查询使用。
 * ## Parameters
 * @param[in,out] global 非空且 mainThreadState 已建立的全局状态；本函数会附着并修改其原生注册表。
 * ## Returns
 * @return Attach 与描述符注册均成功时返回 ZR_TRUE；参数、附着、描述符校验或注册失败时返回 ZR_FALSE。
 * ## Constraints and lifecycle
 * 在全局状态初始化后、消费者查询模块之前调用。Attach 成功而注册失败时，本函数不撤销已附着的注册表。
 * 注册记录借用模块静态描述符；global 的生命周期由调用方管理。接口未承诺并发注册安全，应由初始化流程串行调用。
 * 同名模块是否可接受由注册表的兼容性校验决定，不能将所有重复注册视为成功。
 * ## References
 * @ref ZrLibrary_NativeRegistry_Attach; @ref ZrLibrary_NativeRegistry_RegisterModule;
 * @ref ZrVmLibIteration_GetModuleDescriptor; @ref ZrVmLibContainer_Register.
 */
ZR_VM_LIB_ITERATION_API TZrBool ZrVmLibIteration_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/**
 * ## Summary
 * 导出共享库插件 ABI v1 描述符入口；只返回模块描述符，不附着或修改当前 global 注册表。
 * ## Returns
 * @return 与内建 getter 相同的模块静态 const 描述符；不分配内存。
 * ## Constraints and lifecycle
 * 仅在 ZR_LIBRARY_TYPE_SHARED 下声明。插件加载器按符号名解析此函数；描述符及其字符串由插件映像持有，
 * 注册表使用期间必须保持映像加载，卸载顺序由插件加载器管理。
 * ## References
 * @ref ZrVmLibIteration_GetModuleDescriptor; native_binding_registry_plugin.c::native_registry_find_symbol.
 */
ZR_VM_LIB_ITERATION_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_ITERATION_MODULE_H
