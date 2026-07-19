#ifndef ZR_VM_REFLECTION_MODULE_INTERNAL_H
#define ZR_VM_REFLECTION_MODULE_INTERNAL_H

#include "zr_vm_core/reflection.h"

#define ZR_REFLECTION_MODULE_NAME "zr.reflection"
#define ZR_REFLECTION_MAKE_GENERIC_METHOD_EXPORT "MakeGenericMethod"
#define ZR_REFLECTION_SERVICE_MODULE_CACHE_NAME "__zr_reflection_service_module"

struct SZrObjectModule *ZrCore_Reflection_CreateModuleForRuntimeInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule);

#endif
