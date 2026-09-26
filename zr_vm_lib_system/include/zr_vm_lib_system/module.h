//
// Built-in zr.system native module registration.
//

#ifndef ZR_VM_LIB_SYSTEM_MODULE_H
#define ZR_VM_LIB_SYSTEM_MODULE_H

#include "zr_vm_lib_system/conf.h"

/** @brief 返回聚合入口 zr.system 的静态描述符；叶模块由各自 registry 提供，不随此调用注册。 */
ZR_API const ZrLibModuleDescriptor *ZrVmLibSystem_GetModuleDescriptor(void);
/** @brief 为全局状态注册各叶模块及聚合入口，并预热异常类型供运行期抛错与类型匹配使用。
 *  @pre global 必须已有可导入模块的主线程状态；失败时可能已完成部分注册，宿主负责销毁该全局状态。 */
ZR_API TZrBool ZrVmLibSystem_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 动态加载器约定的固定 v1 入口符号；描述符内的 ABI 版本仍由加载器单独核对。 */
ZR_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_SYSTEM_MODULE_H
