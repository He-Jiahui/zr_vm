#include "internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/log.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_library/native_registry.h"

typedef struct ZrRustBindingNativeGenericParameterStorage {
    ZrLibGenericParameterDescriptor descriptor;
    TZrChar **constraintTypeNames;
} ZrRustBindingNativeGenericParameterStorage;

typedef struct ZrRustBindingNativeCallbackStorage {
    FZrRustBindingNativeCallback callback;
    TZrPtr userData;
    FZrRustBindingDestroyCallback destroyUserData;
} ZrRustBindingNativeCallbackStorage;

typedef struct ZrRustBindingNativeTypeStorage {
    ZrLibFieldDescriptor *fields;
    ZrLibMethodDescriptor *methods;
    ZrRustBindingNativeCallbackStorage *methodCallbacks;
    ZrLibMetaMethodDescriptor *metaMethods;
    ZrRustBindingNativeCallbackStorage *metaMethodCallbacks;
    TZrChar **implementsTypeNames;
    ZrLibEnumMemberDescriptor *enumMembers;
    ZrRustBindingNativeGenericParameterStorage *genericParameters;
} ZrRustBindingNativeTypeStorage;

struct ZrRustBindingNativeModule {
    ZrLibModuleDescriptor descriptor;
    TZrSize refCount;
    TZrSize constantCapacity;
    ZrLibConstantDescriptor *constants;
    TZrSize functionCapacity;
    ZrLibFunctionDescriptor *functions;
    ZrRustBindingNativeCallbackStorage *functionCallbacks;
    TZrSize typeCapacity;
    ZrLibTypeDescriptor *types;
    ZrRustBindingNativeTypeStorage *typeStorage;
    TZrSize typeHintCapacity;
    ZrLibTypeHintDescriptor *typeHints;
    TZrSize moduleLinkCapacity;
    ZrLibModuleLinkDescriptor *moduleLinks;
};

struct ZrRustBindingNativeModuleBuilder {
    ZrRustBindingNativeModule *module;
};

typedef struct ZrRustBindingRuntimeRegistrationEntry {
    TZrSize refCount;
    struct ZrRustBindingRuntime *runtime;
    ZrRustBindingNativeModule *module;
    TZrBool active;
} ZrRustBindingRuntimeRegistrationEntry;

struct ZrRustBindingRuntimeNativeRegistry {
    ZrRustBindingRuntimeRegistrationEntry **entries;
    TZrSize count;
    TZrSize capacity;
};

struct ZrRustBindingRuntimeNativeModuleRegistration {
    ZrRustBindingRuntimeRegistrationEntry *entry;
};

struct ZrRustBindingNativeCallContext {
    ZrLibCallContext *context;
};

static void zr_rust_binding_runtime_registration_entry_release(
        ZrRustBindingRuntimeRegistrationEntry *entry);
static void zr_rust_binding_runtime_native_registry_remove_entry(
        ZrRustBindingRuntime *runtime,
        ZrRustBindingRuntimeRegistrationEntry *entry,
        TZrBool releaseRegistryReference);
static ZrRustBindingNativeModule *zr_rust_binding_native_module_new(const TZrChar *moduleName);
static TZrBool zr_rust_binding_module_reserve_constants(ZrRustBindingNativeModule *module, TZrSize minimumCapacity);
static TZrBool zr_rust_binding_module_reserve_functions(ZrRustBindingNativeModule *module, TZrSize minimumCapacity);
static TZrBool zr_rust_binding_module_reserve_types(ZrRustBindingNativeModule *module, TZrSize minimumCapacity);
static TZrBool zr_rust_binding_module_reserve_type_hints(ZrRustBindingNativeModule *module, TZrSize minimumCapacity);
static TZrBool zr_rust_binding_module_reserve_module_links(ZrRustBindingNativeModule *module, TZrSize minimumCapacity);
static TZrBool zr_rust_binding_copy_type_hint_descriptor(ZrLibTypeHintDescriptor *target,
                                                         const ZrRustBindingNativeTypeHintDescriptor *source);
static TZrBool zr_rust_binding_copy_module_link_descriptor(ZrLibModuleLinkDescriptor *target,
                                                           const ZrRustBindingNativeModuleLinkDescriptor *source);
static TZrBool zr_rust_binding_copy_constant_descriptor(ZrLibConstantDescriptor *target,
                                                        const ZrRustBindingNativeConstantDescriptor *source);
static TZrBool zr_rust_binding_copy_function_descriptor(
        ZrLibFunctionDescriptor *target,
        ZrRustBindingNativeCallbackStorage *callbackStorage,
        const ZrRustBindingNativeFunctionDescriptor *source);
static TZrBool zr_rust_binding_copy_type_descriptor(ZrLibTypeDescriptor *target,
                                                    ZrRustBindingNativeTypeStorage *storage,
                                                    const ZrRustBindingNativeTypeDescriptor *source);
static TZrBool zr_rust_binding_native_dispatch_function(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool zr_rust_binding_native_dispatch_method(ZrLibCallContext *context, SZrTypeValue *result);
static TZrBool zr_rust_binding_native_dispatch_meta_method(ZrLibCallContext *context, SZrTypeValue *result);

static TZrChar *zr_rust_binding_dup_optional_string(const TZrChar *value) {
    return value != ZR_NULL ? zr_rust_binding_strdup(value) : ZR_NULL;
}

static void zr_rust_binding_free_string_array(TZrChar **values, TZrSize count) {
    TZrSize index;

    if (values == ZR_NULL) {
        return;
    }

    for (index = 0; index < count; index++) {
        free(values[index]);
    }
    free(values);
}

void zr_rust_binding_native_module_retain(ZrRustBindingNativeModule *module) {
    if (module != ZR_NULL) {
        module->refCount++;
    }
}

static ZrRustBindingRuntimeNativeRegistry *zr_rust_binding_runtime_native_registry_ensure(
        ZrRustBindingRuntime *runtime) {
    if (runtime == ZR_NULL) {
        return ZR_NULL;
    }
    if (runtime->nativeRegistry == ZR_NULL) {
        runtime->nativeRegistry = (ZrRustBindingRuntimeNativeRegistry *)calloc(1, sizeof(*runtime->nativeRegistry));
    }
    return runtime->nativeRegistry;
}

static TZrBool zr_rust_binding_runtime_registry_reserve(ZrRustBindingRuntimeNativeRegistry *registry,
                                                        TZrSize minimumCapacity) {
    ZrRustBindingRuntimeRegistrationEntry **newEntries;
    TZrSize newCapacity;

    if (registry == ZR_NULL) {
        return ZR_FALSE;
    }
    if (registry->capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = registry->capacity == 0U ? 4U : registry->capacity * 2U;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2U;
    }

    newEntries = (ZrRustBindingRuntimeRegistrationEntry **)realloc(registry->entries,
                                                                   newCapacity * sizeof(*newEntries));
    if (newEntries == ZR_NULL) {
        return ZR_FALSE;
    }

    registry->entries = newEntries;
    registry->capacity = newCapacity;
    return ZR_TRUE;
}

static ZrRustBindingNativeModule *zr_rust_binding_native_module_from_context(
        const ZrLibCallContext *context) {
    return context != ZR_NULL && context->moduleDescriptor != ZR_NULL
                   ? (ZrRustBindingNativeModule *)context->moduleDescriptor
                   : ZR_NULL;
}

static TZrBool zr_rust_binding_native_callback_copy_string(const TZrChar *source,
                                                           TZrChar *buffer,
                                                           TZrSize bufferSize,
                                                           const TZrChar *fieldName) {
    if (!zr_rust_binding_copy_string_to_buffer(source != ZR_NULL ? source : "", buffer, bufferSize)) {
        zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL,
                                  "failed to copy native callback %s",
                                  fieldName);
        return ZR_FALSE;
    }
    zr_rust_binding_clear_error();
    return ZR_TRUE;
}

static ZrRustBindingExecutionOwner *zr_rust_binding_native_callback_owner_new(
        const ZrLibCallContext *context) {
    ZrRustBindingExecutionOwner *owner;
    ZrRustBindingNativeModule *module;

    if (context == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL) {
        return ZR_NULL;
    }

    owner = (ZrRustBindingExecutionOwner *)calloc(1, sizeof(*owner));
    if (owner == ZR_NULL) {
        return ZR_NULL;
    }

    owner->global = context->state->global;
    owner->ownsGlobal = ZR_FALSE;
    owner->refCount = 1U;
    module = zr_rust_binding_native_module_from_context(context);
    if (module != ZR_NULL) {
        owner->nativeModules = (ZrRustBindingNativeModule **)calloc(1, sizeof(*owner->nativeModules));
        if (owner->nativeModules == ZR_NULL) {
            free(owner);
            return ZR_NULL;
        }
        zr_rust_binding_native_module_retain(module);
        owner->nativeModules[0] = module;
        owner->nativeModuleCount = 1U;
    }
    return owner;
}

static ZrRustBindingStatus zr_rust_binding_native_call_context_get_value(
        const ZrRustBindingNativeCallContext *context,
        const SZrTypeValue *value,
        ZrRustBindingValue **outValue) {
    ZrRustBindingExecutionOwner *owner;
    ZrRustBindingValue *handle;

    if (context == ZR_NULL || value == ZR_NULL || outValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native call context value access is invalid");
    }

    owner = zr_rust_binding_native_callback_owner_new(context->context);
    if (owner == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to allocate native callback owner");
    }

    handle = zr_rust_binding_value_new_live(owner, value);
    zr_rust_binding_execution_owner_release(owner);
    if (handle == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to create native callback value handle");
    }

    *outValue = handle;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

static TZrBool zr_rust_binding_native_invoke_callback(const ZrLibCallContext *context,
                                                      const ZrRustBindingNativeCallbackStorage *callbackStorage,
                                                      const TZrChar *callableName,
                                                      SZrTypeValue *result) {
    ZrRustBindingNativeCallContext nativeContext;
    ZrRustBindingValue *callbackResult = ZR_NULL;
    ZrRustBindingStatus status;

    if (context == ZR_NULL || callbackStorage == ZR_NULL || callbackStorage->callback == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeContext.context = (ZrLibCallContext *)context;
    status = callbackStorage->callback(&nativeContext, callbackStorage->userData, &callbackResult);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        ZrCore_Log_Error(context->state,
                         "native callback failed for %s\n",
                         callableName != ZR_NULL ? callableName : "<native>");
        if (callbackResult != ZR_NULL) {
            ZrRustBinding_Value_Free(callbackResult);
        }
        return ZR_FALSE;
    }

    if (callbackResult == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        zr_rust_binding_clear_error();
        return ZR_TRUE;
    }

    if (!zr_rust_binding_materialize_value(context->state, callbackResult, result)) {
        ZrRustBinding_Value_Free(callbackResult);
        ZrCore_Log_Error(context->state,
                         "failed to materialize native callback result for %s\n",
                         callableName != ZR_NULL ? callableName : "<native>");
        return ZR_FALSE;
    }

    ZrRustBinding_Value_Free(callbackResult);
    zr_rust_binding_clear_error();
    return ZR_TRUE;
}

static const ZrRustBindingNativeCallbackStorage *zr_rust_binding_get_function_callback_storage(
        const ZrLibCallContext *context) {
    ZrRustBindingNativeModule *module;
    ptrdiff_t functionIndex;

    module = zr_rust_binding_native_module_from_context(context);
    if (module == ZR_NULL || module->functions == ZR_NULL || context == ZR_NULL || context->functionDescriptor == ZR_NULL) {
        return ZR_NULL;
    }

    functionIndex = context->functionDescriptor - module->functions;
    if (functionIndex < 0 || (TZrSize)functionIndex >= module->descriptor.functionCount) {
        return ZR_NULL;
    }

    return &module->functionCallbacks[functionIndex];
}

static const ZrRustBindingNativeTypeStorage *zr_rust_binding_get_type_storage(const ZrLibCallContext *context,
                                                                              TZrSize *outTypeIndex) {
    ZrRustBindingNativeModule *module;
    ptrdiff_t typeIndex;

    if (outTypeIndex != ZR_NULL) {
        *outTypeIndex = 0U;
    }

    module = zr_rust_binding_native_module_from_context(context);
    if (module == ZR_NULL || module->types == ZR_NULL || context == ZR_NULL || context->typeDescriptor == ZR_NULL) {
        return ZR_NULL;
    }

    typeIndex = context->typeDescriptor - module->types;
    if (typeIndex < 0 || (TZrSize)typeIndex >= module->descriptor.typeCount) {
        return ZR_NULL;
    }

    if (outTypeIndex != ZR_NULL) {
        *outTypeIndex = (TZrSize)typeIndex;
    }
    return &module->typeStorage[typeIndex];
}

static TZrBool zr_rust_binding_native_dispatch_method_common(const ZrLibCallContext *context,
                                                             SZrTypeValue *result,
                                                             TZrBool isMetaMethod) {
    const ZrRustBindingNativeTypeStorage *typeStorage;
    const ZrRustBindingNativeCallbackStorage *callbackStorage = ZR_NULL;
    TZrSize typeIndex = 0U;
    ptrdiff_t callbackIndex;
    const TZrChar *callableName = ZR_NULL;

    typeStorage = zr_rust_binding_get_type_storage(context, &typeIndex);
    if (typeStorage == ZR_NULL) {
        return ZR_FALSE;
    }

    if (isMetaMethod) {
        if (context == ZR_NULL || context->metaMethodDescriptor == ZR_NULL || typeStorage->metaMethods == ZR_NULL) {
            return ZR_FALSE;
        }
        callbackIndex = context->metaMethodDescriptor - typeStorage->metaMethods;
        if (callbackIndex < 0 || (TZrSize)callbackIndex >= context->moduleDescriptor->types[typeIndex].metaMethodCount) {
            return ZR_FALSE;
        }
        callbackStorage = &typeStorage->metaMethodCallbacks[callbackIndex];
        callableName = CZrMetaName[context->metaMethodDescriptor->metaType];
    } else {
        if (context == ZR_NULL || context->methodDescriptor == ZR_NULL || typeStorage->methods == ZR_NULL) {
            return ZR_FALSE;
        }
        callbackIndex = context->methodDescriptor - typeStorage->methods;
        if (callbackIndex < 0 || (TZrSize)callbackIndex >= context->moduleDescriptor->types[typeIndex].methodCount) {
            return ZR_FALSE;
        }
        callbackStorage = &typeStorage->methodCallbacks[callbackIndex];
        callableName = context->methodDescriptor->name;
    }

    return zr_rust_binding_native_invoke_callback(context, callbackStorage, callableName, result);
}

TZrBool zr_rust_binding_native_dispatch_function(ZrLibCallContext *context, SZrTypeValue *result) {
    const ZrRustBindingNativeCallbackStorage *callbackStorage =
            zr_rust_binding_get_function_callback_storage(context);
    return zr_rust_binding_native_invoke_callback(context,
                                                  callbackStorage,
                                                  context != ZR_NULL && context->functionDescriptor != ZR_NULL
                                                          ? context->functionDescriptor->name
                                                          : ZR_NULL,
                                                  result);
}

TZrBool zr_rust_binding_native_dispatch_method(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_rust_binding_native_dispatch_method_common(context, result, ZR_FALSE);
}

TZrBool zr_rust_binding_native_dispatch_meta_method(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_rust_binding_native_dispatch_method_common(context, result, ZR_TRUE);
}

TZrBool zr_rust_binding_runtime_register_native_modules(ZrRustBindingRuntime *runtime, SZrGlobalState *global) {
    TZrSize index;

    if (runtime == ZR_NULL || runtime->nativeRegistry == ZR_NULL || global == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < runtime->nativeRegistry->count; index++) {
        ZrRustBindingRuntimeRegistrationEntry *entry = runtime->nativeRegistry->entries[index];
        const TZrChar *registryError;

        if (entry == ZR_NULL || !entry->active || entry->module == ZR_NULL) {
            continue;
        }

        if (ZrLibrary_NativeRegistry_RegisterModule(global, &entry->module->descriptor)) {
            continue;
        }

        registryError = ZrLibrary_NativeRegistry_GetLastErrorMessage(global);
        zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_RUNTIME_ERROR,
                                  "failed to register native module %s: %s",
                                  entry->module->descriptor.moduleName != ZR_NULL
                                          ? entry->module->descriptor.moduleName
                                          : "<null>",
                                  registryError != ZR_NULL ? registryError : "native registry error");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_New(const TZrChar *moduleName,
                                                          ZrRustBindingNativeModuleBuilder **outBuilder) {
    ZrRustBindingNativeModuleBuilder *builder;

    if (moduleName == ZR_NULL || outBuilder == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "moduleName or outBuilder is null");
    }

    builder = (ZrRustBindingNativeModuleBuilder *)calloc(1, sizeof(*builder));
    if (builder == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to allocate native module builder");
    }

    builder->module = zr_rust_binding_native_module_new(moduleName);
    if (builder->module == ZR_NULL) {
        free(builder);
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to allocate native module");
    }

    *outBuilder = builder;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetDocumentation(ZrRustBindingNativeModuleBuilder *builder,
                                                                       const TZrChar *documentation) {
    TZrChar *copy;

    if (builder == ZR_NULL || builder->module == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "builder is null");
    }

    copy = zr_rust_binding_dup_optional_string(documentation);
    if (documentation != ZR_NULL && copy == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to copy native module documentation");
    }

    free((TZrChar *)builder->module->descriptor.documentation);
    builder->module->descriptor.documentation = copy;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetModuleVersion(ZrRustBindingNativeModuleBuilder *builder,
                                                                       const TZrChar *moduleVersion) {
    TZrChar *copy;

    if (builder == ZR_NULL || builder->module == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "builder is null");
    }

    copy = zr_rust_binding_dup_optional_string(moduleVersion);
    if (moduleVersion != ZR_NULL && copy == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to copy native module version");
    }

    free((TZrChar *)builder->module->descriptor.moduleVersion);
    builder->module->descriptor.moduleVersion = copy;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetTypeHintsJson(ZrRustBindingNativeModuleBuilder *builder,
                                                                       const TZrChar *typeHintsJson) {
    TZrChar *copy;

    if (builder == ZR_NULL || builder->module == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "builder is null");
    }

    copy = zr_rust_binding_dup_optional_string(typeHintsJson);
    if (typeHintsJson != ZR_NULL && copy == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to copy native module type hints json");
    }

    free((TZrChar *)builder->module->descriptor.typeHintsJson);
    builder->module->descriptor.typeHintsJson = copy;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetRuntimeRequirements(
        ZrRustBindingNativeModuleBuilder *builder,
        TZrUInt32 minRuntimeAbi,
        TZrUInt64 requiredCapabilities) {
    if (builder == ZR_NULL || builder->module == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "builder is null");
    }

    builder->module->descriptor.minRuntimeAbi = minRuntimeAbi;
    builder->module->descriptor.requiredCapabilities = requiredCapabilities;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddTypeHint(ZrRustBindingNativeModuleBuilder *builder,
                                                                  const ZrRustBindingNativeTypeHintDescriptor *descriptor) {
    if (builder == ZR_NULL || builder->module == ZR_NULL || descriptor == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "builder or descriptor is null");
    }
    if (!zr_rust_binding_module_reserve_type_hints(builder->module, builder->module->descriptor.typeHintCount + 1U) ||
        !zr_rust_binding_copy_type_hint_descriptor(&builder->module->typeHints[builder->module->descriptor.typeHintCount],
                                                   descriptor)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to add native type hint");
    }

    builder->module->descriptor.typeHintCount++;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddModuleLink(ZrRustBindingNativeModuleBuilder *builder,
                                                                    const ZrRustBindingNativeModuleLinkDescriptor *descriptor) {
    if (builder == ZR_NULL || builder->module == ZR_NULL || descriptor == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "builder or descriptor is null");
    }
    if (!zr_rust_binding_module_reserve_module_links(builder->module,
                                                     builder->module->descriptor.moduleLinkCount + 1U) ||
        !zr_rust_binding_copy_module_link_descriptor(
                &builder->module->moduleLinks[builder->module->descriptor.moduleLinkCount],
                descriptor)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to add native module link");
    }

    builder->module->descriptor.moduleLinkCount++;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddConstant(ZrRustBindingNativeModuleBuilder *builder,
                                                                  const ZrRustBindingNativeConstantDescriptor *descriptor) {
    if (builder == ZR_NULL || builder->module == ZR_NULL || descriptor == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "builder or descriptor is null");
    }
    if (!zr_rust_binding_module_reserve_constants(builder->module, builder->module->descriptor.constantCount + 1U) ||
        !zr_rust_binding_copy_constant_descriptor(
                &builder->module->constants[builder->module->descriptor.constantCount],
                descriptor)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to add native constant");
    }

    builder->module->descriptor.constantCount++;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddFunction(ZrRustBindingNativeModuleBuilder *builder,
                                                                  const ZrRustBindingNativeFunctionDescriptor *descriptor) {
    if (builder == ZR_NULL || builder->module == ZR_NULL || descriptor == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "builder or descriptor is null");
    }
    if (!zr_rust_binding_module_reserve_functions(builder->module, builder->module->descriptor.functionCount + 1U) ||
        !zr_rust_binding_copy_function_descriptor(
                &builder->module->functions[builder->module->descriptor.functionCount],
                &builder->module->functionCallbacks[builder->module->descriptor.functionCount],
                descriptor)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to add native function");
    }

    builder->module->descriptor.functionCount++;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddType(ZrRustBindingNativeModuleBuilder *builder,
                                                              const ZrRustBindingNativeTypeDescriptor *descriptor) {
    TZrSize typeIndex;

    if (builder == ZR_NULL || builder->module == ZR_NULL || descriptor == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "builder or descriptor is null");
    }
    if (!zr_rust_binding_module_reserve_types(builder->module, builder->module->descriptor.typeCount + 1U)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to reserve native type slot");
    }

    typeIndex = builder->module->descriptor.typeCount;
    memset(&builder->module->types[typeIndex], 0, sizeof(builder->module->types[typeIndex]));
    memset(&builder->module->typeStorage[typeIndex], 0, sizeof(builder->module->typeStorage[typeIndex]));
    if (!zr_rust_binding_copy_type_descriptor(&builder->module->types[typeIndex],
                                              &builder->module->typeStorage[typeIndex],
                                              descriptor)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to add native type");
    }

    builder->module->descriptor.typeCount++;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_Build(ZrRustBindingNativeModuleBuilder *builder,
                                                            ZrRustBindingNativeModule **outModule) {
    if (builder == ZR_NULL || builder->module == ZR_NULL || outModule == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "builder or outModule is null");
    }

    *outModule = builder->module;
    builder->module = ZR_NULL;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_Free(ZrRustBindingNativeModuleBuilder *builder) {
    if (builder != ZR_NULL) {
        zr_rust_binding_native_module_release(builder->module);
        free(builder);
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeModule_Free(ZrRustBindingNativeModule *module) {
    zr_rust_binding_native_module_release(module);
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Runtime_RegisterNativeModule(
        ZrRustBindingRuntime *runtime,
        ZrRustBindingNativeModule *module,
        ZrRustBindingRuntimeNativeModuleRegistration **outRegistration) {
    ZrRustBindingRuntimeNativeRegistry *registry;
    ZrRustBindingRuntimeRegistrationEntry *entry;
    ZrRustBindingRuntimeNativeModuleRegistration *registration;
    TZrSize index;

    if (runtime == ZR_NULL || module == ZR_NULL || outRegistration == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "runtime, module, or outRegistration is null");
    }

    registry = zr_rust_binding_runtime_native_registry_ensure(runtime);
    if (registry == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to allocate runtime native registry");
    }

    for (index = 0; index < registry->count; index++) {
        if (registry->entries[index] != ZR_NULL &&
            registry->entries[index]->active &&
            registry->entries[index]->module != ZR_NULL &&
            registry->entries[index]->module->descriptor.moduleName != ZR_NULL &&
            module->descriptor.moduleName != ZR_NULL &&
            strcmp(registry->entries[index]->module->descriptor.moduleName, module->descriptor.moduleName) == 0) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_ALREADY_EXISTS,
                                             "native module already registered on runtime: %s",
                                             module->descriptor.moduleName);
        }
    }

    if (!zr_rust_binding_runtime_registry_reserve(registry, registry->count + 1U)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to reserve runtime native registry entry");
    }

    entry = (ZrRustBindingRuntimeRegistrationEntry *)calloc(1, sizeof(*entry));
    registration = (ZrRustBindingRuntimeNativeModuleRegistration *)calloc(1, sizeof(*registration));
    if (entry == ZR_NULL || registration == ZR_NULL) {
        free(entry);
        free(registration);
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to allocate runtime native registration");
    }

    zr_rust_binding_native_module_retain(module);
    entry->refCount = 2U;
    entry->runtime = runtime;
    entry->module = module;
    entry->active = ZR_TRUE;
    registration->entry = entry;

    registry->entries[registry->count++] = entry;
    *outRegistration = registration;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_RuntimeNativeModuleRegistration_Free(
        ZrRustBindingRuntimeNativeModuleRegistration *registration) {
    if (registration != ZR_NULL && registration->entry != ZR_NULL) {
        if (registration->entry->active && registration->entry->runtime != ZR_NULL) {
            zr_rust_binding_runtime_native_registry_remove_entry(registration->entry->runtime,
                                                                 registration->entry,
                                                                 ZR_TRUE);
        }
        zr_rust_binding_runtime_registration_entry_release(registration->entry);
        registration->entry = ZR_NULL;
    }
    free(registration);
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetModuleName(
        const ZrRustBindingNativeCallContext *context,
        TZrChar *buffer,
        TZrSize bufferSize) {
    if (context == ZR_NULL || context->context == ZR_NULL || context->context->moduleDescriptor == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND, "native module name is unavailable");
    }
    if (!zr_rust_binding_native_callback_copy_string(context->context->moduleDescriptor->moduleName,
                                                     buffer,
                                                     bufferSize,
                                                     "moduleName")) {
        return ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL;
    }
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetTypeName(
        const ZrRustBindingNativeCallContext *context,
        TZrChar *buffer,
        TZrSize bufferSize) {
    if (context == ZR_NULL || context->context == ZR_NULL || context->context->typeDescriptor == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND, "native type name is unavailable");
    }
    if (!zr_rust_binding_native_callback_copy_string(context->context->typeDescriptor->name,
                                                     buffer,
                                                     bufferSize,
                                                     "typeName")) {
        return ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL;
    }
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetCallableName(
        const ZrRustBindingNativeCallContext *context,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const TZrChar *callableName = ZR_NULL;

    if (context == ZR_NULL || context->context == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "context is null");
    }
    if (context->context->functionDescriptor != ZR_NULL) {
        callableName = context->context->functionDescriptor->name;
    } else if (context->context->methodDescriptor != ZR_NULL) {
        callableName = context->context->methodDescriptor->name;
    } else if (context->context->metaMethodDescriptor != ZR_NULL) {
        callableName = CZrMetaName[context->context->metaMethodDescriptor->metaType];
    }
    if (callableName == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND, "native callable name is unavailable");
    }
    if (!zr_rust_binding_native_callback_copy_string(callableName, buffer, bufferSize, "callableName")) {
        return ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL;
    }
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetArgumentCount(
        const ZrRustBindingNativeCallContext *context,
        TZrSize *outArgumentCount) {
    if (context == ZR_NULL || context->context == ZR_NULL || outArgumentCount == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "context or outArgumentCount is null");
    }

    *outArgumentCount = ZrLib_CallContext_ArgumentCount(context->context);
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeCallContext_CheckArity(
        const ZrRustBindingNativeCallContext *context,
        TZrSize minArgumentCount,
        TZrSize maxArgumentCount) {
    if (context == ZR_NULL || context->context == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "context is null");
    }
    if (!ZrLib_CallContext_CheckArity(context->context, minArgumentCount, maxArgumentCount)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native callback arity mismatch");
    }

    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetArgument(
        const ZrRustBindingNativeCallContext *context,
        TZrSize index,
        ZrRustBindingValue **outArgumentValue) {
    const SZrTypeValue *argumentValue;

    if (context == ZR_NULL || context->context == ZR_NULL || outArgumentValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "context or outArgumentValue is null");
    }

    argumentValue = ZrLib_CallContext_Argument(context->context, index);
    if (argumentValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                                         "native callback argument index out of range");
    }

    return zr_rust_binding_native_call_context_get_value(context, argumentValue, outArgumentValue);
}

ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetSelf(
        const ZrRustBindingNativeCallContext *context,
        ZrRustBindingValue **outSelfValue) {
    SZrTypeValue *selfValue;

    if (context == ZR_NULL || context->context == ZR_NULL || outSelfValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "context or outSelfValue is null");
    }

    selfValue = ZrLib_CallContext_Self(context->context);
    if (selfValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND, "native callback self is unavailable");
    }

    return zr_rust_binding_native_call_context_get_value(context, selfValue, outSelfValue);
}

static void zr_rust_binding_runtime_registration_entry_retain(
        ZrRustBindingRuntimeRegistrationEntry *entry) {
    if (entry != ZR_NULL) {
        entry->refCount++;
    }
}

static void zr_rust_binding_runtime_registration_entry_release(
        ZrRustBindingRuntimeRegistrationEntry *entry);

static void zr_rust_binding_runtime_native_registry_remove_entry(
        ZrRustBindingRuntime *runtime,
        ZrRustBindingRuntimeRegistrationEntry *entry,
        TZrBool releaseRegistryReference) {
    TZrSize index;
    ZrRustBindingRuntimeNativeRegistry *registry;

    if (runtime == ZR_NULL || entry == ZR_NULL || runtime->nativeRegistry == ZR_NULL) {
        return;
    }

    registry = runtime->nativeRegistry;
    for (index = 0; index < registry->count; index++) {
        if (registry->entries[index] != entry) {
            continue;
        }

        if (index + 1U < registry->count) {
            memmove(&registry->entries[index],
                    &registry->entries[index + 1U],
                    (registry->count - index - 1U) * sizeof(*registry->entries));
        }
        registry->count--;
        entry->runtime = ZR_NULL;
        entry->active = ZR_FALSE;
        if (releaseRegistryReference) {
            zr_rust_binding_runtime_registration_entry_release(entry);
        }
        return;
    }
}

static void zr_rust_binding_free_parameter_array(ZrLibParameterDescriptor *parameters, TZrSize parameterCount) {
    TZrSize index;

    if (parameters == ZR_NULL) {
        return;
    }

    for (index = 0; index < parameterCount; index++) {
        free((TZrChar *)parameters[index].name);
        free((TZrChar *)parameters[index].typeName);
        free((TZrChar *)parameters[index].documentation);
    }
    free(parameters);
}

static void zr_rust_binding_free_generic_parameter_array(
        ZrRustBindingNativeGenericParameterStorage *genericParameters,
        TZrSize genericParameterCount) {
    TZrSize index;

    if (genericParameters == ZR_NULL) {
        return;
    }

    for (index = 0; index < genericParameterCount; index++) {
        free((TZrChar *)genericParameters[index].descriptor.name);
        free((TZrChar *)genericParameters[index].descriptor.documentation);
        zr_rust_binding_free_string_array(genericParameters[index].constraintTypeNames,
                                          genericParameters[index].descriptor.constraintTypeCount);
    }
    free(genericParameters);
}

static void zr_rust_binding_destroy_callback_storage(ZrRustBindingNativeCallbackStorage *callbacks, TZrSize count) {
    TZrSize index;

    if (callbacks == ZR_NULL) {
        return;
    }

    for (index = 0; index < count; index++) {
        if (callbacks[index].destroyUserData != ZR_NULL) {
            callbacks[index].destroyUserData(callbacks[index].userData);
        }
    }
}

static void zr_rust_binding_free_field_array(ZrLibFieldDescriptor *fields, TZrSize fieldCount) {
    TZrSize index;

    if (fields == ZR_NULL) {
        return;
    }

    for (index = 0; index < fieldCount; index++) {
        free((TZrChar *)fields[index].name);
        free((TZrChar *)fields[index].typeName);
        free((TZrChar *)fields[index].documentation);
    }
    free(fields);
}

static void zr_rust_binding_free_enum_member_array(ZrLibEnumMemberDescriptor *enumMembers, TZrSize enumMemberCount) {
    TZrSize index;

    if (enumMembers == ZR_NULL) {
        return;
    }

    for (index = 0; index < enumMemberCount; index++) {
        free((TZrChar *)enumMembers[index].name);
        free((TZrChar *)enumMembers[index].stringValue);
        free((TZrChar *)enumMembers[index].documentation);
    }
    free(enumMembers);
}

static void zr_rust_binding_free_constant_array(ZrLibConstantDescriptor *constants, TZrSize constantCount) {
    TZrSize index;

    if (constants == ZR_NULL) {
        return;
    }

    for (index = 0; index < constantCount; index++) {
        free((TZrChar *)constants[index].name);
        free((TZrChar *)constants[index].stringValue);
        free((TZrChar *)constants[index].documentation);
        free((TZrChar *)constants[index].typeName);
    }
    free(constants);
}

static void zr_rust_binding_free_function_array(ZrLibFunctionDescriptor *functions,
                                                ZrRustBindingNativeCallbackStorage *callbacks,
                                                TZrSize functionCount) {
    TZrSize index;

    if (functions != ZR_NULL) {
        for (index = 0; index < functionCount; index++) {
            free((TZrChar *)functions[index].name);
            free((TZrChar *)functions[index].returnTypeName);
            free((TZrChar *)functions[index].documentation);
            zr_rust_binding_free_parameter_array((ZrLibParameterDescriptor *)functions[index].parameters,
                                                 functions[index].parameterCount);
            zr_rust_binding_free_generic_parameter_array(
                    (ZrRustBindingNativeGenericParameterStorage *)functions[index].genericParameters,
                    functions[index].genericParameterCount);
        }
    }

    zr_rust_binding_destroy_callback_storage(callbacks, functionCount);
    free(functions);
    free(callbacks);
}

static void zr_rust_binding_free_method_array(ZrLibMethodDescriptor *methods,
                                              ZrRustBindingNativeCallbackStorage *callbacks,
                                              TZrSize methodCount) {
    TZrSize index;

    if (methods != ZR_NULL) {
        for (index = 0; index < methodCount; index++) {
            free((TZrChar *)methods[index].name);
            free((TZrChar *)methods[index].returnTypeName);
            free((TZrChar *)methods[index].documentation);
            zr_rust_binding_free_parameter_array((ZrLibParameterDescriptor *)methods[index].parameters,
                                                 methods[index].parameterCount);
            zr_rust_binding_free_generic_parameter_array(
                    (ZrRustBindingNativeGenericParameterStorage *)methods[index].genericParameters,
                    methods[index].genericParameterCount);
        }
    }

    zr_rust_binding_destroy_callback_storage(callbacks, methodCount);
    free(methods);
    free(callbacks);
}

static void zr_rust_binding_free_meta_method_array(ZrLibMetaMethodDescriptor *metaMethods,
                                                   ZrRustBindingNativeCallbackStorage *callbacks,
                                                   TZrSize metaMethodCount) {
    TZrSize index;

    if (metaMethods != ZR_NULL) {
        for (index = 0; index < metaMethodCount; index++) {
            free((TZrChar *)metaMethods[index].returnTypeName);
            free((TZrChar *)metaMethods[index].documentation);
            zr_rust_binding_free_parameter_array((ZrLibParameterDescriptor *)metaMethods[index].parameters,
                                                 metaMethods[index].parameterCount);
            zr_rust_binding_free_generic_parameter_array(
                    (ZrRustBindingNativeGenericParameterStorage *)metaMethods[index].genericParameters,
                    metaMethods[index].genericParameterCount);
        }
    }

    zr_rust_binding_destroy_callback_storage(callbacks, metaMethodCount);
    free(metaMethods);
    free(callbacks);
}

static void zr_rust_binding_free_type_hint_array(ZrLibTypeHintDescriptor *typeHints, TZrSize typeHintCount) {
    TZrSize index;

    if (typeHints == ZR_NULL) {
        return;
    }

    for (index = 0; index < typeHintCount; index++) {
        free((TZrChar *)typeHints[index].symbolName);
        free((TZrChar *)typeHints[index].symbolKind);
        free((TZrChar *)typeHints[index].signature);
        free((TZrChar *)typeHints[index].documentation);
    }
    free(typeHints);
}

static void zr_rust_binding_free_module_link_array(ZrLibModuleLinkDescriptor *moduleLinks, TZrSize moduleLinkCount) {
    TZrSize index;

    if (moduleLinks == ZR_NULL) {
        return;
    }

    for (index = 0; index < moduleLinkCount; index++) {
        free((TZrChar *)moduleLinks[index].name);
        free((TZrChar *)moduleLinks[index].moduleName);
        free((TZrChar *)moduleLinks[index].documentation);
    }
    free(moduleLinks);
}

static void zr_rust_binding_free_type_storage(ZrLibTypeDescriptor *descriptor,
                                              ZrRustBindingNativeTypeStorage *storage) {
    if (descriptor != ZR_NULL) {
        free((TZrChar *)descriptor->name);
        free((TZrChar *)descriptor->documentation);
        free((TZrChar *)descriptor->extendsTypeName);
        free((TZrChar *)descriptor->enumValueTypeName);
        free((TZrChar *)descriptor->constructorSignature);
        free((TZrChar *)descriptor->ffiLoweringKind);
        free((TZrChar *)descriptor->ffiViewTypeName);
        free((TZrChar *)descriptor->ffiUnderlyingTypeName);
        free((TZrChar *)descriptor->ffiOwnerMode);
        free((TZrChar *)descriptor->ffiReleaseHook);
    }
    if (storage == ZR_NULL) {
        return;
    }

    zr_rust_binding_free_field_array(storage->fields, descriptor != ZR_NULL ? descriptor->fieldCount : 0U);
    zr_rust_binding_free_method_array(storage->methods,
                                      storage->methodCallbacks,
                                      descriptor != ZR_NULL ? descriptor->methodCount : 0U);
    zr_rust_binding_free_meta_method_array(storage->metaMethods,
                                           storage->metaMethodCallbacks,
                                           descriptor != ZR_NULL ? descriptor->metaMethodCount : 0U);
    zr_rust_binding_free_string_array(storage->implementsTypeNames,
                                      descriptor != ZR_NULL ? descriptor->implementsTypeCount : 0U);
    zr_rust_binding_free_enum_member_array(storage->enumMembers,
                                           descriptor != ZR_NULL ? descriptor->enumMemberCount : 0U);
    zr_rust_binding_free_generic_parameter_array(storage->genericParameters,
                                                 descriptor != ZR_NULL ? descriptor->genericParameterCount : 0U);
}

static void zr_rust_binding_runtime_registration_entry_release(
        ZrRustBindingRuntimeRegistrationEntry *entry) {
    if (entry == ZR_NULL) {
        return;
    }

    ZR_ASSERT(entry->refCount > 0U);
    entry->refCount--;
    if (entry->refCount != 0U) {
        return;
    }

    zr_rust_binding_native_module_release(entry->module);
    free(entry);
}

void zr_rust_binding_native_module_release(ZrRustBindingNativeModule *module) {
    TZrSize index;

    if (module == ZR_NULL) {
        return;
    }

    ZR_ASSERT(module->refCount > 0U);
    module->refCount--;
    if (module->refCount != 0U) {
        return;
    }

    free((TZrChar *)module->descriptor.moduleName);
    free((TZrChar *)module->descriptor.typeHintsJson);
    free((TZrChar *)module->descriptor.documentation);
    free((TZrChar *)module->descriptor.moduleVersion);
    zr_rust_binding_free_constant_array(module->constants, module->descriptor.constantCount);
    zr_rust_binding_free_function_array(module->functions,
                                        module->functionCallbacks,
                                        module->descriptor.functionCount);
    if (module->types != ZR_NULL && module->typeStorage != ZR_NULL) {
        for (index = 0; index < module->descriptor.typeCount; index++) {
            zr_rust_binding_free_type_storage(&module->types[index], &module->typeStorage[index]);
        }
    }
    free(module->types);
    free(module->typeStorage);
    zr_rust_binding_free_type_hint_array(module->typeHints, module->descriptor.typeHintCount);
    zr_rust_binding_free_module_link_array(module->moduleLinks, module->descriptor.moduleLinkCount);
    free(module);
}

TZrBool zr_rust_binding_runtime_capture_native_modules(const ZrRustBindingRuntime *runtime,
                                                       ZrRustBindingNativeModule ***outModules,
                                                       TZrSize *outCount) {
    ZrRustBindingNativeModule **modules = ZR_NULL;
    TZrSize activeCount = 0U;
    TZrSize writeIndex = 0U;
    TZrSize index;

    if (outModules == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *outModules = ZR_NULL;
    *outCount = 0U;
    if (runtime == ZR_NULL || runtime->nativeRegistry == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < runtime->nativeRegistry->count; index++) {
        ZrRustBindingRuntimeRegistrationEntry *entry = runtime->nativeRegistry->entries[index];
        if (entry != ZR_NULL && entry->active && entry->module != ZR_NULL) {
            activeCount++;
        }
    }

    if (activeCount == 0U) {
        return ZR_TRUE;
    }

    modules = (ZrRustBindingNativeModule **)calloc(activeCount, sizeof(*modules));
    if (modules == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < runtime->nativeRegistry->count; index++) {
        ZrRustBindingRuntimeRegistrationEntry *entry = runtime->nativeRegistry->entries[index];
        if (entry == ZR_NULL || !entry->active || entry->module == ZR_NULL) {
            continue;
        }
        zr_rust_binding_native_module_retain(entry->module);
        modules[writeIndex++] = entry->module;
    }

    *outModules = modules;
    *outCount = activeCount;
    return ZR_TRUE;
}

void zr_rust_binding_runtime_native_registry_free(ZrRustBindingRuntime *runtime) {
    TZrSize index;

    if (runtime == ZR_NULL || runtime->nativeRegistry == ZR_NULL) {
        return;
    }

    for (index = 0; index < runtime->nativeRegistry->count; index++) {
        ZrRustBindingRuntimeRegistrationEntry *entry = runtime->nativeRegistry->entries[index];
        if (entry == ZR_NULL) {
            continue;
        }
        entry->runtime = ZR_NULL;
        entry->active = ZR_FALSE;
        zr_rust_binding_runtime_registration_entry_release(entry);
    }
    free(runtime->nativeRegistry->entries);
    free(runtime->nativeRegistry);
    runtime->nativeRegistry = ZR_NULL;
}

static ZrRustBindingNativeModule *zr_rust_binding_native_module_new(const TZrChar *moduleName) {
    ZrRustBindingNativeModule *module;

    if (moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    module = (ZrRustBindingNativeModule *)calloc(1, sizeof(*module));
    if (module == ZR_NULL) {
        return ZR_NULL;
    }

    module->descriptor.abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION;
    module->descriptor.minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION;
    module->descriptor.moduleName = zr_rust_binding_strdup(moduleName);
    module->refCount = 1U;
    if (module->descriptor.moduleName == ZR_NULL) {
        zr_rust_binding_native_module_release(module);
        return ZR_NULL;
    }
    return module;
}

static TZrBool zr_rust_binding_module_reserve_constants(ZrRustBindingNativeModule *module, TZrSize minimumCapacity) {
    ZrLibConstantDescriptor *newConstants;
    TZrSize newCapacity;

    if (module == ZR_NULL) {
        return ZR_FALSE;
    }
    if (module->constantCapacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = module->constantCapacity == 0U ? 4U : module->constantCapacity * 2U;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2U;
    }

    newConstants = (ZrLibConstantDescriptor *)realloc(module->constants, newCapacity * sizeof(*newConstants));
    if (newConstants == ZR_NULL) {
        return ZR_FALSE;
    }

    module->constants = newConstants;
    module->constantCapacity = newCapacity;
    module->descriptor.constants = module->constants;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_module_reserve_functions(ZrRustBindingNativeModule *module, TZrSize minimumCapacity) {
    ZrLibFunctionDescriptor *newFunctions;
    ZrRustBindingNativeCallbackStorage *newCallbacks;
    TZrSize newCapacity;

    if (module == ZR_NULL) {
        return ZR_FALSE;
    }
    if (module->functionCapacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = module->functionCapacity == 0U ? 4U : module->functionCapacity * 2U;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2U;
    }

    newFunctions = (ZrLibFunctionDescriptor *)realloc(module->functions, newCapacity * sizeof(*newFunctions));
    newCallbacks = (ZrRustBindingNativeCallbackStorage *)realloc(module->functionCallbacks,
                                                                 newCapacity * sizeof(*newCallbacks));
    if (newFunctions == ZR_NULL || newCallbacks == ZR_NULL) {
        free(newFunctions);
        free(newCallbacks);
        return ZR_FALSE;
    }

    module->functions = newFunctions;
    module->functionCallbacks = newCallbacks;
    module->functionCapacity = newCapacity;
    module->descriptor.functions = module->functions;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_module_reserve_types(ZrRustBindingNativeModule *module, TZrSize minimumCapacity) {
    ZrLibTypeDescriptor *newTypes;
    ZrRustBindingNativeTypeStorage *newStorage;
    TZrSize newCapacity;

    if (module == ZR_NULL) {
        return ZR_FALSE;
    }
    if (module->typeCapacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = module->typeCapacity == 0U ? 4U : module->typeCapacity * 2U;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2U;
    }

    newTypes = (ZrLibTypeDescriptor *)realloc(module->types, newCapacity * sizeof(*newTypes));
    newStorage = (ZrRustBindingNativeTypeStorage *)realloc(module->typeStorage, newCapacity * sizeof(*newStorage));
    if (newTypes == ZR_NULL || newStorage == ZR_NULL) {
        free(newTypes);
        free(newStorage);
        return ZR_FALSE;
    }

    module->types = newTypes;
    module->typeStorage = newStorage;
    module->typeCapacity = newCapacity;
    module->descriptor.types = module->types;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_module_reserve_type_hints(ZrRustBindingNativeModule *module, TZrSize minimumCapacity) {
    ZrLibTypeHintDescriptor *newTypeHints;
    TZrSize newCapacity;

    if (module == ZR_NULL) {
        return ZR_FALSE;
    }
    if (module->typeHintCapacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = module->typeHintCapacity == 0U ? 4U : module->typeHintCapacity * 2U;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2U;
    }

    newTypeHints = (ZrLibTypeHintDescriptor *)realloc(module->typeHints, newCapacity * sizeof(*newTypeHints));
    if (newTypeHints == ZR_NULL) {
        return ZR_FALSE;
    }

    module->typeHints = newTypeHints;
    module->typeHintCapacity = newCapacity;
    module->descriptor.typeHints = module->typeHints;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_module_reserve_module_links(ZrRustBindingNativeModule *module, TZrSize minimumCapacity) {
    ZrLibModuleLinkDescriptor *newModuleLinks;
    TZrSize newCapacity;

    if (module == ZR_NULL) {
        return ZR_FALSE;
    }
    if (module->moduleLinkCapacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = module->moduleLinkCapacity == 0U ? 4U : module->moduleLinkCapacity * 2U;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2U;
    }

    newModuleLinks = (ZrLibModuleLinkDescriptor *)realloc(module->moduleLinks,
                                                          newCapacity * sizeof(*newModuleLinks));
    if (newModuleLinks == ZR_NULL) {
        return ZR_FALSE;
    }

    module->moduleLinks = newModuleLinks;
    module->moduleLinkCapacity = newCapacity;
    module->descriptor.moduleLinks = module->moduleLinks;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_field_array(const ZrRustBindingNativeFieldDescriptor *source,
                                                TZrSize count,
                                                ZrLibFieldDescriptor **outFields);
static TZrBool zr_rust_binding_copy_method_array(const ZrRustBindingNativeMethodDescriptor *source,
                                                 TZrSize count,
                                                 ZrLibMethodDescriptor **outMethods,
                                                 ZrRustBindingNativeCallbackStorage **outCallbacks);
static TZrBool zr_rust_binding_copy_meta_method_array(const ZrRustBindingNativeMetaMethodDescriptor *source,
                                                      TZrSize count,
                                                      ZrLibMetaMethodDescriptor **outMetaMethods,
                                                      ZrRustBindingNativeCallbackStorage **outCallbacks);
static TZrBool zr_rust_binding_copy_function_descriptor(
        ZrLibFunctionDescriptor *target,
        ZrRustBindingNativeCallbackStorage *callbackStorage,
        const ZrRustBindingNativeFunctionDescriptor *source);
static TZrBool zr_rust_binding_copy_method_descriptor(
        ZrLibMethodDescriptor *target,
        ZrRustBindingNativeCallbackStorage *callbackStorage,
        const ZrRustBindingNativeMethodDescriptor *source);
static TZrBool zr_rust_binding_copy_meta_method_descriptor(
        ZrLibMetaMethodDescriptor *target,
        ZrRustBindingNativeCallbackStorage *callbackStorage,
        const ZrRustBindingNativeMetaMethodDescriptor *source);
static TZrBool zr_rust_binding_copy_type_descriptor(ZrLibTypeDescriptor *target,
                                                    ZrRustBindingNativeTypeStorage *storage,
                                                    const ZrRustBindingNativeTypeDescriptor *source);
TZrBool zr_rust_binding_native_dispatch_function(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_rust_binding_native_dispatch_method(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_rust_binding_native_dispatch_meta_method(ZrLibCallContext *context, SZrTypeValue *result);

static TZrBool zr_rust_binding_copy_parameter_array(const ZrRustBindingNativeParameterDescriptor *source,
                                                    TZrSize count,
                                                    ZrLibParameterDescriptor **outParameters) {
    ZrLibParameterDescriptor *parameters = ZR_NULL;
    TZrSize index;

    *outParameters = ZR_NULL;
    if (count == 0U) {
        return ZR_TRUE;
    }
    if (source == ZR_NULL) {
        return ZR_FALSE;
    }

    parameters = (ZrLibParameterDescriptor *)calloc(count, sizeof(*parameters));
    if (parameters == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < count; index++) {
        parameters[index].name = zr_rust_binding_dup_optional_string(source[index].name);
        parameters[index].typeName = zr_rust_binding_dup_optional_string(source[index].typeName);
        parameters[index].documentation = zr_rust_binding_dup_optional_string(source[index].documentation);
        if ((source[index].name != ZR_NULL && parameters[index].name == ZR_NULL) ||
            (source[index].typeName != ZR_NULL && parameters[index].typeName == ZR_NULL) ||
            (source[index].documentation != ZR_NULL && parameters[index].documentation == ZR_NULL)) {
            zr_rust_binding_free_parameter_array(parameters, count);
            return ZR_FALSE;
        }
    }

    *outParameters = parameters;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_generic_parameter_array(
        const ZrRustBindingNativeGenericParameterDescriptor *source,
        TZrSize count,
        ZrRustBindingNativeGenericParameterStorage **outGenericParameters) {
    ZrRustBindingNativeGenericParameterStorage *genericParameters = ZR_NULL;
    TZrSize index;
    TZrSize constraintIndex;

    *outGenericParameters = ZR_NULL;
    if (count == 0U) {
        return ZR_TRUE;
    }
    if (source == ZR_NULL) {
        return ZR_FALSE;
    }

    genericParameters = (ZrRustBindingNativeGenericParameterStorage *)calloc(count, sizeof(*genericParameters));
    if (genericParameters == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < count; index++) {
        genericParameters[index].descriptor.name = zr_rust_binding_dup_optional_string(source[index].name);
        genericParameters[index].descriptor.documentation =
                zr_rust_binding_dup_optional_string(source[index].documentation);
        genericParameters[index].descriptor.constraintTypeCount = source[index].constraintTypeCount;
        if ((source[index].name != ZR_NULL && genericParameters[index].descriptor.name == ZR_NULL) ||
            (source[index].documentation != ZR_NULL && genericParameters[index].descriptor.documentation == ZR_NULL)) {
            zr_rust_binding_free_generic_parameter_array(genericParameters, count);
            return ZR_FALSE;
        }

        if (source[index].constraintTypeCount > 0U) {
            if (source[index].constraintTypeNames == ZR_NULL) {
                zr_rust_binding_free_generic_parameter_array(genericParameters, count);
                return ZR_FALSE;
            }

            genericParameters[index].constraintTypeNames =
                    (TZrChar **)calloc(source[index].constraintTypeCount, sizeof(TZrChar *));
            if (genericParameters[index].constraintTypeNames == ZR_NULL) {
                zr_rust_binding_free_generic_parameter_array(genericParameters, count);
                return ZR_FALSE;
            }

            for (constraintIndex = 0; constraintIndex < source[index].constraintTypeCount; constraintIndex++) {
                genericParameters[index].constraintTypeNames[constraintIndex] =
                        zr_rust_binding_dup_optional_string(source[index].constraintTypeNames[constraintIndex]);
                if (source[index].constraintTypeNames[constraintIndex] != ZR_NULL &&
                    genericParameters[index].constraintTypeNames[constraintIndex] == ZR_NULL) {
                    zr_rust_binding_free_generic_parameter_array(genericParameters, count);
                    return ZR_FALSE;
                }
            }
            genericParameters[index].descriptor.constraintTypeNames =
                    (const TZrChar *const *)genericParameters[index].constraintTypeNames;
        }
    }

    *outGenericParameters = genericParameters;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_callback_storage(ZrRustBindingNativeCallbackStorage *target,
                                                     FZrRustBindingNativeCallback callback,
                                                     TZrPtr userData,
                                                     FZrRustBindingDestroyCallback destroyUserData) {
    if (target == ZR_NULL || callback == ZR_NULL) {
        return ZR_FALSE;
    }

    target->callback = callback;
    target->userData = userData;
    target->destroyUserData = destroyUserData;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_constant_descriptor(ZrLibConstantDescriptor *target,
                                                        const ZrRustBindingNativeConstantDescriptor *source) {
    if (target == ZR_NULL || source == ZR_NULL || source->name == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(target, 0, sizeof(*target));
    target->name = zr_rust_binding_strdup(source->name);
    target->kind = (EZrLibConstantKind)source->kind;
    target->intValue = source->intValue;
    target->floatValue = source->floatValue;
    target->stringValue = zr_rust_binding_dup_optional_string(source->stringValue);
    target->boolValue = source->boolValue;
    target->documentation = zr_rust_binding_dup_optional_string(source->documentation);
    target->typeName = zr_rust_binding_dup_optional_string(source->typeName);
    if (target->name == ZR_NULL ||
        (source->stringValue != ZR_NULL && target->stringValue == ZR_NULL) ||
        (source->documentation != ZR_NULL && target->documentation == ZR_NULL) ||
        (source->typeName != ZR_NULL && target->typeName == ZR_NULL)) {
        zr_rust_binding_free_constant_array(target, 1U);
        memset(target, 0, sizeof(*target));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_type_hint_descriptor(ZrLibTypeHintDescriptor *target,
                                                         const ZrRustBindingNativeTypeHintDescriptor *source) {
    if (target == ZR_NULL || source == ZR_NULL || source->symbolName == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(target, 0, sizeof(*target));
    target->symbolName = zr_rust_binding_strdup(source->symbolName);
    target->symbolKind = zr_rust_binding_dup_optional_string(source->symbolKind);
    target->signature = zr_rust_binding_dup_optional_string(source->signature);
    target->documentation = zr_rust_binding_dup_optional_string(source->documentation);
    if (target->symbolName == ZR_NULL ||
        (source->symbolKind != ZR_NULL && target->symbolKind == ZR_NULL) ||
        (source->signature != ZR_NULL && target->signature == ZR_NULL) ||
        (source->documentation != ZR_NULL && target->documentation == ZR_NULL)) {
        zr_rust_binding_free_type_hint_array(target, 1U);
        memset(target, 0, sizeof(*target));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_module_link_descriptor(ZrLibModuleLinkDescriptor *target,
                                                           const ZrRustBindingNativeModuleLinkDescriptor *source) {
    if (target == ZR_NULL || source == ZR_NULL || source->name == ZR_NULL || source->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(target, 0, sizeof(*target));
    target->name = zr_rust_binding_strdup(source->name);
    target->moduleName = zr_rust_binding_strdup(source->moduleName);
    target->documentation = zr_rust_binding_dup_optional_string(source->documentation);
    if (target->name == ZR_NULL || target->moduleName == ZR_NULL ||
        (source->documentation != ZR_NULL && target->documentation == ZR_NULL)) {
        zr_rust_binding_free_module_link_array(target, 1U);
        memset(target, 0, sizeof(*target));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_function_descriptor(
        ZrLibFunctionDescriptor *target,
        ZrRustBindingNativeCallbackStorage *callbackStorage,
        const ZrRustBindingNativeFunctionDescriptor *source) {
    ZrLibParameterDescriptor *parameters = ZR_NULL;
    ZrRustBindingNativeGenericParameterStorage *genericParameters = ZR_NULL;

    if (target == ZR_NULL || callbackStorage == ZR_NULL || source == ZR_NULL ||
        source->name == ZR_NULL || source->callback == ZR_NULL ||
        source->minArgumentCount > source->maxArgumentCount) {
        return ZR_FALSE;
    }

    if (!zr_rust_binding_copy_parameter_array(source->parameters, source->parameterCount, &parameters) ||
        !zr_rust_binding_copy_generic_parameter_array(source->genericParameters,
                                                      source->genericParameterCount,
                                                      &genericParameters) ||
        !zr_rust_binding_copy_callback_storage(callbackStorage,
                                              source->callback,
                                              source->userData,
                                              source->destroyUserData)) {
        zr_rust_binding_free_parameter_array(parameters, source->parameterCount);
        zr_rust_binding_free_generic_parameter_array(genericParameters, source->genericParameterCount);
        return ZR_FALSE;
    }

    memset(target, 0, sizeof(*target));
    target->name = zr_rust_binding_strdup(source->name);
    target->minArgumentCount = source->minArgumentCount;
    target->maxArgumentCount = source->maxArgumentCount;
    target->callback = zr_rust_binding_native_dispatch_function;
    target->returnTypeName = zr_rust_binding_dup_optional_string(source->returnTypeName);
    target->documentation = zr_rust_binding_dup_optional_string(source->documentation);
    target->parameters = parameters;
    target->parameterCount = source->parameterCount;
    target->genericParameters = (const ZrLibGenericParameterDescriptor *)genericParameters;
    target->genericParameterCount = source->genericParameterCount;
    target->contractRole = source->contractRole;
    if (target->name == ZR_NULL ||
        (source->returnTypeName != ZR_NULL && target->returnTypeName == ZR_NULL) ||
        (source->documentation != ZR_NULL && target->documentation == ZR_NULL)) {
        zr_rust_binding_free_parameter_array(parameters, source->parameterCount);
        zr_rust_binding_free_generic_parameter_array(genericParameters, source->genericParameterCount);
        memset(callbackStorage, 0, sizeof(*callbackStorage));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_method_descriptor(
        ZrLibMethodDescriptor *target,
        ZrRustBindingNativeCallbackStorage *callbackStorage,
        const ZrRustBindingNativeMethodDescriptor *source) {
    ZrLibParameterDescriptor *parameters = ZR_NULL;
    ZrRustBindingNativeGenericParameterStorage *genericParameters = ZR_NULL;

    if (target == ZR_NULL || callbackStorage == ZR_NULL || source == ZR_NULL ||
        source->name == ZR_NULL || source->callback == ZR_NULL ||
        source->minArgumentCount > source->maxArgumentCount) {
        return ZR_FALSE;
    }

    if (!zr_rust_binding_copy_parameter_array(source->parameters, source->parameterCount, &parameters) ||
        !zr_rust_binding_copy_generic_parameter_array(source->genericParameters,
                                                      source->genericParameterCount,
                                                      &genericParameters) ||
        !zr_rust_binding_copy_callback_storage(callbackStorage,
                                              source->callback,
                                              source->userData,
                                              source->destroyUserData)) {
        zr_rust_binding_free_parameter_array(parameters, source->parameterCount);
        zr_rust_binding_free_generic_parameter_array(genericParameters, source->genericParameterCount);
        return ZR_FALSE;
    }

    memset(target, 0, sizeof(*target));
    target->name = zr_rust_binding_strdup(source->name);
    target->minArgumentCount = source->minArgumentCount;
    target->maxArgumentCount = source->maxArgumentCount;
    target->callback = zr_rust_binding_native_dispatch_method;
    target->returnTypeName = zr_rust_binding_dup_optional_string(source->returnTypeName);
    target->documentation = zr_rust_binding_dup_optional_string(source->documentation);
    target->isStatic = source->isStatic;
    target->parameters = parameters;
    target->parameterCount = source->parameterCount;
    target->contractRole = source->contractRole;
    target->genericParameters = (const ZrLibGenericParameterDescriptor *)genericParameters;
    target->genericParameterCount = source->genericParameterCount;
    if (target->name == ZR_NULL ||
        (source->returnTypeName != ZR_NULL && target->returnTypeName == ZR_NULL) ||
        (source->documentation != ZR_NULL && target->documentation == ZR_NULL)) {
        zr_rust_binding_free_parameter_array(parameters, source->parameterCount);
        zr_rust_binding_free_generic_parameter_array(genericParameters, source->genericParameterCount);
        memset(callbackStorage, 0, sizeof(*callbackStorage));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_meta_method_descriptor(
        ZrLibMetaMethodDescriptor *target,
        ZrRustBindingNativeCallbackStorage *callbackStorage,
        const ZrRustBindingNativeMetaMethodDescriptor *source) {
    ZrLibParameterDescriptor *parameters = ZR_NULL;
    ZrRustBindingNativeGenericParameterStorage *genericParameters = ZR_NULL;

    if (target == ZR_NULL || callbackStorage == ZR_NULL || source == ZR_NULL || source->callback == ZR_NULL ||
        source->minArgumentCount > source->maxArgumentCount ||
        (TZrUInt32)source->metaType >= (TZrUInt32)ZR_META_ENUM_MAX) {
        return ZR_FALSE;
    }

    if (!zr_rust_binding_copy_parameter_array(source->parameters, source->parameterCount, &parameters) ||
        !zr_rust_binding_copy_generic_parameter_array(source->genericParameters,
                                                      source->genericParameterCount,
                                                      &genericParameters) ||
        !zr_rust_binding_copy_callback_storage(callbackStorage,
                                              source->callback,
                                              source->userData,
                                              source->destroyUserData)) {
        zr_rust_binding_free_parameter_array(parameters, source->parameterCount);
        zr_rust_binding_free_generic_parameter_array(genericParameters, source->genericParameterCount);
        return ZR_FALSE;
    }

    memset(target, 0, sizeof(*target));
    target->metaType = (EZrMetaType)source->metaType;
    target->minArgumentCount = source->minArgumentCount;
    target->maxArgumentCount = source->maxArgumentCount;
    target->callback = zr_rust_binding_native_dispatch_meta_method;
    target->returnTypeName = zr_rust_binding_dup_optional_string(source->returnTypeName);
    target->documentation = zr_rust_binding_dup_optional_string(source->documentation);
    target->parameters = parameters;
    target->parameterCount = source->parameterCount;
    target->genericParameters = (const ZrLibGenericParameterDescriptor *)genericParameters;
    target->genericParameterCount = source->genericParameterCount;
    if ((source->returnTypeName != ZR_NULL && target->returnTypeName == ZR_NULL) ||
        (source->documentation != ZR_NULL && target->documentation == ZR_NULL)) {
        zr_rust_binding_free_parameter_array(parameters, source->parameterCount);
        zr_rust_binding_free_generic_parameter_array(genericParameters, source->genericParameterCount);
        memset(callbackStorage, 0, sizeof(*callbackStorage));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_field_array(const ZrRustBindingNativeFieldDescriptor *source,
                                                TZrSize count,
                                                ZrLibFieldDescriptor **outFields) {
    ZrLibFieldDescriptor *fields = ZR_NULL;
    TZrSize index;

    *outFields = ZR_NULL;
    if (count == 0U) {
        return ZR_TRUE;
    }
    if (source == ZR_NULL) {
        return ZR_FALSE;
    }

    fields = (ZrLibFieldDescriptor *)calloc(count, sizeof(*fields));
    if (fields == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < count; index++) {
        if (source[index].name == ZR_NULL) {
            zr_rust_binding_free_field_array(fields, count);
            return ZR_FALSE;
        }
        fields[index].name = zr_rust_binding_strdup(source[index].name);
        fields[index].typeName = zr_rust_binding_dup_optional_string(source[index].typeName);
        fields[index].documentation = zr_rust_binding_dup_optional_string(source[index].documentation);
        fields[index].contractRole = source[index].contractRole;
        if (fields[index].name == ZR_NULL ||
            (source[index].typeName != ZR_NULL && fields[index].typeName == ZR_NULL) ||
            (source[index].documentation != ZR_NULL && fields[index].documentation == ZR_NULL)) {
            zr_rust_binding_free_field_array(fields, count);
            return ZR_FALSE;
        }
    }
    *outFields = fields;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_function_array(const ZrRustBindingNativeFunctionDescriptor *source,
                                                   TZrSize count,
                                                   ZrLibFunctionDescriptor **outFunctions,
                                                   ZrRustBindingNativeCallbackStorage **outCallbacks) {
    ZrLibFunctionDescriptor *functions = ZR_NULL;
    ZrRustBindingNativeCallbackStorage *callbacks = ZR_NULL;
    TZrSize index;

    *outFunctions = ZR_NULL;
    *outCallbacks = ZR_NULL;
    if (count == 0U) {
        return ZR_TRUE;
    }
    if (source == ZR_NULL) {
        return ZR_FALSE;
    }

    functions = (ZrLibFunctionDescriptor *)calloc(count, sizeof(*functions));
    callbacks = (ZrRustBindingNativeCallbackStorage *)calloc(count, sizeof(*callbacks));
    if (functions == ZR_NULL || callbacks == ZR_NULL) {
        zr_rust_binding_free_function_array(functions, callbacks, count);
        return ZR_FALSE;
    }

    for (index = 0; index < count; index++) {
        if (!zr_rust_binding_copy_function_descriptor(&functions[index], &callbacks[index], &source[index])) {
            zr_rust_binding_free_function_array(functions, callbacks, count);
            return ZR_FALSE;
        }
    }

    *outFunctions = functions;
    *outCallbacks = callbacks;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_method_array(const ZrRustBindingNativeMethodDescriptor *source,
                                                 TZrSize count,
                                                 ZrLibMethodDescriptor **outMethods,
                                                 ZrRustBindingNativeCallbackStorage **outCallbacks) {
    ZrLibMethodDescriptor *methods = ZR_NULL;
    ZrRustBindingNativeCallbackStorage *callbacks = ZR_NULL;
    TZrSize index;

    *outMethods = ZR_NULL;
    *outCallbacks = ZR_NULL;
    if (count == 0U) {
        return ZR_TRUE;
    }
    if (source == ZR_NULL) {
        return ZR_FALSE;
    }

    methods = (ZrLibMethodDescriptor *)calloc(count, sizeof(*methods));
    callbacks = (ZrRustBindingNativeCallbackStorage *)calloc(count, sizeof(*callbacks));
    if (methods == ZR_NULL || callbacks == ZR_NULL) {
        zr_rust_binding_free_method_array(methods, callbacks, count);
        return ZR_FALSE;
    }

    for (index = 0; index < count; index++) {
        if (!zr_rust_binding_copy_method_descriptor(&methods[index], &callbacks[index], &source[index])) {
            zr_rust_binding_free_method_array(methods, callbacks, count);
            return ZR_FALSE;
        }
    }

    *outMethods = methods;
    *outCallbacks = callbacks;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_meta_method_array(const ZrRustBindingNativeMetaMethodDescriptor *source,
                                                      TZrSize count,
                                                      ZrLibMetaMethodDescriptor **outMetaMethods,
                                                      ZrRustBindingNativeCallbackStorage **outCallbacks) {
    ZrLibMetaMethodDescriptor *metaMethods = ZR_NULL;
    ZrRustBindingNativeCallbackStorage *callbacks = ZR_NULL;
    TZrSize index;

    *outMetaMethods = ZR_NULL;
    *outCallbacks = ZR_NULL;
    if (count == 0U) {
        return ZR_TRUE;
    }
    if (source == ZR_NULL) {
        return ZR_FALSE;
    }

    metaMethods = (ZrLibMetaMethodDescriptor *)calloc(count, sizeof(*metaMethods));
    callbacks = (ZrRustBindingNativeCallbackStorage *)calloc(count, sizeof(*callbacks));
    if (metaMethods == ZR_NULL || callbacks == ZR_NULL) {
        zr_rust_binding_free_meta_method_array(metaMethods, callbacks, count);
        return ZR_FALSE;
    }

    for (index = 0; index < count; index++) {
        if (!zr_rust_binding_copy_meta_method_descriptor(&metaMethods[index], &callbacks[index], &source[index])) {
            zr_rust_binding_free_meta_method_array(metaMethods, callbacks, count);
            return ZR_FALSE;
        }
    }

    *outMetaMethods = metaMethods;
    *outCallbacks = callbacks;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_enum_member_descriptor(ZrLibEnumMemberDescriptor *target,
                                                           const ZrRustBindingNativeEnumMemberDescriptor *source) {
    if (target == ZR_NULL || source == ZR_NULL || source->name == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(target, 0, sizeof(*target));
    target->name = zr_rust_binding_strdup(source->name);
    target->kind = (EZrLibConstantKind)source->kind;
    target->intValue = source->intValue;
    target->floatValue = source->floatValue;
    target->stringValue = zr_rust_binding_dup_optional_string(source->stringValue);
    target->boolValue = source->boolValue;
    target->documentation = zr_rust_binding_dup_optional_string(source->documentation);
    if (target->name == ZR_NULL ||
        (source->stringValue != ZR_NULL && target->stringValue == ZR_NULL) ||
        (source->documentation != ZR_NULL && target->documentation == ZR_NULL)) {
        zr_rust_binding_free_enum_member_array(target, 1U);
        memset(target, 0, sizeof(*target));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_rust_binding_copy_type_descriptor(ZrLibTypeDescriptor *target,
                                                    ZrRustBindingNativeTypeStorage *storage,
                                                    const ZrRustBindingNativeTypeDescriptor *source) {
    TZrSize index;

    if (target == ZR_NULL || storage == ZR_NULL || source == ZR_NULL || source->name == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((TZrUInt32)source->prototypeType >= (TZrUInt32)ZR_OBJECT_PROTOTYPE_TYPE_MAX) {
        return ZR_FALSE;
    }

    memset(target, 0, sizeof(*target));
    memset(storage, 0, sizeof(*storage));
    target->name = zr_rust_binding_strdup(source->name);
    target->prototypeType = (EZrObjectPrototypeType)source->prototypeType;
    target->documentation = zr_rust_binding_dup_optional_string(source->documentation);
    target->extendsTypeName = zr_rust_binding_dup_optional_string(source->extendsTypeName);
    target->enumValueTypeName = zr_rust_binding_dup_optional_string(source->enumValueTypeName);
    target->allowValueConstruction = source->allowValueConstruction;
    target->allowBoxedConstruction = source->allowBoxedConstruction;
    target->constructorSignature = zr_rust_binding_dup_optional_string(source->constructorSignature);
    target->protocolMask = source->protocolMask;
    target->ffiLoweringKind = zr_rust_binding_dup_optional_string(source->ffiLoweringKind);
    target->ffiViewTypeName = zr_rust_binding_dup_optional_string(source->ffiViewTypeName);
    target->ffiUnderlyingTypeName = zr_rust_binding_dup_optional_string(source->ffiUnderlyingTypeName);
    target->ffiOwnerMode = zr_rust_binding_dup_optional_string(source->ffiOwnerMode);
    target->ffiReleaseHook = zr_rust_binding_dup_optional_string(source->ffiReleaseHook);
    if (target->name == ZR_NULL ||
        (source->documentation != ZR_NULL && target->documentation == ZR_NULL) ||
        (source->extendsTypeName != ZR_NULL && target->extendsTypeName == ZR_NULL) ||
        (source->enumValueTypeName != ZR_NULL && target->enumValueTypeName == ZR_NULL) ||
        (source->constructorSignature != ZR_NULL && target->constructorSignature == ZR_NULL) ||
        (source->ffiLoweringKind != ZR_NULL && target->ffiLoweringKind == ZR_NULL) ||
        (source->ffiViewTypeName != ZR_NULL && target->ffiViewTypeName == ZR_NULL) ||
        (source->ffiUnderlyingTypeName != ZR_NULL && target->ffiUnderlyingTypeName == ZR_NULL) ||
        (source->ffiOwnerMode != ZR_NULL && target->ffiOwnerMode == ZR_NULL) ||
        (source->ffiReleaseHook != ZR_NULL && target->ffiReleaseHook == ZR_NULL)) {
        zr_rust_binding_free_type_storage(target, storage);
        memset(target, 0, sizeof(*target));
        memset(storage, 0, sizeof(*storage));
        return ZR_FALSE;
    }

    if (!zr_rust_binding_copy_field_array(source->fields, source->fieldCount, &storage->fields) ||
        !zr_rust_binding_copy_method_array(source->methods,
                                           source->methodCount,
                                           &storage->methods,
                                           &storage->methodCallbacks) ||
        !zr_rust_binding_copy_meta_method_array(source->metaMethods,
                                                source->metaMethodCount,
                                                &storage->metaMethods,
                                                &storage->metaMethodCallbacks) ||
        !zr_rust_binding_copy_generic_parameter_array(source->genericParameters,
                                                      source->genericParameterCount,
                                                      &storage->genericParameters)) {
        zr_rust_binding_free_type_storage(target, storage);
        memset(target, 0, sizeof(*target));
        memset(storage, 0, sizeof(*storage));
        return ZR_FALSE;
    }

    target->fields = storage->fields;
    target->fieldCount = source->fieldCount;
    target->methods = storage->methods;
    target->methodCount = source->methodCount;
    target->metaMethods = storage->metaMethods;
    target->metaMethodCount = source->metaMethodCount;
    target->genericParameters = (const ZrLibGenericParameterDescriptor *)storage->genericParameters;
    target->genericParameterCount = source->genericParameterCount;

    if (source->implementsTypeCount > 0U) {
        if (source->implementsTypeNames == ZR_NULL) {
            zr_rust_binding_free_type_storage(target, storage);
            memset(target, 0, sizeof(*target));
            memset(storage, 0, sizeof(*storage));
            return ZR_FALSE;
        }
        storage->implementsTypeNames = (TZrChar **)calloc(source->implementsTypeCount, sizeof(TZrChar *));
        if (storage->implementsTypeNames == ZR_NULL) {
            zr_rust_binding_free_type_storage(target, storage);
            memset(target, 0, sizeof(*target));
            memset(storage, 0, sizeof(*storage));
            return ZR_FALSE;
        }
        for (index = 0; index < source->implementsTypeCount; index++) {
            storage->implementsTypeNames[index] = zr_rust_binding_dup_optional_string(source->implementsTypeNames[index]);
            if (source->implementsTypeNames[index] != ZR_NULL && storage->implementsTypeNames[index] == ZR_NULL) {
                zr_rust_binding_free_type_storage(target, storage);
                memset(target, 0, sizeof(*target));
                memset(storage, 0, sizeof(*storage));
                return ZR_FALSE;
            }
        }
        target->implementsTypeNames = (const TZrChar *const *)storage->implementsTypeNames;
        target->implementsTypeCount = source->implementsTypeCount;
    }

    if (source->enumMemberCount > 0U) {
        if (source->enumMembers == ZR_NULL) {
            zr_rust_binding_free_type_storage(target, storage);
            memset(target, 0, sizeof(*target));
            memset(storage, 0, sizeof(*storage));
            return ZR_FALSE;
        }
        storage->enumMembers = (ZrLibEnumMemberDescriptor *)calloc(source->enumMemberCount, sizeof(*storage->enumMembers));
        if (storage->enumMembers == ZR_NULL) {
            zr_rust_binding_free_type_storage(target, storage);
            memset(target, 0, sizeof(*target));
            memset(storage, 0, sizeof(*storage));
            return ZR_FALSE;
        }
        for (index = 0; index < source->enumMemberCount; index++) {
            if (!zr_rust_binding_copy_enum_member_descriptor(&storage->enumMembers[index], &source->enumMembers[index])) {
                zr_rust_binding_free_type_storage(target, storage);
                memset(target, 0, sizeof(*target));
                memset(storage, 0, sizeof(*storage));
                return ZR_FALSE;
            }
        }
        target->enumMembers = storage->enumMembers;
        target->enumMemberCount = source->enumMemberCount;
    }

    return ZR_TRUE;
}
