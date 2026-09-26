//
// zr.system.env native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_ENV_H
#define ZR_VM_LIB_SYSTEM_ENV_H

#include "zr_vm_lib_system/conf.h"

/** @brief 查询宿主进程环境变量；缺失时返回 null，存在时复制为 VM 字符串。
 *  @note 查询的是调用时的宿主环境，不维护模块私有快照。 */
TZrBool ZrSystem_Env_GetVariable(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_ENV_H
