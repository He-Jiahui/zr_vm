//
// Native module registry for built-in and plugin-backed descriptors.
//

#ifndef ZR_VM_LIBRARY_NATIVE_REGISTRY_H
#define ZR_VM_LIBRARY_NATIVE_REGISTRY_H

#include "zr_vm_library/native_binding.h"

#define ZR_VM_NATIVE_PLUGIN_ABI_VERSION 1U

typedef enum EZrLibNativeModuleRegistrationKind {
    ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_BUILTIN = 0,
    ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN = 1
} EZrLibNativeModuleRegistrationKind;

typedef enum EZrLibNativeRegistryErrorCode {
    ZR_LIB_NATIVE_REGISTRY_ERROR_NONE = 0,
    ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD = 1,
    ZR_LIB_NATIVE_REGISTRY_ERROR_SYMBOL = 2,
    ZR_LIB_NATIVE_REGISTRY_ERROR_ABI_MISMATCH = 3,
    ZR_LIB_NATIVE_REGISTRY_ERROR_VERSION_MISMATCH = 4,
    ZR_LIB_NATIVE_REGISTRY_ERROR_CAPABILITY_MISMATCH = 5,
    ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_NAME_MISMATCH = 6
} EZrLibNativeRegistryErrorCode;

typedef struct ZrLibRegisteredModuleInfo {
    const ZrLibModuleDescriptor *descriptor;
    const TZrChar *sourcePath;
    EZrLibNativeModuleRegistrationKind registrationKind;
    TZrBool isDescriptorPlugin;
} ZrLibRegisteredModuleInfo;

ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_Attach(SZrGlobalState *global);
ZR_LIBRARY_API void ZrLibrary_NativeRegistry_Free(SZrGlobalState *global);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_RegisterModule(SZrGlobalState *global,
                                                               const ZrLibModuleDescriptor *descriptor);
ZR_LIBRARY_API const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModule(SZrGlobalState *global,
                                                                                const TZrChar *moduleName);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_GetModuleInfo(SZrGlobalState *global,
                                                              const TZrChar *moduleName,
                                                              ZrLibRegisteredModuleInfo *outInfo);
ZR_LIBRARY_API EZrLibNativeRegistryErrorCode ZrLibrary_NativeRegistry_GetLastErrorCode(SZrGlobalState *global);
ZR_LIBRARY_API const TZrChar *ZrLibrary_NativeRegistry_GetLastErrorMessage(SZrGlobalState *global);

#endif // ZR_VM_LIBRARY_NATIVE_REGISTRY_H
