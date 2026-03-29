//
// Native module registry for built-in and plugin-backed descriptors.
//

#ifndef ZR_VM_LIBRARY_NATIVE_REGISTRY_H
#define ZR_VM_LIBRARY_NATIVE_REGISTRY_H

#include "zr_vm_library/native_binding.h"

#define ZR_VM_NATIVE_PLUGIN_ABI_VERSION 1U

ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_Attach(SZrGlobalState *global);
ZR_LIBRARY_API void ZrLibrary_NativeRegistry_Free(SZrGlobalState *global);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_RegisterModule(SZrGlobalState *global,
                                                               const ZrLibModuleDescriptor *descriptor);
ZR_LIBRARY_API const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModule(SZrGlobalState *global,
                                                                                const TZrChar *moduleName);

#endif // ZR_VM_LIBRARY_NATIVE_REGISTRY_H
