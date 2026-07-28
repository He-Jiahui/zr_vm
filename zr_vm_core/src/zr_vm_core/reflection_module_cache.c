#include "zr_vm_core/reflection.h"

#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include "reflection_module_internal.h"
#include "reflection_bound_runtime_native_internal.h"
#include "reflection_construction_native_internal.h"
#include "reflection_object_internal.h"
#include "reflection_type_resolve_native_internal.h"

static TZrBool reflection_cached_export_is_valid(
        SZrState *state,
        SZrObjectModule *serviceModule,
        SZrString *exportName,
        FZrNativeFunction expectedFunction,
        SZrObjectModule *runtimeModule) {
    const SZrTypeValue *exportValue;
    SZrClosureNative *closure;

    exportValue = ZrCore_Module_GetPubExport(state, serviceModule, exportName);
    if (exportValue == ZR_NULL || exportValue->type != ZR_VALUE_TYPE_CLOSURE ||
        !exportValue->isNative || !exportValue->isGarbageCollectable ||
        exportValue->value.object == ZR_NULL ||
        exportValue->value.object->type != ZR_RAW_OBJECT_TYPE_CLOSURE ||
        !exportValue->value.object->isNative) {
        return ZR_FALSE;
    }
    closure = ZR_CAST_NATIVE_CLOSURE(state, exportValue->value.object);
    return ZrCore_Reflection_BoundRuntimeNativeClosureIsValidInternal(
            state, closure, expectedFunction, runtimeModule);
}

static TZrBool reflection_cached_module_is_valid(
        SZrState *state,
        SZrObjectModule *serviceModule,
        SZrString *makeExportName,
        SZrString *resolveExportName,
        SZrString *requireExportName,
        SZrString *createExportName,
        SZrObjectModule *runtimeModule) {
    const TZrChar *moduleName;
    const TZrChar *fullPath;

    if (state == ZR_NULL || serviceModule == ZR_NULL || makeExportName == ZR_NULL ||
        resolveExportName == ZR_NULL || requireExportName == ZR_NULL ||
        createExportName == ZR_NULL || runtimeModule == ZR_NULL ||
        serviceModule->super.super.type != ZR_RAW_OBJECT_TYPE_OBJECT ||
        serviceModule->super.super.isNative ||
        serviceModule->super.internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE ||
        serviceModule->super.prototype != ZR_NULL ||
        serviceModule->initState != ZR_MODULE_INIT_STATE_READY ||
        serviceModule->hasMetadataRuntime ||
        serviceModule->moduleName == ZR_NULL || serviceModule->fullPath == ZR_NULL ||
        serviceModule->moduleName->super.type != ZR_RAW_OBJECT_TYPE_STRING ||
        serviceModule->fullPath->super.type != ZR_RAW_OBJECT_TYPE_STRING ||
        !serviceModule->super.nodeMap.isValid ||
        serviceModule->super.nodeMap.buckets == ZR_NULL ||
        serviceModule->super.nodeMap.capacity == 0u ||
        serviceModule->super.nodeMap.elementCount != 4u) {
        return ZR_FALSE;
    }

    moduleName = ZrCore_String_GetNativeString(serviceModule->moduleName);
    fullPath = ZrCore_String_GetNativeString(serviceModule->fullPath);
    if (moduleName == ZR_NULL || fullPath == ZR_NULL ||
        strcmp(moduleName, ZR_REFLECTION_MODULE_NAME) != 0 ||
        strcmp(fullPath, ZR_REFLECTION_MODULE_NAME) != 0 ||
        serviceModule->pathHash != ZrCore_Module_CalculatePathHash(
                state, serviceModule->fullPath)) {
        return ZR_FALSE;
    }

    return (TZrBool)(
            reflection_cached_export_is_valid(
                    state,
                    serviceModule,
                    makeExportName,
                    ZrCore_Reflection_MakeGenericMethodNativeEntry,
                    runtimeModule) &&
            reflection_cached_export_is_valid(
                    state,
                    serviceModule,
                    resolveExportName,
                    ZrCore_Reflection_ResolveTypeIdNativeEntryInternal,
                    runtimeModule) &&
            reflection_cached_export_is_valid(
                    state,
                    serviceModule,
                    requireExportName,
                    ZrCore_Reflection_RequireConstructibleNativeEntryInternal,
                    runtimeModule) &&
            reflection_cached_export_is_valid(
                    state,
                    serviceModule,
                    createExportName,
                    ZrCore_Reflection_CreateInstanceNativeEntryInternal,
                    runtimeModule));
}

SZrObjectModule *ZrCore_Reflection_GetOrCreateModuleForRuntime(
        SZrState *state,
        SZrMetadataRuntime *runtime) {
    TZrStackValuePointer rootBase;
    SZrTypeValue *cacheNameRoot;
    SZrTypeValue *makeExportNameRoot;
    SZrTypeValue *resolveExportNameRoot;
    SZrTypeValue *requireExportNameRoot;
    SZrTypeValue *createExportNameRoot;
    SZrTypeValue *serviceRoot;
    const SZrTypeValue *cachedValue;
    SZrString *cacheName;
    SZrString *makeExportName;
    SZrString *resolveExportName;
    SZrString *requireExportName;
    SZrString *createExportName;
    SZrObjectModule *runtimeModule;
    SZrObjectModule *serviceModule;
    SZrObjectModule *result = ZR_NULL;
    TZrBool runtimeModulePinned = ZR_FALSE;

    if (state == ZR_NULL || runtime == ZR_NULL || runtime->module == ZR_NULL) {
        return ZR_NULL;
    }

    runtimeModule = runtime->module;
    if (runtimeModule->super.super.type != ZR_RAW_OBJECT_TYPE_OBJECT ||
        runtimeModule->super.super.isNative ||
        runtimeModule->super.internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE ||
        ZrCore_Module_GetMetadataRuntime(runtimeModule) != runtime ||
        !ZrCore_Reflection_ObjectPinRaw(
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule),
                &runtimeModulePinned)) {
        return ZR_NULL;
    }

    rootBase = state->stackTop.valuePointer;
    rootBase = ZrCore_Function_CheckStackAndGc(state, 6u, rootBase);
    cacheName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_SERVICE_MODULE_CACHE_NAME);
    if (cacheName == ZR_NULL) {
        goto cleanup;
    }
    cacheNameRoot = ZrCore_Stack_GetValue(rootBase);
    ZrCore_Value_InitAsRawObject(
            state, cacheNameRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(cacheName));
    state->stackTop.valuePointer = rootBase + 1;

    makeExportName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_MAKE_GENERIC_METHOD_EXPORT);
    if (makeExportName == ZR_NULL) {
        goto cleanup;
    }
    makeExportNameRoot = ZrCore_Stack_GetValue(rootBase + 1);
    ZrCore_Value_InitAsRawObject(
            state, makeExportNameRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(makeExportName));
    state->stackTop.valuePointer = rootBase + 2;

    resolveExportName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_RESOLVE_TYPE_ID_EXPORT);
    if (resolveExportName == ZR_NULL) {
        goto cleanup;
    }
    resolveExportNameRoot = ZrCore_Stack_GetValue(rootBase + 2);
    ZrCore_Value_InitAsRawObject(
            state, resolveExportNameRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(resolveExportName));
    state->stackTop.valuePointer = rootBase + 3;

    requireExportName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_REQUIRE_CONSTRUCTIBLE_EXPORT);
    if (requireExportName == ZR_NULL) {
        goto cleanup;
    }
    requireExportNameRoot = ZrCore_Stack_GetValue(rootBase + 3);
    ZrCore_Value_InitAsRawObject(
            state,
            requireExportNameRoot,
            ZR_CAST_RAW_OBJECT_AS_SUPER(requireExportName));
    state->stackTop.valuePointer = rootBase + 4;

    createExportName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_CREATE_INSTANCE_EXPORT);
    if (createExportName == ZR_NULL) {
        goto cleanup;
    }
    createExportNameRoot = ZrCore_Stack_GetValue(rootBase + 4);
    ZrCore_Value_InitAsRawObject(
            state,
            createExportNameRoot,
            ZR_CAST_RAW_OBJECT_AS_SUPER(createExportName));
    state->stackTop.valuePointer = rootBase + 5;

    cachedValue = ZrCore_Module_GetProExport(state, runtimeModule, cacheName);
    if (cachedValue != ZR_NULL) {
        if (cachedValue->type != ZR_VALUE_TYPE_OBJECT || cachedValue->isNative ||
            !cachedValue->isGarbageCollectable || cachedValue->value.object == ZR_NULL ||
            cachedValue->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
            goto cleanup;
        }
        serviceModule = (SZrObjectModule *)cachedValue->value.object;
    } else {
        serviceModule = ZrCore_Reflection_CreateModuleForRuntimeInternal(
                state, runtime, ZR_FALSE);
        if (serviceModule == ZR_NULL) {
            goto cleanup;
        }
    }

    serviceRoot = ZrCore_Stack_GetValue(rootBase + 5);
    ZrCore_Value_InitAsRawObject(
            state, serviceRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(serviceModule));
    state->stackTop.valuePointer = rootBase + 6;

    cacheNameRoot = ZrCore_Stack_GetValue(rootBase);
    makeExportNameRoot = ZrCore_Stack_GetValue(rootBase + 1);
    resolveExportNameRoot = ZrCore_Stack_GetValue(rootBase + 2);
    requireExportNameRoot = ZrCore_Stack_GetValue(rootBase + 3);
    createExportNameRoot = ZrCore_Stack_GetValue(rootBase + 4);
    serviceRoot = ZrCore_Stack_GetValue(rootBase + 5);
    cacheName = ZR_CAST_STRING(state, cacheNameRoot->value.object);
    makeExportName = ZR_CAST_STRING(state, makeExportNameRoot->value.object);
    resolveExportName = ZR_CAST_STRING(state, resolveExportNameRoot->value.object);
    requireExportName = ZR_CAST_STRING(state, requireExportNameRoot->value.object);
    createExportName = ZR_CAST_STRING(state, createExportNameRoot->value.object);
    serviceModule = (SZrObjectModule *)serviceRoot->value.object;
    if (cachedValue == ZR_NULL) {
        if (!reflection_cached_module_is_valid(
                    state,
                    serviceModule,
                    makeExportName,
                    resolveExportName,
                    requireExportName,
                    createExportName,
                    runtimeModule)) {
            goto cleanup;
        }
        ZrCore_Module_AddProExport(state, runtimeModule, cacheName, serviceRoot);
        cacheNameRoot = ZrCore_Stack_GetValue(rootBase);
        makeExportNameRoot = ZrCore_Stack_GetValue(rootBase + 1);
        resolveExportNameRoot = ZrCore_Stack_GetValue(rootBase + 2);
        requireExportNameRoot = ZrCore_Stack_GetValue(rootBase + 3);
        createExportNameRoot = ZrCore_Stack_GetValue(rootBase + 4);
        serviceRoot = ZrCore_Stack_GetValue(rootBase + 5);
        cacheName = ZR_CAST_STRING(state, cacheNameRoot->value.object);
        makeExportName = ZR_CAST_STRING(state, makeExportNameRoot->value.object);
        resolveExportName = ZR_CAST_STRING(state, resolveExportNameRoot->value.object);
        requireExportName = ZR_CAST_STRING(state, requireExportNameRoot->value.object);
        createExportName = ZR_CAST_STRING(state, createExportNameRoot->value.object);
        serviceModule = (SZrObjectModule *)serviceRoot->value.object;
        cachedValue = ZrCore_Module_GetProExport(state, runtimeModule, cacheName);
        if (cachedValue == ZR_NULL || cachedValue->type != ZR_VALUE_TYPE_OBJECT ||
            cachedValue->isNative || !cachedValue->isGarbageCollectable ||
            cachedValue->value.object != serviceRoot->value.object) {
            goto cleanup;
        }
    }

    if (!reflection_cached_module_is_valid(
                state,
                serviceModule,
                makeExportName,
                resolveExportName,
                requireExportName,
                createExportName,
                runtimeModule)) {
        goto cleanup;
    }
    ZrCore_GarbageCollector_PinObject(
            state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule),
            ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
    result = serviceModule;

cleanup:
    state->stackTop.valuePointer = rootBase;
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global,
            ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule),
            runtimeModulePinned);
    return result;
}
