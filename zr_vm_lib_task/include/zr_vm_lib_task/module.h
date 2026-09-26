/** 旧版 zr.task 原生模块入口；目前不在顶层构建和 CLI 注册路径中。 */

#ifndef ZR_VM_TASK_MODULE_H
#define ZR_VM_TASK_MODULE_H

#include "zr_vm_lib_task/conf.h"

/** @brief 借用旧版模块 descriptor，供显式宿主注册路径查询。
 *  @return 静态数据，调用方不可释放。
 */
ZR_VM_TASK_API const ZrLibModuleDescriptor *ZrVmTask_GetModuleDescriptor(void);
/** @brief 将旧版 provider 挂到指定 global 的 native registry。
 *  @pre global 已创建，且同名 zr.task provider 尚未注册。
 *  @return registry 附着与注册都成功时为真；失败不代替宿主回滚 global。
 *  @note 当前 CLI 使用 ZrCore_TaskRuntime_RegisterBuiltins；重新启用本入口前须核对 provider 唯一性。
 */
ZR_VM_TASK_API TZrBool ZrVmTask_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 供共享库 loader 按 ABI v1 符号查找旧版 descriptor；返回值仍由模块持有。 */
ZR_VM_TASK_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_TASK_MODULE_H
