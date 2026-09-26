//
// zr.system.assembly descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_ASSEMBLY_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_ASSEMBLY_REGISTRY_H

#include "zr_vm_lib_system/assembly.h"

/** @brief 返回 zr.system.assembly 的静态描述符，供根模块注册及原生导出绑定。 */
const ZrLibModuleDescriptor *ZrSystem_AssemblyRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_ASSEMBLY_REGISTRY_H
