#include "reflection_bound_runtime_native_internal.h"
/* 四个反射导出共用此边界：closure 捕获 module，由 module 反查并核验 MetadataRuntime。 */
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"

#include "reflection_object_internal.h"
/* 模块创建和缓存复用还核对预期 module；native 入口至少核对函数身份与捕获形状。 */
TZrBool ZrCore_Reflection_BoundRuntimeNativeClosureIsValidInternal(
        SZrState *state,
        SZrClosureNative *closure,
        FZrNativeFunction expectedFunction,
        SZrObjectModule *expectedRuntimeModule) {
    const SZrTypeValue *runtimeValue;
    SZrRawObject *captureOwner;
    SZrClosureValue *captureClosureValue;
    SZrObjectModule *runtimeModule;
    SZrMetadataRuntime *runtime;

    if (state == ZR_NULL || closure == ZR_NULL || expectedFunction == ZR_NULL ||
        closure->super.type != ZR_RAW_OBJECT_TYPE_CLOSURE || !closure->super.isNative ||
        closure->nativeFunction != expectedFunction || closure->closureValueCount != 1u ||
        closure->closureValuesExtend[0] != ZR_NULL) {
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

    runtimeValue = ZrCore_ClosureValue_GetValue(captureClosureValue);
    if (runtimeValue == ZR_NULL || runtimeValue->type != ZR_VALUE_TYPE_OBJECT ||
        runtimeValue->isNative || !runtimeValue->isGarbageCollectable ||
        runtimeValue->value.object == ZR_NULL ||
        runtimeValue->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT ||
        runtimeValue->value.object->isNative ||
        ((SZrObject *)runtimeValue->value.object)->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_FALSE;
    }

    runtimeModule = (SZrObjectModule *)runtimeValue->value.object;
    runtime = ZrCore_Module_GetMetadataRuntime(runtimeModule);
    return (TZrBool)(
            runtime != ZR_NULL && runtime->module == runtimeModule &&
            (expectedRuntimeModule == ZR_NULL || runtimeModule == expectedRuntimeModule));
}
/* 从当前函数槽取得经身份校验的绑定；调用方在读取参数前先确认入口与捕获匹配。 */
SZrMetadataRuntime *ZrCore_Reflection_GetBoundRuntimeFromCallInternal(
        SZrState *state,
        FZrNativeFunction expectedFunction) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *callableValue;
    SZrClosureNative *closure;
    const SZrTypeValue *runtimeValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return ZR_NULL;
    }
    functionBase = state->callInfoList->functionBase.valuePointer;
    if (functionBase == ZR_NULL) {
        return ZR_NULL;
    }
    callableValue = ZrCore_Stack_GetValue(functionBase);
    if (callableValue == ZR_NULL || callableValue->type != ZR_VALUE_TYPE_CLOSURE ||
        !callableValue->isNative || callableValue->value.object == ZR_NULL ||
        callableValue->value.object->type != ZR_RAW_OBJECT_TYPE_CLOSURE ||
        !callableValue->value.object->isNative) {
        return ZR_NULL;
    }

    closure = ZR_CAST_NATIVE_CLOSURE(state, callableValue->value.object);
    if (!ZrCore_Reflection_BoundRuntimeNativeClosureIsValidInternal(
                state, closure, expectedFunction, ZR_NULL)) {
        return ZR_NULL;
    }
    runtimeValue = ZrCore_ClosureNative_GetCaptureValue(closure, 0u);
    return ZrCore_Module_GetMetadataRuntime((SZrObjectModule *)runtimeValue->value.object);
}
/* 工厂负责临时 root 与捕获闭合；调用方须在后续分配前接管返回 closure 的可达性。 */
SZrClosureNative *ZrCore_Reflection_CreateBoundRuntimeNativeClosureInternal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        FZrNativeFunction nativeFunction,
        TZrBool pinRuntimeModule) {
    TZrStackValuePointer rootBase;
    SZrTypeValue *closureRoot;
    SZrTypeValue *moduleRoot;
    SZrClosureNative *closure;
    SZrClosureValue *captureOwner;
    SZrRawObject **captureOwners;
    SZrObjectModule *runtimeModule;
    TZrBool modulePinned = ZR_FALSE;

    if (state == ZR_NULL || runtime == ZR_NULL || runtime->module == ZR_NULL ||
        nativeFunction == ZR_NULL) {
        return ZR_NULL;
    }

    runtimeModule = runtime->module;
    if (runtimeModule->super.super.type != ZR_RAW_OBJECT_TYPE_OBJECT ||
        runtimeModule->super.super.isNative ||
        runtimeModule->super.internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE ||
        ZrCore_Module_GetMetadataRuntime(runtimeModule) != runtime ||
        !ZrCore_Reflection_ObjectPinRaw(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule), &modulePinned)) {
        return ZR_NULL;
    }
    /* BUG: modulePinned 为真且栈扩容 OOM 时，CheckStackAndGc 抛出会跳过末尾撤根，module 滞留全局 ignored 表。 */
    rootBase = state->stackTop.valuePointer;
    rootBase = ZrCore_Function_CheckStackAndGc(state, 2u, rootBase);
    closure = ZrCore_ClosureNative_New(state, 1u);
    if (closure == ZR_NULL) {
        ZrCore_Reflection_ObjectUnpinRaw(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule), modulePinned);
        return ZR_NULL;
    }
    /* 两个栈槽同时保护新 closure 和被捕获 module，直到捕获对象闭合。 */
    closure->nativeFunction = nativeFunction;
    closureRoot = ZrCore_Stack_GetValue(rootBase);
    ZrCore_Value_InitAsRawObject(state, closureRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    moduleRoot = ZrCore_Stack_GetValue(rootBase + 1);
    ZrCore_Value_InitAsRawObject(
            state, moduleRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule));
    state->stackTop.valuePointer = rootBase + 2;
    /* 捕获通过 closure-value owner 持有 module，屏障把这条新 GC 边加入标记协议。 */
    captureOwner = ZrCore_Closure_FindOrCreateValue(state, rootBase + 1);
    if (captureOwner == ZR_NULL) {
        ZrCore_Reflection_ObjectUnpinRaw(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule), modulePinned);
        state->stackTop.valuePointer = rootBase;
        return ZR_NULL;
    }
    /* 下游分配可能触发 GC，最终返回值从受追踪槽重新取得。 */
    closure = ZR_CAST_NATIVE_CLOSURE(state, closureRoot->value.object);
    captureOwners = ZrCore_ClosureNative_GetCaptureOwners(closure);
    closure->closureValuesExtend[0] = ZR_NULL;
    captureOwners[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(captureOwner);
    ZrCore_RawObject_Barrier(
            state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
            ZR_CAST_RAW_OBJECT_AS_SUPER(captureOwner));
    ZrCore_Closure_CloseStackValue(state, rootBase + 1);
    if (!ZrCore_GarbageCollector_IsObjectIgnoredFast(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule)) &&
        !ZrCore_GarbageCollector_IgnoreObject(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule))) {
        ZrCore_Reflection_ObjectUnpinRaw(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule), modulePinned);
        state->stackTop.valuePointer = rootBase;
        return ZR_NULL;
    }
    /* 公开工厂才额外请求 native-handle pin；模块内部导出只靠正常可达关系。 */
    closure = ZR_CAST_NATIVE_CLOSURE(state, closureRoot->value.object);
    if (pinRuntimeModule) {
        ZrCore_GarbageCollector_PinObject(
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule),
                ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
    }
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule), modulePinned);
    state->stackTop.valuePointer = rootBase;
    return closure;
}
