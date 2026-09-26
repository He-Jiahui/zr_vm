// zr.iteration 的公开协议由本模块拥有，具体容器的迭代回调由各自 provider 提供。

#ifndef ZR_VM_LIB_ITERATION_MODULE_H
#define ZR_VM_LIB_ITERATION_MODULE_H

#include "zr_vm_library.h"

#define ZR_VM_LIB_ITERATION_API ZR_API

/**
 * @brief 返回 zr.iteration 的规范类型描述符，供注册器、反射和编译器共享同一协议身份。
 * @return 指向模块持有的静态描述符；调用方只借用，不释放，也不修改。
 */
ZR_VM_LIB_ITERATION_API const ZrLibModuleDescriptor *ZrVmLibIteration_GetModuleDescriptor(void);

/**
 * @brief 在全局原生注册表中发布迭代协议，供容器注册和源代码导入使用。
 * @pre global 已建立 mainThreadState；重复传入同一描述符由注册表处理。
 * @return 注册表附着或描述符注册失败时返回 ZR_FALSE。
 */
ZR_VM_LIB_ITERATION_API TZrBool ZrVmLibIteration_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 共享库插件加载器约定的入口；返回与内建注册路径相同的静态描述符。 */
ZR_VM_LIB_ITERATION_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_ITERATION_MODULE_H
