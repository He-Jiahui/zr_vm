//
// zr.system.fs descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_FS_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_FS_REGISTRY_H

#include "zr_vm_lib_system/fs.h"

/** @brief 返回静态 zr.system.fs 描述符，供 zr.system 聚合注册及 native materializer 复用。 */
const ZrLibModuleDescriptor *ZrSystem_FsRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_FS_REGISTRY_H
