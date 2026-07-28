#include "debug_evaluation_context_internal.h"

#include <string.h>

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/state.h"

static TZrUInt32 debug_evaluation_context_current_instruction_offset(
        SZrCallInfo *callInfo,
        SZrFunction *function) {
    if (callInfo == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        callInfo->context.context.programCounter == ZR_NULL ||
        callInfo->context.context.programCounter < function->instructionsList) {
        return 0u;
    }

    return (TZrUInt32)(callInfo->context.context.programCounter - function->instructionsList);
}

EZrDebugEvaluationContextStatus debug_evaluation_context_validate(
        SZrState *state,
        const SZrDebugEvaluationContext *context,
        SZrCallInfo **outCallInfo,
        SZrFunction **outFunction) {
    SZrCallInfo *activeCallInfo;
    SZrCallInfo *callInfo;
    SZrFunction *function;

    if (outCallInfo != ZR_NULL) {
        *outCallInfo = ZR_NULL;
    }
    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || context->activation.callInfo == ZR_NULL ||
        context->activation.function == ZR_NULL || context->frameGeneration == 0u) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT;
    }

    callInfo = context->activation.callInfo;
    for (activeCallInfo = state->callInfoList;
         activeCallInfo != ZR_NULL && activeCallInfo != callInfo;
         activeCallInfo = activeCallInfo->previous) {
    }
    if (activeCallInfo == ZR_NULL || !ZR_CALL_INFO_IS_VM(callInfo) ||
        callInfo->debugFrameGeneration != context->frameGeneration) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME;
    }

    function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
    if (function == ZR_NULL || function != context->activation.function ||
        debug_evaluation_context_current_instruction_offset(callInfo, function) !=
                context->instructionOffset) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME;
    }

    if (outCallInfo != ZR_NULL) {
        *outCallInfo = callInfo;
    }
    if (outFunction != ZR_NULL) {
        *outFunction = function;
    }
    return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK;
}

EZrDebugEvaluationContextStatus ZrCore_Debug_EvaluationContext_GetGenericArgument(
        SZrState *state,
        const SZrDebugEvaluationContext *context,
        SZrMetadataRuntime *runtime,
        EZrDebugGenericContextKind contextKind,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        SZrDebugGenericArgument *outArgument) {
    SZrCallInfo *callInfo;
    struct SZrObject *typeObject = ZR_NULL;
    EZrDebugEvaluationContextStatus status;

    if (outArgument != ZR_NULL) {
        memset(outArgument, 0, sizeof(*outArgument));
    }
    if (outArgument == ZR_NULL || runtime == ZR_NULL || ownerToken == 0u ||
        (contextKind != ZR_DEBUG_GENERIC_CONTEXT_TYPE &&
         contextKind != ZR_DEBUG_GENERIC_CONTEXT_METHOD)) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT;
    }

    status = debug_evaluation_context_validate(state, context, &callInfo, ZR_NULL);
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return status;
    }

    if (contextKind == ZR_DEBUG_GENERIC_CONTEXT_TYPE) {
        if (!context->hasGenericContext) {
            return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
        }
        typeObject = ZrCore_Reflection_ResolveInterpreterGenericCallInfoParameterTypeObject(
                state, runtime, callInfo, ownerToken, parameterIndex);
    } else {
        if (!context->hasGenericMethodContext) {
            return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
        }
        typeObject = ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject(
                state, runtime, callInfo, ownerToken, parameterIndex);
    }
    if (typeObject == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
    }

    outArgument->contextKind = contextKind;
    outArgument->ownerToken = ownerToken;
    outArgument->parameterIndex = parameterIndex;
    outArgument->typeObject = typeObject;
    return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK;
}
