//
// zr.thread native module registration.
//

#ifndef ZR_VM_THREAD_MODULE_H
#define ZR_VM_THREAD_MODULE_H

#include "zr_vm_lib_thread/conf.h"

/** @brief 返回模块持有的静态 zr.thread 描述符；调用方只读使用，不负责释放。 */
ZR_VM_THREAD_API const ZrLibModuleDescriptor *ZrVmThread_GetModuleDescriptor(void);
/** @brief 将描述符注册到目标全局状态；实例化调度器前宿主还须注册 zr.task 的 Job/Task 内建类型。 */
ZR_VM_THREAD_API TZrBool ZrVmThread_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 插件加载器查找的固定入口符号，返回同一静态描述符；ABI 版本由描述符另行校验。 */
ZR_VM_THREAD_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_THREAD_MODULE_H
