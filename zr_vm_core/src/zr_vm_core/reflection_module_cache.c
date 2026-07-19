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
#include "reflection_object_internal.h"

static TZrBool reflection_cached_module_is_valid(
        SZrState *state,
        SZrObjectModule *serviceModule,
        SZrString *exportName,
        SZrObjectModule *runtimeModule) {
    const SZrTypeValue *exportValue;
    const SZrTypeValue *captureValue;
    SZrRawObject *captureOwner;
    SZrClosureValue *captureClosureValue;
    SZrClosureNative *closure;
    const TZrChar *moduleName;
    const TZrChar *fullPath;

    if (state == ZR_NULL || serviceModule == ZR_NULL || exportName == ZR_NULL || runtimeModule == ZR_NULL ||
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
        serviceModule->super.nodeMap.elementCount != 1u) {
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

    exportValue = ZrCore_Module_GetPubExport(state, serviceModule, exportName);
    if (exportValue == ZR_NULL || exportValue->type != ZR_VALUE_TYPE_CLOSURE ||
        !exportValue->isNative || !exportValue->isGarbageCollectable ||
        exportValue->value.object == ZR_NULL ||
        exportValue->value.object->type != ZR_RAW_OBJECT_TYPE_CLOSURE ||
        !exportValue->value.object->isNative) {
        return ZR_FALSE;
    }

    closure = ZR_CAST_NATIVE_CLOSURE(state, exportValue->value.object);
    if (closure->nativeFunction != ZrCore_Reflection_MakeGenericMethodNativeEntry ||
        closure->closureValueCount != 1u || closure->closureValuesExtend[0] != ZR_NULL) {
        return ZR_FALSE;
    }
    captureOwner = ZrCore_ClosureNative_GetCaptureOwner(closure, 0u);
    if (captureOwner == ZR_NULL || captureOwner->type != ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE ||
        captureOwner->isNative) {
        return ZR_FALSE;
    }
    captureClosureValue = (SZrClosureValue *)captureOwner;
    if (!ZrCore_ClosureValue_IsClosed(captureClosureValue)) {
        return ZR_FALSE;
    }

    captureValue = ZrCore_ClosureValue_GetValue(captureClosureValue);
    return (TZrBool)(
            captureValue != ZR_NULL && captureValue->type == ZR_VALUE_TYPE_OBJECT &&
            !captureValue->isNative && captureValue->isGarbageCollectable &&
            captureValue->value.object != ZR_NULL &&
            captureValue->value.object->type == ZR_RAW_OBJECT_TYPE_OBJECT &&
            !captureValue->value.object->isNative &&
            ((SZrObject *)captureValue->value.object)->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE &&
            captureValue->value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule));
}

SZrObjectModule *ZrCore_Reflection_GetOrCreateModuleForRuntime(
        SZrState *state,
        SZrMetadataRuntime *runtime) {
    TZrStackValuePointer rootBase;
    SZrTypeValue *cacheNameRoot;
    SZrTypeValue *exportNameRoot;
    SZrTypeValue *serviceRoot;
    const SZrTypeValue *cachedValue;
    SZrString *cacheName;
    SZrString *exportName;
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
    rootBase = ZrCore_Function_CheckStackAndGc(state, 3u, rootBase);
    cacheName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_SERVICE_MODULE_CACHE_NAME);
    if (cacheName == ZR_NULL) {
        goto cleanup;
    }
    cacheNameRoot = ZrCore_Stack_GetValue(rootBase);
    ZrCore_Value_InitAsRawObject(
            state, cacheNameRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(cacheName));
    state->stackTop.valuePointer = rootBase + 1;

    exportName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_MAKE_GENERIC_METHOD_EXPORT);
    if (exportName == ZR_NULL) {
        goto cleanup;
    }
    exportNameRoot = ZrCore_Stack_GetValue(rootBase + 1);
    ZrCore_Value_InitAsRawObject(
            state, exportNameRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(exportName));
    state->stackTop.valuePointer = rootBase + 2;

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

    serviceRoot = ZrCore_Stack_GetValue(rootBase + 2);
    ZrCore_Value_InitAsRawObject(
            state, serviceRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(serviceModule));
    state->stackTop.valuePointer = rootBase + 3;

    cacheNameRoot = ZrCore_Stack_GetValue(rootBase);
    exportNameRoot = ZrCore_Stack_GetValue(rootBase + 1);
    serviceRoot = ZrCore_Stack_GetValue(rootBase + 2);
    cacheName = ZR_CAST_STRING(state, cacheNameRoot->value.object);
    exportName = ZR_CAST_STRING(state, exportNameRoot->value.object);
    serviceModule = (SZrObjectModule *)serviceRoot->value.object;
    if (cachedValue == ZR_NULL) {
        if (!reflection_cached_module_is_valid(
                    state, serviceModule, exportName, runtimeModule)) {
            goto cleanup;
        }
        ZrCore_Module_AddProExport(state, runtimeModule, cacheName, serviceRoot);
        cacheNameRoot = ZrCore_Stack_GetValue(rootBase);
        exportNameRoot = ZrCore_Stack_GetValue(rootBase + 1);
        serviceRoot = ZrCore_Stack_GetValue(rootBase + 2);
        cacheName = ZR_CAST_STRING(state, cacheNameRoot->value.object);
        exportName = ZR_CAST_STRING(state, exportNameRoot->value.object);
        serviceModule = (SZrObjectModule *)serviceRoot->value.object;
        cachedValue = ZrCore_Module_GetProExport(state, runtimeModule, cacheName);
        if (cachedValue == ZR_NULL || cachedValue->type != ZR_VALUE_TYPE_OBJECT ||
            cachedValue->isNative || !cachedValue->isGarbageCollectable ||
            cachedValue->value.object != serviceRoot->value.object) {
            goto cleanup;
        }
    }

    if (!reflection_cached_module_is_valid(
                state, serviceModule, exportName, runtimeModule)) {
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
