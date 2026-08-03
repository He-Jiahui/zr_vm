#include "zr_vm_core/reflection.h"

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

#include "reflection_generic_method_native_internal.h"
#include "reflection_bound_runtime_native_internal.h"
#include "reflection_construction_native_internal.h"
#include "reflection_module_internal.h"
#include "reflection_object_internal.h"
#include "reflection_type_resolve_native_internal.h"

static TZrBool reflection_module_export_is_installed(
        SZrState *state,
        SZrObjectModule *module,
        SZrString *exportName,
        SZrClosureNative *closure,
        FZrNativeFunction expectedFunction,
        SZrObjectModule *runtimeModule) {
    const SZrTypeValue *installedValue;

    installedValue = ZrCore_Module_GetPubExport(state, module, exportName);
    return (TZrBool)(
            installedValue != ZR_NULL && installedValue->type == ZR_VALUE_TYPE_CLOSURE &&
            installedValue->value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(closure) &&
            ZrCore_Reflection_BoundRuntimeNativeClosureIsValidInternal(
                    state, closure, expectedFunction, runtimeModule));
}

SZrObjectModule *ZrCore_Reflection_CreateModuleForRuntimeInternal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule) {
    TZrStackValuePointer rootBase;
    SZrTypeValue *makeClosureRoot;
    SZrTypeValue *resolveClosureRoot;
    SZrTypeValue *requireClosureRoot;
    SZrTypeValue *createClosureRoot;
    SZrTypeValue *moduleRoot;
    SZrTypeValue *moduleNameRoot;
    SZrTypeValue *makeExportNameRoot;
    SZrTypeValue *resolveExportNameRoot;
    SZrTypeValue *requireExportNameRoot;
    SZrTypeValue *createExportNameRoot;
    SZrClosureNative *makeClosure;
    SZrClosureNative *resolveClosure;
    SZrClosureNative *requireClosure;
    SZrClosureNative *createClosure;
    SZrObjectModule *module;
    SZrString *moduleName;
    SZrString *makeExportName;
    SZrString *resolveExportName;
    SZrString *requireExportName;
    SZrString *createExportName;
    SZrObjectModule *runtimeModule;
    const TZrChar *providerModuleName;
    SZrObjectModule *result = ZR_NULL;
    TZrBool runtimeModulePinned = ZR_FALSE;

    if (state == ZR_NULL || state->global == ZR_NULL || runtime == ZR_NULL ||
        runtime->module == ZR_NULL) {
        return ZR_NULL;
    }

    providerModuleName = ZrCore_GlobalState_ResolveProviderModuleName(
            state->global, ZR_PROVIDER_CONTRACT_ROLE_REFLECTION);
    if (providerModuleName == ZR_NULL || providerModuleName[0] == '\0') {
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
    rootBase = ZrCore_Function_CheckStackAndGc(state, 10u, rootBase);

    module = ZrCore_Module_Create(state);
    if (module == ZR_NULL) {
        goto cleanup;
    }
    moduleRoot = ZrCore_Stack_GetValue(rootBase);
    ZrCore_Value_InitAsRawObject(
            state, moduleRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    state->stackTop.valuePointer = rootBase + 1;

    moduleName = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)providerModuleName);
    if (moduleName == ZR_NULL) {
        goto cleanup;
    }
    moduleNameRoot = ZrCore_Stack_GetValue(rootBase + 1);
    ZrCore_Value_InitAsRawObject(
            state, moduleNameRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(moduleName));
    state->stackTop.valuePointer = rootBase + 2;

    makeExportName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_MAKE_GENERIC_METHOD_EXPORT);
    if (makeExportName == ZR_NULL) {
        goto cleanup;
    }
    makeExportNameRoot = ZrCore_Stack_GetValue(rootBase + 2);
    ZrCore_Value_InitAsRawObject(
            state, makeExportNameRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(makeExportName));
    state->stackTop.valuePointer = rootBase + 3;

    makeClosure = ZrCore_Reflection_CreateMakeGenericMethodNativeClosureInternal(
            state, runtime, ZR_FALSE);
    if (makeClosure == ZR_NULL) {
        goto cleanup;
    }
    makeClosureRoot = ZrCore_Stack_GetValue(rootBase + 3);
    ZrCore_Value_InitAsRawObject(
            state, makeClosureRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(makeClosure));
    state->stackTop.valuePointer = rootBase + 4;

    resolveExportName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_RESOLVE_TYPE_ID_EXPORT);
    if (resolveExportName == ZR_NULL) {
        goto cleanup;
    }
    resolveExportNameRoot = ZrCore_Stack_GetValue(rootBase + 4);
    ZrCore_Value_InitAsRawObject(
            state, resolveExportNameRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(resolveExportName));
    state->stackTop.valuePointer = rootBase + 5;

    resolveClosure = ZrCore_Reflection_CreateResolveTypeIdNativeClosureInternal(
            state, runtime, ZR_FALSE);
    if (resolveClosure == ZR_NULL) {
        goto cleanup;
    }
    resolveClosureRoot = ZrCore_Stack_GetValue(rootBase + 5);
    ZrCore_Value_InitAsRawObject(
            state, resolveClosureRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(resolveClosure));
    state->stackTop.valuePointer = rootBase + 6;

    requireExportName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_REQUIRE_CONSTRUCTIBLE_EXPORT);
    if (requireExportName == ZR_NULL) {
        goto cleanup;
    }
    requireExportNameRoot = ZrCore_Stack_GetValue(rootBase + 6);
    ZrCore_Value_InitAsRawObject(
            state,
            requireExportNameRoot,
            ZR_CAST_RAW_OBJECT_AS_SUPER(requireExportName));
    state->stackTop.valuePointer = rootBase + 7;

    requireClosure =
            ZrCore_Reflection_CreateRequireConstructibleNativeClosureInternal(
                    state, runtime, ZR_FALSE);
    if (requireClosure == ZR_NULL) {
        goto cleanup;
    }
    requireClosureRoot = ZrCore_Stack_GetValue(rootBase + 7);
    ZrCore_Value_InitAsRawObject(
            state,
            requireClosureRoot,
            ZR_CAST_RAW_OBJECT_AS_SUPER(requireClosure));
    state->stackTop.valuePointer = rootBase + 8;

    createExportName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_CREATE_INSTANCE_EXPORT);
    if (createExportName == ZR_NULL) {
        goto cleanup;
    }
    createExportNameRoot = ZrCore_Stack_GetValue(rootBase + 8);
    ZrCore_Value_InitAsRawObject(
            state,
            createExportNameRoot,
            ZR_CAST_RAW_OBJECT_AS_SUPER(createExportName));
    state->stackTop.valuePointer = rootBase + 9;

    createClosure = ZrCore_Reflection_CreateInstanceNativeClosureInternal(
            state, runtime, ZR_FALSE);
    if (createClosure == ZR_NULL) {
        goto cleanup;
    }
    createClosureRoot = ZrCore_Stack_GetValue(rootBase + 9);
    ZrCore_Value_InitAsRawObject(
            state,
            createClosureRoot,
            ZR_CAST_RAW_OBJECT_AS_SUPER(createClosure));
    state->stackTop.valuePointer = rootBase + 10;

    moduleRoot = ZrCore_Stack_GetValue(rootBase);
    moduleNameRoot = ZrCore_Stack_GetValue(rootBase + 1);
    makeExportNameRoot = ZrCore_Stack_GetValue(rootBase + 2);
    makeClosureRoot = ZrCore_Stack_GetValue(rootBase + 3);
    resolveExportNameRoot = ZrCore_Stack_GetValue(rootBase + 4);
    resolveClosureRoot = ZrCore_Stack_GetValue(rootBase + 5);
    requireExportNameRoot = ZrCore_Stack_GetValue(rootBase + 6);
    requireClosureRoot = ZrCore_Stack_GetValue(rootBase + 7);
    createExportNameRoot = ZrCore_Stack_GetValue(rootBase + 8);
    createClosureRoot = ZrCore_Stack_GetValue(rootBase + 9);
    module = (SZrObjectModule *)moduleRoot->value.object;
    moduleName = ZR_CAST_STRING(state, moduleNameRoot->value.object);
    makeExportName = ZR_CAST_STRING(state, makeExportNameRoot->value.object);
    resolveExportName = ZR_CAST_STRING(state, resolveExportNameRoot->value.object);
    requireExportName = ZR_CAST_STRING(state, requireExportNameRoot->value.object);
    createExportName = ZR_CAST_STRING(state, createExportNameRoot->value.object);
    ZrCore_Module_SetInfo(
            state,
            module,
            moduleName,
            ZrCore_Module_CalculatePathHash(state, moduleName),
            moduleName);
    ZrCore_Module_AddPubExport(state, module, makeExportName, makeClosureRoot);
    ZrCore_Module_AddPubExport(state, module, resolveExportName, resolveClosureRoot);
    ZrCore_Module_AddPubExport(state, module, requireExportName, requireClosureRoot);
    ZrCore_Module_AddPubExport(state, module, createExportName, createClosureRoot);

    moduleRoot = ZrCore_Stack_GetValue(rootBase);
    makeExportNameRoot = ZrCore_Stack_GetValue(rootBase + 2);
    makeClosureRoot = ZrCore_Stack_GetValue(rootBase + 3);
    resolveExportNameRoot = ZrCore_Stack_GetValue(rootBase + 4);
    resolveClosureRoot = ZrCore_Stack_GetValue(rootBase + 5);
    requireExportNameRoot = ZrCore_Stack_GetValue(rootBase + 6);
    requireClosureRoot = ZrCore_Stack_GetValue(rootBase + 7);
    createExportNameRoot = ZrCore_Stack_GetValue(rootBase + 8);
    createClosureRoot = ZrCore_Stack_GetValue(rootBase + 9);
    module = (SZrObjectModule *)moduleRoot->value.object;
    makeExportName = ZR_CAST_STRING(state, makeExportNameRoot->value.object);
    makeClosure = ZR_CAST_NATIVE_CLOSURE(state, makeClosureRoot->value.object);
    resolveExportName = ZR_CAST_STRING(state, resolveExportNameRoot->value.object);
    resolveClosure = ZR_CAST_NATIVE_CLOSURE(state, resolveClosureRoot->value.object);
    requireExportName = ZR_CAST_STRING(state, requireExportNameRoot->value.object);
    requireClosure = ZR_CAST_NATIVE_CLOSURE(state, requireClosureRoot->value.object);
    createExportName = ZR_CAST_STRING(state, createExportNameRoot->value.object);
    createClosure = ZR_CAST_NATIVE_CLOSURE(state, createClosureRoot->value.object);
    if (!reflection_module_export_is_installed(
                state,
                module,
                makeExportName,
                makeClosure,
                ZrCore_Reflection_MakeGenericMethodNativeEntry,
                runtimeModule) ||
        !reflection_module_export_is_installed(
                state,
                module,
                resolveExportName,
                resolveClosure,
                ZrCore_Reflection_ResolveTypeIdNativeEntryInternal,
                runtimeModule) ||
        !reflection_module_export_is_installed(
                state,
                module,
                requireExportName,
                requireClosure,
                ZrCore_Reflection_RequireConstructibleNativeEntryInternal,
                runtimeModule) ||
        !reflection_module_export_is_installed(
                state,
                module,
                createExportName,
                createClosure,
                ZrCore_Reflection_CreateInstanceNativeEntryInternal,
                runtimeModule)) {
        goto cleanup;
    }
    ZrCore_Module_SetInitializationState(module, ZR_MODULE_INIT_STATE_READY);
    result = module;

cleanup:
    state->stackTop.valuePointer = rootBase;
    if (!ZrCore_GarbageCollector_IsObjectIgnoredFast(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule)) &&
        !ZrCore_GarbageCollector_IgnoreObject(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule))) {
        result = ZR_NULL;
    }
    if (result != ZR_NULL && pinRuntimeModule) {
        ZrCore_GarbageCollector_PinObject(
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule),
                ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
    }
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global,
            ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule),
            runtimeModulePinned);
    return result;
}

SZrObjectModule *ZrCore_Reflection_CreateModuleForRuntime(
        SZrState *state,
        SZrMetadataRuntime *runtime) {
    return ZrCore_Reflection_CreateModuleForRuntimeInternal(
            state, runtime, ZR_TRUE);
}
