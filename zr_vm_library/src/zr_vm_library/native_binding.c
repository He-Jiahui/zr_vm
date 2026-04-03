#include "native_binding_internal.h"

TZrBool ZrLibrary_NativeRegistry_Attach(SZrGlobalState *global) {
    ZrLibrary_NativeRegistryState *registry;
    SZrState *state;

    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return ZR_FALSE;
    }

    if (native_registry_get(global) != ZR_NULL) {
        return ZR_TRUE;
    }

    state = global->mainThreadState;
    registry = (ZrLibrary_NativeRegistryState *)global->allocator(global->userAllocationArguments,
                                                                  ZR_NULL,
                                                                  0,
                                                                  sizeof(ZrLibrary_NativeRegistryState),
                                                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (registry == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(registry, 0, sizeof(*registry));
    ZrCore_Array_Construct(&registry->moduleRecords);
    ZrCore_Array_Construct(&registry->bindingEntries);
    ZrCore_Array_Construct(&registry->pluginHandles);
    ZrCore_Array_Init(state,
                      &registry->moduleRecords,
                      sizeof(ZrLibRegisteredModuleRecord),
                      ZR_LIBRARY_NATIVE_MODULE_RECORDS_INITIAL_CAPACITY);
    ZrCore_Array_Init(state,
                      &registry->bindingEntries,
                      sizeof(ZrLibBindingEntry),
                      ZR_LIBRARY_NATIVE_BINDING_ENTRIES_INITIAL_CAPACITY);
    ZrCore_Array_Init(state,
                      &registry->pluginHandles,
                      sizeof(void *),
                      ZR_LIBRARY_NATIVE_PLUGIN_HANDLES_INITIAL_CAPACITY);
    native_registry_clear_error(registry);

    ZrCore_GlobalState_SetNativeModuleLoader(global, native_registry_loader, registry);
    return ZR_TRUE;
}

void ZrLibrary_NativeRegistry_Free(SZrGlobalState *global) {
    ZrLibrary_NativeRegistryState *registry;
    SZrState *state;
    TZrSize index;

    if (global == ZR_NULL) {
        return;
    }

    registry = native_registry_get(global);
    if (registry == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return;
    }

    state = global->mainThreadState;
    if (registry->pluginHandles.isValid) {
        for (index = 0; index < registry->pluginHandles.length; index++) {
            void **handle = (void **)ZrCore_Array_Get(&registry->pluginHandles, index);
            if (handle != ZR_NULL && *handle != ZR_NULL) {
                native_registry_close_library(*handle);
            }
        }
    }

    if (registry->moduleRecords.isValid) {
        for (index = 0; index < registry->moduleRecords.length; index++) {
            ZrLibRegisteredModuleRecord *record =
                    (ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
            if (record != ZR_NULL && record->sourcePath != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record->sourcePath,
                                  strlen(record->sourcePath) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                record->sourcePath = ZR_NULL;
            }
        }
    }

    ZrCore_Array_Free(state, &registry->pluginHandles);
    ZrCore_Array_Free(state, &registry->bindingEntries);
    ZrCore_Array_Free(state, &registry->moduleRecords);

    global->allocator(global->userAllocationArguments,
                      registry,
                      sizeof(ZrLibrary_NativeRegistryState),
                      0,
                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    ZrCore_GlobalState_SetNativeModuleLoader(global, ZR_NULL, ZR_NULL);
}

TZrBool ZrLibrary_NativeRegistry_RegisterModule(SZrGlobalState *global, const ZrLibModuleDescriptor *descriptor) {
    return native_registry_register_module_record(global,
                                                  descriptor,
                                                  ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_BUILTIN,
                                                  descriptor != ZR_NULL ? descriptor->moduleName : ZR_NULL,
                                                  ZR_FALSE);
}

const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModule(SZrGlobalState *global, const TZrChar *moduleName) {
    ZrLibrary_NativeRegistryState *registry;
    const ZrLibRegisteredModuleRecord *record;

    if (global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    registry = native_registry_get(global);
    if (registry == ZR_NULL || !registry->moduleRecords.isValid) {
        return ZR_NULL;
    }

    record = native_registry_find_record(registry, moduleName);
    return record != ZR_NULL ? record->descriptor : ZR_NULL;
}

TZrBool ZrLibrary_NativeRegistry_GetModuleInfo(SZrGlobalState *global,
                                               const TZrChar *moduleName,
                                               ZrLibRegisteredModuleInfo *outInfo) {
    ZrLibrary_NativeRegistryState *registry;
    const ZrLibRegisteredModuleRecord *record;

    if (outInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outInfo, 0, sizeof(*outInfo));
    if (global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    registry = native_registry_get(global);
    record = native_registry_find_record(registry, moduleName);
    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    outInfo->descriptor = record->descriptor;
    outInfo->sourcePath = record->sourcePath;
    outInfo->registrationKind = record->registrationKind;
    outInfo->isDescriptorPlugin = record->isDescriptorPlugin;
    return ZR_TRUE;
}

EZrLibNativeRegistryErrorCode ZrLibrary_NativeRegistry_GetLastErrorCode(SZrGlobalState *global) {
    ZrLibrary_NativeRegistryState *registry = native_registry_get(global);
    return registry != ZR_NULL ? registry->lastErrorCode : ZR_LIB_NATIVE_REGISTRY_ERROR_NONE;
}

const TZrChar *ZrLibrary_NativeRegistry_GetLastErrorMessage(SZrGlobalState *global) {
    ZrLibrary_NativeRegistryState *registry = native_registry_get(global);
    return registry != ZR_NULL ? registry->lastErrorMessage : ZR_NULL;
}

const TZrChar *ZrLibrary_NativeHints_GetSchemaId(void) {
    return ZR_VM_NATIVE_HINTS_SCHEMA_ID;
}

const TZrChar *ZrLibrary_NativeHints_GetModuleJson(const ZrLibModuleDescriptor *descriptor) {
    return descriptor != ZR_NULL ? descriptor->typeHintsJson : ZR_NULL;
}

TZrBool ZrLibrary_NativeHints_WriteSidecar(const ZrLibModuleDescriptor *descriptor, const TZrChar *outputPath) {
    FILE *file;

    if (descriptor == ZR_NULL || outputPath == ZR_NULL || descriptor->typeHintsJson == ZR_NULL) {
        return ZR_FALSE;
    }

    file = fopen(outputPath, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    fwrite(descriptor->typeHintsJson, 1, strlen(descriptor->typeHintsJson), file);
    fclose(file);
    return ZR_TRUE;
}
