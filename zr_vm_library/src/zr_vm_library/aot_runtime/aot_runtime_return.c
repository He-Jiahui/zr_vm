#include "zr_vm_library/aot_runtime.h"

#include "aot_runtime_internal.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

TZrBool ZrLibrary_AotRuntime_ReturnI64(SZrState *state, TZrInt64 value) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    SZrTypeValue *callerResultValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT i64 return failed");
        return ZR_FALSE;
    }

    callerResultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (callerResultValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT i64 return failed");
        return ZR_FALSE;
    }

    execution_discard_exception_handlers_for_callinfo(state, callInfo);
    if (callInfo->functionTop.valuePointer != ZR_NULL &&
        (state->stackTop.valuePointer == ZR_NULL || state->stackTop.valuePointer < callInfo->functionTop.valuePointer)) {
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    }
    ZrCore_Closure_CloseClosure(state,
                                callInfo->functionBase.valuePointer + 1,
                                ZR_THREAD_STATUS_INVALID,
                                ZR_FALSE);
    ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, callInfo);
    if (callerResultValue->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
        callerResultValue->isGarbageCollectable) {
        ZrCore_Ownership_ReleaseValue(state, callerResultValue);
    }
    ZrCore_Value_InitAsInt(state, callerResultValue, value);
    state->stackTop.valuePointer = callInfo->functionBase.valuePointer + 1;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CallStackValue(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 functionSlot,
                                            TZrUInt32 argumentCount,
                                            const TZrChar *errorLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *label = errorLabel != ZR_NULL ? errorLabel : "function call";
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    TZrStackValuePointer resultBase;
    SZrFunctionStackAnchor callAnchor;
    SZrFunctionStackAnchor destinationAnchor;
    SZrTypeValue *callableValue;
    SZrTypeValue *destinationValue;
    SZrTypeValue *resultValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || frame->slotBase == ZR_NULL ||
        functionSlot >= frame->generatedFrameSlotCount || destinationSlot >= frame->generatedFrameSlotCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s failed", label);
        return ZR_FALSE;
    }

    callBase = frame->slotBase + functionSlot;
    destinationPointer = frame->slotBase + destinationSlot;
    if (state->callInfoList == ZR_NULL || state->stackTop.valuePointer == ZR_NULL ||
        state->stackTop.valuePointer < callBase + 1 + argumentCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s has invalid stack range", label);
        return ZR_FALSE;
    }

    callableValue = ZrCore_Stack_GetValue(callBase);
    if (callableValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s is missing callable value", label);
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callAnchor);
    ZrCore_Function_StackAnchorInit(state, destinationPointer, &destinationAnchor);
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (state->callInfoList->functionTop.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer < state->stackTop.valuePointer) {
        state->callInfoList->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &callAnchor, 1);
    if (resultBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s failed", label);
        return ZR_FALSE;
    }

    destinationPointer = ZrCore_Function_StackAnchorRestore(state, &destinationAnchor);
    if (destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s lost destination slot", label);
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    resultValue = ZrCore_Stack_GetValue(resultBase);
    if (destinationValue == ZR_NULL || resultValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s has invalid result slot", label);
        return ZR_FALSE;
    }

    *destinationValue = *resultValue;
    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_UnsupportedMetaCall(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 receiverSlot,
                                                 TZrUInt32 argumentCount,
                                                 const TZrChar *errorLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *label = errorLabel != ZR_NULL ? errorLabel : "unsupported AOT meta call";
    SZrTypeValue *receiverValue;
    SZrTypeValue *destinationValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL ||
        destinationSlot >= frame->generatedFrameSlotCount || receiverSlot >= frame->generatedFrameSlotCount) {
        aot_runtime_fail(state, runtimeState, "%s", label);
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(frame->slotBase + receiverSlot);
    destinationValue = ZrCore_Stack_GetValue(frame->slotBase + destinationSlot);
    if (receiverValue == ZR_NULL || destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s", label);
        return ZR_FALSE;
    }

    (void)argumentCount;
    (void)receiverValue;
    (void)destinationValue;
    aot_runtime_fail(state, runtimeState, "%s", label);
    return ZR_FALSE;
}

TZrBool ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 primarySlot,
                                                        TZrUInt32 secondarySlot,
                                                        TZrUInt32 memberOrCacheIndex,
                                                        const TZrChar *opcodeName) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *safeOpcodeName = opcodeName != ZR_NULL ? opcodeName : "META_VALUE_ACCESS";
    SZrTypeValue *primaryValue;
    SZrTypeValue *secondaryValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL ||
        primarySlot >= frame->generatedFrameSlotCount || secondarySlot >= frame->generatedFrameSlotCount) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT meta value access: %s", safeOpcodeName);
        return ZR_FALSE;
    }

    primaryValue = ZrCore_Stack_GetValue(frame->slotBase + primarySlot);
    secondaryValue = ZrCore_Stack_GetValue(frame->slotBase + secondarySlot);
    if (primaryValue == ZR_NULL || secondaryValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT meta value access: %s", safeOpcodeName);
        return ZR_FALSE;
    }

    (void)memberOrCacheIndex;
    (void)primaryValue;
    (void)secondaryValue;
    aot_runtime_fail(state, runtimeState, "unsupported AOT meta value access: %s", safeOpcodeName);
    return ZR_FALSE;
}

TZrBool ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess(SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 primarySlot,
                                                           TZrUInt32 secondarySlot,
                                                           TZrUInt32 operandIndex,
                                                           const TZrChar *opcodeName) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *safeOpcodeName = opcodeName != ZR_NULL ? opcodeName : "DYNAMIC_VALUE_ACCESS";
    SZrTypeValue *primaryValue;
    SZrTypeValue *secondaryValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL ||
        primarySlot >= frame->generatedFrameSlotCount || secondarySlot >= frame->generatedFrameSlotCount) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT dynamic value access: %s", safeOpcodeName);
        return ZR_FALSE;
    }

    primaryValue = ZrCore_Stack_GetValue(frame->slotBase + primarySlot);
    secondaryValue = ZrCore_Stack_GetValue(frame->slotBase + secondarySlot);
    if (primaryValue == ZR_NULL || secondaryValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT dynamic value access: %s", safeOpcodeName);
        return ZR_FALSE;
    }

    (void)operandIndex;
    (void)primaryValue;
    (void)secondaryValue;
    aot_runtime_fail(state, runtimeState, "unsupported AOT dynamic value access: %s", safeOpcodeName);
    return ZR_FALSE;
}

TZrBool ZrLibrary_AotRuntime_CallStaticDirect(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 functionSlot,
                                              TZrUInt32 argumentCount,
                                              TZrUInt32 calleeFunctionIndex,
                                              FZrAotEntryThunk calleeThunk) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    SZrFunctionStackAnchor callerFrameTopAnchor;
    SZrCallInfo *callInfo;
    SZrFunction *metadataFunction;
    SZrTypeValue *callableValue;
    TZrUInt32 callWindowIndex;
    TZrUInt32 argumentIndex;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || frame->slotBase == ZR_NULL ||
        frame->callInfo == ZR_NULL || frame->callInfo != state->callInfoList ||
        destinationSlot >= frame->generatedFrameSlotCount || functionSlot >= frame->generatedFrameSlotCount ||
        argumentCount > frame->generatedFrameSlotCount - functionSlot - 1u ||
        calleeFunctionIndex == ZR_AOT_RUNTIME_RESUME_FALLTHROUGH || calleeThunk == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }

    callBase = frame->callInfo->functionTop.valuePointer;
    if (callBase != ZR_NULL && callBase < frame->slotBase + frame->generatedFrameSlotCount) {
        callBase = frame->slotBase + frame->generatedFrameSlotCount;
    }
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid stack range");
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callerFrameTopAnchor);
    callBase = ZrCore_Function_CheckStackAndGc(state, 1u + argumentCount, callBase);
    if (callBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid stack range");
        return ZR_FALSE;
    }
    callBase = ZrCore_Function_StackAnchorRestore(state, &callerFrameTopAnchor);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid stack range");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    for (callWindowIndex = 0u; callWindowIndex < 1u + argumentCount; callWindowIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase + callWindowIndex));
    }
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (state->callInfoList->functionTop.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer < state->stackTop.valuePointer) {
        state->callInfoList->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    callableValue = ZrCore_Stack_GetValue(frame->slotBase + functionSlot);
    destinationPointer = frame->slotBase + destinationSlot;
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);
    if (callableValue == ZR_NULL || metadataFunction == ZR_NULL ||
        frame->callInfo->functionTop.valuePointer < frame->slotBase + functionSlot + 1u + argumentCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(callBase), callableValue);
    for (argumentIndex = 0u; argumentIndex < argumentCount; argumentIndex++) {
        const TZrUInt32 sourceSlot = functionSlot + 1u + argumentIndex;
        const SZrFunctionFrameSlotLayout *argumentLayout =
                ZrCore_Function_FindFrameSlotLayout(frame->function, sourceSlot);
        SZrTypeValue *argumentValue = ZrCore_Stack_GetValue(frame->slotBase + sourceSlot);

        if (argumentValue == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid argument source");
            return ZR_FALSE;
        }

        if (argumentLayout != ZR_NULL &&
            argumentLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
            argumentLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue)) {
            SZrStackFramePlace argumentPlace;
            SZrTypeValue *argumentFrame;

            if (!ZrCore_Function_MakeFrameSlotPlace(state,
                                                    frame->function,
                                                    frame->slotBase,
                                                    sourceSlot,
                                                    &argumentPlace)) {
                aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid argument source");
                return ZR_FALSE;
            }

            argumentFrame = (SZrTypeValue *)argumentPlace.address;
            if (argumentFrame == ZR_NULL) {
                aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid argument source");
                return ZR_FALSE;
            }
            if (argumentFrame != argumentValue) {
                ZrCore_Value_Copy(state, argumentFrame, argumentValue);
            }
            argumentValue = argumentFrame;
        }

        ZrCore_Value_Copy(state,
                          ZrCore_Stack_GetValue(callBase + 1u + argumentIndex),
                          argumentValue);
    }

    callInfo = ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource(state,
                                                                                   callBase,
                                                                                   metadataFunction,
                                                                                   argumentCount,
                                                                                   1,
                                                                                   destinationPointer,
                                                                                   frame->slotBase,
                                                                                   functionSlot + 1u);
    if (callInfo == ZR_NULL || state->callInfoList != callInfo) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }

    if (!calleeThunk(state)) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }
    ZrCore_Function_PostCall(state, callInfo, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE || state->callInfoList == ZR_NULL ||
        state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    callBase = ZrCore_Function_StackAnchorRestore(state, &callerFrameTopAnchor);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }

    for (callWindowIndex = 0u; callWindowIndex < 1u + argumentCount; callWindowIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase + callWindowIndex));
    }
    frame->callInfo->functionTop.valuePointer = callBase;
    state->stackTop.valuePointer = callBase;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CallInlineStruct(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 functionSlot,
                                              TZrUInt32 argumentCount,
                                              TZrUInt32 calleeFunctionIndex,
                                              TZrUInt32 destinationTypeLayoutId,
                                              TZrUInt32 destinationByteOffset,
                                              TZrUInt32 destinationByteSize,
                                              FZrAotEntryThunk calleeThunk) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrTypeLayout *returnLayout;
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    SZrFunctionStackAnchor callerFrameTopAnchor;
    SZrCallInfo *callInfo;
    SZrFunction *metadataFunction;
    SZrTypeValue *callableValue;
    TZrUInt32 callWindowIndex;
    TZrUInt32 argumentIndex;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || frame->slotBase == ZR_NULL ||
        frame->callInfo == ZR_NULL || frame->callInfo != state->callInfoList ||
        destinationSlot >= frame->generatedFrameSlotCount || functionSlot >= frame->generatedFrameSlotCount ||
        argumentCount > frame->generatedFrameSlotCount - functionSlot - 1u ||
        calleeFunctionIndex == ZR_AOT_RUNTIME_RESUME_FALLTHROUGH || calleeThunk == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    returnLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame->function,
                                                                   destinationTypeLayoutId,
                                                                   state);
    if (returnLayout == ZR_NULL || returnLayout->kind != ZR_TYPE_LAYOUT_KIND_STRUCT ||
        returnLayout->byteSize != destinationByteSize) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    callBase = frame->callInfo->functionTop.valuePointer;
    if (callBase != ZR_NULL && callBase < frame->slotBase + frame->generatedFrameSlotCount) {
        callBase = frame->slotBase + frame->generatedFrameSlotCount;
    }
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callerFrameTopAnchor);
    callBase = ZrCore_Function_CheckStackAndGc(state, 1u + argumentCount, callBase);
    if (callBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }
    callBase = ZrCore_Function_StackAnchorRestore(state, &callerFrameTopAnchor);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    for (callWindowIndex = 0u; callWindowIndex < 1u + argumentCount; callWindowIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase + callWindowIndex));
    }
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (state->callInfoList->functionTop.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer < state->stackTop.valuePointer) {
        state->callInfoList->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    callableValue = ZrCore_Stack_GetValue(frame->slotBase + functionSlot);
    destinationPointer = (TZrStackValuePointer)((TZrByte *)frame->slotBase + destinationByteOffset);
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);
    if (callableValue == ZR_NULL || metadataFunction == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(callBase), callableValue);
    for (argumentIndex = 0u; argumentIndex < argumentCount; argumentIndex++) {
        const TZrUInt32 sourceSlot = functionSlot + 1u + argumentIndex;
        const SZrFunctionFrameSlotLayout *argumentLayout =
                ZrCore_Function_FindFrameSlotLayout(frame->function, sourceSlot);
        SZrTypeValue *argumentDense = ZrCore_Stack_GetValue(frame->slotBase + sourceSlot);
        SZrTypeValue *argumentFrame = ZR_NULL;

        if (argumentLayout == ZR_NULL ||
            argumentLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            argumentLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue) ||
            argumentDense == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
            return ZR_FALSE;
        }

        {
            SZrStackFramePlace argumentPlace;

            if (!ZrCore_Function_MakeFrameSlotPlace(state,
                                                    frame->function,
                                                    frame->slotBase,
                                                    sourceSlot,
                                                    &argumentPlace)) {
                aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
                return ZR_FALSE;
            }
            argumentFrame = (SZrTypeValue *)argumentPlace.address;
        }
        if (argumentFrame == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
            return ZR_FALSE;
        }
        if (argumentFrame != argumentDense) {
            ZrCore_Value_Copy(state, argumentFrame, argumentDense);
        }
        ZrCore_Value_Copy(state,
                          ZrCore_Stack_GetValue(callBase + 1u + argumentIndex),
                          argumentFrame);
    }

    callInfo = ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource(state,
                                                                                   callBase,
                                                                                   metadataFunction,
                                                                                   argumentCount,
                                                                                   1,
                                                                                   destinationPointer,
                                                                                   frame->slotBase,
                                                                                   functionSlot + 1u);
    if (callInfo == ZR_NULL || state->callInfoList != callInfo) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    if (!calleeThunk(state)) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }
    ZrCore_Function_PostCall(state, callInfo, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE || state->callInfoList == ZR_NULL ||
        state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    callBase = ZrCore_Function_StackAnchorRestore(state, &callerFrameTopAnchor);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    for (callWindowIndex = 0u; callWindowIndex < 1u + argumentCount; callWindowIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase + callWindowIndex));
    }
    frame->callInfo->functionTop.valuePointer = callBase;
    state->stackTop.valuePointer = callBase;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ReturnInlineStruct(SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 sourceSlot,
                                                TZrUInt32 sourceTypeLayoutId,
                                                TZrUInt32 sourceByteOffset,
                                                TZrUInt32 sourceByteSize,
                                                TZrUInt32 *outSkipDropSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrTypeLayout *returnLayout;
    SZrCallInfo *callInfo;
    TZrStackValuePointer returnSource;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || frame->slotBase == ZR_NULL ||
        outSkipDropSlot == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct return failed");
        return ZR_FALSE;
    }

    returnLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame->function, sourceTypeLayoutId, state);
    callInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    returnSource = (TZrStackValuePointer)((TZrByte *)frame->slotBase + sourceByteOffset);
    if (returnLayout == ZR_NULL || returnLayout->kind != ZR_TYPE_LAYOUT_KIND_STRUCT ||
        returnLayout->byteSize != sourceByteSize || callInfo == ZR_NULL ||
        callInfo->functionBase.valuePointer == ZR_NULL || returnSource == ZR_NULL ||
        (callInfo->functionTop.valuePointer != ZR_NULL && returnSource >= callInfo->functionTop.valuePointer)) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct return failed");
        return ZR_FALSE;
    }

    state->stackTop.valuePointer = returnSource + 1;
    *outSkipDropSlot = sourceSlot;
    return ZR_TRUE;
}
