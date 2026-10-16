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

    effective.requestedFastMath |= ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA;
    effective.effectiveFastMath |= ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA;
    request = make_request(ZR_EXEC_IR_NUMERIC_VECTOR_FMA,
                           ZR_EXEC_IR_NUMERIC_ELEMENT_F32, 4u);
    request.targetCapabilities = ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR |
                                 ZR_EXEC_IR_NUMERIC_TARGET_CAP_F32 |
                                 ZR_EXEC_IR_NUMERIC_TARGET_CAP_FMA;
    request.requiredFastMath = ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA;
    request.policy = &effective;
    assert(ZrParser_ExecIr_LegalizeVector(&request, &result, &diagnostic));
    assert(result.decision == ZR_EXEC_IR_NUMERIC_LEGALIZE_VECTOR);

    request.policy = ZR_NULL;
    assert(ZrParser_ExecIr_LegalizeVector(&request, &result, &diagnostic));
    assert(result.decision == ZR_EXEC_IR_NUMERIC_LEGALIZE_SCALAR_FALLBACK);
    assert(result.reason == ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_FAST_MATH_DENIED);
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

    request = make_request(ZR_EXEC_IR_NUMERIC_VECTOR_SHIFT_LEFT,
                           ZR_EXEC_IR_NUMERIC_ELEMENT_F32, 4u);
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

/**
 * @fn static void test_strict_policy_defaults_and_hash(void)
 *
 * ## Summary
 * 固定严格策略初始化的可见字段，并检查这一份策略可通过校验且重复摘要一致。
 *
 * ## Scenario and assertions
 * 核对schema、CHECKED溢出、STRICT_IEEE、STRICT_ORDERED、空requested/effective掩码和三项保留布尔；随后校验成功，断言首个hash非零及再次计算相等。
 *
 * ## Returns
 * 无返回值；正常返回表示上述启用断言完成。assert失败会终止当前进程，不提供可恢复的失败返回。
 *
 * ## Constraints and lifecycle
 * 策略与诊断均在本调用栈上独立存放；初始化及摘要不持有外部资源。仅依次读取已初始化记录，不与其他线程共享写入。 同一值的两次hash相同不证明跨平台序列化、不同值无碰撞或全部默认字段；没有执行数值运算。 用例依赖启用assert；失败直接终止进程，不执行Unity hook或长跳转。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:13
 * - tests/parser/test_ssa_numeric_vector_ir.c:18
 * - tests/parser/test_ssa_numeric_vector_ir.c:20
 * - tests/parser/test_ssa_numeric_vector_ir.c:29
 * - tests/parser/test_ssa_numeric_vector_ir.c:30
 * - tests/parser/test_ssa_numeric_vector_ir.c:32
 * - tests/parser/test_ssa_numeric_vector_ir.c:335
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:333
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:352
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:358
 */

/**
 * @fn static void test_operation_semantic_row_is_explicit(void)
 *
 * ## Summary
 * 检查数值语义记录的默认约束、MASK拒绝和两种整数操作的显式语义字段。
 *
 * ## Scenario and assertions
 * 默认记录的溢出、移位、除法、NaN、舍入、异常与有符号零字段逐项断言；MASK元素拒绝并给INVALID_ARGUMENT；ADD/I64检查CHECKED溢出与除法，SHIFT_LEFT/U32检查MASKED移位。
 *
 * ## Returns
 * 无返回值；正常返回表示上述启用断言完成。assert失败会终止当前进程，不提供可恢复的失败返回。
 *
 * ## Constraints and lifecycle
 * contract、policy和diagnostic分别位于本调用栈；每次API同步读写各自完整记录，不保存这些地址，无堆资源需要释放。 只验证所列记录字段与两种合法整数操作；不证明实际溢出、除零、移位结果或所有operation/element组合。 用例依赖启用assert；失败直接终止进程，不执行Unity hook或长跳转。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:35
 * - tests/parser/test_ssa_numeric_vector_ir.c:40
 * - tests/parser/test_ssa_numeric_vector_ir.c:42
 * - tests/parser/test_ssa_numeric_vector_ir.c:49
 * - tests/parser/test_ssa_numeric_vector_ir.c:52
 * - tests/parser/test_ssa_numeric_vector_ir.c:53
 * - tests/parser/test_ssa_numeric_vector_ir.c:55
 * - tests/parser/test_ssa_numeric_vector_ir.c:58
 * - tests/parser/test_ssa_numeric_vector_ir.c:62
 * - tests/parser/test_ssa_numeric_vector_ir.c:64
 * - tests/parser/test_ssa_numeric_vector_ir.c:68
 * - tests/parser/test_ssa_numeric_vector_ir.c:336
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:191
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:208
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:234
 */

/**
 * @fn static void test_policy_rejects_unknown_values_and_contradictions(void)
 *
 * ## Summary
 * 保留三种策略拒绝场景：未知fast-math位、非法溢出枚举和严格配置中的NO_SIGNED_ZERO有效位。
 *
 * ## Scenario and assertions
 * 分别重新初始化策略后设置bit31、overflowMode=99和NO_SIGNED_ZERO的requested/allowed/effective，逐次断言FALSE及INVALID_ARGUMENT。
 *
 * ## Returns
 * 无返回值；正常返回表示上述启用断言完成。assert失败会终止当前进程，不提供可恢复的失败返回。
 *
 * ## Constraints and lifecycle
 * 每个场景在前一场景结束后重置policy；diagnostic由校验入口同步清理和写入，输入输出使用独立栈记录。第三场景先命中非RELAXED配置对有效变换位的拒绝。 第三场景不能单独证明后续preserveSignedZero专门检查被到达；三个离散输入也不代表全部枚举、布尔或掩码边界。 用例依赖启用assert；失败直接终止进程，不执行Unity hook或长跳转。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:71
 * - tests/parser/test_ssa_numeric_vector_ir.c:75
 * - tests/parser/test_ssa_numeric_vector_ir.c:76
 * - tests/parser/test_ssa_numeric_vector_ir.c:78
 * - tests/parser/test_ssa_numeric_vector_ir.c:82
 * - tests/parser/test_ssa_numeric_vector_ir.c:83
 * - tests/parser/test_ssa_numeric_vector_ir.c:87
 * - tests/parser/test_ssa_numeric_vector_ir.c:91
 * - tests/parser/test_ssa_numeric_vector_ir.c:92
 * - tests/parser/test_ssa_numeric_vector_ir.c:337
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:91
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:102
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:137
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:161
 */

/**
 * @fn static void test_permissions_intersect_without_fast_math_leakage(void)
 *
 * ## Summary
 * 检查项目、包和目标的许可交集不会把被更窄作用域拒绝的变换带入effective记录。
 *
 * ## Scenario and assertions
 * 项目ALL、包REASSOCIATE|FMA、目标FMA及relaxed请求得到FMA；断言UNORDERED_REDUCTION和APPROX_RECIPROCAL缺席并校验effective。把包许可清空后再次求交，断言空effective及NaN/有符号零保留。
 *
 * ## Returns
 * 无返回值；正常返回表示上述启用断言完成。assert失败会终止当前进程，不提供可恢复的失败返回。
 *
 * ## Constraints and lifecycle
 * 四个输入与effective、diagnostic彼此独立；输出重用前由Intersect覆盖，所有记录为栈值，不保存输入地址，不把输出与输入别名。 只证明这些成功求交样本；不覆盖NULL、无效输入、最终校验失败后候选状态、任意alias或全部浮点许可组合。 用例依赖启用assert；失败直接终止进程，不执行Unity hook或长跳转。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:95
 * - tests/parser/test_ssa_numeric_vector_ir.c:104
 * - tests/parser/test_ssa_numeric_vector_ir.c:108
 * - tests/parser/test_ssa_numeric_vector_ir.c:117
 * - tests/parser/test_ssa_numeric_vector_ir.c:119
 * - tests/parser/test_ssa_numeric_vector_ir.c:124
 * - tests/parser/test_ssa_numeric_vector_ir.c:127
 * - tests/parser/test_ssa_numeric_vector_ir.c:128
 * - tests/parser/test_ssa_numeric_vector_ir.c:130
 * - tests/parser/test_ssa_numeric_vector_ir.c:338
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:397
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:415
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:424
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:445
 */

/**
 * @fn static void test_fma_permission_is_independent_of_reassociation_and_reduction(void)
 *
 * ## Summary
 * 验证只有FMA获准时，兼容Allows别名不会隐式授予重结合或无序归约。
 *
 * ## Scenario and assertions
 * 请求FMA|UNORDERED_REDUCTION|REASSOCIATE，项目ALL而包/目标仅FMA，relaxed模式求交得到FMA；三个Allows断言分别为TRUE、FALSE、FALSE。
 *
 * ## Returns
 * 无返回值；正常返回表示上述启用断言完成。assert失败会终止当前进程，不提供可恢复的失败返回。
 *
 * ## Constraints and lifecycle
 * 独立的栈策略与诊断仅在调用期间借用；NumericPolicy_Allows对象宏转发至NumericPolicyAllows，无回调注册或额外持有状态。 这是许可位查询，不是FMA精度或归约执行测试；未覆盖Allows的NULL、未知位或无效policy。 用例依赖启用assert；失败直接终止进程，不执行Unity hook或长跳转。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:135
 * - tests/parser/test_ssa_numeric_vector_ir.c:143
 * - tests/parser/test_ssa_numeric_vector_ir.c:148
 * - tests/parser/test_ssa_numeric_vector_ir.c:150
 * - tests/parser/test_ssa_numeric_vector_ir.c:155
 * - tests/parser/test_ssa_numeric_vector_ir.c:157
 * - tests/parser/test_ssa_numeric_vector_ir.c:158
 * - tests/parser/test_ssa_numeric_vector_ir.c:160
 * - tests/parser/test_ssa_numeric_vector_ir.c:162
 * - tests/parser/test_ssa_numeric_vector_ir.c:339
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:451
 */

/**
 * @fn static SZrVectorLegalizationRequest make_request( EZrNumericVectorOperation operation, EZrNumericElementKind elementKind, TZrUInt64 length)
 *
 * ## Summary
 * 为本文件合法化场景按值构造一个四lane、允许标量回退的请求基准，再由用例覆盖特殊字段。
 *
 * ## Scenario and assertions
 * 先清零整记录，再设置schema、传入的operation/element/length、16/4对齐、4lane、VECTOR|F32|MASK能力、允许fallback和保留源顺序；policy及requiredFastMath保持零。
 *
 * ## Parameters
 * @param[in] operation 测试要传给合法化入口的操作枚举，原值保存，允许负例使用不合法的元素组合。
 * @param[in] elementKind 测试元素种类，按值保存；默认能力仍只含F32，其他种类由调用用例另行设置目标能力。
 * @param[in] length 逻辑元素数，可为0；仅保存UInt64计数，不分配对应缓冲区。
 *
 * ## Returns
 * @return 返回独立的请求值；其中不持有堆对象，初始policy为NULL，合法性留给消费入口判断。
 *
 * ## Constraints and lifecycle
 * 不校验三个输入，因此可用于制造非法operation/element组合或后续零lane负例；返回值拥有纯记录字段，policy初始NULL，不保留局部request地址。调用者如补入policy指针，只能在后续同步合法化期间借用有效policy。 该helper不读取数组、不分配或认证目标能力与实际地址，也不保证返回请求一定通过LegalizeVector。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:166
 * - tests/parser/test_ssa_numeric_vector_ir.c:171
 * - tests/parser/test_ssa_numeric_vector_ir.c:172
 * - tests/parser/test_ssa_numeric_vector_ir.c:176
 * - tests/parser/test_ssa_numeric_vector_ir.c:179
 * - tests/parser/test_ssa_numeric_vector_ir.c:182
 * - tests/parser/test_ssa_numeric_vector_ir.c:184
 * - tests/parser/test_ssa_numeric_vector_ir.c:192
 * - tests/parser/test_ssa_numeric_vector_ir.c:240
 * - tests/parser/test_ssa_numeric_vector_ir.c:288
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:241
 */

/**
 * @fn static void test_vector_legalization_and_scalar_fallback(void)
 *
 * ## Summary
 * 检查LOAD/F32长度9在四lane目标上的规划字段，以及缺少向量能力和对齐不足的显式成功回退。
 *
 * ## Scenario and assertions
 * 先断言VECTOR、lanes4、iterations2、tail1及NaN/有符号零保留；移除VECTOR后断言SCALAR_FALLBACK、TARGET_UNSUPPORTED、lane1、scalarIterations9、源顺序和空诊断；恢复向量能力并把对齐改为1/16，断言ALIGNMENT回退。
 *
 * ## Returns
 * 无返回值；正常返回表示上述启用断言完成。assert失败会终止当前进程，不提供可恢复的失败返回。
 *
 * ## Constraints and lifecycle
 * request、result和diagnostic独立存放且逐次同步使用；结果是逻辑规划值，没有数组地址或待释放池。允许fallback的TRUE必须结合decision解释。 不验证实际LOAD、对齐地址、SIMD执行、禁止fallback的FALSE路径，也未断言回退的vectorIterations/tailLanes清零。 用例依赖启用assert；失败直接终止进程，不执行Unity hook或长跳转。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:187
 * - tests/parser/test_ssa_numeric_vector_ir.c:192
 * - tests/parser/test_ssa_numeric_vector_ir.c:194
 * - tests/parser/test_ssa_numeric_vector_ir.c:195
 * - tests/parser/test_ssa_numeric_vector_ir.c:202
 * - tests/parser/test_ssa_numeric_vector_ir.c:203
 * - tests/parser/test_ssa_numeric_vector_ir.c:204
 * - tests/parser/test_ssa_numeric_vector_ir.c:209
 * - tests/parser/test_ssa_numeric_vector_ir.c:213
 * - tests/parser/test_ssa_numeric_vector_ir.c:215
 * - tests/parser/test_ssa_numeric_vector_ir.c:217
 * - tests/parser/test_ssa_numeric_vector_ir.c:340
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:298
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:335
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:394
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:420
 */

/**
 * @fn static void test_ordered_reduction_never_changes_association(void)
 *
 * ## Summary
 * 检查归约保序回退、显式relaxed无序许可正例，以及FMA在策略指针撤销后的回退选择。
 *
 * ## Scenario and assertions
 * 严格有效策略配合保留源顺序的REDUCE_ADD/F64请求断言ORDER_REQUIRED回退；显式写入UNORDERED_REDUCTION、relaxed浮点/确定性且取消保序后断言VECTOR。再添加FMA许可与能力得到VECTOR，最后policy=NULL得到FAST_MATH_DENIED回退。
 *
 * ## Returns
 * 无返回值；正常返回表示上述启用断言完成。assert失败会终止当前进程，不提供可恢复的失败返回。
 *
 * ## Constraints and lifecycle
 * request.policy只同步借用本调用栈的effective；每次Legalize完成后才修改policy/request，result不保存该地址。各记录独立，无共享写入或堆资源。 只检验方案与保序标记，没有实际求和或关联顺序结果。无序正例同时设置许可位和relaxed模式，未隔离缺少UNORDERED_REDUCTION的组合，不能据此推定所有归约都会自动检查该位。 用例依赖启用assert；失败直接终止进程，不执行Unity hook或长跳转。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:220
 * - tests/parser/test_ssa_numeric_vector_ir.c:237
 * - tests/parser/test_ssa_numeric_vector_ir.c:240
 * - tests/parser/test_ssa_numeric_vector_ir.c:245
 * - tests/parser/test_ssa_numeric_vector_ir.c:247
 * - tests/parser/test_ssa_numeric_vector_ir.c:249
 * - tests/parser/test_ssa_numeric_vector_ir.c:252
 * - tests/parser/test_ssa_numeric_vector_ir.c:257
 * - tests/parser/test_ssa_numeric_vector_ir.c:261
 * - tests/parser/test_ssa_numeric_vector_ir.c:268
 * - tests/parser/test_ssa_numeric_vector_ir.c:270
 * - tests/parser/test_ssa_numeric_vector_ir.c:273
 * - tests/parser/test_ssa_numeric_vector_ir.c:274
 * - tests/parser/test_ssa_numeric_vector_ir.c:276
 * - tests/parser/test_ssa_numeric_vector_ir.c:341
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:317
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:330
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:341
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:345
 */

/**
 * @fn static void test_vector_shape_boundaries_and_operation_validation(void)
 *
 * ## Summary
 * 用八个离散长度核对四lane计数公式，并保留三种非法请求的拒绝场景。
 *
 * ## Scenario and assertions
 * MUL/F32长度0、1、3、4、5、7、8、9逐次断言合法化TRUE、vectorIterations=length/4、tailLanes=length%4、scalarIterations=length；随后FMA/I32、SHIFT_LEFT/F32和lane0均断言FALSE及INVALID_ARGUMENT。
 *
 * ## Returns
 * 无返回值；正常返回表示上述启用断言完成。assert失败会终止当前进程，不提供可恢复的失败返回。
 *
 * ## Constraints and lifecycle
 * lengths是函数内只读静态数据表，只有此用例遍历；每轮覆盖独立栈请求/结果/诊断，不持有资源。0长度是该记录规划的合法样本，不代表真实空缓冲区访问。 循环没有独立断言decision；八个长度不构成全部UInt64或lane范围覆盖。三种负例在请求校验阶段拒绝，不覆盖目标fallback、NULL或超过最大lane的边界。 用例依赖启用assert；失败直接终止进程，不执行Unity hook或长跳转。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:279
 * - tests/parser/test_ssa_numeric_vector_ir.c:280
 * - tests/parser/test_ssa_numeric_vector_ir.c:286
 * - tests/parser/test_ssa_numeric_vector_ir.c:288
 * - tests/parser/test_ssa_numeric_vector_ir.c:290
 * - tests/parser/test_ssa_numeric_vector_ir.c:291
 * - tests/parser/test_ssa_numeric_vector_ir.c:293
 * - tests/parser/test_ssa_numeric_vector_ir.c:296
 * - tests/parser/test_ssa_numeric_vector_ir.c:298
 * - tests/parser/test_ssa_numeric_vector_ir.c:301
 * - tests/parser/test_ssa_numeric_vector_ir.c:303
 * - tests/parser/test_ssa_numeric_vector_ir.c:308
 * - tests/parser/test_ssa_numeric_vector_ir.c:309
 * - tests/parser/test_ssa_numeric_vector_ir.c:342
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:257
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:420
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:424
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:425
 */

/**
 * @fn static void test_policy_hash_and_result_hash_include_target_contract(void)
 *
 * ## Summary
 * 检查其他输入相同而targetHash为两个指定值时，合法化结果的契约摘要可区分这两个目标身份。
 *
 * ## Scenario and assertions
 * 同一默认policy由first/second同步借用；请求长度16、ADD/F32，只把targetHash从0x1111改为0x2222。两次合法化成功后断言第一个contractHash非零且两个摘要不同。
 *
 * ## Returns
 * 无返回值；正常返回表示上述启用断言完成。assert失败会终止当前进程，不提供可恢复的失败返回。
 *
 * ## Constraints and lifecycle
 * first/second按值复制，policy地址保持有效直到两次调用结束；结果只保存逻辑摘要，不延长policy生命周期，无资源转移。 此用例没有直接调用NumericPolicyHash，名称不构成额外断言；只证明两个指定targetHash样本，不能推导所有输入无碰撞或目标能力真实性。 用例依赖启用assert；失败直接终止进程，不执行Unity hook或长跳转。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:313
 * - tests/parser/test_ssa_numeric_vector_ir.c:321
 * - tests/parser/test_ssa_numeric_vector_ir.c:322
 * - tests/parser/test_ssa_numeric_vector_ir.c:324
 * - tests/parser/test_ssa_numeric_vector_ir.c:325
 * - tests/parser/test_ssa_numeric_vector_ir.c:326
 * - tests/parser/test_ssa_numeric_vector_ir.c:327
 * - tests/parser/test_ssa_numeric_vector_ir.c:328
 * - tests/parser/test_ssa_numeric_vector_ir.c:329
 * - tests/parser/test_ssa_numeric_vector_ir.c:330
 * - tests/parser/test_ssa_numeric_vector_ir.c:331
 * - tests/parser/test_ssa_numeric_vector_ir.c:343
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:377
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:379
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:386
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 按固定顺序执行本文件九个普通C数值契约用例，并以进程结果交付整体成功。
 *
 * ## Scenario and assertions
 * 顺序调用九个static用例，不使用RUN_TEST、Unity计数、选择参数或异常恢复；全部正常完成后返回0。
 *
 * ## Returns
 * @return 返回0表示九个用例按顺序正常完成；assert失败时进程终止，不返回main调用方。
 *
 * ## Constraints and lifecycle
 * 由ssa_numeric_vector_ir CTest目标启动；构建必须保留assert表达式，其中多处包含必要的API调用。assert失败会终止进程，不跳转到后续用例或Unity tearDown；本文件测试数据均为栈记录或只读常量。 main的0只表示启用断言且全部用例正常完成的进程结果，不代表真实数值执行或未构造组合；没有命令行筛选接口。
 *
 * ## References
 * - tests/parser/test_ssa_numeric_vector_ir.c:334
 * - tests/parser/test_ssa_numeric_vector_ir.c:335
 * - tests/parser/test_ssa_numeric_vector_ir.c:339
 * - tests/parser/test_ssa_numeric_vector_ir.c:343
 * - tests/parser/test_ssa_numeric_vector_ir.c:344
 * - tests/cmake/ssa-tests.cmake:1287
 * - tests/cmake/ssa-tests.cmake:1296
 * - tests/CMakeLists.txt:7
 * - tests/CMakeLists.txt:12
 */
