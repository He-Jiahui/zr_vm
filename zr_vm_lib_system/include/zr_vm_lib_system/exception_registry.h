//
// zr.system.exception descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_EXCEPTION_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_EXCEPTION_REGISTRY_H

#include "zr_vm_lib_system/exception.h"

/** @brief 返回异常类型层级与未处理异常注册入口的静态描述符。 */
const ZrLibModuleDescriptor *ZrSystem_ExceptionRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_EXCEPTION_REGISTRY_H
