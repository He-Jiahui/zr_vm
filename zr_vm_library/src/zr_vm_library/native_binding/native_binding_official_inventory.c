#include "native_binding_internal.h"

static const ZrLibOfficialModuleInventoryEntry k_official_modules[] = {
        {"zr.builtin", ZR_LIB_OFFICIAL_MODULE_TIER_N0, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.container", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.iteration", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.math", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.task", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.debug", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.ffi", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.network", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.network.tcp", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.network.udp", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.pooling", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.reflection", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.assembly", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.console", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.env", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.exception", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.fs", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.gc", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.process", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.vm", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.thread", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.compile", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL},
        {"zr.compile.declaration", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL},
        {"zr.testing", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_TEST},
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
