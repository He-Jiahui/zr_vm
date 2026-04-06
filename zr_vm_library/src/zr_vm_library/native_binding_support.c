#include "native_binding_internal.h"

#include "zr_vm_core/log.h"

TZrBool native_binding_trace_import_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_NATIVE_IMPORT");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

void native_binding_init_call_context_layout(ZrLibCallContext *context,
                                                    TZrStackValuePointer functionBase,
                                                    TZrSize rawArgumentCount) {
    TZrBool usesReceiver;

    if (context == ZR_NULL) {
        return;
    }

    usesReceiver = ZR_FALSE;
    if (context->methodDescriptor != ZR_NULL) {
        usesReceiver = !context->methodDescriptor->isStatic;
    } else if (context->metaMethodDescriptor != ZR_NULL) {
        usesReceiver = ZR_TRUE;
    }

    if (!usesReceiver) {
        context->argumentBase = functionBase + 1;
        context->argumentValues = ZR_NULL;
        context->argumentCount = rawArgumentCount;
        context->selfValue = ZR_NULL;
        return;
    }

    context->selfValue = rawArgumentCount > 0 ? ZrCore_Stack_GetValue(functionBase + 1) : ZR_NULL;
    context->argumentBase = rawArgumentCount > 0 ? functionBase + 2 : functionBase + 1;
    context->argumentValues = ZR_NULL;
    context->argumentCount = rawArgumentCount > 0 ? rawArgumentCount - 1 : 0;
}

void native_binding_trace_import(SZrState *state, const TZrChar *format, ...) {
    va_list arguments;
    va_list copyArguments;
    int requiredLength;
    TZrChar stackBuffer[512];
    TZrChar *heapBuffer = ZR_NULL;
    TZrNativeString message = stackBuffer;

    if (!native_binding_trace_import_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    va_copy(copyArguments, arguments);
    requiredLength = vsnprintf(stackBuffer, sizeof(stackBuffer), format, copyArguments);
    va_end(copyArguments);
    if (requiredLength < 0) {
        va_end(arguments);
        return;
    }
    if ((size_t)requiredLength >= sizeof(stackBuffer)) {
        heapBuffer = (TZrChar *)malloc((size_t)requiredLength + 1u);
        if (heapBuffer == ZR_NULL) {
            va_end(arguments);
            return;
        }
        vsnprintf(heapBuffer, (size_t)requiredLength + 1u, format, arguments);
        message = heapBuffer;
    }
    va_end(arguments);
    ZrCore_Log_Write(state, ZR_LOG_LEVEL_DEBUG, ZR_OUTPUT_CHANNEL_STDERR, ZR_OUTPUT_KIND_DIAGNOSTIC, message);
    if (heapBuffer != ZR_NULL) {
        free(heapBuffer);
    }
}

void native_registry_clear_error(ZrLibrary_NativeRegistryState *registry) {
    if (registry == ZR_NULL) {
        return;
    }

    registry->lastErrorCode = ZR_LIB_NATIVE_REGISTRY_ERROR_NONE;
    registry->lastErrorMessage[0] = '\0';
}

void native_registry_set_error(ZrLibrary_NativeRegistryState *registry,
                                      EZrLibNativeRegistryErrorCode errorCode,
                                      const TZrChar *format,
                                      ...) {
    va_list arguments;

    if (registry == ZR_NULL) {
        return;
    }

    registry->lastErrorCode = errorCode;
    registry->lastErrorMessage[0] = '\0';
    if (format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    vsnprintf(registry->lastErrorMessage, sizeof(registry->lastErrorMessage), format, arguments);
    va_end(arguments);
}

TZrChar *native_registry_duplicate_string(SZrGlobalState *global, const TZrChar *text) {
    TZrSize length;
    TZrChar *copy;

    if (global == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    length = strlen(text);
    copy = (TZrChar *)global->allocator(global->userAllocationArguments,
                                        ZR_NULL,
                                        0,
                                        length + 1,
                                        ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

const TZrChar *native_binding_value_type_name(SZrState *state, const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return "null";
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            return "null";
        case ZR_VALUE_TYPE_BOOL:
            return "bool";
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return "int";
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return "float";
        case ZR_VALUE_TYPE_STRING:
            return "string";
        case ZR_VALUE_TYPE_ARRAY:
            return "array";
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            return "function";
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            return "nativePointer";
        case ZR_VALUE_TYPE_OBJECT: {
            ZR_UNUSED_PARAMETER(state);
            return "object";
        }
        default:
            return "value";
    }
}

const TZrChar *native_binding_call_name(const ZrLibCallContext *context) {
    if (context == ZR_NULL) {
        return "native";
    }
    if (context->functionDescriptor != ZR_NULL && context->functionDescriptor->name != ZR_NULL) {
        return context->functionDescriptor->name;
    }
    if (context->methodDescriptor != ZR_NULL && context->methodDescriptor->name != ZR_NULL) {
        return context->methodDescriptor->name;
    }
    if (context->metaMethodDescriptor != ZR_NULL &&
        context->metaMethodDescriptor->metaType < ZR_META_ENUM_MAX) {
        return CZrMetaName[context->metaMethodDescriptor->metaType];
    }
    return "native";
}

SZrString *native_binding_create_string(SZrState *state, const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_CreateTryHitCache(state, (TZrNativeString)text);
}

SZrObjectModule *native_binding_import_module(SZrState *state, const TZrChar *moduleName) {
    SZrString *moduleNameString;

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    moduleNameString = native_binding_create_string(state, moduleName);
    if (moduleNameString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_ImportByPath(state, moduleNameString);
}

void native_binding_register_prototype_in_global_scope(SZrState *state,
                                                              SZrString *typeName,
                                                              const SZrTypeValue *prototypeValue) {
    SZrObject *zrObject;
    SZrTypeValue key;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL || prototypeValue == ZR_NULL) {
        return;
    }

    if (state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return;
    }

    zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (zrObject == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, zrObject, &key, prototypeValue);
}

ZrLibrary_NativeRegistryState *native_registry_get(SZrGlobalState *global) {
    if (global == ZR_NULL || global->nativeModuleLoader != native_registry_loader) {
        return ZR_NULL;
    }
    return (ZrLibrary_NativeRegistryState *)global->nativeModuleLoaderUserData;
}

ZrLibBindingEntry *native_registry_find_binding(ZrLibrary_NativeRegistryState *registry,
                                                       SZrClosureNative *closure) {
    TZrSize index;

    if (registry == ZR_NULL || closure == ZR_NULL || !registry->bindingEntries.isValid) {
        return ZR_NULL;
    }

    for (index = 0; index < registry->bindingEntries.length; index++) {
        ZrLibBindingEntry *entry = (ZrLibBindingEntry *)ZrCore_Array_Get(&registry->bindingEntries, index);
        if (entry != ZR_NULL && entry->closure == closure) {
            return entry;
        }
    }

    return ZR_NULL;
}

const ZrLibRegisteredModuleRecord *native_registry_find_record(ZrLibrary_NativeRegistryState *registry,
                                                                      const TZrChar *moduleName) {
    TZrSize index;

    if (registry == ZR_NULL || moduleName == ZR_NULL || !registry->moduleRecords.isValid) {
        return ZR_NULL;
    }

    for (index = 0; index < registry->moduleRecords.length; index++) {
        const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
        if (record != ZR_NULL &&
            record->moduleName != ZR_NULL &&
            strcmp(record->moduleName, moduleName) == 0) {
            return record;
        }
    }

    return ZR_NULL;
}

const ZrLibRegisteredModuleRecord *native_registry_find_record_by_descriptor(
        ZrLibrary_NativeRegistryState *registry,
        const ZrLibModuleDescriptor *descriptor) {
    TZrSize index;

    if (registry == ZR_NULL || descriptor == ZR_NULL || !registry->moduleRecords.isValid) {
        return ZR_NULL;
    }

    for (index = 0; index < registry->moduleRecords.length; index++) {
        const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
        if (record != ZR_NULL && record->descriptor == descriptor) {
            return record;
        }
    }

    return ZR_NULL;
}

TZrBool native_registry_validate_descriptor_compatibility(ZrLibrary_NativeRegistryState *registry,
                                                                 const ZrLibModuleDescriptor *descriptor) {
    TZrUInt32 minimumRuntimeAbi;

    if (descriptor == ZR_NULL) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD,
                                  "native descriptor is null");
        return ZR_FALSE;
    }

    minimumRuntimeAbi = descriptor->minRuntimeAbi != 0 ? descriptor->minRuntimeAbi : ZR_VM_NATIVE_RUNTIME_ABI_VERSION;
    if (minimumRuntimeAbi > ZR_VM_NATIVE_RUNTIME_ABI_VERSION) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_VERSION_MISMATCH,
                                  "module '%s' requires runtime ABI %u but runtime provides %u",
                                  descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>",
                                  minimumRuntimeAbi,
                                  ZR_VM_NATIVE_RUNTIME_ABI_VERSION);
        return ZR_FALSE;
    }

    if ((descriptor->requiredCapabilities & ~ZR_VM_NATIVE_RUNTIME_CAPABILITIES) != 0) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_CAPABILITY_MISMATCH,
                                  "module '%s' requires unsupported capabilities 0x%llx",
                                  descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>",
                                  (unsigned long long)(descriptor->requiredCapabilities & ~ZR_VM_NATIVE_RUNTIME_CAPABILITIES));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool native_registry_register_module_record(SZrGlobalState *global,
                                                      const ZrLibModuleDescriptor *descriptor,
                                                      EZrLibNativeModuleRegistrationKind registrationKind,
                                                      const TZrChar *sourcePath,
                                                      TZrBool isDescriptorPlugin) {
    ZrLibrary_NativeRegistryState *registry;
    TZrSize index;

    if (global == ZR_NULL || descriptor == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        return ZR_FALSE;
    }

    registry = native_registry_get(global);
    if (registry == ZR_NULL || !native_registry_validate_descriptor_compatibility(registry, descriptor)) {
        return ZR_FALSE;
    }

    for (index = 0; index < registry->moduleRecords.length; index++) {
        ZrLibRegisteredModuleRecord *record =
                (ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
        if (record != ZR_NULL &&
            record->moduleName != ZR_NULL &&
            strcmp(record->moduleName, descriptor->moduleName) == 0) {
            TZrChar *moduleNameCopy = native_registry_duplicate_string(global, descriptor->moduleName);
            TZrChar *sourcePathCopy = native_registry_duplicate_string(global, sourcePath);

            if (moduleNameCopy == ZR_NULL || sourcePathCopy == ZR_NULL) {
                if (moduleNameCopy != ZR_NULL) {
                    global->allocator(global->userAllocationArguments,
                                      moduleNameCopy,
                                      strlen(moduleNameCopy) + 1,
                                      0,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                }
                if (sourcePathCopy != ZR_NULL) {
                    global->allocator(global->userAllocationArguments,
                                      sourcePathCopy,
                                      strlen(sourcePathCopy) + 1,
                                      0,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                }
                return ZR_FALSE;
            }

            if (record->moduleName != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record->moduleName,
                                  strlen(record->moduleName) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            if (record->sourcePath != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record->sourcePath,
                                  strlen(record->sourcePath) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            record->descriptor = descriptor;
            record->moduleName = moduleNameCopy;
            record->registrationKind = registrationKind;
            record->isDescriptorPlugin = isDescriptorPlugin;
            record->sourcePath = sourcePathCopy;
            native_registry_clear_error(registry);
            return ZR_TRUE;
        }
    }

    {
        ZrLibRegisteredModuleRecord record;
        memset(&record, 0, sizeof(record));
        record.descriptor = descriptor;
        record.moduleName = native_registry_duplicate_string(global, descriptor->moduleName);
        record.registrationKind = registrationKind;
        record.isDescriptorPlugin = isDescriptorPlugin;
        record.sourcePath = native_registry_duplicate_string(global, sourcePath);
        if (record.moduleName == ZR_NULL || record.sourcePath == ZR_NULL) {
            if (record.moduleName != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record.moduleName,
                                  strlen(record.moduleName) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            if (record.sourcePath != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record.sourcePath,
                                  strlen(record.sourcePath) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            return ZR_FALSE;
        }
        ZrCore_Array_Push(global->mainThreadState, &registry->moduleRecords, &record);
    }

    native_registry_clear_error(registry);
    return ZR_TRUE;
}
