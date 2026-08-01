#include "debug_evaluation_context_internal.h"

#include <string.h>

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

static SZrClosure *debug_evaluation_context_get_vm_closure(
        SZrState *state,
        SZrCallInfo *callInfo,
        SZrFunction *function) {
    SZrTypeValue *callableValue;
    SZrClosure *closure;

    if (state == ZR_NULL || callInfo == ZR_NULL || function == ZR_NULL ||
        callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_NULL;
    }

    callableValue = ZrCore_Stack_GetValueNoProfile(callInfo->functionBase.valuePointer);
    if (callableValue == ZR_NULL || callableValue->type != ZR_VALUE_TYPE_CLOSURE ||
        callableValue->isNative || callableValue->value.object == ZR_NULL ||
        ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue) != function) {
        return ZR_NULL;
    }

    closure = ZR_CAST_VM_CLOSURE(state, callableValue->value.object);
    if (closure == ZR_NULL || closure->closureValueCount != function->closureValueLength) {
        return ZR_NULL;
    }
    return closure;
}

static TZrBool debug_evaluation_context_closure_capture_matches(
        const SZrDebugClosureCaptureBinding *expected,
        const SZrDebugClosureCaptureBinding *actual) {
    return (TZrBool)(expected != ZR_NULL && actual != ZR_NULL &&
                     expected->captureIndex == actual->captureIndex &&
                     expected->type == actual->type && expected->symbolId == actual->symbolId &&
                     expected->typeId == actual->typeId &&
                     expected->declarationStartLine == actual->declarationStartLine &&
                     expected->declarationStartColumn == actual->declarationStartColumn &&
                     expected->declarationEndLine == actual->declarationEndLine &&
                     expected->declarationEndColumn == actual->declarationEndColumn &&
                     expected->token == actual->token);
}

EZrDebugEvaluationContextStatus ZrCore_Debug_EvaluationContext_GetClosureCapture(
        SZrState *state,
        const SZrDebugEvaluationContext *context,
        TZrUInt32 captureIndex,
        SZrDebugClosureCaptureBinding *outBinding) {
    SZrCallInfo *callInfo;
    SZrFunction *function;
    SZrClosure *closure;
    const SZrFunctionTypedTypeRef *type = ZR_NULL;
    TZrUInt32 symbolId = 0u;
    TZrUInt32 typeId = 0u;
    SZrFunctionSourceRange declarationRange;
    EZrDebugEvaluationContextStatus status;

    if (outBinding != ZR_NULL) {
        memset(outBinding, 0, sizeof(*outBinding));
    }
    if (outBinding == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT;
    }

    status = debug_evaluation_context_validate(state, context, &callInfo, &function);
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return status;
    }

    closure = debug_evaluation_context_get_vm_closure(state, callInfo, function);
    if (closure == ZR_NULL || captureIndex >= closure->closureValueCount ||
        closure->closureValuesExtend[captureIndex] == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
    }

    memset(&declarationRange, 0, sizeof(declarationRange));
    if (!ZrCore_Function_GetClosureCaptureIdentity(function,
                                                   captureIndex,
                                                   &type,
                                                   &symbolId,
                                                   &typeId,
                                                   &declarationRange)) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
    }

    outBinding->captureIndex = captureIndex;
    outBinding->type = type;
    outBinding->symbolId = symbolId;
    outBinding->typeId = typeId;
    outBinding->declarationStartLine = declarationRange.startLine;
    outBinding->declarationStartColumn = declarationRange.startColumn;
    outBinding->declarationEndLine = declarationRange.endLine;
    outBinding->declarationEndColumn = declarationRange.endColumn;
    outBinding->token = context->frameGeneration;
    return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK;
}

EZrDebugEvaluationContextStatus ZrCore_Debug_EvaluationContext_ResolveClosureCapture(
        SZrState *state,
        const SZrDebugEvaluationContext *context,
        const SZrDebugClosureCaptureBinding *binding,
        SZrTypeValue *outValue) {
    SZrCallInfo *callInfo;
    SZrFunction *function;
    SZrClosure *closure;
    SZrDebugClosureCaptureBinding actualBinding;
    SZrTypeValue *captureValue;
    EZrDebugEvaluationContextStatus status;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (binding == ZR_NULL || outValue == ZR_NULL || binding->type == ZR_NULL || binding->symbolId == 0u ||
        binding->typeId == 0u || binding->token == 0u) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT;
    }

    status = debug_evaluation_context_validate(state, context, &callInfo, &function);
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return status;
    }
    if (binding->token != context->frameGeneration) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME;
    }

    memset(&actualBinding, 0, sizeof(actualBinding));
    status = ZrCore_Debug_EvaluationContext_GetClosureCapture(
            state, context, binding->captureIndex, &actualBinding);
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return status;
    }
    if (!debug_evaluation_context_closure_capture_matches(binding, &actualBinding)) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME;
    }

    closure = debug_evaluation_context_get_vm_closure(state, callInfo, function);
    if (closure == ZR_NULL || binding->captureIndex >= closure->closureValueCount ||
        closure->closureValuesExtend[binding->captureIndex] == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
    }

    captureValue = ZrCore_ClosureValue_GetValue(closure->closureValuesExtend[binding->captureIndex]);
    if (captureValue == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
    }

    debug_evaluation_context_snapshot_value(state, outValue, captureValue);
    return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK;
}
