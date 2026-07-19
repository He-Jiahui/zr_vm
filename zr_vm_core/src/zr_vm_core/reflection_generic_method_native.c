#include "zr_vm_core/reflection.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include "reflection_object_internal.h"

static TZrInt64 reflection_make_generic_method_native_return_null(
        SZrState *state,
        TZrStackValuePointer functionBase) {
    if (state == ZR_NULL || functionBase == ZR_NULL) {
        return 0;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

TZrInt64 ZrCore_Reflection_MakeGenericMethodNativeEntry(SZrState *state) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *callableValue;
    SZrTypeValue *definitionValue;
    SZrTypeValue *argumentsValue;
    SZrTypeValue *runtimeValue;
    SZrRawObject *captureOwner;
    SZrClosureNative *closure;
    SZrObjectModule *runtimeModule;
    SZrMetadataRuntime *runtime;
    SZrObject *constructedMethod;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    if (functionBase == ZR_NULL || state->stackTop.valuePointer != functionBase + 3) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    callableValue = ZrCore_Stack_GetValue(functionBase);
    if (callableValue == ZR_NULL || callableValue->type != ZR_VALUE_TYPE_CLOSURE || !callableValue->isNative ||
        callableValue->value.object == ZR_NULL || callableValue->value.object->type != ZR_RAW_OBJECT_TYPE_CLOSURE ||
        !callableValue->value.object->isNative) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    closure = ZR_CAST_NATIVE_CLOSURE(state, callableValue->value.object);
    captureOwner = ZrCore_ClosureNative_GetCaptureOwner(closure, 0u);
    runtimeValue = ZrCore_ClosureNative_GetCaptureValue(closure, 0u);
    if (closure->nativeFunction != ZrCore_Reflection_MakeGenericMethodNativeEntry ||
        closure->closureValueCount != 1u || captureOwner == ZR_NULL ||
        captureOwner->type != ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE || captureOwner->isNative ||
        runtimeValue == ZR_NULL || runtimeValue->type != ZR_VALUE_TYPE_OBJECT ||
        runtimeValue->isNative || !runtimeValue->isGarbageCollectable ||
        runtimeValue->value.object == ZR_NULL ||
        runtimeValue->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT ||
        runtimeValue->value.object->isNative ||
        ((SZrObject *)runtimeValue->value.object)->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    definitionValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentsValue = ZrCore_Stack_GetValue(functionBase + 2);
    if (definitionValue == ZR_NULL || definitionValue->type != ZR_VALUE_TYPE_OBJECT ||
        definitionValue->value.object == ZR_NULL ||
        definitionValue->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT ||
        argumentsValue == ZR_NULL || argumentsValue->type != ZR_VALUE_TYPE_ARRAY ||
        argumentsValue->value.object == ZR_NULL ||
        argumentsValue->value.object->type != ZR_RAW_OBJECT_TYPE_ARRAY) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    runtimeModule = (SZrObjectModule *)runtimeValue->value.object;
    runtime = ZrCore_Module_GetMetadataRuntime(runtimeModule);
    if (runtime == ZR_NULL || runtime->module != runtimeModule) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }
    constructedMethod = ZrCore_Reflection_MakeGenericMethodFromObjects(
            state,
            runtime,
            (SZrObject *)definitionValue->value.object,
            (SZrObject *)argumentsValue->value.object);
    if (constructedMethod == ZR_NULL) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    ZrCore_Value_InitAsRawObject(
            state,
            ZrCore_Stack_GetValue(functionBase),
            ZR_CAST_RAW_OBJECT_AS_SUPER(constructedMethod));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

SZrClosureNative *ZrCore_Reflection_CreateMakeGenericMethodNativeClosure(
        SZrState *state,
        SZrMetadataRuntime *runtime) {
    TZrStackValuePointer rootBase;
    SZrTypeValue *closureRoot;
    SZrTypeValue *moduleRoot;
    SZrClosureNative *closure;
    SZrClosureValue *captureOwner;
    SZrRawObject **captureOwners;
    SZrObjectModule *runtimeModule;
    TZrBool modulePinned = ZR_FALSE;

    if (state == ZR_NULL || runtime == ZR_NULL || runtime->module == ZR_NULL) {
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

    rootBase = state->stackTop.valuePointer;
    rootBase = ZrCore_Function_CheckStackAndGc(state, 2u, rootBase);
    closure = ZrCore_ClosureNative_New(state, 1u);
    if (closure == ZR_NULL) {
        ZrCore_Reflection_ObjectUnpinRaw(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule), modulePinned);
        return ZR_NULL;
    }

    closure->nativeFunction = ZrCore_Reflection_MakeGenericMethodNativeEntry;
    closureRoot = ZrCore_Stack_GetValue(rootBase);
    ZrCore_Value_InitAsRawObject(state, closureRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    moduleRoot = ZrCore_Stack_GetValue(rootBase + 1);
    ZrCore_Value_InitAsRawObject(
            state, moduleRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule));
    state->stackTop.valuePointer = rootBase + 2;

    captureOwner = ZrCore_Closure_FindOrCreateValue(state, rootBase + 1);
    if (captureOwner == ZR_NULL) {
        ZrCore_Reflection_ObjectUnpinRaw(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule), modulePinned);
        state->stackTop.valuePointer = rootBase;
        return ZR_NULL;
    }

    closure = ZR_CAST_NATIVE_CLOSURE(state, closureRoot->value.object);
    captureOwners = ZrCore_ClosureNative_GetCaptureOwners(closure);
    // The owner resolves the closed module value after GC moves; the direct slot must not retain a stack address.
    closure->closureValuesExtend[0] = ZR_NULL;
    captureOwners[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(captureOwner);
    ZrCore_RawObject_Barrier(
            state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
            ZR_CAST_RAW_OBJECT_AS_SUPER(captureOwner));
    ZrCore_Closure_CloseStackValue(state, rootBase + 1);

    closure = ZR_CAST_NATIVE_CLOSURE(state, closureRoot->value.object);
    ZrCore_GarbageCollector_PinObject(
            state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule),
            ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule), modulePinned);
    state->stackTop.valuePointer = rootBase;
    return closure;
}
