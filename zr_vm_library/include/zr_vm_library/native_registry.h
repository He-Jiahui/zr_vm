//
// Native module registry for built-in and plugin-backed descriptors.
//

#ifndef ZR_VM_LIBRARY_NATIVE_REGISTRY_H
#define ZR_VM_LIBRARY_NATIVE_REGISTRY_H

#include "zr_vm_library/native_binding.h"
#include "zr_vm_core/call_binding.h"

struct SZrClosureNative;

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
    ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_NAME_MISMATCH = 6,
    ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_IN_USE = 7,
    ZR_LIB_NATIVE_REGISTRY_ERROR_PHASE_MISMATCH = 8,
    ZR_LIB_NATIVE_REGISTRY_ERROR_RESERVED_OFFICIAL_MODULE = 9,
    ZR_LIB_NATIVE_REGISTRY_ERROR_DUPLICATE_OFFICIAL_PROVIDER = 10,
    ZR_LIB_NATIVE_REGISTRY_ERROR_PROVIDER_CONTRACT_MISMATCH = 11,
    ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE = 12,
    ZR_LIB_NATIVE_REGISTRY_ERROR_DUPLICATE_PROVIDER_CONTRACT = 13
} EZrLibNativeRegistryErrorCode;

typedef enum EZrLibOfficialModuleTier {
    ZR_LIB_OFFICIAL_MODULE_TIER_N0 = 0,
    ZR_LIB_OFFICIAL_MODULE_TIER_N1 = 1,
    ZR_LIB_OFFICIAL_MODULE_TIER_N2 = 2,
    ZR_LIB_OFFICIAL_MODULE_TIER_N3 = 3
} EZrLibOfficialModuleTier;

typedef struct ZrLibOfficialModuleInventoryEntry {
    const TZrChar *moduleName;
    EZrLibOfficialModuleTier tier;
    EZrLibrary_ProviderPhase phase;
    EZrProviderContractRole providerContractRole;
} ZrLibOfficialModuleInventoryEntry;

typedef struct ZrLibRegisteredCanonicalTypeRole {
    const ZrLibModuleDescriptor *provider;
    const ZrLibCanonicalTypeRoleDescriptor *typeRole;
} ZrLibRegisteredCanonicalTypeRole;

typedef struct ZrLibRegisteredModuleInfo {
    const ZrLibModuleDescriptor *descriptor;
    const TZrChar *moduleName;
    const TZrChar *sourcePath;
    EZrLibNativeModuleRegistrationKind registrationKind;
    TZrBool isDescriptorPlugin;
    TZrUInt32 ownerRefCount;
} ZrLibRegisteredModuleInfo;

ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_Attach(SZrGlobalState *global);
ZR_LIBRARY_API void ZrLibrary_NativeRegistry_Free(SZrGlobalState *global);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_RegisterModule(SZrGlobalState *global,
                                                               const ZrLibModuleDescriptor *descriptor);
ZR_LIBRARY_API const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModule(SZrGlobalState *global,
                                                                                const TZrChar *moduleName);
ZR_LIBRARY_API const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModuleByProviderRole(
        SZrGlobalState *global,
        EZrProviderContractRole providerRole);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_FindCanonicalTypeRole(
        SZrGlobalState *global,
        EZrCanonicalTypeRole role,
        ZrLibRegisteredCanonicalTypeRole *outRole);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByName(
        SZrGlobalState *global,
        const TZrChar *canonicalName,
        ZrLibRegisteredCanonicalTypeRole *outRole);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByProjection(
        SZrGlobalState *global,
        EZrProviderContractRole providerRole,
        EZrCanonicalTypeProjectionKind projectionKind,
        ZrLibRegisteredCanonicalTypeRole *outRole);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_ValidateModuleDescriptor(
        SZrGlobalState *global,
        const ZrLibModuleDescriptor *descriptor);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_GetModuleInfo(SZrGlobalState *global,
                                                              const TZrChar *moduleName,
                                                              ZrLibRegisteredModuleInfo *outInfo);
ZR_LIBRARY_API TZrSize ZrLibrary_NativeRegistry_GetModuleCount(SZrGlobalState *global);
ZR_LIBRARY_API TZrUInt32 ZrLibrary_NativeRegistry_GetModuleRefCount(SZrGlobalState *global,
                                                                    const TZrChar *moduleName);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_GetModuleInfoAt(SZrGlobalState *global,
                                                                TZrSize index,
                                                                ZrLibRegisteredModuleInfo *outInfo);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_GetModuleInfoBySourcePath(SZrGlobalState *global,
                                                                          const TZrChar *sourcePath,
                                                                          ZrLibRegisteredModuleInfo *outInfo);
/* Stable provider identity used by compiler call-binding contracts. */
ZR_LIBRARY_API TZrUInt64 ZrLibrary_NativeRegistry_ComputeModuleSignatureHash(
        const ZrLibModuleDescriptor *descriptor);
ZR_LIBRARY_API EZrCallBindingStatus ZrLibrary_NativeRegistry_ResolveCallBinding(
        SZrState *state,
        const SZrCallBindingContract *contract,
        SZrCallBindingTarget *outTarget,
        SZrCallBindingDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_GetCallBindingIdentity(
        SZrGlobalState *global,
        struct SZrClosureNative *closure,
        SZrCallBindingContract *outContract);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin(SZrState *state,
                                                                              const TZrChar *projectDirectory,
                                                                              const TZrChar *moduleName);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_InvalidateDescriptorPluginSource(SZrGlobalState *global,
                                                                                 const TZrChar *sourcePath);
ZR_LIBRARY_API EZrLibNativeRegistryErrorCode ZrLibrary_NativeRegistry_GetLastErrorCode(SZrGlobalState *global);
ZR_LIBRARY_API const TZrChar *ZrLibrary_NativeRegistry_GetLastErrorMessage(SZrGlobalState *global);
ZR_LIBRARY_API void ZrLibrary_State_SetProviderPhase(SZrState *state,
                                                     EZrLibrary_ProviderPhase phase);
ZR_LIBRARY_API EZrLibrary_ProviderPhase ZrLibrary_State_GetProviderPhase(const SZrState *state);
ZR_LIBRARY_API TZrBool ZrLibrary_ProviderPhase_CanConsume(
        EZrLibrary_ProviderPhase hostPhase,
        EZrLibrary_ProviderPhase providerPhase);
ZR_LIBRARY_API TZrSize ZrLibrary_OfficialModuleInventory_GetCount(void);
ZR_LIBRARY_API const ZrLibOfficialModuleInventoryEntry *ZrLibrary_OfficialModuleInventory_GetAt(
        TZrSize index);
ZR_LIBRARY_API const ZrLibOfficialModuleInventoryEntry *ZrLibrary_OfficialModuleInventory_Find(
        const TZrChar *moduleName);

#endif // ZR_VM_LIBRARY_NATIVE_REGISTRY_H
