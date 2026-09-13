#include "zr_vm_core/native_call_contract.h"

#include <stdint.h>
#include <string.h>

static void native_call_marshal_set_diagnostic(
        SZrNativeCallDiagnostic *diagnostic,
        EZrNativeCallStatus status,
        EZrNativeCallPhase phase,
        TZrUInt32 parameterIndex,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    TZrUInt64 sourceId;

    if (diagnostic == ZR_NULL) {
        return;
    }
    sourceId = diagnostic->sourceId;
    diagnostic->status = status;
    diagnostic->phase = phase;
    diagnostic->parameterIndex = parameterIndex;
    diagnostic->operationIndex = parameterIndex;
    diagnostic->sourceId = sourceId;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

static TZrBool native_call_marshal_pointer_aligned(
        const void *pointer,
        TZrUInt32 alignment) {
    if (pointer == ZR_NULL || alignment == 0u) {
        return ZR_TRUE;
    }
    return (TZrBool)(((uintptr_t)pointer % (uintptr_t)alignment) == 0u);
}

static const SZrNativeCallMarshalOp *native_call_marshal_get_op(
        const SZrNativeCallPlan *plan,
        TZrUInt32 parameterIndex,
        SZrNativeCallDiagnostic *diagnostic) {
    if (plan == ZR_NULL || parameterIndex >= plan->marshalOpCount) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex, 0u, 0u);
        return ZR_NULL;
    }
    return &plan->marshalOps[parameterIndex];
}

TZrBool ZrCore_NativeCall_MarshalArgument(
        const SZrNativeCallPlan *plan,
        TZrUInt32 parameterIndex,
        const SZrNativeCallMarshalRequest *request,
        SZrNativeCallMarshalResult *result,
        SZrNativeCallDiagnostic *diagnostic) {
    const SZrNativeCallMarshalOp *op;
    TZrBool sourceRequired;
    TZrBool sourcePresent;
    TZrBool sourceInitialized;
    TZrBool nullable;
    void *pointer;

    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (result != ZR_NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (diagnostic != ZR_NULL && plan != ZR_NULL) {
        diagnostic->sourceId = plan->sourceId;
    }
    if (request == ZR_NULL || result == ZR_NULL ||
        !ZrCore_NativeCall_ValidatePlan(plan, diagnostic)) {
        if (request == ZR_NULL || result == ZR_NULL) {
            native_call_marshal_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex, 0u, 0u);
        }
        return ZR_FALSE;
    }
    if (request->writeBack > ZR_TRUE ||
        request->sourceInitialized > ZR_TRUE || request->reserved0 != 0u) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex, 1u,
                ((TZrUInt64)request->writeBack << 8u) |
                        request->sourceInitialized);
        return ZR_FALSE;
    }
    op = native_call_marshal_get_op(plan, parameterIndex, diagnostic);
    if (op == ZR_NULL) {
        return ZR_FALSE;
    }
    nullable = (TZrBool)((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_NULLABLE) != 0u);
    sourcePresent = (TZrBool)(request->source != ZR_NULL);
    sourceInitialized = (TZrBool)(request->sourceInitialized && sourcePresent);
    sourceRequired = (TZrBool)(op->direction == ZR_FFI_CONTRACT_DIRECTION_IN ||
                               op->direction == ZR_FFI_CONTRACT_DIRECTION_REF);
    if (sourceRequired && !sourcePresent && !nullable) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex, op->byteSize, 0u);
        return ZR_FALSE;
    }
    if (op->direction != ZR_FFI_CONTRACT_DIRECTION_OUT &&
        sourcePresent && request->sourceInitialized &&
        request->sourceSize < op->byteSize) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_BUFFER_TOO_SMALL,
                ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex,
                op->byteSize, request->sourceSize);
        return ZR_FALSE;
    }
    if (sourceRequired && sourcePresent && !sourceInitialized) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex, 1u, 0u);
        return ZR_FALSE;
    }
    if (op->marshalKind == ZR_NATIVE_CALL_MARSHAL_DIRECT ||
        op->marshalKind == ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT) {
        if (op->direction == ZR_FFI_CONTRACT_DIRECTION_IN && !sourcePresent &&
            !nullable) {
            native_call_marshal_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex, 0u, 0u);
            return ZR_FALSE;
        }
        if (op->direction == ZR_FFI_CONTRACT_DIRECTION_IN) {
            pointer = (void *)request->source;
        } else if (request->destination != ZR_NULL) {
            pointer = request->destination;
        } else if (op->direction == ZR_FFI_CONTRACT_DIRECTION_REF &&
                   sourcePresent) {
            pointer = (void *)request->source;
        } else {
            native_call_marshal_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex,
                    op->byteSize, 0u);
            return ZR_FALSE;
        }
        if (!native_call_marshal_pointer_aligned(pointer, op->byteAlignment)) {
            native_call_marshal_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_ALIGNMENT,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex,
                    op->byteAlignment, (TZrUInt64)(uintptr_t)pointer);
            return ZR_FALSE;
        }
        if (op->direction != ZR_FFI_CONTRACT_DIRECTION_IN &&
            request->destination != ZR_NULL &&
            request->destinationCapacity < op->byteSize) {
            native_call_marshal_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_BUFFER_TOO_SMALL,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex,
                    op->byteSize, request->destinationCapacity);
            return ZR_FALSE;
        }
        if (pointer == ZR_NULL && !nullable) {
            native_call_marshal_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex, 1u, 0u);
            return ZR_FALSE;
        }
        result->pointer = pointer;
        result->byteSize = op->byteSize;
        result->usedTemporary = ZR_FALSE;
        result->requiresWriteback = ZR_FALSE;
        return ZR_TRUE;
    }
    /* A registered lane already names storage owned by the provider/registry.
     * It is not a request for another temporary copy: the registry has
     * established the address/lifetime separately (the lease still roots the
     * managed owner where required).  Keep the same source validation as the
     * direct lane, but do not require caller-owned destination bytes. */
    if (op->marshalKind == ZR_NATIVE_CALL_MARSHAL_REGISTERED) {
        if (!sourcePresent) {
            if (!nullable) {
                native_call_marshal_set_diagnostic(
                        diagnostic, ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT,
                        ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex,
                        op->byteSize, 0u);
                return ZR_FALSE;
            }
            result->pointer = ZR_NULL;
            result->byteSize = op->byteSize;
            result->usedTemporary = ZR_FALSE;
            result->requiresWriteback = ZR_FALSE;
            return ZR_TRUE;
        }
        if (!sourceInitialized) {
            native_call_marshal_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex, 1u, 0u);
            return ZR_FALSE;
        }
        if (request->sourceSize < op->byteSize) {
            native_call_marshal_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_BUFFER_TOO_SMALL,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex,
                    op->byteSize, request->sourceSize);
            return ZR_FALSE;
        }
        if (!native_call_marshal_pointer_aligned(
                    request->source, op->byteAlignment)) {
            native_call_marshal_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_ALIGNMENT,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex,
                    op->byteAlignment,
                    (TZrUInt64)(uintptr_t)request->source);
            return ZR_FALSE;
        }
        result->pointer = (void *)request->source;
        result->byteSize = op->byteSize;
        result->usedTemporary = ZR_FALSE;
        result->requiresWriteback = ZR_FALSE;
        return ZR_TRUE;
    }
    if (request->destination == ZR_NULL) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex,
                op->byteSize, 0u);
        return ZR_FALSE;
    }
    if (request->destinationCapacity < op->byteSize) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_BUFFER_TOO_SMALL,
                ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex,
                op->byteSize, request->destinationCapacity);
        return ZR_FALSE;
    }
    if (!native_call_marshal_pointer_aligned(
                request->destination, op->byteAlignment)) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ALIGNMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex,
                op->byteAlignment, (TZrUInt64)(uintptr_t)request->destination);
        return ZR_FALSE;
    }
    if (sourceInitialized &&
        (op->direction == ZR_FFI_CONTRACT_DIRECTION_IN ||
         op->direction == ZR_FFI_CONTRACT_DIRECTION_REF)) {
        memmove(request->destination, request->source, op->byteSize);
    } else if (sourceRequired && !nullable) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, parameterIndex, 1u, 0u);
        return ZR_FALSE;
    } else {
        memset(request->destination, 0, op->byteSize);
    }
    result->pointer = request->destination;
    result->byteSize = op->byteSize;
    result->usedTemporary = ZR_TRUE;
    result->requiresWriteback = (TZrBool)(
            (op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK) != 0u);
    return ZR_TRUE;
}

TZrBool ZrCore_NativeCall_WriteBackArgument(
        const SZrNativeCallPlan *plan,
        TZrUInt32 parameterIndex,
        const void *temporary,
        TZrSize temporarySize,
        void *destination,
        TZrSize destinationCapacity,
        SZrNativeCallDiagnostic *diagnostic) {
    const SZrNativeCallMarshalOp *op;

    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (diagnostic != ZR_NULL && plan != ZR_NULL) {
        diagnostic->sourceId = plan->sourceId;
    }
    if (!ZrCore_NativeCall_ValidatePlan(plan, diagnostic)) {
        return ZR_FALSE;
    }
    op = native_call_marshal_get_op(plan, parameterIndex, diagnostic);
    if (op == ZR_NULL) {
        return ZR_FALSE;
    }
    if (op->direction == ZR_FFI_CONTRACT_DIRECTION_IN) {
        return ZR_TRUE;
    }
    if (temporary == ZR_NULL || destination == ZR_NULL ||
        temporarySize < op->byteSize || destinationCapacity < op->byteSize) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_WRITEBACK_REQUIRED,
                ZR_NATIVE_CALL_PHASE_WRITEBACK, parameterIndex,
                op->byteSize, temporarySize);
        return ZR_FALSE;
    }
    if (!native_call_marshal_pointer_aligned(temporary, op->byteAlignment) ||
        !native_call_marshal_pointer_aligned(destination, op->byteAlignment)) {
        native_call_marshal_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ALIGNMENT,
                ZR_NATIVE_CALL_PHASE_WRITEBACK, parameterIndex,
                op->byteAlignment, (TZrUInt64)(uintptr_t)destination);
        return ZR_FALSE;
    }
    if (temporary != destination) {
        memmove(destination, temporary, op->byteSize);
    }
    return ZR_TRUE;
}
