//
// zr.system.process descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_PROCESS_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_PROCESS_REGISTRY_H

#include "zr_vm_lib_system/process.h"

/** @brief 返回 process 描述符；arguments 的运行时值由 CLI 入口注入。 */
const ZrLibModuleDescriptor *ZrSystem_ProcessRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_PROCESS_REGISTRY_H
