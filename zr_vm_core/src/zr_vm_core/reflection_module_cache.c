#include "zr_vm_core/reflection.h"

#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
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
/** @brief 按导出名核对 native closure 的类型、回调身份及所属 runtime module。 */
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
/* 缓存服务必须是当前 provider 的完整就绪 module，四个公开导出缺一不可。 */
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
    const TZrChar *providerModuleName;

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

    providerModuleName = ZrCore_GlobalState_ResolveProviderModuleName(
            state->global, ZR_PROVIDER_CONTRACT_ROLE_REFLECTION);
    moduleName = ZrCore_String_GetNativeString(serviceModule->moduleName);
    fullPath = ZrCore_String_GetNativeString(serviceModule->fullPath);
    if (providerModuleName == ZR_NULL || moduleName == ZR_NULL ||
        fullPath == ZR_NULL || strcmp(moduleName, providerModuleName) != 0 ||
        strcmp(fullPath, providerModuleName) != 0 ||
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
/**
 * @brief 按所属 MetadataRuntime 获取或创建并验证反射服务 module。
 * @pre state、runtime 及 runtime->module 有效，module 与 runtime 互相归属，且 module 属于 state 的 GC domain。
 * @return 命中或创建并发布的有效服务 module；正常失败返回 null，损坏缓存不覆盖；OOM 可非局部抛出。
 * @note 服务存入目标 module 的 protected 表；正常退出时恢复六个临时 VM root，成功后目标 module 获得 NATIVE_HANDLE pin。
 * BUG: 多个 RUNNING mutator 并发查询或创建同一 runtime 时，proNodeMap 与 ignored-root 登记无同步，可能破坏缓存或丢失 GC 根。
 * BUG: 公开入口未核 runtime module 的真实 GC 域；传入外域 runtime 可将临时根登记到错误 collector，并跨域写入缓存和 pin 元数据。
 */
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
    /* state、runtime 或所属 module 不完整时不建立缓存。 */
    if (state == ZR_NULL || runtime == ZR_NULL || runtime->module == ZR_NULL) {
        return ZR_NULL;
    }
    /* module 必须与 runtime 反向归属；先临时保活目标再分配对象。 */
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
    /* 六个槽根住 cache key、四个导出名和 service；正常退出统一恢复栈顶。 */
    rootBase = state->stackTop.valuePointer;
    /* BUG: 本次新增 ignored root 后若扩栈 OOM 非局部抛出，会绕过 cleanup 的唯一 unpin，令临时根滞留。 */
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
    /* 后续导出名各自入栈 root，避免分配时丢失尚未安装的名称。 */
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
    /* cache key 仅在目标 module 的 protected 表；坏命中失败关闭且不覆盖。 */
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
    /* BUG: miss builder 可再次扩栈并搬移底层数组，旧 rootBase 在此处写第六槽时已可能失效。 */
    serviceRoot = ZrCore_Stack_GetValue(rootBase + 5);
    ZrCore_Value_InitAsRawObject(
            state, serviceRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(serviceModule));
    state->stackTop.valuePointer = rootBase + 6;
    /* 命中或新建的 service 本应由第六槽保护，再用于完整性校验和缓存发布。 */
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
        /* TODO: 强制 GcFull 落在 AddProExport 内时，核实 HashSet_Add 的表和根槽地址稳定。 */
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
    /* 命中与 miss 发布共用最终完整性校验。 */
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
    /* BUG: pinned-region 描述符扩容 OOM 无失败返回，零 region id 会漏计区段快照；本函数仍返回服务。 */
    ZrCore_GarbageCollector_PinObject(
            state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule),
            ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
    result = serviceModule;
    /* 正常返回与 goto cleanup 恢复调用者 stackTop，并撤销本次新增的临时 ignored root。 */
cleanup:
    state->stackTop.valuePointer = rootBase;
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global,
            ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule),
            runtimeModulePinned);
    return result;
}
