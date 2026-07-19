#include "module/module_reflection_import.h"

#include <string.h>

#include "module/module_internal.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/reflection.h"

#define ZR_MODULE_REFLECTION_OWNER_DEPTH_LIMIT ((TZrUInt32)1024u)

static ZR_FORCE_INLINE SZrFunction *module_reflection_import_refresh_function(
        SZrFunction *function) {
    SZrRawObject *forwarded;

    if (function == ZR_NULL) {
        return ZR_NULL;
    }
    forwarded = (SZrRawObject *)function->super.garbageCollectMark.forwardingAddress;
    return forwarded != ZR_NULL ? (SZrFunction *)forwarded : function;
}

static SZrFunction *module_reflection_import_owner_root(SZrFunction *function) {
    TZrUInt32 depth;

    for (depth = 0u; depth < ZR_MODULE_REFLECTION_OWNER_DEPTH_LIMIT; ++depth) {
        SZrFunction *owner;

        function = module_reflection_import_refresh_function(function);
        if (function == ZR_NULL) {
            return ZR_NULL;
        }
        owner = module_reflection_import_refresh_function(function->ownerFunction);
        if (owner == ZR_NULL) {
            return function;
        }
        function = owner;
    }
    return ZR_NULL;
}

static TZrBool module_reflection_import_path_matches(SZrString *path) {
    static const TZrChar reflectionModuleName[] = ZR_REFLECTION_MODULE_NAME;
    TZrNativeString pathText;

    if (path == ZR_NULL || path->super.type != ZR_RAW_OBJECT_TYPE_STRING) {
        return ZR_FALSE;
    }
    pathText = ZrCore_String_GetNativeString(path);
    return pathText != ZR_NULL &&
                           ZrCore_String_GetByteLength(path) ==
                                   sizeof(reflectionModuleName) - 1u &&
                           memcmp(pathText,
                                  reflectionModuleName,
                                  sizeof(reflectionModuleName) - 1u) == 0
                   ? ZR_TRUE
                   : ZR_FALSE;
}

static TZrBool module_reflection_import_pair_has_module(
        SZrHashKeyValuePair *pair,
        SZrObjectModule **outModule) {
    SZrRawObject *keyObject;
    SZrRawObject *valueObject;
    SZrObject *object;

    if (outModule != ZR_NULL) {
        *outModule = ZR_NULL;
    }
    if (pair == ZR_NULL || outModule == ZR_NULL ||
        pair->key.type != ZR_VALUE_TYPE_STRING ||
        !pair->key.isNative || !pair->key.isGarbageCollectable ||
        pair->key.value.object == ZR_NULL ||
        pair->value.type != ZR_VALUE_TYPE_OBJECT || pair->value.isNative ||
        !pair->value.isGarbageCollectable || pair->value.value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    keyObject = pair->key.value.object;
    valueObject = pair->value.value.object;
    if (keyObject->type != ZR_RAW_OBJECT_TYPE_STRING || !keyObject->isNative ||
        valueObject->type != ZR_RAW_OBJECT_TYPE_OBJECT || valueObject->isNative) {
        return ZR_FALSE;
    }
    object = (SZrObject *)valueObject;
    if (object->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_FALSE;
    }
    *outModule = (SZrObjectModule *)object;
    return ZR_TRUE;
}

static SZrMetadataRuntime *module_reflection_import_find_runtime(
        SZrState *state,
        SZrFunction *callerFunction) {
    SZrObject *registry;
    SZrFunction *callerRoot;
    SZrObjectModule *matchedModule = ZR_NULL;
    SZrMetadataRuntime *matchedRuntime = ZR_NULL;
    TZrSize bucketIndex;
    TZrSize visitedPairCount = 0u;

    callerRoot = module_reflection_import_owner_root(callerFunction);
    registry = zr_module_get_loaded_modules_registry(state);
    if (callerRoot == ZR_NULL || registry == ZR_NULL ||
        !registry->nodeMap.isValid || registry->nodeMap.buckets == ZR_NULL ||
        registry->nodeMap.capacity == 0u) {
        return ZR_NULL;
    }

    for (bucketIndex = 0u; bucketIndex < registry->nodeMap.capacity; ++bucketIndex) {
        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[bucketIndex];

        while (pair != ZR_NULL) {
            SZrObjectModule *module;

            if (visitedPairCount >= registry->nodeMap.elementCount) {
                return ZR_NULL;
            }
            ++visitedPairCount;
            if (!module_reflection_import_pair_has_module(pair, &module)) {
                return ZR_NULL;
            }
            if (module->initState == ZR_MODULE_INIT_STATE_READY) {
                SZrMetadataRuntime *runtime = ZrCore_Module_GetMetadataRuntime(module);
                SZrFunction *runtimeRoot = runtime != ZR_NULL
                                                       ? module_reflection_import_owner_root(
                                                                 runtime->metadataFunction)
                                                       : ZR_NULL;

                if (runtime != ZR_NULL && runtime->module == module &&
                    runtime->codeRegistration != ZR_NULL && runtimeRoot == callerRoot) {
                    if (matchedModule != ZR_NULL && matchedModule != module) {
                        return ZR_NULL;
                    }
                    matchedModule = module;
                    matchedRuntime = runtime;
                }
            }
            pair = pair->next;
        }
    }
    return visitedPairCount == registry->nodeMap.elementCount
                   ? matchedRuntime
                   : ZR_NULL;
}

TZrBool zr_module_reflection_import_try_resolve(
        SZrState *state,
        SZrString *path,
        SZrFunction *callerFunction,
        SZrObjectModule **outModule) {
    SZrMetadataRuntime *runtime;

    if (outModule != ZR_NULL) {
        *outModule = ZR_NULL;
    }
    if (!module_reflection_import_path_matches(path)) {
        return ZR_FALSE;
    }
    if (outModule == ZR_NULL) {
        return ZR_TRUE;
    }
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrCore_GlobalState_ClearModuleLoadDiagnostic(state->global);
    if (callerFunction == ZR_NULL) {
        return ZR_TRUE;
    }
    runtime = module_reflection_import_find_runtime(state, callerFunction);
    if (runtime != ZR_NULL) {
        *outModule = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, runtime);
    }
    return ZR_TRUE;
}

#undef ZR_MODULE_REFLECTION_OWNER_DEPTH_LIMIT
