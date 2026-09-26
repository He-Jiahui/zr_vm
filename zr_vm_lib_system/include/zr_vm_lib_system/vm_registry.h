//
// zr.system.vm descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_VM_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_VM_REGISTRY_H

#include "zr_vm_lib_system/vm.h"

/** @brief 返回 VM 观察与跨模块调用入口的静态描述符及快照类型。 */
const ZrLibModuleDescriptor *ZrSystem_VmRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_VM_REGISTRY_H
