//
// zr.system.gc descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_GC_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_GC_REGISTRY_H

#include "zr_vm_lib_system/gc.h"

/** @brief 返回 GC 控制回调及 SystemGcStats 字段契约的静态描述符。 */
const ZrLibModuleDescriptor *ZrSystem_GcRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_GC_REGISTRY_H
