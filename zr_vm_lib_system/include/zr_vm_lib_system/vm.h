//
// zr.system.vm native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_VM_H
#define ZR_VM_LIB_SYSTEM_VM_H

#include "zr_vm_lib_system/conf.h"

/** @brief 将当前全局状态的已加载模块与原生注册信息投影为脚本快照数组。
 *  @note 只列已加载实例；描述符已登记但尚未导入的模块不在结果中。 */
TZrBool ZrSystem_Vm_LoadedModules(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 返回当前调用线程的栈状态和所属全局状态的 GC/模块计数快照。 */
TZrBool ZrSystem_Vm_State(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 用数组参数调用另一模块的公开导出，沿用原生绑定的参数与异常检查。
 *  @note 目标模块可能因查询导出而被导入；脚本侧调用可产生任意目标导出副作用。 */
TZrBool ZrSystem_Vm_CallModuleExport(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_VM_H
