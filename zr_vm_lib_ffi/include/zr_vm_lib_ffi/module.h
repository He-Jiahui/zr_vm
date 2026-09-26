//
// Built-in zr.ffi native module registration.
//

#ifndef ZR_VM_LIB_FFI_MODULE_H
#define ZR_VM_LIB_FFI_MODULE_H

#include "zr_vm_lib_ffi/conf.h"

/** @brief 返回 zr.ffi 的进程期模块描述符，供宿主查询或静态注册使用。
 * @return 借用指针；调用方不得修改或释放，也不会因此自动注册模块。
 */
ZR_API const ZrLibModuleDescriptor *ZrVmLibFfi_GetModuleDescriptor(void);
/** @brief 在执行 import 或 native extern 前，将 zr.ffi 注册到指定 global。
 * @pre global 是有效的 VM 全局状态。
 * @return 注册是否成功。
 */
ZR_API TZrBool ZrVmLibFfi_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 供 native plugin loader 通过固定符号名取得同一模块描述符。
 * @note 仅共享库构建导出；符号名中的 v1 与描述符的 abiVersion 分别校验。
 * @return 借用的静态描述符指针。
 */
ZR_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_FFI_MODULE_H
