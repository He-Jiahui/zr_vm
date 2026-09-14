#include "zr_vm_core/exec_ir_numeric.h"

#include <stdint.h>
#include <string.h>

#define ZR_NUMERIC_LEGALIZE_HASH_OFFSET UINT64_C(1469598103934665603)
#define ZR_NUMERIC_LEGALIZE_HASH_PRIME UINT64_C(1099511628211)
#define ZR_NUMERIC_MAX_VECTOR_LANES ((TZrUInt32)4096u)

static void vector_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 byte;
    if (hash == ZR_NULL) {
        return;
    }
    for (byte = 0u; byte < 4u; ++byte) {
        *hash ^= (TZrUInt64)((value >> (byte * 8u)) & 0xffu);
        *hash *= ZR_NUMERIC_LEGALIZE_HASH_PRIME;
    }
}

static void vector_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    vector_hash_u32(hash, (TZrUInt32)value);
    vector_hash_u32(hash, (TZrUInt32)(value >> 32u));
}

static void vector_diagnostic_clear(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        (void)memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static void vector_diagnostic_set(SZrExecIrDiagnostic *diagnostic,
                                  EZrExecutionDiagnosticCode code,
                                  TZrUInt64 expected,
                                  TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    vector_diagnostic_clear(diagnostic);
    diagnostic->code = code;
    diagnostic->expectedVersion = (TZrUInt32)expected;
    diagnostic->actualVersion = (TZrUInt32)actual;
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
}

static void vector_diagnostic_apply_location(
        SZrExecIrDiagnostic *diagnostic,
        const SZrVectorLegalizationRequest *request) {
    if (diagnostic == ZR_NULL || request == ZR_NULL) {
        return;
    }
    diagnostic->functionToken = request->functionToken;
    diagnostic->blockId = request->blockId;
    diagnostic->instructionId = request->instructionId;
    diagnostic->sourceId = request->sourceId;
}

static TZrBool vector_valid_bool(TZrBool value) {
    return (TZrBool)(value == ZR_FALSE || value == ZR_TRUE);
}

static TZrBool vector_is_float(EZrNumericElementKind kind) {
    return (TZrBool)(kind == ZR_EXEC_IR_NUMERIC_ELEMENT_F32 ||
                     kind == ZR_EXEC_IR_NUMERIC_ELEMENT_F64);
}

static TZrBool vector_is_integer(EZrNumericElementKind kind) {
    return (TZrBool)(kind >= ZR_EXEC_IR_NUMERIC_ELEMENT_I8 &&
                     kind <= ZR_EXEC_IR_NUMERIC_ELEMENT_U64);
}

static TZrBool vector_element_supported(TZrUInt32 capabilities,
                                         EZrNumericElementKind kind) {
    TZrUInt32 widthCapability;

    if (kind == ZR_EXEC_IR_NUMERIC_ELEMENT_F32) {
        return (TZrBool)((capabilities &
                          ZR_EXEC_IR_NUMERIC_TARGET_CAP_F32) != 0u);
    }
    if (kind == ZR_EXEC_IR_NUMERIC_ELEMENT_F64) {
        return (TZrBool)((capabilities &
                          ZR_EXEC_IR_NUMERIC_TARGET_CAP_F64) != 0u);
    }
    if (kind == ZR_EXEC_IR_NUMERIC_ELEMENT_BOOL ||
        kind == ZR_EXEC_IR_NUMERIC_ELEMENT_MASK) {
        return (TZrBool)((capabilities &
                          ZR_EXEC_IR_NUMERIC_TARGET_CAP_MASK) != 0u);
    }
    if (kind < ZR_EXEC_IR_NUMERIC_ELEMENT_I8 ||
        kind > ZR_EXEC_IR_NUMERIC_ELEMENT_U64 ||
        (capabilities & ZR_EXEC_IR_NUMERIC_TARGET_CAP_INTEGER) == 0u) {
        return ZR_FALSE;
    }
    /* A generic INTEGER capability promises all fixed integer widths.  A
     * backend may publish an additional width bit to document a narrower
     * target, but the generic bit remains the portable fallback contract. */
    switch (kind) {
        case ZR_EXEC_IR_NUMERIC_ELEMENT_I8:
            widthCapability = ZR_EXEC_IR_NUMERIC_TARGET_CAP_I8;
            break;
        case ZR_EXEC_IR_NUMERIC_ELEMENT_I16:
            widthCapability = ZR_EXEC_IR_NUMERIC_TARGET_CAP_I16;
            break;
        case ZR_EXEC_IR_NUMERIC_ELEMENT_I32:
            widthCapability = ZR_EXEC_IR_NUMERIC_TARGET_CAP_I32;
            break;
        case ZR_EXEC_IR_NUMERIC_ELEMENT_I64:
            widthCapability = ZR_EXEC_IR_NUMERIC_TARGET_CAP_I64;
            break;
        case ZR_EXEC_IR_NUMERIC_ELEMENT_U8:
            widthCapability = ZR_EXEC_IR_NUMERIC_TARGET_CAP_U8;
            break;
        case ZR_EXEC_IR_NUMERIC_ELEMENT_U16:
            widthCapability = ZR_EXEC_IR_NUMERIC_TARGET_CAP_U16;
            break;
        case ZR_EXEC_IR_NUMERIC_ELEMENT_U32:
            widthCapability = ZR_EXEC_IR_NUMERIC_TARGET_CAP_U32;
            break;
        case ZR_EXEC_IR_NUMERIC_ELEMENT_U64:
            widthCapability = ZR_EXEC_IR_NUMERIC_TARGET_CAP_U64;
            break;
        default:
            widthCapability = 0u;
            break;
    }
    return (TZrBool)((capabilities & widthCapability) != 0u ||
                     (capabilities & ZR_EXEC_IR_NUMERIC_TARGET_CAP_INTEGER) != 0u);
}

static TZrBool vector_operation_is_reduction(EZrNumericVectorOperation operation) {
    return (TZrBool)(operation == ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_ADD ||
                     operation == ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MIN ||
                     operation == ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MAX);
}

static TZrBool vector_operation_is_memory(EZrNumericVectorOperation operation) {
    return (TZrBool)(operation == ZR_EXEC_IR_NUMERIC_VECTOR_LOAD ||
                     operation == ZR_EXEC_IR_NUMERIC_VECTOR_STORE);
}

static TZrBool vector_operation_valid(EZrNumericVectorOperation operation,
                                      EZrNumericElementKind elementKind) {
    if (operation <= ZR_EXEC_IR_NUMERIC_VECTOR_INVALID ||
        operation >= ZR_EXEC_IR_NUMERIC_VECTOR_OPERATION_COUNT ||
        elementKind <= ZR_EXEC_IR_NUMERIC_ELEMENT_INVALID ||
        elementKind >= ZR_EXEC_IR_NUMERIC_ELEMENT_COUNT) {
        return ZR_FALSE;
    }
    if (operation == ZR_EXEC_IR_NUMERIC_VECTOR_FMA &&
        !vector_is_float(elementKind)) {
        return ZR_FALSE;
    }
    if ((operation == ZR_EXEC_IR_NUMERIC_VECTOR_SHIFT_LEFT ||
         operation == ZR_EXEC_IR_NUMERIC_VECTOR_SHIFT_RIGHT ||
         operation == ZR_EXEC_IR_NUMERIC_VECTOR_MOD) &&
        !vector_is_integer(elementKind)) {
        return ZR_FALSE;
    }
    if (vector_operation_is_reduction(operation) &&
        !vector_is_float(elementKind) && !vector_is_integer(elementKind)) {
        return ZR_FALSE;
    }
    if ((operation == ZR_EXEC_IR_NUMERIC_VECTOR_ADD ||
         operation == ZR_EXEC_IR_NUMERIC_VECTOR_SUB ||
         operation == ZR_EXEC_IR_NUMERIC_VECTOR_MUL ||
         operation == ZR_EXEC_IR_NUMERIC_VECTOR_DIV ||
         operation == ZR_EXEC_IR_NUMERIC_VECTOR_NEG) &&
        (elementKind == ZR_EXEC_IR_NUMERIC_ELEMENT_MASK ||
         elementKind == ZR_EXEC_IR_NUMERIC_ELEMENT_BOOL)) {
        return ZR_FALSE;
    }
    if (operation == ZR_EXEC_IR_NUMERIC_VECTOR_SELECT &&
        elementKind == ZR_EXEC_IR_NUMERIC_ELEMENT_MASK) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrUInt64 vector_request_hash(const SZrVectorLegalizationRequest *request,
                                     const SZrNumericPolicy *policy) {
    TZrUInt64 hash = ZR_NUMERIC_LEGALIZE_HASH_OFFSET;
    if (request == ZR_NULL) {
        return 0u;
    }
    vector_hash_u32(&hash, request->schemaVersion);
    vector_hash_u32(&hash, (TZrUInt32)request->operation);
    vector_hash_u32(&hash, (TZrUInt32)request->elementKind);
    vector_hash_u64(&hash, request->length);
    vector_hash_u32(&hash, request->targetVectorLanes);
    vector_hash_u32(&hash, request->targetCapabilities);
    vector_hash_u32(&hash, request->sourceAlignment);
    vector_hash_u32(&hash, request->requiredAlignment);
    vector_hash_u32(&hash, request->requiredFastMath);
    vector_hash_u64(&hash, request->targetHash);
    vector_hash_u64(&hash, request->targetSignatureHash);
    vector_hash_u64(&hash, request->targetLayoutHash);
    vector_hash_u32(&hash, request->functionToken);
    vector_hash_u32(&hash, request->blockId);
    vector_hash_u32(&hash, request->instructionId);
    vector_hash_u32(&hash, request->sourceId);
    vector_hash_u32(&hash, (TZrUInt32)request->allowScalarFallback);
    vector_hash_u32(&hash, (TZrUInt32)request->preserveSourceOrder);
    if (policy != ZR_NULL) {
        vector_hash_u64(&hash, ZrParser_ExecIr_NumericPolicyHash(policy));
    }
    return hash;
}

void ZrParser_ExecIr_VectorLegalizationRequestInit(
        SZrVectorLegalizationRequest *request) {
    if (request == ZR_NULL) {
        return;
    }
    (void)memset(request, 0, sizeof(*request));
    request->schemaVersion = ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION;
    request->operation = ZR_EXEC_IR_NUMERIC_VECTOR_INVALID;
    request->elementKind = ZR_EXEC_IR_NUMERIC_ELEMENT_INVALID;
    request->targetVectorLanes = 1u;
    request->requiredAlignment = 1u;
    request->allowScalarFallback = ZR_TRUE;
    request->preserveSourceOrder = ZR_TRUE;
}

void ZrParser_ExecIr_VectorLegalizationResultInit(
        SZrVectorLegalizationResult *result) {
    if (result == ZR_NULL) {
        return;
    }
    (void)memset(result, 0, sizeof(*result));
    result->schemaVersion = ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION;
    result->decision = ZR_EXEC_IR_NUMERIC_LEGALIZE_NONE;
    result->reason = ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_NONE;
    result->chosenLanes = 1u;
    result->preservesNaN = ZR_TRUE;
    result->preservesSignedZero = ZR_TRUE;
    result->preservesExceptions = ZR_TRUE;
    result->preservesSourceOrder = ZR_TRUE;
}

static TZrBool vector_request_validate(
        const SZrVectorLegalizationRequest *request,
        SZrExecIrDiagnostic *diagnostic) {
    if (request == ZR_NULL) {
        vector_diagnostic_set(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION, 0u);
        return ZR_FALSE;
    }
    if (request->schemaVersion != ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION) {
        vector_diagnostic_set(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                              ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION,
                              request->schemaVersion);
        return ZR_FALSE;
    }
    if (!vector_operation_valid(request->operation, request->elementKind) ||
        request->targetVectorLanes == 0u ||
        request->targetVectorLanes > ZR_NUMERIC_MAX_VECTOR_LANES ||
        (request->targetCapabilities &
         ~ZR_EXEC_IR_NUMERIC_TARGET_CAP_KNOWN_MASK) != 0u ||
        (request->requiredFastMath &
         ~ZR_EXEC_IR_NUMERIC_FAST_MATH_KNOWN_MASK) != 0u ||
        !vector_valid_bool(request->allowScalarFallback) ||
        !vector_valid_bool(request->preserveSourceOrder) ||
        !vector_valid_bool(request->reserved0) ||
        !vector_valid_bool(request->reserved1)) {
        vector_diagnostic_set(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION,
                              request->schemaVersion);
        return ZR_FALSE;
    }
    if (request->requiredAlignment != 0u &&
        (request->requiredAlignment & (request->requiredAlignment - 1u)) != 0u) {
        vector_diagnostic_set(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, request->requiredAlignment);
        return ZR_FALSE;
    }
    if (request->sourceAlignment != 0u &&
        (request->sourceAlignment & (request->sourceAlignment - 1u)) != 0u) {
        vector_diagnostic_set(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, request->sourceAlignment);
        return ZR_FALSE;
    }
    if (request->policy != ZR_NULL &&
        !ZrParser_ExecIr_ValidateNumericPolicy(request->policy, diagnostic)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static EZrNumericLegalizationReason vector_unsupported_reason(
        const SZrVectorLegalizationRequest *request,
        const SZrNumericPolicy *policy) {
    if ((request->targetCapabilities & ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR) == 0u) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_TARGET_UNSUPPORTED;
    }
    if (!vector_element_supported(request->targetCapabilities,
                                  request->elementKind)) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_TARGET_UNSUPPORTED;
    }
    if (request->operation == ZR_EXEC_IR_NUMERIC_VECTOR_SELECT &&
        (request->targetCapabilities & ZR_EXEC_IR_NUMERIC_TARGET_CAP_MASK) == 0u) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_OPERATION_UNSUPPORTED;
    }
    if (vector_operation_is_reduction(request->operation) &&
        (request->targetCapabilities & ZR_EXEC_IR_NUMERIC_TARGET_CAP_REDUCE) == 0u) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_OPERATION_UNSUPPORTED;
    }
    if (request->operation == ZR_EXEC_IR_NUMERIC_VECTOR_FMA &&
        (request->targetCapabilities & ZR_EXEC_IR_NUMERIC_TARGET_CAP_FMA) == 0u) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_TARGET_UNSUPPORTED;
    }
    if (request->operation == ZR_EXEC_IR_NUMERIC_VECTOR_FMA &&
        (policy == ZR_NULL ||
         !ZrParser_ExecIr_NumericPolicyAllows(
                 policy, ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA))) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_FAST_MATH_DENIED;
    }
    if (request->operation == ZR_EXEC_IR_NUMERIC_VECTOR_DIV &&
        (request->requiredFastMath &
         ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL) != 0u &&
        (request->targetCapabilities &
         ZR_EXEC_IR_NUMERIC_TARGET_CAP_APPROX_RECIPROCAL) == 0u) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_TARGET_UNSUPPORTED;
    }
    if ((request->requiredFastMath &
         ~((policy == ZR_NULL) ? ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE :
                               policy->effectiveFastMath)) != 0u) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_FAST_MATH_DENIED;
    }
    if (vector_operation_is_memory(request->operation) &&
        (request->requiredAlignment == 0u || request->sourceAlignment == 0u ||
         request->sourceAlignment < request->requiredAlignment) &&
        (request->targetCapabilities & ZR_EXEC_IR_NUMERIC_TARGET_CAP_UNALIGNED) == 0u) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ALIGNMENT;
    }
    if (vector_operation_is_reduction(request->operation) &&
        request->preserveSourceOrder) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ORDER_REQUIRED;
    }
    if (vector_operation_is_reduction(request->operation) &&
        policy != ZR_NULL &&
        policy->determinism == ZR_EXEC_IR_NUMERIC_DETERMINISM_STRICT_ORDERED) {
        return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ORDER_REQUIRED;
    }
    return ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_NONE;
}

TZrBool ZrParser_ExecIr_LegalizeVector(
        const SZrVectorLegalizationRequest *request,
        SZrVectorLegalizationResult *result,
        SZrExecIrDiagnostic *diagnostic) {
    SZrNumericPolicy defaultPolicy;
    const SZrNumericPolicy *policy;
    EZrNumericLegalizationReason reason;
    TZrUInt64 hash;

    vector_diagnostic_clear(diagnostic);
    if (result != ZR_NULL) {
        ZrParser_ExecIr_VectorLegalizationResultInit(result);
    }
    if (result == ZR_NULL || !vector_request_validate(request, diagnostic)) {
        vector_diagnostic_apply_location(diagnostic, request);
        if (result == ZR_NULL && diagnostic != ZR_NULL) {
            vector_diagnostic_set(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION, 0u);
            vector_diagnostic_apply_location(diagnostic, request);
        }
        return ZR_FALSE;
    }

    ZrParser_ExecIr_NumericPolicyInit(&defaultPolicy);
    policy = request->policy == ZR_NULL ? &defaultPolicy : request->policy;
    hash = vector_request_hash(request, policy);
    result->operation = request->operation;
    result->elementKind = request->elementKind;
    result->functionToken = request->functionToken;
    result->blockId = request->blockId;
    result->instructionId = request->instructionId;
    result->sourceId = request->sourceId;
    result->contractHash = hash;
    result->scalarIterations = request->length;
    result->preservesSourceOrder = request->preserveSourceOrder;
    result->preservesNaN = policy->preserveNaN;
    result->preservesSignedZero = policy->preserveSignedZero;
    result->preservesExceptions = policy->preserveExceptions;
    result->appliedFastMath = policy->effectiveFastMath;

    reason = vector_unsupported_reason(request, policy);
    if (reason != ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_NONE) {
        result->reason = reason;
        result->decision = ZR_EXEC_IR_NUMERIC_LEGALIZE_SCALAR_FALLBACK;
        result->usesScalarFallback = ZR_TRUE;
        result->chosenLanes = 1u;
        result->vectorIterations = 0u;
        result->tailLanes = 0u;
        result->appliedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE;
        if (reason == ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ORDER_REQUIRED) {
            result->preservesSourceOrder = ZR_TRUE;
        }
        if (!request->allowScalarFallback) {
            EZrExecutionDiagnosticCode code =
                    reason == ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ALIGNMENT
                    ? ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH
                    : ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
            vector_diagnostic_set(diagnostic, code,
                                  request->targetCapabilities,
                                  (TZrUInt64)reason);
            vector_diagnostic_apply_location(diagnostic, request);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    result->decision = ZR_EXEC_IR_NUMERIC_LEGALIZE_VECTOR;
    result->reason = ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_NONE;
    result->usesScalarFallback = ZR_FALSE;
    result->chosenLanes = request->targetVectorLanes;
    result->vectorIterations = request->length / request->targetVectorLanes;
    result->tailLanes = (TZrUInt32)(request->length % request->targetVectorLanes);
    return ZR_TRUE;
}
