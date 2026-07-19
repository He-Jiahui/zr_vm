#include "zr_vm_core/reflection.h"

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

#include "reflection_generic_method_native_internal.h"
#include "reflection_module_internal.h"
#include "reflection_object_internal.h"

SZrObjectModule *ZrCore_Reflection_CreateModuleForRuntimeInternal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule) {
    TZrStackValuePointer rootBase;
    SZrTypeValue *closureRoot;
    SZrTypeValue *moduleRoot;
    SZrTypeValue *moduleNameRoot;
    SZrTypeValue *exportNameRoot;
    const SZrTypeValue *installedValue;
    SZrClosureNative *closure;
    SZrObjectModule *module;
    SZrString *moduleName;
    SZrString *exportName;
    SZrObjectModule *runtimeModule;
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
    rootBase = ZrCore_Function_CheckStackAndGc(state, 4u, rootBase);

    module = ZrCore_Module_Create(state);
    if (module == ZR_NULL) {
        goto cleanup;
    }
    moduleRoot = ZrCore_Stack_GetValue(rootBase);
    ZrCore_Value_InitAsRawObject(
            state, moduleRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    state->stackTop.valuePointer = rootBase + 1;

    moduleName = ZrCore_String_CreateFromNative(state, ZR_REFLECTION_MODULE_NAME);
    if (moduleName == ZR_NULL) {
        goto cleanup;
    }
    moduleNameRoot = ZrCore_Stack_GetValue(rootBase + 1);
    ZrCore_Value_InitAsRawObject(
            state, moduleNameRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(moduleName));
    state->stackTop.valuePointer = rootBase + 2;

    exportName = ZrCore_String_CreateFromNative(
            state, ZR_REFLECTION_MAKE_GENERIC_METHOD_EXPORT);
    if (exportName == ZR_NULL) {
        goto cleanup;
    }
    exportNameRoot = ZrCore_Stack_GetValue(rootBase + 2);
    ZrCore_Value_InitAsRawObject(
            state, exportNameRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(exportName));
    state->stackTop.valuePointer = rootBase + 3;

    closure = ZrCore_Reflection_CreateMakeGenericMethodNativeClosureInternal(
            state, runtime, ZR_FALSE);
    if (closure == ZR_NULL) {
        goto cleanup;
    }
    closureRoot = ZrCore_Stack_GetValue(rootBase + 3);
    ZrCore_Value_InitAsRawObject(
            state, closureRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = rootBase + 4;

    moduleRoot = ZrCore_Stack_GetValue(rootBase);
    moduleNameRoot = ZrCore_Stack_GetValue(rootBase + 1);
    exportNameRoot = ZrCore_Stack_GetValue(rootBase + 2);
    closureRoot = ZrCore_Stack_GetValue(rootBase + 3);
    module = (SZrObjectModule *)moduleRoot->value.object;
    moduleName = ZR_CAST_STRING(state, moduleNameRoot->value.object);
    exportName = ZR_CAST_STRING(state, exportNameRoot->value.object);
    ZrCore_Module_SetInfo(
            state,
            module,
            moduleName,
            ZrCore_Module_CalculatePathHash(state, moduleName),
            moduleName);
    ZrCore_Module_AddPubExport(state, module, exportName, closureRoot);

    moduleRoot = ZrCore_Stack_GetValue(rootBase);
    exportNameRoot = ZrCore_Stack_GetValue(rootBase + 2);
    closureRoot = ZrCore_Stack_GetValue(rootBase + 3);
    module = (SZrObjectModule *)moduleRoot->value.object;
    exportName = ZR_CAST_STRING(state, exportNameRoot->value.object);
    installedValue = ZrCore_Module_GetPubExport(state, module, exportName);
    if (installedValue == ZR_NULL || installedValue->type != ZR_VALUE_TYPE_CLOSURE ||
        installedValue->value.object != closureRoot->value.object) {
        goto cleanup;
    }

    closure = ZR_CAST_NATIVE_CLOSURE(state, closureRoot->value.object);
    installedValue = ZrCore_ClosureNative_GetCaptureValue(closure, 0u);
    if (installedValue == ZR_NULL || installedValue->type != ZR_VALUE_TYPE_OBJECT ||
        installedValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule)) {
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
