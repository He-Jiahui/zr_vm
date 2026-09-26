//
// zr.system.env descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_ENV_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_ENV_REGISTRY_H

#include "zr_vm_lib_system/env.h"

/** @brief 返回环境变量查询接口的静态原生描述符。 */
const ZrLibModuleDescriptor *ZrSystem_EnvRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_ENV_REGISTRY_H
