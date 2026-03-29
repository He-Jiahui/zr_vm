//
// Descriptor-driven native binding implementation.
//

#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_library/native_binding.h"

#include "zr_vm_library/file.h"
#include "zr_vm_library/native_hints.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#include <direct.h>
#elif defined(ZR_PLATFORM_DARWIN)
#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef enum EZrLibResolvedBindingKind {
    ZR_LIB_RESOLVED_BINDING_FUNCTION = 0,
    ZR_LIB_RESOLVED_BINDING_METHOD = 1,
    ZR_LIB_RESOLVED_BINDING_META_METHOD = 2
} EZrLibResolvedBindingKind;

typedef struct ZrLibBindingEntry {
    SZrClosureNative *closure;
    EZrLibResolvedBindingKind bindingKind;
    const ZrLibModuleDescriptor *moduleDescriptor;
    const ZrLibTypeDescriptor *typeDescriptor;
    union {
        const ZrLibFunctionDescriptor *functionDescriptor;
        const ZrLibMethodDescriptor *methodDescriptor;
        const ZrLibMetaMethodDescriptor *metaMethodDescriptor;
    } descriptor;
} ZrLibBindingEntry;

typedef struct ZrLibRegisteredModuleRecord {
    const ZrLibModuleDescriptor *descriptor;
    TZrChar *sourcePath;
    EZrLibNativeModuleRegistrationKind registrationKind;
    TZrBool isDescriptorPlugin;
} ZrLibRegisteredModuleRecord;

typedef struct ZrLibrary_NativeRegistryState {
    SZrArray moduleRecords;
    SZrArray bindingEntries;
    SZrArray pluginHandles;
    EZrLibNativeRegistryErrorCode lastErrorCode;
    TZrChar lastErrorMessage[512];
} ZrLibrary_NativeRegistryState;

static const TZrChar *kNativeEnumValueFieldName = "__zr_enumValue";
static const TZrChar *kNativeEnumNameFieldName = "__zr_enumName";
static const TZrChar *kNativeEnumValueTypeFieldName = "__zr_enumValueTypeName";
static const TZrChar *kNativeAllowValueConstructionFieldName = "__zr_allowValueConstruction";
static const TZrChar *kNativeAllowBoxedConstructionFieldName = "__zr_allowBoxedConstruction";

typedef const ZrLibModuleDescriptor *(*FZrVmGetNativeModuleV1)(void);

static ZrLibrary_NativeRegistryState *native_registry_get(SZrGlobalState *global);
static struct SZrObjectModule *native_registry_loader(SZrState *state, SZrString *moduleName, TZrPtr userData);
static TZrInt64 native_binding_dispatcher(SZrState *state);
static ZrLibBindingEntry *native_registry_find_binding(ZrLibrary_NativeRegistryState *registry,
                                                       SZrClosureNative *closure);
static SZrObjectModule *native_registry_materialize_module(SZrState *state,
                                                           ZrLibrary_NativeRegistryState *registry,
                                                           const ZrLibModuleDescriptor *descriptor);
static const ZrLibModuleDescriptor *native_registry_find_descriptor_or_plugin(SZrState *state,
                                                                              ZrLibrary_NativeRegistryState *registry,
                                                                              const TZrChar *moduleName);
static SZrObjectModule *native_registry_resolve_loaded_module(SZrState *state,
                                                              ZrLibrary_NativeRegistryState *registry,
                                                              const TZrChar *moduleName);
static void native_registry_clear_error(ZrLibrary_NativeRegistryState *registry);
static void native_registry_set_error(ZrLibrary_NativeRegistryState *registry,
                                      EZrLibNativeRegistryErrorCode errorCode,
                                      const TZrChar *format,
                                      ...);
static const ZrLibRegisteredModuleRecord *native_registry_find_record(ZrLibrary_NativeRegistryState *registry,
                                                                      const TZrChar *moduleName);
static const ZrLibRegisteredModuleRecord *native_registry_find_record_by_descriptor(
        ZrLibrary_NativeRegistryState *registry,
        const ZrLibModuleDescriptor *descriptor);
static TZrBool native_registry_validate_descriptor_compatibility(ZrLibrary_NativeRegistryState *registry,
                                                                 const ZrLibModuleDescriptor *descriptor);
static TZrBool native_registry_register_module_record(SZrGlobalState *global,
                                                      const ZrLibModuleDescriptor *descriptor,
                                                      EZrLibNativeModuleRegistrationKind registrationKind,
                                                      const TZrChar *sourcePath,
                                                      TZrBool isDescriptorPlugin);

static TZrBool native_binding_trace_import_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_NATIVE_IMPORT");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void native_binding_init_call_context_layout(ZrLibCallContext *context,
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
        context->argumentCount = rawArgumentCount;
        context->selfValue = ZR_NULL;
        return;
    }

    context->selfValue = rawArgumentCount > 0 ? ZrCore_Stack_GetValue(functionBase + 1) : ZR_NULL;
    context->argumentBase = rawArgumentCount > 0 ? functionBase + 2 : functionBase + 1;
    context->argumentCount = rawArgumentCount > 0 ? rawArgumentCount - 1 : 0;
}

static void native_binding_trace_import(const TZrChar *format, ...) {
    va_list arguments;

    if (!native_binding_trace_import_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fflush(stderr);
}

static void native_registry_clear_error(ZrLibrary_NativeRegistryState *registry) {
    if (registry == ZR_NULL) {
        return;
    }

    registry->lastErrorCode = ZR_LIB_NATIVE_REGISTRY_ERROR_NONE;
    registry->lastErrorMessage[0] = '\0';
}

static void native_registry_set_error(ZrLibrary_NativeRegistryState *registry,
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

static TZrChar *native_registry_duplicate_string(SZrGlobalState *global, const TZrChar *text) {
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

static const TZrChar *native_binding_value_type_name(SZrState *state, const SZrTypeValue *value) {
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

static const TZrChar *native_binding_call_name(const ZrLibCallContext *context) {
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

static SZrString *native_binding_create_string(SZrState *state, const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_CreateTryHitCache(state, text);
}

static const SZrTypeValue *native_binding_get_zr_global_member(SZrState *state, const TZrChar *memberName) {
    SZrObject *zrObject;
    SZrString *memberNameString;
    SZrTypeValue key;

    if (state == ZR_NULL || state->global == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT || state->global->zrObject.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    memberNameString = native_binding_create_string(state, memberName);
    if (zrObject == ZR_NULL || memberNameString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, zrObject, &key);
}

static SZrObjectModule *native_binding_import_module(SZrState *state, const TZrChar *moduleName) {
    const SZrTypeValue *importValue;
    SZrTypeValue importArgument;
    SZrTypeValue importResult;
    SZrObject *moduleObject;

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    importValue = native_binding_get_zr_global_member(state, "import");
    if (importValue == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLib_Value_SetString(state, &importArgument, moduleName);
    if (!ZrLib_CallValue(state, importValue, ZR_NULL, &importArgument, 1, &importResult)) {
        return ZR_NULL;
    }

    if (importResult.type != ZR_VALUE_TYPE_OBJECT || importResult.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    moduleObject = ZR_CAST_OBJECT(state, importResult.value.object);
    if (moduleObject == ZR_NULL || moduleObject->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_NULL;
    }

    return (SZrObjectModule *)moduleObject;
}

static void native_binding_register_prototype_in_global_scope(SZrState *state,
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

static ZrLibrary_NativeRegistryState *native_registry_get(SZrGlobalState *global) {
    if (global == ZR_NULL || global->nativeModuleLoader != native_registry_loader) {
        return ZR_NULL;
    }
    return (ZrLibrary_NativeRegistryState *)global->nativeModuleLoaderUserData;
}

static ZrLibBindingEntry *native_registry_find_binding(ZrLibrary_NativeRegistryState *registry,
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

static const ZrLibRegisteredModuleRecord *native_registry_find_record(ZrLibrary_NativeRegistryState *registry,
                                                                      const TZrChar *moduleName) {
    TZrSize index;

    if (registry == ZR_NULL || moduleName == ZR_NULL || !registry->moduleRecords.isValid) {
        return ZR_NULL;
    }

    for (index = 0; index < registry->moduleRecords.length; index++) {
        const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
        if (record != ZR_NULL &&
            record->descriptor != ZR_NULL &&
            record->descriptor->moduleName != ZR_NULL &&
            strcmp(record->descriptor->moduleName, moduleName) == 0) {
            return record;
        }
    }

    return ZR_NULL;
}

static const ZrLibRegisteredModuleRecord *native_registry_find_record_by_descriptor(
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

static TZrBool native_registry_validate_descriptor_compatibility(ZrLibrary_NativeRegistryState *registry,
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

static TZrBool native_registry_register_module_record(SZrGlobalState *global,
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
            record->descriptor != ZR_NULL &&
            record->descriptor->moduleName != ZR_NULL &&
            strcmp(record->descriptor->moduleName, descriptor->moduleName) == 0) {
            if (record->sourcePath != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record->sourcePath,
                                  strlen(record->sourcePath) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            record->descriptor = descriptor;
            record->registrationKind = registrationKind;
            record->isDescriptorPlugin = isDescriptorPlugin;
            record->sourcePath = native_registry_duplicate_string(global, sourcePath);
            native_registry_clear_error(registry);
            return ZR_TRUE;
        }
    }

    {
        ZrLibRegisteredModuleRecord record;
        memset(&record, 0, sizeof(record));
        record.descriptor = descriptor;
        record.registrationKind = registrationKind;
        record.isDescriptorPlugin = isDescriptorPlugin;
        record.sourcePath = native_registry_duplicate_string(global, sourcePath);
        ZrCore_Array_Push(global->mainThreadState, &registry->moduleRecords, &record);
    }

    native_registry_clear_error(registry);
    return ZR_TRUE;
}

static void *native_registry_open_library(const TZrChar *path) {
    if (path == ZR_NULL) {
        return ZR_NULL;
    }
#if defined(ZR_PLATFORM_WIN)
    return (void *)LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void native_registry_close_library(void *handle) {
    if (handle == ZR_NULL) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    FreeLibrary((HMODULE)handle);
#else
    dlclose(handle);
#endif
}

static TZrPtr native_registry_find_symbol(void *handle, const TZrChar *symbolName) {
    if (handle == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_NULL;
    }
#if defined(ZR_PLATFORM_WIN)
    return (TZrPtr)GetProcAddress((HMODULE)handle, symbolName);
#else
    return (TZrPtr)dlsym(handle, symbolName);
#endif
}

static const TZrChar *native_registry_dynamic_library_extension(void) {
#if defined(ZR_PLATFORM_WIN)
    return ".dll";
#elif defined(ZR_PLATFORM_DARWIN)
    return ".dylib";
#else
    return ".so";
#endif
}

static TZrBool native_registry_get_executable_directory(TZrChar *buffer, TZrSize bufferSize) {
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

static void native_registry_sanitize_module_name(const TZrChar *moduleName,
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

static const ZrLibModuleDescriptor *native_registry_load_plugin_descriptor(SZrState *state,
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

    symbol = (FZrVmGetNativeModuleV1)native_registry_find_symbol(handle, "ZrVm_GetNativeModule_v1");
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

static const ZrLibModuleDescriptor *native_registry_try_plugin_directory(SZrState *state,
                                                                         ZrLibrary_NativeRegistryState *registry,
                                                                         const TZrChar *directory,
                                                                         const TZrChar *moduleName) {
    TZrChar sanitizedModuleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar pluginFileName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar candidatePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (directory == ZR_NULL || moduleName == ZR_NULL || directory[0] == '\0') {
        return ZR_NULL;
    }

    native_registry_sanitize_module_name(moduleName, sanitizedModuleName, sizeof(sanitizedModuleName));
    snprintf(pluginFileName,
             sizeof(pluginFileName),
             "zrvm_native_%s%s",
             sanitizedModuleName,
             native_registry_dynamic_library_extension());
    ZrLibrary_File_PathJoin(directory, pluginFileName, candidatePath);

    if (ZrLibrary_File_Exist(candidatePath) != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_NULL;
    }

    return native_registry_load_plugin_descriptor(state, registry, candidatePath, moduleName);
}

static const ZrLibModuleDescriptor *native_registry_try_plugin_paths(SZrState *state,
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

static const ZrLibModuleDescriptor *native_registry_find_descriptor_or_plugin(SZrState *state,
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

static SZrObjectModule *native_registry_resolve_loaded_module(SZrState *state,
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

static TZrBool native_binding_make_callable_value(SZrState *state,
                                                  ZrLibrary_NativeRegistryState *registry,
                                                  EZrLibResolvedBindingKind bindingKind,
                                                  const ZrLibModuleDescriptor *moduleDescriptor,
                                                  const ZrLibTypeDescriptor *typeDescriptor,
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

static TZrStackValuePointer native_binding_resolve_call_scratch_base(TZrStackValuePointer stackTop,
                                                                     const SZrCallInfo *callInfo) {
    TZrStackValuePointer base = stackTop;

    if (callInfo != ZR_NULL && callInfo->functionTop.valuePointer > base) {
        base = callInfo->functionTop.valuePointer;
    }

    return base;
}

static SZrObject *native_binding_new_instance_with_prototype(SZrState *state, SZrObjectPrototype *prototype) {
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

static void native_metadata_set_value_field(SZrState *state,
                                            SZrObject *object,
                                            const TZrChar *fieldName,
                                            const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }
    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

static void native_metadata_set_string_field(SZrState *state,
                                             SZrObject *object,
                                             const TZrChar *fieldName,
                                             const TZrChar *value) {
    SZrTypeValue fieldValue;

    if (value == ZR_NULL) {
        ZrLib_Value_SetNull(&fieldValue);
    } else {
        ZrLib_Value_SetString(state, &fieldValue, value);
    }
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
}

static void native_metadata_set_int_field(SZrState *state,
                                          SZrObject *object,
                                          const TZrChar *fieldName,
                                          TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetInt(state, &fieldValue, value);
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
}

static void native_metadata_set_float_field(SZrState *state,
                                            SZrObject *object,
                                            const TZrChar *fieldName,
                                            TZrFloat64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetFloat(state, &fieldValue, value);
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
}

static void native_metadata_set_bool_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrBool value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetBool(state, &fieldValue, value);
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
}

static const TZrChar *native_metadata_constant_type_name(const ZrLibConstantDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return "value";
    }

    if (descriptor->typeName != ZR_NULL) {
        return descriptor->typeName;
    }

    switch (descriptor->kind) {
        case ZR_LIB_CONSTANT_KIND_NULL:
            return "null";
        case ZR_LIB_CONSTANT_KIND_BOOL:
            return "bool";
        case ZR_LIB_CONSTANT_KIND_INT:
            return "int";
        case ZR_LIB_CONSTANT_KIND_FLOAT:
            return "float";
        case ZR_LIB_CONSTANT_KIND_STRING:
            return "string";
        case ZR_LIB_CONSTANT_KIND_ARRAY:
            return "array";
        default:
            return "value";
    }
}

static TZrBool native_descriptor_allows_value_construction(const ZrLibTypeDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_FALSE;
    }
    return descriptor->allowValueConstruction;
}

static TZrBool native_descriptor_allows_boxed_construction(const ZrLibTypeDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_FALSE;
    }
    return descriptor->allowBoxedConstruction;
}

static void native_metadata_set_constant_value_fields(SZrState *state,
                                                      SZrObject *object,
                                                      EZrLibConstantKind kind,
                                                      TZrInt64 intValue,
                                                      TZrFloat64 floatValue,
                                                      const TZrChar *stringValue,
                                                      TZrBool boolValue) {
    if (state == ZR_NULL || object == ZR_NULL) {
        return;
    }

    native_metadata_set_int_field(state, object, "kind", kind);
    switch (kind) {
        case ZR_LIB_CONSTANT_KIND_BOOL:
            native_metadata_set_bool_field(state, object, "boolValue", boolValue);
            break;
        case ZR_LIB_CONSTANT_KIND_INT:
            native_metadata_set_int_field(state, object, "intValue", intValue);
            break;
        case ZR_LIB_CONSTANT_KIND_FLOAT:
            native_metadata_set_float_field(state, object, "floatValue", floatValue);
            break;
        case ZR_LIB_CONSTANT_KIND_STRING:
            native_metadata_set_string_field(state, object, "stringValue", stringValue);
            break;
        default:
            break;
    }
}

static TZrBool native_metadata_push_string_value(SZrState *state, SZrObject *array, const TZrChar *value) {
    SZrTypeValue entryValue;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(state, &entryValue, value);
    return ZrLib_Array_PushValue(state, array, &entryValue);
}

static SZrObject *native_metadata_make_string_array(SZrState *state,
                                                    const TZrChar *const *values,
                                                    TZrSize valueCount) {
    SZrObject *array;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrLib_Array_New(state);
    if (array == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < valueCount; index++) {
        if (values[index] != ZR_NULL) {
            native_metadata_push_string_value(state, array, values[index]);
        }
    }

    return array;
}

static SZrObject *native_metadata_make_field_entry(SZrState *state, const ZrLibFieldDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "typeName", descriptor->typeName);
    return object;
}

static SZrObject *native_metadata_make_method_entry(SZrState *state, const ZrLibMethodDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "returnTypeName", descriptor->returnTypeName);
    native_metadata_set_int_field(state, object, "minArgumentCount", descriptor->minArgumentCount);
    native_metadata_set_int_field(state, object, "maxArgumentCount", descriptor->maxArgumentCount);
    native_metadata_set_bool_field(state, object, "isStatic", descriptor->isStatic);
    return object;
}

static SZrObject *native_metadata_make_meta_method_entry(SZrState *state,
                                                         const ZrLibMetaMethodDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->metaType >= ZR_META_ENUM_MAX) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_int_field(state, object, "metaType", descriptor->metaType);
    native_metadata_set_string_field(state, object, "name", CZrMetaName[descriptor->metaType]);
    native_metadata_set_string_field(state, object, "returnTypeName", descriptor->returnTypeName);
    native_metadata_set_int_field(state, object, "minArgumentCount", descriptor->minArgumentCount);
    native_metadata_set_int_field(state, object, "maxArgumentCount", descriptor->maxArgumentCount);
    return object;
}

static SZrObject *native_metadata_make_function_entry(SZrState *state, const ZrLibFunctionDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "returnTypeName", descriptor->returnTypeName);
    native_metadata_set_int_field(state, object, "minArgumentCount", descriptor->minArgumentCount);
    native_metadata_set_int_field(state, object, "maxArgumentCount", descriptor->maxArgumentCount);
    return object;
}

static SZrObject *native_metadata_make_constant_entry(SZrState *state, const ZrLibConstantDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "typeName", native_metadata_constant_type_name(descriptor));
    native_metadata_set_constant_value_fields(state,
                                              object,
                                              descriptor->kind,
                                              descriptor->intValue,
                                              descriptor->floatValue,
                                              descriptor->stringValue,
                                              descriptor->boolValue);
    return object;
}

static SZrObject *native_metadata_make_enum_member_entry(SZrState *state, const ZrLibEnumMemberDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_constant_value_fields(state,
                                              object,
                                              descriptor->kind,
                                              descriptor->intValue,
                                              descriptor->floatValue,
                                              descriptor->stringValue,
                                              descriptor->boolValue);
    native_metadata_set_string_field(state, object, "documentation", descriptor->documentation);
    return object;
}

static SZrObject *native_metadata_make_module_link_entry(SZrState *state, const ZrLibModuleLinkDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "moduleName", descriptor->moduleName);
    native_metadata_set_string_field(state, object, "documentation", descriptor->documentation);
    return object;
}

static SZrObject *native_metadata_make_type_entry(SZrState *state, const ZrLibTypeDescriptor *descriptor) {
    SZrObject *object;
    SZrObject *fieldsArray;
    SZrObject *methodsArray;
    SZrObject *metaMethodsArray;
    SZrObject *implementsArray;
    SZrObject *enumMembersArray;
    TZrSize index;
    SZrTypeValue arrayValue;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    fieldsArray = ZrLib_Array_New(state);
    methodsArray = ZrLib_Array_New(state);
    metaMethodsArray = ZrLib_Array_New(state);
    implementsArray = native_metadata_make_string_array(state,
                                                        descriptor->implementsTypeNames,
                                                        descriptor->implementsTypeCount);
    enumMembersArray = ZrLib_Array_New(state);
    if (object == ZR_NULL || fieldsArray == ZR_NULL || methodsArray == ZR_NULL || metaMethodsArray == ZR_NULL ||
        implementsArray == ZR_NULL || enumMembersArray == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_int_field(state, object, "prototypeType", descriptor->prototypeType);
    native_metadata_set_string_field(state, object, "extendsTypeName", descriptor->extendsTypeName);
    native_metadata_set_string_field(state, object, "enumValueTypeName", descriptor->enumValueTypeName);
    native_metadata_set_bool_field(state, object, "allowValueConstruction", native_descriptor_allows_value_construction(descriptor));
    native_metadata_set_bool_field(state, object, "allowBoxedConstruction", native_descriptor_allows_boxed_construction(descriptor));
    native_metadata_set_string_field(state, object, "constructorSignature", descriptor->constructorSignature);

    for (index = 0; index < descriptor->fieldCount; index++) {
        SZrObject *fieldEntry = native_metadata_make_field_entry(state, &descriptor->fields[index]);
        if (fieldEntry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, fieldEntry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, fieldsArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->methodCount; index++) {
        SZrObject *methodEntry = native_metadata_make_method_entry(state, &descriptor->methods[index]);
        if (methodEntry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, methodEntry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, methodsArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->metaMethodCount; index++) {
        SZrObject *metaMethodEntry = native_metadata_make_meta_method_entry(state, &descriptor->metaMethods[index]);
        if (metaMethodEntry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, metaMethodEntry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, metaMethodsArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->enumMemberCount; index++) {
        SZrObject *enumMemberEntry = native_metadata_make_enum_member_entry(state, &descriptor->enumMembers[index]);
        if (enumMemberEntry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, enumMemberEntry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, enumMembersArray, &entryValue);
        }
    }

    ZrLib_Value_SetObject(state, &arrayValue, fieldsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "fields", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, methodsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "methods", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, metaMethodsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "metaMethods", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, implementsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "implements", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, enumMembersArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "enumMembers", &arrayValue);

    return object;
}

static SZrObject *native_metadata_make_module_info(SZrState *state,
                                                   const ZrLibModuleDescriptor *descriptor,
                                                   const ZrLibRegisteredModuleRecord *record) {
    SZrObject *object;
    SZrObject *functionsArray;
    SZrObject *constantsArray;
    SZrObject *typesArray;
    SZrObject *modulesArray;
    TZrSize index;
    SZrTypeValue arrayValue;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    functionsArray = ZrLib_Array_New(state);
    constantsArray = ZrLib_Array_New(state);
    typesArray = ZrLib_Array_New(state);
    modulesArray = ZrLib_Array_New(state);
    if (object == ZR_NULL || functionsArray == ZR_NULL || constantsArray == ZR_NULL ||
        typesArray == ZR_NULL || modulesArray == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_int_field(state, object, "version", ZR_NATIVE_MODULE_INFO_VERSION);
    native_metadata_set_string_field(state, object, "moduleName", descriptor->moduleName);
    native_metadata_set_string_field(state, object, "typeHintsJson", descriptor->typeHintsJson);
    native_metadata_set_string_field(state, object, "moduleVersion", descriptor->moduleVersion);
    native_metadata_set_int_field(state,
                                  object,
                                  "runtimeAbiVersion",
                                  descriptor->minRuntimeAbi != 0 ? descriptor->minRuntimeAbi : ZR_VM_NATIVE_RUNTIME_ABI_VERSION);
    native_metadata_set_int_field(state, object, "requiredCapabilities", (TZrInt64)descriptor->requiredCapabilities);
    native_metadata_set_string_field(state,
                                     object,
                                     "registrationKind",
                                     record != ZR_NULL &&
                                                     record->registrationKind ==
                                                             ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN
                                             ? "descriptor-plugin"
                                             : "builtin");
    native_metadata_set_bool_field(state,
                                   object,
                                   "isDescriptorPlugin",
                                   record != ZR_NULL ? record->isDescriptorPlugin : ZR_FALSE);
    native_metadata_set_string_field(state, object, "sourcePath", record != ZR_NULL ? record->sourcePath : ZR_NULL);

    for (index = 0; index < descriptor->functionCount; index++) {
        SZrObject *entry = native_metadata_make_function_entry(state, &descriptor->functions[index]);
        if (entry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, entry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, functionsArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->constantCount; index++) {
        SZrObject *entry = native_metadata_make_constant_entry(state, &descriptor->constants[index]);
        if (entry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, entry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, constantsArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->typeCount; index++) {
        SZrObject *entry = native_metadata_make_type_entry(state, &descriptor->types[index]);
        if (entry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, entry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, typesArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->moduleLinkCount; index++) {
        SZrObject *entry = native_metadata_make_module_link_entry(state, &descriptor->moduleLinks[index]);
        if (entry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, entry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, modulesArray, &entryValue);
        }
    }

    ZrLib_Value_SetObject(state, &arrayValue, functionsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "functions", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, constantsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "constants", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, typesArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "types", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, modulesArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "modules", &arrayValue);

    return object;
}

static TZrBool native_registry_add_constant(SZrState *state,
                                            SZrObjectModule *module,
                                            const ZrLibConstantDescriptor *descriptor) {
    SZrString *name;
    SZrTypeValue value;

    if (state == ZR_NULL || module == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    name = native_binding_create_string(state, descriptor->name);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (descriptor->kind) {
        case ZR_LIB_CONSTANT_KIND_NULL:
            ZrLib_Value_SetNull(&value);
            break;
        case ZR_LIB_CONSTANT_KIND_BOOL:
            ZrLib_Value_SetBool(state, &value, descriptor->boolValue);
            break;
        case ZR_LIB_CONSTANT_KIND_INT:
            ZrLib_Value_SetInt(state, &value, descriptor->intValue);
            break;
        case ZR_LIB_CONSTANT_KIND_FLOAT:
            ZrLib_Value_SetFloat(state, &value, descriptor->floatValue);
            break;
        case ZR_LIB_CONSTANT_KIND_STRING:
            ZrLib_Value_SetString(state, &value, descriptor->stringValue != ZR_NULL ? descriptor->stringValue : "");
            break;
        case ZR_LIB_CONSTANT_KIND_ARRAY: {
            SZrObject *array = ZrLib_Array_New(state);
            if (array == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrLib_Value_SetObject(state, &value, array, ZR_VALUE_TYPE_ARRAY);
            break;
        }
        default:
            ZrLib_Value_SetNull(&value);
            break;
    }

    ZrCore_Module_AddPubExport(state, module, name, &value);
    return ZR_TRUE;
}

static TZrBool native_registry_add_function(SZrState *state,
                                            ZrLibrary_NativeRegistryState *registry,
                                            SZrObjectModule *module,
                                            const ZrLibModuleDescriptor *moduleDescriptor,
                                            const ZrLibFunctionDescriptor *functionDescriptor) {
    SZrString *name;
    SZrTypeValue value;

    if (state == ZR_NULL || registry == ZR_NULL || module == ZR_NULL || moduleDescriptor == ZR_NULL ||
        functionDescriptor == ZR_NULL || functionDescriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!native_binding_make_callable_value(state,
                                            registry,
                                            ZR_LIB_RESOLVED_BINDING_FUNCTION,
                                            moduleDescriptor,
                                            ZR_NULL,
                                            functionDescriptor,
                                            &value)) {
        return ZR_FALSE;
    }

    name = native_binding_create_string(state, functionDescriptor->name);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Module_AddPubExport(state, module, name, &value);
    return ZR_TRUE;
}

static TZrBool native_registry_add_module_link(SZrState *state,
                                               ZrLibrary_NativeRegistryState *registry,
                                               SZrObjectModule *module,
                                               const ZrLibModuleLinkDescriptor *descriptor) {
    SZrObjectModule *linkedModule;
    SZrString *name;
    SZrTypeValue value;

    if (state == ZR_NULL || registry == ZR_NULL || module == ZR_NULL || descriptor == ZR_NULL ||
        descriptor->name == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    linkedModule = native_registry_resolve_loaded_module(state, registry, descriptor->moduleName);
    if (linkedModule == ZR_NULL) {
        return ZR_FALSE;
    }

    name = native_binding_create_string(state, descriptor->name);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &value, &linkedModule->super, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Module_AddPubExport(state, module, name, &value);
    return ZR_TRUE;
}

static TZrBool native_registry_add_methods(SZrState *state,
                                           ZrLibrary_NativeRegistryState *registry,
                                           const ZrLibModuleDescriptor *moduleDescriptor,
                                           const ZrLibTypeDescriptor *typeDescriptor,
                                           SZrObjectPrototype *prototype) {
    TZrSize index;

    if (state == ZR_NULL || registry == ZR_NULL || moduleDescriptor == ZR_NULL || typeDescriptor == ZR_NULL ||
        prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < typeDescriptor->methodCount; index++) {
        const ZrLibMethodDescriptor *methodDescriptor = &typeDescriptor->methods[index];
        SZrTypeValue methodValue;
        SZrString *methodName;
        SZrTypeValue methodKey;

        if (methodDescriptor->name == ZR_NULL || methodDescriptor->callback == ZR_NULL) {
            continue;
        }

        if (!native_binding_make_callable_value(state,
                                                registry,
                                                ZR_LIB_RESOLVED_BINDING_METHOD,
                                                moduleDescriptor,
                                                typeDescriptor,
                                                methodDescriptor,
                                                &methodValue)) {
            return ZR_FALSE;
        }

        methodName = native_binding_create_string(state, methodDescriptor->name);
        if (methodName == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Value_InitAsRawObject(state, &methodKey, ZR_CAST_RAW_OBJECT_AS_SUPER(methodName));
        methodKey.type = ZR_VALUE_TYPE_STRING;
        ZrCore_Object_SetValue(state, &prototype->super, &methodKey, &methodValue);
    }

    for (index = 0; index < typeDescriptor->metaMethodCount; index++) {
        const ZrLibMetaMethodDescriptor *metaDescriptor = &typeDescriptor->metaMethods[index];
        SZrTypeValue metaValue;
        SZrString *constructorName;
        SZrTypeValue constructorKey;

        if (metaDescriptor->callback == ZR_NULL || metaDescriptor->metaType >= ZR_META_ENUM_MAX) {
            continue;
        }

        if (!native_binding_make_callable_value(state,
                                                registry,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                moduleDescriptor,
                                                typeDescriptor,
                                                metaDescriptor,
                                                &metaValue)) {
            return ZR_FALSE;
        }

        ZrCore_ObjectPrototype_AddMeta(state,
                                       prototype,
                                       metaDescriptor->metaType,
                                       (SZrFunction *)metaValue.value.object);

        if (metaDescriptor->metaType == ZR_META_CONSTRUCTOR) {
            constructorName = native_binding_create_string(state, "__constructor");
            if (constructorName == ZR_NULL) {
                return ZR_FALSE;
            }

            ZrCore_Value_InitAsRawObject(state, &constructorKey, ZR_CAST_RAW_OBJECT_AS_SUPER(constructorName));
            constructorKey.type = ZR_VALUE_TYPE_STRING;
            ZrCore_Object_SetValue(state, &prototype->super, &constructorKey, &metaValue);
        }
    }

    return ZR_TRUE;
}

static void native_registry_set_hidden_string_metadata(SZrState *state,
                                                       SZrObject *object,
                                                       const TZrChar *fieldName,
                                                       const TZrChar *value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetString(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void native_registry_set_hidden_bool_metadata(SZrState *state,
                                                     SZrObject *object,
                                                     const TZrChar *fieldName,
                                                     TZrBool value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static TZrBool native_registry_init_enum_member_scalar(SZrState *state,
                                                       const ZrLibEnumMemberDescriptor *descriptor,
                                                       SZrTypeValue *value) {
    if (state == ZR_NULL || descriptor == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (descriptor->kind) {
        case ZR_LIB_CONSTANT_KIND_NULL:
            ZrLib_Value_SetNull(value);
            return ZR_TRUE;
        case ZR_LIB_CONSTANT_KIND_BOOL:
            ZrLib_Value_SetBool(state, value, descriptor->boolValue);
            return ZR_TRUE;
        case ZR_LIB_CONSTANT_KIND_INT:
            ZrLib_Value_SetInt(state, value, descriptor->intValue);
            return ZR_TRUE;
        case ZR_LIB_CONSTANT_KIND_FLOAT:
            ZrLib_Value_SetFloat(state, value, descriptor->floatValue);
            return ZR_TRUE;
        case ZR_LIB_CONSTANT_KIND_STRING:
            ZrLib_Value_SetString(state, value, descriptor->stringValue != ZR_NULL ? descriptor->stringValue : "");
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static SZrObject *native_registry_make_enum_instance(SZrState *state,
                                                     SZrObjectPrototype *prototype,
                                                     const SZrTypeValue *underlyingValue,
                                                     const TZrChar *memberName) {
    SZrObject *enumObject;

    if (state == ZR_NULL || prototype == ZR_NULL || underlyingValue == ZR_NULL) {
        return ZR_NULL;
    }

    enumObject = ZrCore_Object_New(state, prototype);
    if (enumObject == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Object_Init(state, enumObject);
    ZrLib_Object_SetFieldCString(state, enumObject, kNativeEnumValueFieldName, underlyingValue);
    if (memberName != ZR_NULL) {
        SZrTypeValue nameValue;
        ZrLib_Value_SetString(state, &nameValue, memberName);
        ZrLib_Object_SetFieldCString(state, enumObject, kNativeEnumNameFieldName, &nameValue);
    }

    return enumObject;
}

static void native_registry_attach_type_runtime_metadata(SZrState *state,
                                                         const ZrLibTypeDescriptor *typeDescriptor,
                                                         SZrObjectPrototype *prototype) {
    if (state == ZR_NULL || typeDescriptor == ZR_NULL || prototype == ZR_NULL) {
        return;
    }

    native_registry_set_hidden_bool_metadata(state,
                                             &prototype->super,
                                             kNativeAllowValueConstructionFieldName,
                                             native_descriptor_allows_value_construction(typeDescriptor));
    native_registry_set_hidden_bool_metadata(state,
                                             &prototype->super,
                                             kNativeAllowBoxedConstructionFieldName,
                                             native_descriptor_allows_boxed_construction(typeDescriptor));

    if (typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        native_registry_set_hidden_string_metadata(state,
                                                   &prototype->super,
                                                   kNativeEnumValueTypeFieldName,
                                                   typeDescriptor->enumValueTypeName);
    }
}

static TZrBool native_registry_add_enum_members(SZrState *state,
                                                SZrObjectPrototype *prototype,
                                                const ZrLibTypeDescriptor *typeDescriptor) {
    TZrSize index;

    if (state == ZR_NULL || prototype == ZR_NULL || typeDescriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeDescriptor->prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        return ZR_TRUE;
    }

    for (index = 0; index < typeDescriptor->enumMemberCount; index++) {
        const ZrLibEnumMemberDescriptor *memberDescriptor = &typeDescriptor->enumMembers[index];
        SZrTypeValue scalarValue;
        SZrObject *enumObject;
        SZrTypeValue enumValue;

        if (memberDescriptor->name == ZR_NULL ||
            !native_registry_init_enum_member_scalar(state, memberDescriptor, &scalarValue)) {
            continue;
        }

        enumObject = native_registry_make_enum_instance(state, prototype, &scalarValue, memberDescriptor->name);
        if (enumObject == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrLib_Value_SetObject(state, &enumValue, enumObject, ZR_VALUE_TYPE_OBJECT);
        ZrLib_Object_SetFieldCString(state, &prototype->super, memberDescriptor->name, &enumValue);
    }

    return ZR_TRUE;
}

static TZrBool native_registry_add_type(SZrState *state,
                                        ZrLibrary_NativeRegistryState *registry,
                                        SZrObjectModule *module,
                                        const ZrLibModuleDescriptor *moduleDescriptor,
                                        const ZrLibTypeDescriptor *typeDescriptor) {
    SZrString *typeName;
    SZrObjectPrototype *prototype;
    EZrObjectPrototypeType expectedPrototypeType;
    TZrSize index;
    SZrTypeValue prototypeValue;

    if (state == ZR_NULL || registry == ZR_NULL || module == ZR_NULL || moduleDescriptor == ZR_NULL ||
        typeDescriptor == ZR_NULL || typeDescriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    typeName = native_binding_create_string(state, typeDescriptor->name);
    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    expectedPrototypeType = typeDescriptor->prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INVALID
                                    ? typeDescriptor->prototypeType
                                    : ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    prototype = ZrLib_Type_FindPrototype(state, typeDescriptor->name);
    if (prototype != ZR_NULL &&
        expectedPrototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INVALID &&
        prototype->type != expectedPrototypeType) {
        return ZR_FALSE;
    }

    if (prototype == ZR_NULL && typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        prototype = (SZrObjectPrototype *)ZrCore_StructPrototype_New(state, typeName);
        if (prototype != ZR_NULL) {
            SZrStructPrototype *structPrototype = (SZrStructPrototype *)prototype;
            for (index = 0; index < typeDescriptor->fieldCount; index++) {
                const ZrLibFieldDescriptor *fieldDescriptor = &typeDescriptor->fields[index];
                if (fieldDescriptor->name != ZR_NULL) {
                    SZrString *fieldName = native_binding_create_string(state, fieldDescriptor->name);
                    if (fieldName != ZR_NULL) {
                        ZrCore_StructPrototype_AddField(state, structPrototype, fieldName, index);
                    }
                }
            }
        }
    } else if (prototype == ZR_NULL) {
        prototype = ZrCore_ObjectPrototype_New(state,
                                               typeName,
                                               expectedPrototypeType);
    }

    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    native_registry_attach_type_runtime_metadata(state, typeDescriptor, prototype);

    if (!native_registry_add_methods(state, registry, moduleDescriptor, typeDescriptor, prototype)) {
        return ZR_FALSE;
    }

    if (!native_registry_add_enum_members(state, prototype, typeDescriptor)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Module_AddPubExport(state, module, typeName, &prototypeValue);
    native_binding_register_prototype_in_global_scope(state, typeName, &prototypeValue);
    return ZR_TRUE;
}

static SZrObjectPrototype *native_registry_get_module_prototype(SZrState *state,
                                                                SZrObjectModule *module,
                                                                const TZrChar *typeName) {
    SZrString *name;
    const SZrTypeValue *exportedValue;
    SZrObject *object;

    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    name = native_binding_create_string(state, typeName);
    if (name == ZR_NULL) {
        return ZR_NULL;
    }

    exportedValue = ZrCore_Module_GetPubExport(state, module, name);
    if (exportedValue == ZR_NULL || exportedValue->type != ZR_VALUE_TYPE_OBJECT || exportedValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, exportedValue->value.object);
    if (object == ZR_NULL || object->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)object;
}

static void native_registry_resolve_type_relationships(SZrState *state,
                                                       SZrObjectModule *module,
                                                       const ZrLibModuleDescriptor *descriptor) {
    TZrSize index;

    if (state == ZR_NULL || module == ZR_NULL || descriptor == ZR_NULL) {
        return;
    }

    for (index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        SZrObjectPrototype *prototype;
        SZrObjectPrototype *superPrototype;

        if (typeDescriptor->name == ZR_NULL || typeDescriptor->extendsTypeName == ZR_NULL) {
            continue;
        }

        prototype = native_registry_get_module_prototype(state, module, typeDescriptor->name);
        if (prototype == ZR_NULL) {
            continue;
        }

        superPrototype = ZrLib_Type_FindPrototype(state, typeDescriptor->extendsTypeName);
        if (superPrototype == ZR_NULL || superPrototype == prototype) {
            continue;
        }

        ZrCore_ObjectPrototype_SetSuper(state, prototype, superPrototype);
    }
}

static SZrObjectModule *native_registry_materialize_module(SZrState *state,
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

static struct SZrObjectModule *native_registry_loader(SZrState *state, SZrString *moduleName, TZrPtr userData) {
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

static TZrBool native_binding_auto_check_arity(const ZrLibCallContext *context) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    if (context->functionDescriptor != ZR_NULL) {
        return ZrLib_CallContext_CheckArity(context,
                                            context->functionDescriptor->minArgumentCount,
                                            context->functionDescriptor->maxArgumentCount);
    }

    if (context->methodDescriptor != ZR_NULL) {
        return ZrLib_CallContext_CheckArity(context,
                                            context->methodDescriptor->minArgumentCount,
                                            context->methodDescriptor->maxArgumentCount);
    }

    if (context->metaMethodDescriptor != ZR_NULL) {
        return ZrLib_CallContext_CheckArity(context,
                                            context->metaMethodDescriptor->minArgumentCount,
                                            context->metaMethodDescriptor->maxArgumentCount);
    }

    return ZR_TRUE;
}

static TZrInt64 native_binding_dispatcher(SZrState *state) {
    ZrLibrary_NativeRegistryState *registry;
    TZrStackValuePointer functionBase;
    SZrFunctionStackAnchor functionBaseAnchor;
    SZrTypeValue *closureValue;
    SZrClosureNative *closure;
    ZrLibBindingEntry *entry;
    ZrLibCallContext context;
    SZrTypeValue result;
    TZrSize rawArgumentCount;
    TZrBool success;

    if (state == ZR_NULL || state->global == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    registry = native_registry_get(state->global);
    functionBase = state->callInfoList->functionBase.valuePointer;
    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);
    closureValue = ZrCore_Stack_GetValue(functionBase);
    if (registry == ZR_NULL || closureValue == ZR_NULL) {
        return 0;
    }

    closure = ZR_CAST_NATIVE_CLOSURE(state, closureValue->value.object);
    entry = native_registry_find_binding(registry, closure);
    if (entry == ZR_NULL) {
        return 0;
    }

    rawArgumentCount = (TZrSize)(state->stackTop.valuePointer - (functionBase + 1));

    memset(&context, 0, sizeof(context));
    context.state = state;
    context.moduleDescriptor = entry->moduleDescriptor;
    context.typeDescriptor = entry->typeDescriptor;
    context.functionDescriptor =
            entry->bindingKind == ZR_LIB_RESOLVED_BINDING_FUNCTION ? entry->descriptor.functionDescriptor : ZR_NULL;
    context.methodDescriptor =
            entry->bindingKind == ZR_LIB_RESOLVED_BINDING_METHOD ? entry->descriptor.methodDescriptor : ZR_NULL;
    context.metaMethodDescriptor =
            entry->bindingKind == ZR_LIB_RESOLVED_BINDING_META_METHOD ? entry->descriptor.metaMethodDescriptor : ZR_NULL;
    context.functionBase = functionBase;

    native_binding_init_call_context_layout(&context, functionBase, rawArgumentCount);

    ZrLib_Value_SetNull(&result);

    if (!native_binding_auto_check_arity(&context)) {
        return 0;
    }

    success = ZR_FALSE;
    if (context.functionDescriptor != ZR_NULL && context.functionDescriptor->callback != ZR_NULL) {
        success = context.functionDescriptor->callback(&context, &result);
    } else if (context.methodDescriptor != ZR_NULL && context.methodDescriptor->callback != ZR_NULL) {
        success = context.methodDescriptor->callback(&context, &result);
    } else if (context.metaMethodDescriptor != ZR_NULL && context.metaMethodDescriptor->callback != ZR_NULL) {
        success = context.metaMethodDescriptor->callback(&context, &result);
    }

    if (!success) {
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return 0;
        }
        ZrLib_Value_SetNull(&result);
    }

    functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
    closureValue = ZrCore_Stack_GetValue(functionBase);
    ZrCore_Value_Copy(state, closureValue, &result);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static TZrStackValuePointer native_binding_temp_root_slot(ZrLibTempValueRoot *root) {
    if (root == ZR_NULL || !root->active || root->state == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Function_StackAnchorRestore(root->state, &root->slotAnchor);
}

TZrSize ZrLib_CallContext_ArgumentCount(const ZrLibCallContext *context) {
    return context != ZR_NULL ? context->argumentCount : 0;
}

SZrTypeValue *ZrLib_CallContext_Self(const ZrLibCallContext *context) {
    return context != ZR_NULL ? context->selfValue : ZR_NULL;
}

SZrTypeValue *ZrLib_CallContext_Argument(const ZrLibCallContext *context, TZrSize index) {
    if (context == ZR_NULL || index >= context->argumentCount) {
        return ZR_NULL;
    }
    return ZrCore_Stack_GetValue(context->argumentBase + index);
}

TZrBool ZrLib_CallContext_CheckArity(const ZrLibCallContext *context,
                                     TZrSize minArgumentCount,
                                     TZrSize maxArgumentCount) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    if (context->argumentCount < minArgumentCount ||
        (maxArgumentCount != UINT16_MAX && context->argumentCount > maxArgumentCount)) {
        ZrLib_CallContext_RaiseArityError(context, minArgumentCount, maxArgumentCount);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

void ZrLib_CallContext_RaiseTypeError(const ZrLibCallContext *context, TZrSize index, const TZrChar *expectedType) {
    const TZrChar *callName = native_binding_call_name(context);
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    const TZrChar *actualType = native_binding_value_type_name(context != ZR_NULL ? context->state : ZR_NULL, value);
    ZrCore_Debug_RunError(context->state,
                          "%s argument %u expected %s but got %s",
                          callName,
                          (unsigned)(index + 1),
                          expectedType != ZR_NULL ? expectedType : "value",
                          actualType != ZR_NULL ? actualType : "value");
}

void ZrLib_CallContext_RaiseArityError(const ZrLibCallContext *context,
                                       TZrSize minArgumentCount,
                                       TZrSize maxArgumentCount) {
    const TZrChar *callName = native_binding_call_name(context);
    if (maxArgumentCount == UINT16_MAX) {
        ZrCore_Debug_RunError(context->state,
                              "%s expected at least %u arguments but got %u",
                              callName,
                              (unsigned)minArgumentCount,
                              (unsigned)context->argumentCount);
    } else if (minArgumentCount == maxArgumentCount) {
        ZrCore_Debug_RunError(context->state,
                              "%s expected %u arguments but got %u",
                              callName,
                              (unsigned)minArgumentCount,
                              (unsigned)context->argumentCount);
    } else {
        ZrCore_Debug_RunError(context->state,
                              "%s expected %u..%u arguments but got %u",
                              callName,
                              (unsigned)minArgumentCount,
                              (unsigned)maxArgumentCount,
                              (unsigned)context->argumentCount);
    }
}

TZrBool ZrLib_TempValueRoot_Begin(SZrState *state, ZrLibTempValueRoot *root) {
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer slot;

    if (state == ZR_NULL || root == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(root, 0, sizeof(*root));
    root->state = state;
    root->callInfo = state->callInfoList;
    savedStackTop = state->stackTop.valuePointer;
    if (savedStackTop == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &root->savedStackTopAnchor);
    if (root->callInfo != ZR_NULL && root->callInfo->functionTop.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, root->callInfo->functionTop.valuePointer, &root->savedCallInfoTopAnchor);
        root->hasSavedCallInfoTop = ZR_TRUE;
    }

    slot = ZrCore_Function_CheckStackAndAnchor(state, 1, savedStackTop, savedStackTop, &root->slotAnchor);
    if (slot == ZR_NULL) {
        memset(root, 0, sizeof(*root));
        return ZR_FALSE;
    }

    if (root->hasSavedCallInfoTop && root->callInfo != ZR_NULL) {
        root->callInfo->functionTop.valuePointer =
                ZrCore_Function_StackAnchorRestore(state, &root->savedCallInfoTopAnchor);
    }

    slot = ZrCore_Function_StackAnchorRestore(state, &root->slotAnchor);
    if (slot == ZR_NULL) {
        memset(root, 0, sizeof(*root));
        return ZR_FALSE;
    }

    state->stackTop.valuePointer = slot + 1;
    if (root->callInfo != ZR_NULL &&
        (root->callInfo->functionTop.valuePointer == ZR_NULL ||
         root->callInfo->functionTop.valuePointer < state->stackTop.valuePointer)) {
        root->callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(slot));
    root->active = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrLib_CallContext_BeginTempValueRoot(const ZrLibCallContext *context,
                                             ZrLibTempValueRoot *root) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLib_TempValueRoot_Begin(context->state, root);
}

SZrTypeValue *ZrLib_TempValueRoot_Value(ZrLibTempValueRoot *root) {
    TZrStackValuePointer slot = native_binding_temp_root_slot(root);
    return slot != ZR_NULL ? ZrCore_Stack_GetValue(slot) : ZR_NULL;
}

TZrBool ZrLib_TempValueRoot_SetValue(ZrLibTempValueRoot *root, const SZrTypeValue *value) {
    SZrTypeValue *slotValue;

    if (root == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    slotValue = ZrLib_TempValueRoot_Value(root);
    if (slotValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Stack_CopyValue(root->state, native_binding_temp_root_slot(root), value);
    return ZR_TRUE;
}

TZrBool ZrLib_TempValueRoot_SetObject(ZrLibTempValueRoot *root,
                                      SZrObject *object,
                                      EZrValueType type) {
    SZrTypeValue *slotValue;

    if (root == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    slotValue = ZrLib_TempValueRoot_Value(root);
    if (slotValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(root->state, slotValue, object, type);
    return ZR_TRUE;
}

void ZrLib_TempValueRoot_SetNull(ZrLibTempValueRoot *root) {
    SZrTypeValue *slotValue = ZrLib_TempValueRoot_Value(root);
    if (slotValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(slotValue);
    }
}

void ZrLib_TempValueRoot_End(ZrLibTempValueRoot *root) {
    if (root == ZR_NULL || !root->active || root->state == ZR_NULL) {
        return;
    }

    if (root->hasSavedCallInfoTop && root->callInfo != ZR_NULL) {
        root->callInfo->functionTop.valuePointer =
                ZrCore_Function_StackAnchorRestore(root->state, &root->savedCallInfoTopAnchor);
    }
    root->state->stackTop.valuePointer = ZrCore_Function_StackAnchorRestore(root->state, &root->savedStackTopAnchor);
    memset(root, 0, sizeof(*root));
}

TZrBool ZrLib_CallContext_ReadInt(const ZrLibCallContext *context, TZrSize index, TZrInt64 *outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
        return ZR_FALSE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            if (outValue != ZR_NULL) {
                *outValue = value->value.nativeObject.nativeInt64;
            }
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            if (outValue != ZR_NULL) {
                *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
            }
            return ZR_TRUE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            if (outValue != ZR_NULL) {
                *outValue = (TZrInt64)value->value.nativeObject.nativeDouble;
            }
            return ZR_TRUE;
        default:
            ZrLib_CallContext_RaiseTypeError(context, index, "int");
            return ZR_FALSE;
    }
}

TZrBool ZrLib_CallContext_ReadFloat(const ZrLibCallContext *context, TZrSize index, TZrFloat64 *outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
        return ZR_FALSE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            if (outValue != ZR_NULL) {
                *outValue = (TZrFloat64)value->value.nativeObject.nativeInt64;
            }
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            if (outValue != ZR_NULL) {
                *outValue = (TZrFloat64)value->value.nativeObject.nativeUInt64;
            }
            return ZR_TRUE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            if (outValue != ZR_NULL) {
                *outValue = value->value.nativeObject.nativeDouble;
            }
            return ZR_TRUE;
        default:
            ZrLib_CallContext_RaiseTypeError(context, index, "float");
            return ZR_FALSE;
    }
}

TZrBool ZrLib_CallContext_ReadBool(const ZrLibCallContext *context, TZrSize index, TZrBool *outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
        return ZR_FALSE;
    }

    if (value->type != ZR_VALUE_TYPE_BOOL) {
        ZrLib_CallContext_RaiseTypeError(context, index, "bool");
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = value->value.nativeObject.nativeBool;
    }
    return ZR_TRUE;
}

TZrBool ZrLib_CallContext_ReadString(const ZrLibCallContext *context, TZrSize index, SZrString **outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
        return ZR_FALSE;
    }

    if (value->type != ZR_VALUE_TYPE_STRING) {
        ZrLib_CallContext_RaiseTypeError(context, index, "string");
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = ZR_CAST_STRING(context->state, value->value.object);
    }
    return ZR_TRUE;
}

TZrBool ZrLib_CallContext_ReadObject(const ZrLibCallContext *context, TZrSize index, SZrObject **outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
        return ZR_FALSE;
    }

    if (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) {
        ZrLib_CallContext_RaiseTypeError(context, index, "object");
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = ZR_CAST_OBJECT(context->state, value->value.object);
    }
    return ZR_TRUE;
}

TZrBool ZrLib_CallContext_ReadArray(const ZrLibCallContext *context, TZrSize index, SZrObject **outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
        return ZR_FALSE;
    }

    if (value->type != ZR_VALUE_TYPE_ARRAY) {
        ZrLib_CallContext_RaiseTypeError(context, index, "array");
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = ZR_CAST_OBJECT(context->state, value->value.object);
    }
    return ZR_TRUE;
}

TZrBool ZrLib_CallContext_ReadFunction(const ZrLibCallContext *context, TZrSize index, SZrTypeValue **outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
        return ZR_FALSE;
    }

    if (value->type != ZR_VALUE_TYPE_FUNCTION &&
        value->type != ZR_VALUE_TYPE_CLOSURE &&
        value->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        ZrLib_CallContext_RaiseTypeError(context, index, "function");
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = value;
    }
    return ZR_TRUE;
}

void ZrLib_Value_SetNull(SZrTypeValue *value) {
    if (value != ZR_NULL) {
        ZrCore_Value_ResetAsNull(value);
    }
}

void ZrLib_Value_SetBool(SZrState *state, SZrTypeValue *value, TZrBool boolValue) {
    ZR_UNUSED_PARAMETER(state);
    if (value != ZR_NULL) {
        ZR_VALUE_FAST_SET(value, nativeBool, boolValue, ZR_VALUE_TYPE_BOOL);
    }
}

void ZrLib_Value_SetInt(SZrState *state, SZrTypeValue *value, TZrInt64 intValue) {
    if (value != ZR_NULL) {
        ZrCore_Value_InitAsInt(state, value, intValue);
    }
}

void ZrLib_Value_SetFloat(SZrState *state, SZrTypeValue *value, TZrFloat64 floatValue) {
    if (value != ZR_NULL) {
        ZrCore_Value_InitAsFloat(state, value, floatValue);
    }
}

void ZrLib_Value_SetString(SZrState *state, SZrTypeValue *value, const TZrChar *stringValue) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return;
    }
    ZrLib_Value_SetStringObject(state, value, native_binding_create_string(state, stringValue != ZR_NULL ? stringValue : ""));
}

void ZrLib_Value_SetStringObject(SZrState *state, SZrTypeValue *value, SZrString *stringObject) {
    if (state == ZR_NULL || value == ZR_NULL || stringObject == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    value->type = ZR_VALUE_TYPE_STRING;
}

void ZrLib_Value_SetObject(SZrState *state, SZrTypeValue *value, SZrObject *object, EZrValueType type) {
    if (state == ZR_NULL || value == ZR_NULL || object == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value->type = type;
}

void ZrLib_Value_SetNativePointer(SZrState *state, SZrTypeValue *value, TZrPtr pointerValue) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsNativePointer(state, value, pointerValue);
}

SZrObject *ZrLib_Object_New(SZrState *state) {
    SZrObject *object;
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    object = ZrCore_Object_New(state, ZR_NULL);
    if (object != ZR_NULL) {
        ZrCore_Object_Init(state, object);
    }
    return object;
}

SZrObject *ZrLib_Array_New(SZrState *state) {
    SZrObject *array;
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(state, array);
    }
    return array;
}

void ZrLib_Object_SetFieldCString(SZrState *state,
                                  SZrObject *object,
                                  const TZrChar *fieldName,
                                  const SZrTypeValue *value) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    fieldString = native_binding_create_string(state, fieldName);
    if (fieldString == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
}

const SZrTypeValue *ZrLib_Object_GetFieldCString(SZrState *state,
                                                 SZrObject *object,
                                                 const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = native_binding_create_string(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

TZrBool ZrLib_Array_PushValue(SZrState *state, SZrObject *array, const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)ZrLib_Array_Length(array));
    ZrCore_Object_SetValue(state, array, &key, value);
    return ZR_TRUE;
}

TZrSize ZrLib_Array_Length(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0;
    }
    return array->nodeMap.elementCount;
}

const SZrTypeValue *ZrLib_Array_Get(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;
    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(state, array, &key);
}

SZrObjectPrototype *ZrLib_Type_FindPrototype(SZrState *state, const TZrChar *typeName) {
    SZrTypeValue key;
    SZrString *typeString;
    const SZrTypeValue *value;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL ||
        state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    typeString = native_binding_create_string(state, typeName);
    if (typeString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeString));
    key.type = ZR_VALUE_TYPE_STRING;
    value = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, state->global->zrObject.value.object), &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)ZR_CAST_OBJECT(state, value->value.object);
}

SZrObject *ZrLib_Type_NewInstance(SZrState *state, const TZrChar *typeName) {
    return native_binding_new_instance_with_prototype(state, ZrLib_Type_FindPrototype(state, typeName));
}

SZrObjectModule *ZrLib_Module_GetLoaded(SZrState *state, const TZrChar *moduleName) {
    SZrString *moduleString;
    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }
    moduleString = native_binding_create_string(state, moduleName);
    if (moduleString == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_Module_GetFromCache(state, moduleString);
}

const SZrTypeValue *ZrLib_Module_GetExport(SZrState *state,
                                           const TZrChar *moduleName,
                                           const TZrChar *exportName) {
    SZrObjectModule *module;
    SZrString *exportString;

    if (state == ZR_NULL || moduleName == ZR_NULL || exportName == ZR_NULL) {
        return ZR_NULL;
    }

    module = ZrLib_Module_GetLoaded(state, moduleName);
    if (module == ZR_NULL) {
        module = native_binding_import_module(state, moduleName);
    }
    if (module == ZR_NULL) {
        return ZR_NULL;
    }

    exportString = native_binding_create_string(state, exportName);
    if (exportString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_GetPubExport(state, module, exportString);
}

TZrBool ZrLib_CallValue(SZrState *state,
                        const SZrTypeValue *callable,
                        const SZrTypeValue *receiver,
                        const SZrTypeValue *arguments,
                        TZrSize argumentCount,
                        SZrTypeValue *result) {
    SZrTypeValue stableCallable;
    SZrTypeValue stableReceiver;
    TZrStackValuePointer savedStackTop;
    SZrCallInfo *savedCallInfo;
    TZrSize totalArguments;
    TZrSize scratchSlots;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor baseAnchor;
    SZrFunctionStackAnchor callInfoBaseAnchor;
    SZrFunctionStackAnchor callInfoTopAnchor;
    SZrFunctionStackAnchor callInfoReturnAnchor;
    TZrBool hasAnchoredReturnDestination = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL || callable == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    stableCallable = *callable;
    if (receiver != ZR_NULL) {
        stableReceiver = *receiver;
    }
    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    totalArguments = argumentCount + (receiver != ZR_NULL ? 1 : 0);
    scratchSlots = 1 + totalArguments;
    base = native_binding_resolve_call_scratch_base(savedStackTop, savedCallInfo);

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    ZrCore_Function_StackAnchorInit(state, base, &baseAnchor);
    if (savedCallInfo != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &callInfoTopAnchor);
        hasAnchoredReturnDestination =
                (TZrBool)(savedCallInfo->hasReturnDestination && savedCallInfo->returnDestination != ZR_NULL);
        if (hasAnchoredReturnDestination) {
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->returnDestination, &callInfoReturnAnchor);
        }
    }

    ZrCore_Function_CheckStackAndGc(state, scratchSlots, base);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
    if (savedCallInfo != ZR_NULL) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }
        base = native_binding_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    }

    ZrCore_Stack_CopyValue(state, base, &stableCallable);
    if (receiver != ZR_NULL) {
        ZrCore_Stack_CopyValue(state, base + 1, &stableReceiver);
    }
    for (index = 0; index < argumentCount; index++) {
        ZrCore_Stack_CopyValue(state, base + 1 + (receiver != ZR_NULL ? 1 : 0) + index, &arguments[index]);
    }

    state->stackTop.valuePointer = base + scratchSlots;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &callInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->returnDestination, &callInfoReturnAnchor);
        }
    }
    base = ZrCore_Function_CallWithoutYieldAndRestore(state, base, 1);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }
    }

    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        return ZR_TRUE;
    }

    state->stackTop.valuePointer = savedStackTop;
    state->callInfoList = savedCallInfo;
    return ZR_FALSE;
}

TZrBool ZrLib_CallModuleExport(SZrState *state,
                               const TZrChar *moduleName,
                               const TZrChar *exportName,
                               const SZrTypeValue *arguments,
                               TZrSize argumentCount,
                               SZrTypeValue *result) {
    const SZrTypeValue *exportValue = ZrLib_Module_GetExport(state, moduleName, exportName);
    if (exportValue == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrLib_CallValue(state, exportValue, ZR_NULL, arguments, argumentCount, result);
}

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
    ZrCore_Array_Init(state, &registry->moduleRecords, sizeof(ZrLibRegisteredModuleRecord), 8);
    ZrCore_Array_Init(state, &registry->bindingEntries, sizeof(ZrLibBindingEntry), 64);
    ZrCore_Array_Init(state, &registry->pluginHandles, sizeof(void *), 4);
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
