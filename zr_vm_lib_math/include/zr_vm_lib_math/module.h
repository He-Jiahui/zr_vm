//
// Built-in zr.math native module registration.
//

#ifndef ZR_VM_LIB_MATH_MODULE_H
#define ZR_VM_LIB_MATH_MODULE_H

#include "zr_vm_lib_math/conf.h"

/** @brief 供宿主或插件加载器读取 `zr.math` 的进程内静态描述符。
 *  @return 借用指针；调用方不得释放，也不得修改其中的注册表。
 */
ZR_API const ZrLibModuleDescriptor *ZrVmLibMath_GetModuleDescriptor(void);
/** @brief 将同一描述符注册到指定 VM 的 native registry，供 `import("zr.math")` 使用。
 *  @pre `global` 已创建，且其 native registry 可用于注册模块。
 *  @return 注册是否成功；失败细节由 native registry 报告。
 */
ZR_API TZrBool ZrVmLibMath_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 共享库插件协议规定的发现入口；返回值与内建注册使用同一描述符。 */
ZR_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_MATH_MODULE_H
