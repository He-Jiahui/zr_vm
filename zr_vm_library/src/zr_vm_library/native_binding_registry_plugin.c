#include "native_binding_internal.h"

void *native_registry_open_library(const TZrChar *path) {
    if (path == ZR_NULL) {
        return ZR_NULL;
    }
#if defined(ZR_PLATFORM_WIN)
    return (void *)LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

void native_registry_close_library(void *handle) {
    if (handle == ZR_NULL) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    FreeLibrary((HMODULE)handle);
#else
    dlclose(handle);
#endif
}

TZrPtr native_registry_find_symbol(void *handle, const TZrChar *symbolName) {
    if (handle == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_NULL;
    }
#if defined(ZR_PLATFORM_WIN)
    return (TZrPtr)GetProcAddress((HMODULE)handle, symbolName);
#else
    return (TZrPtr)dlsym(handle, symbolName);
#endif
}

const TZrChar *native_registry_dynamic_library_extension(void) {
#if defined(ZR_PLATFORM_WIN)
    return ".dll";
#elif defined(ZR_PLATFORM_DARWIN)
    return ".dylib";
#else
    return ".so";
#endif
}

static FZrVmGetNativeModuleV1 native_registry_cast_module_symbol(TZrPtr symbolPointer) {
    FZrVmGetNativeModuleV1 symbol = ZR_NULL;
    if (symbolPointer != ZR_NULL) {
        memcpy(&symbol, &symbolPointer, sizeof(symbol));
    }
    return symbol;
}

TZrBool native_registry_get_executable_directory(TZrChar *buffer, TZrSize bufferSize) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    {
        DWORD length = GetModuleFileNameA(ZR_NULL, buffer, (DWORD)bufferSize);
        if (length == 0 || length >= bufferSize) {
            return ZR_FALSE;
        }
    }
#elif defined(ZR_PLATFORM_DARWIN)
    {
        uint32_t size = (uint32_t)bufferSize;
        if (_NSGetExecutablePath(buffer, &size) != 0) {
            return ZR_FALSE;
        }
    }
#else
    {
        ssize_t length = readlink("/proc/self/exe", buffer, bufferSize - 1);
        if (length <= 0 || (TZrSize)length >= bufferSize) {
            return ZR_FALSE;
        }
        buffer[length] = '\0';
    }
#endif

    {
        TZrSize length = ZrCore_NativeString_Length(buffer);
        while (length > 0) {
            TZrChar current = buffer[length - 1];
            if (current == '/' || current == '\\') {
                buffer[length - 1] = '\0';
                return ZR_TRUE;
            }
            length--;
        }
    }

    return ZR_FALSE;
}

void native_registry_sanitize_module_name(const TZrChar *moduleName,
                                                 TZrChar *buffer,
                                                 TZrSize bufferSize) {
    TZrSize index;
    TZrSize cursor = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (moduleName == ZR_NULL) {
        return;
    }

    for (index = 0; moduleName[index] != '\0' && cursor + 1 < bufferSize; index++) {
        TZrChar current = moduleName[index];
        buffer[cursor++] = (TZrChar)(isalnum((unsigned char)current) ? current : '_');
    }
    buffer[cursor] = '\0';
}

const ZrLibModuleDescriptor *native_registry_load_plugin_descriptor(SZrState *state,
                                                                           ZrLibrary_NativeRegistryState *registry,
                                                                           const TZrChar *candidatePath,
                                                                           const TZrChar *requestedModuleName) {
    void *handle;
    FZrVmGetNativeModuleV1 symbol;
    const ZrLibModuleDescriptor *descriptor;

    if (state == ZR_NULL || registry == ZR_NULL || candidatePath == ZR_NULL || requestedModuleName == ZR_NULL) {
        return ZR_NULL;
    }

    handle = native_registry_open_library(candidatePath);
    if (handle == ZR_NULL) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD,
                                  "failed to load native plugin '%s'",
                                  candidatePath);
        return ZR_NULL;
    }

    symbol = native_registry_cast_module_symbol(native_registry_find_symbol(handle, "ZrVm_GetNativeModule_v1"));
    if (symbol == ZR_NULL) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_SYMBOL,
                                  "native plugin '%s' does not export ZrVm_GetNativeModule_v1",
                                  candidatePath);
        native_registry_close_library(handle);
        return ZR_NULL;
    }

    descriptor = symbol();
    if (descriptor == ZR_NULL) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD,
                                  "native plugin '%s' returned a null module descriptor",
                                  candidatePath);
        native_registry_close_library(handle);
        return ZR_NULL;
    }

    if (descriptor->abiVersion != ZR_VM_NATIVE_PLUGIN_ABI_VERSION) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_ABI_MISMATCH,
                                  "native plugin '%s' uses ABI %u but runtime expects %u",
                                  candidatePath,
                                  descriptor->abiVersion,
                                  ZR_VM_NATIVE_PLUGIN_ABI_VERSION);
        native_registry_close_library(handle);
        return ZR_NULL;
    }

    if (descriptor->moduleName == ZR_NULL ||
        strcmp(descriptor->moduleName, requestedModuleName) != 0) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_NAME_MISMATCH,
                                  "native plugin '%s' exports module '%s' but '%s' was requested",
                                  candidatePath,
                                  descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>",
                                  requestedModuleName);
        native_registry_close_library(handle);
        return ZR_NULL;
    }

    if (!native_registry_validate_descriptor_compatibility(registry, descriptor) ||
        !native_registry_register_module_record(state->global,
                                               descriptor,
                                               ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN,
                                               candidatePath,
                                               ZR_TRUE)) {
        native_registry_close_library(handle);
        return ZR_NULL;
    }

    ZrCore_Array_Push(state, &registry->pluginHandles, &handle);
    native_registry_clear_error(registry);
    return descriptor;
}

const ZrLibModuleDescriptor *native_registry_try_plugin_directory(SZrState *state,
                                                                         ZrLibrary_NativeRegistryState *registry,
                                                                         const TZrChar *directory,
                                                                         const TZrChar *moduleName) {
    TZrChar sanitizedModuleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar pluginFileName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar candidatePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *extension;
    TZrSize prefixLength;
    TZrSize moduleNameLength;
    TZrSize extensionLength;
    TZrSize totalLength;

    if (directory == ZR_NULL || moduleName == ZR_NULL || directory[0] == '\0') {
        return ZR_NULL;
    }

    native_registry_sanitize_module_name(moduleName, sanitizedModuleName, sizeof(sanitizedModuleName));
    extension = native_registry_dynamic_library_extension();
    prefixLength = sizeof("zrvm_native_") - 1;
    moduleNameLength = ZrCore_NativeString_Length(sanitizedModuleName);
    extensionLength = ZrCore_NativeString_Length((TZrNativeString)extension);
    totalLength = prefixLength + moduleNameLength + extensionLength;
    if (totalLength + 1 > sizeof(pluginFileName)) {
        return ZR_NULL;
    }
    memcpy(pluginFileName, "zrvm_native_", prefixLength);
    memcpy(pluginFileName + prefixLength, sanitizedModuleName, moduleNameLength);
    memcpy(pluginFileName + prefixLength + moduleNameLength, extension, extensionLength);
    pluginFileName[totalLength] = '\0';
    ZrLibrary_File_PathJoin((TZrNativeString)directory, pluginFileName, candidatePath);

    if (ZrLibrary_File_Exist(candidatePath) != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_NULL;
    }

    return native_registry_load_plugin_descriptor(state, registry, candidatePath, moduleName);
}

const ZrLibModuleDescriptor *native_registry_try_plugin_paths(SZrState *state,
                                                                     ZrLibrary_NativeRegistryState *registry,
                                                                     const TZrChar *moduleName) {
    TZrChar nativeDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar executableDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *envPaths;

    if (state == ZR_NULL || registry == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global != ZR_NULL && state->global->userData != ZR_NULL) {
        SZrLibrary_Project *project = (SZrLibrary_Project *)state->global->userData;
        if (project != ZR_NULL && project->directory != ZR_NULL) {
            ZrLibrary_File_PathJoin(ZrCore_String_GetNativeString(project->directory), "native", nativeDirectory);
            {
                const ZrLibModuleDescriptor *descriptor =
                        native_registry_try_plugin_directory(state, registry, nativeDirectory, moduleName);
                if (descriptor != ZR_NULL) {
                    return descriptor;
                }
            }
        }
    }

    if (native_registry_get_executable_directory(executableDirectory, sizeof(executableDirectory))) {
        ZrLibrary_File_PathJoin(executableDirectory, "native", nativeDirectory);
        {
            const ZrLibModuleDescriptor *descriptor =
                    native_registry_try_plugin_directory(state, registry, nativeDirectory, moduleName);
            if (descriptor != ZR_NULL) {
                return descriptor;
            }
        }
    }

    envPaths = getenv("ZR_VM_NATIVE_PATH");
    if (envPaths != ZR_NULL && envPaths[0] != '\0') {
        TZrChar pathBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
        TZrSize cursor = 0;
        TZrSize index = 0;
#if defined(ZR_PLATFORM_WIN)
        const TZrChar separator = ';';
#else
        const TZrChar separator = ':';
#endif
        while (envPaths[index] != '\0') {
            if (envPaths[index] == separator) {
                pathBuffer[cursor] = '\0';
                {
                    const ZrLibModuleDescriptor *descriptor =
                            native_registry_try_plugin_directory(state, registry, pathBuffer, moduleName);
                    if (descriptor != ZR_NULL) {
                        return descriptor;
                    }
                }
                cursor = 0;
            } else if (cursor + 1 < sizeof(pathBuffer)) {
                pathBuffer[cursor++] = envPaths[index];
            }
            index++;
        }

        if (cursor > 0) {
            pathBuffer[cursor] = '\0';
            return native_registry_try_plugin_directory(state, registry, pathBuffer, moduleName);
        }
    }

    return ZR_NULL;
}

const ZrLibModuleDescriptor *native_registry_find_descriptor_or_plugin(SZrState *state,
                                                                              ZrLibrary_NativeRegistryState *registry,
                                                                              const TZrChar *moduleName) {
    const ZrLibModuleDescriptor *descriptor;

    if (state == ZR_NULL || registry == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    descriptor = ZrLibrary_NativeRegistry_FindModule(state->global, moduleName);
    if (descriptor == ZR_NULL) {
        descriptor = native_registry_try_plugin_paths(state, registry, moduleName);
    }

    return descriptor;
}

SZrObjectModule *native_registry_resolve_loaded_module(SZrState *state,
                                                              ZrLibrary_NativeRegistryState *registry,
                                                              const TZrChar *moduleName) {
    SZrString *moduleNameString;
    SZrObjectModule *module;
    const ZrLibModuleDescriptor *descriptor;

    if (state == ZR_NULL || registry == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    moduleNameString = native_binding_create_string(state, moduleName);
    if (moduleNameString == ZR_NULL) {
        return ZR_NULL;
    }

    module = ZrCore_Module_GetFromCache(state, moduleNameString);
    if (module != ZR_NULL) {
        return module;
    }

    descriptor = native_registry_find_descriptor_or_plugin(state, registry, moduleName);
    if (descriptor == ZR_NULL) {
        return ZR_NULL;
    }

    module = native_registry_materialize_module(state, registry, descriptor);
    if (module != ZR_NULL) {
        ZrCore_Module_AddToCache(state, moduleNameString, module);
    }

    return module;
}

TZrBool native_binding_make_callable_value(SZrState *state,
                                                  ZrLibrary_NativeRegistryState *registry,
                                                  EZrLibResolvedBindingKind bindingKind,
                                                  const ZrLibModuleDescriptor *moduleDescriptor,
                                                  const ZrLibTypeDescriptor *typeDescriptor,
                                                  SZrObjectPrototype *ownerPrototype,
                                                  const void *descriptor,
                                                  SZrTypeValue *value) {
    SZrClosureNative *closure;
    ZrLibBindingEntry entry;

    if (state == ZR_NULL || registry == ZR_NULL || moduleDescriptor == ZR_NULL || descriptor == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    closure = ZrCore_ClosureNative_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->nativeFunction = native_binding_dispatcher;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));

    entry.closure = closure;
    entry.bindingKind = bindingKind;
    entry.moduleDescriptor = moduleDescriptor;
    entry.typeDescriptor = typeDescriptor;
    entry.ownerPrototype = ownerPrototype;
    switch (bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION:
            entry.descriptor.functionDescriptor = (const ZrLibFunctionDescriptor *)descriptor;
            break;
        case ZR_LIB_RESOLVED_BINDING_METHOD:
            entry.descriptor.methodDescriptor = (const ZrLibMethodDescriptor *)descriptor;
            break;
        case ZR_LIB_RESOLVED_BINDING_META_METHOD:
            entry.descriptor.metaMethodDescriptor = (const ZrLibMetaMethodDescriptor *)descriptor;
            break;
        default:
            return ZR_FALSE;
    }

    ZrCore_Array_Push(state, &registry->bindingEntries, &entry);

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value->isNative = ZR_TRUE;
    return ZR_TRUE;
}

TZrStackValuePointer native_binding_resolve_call_scratch_base(TZrStackValuePointer stackTop,
                                                                     const SZrCallInfo *callInfo) {
    TZrStackValuePointer base = stackTop;

    if (callInfo != ZR_NULL && callInfo->functionTop.valuePointer > base) {
        base = callInfo->functionTop.valuePointer;
    }

    return base;
}

SZrObject *native_binding_new_instance_with_prototype(SZrState *state, SZrObjectPrototype *prototype) {
    SZrObject *object;
    EZrObjectInternalType internalType;

    if (state == ZR_NULL || prototype == ZR_NULL) {
        return ZR_NULL;
    }

    internalType = prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                           ? ZR_OBJECT_INTERNAL_TYPE_STRUCT
                           : ZR_OBJECT_INTERNAL_TYPE_OBJECT;
    object = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), internalType);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    object->prototype = prototype;
    ZrCore_Object_Init(state, object);
    return object;
}

SZrObjectModule *native_registry_materialize_module(SZrState *state,
                                                           ZrLibrary_NativeRegistryState *registry,
                                                           const ZrLibModuleDescriptor *descriptor) {
    SZrObjectModule *module;
    SZrString *moduleName;
    SZrString *moduleInfoName;
    SZrObject *moduleInfo;
    SZrTypeValue moduleInfoValue;
    TZrUInt64 pathHash;
    TZrSize index;

    if (state == ZR_NULL || registry == ZR_NULL || descriptor == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    native_binding_trace_import("[zr_native_import] materialize start module=%s types=%llu consts=%llu funcs=%llu\n",
                                descriptor->moduleName,
                                (unsigned long long)descriptor->typeCount,
                                (unsigned long long)descriptor->constantCount,
                                (unsigned long long)descriptor->functionCount);

    module = ZrCore_Module_Create(state);
    if (module == ZR_NULL) {
        native_binding_trace_import("[zr_native_import] materialize failed module=%s reason=create_module\n",
                                    descriptor->moduleName);
        return ZR_NULL;
    }
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module));

    moduleName = native_binding_create_string(state, descriptor->moduleName);
    if (moduleName == ZR_NULL) {
        native_binding_trace_import("[zr_native_import] materialize failed module=%s reason=create_module_name\n",
                                    descriptor->moduleName);
        return ZR_NULL;
    }

    pathHash = ZrCore_Module_CalculatePathHash(state, moduleName);
    ZrCore_Module_SetInfo(state, module, moduleName, pathHash, moduleName);

    for (index = 0; index < descriptor->typeCount; index++) {
        if (!native_registry_add_type(state, registry, module, descriptor, &descriptor->types[index])) {
            native_binding_trace_import("[zr_native_import] materialize failed module=%s reason=register_type index=%llu name=%s\n",
                                        descriptor->moduleName,
                                        (unsigned long long)index,
                                        descriptor->types[index].name != ZR_NULL ? descriptor->types[index].name : "<null>");
            return ZR_NULL;
        }
    }

    native_registry_resolve_type_relationships(state, module, descriptor);

    for (index = 0; index < descriptor->constantCount; index++) {
        if (!native_registry_add_constant(state, module, &descriptor->constants[index])) {
            native_binding_trace_import("[zr_native_import] materialize failed module=%s reason=register_constant index=%llu name=%s\n",
                                        descriptor->moduleName,
                                        (unsigned long long)index,
                                        descriptor->constants[index].name != ZR_NULL ? descriptor->constants[index].name : "<null>");
            return ZR_NULL;
        }
    }

    for (index = 0; index < descriptor->functionCount; index++) {
        if (!native_registry_add_function(state, registry, module, descriptor, &descriptor->functions[index])) {
            native_binding_trace_import("[zr_native_import] materialize failed module=%s reason=register_function index=%llu name=%s\n",
                                        descriptor->moduleName,
                                        (unsigned long long)index,
                                        descriptor->functions[index].name != ZR_NULL ? descriptor->functions[index].name : "<null>");
            return ZR_NULL;
        }
    }

    for (index = 0; index < descriptor->moduleLinkCount; index++) {
        if (!native_registry_add_module_link(state, registry, module, &descriptor->moduleLinks[index])) {
            native_binding_trace_import("[zr_native_import] materialize failed module=%s reason=register_module_link index=%llu name=%s target=%s\n",
                                        descriptor->moduleName,
                                        (unsigned long long)index,
                                        descriptor->moduleLinks[index].name != ZR_NULL ? descriptor->moduleLinks[index].name : "<null>",
                                        descriptor->moduleLinks[index].moduleName != ZR_NULL ? descriptor->moduleLinks[index].moduleName : "<null>");
            return ZR_NULL;
        }
    }

    moduleInfo = native_metadata_make_module_info(state,
                                                  descriptor,
                                                  native_registry_find_record_by_descriptor(registry, descriptor));
    moduleInfoName = native_binding_create_string(state, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
    if (moduleInfo == ZR_NULL || moduleInfoName == ZR_NULL) {
        native_binding_trace_import("[zr_native_import] materialize failed module=%s reason=module_info_export\n",
                                    descriptor->moduleName);
        return ZR_NULL;
    }
    ZrLib_Value_SetObject(state, &moduleInfoValue, moduleInfo, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Module_AddPubExport(state, module, moduleInfoName, &moduleInfoValue);

    native_binding_trace_import("[zr_native_import] materialize success module=%s module=%p\n",
                                descriptor->moduleName,
                                (void *)module);
    return module;
}

struct SZrObjectModule *native_registry_loader(SZrState *state, SZrString *moduleName, TZrPtr userData) {
    ZrLibrary_NativeRegistryState *registry = (ZrLibrary_NativeRegistryState *)userData;
    const TZrChar *nativeModuleName;
    const ZrLibModuleDescriptor *descriptor;

    if (state == ZR_NULL || moduleName == ZR_NULL || registry == ZR_NULL) {
        return ZR_NULL;
    }

    nativeModuleName = ZrCore_String_GetNativeString(moduleName);
    if (nativeModuleName == ZR_NULL) {
        return ZR_NULL;
    }

    native_binding_trace_import("[zr_native_import] loader request module=%s\n", nativeModuleName);

    descriptor = native_registry_find_descriptor_or_plugin(state, registry, nativeModuleName);

    if (descriptor == ZR_NULL) {
        native_binding_trace_import("[zr_native_import] loader miss module=%s\n", nativeModuleName);
        return ZR_NULL;
    }

    native_binding_trace_import("[zr_native_import] loader hit module=%s descriptor=%p\n",
                                nativeModuleName,
                                (const void *)descriptor);
    return native_registry_materialize_module(state, registry, descriptor);
}
