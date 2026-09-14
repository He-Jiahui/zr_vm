#include "zr_vm_core/exec_ir_numeric.h"

#include <stdint.h>
#include <string.h>

#define ZR_NUMERIC_HASH_OFFSET UINT64_C(1469598103934665603)
#define ZR_NUMERIC_HASH_PRIME UINT64_C(1099511628211)

static void numeric_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 byte;
    if (hash == ZR_NULL) {
        return;
    }
    for (byte = 0u; byte < 4u; ++byte) {
        *hash ^= (TZrUInt64)((value >> (byte * 8u)) & 0xffu);
        *hash *= ZR_NUMERIC_HASH_PRIME;
    }
}

static void numeric_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    numeric_hash_u32(hash, (TZrUInt32)value);
    numeric_hash_u32(hash, (TZrUInt32)(value >> 32u));
}

static void numeric_diagnostic_clear(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        (void)memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static void numeric_diagnostic_set(SZrExecIrDiagnostic *diagnostic,
                                   EZrExecutionDiagnosticCode code,
                                   TZrUInt64 expected,
                                   TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    numeric_diagnostic_clear(diagnostic);
    diagnostic->code = code;
    diagnostic->expectedVersion = (TZrUInt32)expected;
    diagnostic->actualVersion = (TZrUInt32)actual;
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
}

static TZrBool numeric_is_float(EZrNumericElementKind kind) {
    return (TZrBool)(kind == ZR_EXEC_IR_NUMERIC_ELEMENT_F32 ||
                     kind == ZR_EXEC_IR_NUMERIC_ELEMENT_F64);
}

static TZrBool numeric_is_scalar_element(EZrNumericElementKind kind) {
    return (TZrBool)(kind > ZR_EXEC_IR_NUMERIC_ELEMENT_INVALID &&
                     kind < ZR_EXEC_IR_NUMERIC_ELEMENT_MASK);
}

static TZrBool numeric_valid_bool(TZrBool value) {
    return (TZrBool)(value == ZR_FALSE || value == ZR_TRUE);
}

static TZrBool numeric_operation_is_reduction(EZrNumericVectorOperation operation) {
    return (TZrBool)(operation == ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_ADD ||
                     operation == ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MIN ||
                     operation == ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MAX);
}

static TZrBool numeric_operation_is_integer_only(
        EZrNumericVectorOperation operation) {
    return (TZrBool)(operation == ZR_EXEC_IR_NUMERIC_VECTOR_SHIFT_LEFT ||
                     operation == ZR_EXEC_IR_NUMERIC_VECTOR_SHIFT_RIGHT ||
                     operation == ZR_EXEC_IR_NUMERIC_VECTOR_MOD);
}

static TZrBool numeric_validate_policy_shape(const SZrNumericPolicy *policy,
                                             SZrExecIrDiagnostic *diagnostic,
                                             TZrBool checkEffectiveRules) {
    TZrUInt32 permitted;

    if (policy == ZR_NULL) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                               ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION, 0u);
        return ZR_FALSE;
    }
    if (policy->schemaVersion != ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                               ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION,
                               policy->schemaVersion);
        return ZR_FALSE;
    }
    if (policy->overflowMode <= ZR_EXEC_IR_NUMERIC_OVERFLOW_INVALID ||
        policy->overflowMode >= ZR_EXEC_IR_NUMERIC_OVERFLOW_MODE_COUNT ||
        policy->shiftMode <= ZR_EXEC_IR_NUMERIC_SHIFT_INVALID ||
        policy->shiftMode >= ZR_EXEC_IR_NUMERIC_SHIFT_MODE_COUNT ||
        policy->floatSemantics >= ZR_EXEC_IR_NUMERIC_FLOAT_SEMANTICS_COUNT ||
        policy->determinism >= ZR_EXEC_IR_NUMERIC_DETERMINISM_COUNT ||
        policy->divideMode <= ZR_EXEC_IR_NUMERIC_DIVIDE_INVALID ||
        policy->divideMode >= ZR_EXEC_IR_NUMERIC_DIVIDE_MODE_COUNT ||
        policy->nanMode >= ZR_EXEC_IR_NUMERIC_NAN_MODE_COUNT ||
        policy->roundingMode >= ZR_EXEC_IR_NUMERIC_ROUND_MODE_COUNT ||
        policy->exceptionMode >= ZR_EXEC_IR_NUMERIC_EXCEPTIONS_MODE_COUNT ||
        (policy->requestedFastMath & ~ZR_EXEC_IR_NUMERIC_FAST_MATH_KNOWN_MASK) != 0u ||
        (policy->allowedFastMath & ~ZR_EXEC_IR_NUMERIC_FAST_MATH_KNOWN_MASK) != 0u ||
        (policy->effectiveFastMath & ~ZR_EXEC_IR_NUMERIC_FAST_MATH_KNOWN_MASK) != 0u ||
        (policy->deniedFastMath & ~ZR_EXEC_IR_NUMERIC_FAST_MATH_KNOWN_MASK) != 0u ||
        (policy->targetCapabilities & ~ZR_EXEC_IR_NUMERIC_TARGET_CAP_KNOWN_MASK) != 0u ||
        !numeric_valid_bool(policy->preserveNaN) ||
        !numeric_valid_bool(policy->preserveSignedZero) ||
        !numeric_valid_bool(policy->preserveExceptions) ||
        !numeric_valid_bool(policy->reserved0)) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                               ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION,
                               policy->schemaVersion);
        return ZR_FALSE;
    }

    permitted = policy->requestedFastMath & policy->allowedFastMath;
    /* A request is allowed to leave effectiveFastMath/deniedFastMath empty;
     * those fields are populated by the intersection step.  Once a caller
     * publishes a non-zero denied mask, however, it must be the exact
     * complement of the effective permissions. */
    if ((policy->effectiveFastMath & ~permitted) != 0u ||
        (policy->deniedFastMath != 0u &&
         policy->deniedFastMath !=
                 (policy->requestedFastMath & ~policy->effectiveFastMath))) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
                               permitted, policy->effectiveFastMath);
        return ZR_FALSE;
    }

    if (checkEffectiveRules) {
        /* Strict IEEE and bitwise profiles cannot silently acquire a
         * transformation that changes association, exception behavior, or
         * signed-zero observability. */
        if (policy->floatSemantics != ZR_EXEC_IR_NUMERIC_FLOAT_RELAXED &&
            (policy->effectiveFastMath &
             (ZR_EXEC_IR_NUMERIC_FAST_MATH_REASSOCIATE |
              ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA |
              ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL |
              ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO |
              ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION)) != 0u) {
            numeric_diagnostic_set(diagnostic,
                                   ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_EXEC_IR_NUMERIC_FLOAT_RELAXED,
                                   policy->floatSemantics);
            return ZR_FALSE;
        }
        if (policy->determinism != ZR_EXEC_IR_NUMERIC_DETERMINISM_RELAXED &&
            (policy->effectiveFastMath &
             (ZR_EXEC_IR_NUMERIC_FAST_MATH_REASSOCIATE |
              ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA |
              ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION)) != 0u) {
            numeric_diagnostic_set(diagnostic,
                                   ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_EXEC_IR_NUMERIC_DETERMINISM_RELAXED,
                                   policy->determinism);
            return ZR_FALSE;
        }
        if ((policy->effectiveFastMath &
             ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO) != 0u &&
            policy->preserveSignedZero) {
            numeric_diagnostic_set(diagnostic,
                                   ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_FALSE, policy->preserveSignedZero);
            return ZR_FALSE;
        }
        if (policy->floatSemantics == ZR_EXEC_IR_NUMERIC_FLOAT_STRICT_IEEE &&
            (!policy->preserveNaN || !policy->preserveSignedZero ||
             !policy->preserveExceptions)) {
            numeric_diagnostic_set(diagnostic,
                                   ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_TRUE, policy->preserveNaN);
            return ZR_FALSE;
        }
        if (policy->determinism ==
                    ZR_EXEC_IR_NUMERIC_DETERMINISM_STRICT_ORDERED &&
            (policy->effectiveFastMath &
             ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION) != 0u) {
            numeric_diagnostic_set(diagnostic,
                                   ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE,
                                   policy->effectiveFastMath);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

void ZrParser_ExecIr_NumericSemanticContractInit(
        SZrNumericSemanticContract *contract) {
    if (contract == ZR_NULL) {
        return;
    }
    (void)memset(contract, 0, sizeof(*contract));
    contract->schemaVersion = ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION;
    contract->elementKind = ZR_EXEC_IR_NUMERIC_ELEMENT_F32;
    contract->overflowMode = ZR_EXEC_IR_NUMERIC_OVERFLOW_CHECKED;
    contract->shiftMode = ZR_EXEC_IR_NUMERIC_SHIFT_MASKED;
    contract->divideMode = ZR_EXEC_IR_NUMERIC_DIVIDE_IEEE;
    contract->nanMode = ZR_EXEC_IR_NUMERIC_NAN_PRESERVE;
    contract->roundingMode = ZR_EXEC_IR_NUMERIC_ROUND_NEAREST_EVEN;
    contract->exceptionMode = ZR_EXEC_IR_NUMERIC_EXCEPTIONS_PRESERVE;
    contract->preserveSignedZero = ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ValidateNumericSemanticContract(
        const SZrNumericSemanticContract *contract,
        SZrExecIrDiagnostic *diagnostic) {
    numeric_diagnostic_clear(diagnostic);
    if (contract == ZR_NULL ||
        contract->schemaVersion != ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION ||
        !numeric_is_scalar_element(contract->elementKind) ||
        contract->overflowMode <= ZR_EXEC_IR_NUMERIC_OVERFLOW_INVALID ||
        contract->overflowMode >= ZR_EXEC_IR_NUMERIC_OVERFLOW_MODE_COUNT ||
        contract->shiftMode <= ZR_EXEC_IR_NUMERIC_SHIFT_INVALID ||
        contract->shiftMode >= ZR_EXEC_IR_NUMERIC_SHIFT_MODE_COUNT ||
        contract->divideMode <= ZR_EXEC_IR_NUMERIC_DIVIDE_INVALID ||
        contract->divideMode >= ZR_EXEC_IR_NUMERIC_DIVIDE_MODE_COUNT ||
        contract->nanMode >= ZR_EXEC_IR_NUMERIC_NAN_MODE_COUNT ||
        contract->roundingMode >= ZR_EXEC_IR_NUMERIC_ROUND_MODE_COUNT ||
        contract->exceptionMode >= ZR_EXEC_IR_NUMERIC_EXCEPTIONS_MODE_COUNT ||
        !numeric_valid_bool(contract->preserveSignedZero)) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                               ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION,
                               contract == ZR_NULL ? 0u : contract->schemaVersion);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_NumericSemanticsFor(
        EZrNumericVectorOperation operation,
        EZrNumericElementKind elementKind,
        const SZrNumericPolicy *policy,
        SZrNumericSemanticContract *contract,
        SZrExecIrDiagnostic *diagnostic) {
    SZrNumericPolicy defaultPolicy;
    const SZrNumericPolicy *sourcePolicy;
    TZrBool isFloat;
    TZrBool isInteger;

    numeric_diagnostic_clear(diagnostic);
    if (contract != ZR_NULL) {
        (void)memset(contract, 0, sizeof(*contract));
    }
    if (contract == ZR_NULL ||
        operation <= ZR_EXEC_IR_NUMERIC_VECTOR_INVALID ||
        operation >= ZR_EXEC_IR_NUMERIC_VECTOR_OPERATION_COUNT ||
        elementKind <= ZR_EXEC_IR_NUMERIC_ELEMENT_INVALID ||
        elementKind >= ZR_EXEC_IR_NUMERIC_ELEMENT_MASK) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                               ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION, 0u);
        return ZR_FALSE;
    }
    ZrParser_ExecIr_NumericPolicyInit(&defaultPolicy);
    sourcePolicy = policy == ZR_NULL ? &defaultPolicy : policy;
    if (!ZrParser_ExecIr_ValidateNumericPolicy(sourcePolicy, diagnostic)) {
        return ZR_FALSE;
    }
    isFloat = numeric_is_float(elementKind);
    isInteger = (TZrBool)(elementKind >= ZR_EXEC_IR_NUMERIC_ELEMENT_I8 &&
                          elementKind <= ZR_EXEC_IR_NUMERIC_ELEMENT_U64);
    if (numeric_operation_is_integer_only(operation) && !isInteger) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                               ZR_EXEC_IR_NUMERIC_ELEMENT_I8,
                               (TZrUInt64)elementKind);
        return ZR_FALSE;
    }
    if (operation == ZR_EXEC_IR_NUMERIC_VECTOR_FMA && !isFloat) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                               ZR_EXEC_IR_NUMERIC_ELEMENT_F32,
                               (TZrUInt64)elementKind);
        return ZR_FALSE;
    }
    if (numeric_operation_is_reduction(operation) && !isFloat && !isInteger) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                               ZR_EXEC_IR_NUMERIC_ELEMENT_I8,
                               (TZrUInt64)elementKind);
        return ZR_FALSE;
    }
    if (operation == ZR_EXEC_IR_NUMERIC_VECTOR_FMA &&
        !ZrParser_ExecIr_NumericPolicyAllows(
                sourcePolicy, ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA)) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
                               ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA,
                               sourcePolicy->effectiveFastMath);
        return ZR_FALSE;
    }

    ZrParser_ExecIr_NumericSemanticContractInit(contract);
    contract->elementKind = elementKind;
    contract->overflowMode = sourcePolicy->overflowMode;
    contract->shiftMode = sourcePolicy->shiftMode;
    contract->divideMode = isFloat ? ZR_EXEC_IR_NUMERIC_DIVIDE_IEEE
                                   : ZR_EXEC_IR_NUMERIC_DIVIDE_CHECKED;
    contract->nanMode = isFloat ? sourcePolicy->nanMode
                                : ZR_EXEC_IR_NUMERIC_NAN_PRESERVE;
    contract->roundingMode = sourcePolicy->roundingMode;
    contract->exceptionMode = sourcePolicy->exceptionMode;
    contract->preserveSignedZero = sourcePolicy->preserveSignedZero;
    if (!isFloat) {
        contract->roundingMode = ZR_EXEC_IR_NUMERIC_ROUND_TOWARD_ZERO;
        contract->exceptionMode = ZR_EXEC_IR_NUMERIC_EXCEPTIONS_PRESERVE;
    }
    if (numeric_operation_is_reduction(operation) &&
        (sourcePolicy->effectiveFastMath &
         ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION) != 0u) {
        contract->preserveSignedZero = ZR_FALSE;
    }
    return ZrParser_ExecIr_ValidateNumericSemanticContract(contract, diagnostic);
}

void ZrParser_ExecIr_NumericPolicyInit(SZrNumericPolicy *policy) {
    if (policy == ZR_NULL) {
        return;
    }
    (void)memset(policy, 0, sizeof(*policy));
    policy->schemaVersion = ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION;
    policy->overflowMode = ZR_EXEC_IR_NUMERIC_OVERFLOW_CHECKED;
    policy->shiftMode = ZR_EXEC_IR_NUMERIC_SHIFT_MASKED;
    policy->floatSemantics = ZR_EXEC_IR_NUMERIC_FLOAT_STRICT_IEEE;
    policy->determinism = ZR_EXEC_IR_NUMERIC_DETERMINISM_STRICT_ORDERED;
    policy->divideMode = ZR_EXEC_IR_NUMERIC_DIVIDE_IEEE;
    policy->nanMode = ZR_EXEC_IR_NUMERIC_NAN_PRESERVE;
    policy->roundingMode = ZR_EXEC_IR_NUMERIC_ROUND_NEAREST_EVEN;
    policy->exceptionMode = ZR_EXEC_IR_NUMERIC_EXCEPTIONS_PRESERVE;
    policy->preserveNaN = ZR_TRUE;
    policy->preserveSignedZero = ZR_TRUE;
    policy->preserveExceptions = ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ValidateNumericPolicy(
        const SZrNumericPolicy *policy, SZrExecIrDiagnostic *diagnostic) {
    numeric_diagnostic_clear(diagnostic);
    return numeric_validate_policy_shape(policy, diagnostic, ZR_TRUE);
}

TZrUInt64 ZrParser_ExecIr_NumericPolicyHash(
        const SZrNumericPolicy *policy) {
    TZrUInt64 hash = ZR_NUMERIC_HASH_OFFSET;
    if (policy == ZR_NULL) {
        return 0u;
    }
    numeric_hash_u32(&hash, policy->schemaVersion);
    numeric_hash_u32(&hash, (TZrUInt32)policy->overflowMode);
    numeric_hash_u32(&hash, (TZrUInt32)policy->shiftMode);
    numeric_hash_u32(&hash, (TZrUInt32)policy->floatSemantics);
    numeric_hash_u32(&hash, (TZrUInt32)policy->determinism);
    numeric_hash_u32(&hash, (TZrUInt32)policy->divideMode);
    numeric_hash_u32(&hash, (TZrUInt32)policy->nanMode);
    numeric_hash_u32(&hash, (TZrUInt32)policy->roundingMode);
    numeric_hash_u32(&hash, (TZrUInt32)policy->exceptionMode);
    numeric_hash_u32(&hash, policy->requestedFastMath);
    numeric_hash_u32(&hash, policy->allowedFastMath);
    numeric_hash_u32(&hash, policy->effectiveFastMath);
    numeric_hash_u32(&hash, policy->deniedFastMath);
    numeric_hash_u32(&hash, policy->targetCapabilities);
    numeric_hash_u32(&hash, (TZrUInt32)policy->preserveNaN);
    numeric_hash_u32(&hash, (TZrUInt32)policy->preserveSignedZero);
    numeric_hash_u32(&hash, (TZrUInt32)policy->preserveExceptions);
    numeric_hash_u32(&hash, (TZrUInt32)policy->reserved0);
    numeric_hash_u64(&hash, policy->projectHash);
    numeric_hash_u64(&hash, policy->packageHash);
    numeric_hash_u64(&hash, policy->targetHash);
    return hash;
}

TZrBool ZrParser_ExecIr_NumericPolicyIntersect(
        const SZrNumericPolicy *requested,
        const SZrNumericPolicy *project,
        const SZrNumericPolicy *package,
        const SZrNumericPolicy *target,
        SZrNumericPolicy *effective,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 allowed;

    numeric_diagnostic_clear(diagnostic);
    if (effective != ZR_NULL) {
        (void)memset(effective, 0, sizeof(*effective));
    }
    if (requested == ZR_NULL || project == ZR_NULL || package == ZR_NULL ||
        target == ZR_NULL || effective == ZR_NULL) {
        numeric_diagnostic_set(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                               ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION, 0u);
        return ZR_FALSE;
    }
    if (!numeric_validate_policy_shape(requested, diagnostic, ZR_FALSE) ||
        !numeric_validate_policy_shape(project, diagnostic, ZR_FALSE) ||
        !numeric_validate_policy_shape(package, diagnostic, ZR_FALSE) ||
        !numeric_validate_policy_shape(target, diagnostic, ZR_FALSE)) {
        return ZR_FALSE;
    }

    *effective = *requested;
    allowed = project->allowedFastMath & package->allowedFastMath &
              target->allowedFastMath;
    /* A request with an explicit allowed mask can further narrow the
     * intersection.  Zero means "not specified" for the transient request,
     * matching the initializer's conservative defaults. */
    if (requested->allowedFastMath != ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE) {
        allowed &= requested->allowedFastMath;
    }
    effective->allowedFastMath = allowed;
    effective->effectiveFastMath = requested->requestedFastMath & allowed;
    effective->deniedFastMath = requested->requestedFastMath &
                                ~effective->effectiveFastMath;
    effective->targetCapabilities = target->targetCapabilities;
    effective->projectHash = project->projectHash;
    effective->packageHash = package->packageHash;
    effective->targetHash = target->targetHash;

    /* The effective profile exposes the semantic consequences of each
     * accepted transform.  A denied transform never weakens the baseline. */
    if ((effective->effectiveFastMath &
         ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO) != 0u) {
        effective->preserveSignedZero = ZR_FALSE;
    }
    if ((effective->effectiveFastMath &
         (ZR_EXEC_IR_NUMERIC_FAST_MATH_REASSOCIATE |
          ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL |
          ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION)) != 0u) {
        effective->preserveExceptions = ZR_FALSE;
    }
    if (!numeric_validate_policy_shape(effective, diagnostic, ZR_TRUE)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_NumericPolicyAllows(
        const SZrNumericPolicy *policy, TZrUInt32 fastMathFlags) {
    if (policy == ZR_NULL ||
        (fastMathFlags & ~ZR_EXEC_IR_NUMERIC_FAST_MATH_KNOWN_MASK) != 0u) {
        return ZR_FALSE;
    }
    if (!ZrParser_ExecIr_ValidateNumericPolicy(policy, ZR_NULL)) {
        return ZR_FALSE;
    }
    return (TZrBool)((fastMathFlags & ~policy->effectiveFastMath) == 0u);
}

const TZrChar *ZrParser_ExecIr_NumericElementName(
        EZrNumericElementKind kind) {
    static const TZrChar *const names[] = {
        "invalid", "bool", "i8", "i16", "i32", "i64", "u8", "u16",
        "u32", "u64", "f32", "f64", "mask"
    };
    if (kind < ZR_EXEC_IR_NUMERIC_ELEMENT_INVALID ||
        kind >= ZR_EXEC_IR_NUMERIC_ELEMENT_COUNT) {
        return "invalid";
    }
    return names[(TZrUInt32)kind];
}

const TZrChar *ZrParser_ExecIr_NumericLegalizationReasonName(
        EZrNumericLegalizationReason reason) {
    static const TZrChar *const names[] = {
        "none", "target-unsupported", "alignment", "lane-unsupported",
        "operation-unsupported", "fast-math-denied", "order-required",
        "empty-input", "invalid"
    };
    if (reason < ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_NONE ||
        reason >= ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_COUNT) {
        return "invalid";
    }
    return names[(TZrUInt32)reason];
}
