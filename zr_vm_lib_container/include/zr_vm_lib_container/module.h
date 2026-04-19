//
// Built-in zr.container native module registration.
//

#ifndef ZR_VM_LIB_CONTAINER_MODULE_H
#define ZR_VM_LIB_CONTAINER_MODULE_H

#include "zr_vm_lib_container/conf.h"

typedef struct ZrVmLibContainerDebugHotMapLookupStats {
    TZrUInt64 hotHitCount;
    TZrUInt64 memberVersionHitCount;
    TZrUInt64 entryValidationReadCount;
} ZrVmLibContainerDebugHotMapLookupStats;

ZR_VM_LIB_CONTAINER_API const ZrLibModuleDescriptor *ZrVmLibContainer_GetModuleDescriptor(void);
ZR_VM_LIB_CONTAINER_API TZrBool ZrVmLibContainer_Register(SZrGlobalState *global);
ZR_VM_LIB_CONTAINER_API void ZrVmLibContainer_Debug_ResetHotMapLookupStats(void);
ZR_VM_LIB_CONTAINER_API ZrVmLibContainerDebugHotMapLookupStats ZrVmLibContainer_Debug_GetHotMapLookupStats(void);

#if defined(ZR_LIBRARY_TYPE_SHARED)
ZR_VM_LIB_CONTAINER_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_CONTAINER_MODULE_H
