#include "native_binding_internal.h"

static const ZrLibOfficialModuleInventoryEntry k_official_modules[] = {
        {"zr.builtin", ZR_LIB_OFFICIAL_MODULE_TIER_N0, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
         ZR_PROVIDER_CONTRACT_ROLE_BUILTIN_TYPE_SURFACE},
        {"zr.container", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.iteration", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.math", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.task", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.debug", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.ffi", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.network", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.network.tcp", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.network.udp", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.pooling", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.reflection", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
         ZR_PROVIDER_CONTRACT_ROLE_REFLECTION},
        {"zr.system", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.assembly", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.console", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.env", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.exception", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.fs", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.gc", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.process", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.vm", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.thread", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.compile", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.compile.declaration", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.testing", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_TEST, ZR_PROVIDER_CONTRACT_ROLE_NONE},
};

TZrSize ZrLibrary_OfficialModuleInventory_GetCount(void) {
    return ZR_ARRAY_COUNT(k_official_modules);
}

const ZrLibOfficialModuleInventoryEntry *ZrLibrary_OfficialModuleInventory_GetAt(
        TZrSize index) {
    return index < ZR_ARRAY_COUNT(k_official_modules)
                   ? &k_official_modules[index]
                   : ZR_NULL;
}

const ZrLibOfficialModuleInventoryEntry *ZrLibrary_OfficialModuleInventory_Find(
        const TZrChar *moduleName) {
    TZrSize index;

    if (moduleName == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < ZR_ARRAY_COUNT(k_official_modules); index++) {
        if (strcmp(moduleName, k_official_modules[index].moduleName) == 0) {
            return &k_official_modules[index];
        }
    }
    return ZR_NULL;
}

TZrBool native_registry_validate_official_descriptor(
        ZrLibrary_NativeRegistryState *registry,
        const ZrLibModuleDescriptor *descriptor) {
    const ZrLibOfficialModuleInventoryEntry *entry;

    if (registry == ZR_NULL || descriptor == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(descriptor->moduleName, "debug") == 0) {
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_RESERVED_OFFICIAL_MODULE,
                "legacy native module 'debug' was removed; register canonical module 'zr.debug'");
        return ZR_FALSE;
    }

    entry = ZrLibrary_OfficialModuleInventory_Find(descriptor->moduleName);
    if ((entry == ZR_NULL &&
         descriptor->providerContractRole != ZR_PROVIDER_CONTRACT_ROLE_NONE) ||
        (entry != ZR_NULL &&
         descriptor->providerContractRole != entry->providerContractRole)) {
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_PROVIDER_CONTRACT_MISMATCH,
                "module '%s' declares provider contract role %u but official inventory requires %u",
                descriptor->moduleName,
                (unsigned)descriptor->providerContractRole,
                (unsigned)(entry != ZR_NULL
                                   ? entry->providerContractRole
                                   : ZR_PROVIDER_CONTRACT_ROLE_NONE));
        return ZR_FALSE;
    }
    if (entry != ZR_NULL && descriptor->providerPhase != entry->phase) {
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_PHASE_MISMATCH,
                "official module '%s' requires provider phase %u but descriptor declares %u",
                descriptor->moduleName,
                (unsigned)entry->phase,
                (unsigned)descriptor->providerPhase);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool native_registry_validate_official_duplicate(
        ZrLibrary_NativeRegistryState *registry,
        const ZrLibModuleDescriptor *current,
        const ZrLibModuleDescriptor *replacement) {
    const ZrLibOfficialModuleInventoryEntry *entry;

    if (registry == ZR_NULL || current == ZR_NULL || replacement == ZR_NULL ||
        current->moduleName == ZR_NULL || replacement->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }
    entry = ZrLibrary_OfficialModuleInventory_Find(replacement->moduleName);
    if (entry == ZR_NULL || current == replacement) {
        return ZR_TRUE;
    }
    native_registry_set_error(
            registry,
            ZR_LIB_NATIVE_REGISTRY_ERROR_DUPLICATE_OFFICIAL_PROVIDER,
            "official module '%s' is already registered with a different provider contract",
            replacement->moduleName);
    return ZR_FALSE;
}
