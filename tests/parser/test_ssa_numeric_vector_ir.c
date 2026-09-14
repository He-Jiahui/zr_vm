/*
 * Focused contract tests for 09.02 strict numeric semantics and portable
 * vector legalization.  The test deliberately uses only the pointer-free
 * parser/core contract so it can also be compiled as a small standalone
 * fixture before the regular CMake/CTest registration is added.
 */
#include "zr_vm_core/exec_ir_numeric.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_strict_policy_defaults_and_hash(void) {
    SZrNumericPolicy policy;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt64 firstHash;

    ZrParser_ExecIr_NumericPolicyInit(&policy);
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(policy.schemaVersion == ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION);
    assert(policy.overflowMode == ZR_EXEC_IR_NUMERIC_OVERFLOW_CHECKED);
    assert(policy.floatSemantics == ZR_EXEC_IR_NUMERIC_FLOAT_STRICT_IEEE);
    assert(policy.determinism == ZR_EXEC_IR_NUMERIC_DETERMINISM_STRICT_ORDERED);
    assert(policy.requestedFastMath == ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE);
    assert(policy.effectiveFastMath == ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE);
    assert(policy.preserveNaN == ZR_TRUE);
    assert(policy.preserveSignedZero == ZR_TRUE);
    assert(policy.preserveExceptions == ZR_TRUE);
    assert(ZrParser_ExecIr_ValidateNumericPolicy(&policy, &diagnostic));
    firstHash = ZrParser_ExecIr_NumericPolicyHash(&policy);
    assert(firstHash != 0u);
    assert(firstHash == ZrParser_ExecIr_NumericPolicyHash(&policy));
}

static void test_operation_semantic_row_is_explicit(void) {
    SZrNumericSemanticContract contract;
    SZrNumericPolicy policy;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_ExecIr_NumericSemanticContractInit(&contract);
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(contract.overflowMode == ZR_EXEC_IR_NUMERIC_OVERFLOW_CHECKED);
    assert(contract.shiftMode == ZR_EXEC_IR_NUMERIC_SHIFT_MASKED);
    assert(contract.divideMode == ZR_EXEC_IR_NUMERIC_DIVIDE_IEEE);
    assert(contract.nanMode == ZR_EXEC_IR_NUMERIC_NAN_PRESERVE);
    assert(contract.roundingMode == ZR_EXEC_IR_NUMERIC_ROUND_NEAREST_EVEN);
    assert(contract.exceptionMode == ZR_EXEC_IR_NUMERIC_EXCEPTIONS_PRESERVE);
    assert(contract.preserveSignedZero == ZR_TRUE);
    assert(ZrParser_ExecIr_ValidateNumericSemanticContract(&contract,
                                                            &diagnostic));

    contract.elementKind = ZR_EXEC_IR_NUMERIC_ELEMENT_MASK;
    assert(!ZrParser_ExecIr_ValidateNumericSemanticContract(&contract,
                                                              &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);

    ZrParser_ExecIr_NumericPolicyInit(&policy);
    assert(ZrParser_ExecIr_NumericSemanticsFor(
            ZR_EXEC_IR_NUMERIC_VECTOR_ADD,
            ZR_EXEC_IR_NUMERIC_ELEMENT_I64,
            &policy, &contract, &diagnostic));
    assert(contract.overflowMode == ZR_EXEC_IR_NUMERIC_OVERFLOW_CHECKED);
    assert(contract.divideMode == ZR_EXEC_IR_NUMERIC_DIVIDE_CHECKED);
    assert(ZrParser_ExecIr_NumericSemanticsFor(
            ZR_EXEC_IR_NUMERIC_VECTOR_SHIFT_LEFT,
            ZR_EXEC_IR_NUMERIC_ELEMENT_U32,
            &policy, &contract, &diagnostic));
    assert(contract.shiftMode == ZR_EXEC_IR_NUMERIC_SHIFT_MASKED);
}

static void test_policy_rejects_unknown_values_and_contradictions(void) {
    SZrNumericPolicy policy;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_ExecIr_NumericPolicyInit(&policy);
    policy.requestedFastMath = ((TZrUInt32)1u << 31u);
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(!ZrParser_ExecIr_ValidateNumericPolicy(&policy, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);

    ZrParser_ExecIr_NumericPolicyInit(&policy);
    policy.overflowMode = (EZrNumericOverflowMode)99;
    assert(!ZrParser_ExecIr_ValidateNumericPolicy(&policy, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);

    ZrParser_ExecIr_NumericPolicyInit(&policy);
    policy.requestedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO;
    policy.allowedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO;
    policy.effectiveFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO;
    /* Strict IEEE mode cannot opt out of signed-zero observability. */
    assert(!ZrParser_ExecIr_ValidateNumericPolicy(&policy, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
}

static void test_permissions_intersect_without_fast_math_leakage(void) {
    SZrNumericPolicy project;
    SZrNumericPolicy package;
    SZrNumericPolicy target;
    SZrNumericPolicy requested;
    SZrNumericPolicy effective;
    SZrExecIrDiagnostic diagnostic;
    const TZrUInt32 allFlags = ZR_EXEC_IR_NUMERIC_FAST_MATH_ALL;

    ZrParser_ExecIr_NumericPolicyInit(&project);
    ZrParser_ExecIr_NumericPolicyInit(&package);
    ZrParser_ExecIr_NumericPolicyInit(&target);
    ZrParser_ExecIr_NumericPolicyInit(&requested);
    project.allowedFastMath = allFlags;
    package.allowedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_REASSOCIATE |
                              ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA;
    target.allowedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA;
    requested.requestedFastMath = allFlags;
    requested.floatSemantics = ZR_EXEC_IR_NUMERIC_FLOAT_RELAXED;
    requested.determinism = ZR_EXEC_IR_NUMERIC_DETERMINISM_RELAXED;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(ZrParser_ExecIr_NumericPolicyIntersect(
            &requested, &project, &package, &target, &effective, &diagnostic));
    assert(effective.effectiveFastMath == ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA);
    assert((effective.effectiveFastMath &
            ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION) == 0u);
    assert((effective.effectiveFastMath &
            ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL) == 0u);
    assert(ZrParser_ExecIr_ValidateNumericPolicy(&effective, &diagnostic));

    /* A strict package cannot inherit project permissions. */
    package.allowedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE;
    assert(ZrParser_ExecIr_NumericPolicyIntersect(
            &requested, &project, &package, &target, &effective, &diagnostic));
    assert(effective.effectiveFastMath == ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE);
    assert(effective.preserveNaN == ZR_TRUE);
    assert(effective.preserveSignedZero == ZR_TRUE);
}

static void test_fma_permission_is_independent_of_reassociation_and_reduction(void) {
    SZrNumericPolicy project;
    SZrNumericPolicy package;
    SZrNumericPolicy target;
    SZrNumericPolicy requested;
    SZrNumericPolicy effective;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_ExecIr_NumericPolicyInit(&project);
    ZrParser_ExecIr_NumericPolicyInit(&package);
    ZrParser_ExecIr_NumericPolicyInit(&target);
    ZrParser_ExecIr_NumericPolicyInit(&requested);
    project.allowedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_ALL;
    package.allowedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA;
    target.allowedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA;
    requested.requestedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA |
                                  ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION |
                                  ZR_EXEC_IR_NUMERIC_FAST_MATH_REASSOCIATE;
    requested.floatSemantics = ZR_EXEC_IR_NUMERIC_FLOAT_RELAXED;
    requested.determinism = ZR_EXEC_IR_NUMERIC_DETERMINISM_RELAXED;
    assert(ZrParser_ExecIr_NumericPolicyIntersect(
            &requested, &project, &package, &target, &effective, &diagnostic));
    assert(effective.effectiveFastMath == ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA);
    assert(ZrParser_ExecIr_NumericPolicy_Allows(
            &effective, ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA));
    assert(!ZrParser_ExecIr_NumericPolicy_Allows(
            &effective, ZR_EXEC_IR_NUMERIC_FAST_MATH_REASSOCIATE));
    assert(!ZrParser_ExecIr_NumericPolicy_Allows(
            &effective, ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION));
}

static SZrVectorLegalizationRequest make_request(
        EZrNumericVectorOperation operation,
        EZrNumericElementKind elementKind,
        TZrUInt64 length) {
    SZrVectorLegalizationRequest request;
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION;
    request.operation = operation;
    request.elementKind = elementKind;
    request.length = length;
    request.sourceAlignment = 16u;
    request.requiredAlignment = 4u;
    request.targetVectorLanes = 4u;
    request.targetCapabilities = ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR |
                                 ZR_EXEC_IR_NUMERIC_TARGET_CAP_F32 |
                                 ZR_EXEC_IR_NUMERIC_TARGET_CAP_MASK;
    request.allowScalarFallback = ZR_TRUE;
    request.preserveSourceOrder = ZR_TRUE;
    return request;
}

static void test_vector_legalization_and_scalar_fallback(void) {
    SZrVectorLegalizationRequest request;
    SZrVectorLegalizationResult result;
    SZrExecIrDiagnostic diagnostic;

    request = make_request(ZR_EXEC_IR_NUMERIC_VECTOR_LOAD,
                           ZR_EXEC_IR_NUMERIC_ELEMENT_F32, 9u);
    assert(ZrParser_ExecIr_LegalizeVector(&request, &result, &diagnostic));
    assert(result.decision == ZR_EXEC_IR_NUMERIC_LEGALIZE_VECTOR);
    assert(result.chosenLanes == 4u);
    assert(result.vectorIterations == 2u);
    assert(result.tailLanes == 1u);
    assert(result.preservesNaN == ZR_TRUE);
    assert(result.preservesSignedZero == ZR_TRUE);

    request.targetCapabilities = ZR_EXEC_IR_NUMERIC_TARGET_CAP_F32;
    assert(ZrParser_ExecIr_LegalizeVector(&request, &result, &diagnostic));
    assert(result.decision == ZR_EXEC_IR_NUMERIC_LEGALIZE_SCALAR_FALLBACK);
    assert(result.reason == ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_TARGET_UNSUPPORTED);
    assert(result.chosenLanes == 1u);
    assert(result.scalarIterations == 9u);
    assert(result.preservesSourceOrder == ZR_TRUE);
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE);

    request.targetCapabilities = ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR |
                                 ZR_EXEC_IR_NUMERIC_TARGET_CAP_F32;
    request.sourceAlignment = 1u;
    request.requiredAlignment = 16u;
    assert(ZrParser_ExecIr_LegalizeVector(&request, &result, &diagnostic));
    assert(result.decision == ZR_EXEC_IR_NUMERIC_LEGALIZE_SCALAR_FALLBACK);
    assert(result.reason == ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ALIGNMENT);
}

static void test_ordered_reduction_never_changes_association(void) {
    SZrNumericPolicy strict;
    SZrNumericPolicy project;
    SZrNumericPolicy package;
    SZrNumericPolicy target;
    SZrNumericPolicy effective;
    SZrVectorLegalizationRequest request;
    SZrVectorLegalizationResult result;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_ExecIr_NumericPolicyInit(&strict);
    ZrParser_ExecIr_NumericPolicyInit(&project);
    ZrParser_ExecIr_NumericPolicyInit(&package);
    ZrParser_ExecIr_NumericPolicyInit(&target);
    project.allowedFastMath = package.allowedFastMath =
            target.allowedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_ALL;
    strict.requestedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE;
    assert(ZrParser_ExecIr_NumericPolicyIntersect(
            &strict, &project, &package, &target, &effective, &diagnostic));

    request = make_request(ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_ADD,
                           ZR_EXEC_IR_NUMERIC_ELEMENT_F64, 8u);
    request.targetCapabilities = ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR |
                                 ZR_EXEC_IR_NUMERIC_TARGET_CAP_F64 |
                                 ZR_EXEC_IR_NUMERIC_TARGET_CAP_REDUCE;
    request.policy = &effective;
    request.preserveSourceOrder = ZR_TRUE;
    assert(ZrParser_ExecIr_LegalizeVector(&request, &result, &diagnostic));
    assert(result.decision == ZR_EXEC_IR_NUMERIC_LEGALIZE_SCALAR_FALLBACK);
    assert(result.reason == ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ORDER_REQUIRED);
    assert(result.preservesSourceOrder == ZR_TRUE);

    effective.requestedFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION;
    effective.effectiveFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION;
    effective.floatSemantics = ZR_EXEC_IR_NUMERIC_FLOAT_RELAXED;
    effective.determinism = ZR_EXEC_IR_NUMERIC_DETERMINISM_RELAXED;
    request.preserveSourceOrder = ZR_FALSE;
    assert(ZrParser_ExecIr_LegalizeVector(&request, &result, &diagnostic));
    assert(result.decision == ZR_EXEC_IR_NUMERIC_LEGALIZE_VECTOR);
    assert(result.preservesSourceOrder == ZR_FALSE);
}

static void test_vector_shape_boundaries_and_operation_validation(void) {
    static const TZrUInt64 lengths[] = {0u, 1u, 3u, 4u, 5u, 7u, 8u, 9u};
    SZrVectorLegalizationRequest request;
    SZrVectorLegalizationResult result;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 index;

    for (index = 0u; index < (TZrUInt32)(sizeof(lengths) / sizeof(lengths[0]));
         ++index) {
        request = make_request(ZR_EXEC_IR_NUMERIC_VECTOR_MUL,
                               ZR_EXEC_IR_NUMERIC_ELEMENT_F32, lengths[index]);
        assert(ZrParser_ExecIr_LegalizeVector(&request, &result, &diagnostic));
        assert(result.vectorIterations == lengths[index] / 4u);
        assert(result.tailLanes == lengths[index] % 4u);
        assert(result.scalarIterations == lengths[index]);
    }

    request = make_request(ZR_EXEC_IR_NUMERIC_VECTOR_FMA,
                           ZR_EXEC_IR_NUMERIC_ELEMENT_I32, 4u);
    assert(!ZrParser_ExecIr_LegalizeVector(&request, &result, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);

    request = make_request(ZR_EXEC_IR_NUMERIC_VECTOR_ADD,
                           ZR_EXEC_IR_NUMERIC_ELEMENT_F32, 4u);
    request.targetVectorLanes = 0u;
    assert(!ZrParser_ExecIr_LegalizeVector(&request, &result, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
}

static void test_policy_hash_and_result_hash_include_target_contract(void) {
    SZrNumericPolicy policy;
    SZrVectorLegalizationRequest first;
    SZrVectorLegalizationRequest second;
    SZrVectorLegalizationResult result1;
    SZrVectorLegalizationResult result2;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_ExecIr_NumericPolicyInit(&policy);
    first = make_request(ZR_EXEC_IR_NUMERIC_VECTOR_ADD,
                         ZR_EXEC_IR_NUMERIC_ELEMENT_F32, 16u);
    first.policy = &policy;
    first.targetHash = UINT64_C(0x1111);
    second = first;
    second.targetHash = UINT64_C(0x2222);
    assert(ZrParser_ExecIr_LegalizeVector(&first, &result1, &diagnostic));
    assert(ZrParser_ExecIr_LegalizeVector(&second, &result2, &diagnostic));
    assert(result1.contractHash != 0u);
    assert(result1.contractHash != result2.contractHash);
}

int main(void) {
    test_strict_policy_defaults_and_hash();
    test_operation_semantic_row_is_explicit();
    test_policy_rejects_unknown_values_and_contradictions();
    test_permissions_intersect_without_fast_math_leakage();
    test_fma_permission_is_independent_of_reassociation_and_reduction();
    test_vector_legalization_and_scalar_fallback();
    test_ordered_reduction_never_changes_association();
    test_vector_shape_boundaries_and_operation_validation();
    test_policy_hash_and_result_hash_include_target_contract();
    return 0;
}
