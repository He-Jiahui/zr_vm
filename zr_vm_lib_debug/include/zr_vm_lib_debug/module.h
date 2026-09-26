/** @file
 *  @brief 脚本级 `zr.debug` 内建 provider 的描述符和注册入口。
 *
 *  宿主注册可选择完整或沙箱描述符；动态插件加载器经统一符号取得
 *  描述符。注册只借用静态描述符，不转移其生命周期。
 */

#ifndef ZR_VM_LIB_DEBUG_MODULE_H
#define ZR_VM_LIB_DEBUG_MODULE_H

#include "zr_vm_lib_debug/conf.h"

#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"

/** @brief 返回完整脚本调试 provider 的静态描述符。 */
ZR_DEBUG_API const ZrLibModuleDescriptor *ZrVmLibDebug_GetModuleDescriptor(void);
/** @brief 返回在回调运行时拒绝写入 API 的沙箱调试 provider 描述符。 */
ZR_DEBUG_API const ZrLibModuleDescriptor *ZrVmLibDebug_GetSandboxedModuleDescriptor(void);
/** @brief 在 VM 全局 native registry 注册完整调试 provider。 */
ZR_DEBUG_API TZrBool ZrVmLibDebug_Register(SZrGlobalState *global);
/** @brief 在 VM 全局 native registry 注册沙箱 provider。 */
ZR_DEBUG_API TZrBool ZrVmLibDebug_RegisterSandboxed(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 共享库通用插件入口，供 native module loader 解析。 */
ZR_DEBUG_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_DEBUG_MODULE_H
