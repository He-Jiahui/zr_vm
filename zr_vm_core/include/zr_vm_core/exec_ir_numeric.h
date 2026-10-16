#ifndef ZR_VM_CORE_EXEC_IR_NUMERIC_H
#define ZR_VM_CORE_EXEC_IR_NUMERIC_H

/*
 * Parser-owned numeric and vector contract.
 *
 * The contract deliberately contains only fixed-width scalar values.  It is
 * suitable for an ExecIR side table, an artifact key, or a diagnostic; no
 * AST, runtime object, host pointer, or variable-width instruction is stored
 * here.  The implementation lives in zr_vm_parser because policy comes from
 * language/package analysis, while the data model is shared with backends.
 */

#include "zr_vm_core/conf.h"
#include "zr_vm_core/execution_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_EXEC_IR_NUMERIC_API ZR_API

/* Fixed element kinds.  A mask is a logical lane predicate, not a numeric
 * storage type and is therefore kept separate from the scalar kinds. */
typedef enum EZrNumericElementKind {
    ZR_EXEC_IR_NUMERIC_ELEMENT_INVALID = 0,
    ZR_EXEC_IR_NUMERIC_ELEMENT_BOOL,
    ZR_EXEC_IR_NUMERIC_ELEMENT_I8,
    ZR_EXEC_IR_NUMERIC_ELEMENT_I16,
    ZR_EXEC_IR_NUMERIC_ELEMENT_I32,
    ZR_EXEC_IR_NUMERIC_ELEMENT_I64,
    ZR_EXEC_IR_NUMERIC_ELEMENT_U8,
    ZR_EXEC_IR_NUMERIC_ELEMENT_U16,
    ZR_EXEC_IR_NUMERIC_ELEMENT_U32,
    ZR_EXEC_IR_NUMERIC_ELEMENT_U64,
    ZR_EXEC_IR_NUMERIC_ELEMENT_F32,
    ZR_EXEC_IR_NUMERIC_ELEMENT_F64,
    ZR_EXEC_IR_NUMERIC_ELEMENT_MASK,
    ZR_EXEC_IR_NUMERIC_ELEMENT_COUNT
} EZrNumericElementKind;
typedef EZrNumericElementKind EZrExecIrNumericElementKind;

#define ZR_EXEC_IR_NUMERIC_ELEMENT_INT8 ZR_EXEC_IR_NUMERIC_ELEMENT_I8
#define ZR_EXEC_IR_NUMERIC_ELEMENT_INT16 ZR_EXEC_IR_NUMERIC_ELEMENT_I16
#define ZR_EXEC_IR_NUMERIC_ELEMENT_INT32 ZR_EXEC_IR_NUMERIC_ELEMENT_I32
#define ZR_EXEC_IR_NUMERIC_ELEMENT_INT64 ZR_EXEC_IR_NUMERIC_ELEMENT_I64
#define ZR_EXEC_IR_NUMERIC_ELEMENT_UINT8 ZR_EXEC_IR_NUMERIC_ELEMENT_U8
#define ZR_EXEC_IR_NUMERIC_ELEMENT_UINT16 ZR_EXEC_IR_NUMERIC_ELEMENT_U16
#define ZR_EXEC_IR_NUMERIC_ELEMENT_UINT32 ZR_EXEC_IR_NUMERIC_ELEMENT_U32
#define ZR_EXEC_IR_NUMERIC_ELEMENT_UINT64 ZR_EXEC_IR_NUMERIC_ELEMENT_U64
#define ZR_EXEC_IR_NUMERIC_ELEMENT_FLOAT32 ZR_EXEC_IR_NUMERIC_ELEMENT_F32
#define ZR_EXEC_IR_NUMERIC_ELEMENT_FLOAT64 ZR_EXEC_IR_NUMERIC_ELEMENT_F64
#define ZR_EXEC_IR_NUMERIC_ELEMENT_INTEGER32 ZR_EXEC_IR_NUMERIC_ELEMENT_I32
#define ZR_EXEC_IR_NUMERIC_ELEMENT_INTEGER64 ZR_EXEC_IR_NUMERIC_ELEMENT_I64

typedef enum EZrNumericOverflowMode {
    ZR_EXEC_IR_NUMERIC_OVERFLOW_INVALID = 0,
    ZR_EXEC_IR_NUMERIC_OVERFLOW_CHECKED,
    ZR_EXEC_IR_NUMERIC_OVERFLOW_WRAPPING,
    ZR_EXEC_IR_NUMERIC_OVERFLOW_SATURATING,
    ZR_EXEC_IR_NUMERIC_OVERFLOW_MODE_COUNT
} EZrNumericOverflowMode;
typedef EZrNumericOverflowMode EZrExecIrNumericOverflowMode;

typedef enum EZrNumericShiftMode {
    ZR_EXEC_IR_NUMERIC_SHIFT_INVALID = 0,
    /* Shift counts are masked by the destination bit width. */
    ZR_EXEC_IR_NUMERIC_SHIFT_MASKED,
    /* An out-of-range count is a checked numeric error. */
    ZR_EXEC_IR_NUMERIC_SHIFT_CHECKED,
    ZR_EXEC_IR_NUMERIC_SHIFT_MODE_COUNT
} EZrNumericShiftMode;

typedef enum EZrNumericFloatSemantics {
    ZR_EXEC_IR_NUMERIC_FLOAT_STRICT_IEEE = 0,
    ZR_EXEC_IR_NUMERIC_FLOAT_DETERMINISTIC,
    ZR_EXEC_IR_NUMERIC_FLOAT_RELAXED,
    ZR_EXEC_IR_NUMERIC_FLOAT_SEMANTICS_COUNT
} EZrNumericFloatSemantics;
typedef EZrNumericFloatSemantics EZrExecIrNumericFloatSemantics;

typedef enum EZrNumericDeterminism {
    /* Source-order operations and observable NaN/zero/exception rules. */
    ZR_EXEC_IR_NUMERIC_DETERMINISM_STRICT_ORDERED = 0,
    /* Bitwise deterministic profile; still does not make arbitrary libm
     * calls bitwise portable. */
    ZR_EXEC_IR_NUMERIC_DETERMINISM_BITWISE,
    /* Reassociation and unordered reductions may be selected if permitted. */
    ZR_EXEC_IR_NUMERIC_DETERMINISM_RELAXED,
    ZR_EXEC_IR_NUMERIC_DETERMINISM_COUNT
} EZrNumericDeterminism;
typedef EZrNumericDeterminism EZrExecIrNumericDeterminism;

typedef enum EZrNumericDivideMode {
    ZR_EXEC_IR_NUMERIC_DIVIDE_INVALID = 0,
    ZR_EXEC_IR_NUMERIC_DIVIDE_CHECKED,
    /* Floating divide follows IEEE infinities/NaN and signed-zero rules. */
    ZR_EXEC_IR_NUMERIC_DIVIDE_IEEE,
    ZR_EXEC_IR_NUMERIC_DIVIDE_MODE_COUNT
} EZrNumericDivideMode;

typedef enum EZrNumericNaNMode {
    ZR_EXEC_IR_NUMERIC_NAN_PRESERVE = 0,
    ZR_EXEC_IR_NUMERIC_NAN_CANONICALIZE,
    ZR_EXEC_IR_NUMERIC_NAN_RELAXED,
    ZR_EXEC_IR_NUMERIC_NAN_MODE_COUNT
} EZrNumericNaNMode;

typedef enum EZrNumericRoundingMode {
    ZR_EXEC_IR_NUMERIC_ROUND_CURRENT = 0,
    ZR_EXEC_IR_NUMERIC_ROUND_NEAREST_EVEN,
    ZR_EXEC_IR_NUMERIC_ROUND_TOWARD_ZERO,
    ZR_EXEC_IR_NUMERIC_ROUND_MODE_COUNT
} EZrNumericRoundingMode;

typedef enum EZrNumericExceptionMode {
    ZR_EXEC_IR_NUMERIC_EXCEPTIONS_PRESERVE = 0,
    ZR_EXEC_IR_NUMERIC_EXCEPTIONS_MASKED,
    ZR_EXEC_IR_NUMERIC_EXCEPTIONS_MODE_COUNT
} EZrNumericExceptionMode;

/* Each transformation has an independent permission bit.  In particular,
 * FMA/contract is not evidence that reassociation or unordered reduction is
 * legal. */
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE ((TZrUInt32)0u)
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_REASSOCIATE ((TZrUInt32)1u << 0u)
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA ((TZrUInt32)1u << 1u)
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL ((TZrUInt32)1u << 2u)
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO ((TZrUInt32)1u << 3u)
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION ((TZrUInt32)1u << 4u)
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_KNOWN_MASK ((TZrUInt32)0x1fu)
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_ALL \
    ZR_EXEC_IR_NUMERIC_FAST_MATH_KNOWN_MASK

/* Compatibility spellings used by backend and generated-code clients. */
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_CONTRACT ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_DIVISION \
    ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZEROS \
    ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO
#define ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCE \
    ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION

/* Target capabilities are also fixed scalar bits.  Element capability bits
 * prevent a backend from silently widening f32/f64 or an integer lane. */
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR ((TZrUInt32)1u << 0u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_F32 ((TZrUInt32)1u << 1u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_F64 ((TZrUInt32)1u << 2u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_INTEGER ((TZrUInt32)1u << 3u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_MASK ((TZrUInt32)1u << 4u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_FMA ((TZrUInt32)1u << 5u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_APPROX_RECIPROCAL ((TZrUInt32)1u << 6u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_UNALIGNED ((TZrUInt32)1u << 7u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_REDUCE ((TZrUInt32)1u << 8u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_I8 ((TZrUInt32)1u << 9u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_I16 ((TZrUInt32)1u << 10u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_I32 ((TZrUInt32)1u << 11u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_I64 ((TZrUInt32)1u << 12u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_U8 ((TZrUInt32)1u << 13u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_U16 ((TZrUInt32)1u << 14u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_U32 ((TZrUInt32)1u << 15u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_U64 ((TZrUInt32)1u << 16u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_KNOWN_MASK ((TZrUInt32)0x1ffffu)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_INT ZR_EXEC_IR_NUMERIC_TARGET_CAP_INTEGER
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_SCALAR ((TZrUInt32)0u)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR_F32 \
    (ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR | ZR_EXEC_IR_NUMERIC_TARGET_CAP_F32)
#define ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR_F64 \
    (ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR | ZR_EXEC_IR_NUMERIC_TARGET_CAP_F64)

typedef enum EZrNumericVectorOperation {
    ZR_EXEC_IR_NUMERIC_VECTOR_INVALID = 0,
    ZR_EXEC_IR_NUMERIC_VECTOR_LOAD,
    ZR_EXEC_IR_NUMERIC_VECTOR_STORE,
    ZR_EXEC_IR_NUMERIC_VECTOR_ADD,
    ZR_EXEC_IR_NUMERIC_VECTOR_SUB,
    ZR_EXEC_IR_NUMERIC_VECTOR_MUL,
    ZR_EXEC_IR_NUMERIC_VECTOR_DIV,
    ZR_EXEC_IR_NUMERIC_VECTOR_FMA,
    ZR_EXEC_IR_NUMERIC_VECTOR_NEG,
    ZR_EXEC_IR_NUMERIC_VECTOR_COMPARE,
    ZR_EXEC_IR_NUMERIC_VECTOR_SELECT,
    ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_ADD,
    ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MIN,
    ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MAX,
    ZR_EXEC_IR_NUMERIC_VECTOR_SHIFT_LEFT,
    ZR_EXEC_IR_NUMERIC_VECTOR_SHIFT_RIGHT,
    ZR_EXEC_IR_NUMERIC_VECTOR_MOD,
    ZR_EXEC_IR_NUMERIC_VECTOR_OPERATION_COUNT
} EZrNumericVectorOperation;
typedef EZrNumericVectorOperation EZrExecIrNumericVectorOperation;

#define ZR_EXEC_IR_NUMERIC_VECTOR_REDUCTION_ADD \
    ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_ADD
#define ZR_EXEC_IR_NUMERIC_VECTOR_REDUCTION_MIN \
    ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MIN
#define ZR_EXEC_IR_NUMERIC_VECTOR_REDUCTION_MAX \
    ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MAX
#define ZR_EXEC_IR_NUMERIC_VECTOR_ARITH_ADD ZR_EXEC_IR_NUMERIC_VECTOR_ADD
#define ZR_EXEC_IR_NUMERIC_VECTOR_ARITH_SUB ZR_EXEC_IR_NUMERIC_VECTOR_SUB
#define ZR_EXEC_IR_NUMERIC_VECTOR_ARITH_MUL ZR_EXEC_IR_NUMERIC_VECTOR_MUL
#define ZR_EXEC_IR_NUMERIC_VECTOR_ARITH_DIV ZR_EXEC_IR_NUMERIC_VECTOR_DIV

typedef enum EZrNumericLegalizationDecision {
    ZR_EXEC_IR_NUMERIC_LEGALIZE_NONE = 0,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_VECTOR,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_SCALAR_FALLBACK,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_DECISION_COUNT
} EZrNumericLegalizationDecision;
typedef EZrNumericLegalizationDecision EZrExecIrNumericLegalizationDecision;

typedef enum EZrNumericLegalizationReason {
    ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_NONE = 0,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_TARGET_UNSUPPORTED,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ALIGNMENT,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_LANE_UNSUPPORTED,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_OPERATION_UNSUPPORTED,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_FAST_MATH_DENIED,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ORDER_REQUIRED,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_EMPTY_INPUT,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_INVALID,
    ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_COUNT
} EZrNumericLegalizationReason;
typedef EZrNumericLegalizationReason EZrExecIrNumericLegalizationReason;

/* A compact operation-level semantic row.  This is an oracle contract, not
 * an implementation of arithmetic; backends must implement the row without
 * relying on host signed-overflow or global compiler fast-math switches. */
typedef struct SZrNumericSemanticContract {
    TZrUInt32 schemaVersion;
    EZrNumericElementKind elementKind;
    EZrNumericOverflowMode overflowMode;
    EZrNumericShiftMode shiftMode;
    EZrNumericDivideMode divideMode;
    EZrNumericNaNMode nanMode;
    EZrNumericRoundingMode roundingMode;
    EZrNumericExceptionMode exceptionMode;
    TZrBool preserveSignedZero;
} SZrNumericSemanticContract;

typedef struct SZrNumericPolicy {
    TZrUInt32 schemaVersion;
    EZrNumericOverflowMode overflowMode;
    EZrNumericShiftMode shiftMode;
    EZrNumericFloatSemantics floatSemantics;
    EZrNumericDeterminism determinism;
    EZrNumericDivideMode divideMode;
    EZrNumericNaNMode nanMode;
    EZrNumericRoundingMode roundingMode;
    EZrNumericExceptionMode exceptionMode;
    TZrUInt32 requestedFastMath;
    TZrUInt32 allowedFastMath;
    TZrUInt32 effectiveFastMath;
    TZrUInt32 deniedFastMath;
    TZrUInt32 targetCapabilities;
    TZrBool preserveNaN;
    TZrBool preserveSignedZero;
    TZrBool preserveExceptions;
    TZrBool reserved0;
    TZrUInt64 projectHash;
    TZrUInt64 packageHash;
    TZrUInt64 targetHash;
} SZrNumericPolicy;
typedef SZrNumericPolicy SZrExecIrNumericPolicy;

ZR_EXEC_IR_NUMERIC_API TZrBool ZrParser_ExecIr_NumericSemanticsFor(
        EZrNumericVectorOperation operation,
        EZrNumericElementKind elementKind,
        const SZrNumericPolicy *policy,
        SZrNumericSemanticContract *contract,
        SZrExecIrDiagnostic *diagnostic);

typedef struct SZrVectorLegalizationRequest {
    TZrUInt32 schemaVersion;
    EZrNumericVectorOperation operation;
    EZrNumericElementKind elementKind;
    TZrUInt64 length;
    TZrUInt32 targetVectorLanes;
    TZrUInt32 targetCapabilities;
    TZrUInt32 sourceAlignment;
    TZrUInt32 requiredAlignment;
    TZrUInt32 requiredFastMath;
    TZrUInt64 targetHash;
    TZrUInt64 targetSignatureHash;
    TZrUInt64 targetLayoutHash;
    TZrUInt32 functionToken;
    TZrUInt32 blockId;
    TZrUInt32 instructionId;
    TZrUInt32 sourceId;
    const SZrNumericPolicy *policy;
    TZrBool allowScalarFallback;
    TZrBool preserveSourceOrder;
    TZrBool reserved0;
    TZrBool reserved1;
} SZrVectorLegalizationRequest;
typedef SZrVectorLegalizationRequest SZrExecIrVectorLegalizationRequest;

typedef struct SZrVectorLegalizationResult {
    TZrUInt32 schemaVersion;
    EZrNumericLegalizationDecision decision;
    EZrNumericLegalizationReason reason;
    EZrNumericVectorOperation operation;
    EZrNumericElementKind elementKind;
    union {
        TZrUInt32 chosenLanes;
        TZrUInt32 vectorLanes;
        TZrUInt32 vectorWidth;
    };
    TZrUInt64 vectorIterations;
    TZrUInt64 scalarIterations;
    TZrUInt32 tailLanes;
    TZrUInt32 appliedFastMath;
    TZrUInt64 contractHash;
    TZrBool preservesNaN;
    TZrBool preservesSignedZero;
    TZrBool preservesExceptions;
    TZrBool preservesSourceOrder;
    TZrBool usesScalarFallback;
    TZrUInt32 functionToken;
    TZrUInt32 blockId;
    TZrUInt32 instructionId;
    TZrUInt32 sourceId;
    TZrBool reserved0;
    TZrBool reserved1;
    TZrBool reserved2;
} SZrVectorLegalizationResult;
typedef SZrVectorLegalizationResult SZrExecIrVectorLegalizationResult;

ZR_EXEC_IR_NUMERIC_API void ZrParser_ExecIr_NumericSemanticContractInit(
        SZrNumericSemanticContract *contract);
ZR_EXEC_IR_NUMERIC_API TZrBool ZrParser_ExecIr_ValidateNumericSemanticContract(
        const SZrNumericSemanticContract *contract,
        SZrExecIrDiagnostic *diagnostic);
ZR_EXEC_IR_NUMERIC_API void ZrParser_ExecIr_NumericPolicyInit(
        SZrNumericPolicy *policy);
ZR_EXEC_IR_NUMERIC_API TZrBool ZrParser_ExecIr_ValidateNumericPolicy(
        const SZrNumericPolicy *policy, SZrExecIrDiagnostic *diagnostic);
ZR_EXEC_IR_NUMERIC_API TZrUInt64 ZrParser_ExecIr_NumericPolicyHash(
        const SZrNumericPolicy *policy);
ZR_EXEC_IR_NUMERIC_API TZrBool ZrParser_ExecIr_NumericPolicyIntersect(
        const SZrNumericPolicy *requested,
        const SZrNumericPolicy *project,
        const SZrNumericPolicy *package,
        const SZrNumericPolicy *target,
        SZrNumericPolicy *effective,
        SZrExecIrDiagnostic *diagnostic);
ZR_EXEC_IR_NUMERIC_API TZrBool ZrParser_ExecIr_NumericPolicyAllows(
        const SZrNumericPolicy *policy, TZrUInt32 fastMathFlags);

/* Alternate names retain compatibility with the plan's shorter API sketch. */
#define ZrParser_ExecIr_ValidateNumeric ZrParser_ExecIr_ValidateNumericPolicy
#define ZrParser_ExecIr_NumericPolicy_Effective \
    ZrParser_ExecIr_NumericPolicyIntersect
#define ZrParser_ExecIr_NumericPolicy_Intersect \
    ZrParser_ExecIr_NumericPolicyIntersect
#define ZrParser_ExecIr_NumericPolicy_Hash ZrParser_ExecIr_NumericPolicyHash
#define ZrParser_ExecIr_NumericPolicy_Allows ZrParser_ExecIr_NumericPolicyAllows

ZR_EXEC_IR_NUMERIC_API void ZrParser_ExecIr_VectorLegalizationRequestInit(
        SZrVectorLegalizationRequest *request);
ZR_EXEC_IR_NUMERIC_API void ZrParser_ExecIr_VectorLegalizationResultInit(
        SZrVectorLegalizationResult *result);
ZR_EXEC_IR_NUMERIC_API TZrBool ZrParser_ExecIr_LegalizeVector(
        const SZrVectorLegalizationRequest *request,
        SZrVectorLegalizationResult *result,
        SZrExecIrDiagnostic *diagnostic);

#define ZrParser_ExecIr_VectorLegalize ZrParser_ExecIr_LegalizeVector
#define ZrParser_ExecIr_Legalize_Vector ZrParser_ExecIr_LegalizeVector

ZR_EXEC_IR_NUMERIC_API const TZrChar *ZrParser_ExecIr_NumericElementName(
        EZrNumericElementKind kind);
ZR_EXEC_IR_NUMERIC_API const TZrChar *ZrParser_ExecIr_NumericLegalizationReasonName(
        EZrNumericLegalizationReason reason);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_EXEC_IR_NUMERIC_H */

/**
 * @def ZR_EXEC_IR_NUMERIC_SCHEMA_VERSION
 *
 * ## Summary
 * 标记本数值语义/策略/合法化记录使用的模型版本，供初始化与消费前版本校验共用。
 *
 * ## Value and use
 * ((TZrUInt32)1u)
 *
 * ## Constraints and lifecycle
 * 值是 UInt32 的 1，不是 sizeof(struct)、ABI 位宽或硬件能力。各 Init 写入此值；策略/请求的版本拒绝路径可报告 VERSION_MISMATCH，SemanticContract 校验将版本错误归 INVALID_ARGUMENT。更改模型时需联动生产/消费双方，不能仅靠原始内存布局兼容。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:21
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:84
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:197
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:213
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:338
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:216
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:250
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:231
 * - tests/parser/test_ssa_numeric_vector_ir.c:20
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_API
 *
 * ## Summary
 * 使此 Core 公共数据模型中的 Parser 函数声明沿用首方平台可见性宏。
 *
 * ## Value and use
 * ZR_API
 *
 * ## Constraints and lifecycle
 * 该对象宏只展开为 ZR_API，不改变函数签名、调用次数或线程安全。在 ZR_PLATFORM_WIN 与 ZR_LIBRARY_TYPE_SHARED 同时为真时继承 dllexport，否则为 extern；实际实现来自 Parser 两个 C 文件。包含 Core 头不等于只链接 Core 就能得到这些 Parser 定义，调用方须链接拥有实现的目标。C++ 消费者由本头 extern C 条件区保持 C linkage；不据此承诺所有平台原生 ABI 相同。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:22
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:17
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:268
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:331
 * - zr_vm_common/include/zr_vm_common/zr_api_conf.h:14
 * - zr_vm_common/include/zr_vm_common/zr_api_conf.h:15
 * - zr_vm_common/include/zr_vm_common/zr_api_conf.h:19
 * - zr_vm_parser/CMakeLists.txt:4
 * - tests/cmake/ssa-tests.cmake:1289
 */

/**
 * @enum EZrNumericElementKind
 *
 * ## Summary
 * 标记数值语义或向量计划的元素域，使 I8/I16/I32/I64、无符号整数、F32/F64 与逻辑谓词明确区分。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_ELEMENT_INVALID = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_BOOL = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_I8 = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_I16 = 3；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_I32 = 4；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_I64 = 5；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_U8 = 6；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_U16 = 7；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_U32 = 8；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_U64 = 9；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_F32 = 10；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_F64 = 11；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_MASK = 12；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ELEMENT_COUNT = 13；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * INVALID 是无有效元素的哨兵；BOOL 为逻辑布尔元素，MASK 为 lane 谓词，不能把 MASK 当标量存储类型。NumericSemanticContract 只接受 BOOL..F64，向量请求按操作额外筛选 BOOL/MASK 和整数/浮点组合。这是 C 枚举标签，不保证 sizeof(enum)、字节序、padding 或跨 ABI 的二进制布局；持久化/FFI 应按已知逻辑数值编码。COUNT 是范围上界，不能作为有效模式。此头不执行数值运算或改变宿主浮点环境。 I/U 后缀表示目标语义位宽，并非 enum 自身的存储宽度；F32/F64 标签不证明宿主 float/double 或后端执行路径符合 IEEE。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:51
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:252
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:150
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:463
 * - tests/parser/test_ssa_numeric_vector_ir.c:52
 * - tests/parser/test_ssa_numeric_vector_ir.c:58
 * - tests/parser/test_ssa_numeric_vector_ir.c:64
 */

/**
 * @typedef EZrExecIrNumericElementKind
 *
 * ## Summary
 * 为 EZrNumericElementKind 提供 ExecIr 命名的兼容类型拼写，供公开头与仓外调用方使用。
 *
 * ## Representation
 * 本声明与 EZrNumericElementKind 是同一 C 类型，未增加成员、存储或 ABI 保证。
 *
 * ## Constraints and lifecycle
 * 这是同一 C 类型的 typedef 别名，不产生独立 enum/struct、存储、位宽或所有权；有效值、初始化/验证约束及借用生命周期完全沿用 EZrNumericElementKind。不能因别名包含 ExecIr 就推断可对 struct 原始字节进行跨 ABI 序列化。当前首方文本检索未发现别名在本头之外的直接使用，仓外公共消费者未知，不据此称为死代码。标记数值语义或向量计划的元素域，使 I8/I16/I32/I64、无符号整数、F32/F64 与逻辑谓词明确区分。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:42
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:51
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:252
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_INT8
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_I8 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_I8
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_I8 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:44
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_INT16
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_I16 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_I16
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_I16 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:45
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_INT32
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_I32 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_I32
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_I32 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:46
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_INT64
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_I64 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_I64
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_I64 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:47
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_UINT8
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_U8 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_U8
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_U8 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:48
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_UINT16
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_U16 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_U16
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_U16 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:49
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_UINT32
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_U32 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_U32
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_U32 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:50
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_UINT64
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_U64 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_U64
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_U64 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:51
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_FLOAT32
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_F32 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_F32
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_F32 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:52
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_FLOAT64
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_F64 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_F64
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_F64 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:53
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_INTEGER32
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_I32 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_I32
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_I32 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:54
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_ELEMENT_INTEGER64
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_ELEMENT_I64 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_ELEMENT_I64
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_ELEMENT_I64 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:55
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:26
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 */

/**
 * @enum EZrNumericOverflowMode
 *
 * ## Summary
 * 为整数语义记录选择检查、回绕或饱和的溢出处理意图，防止后端默用宿主带符号溢出行为。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_OVERFLOW_INVALID = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_OVERFLOW_CHECKED = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_OVERFLOW_WRAPPING = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_OVERFLOW_SATURATING = 3；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_OVERFLOW_MODE_COUNT = 4；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * INVALID/COUNT 不可用，默认 CHECKED；WRAPPING 和 SATURATING 也是策略允许的有效标签。策略及语义记录校验只检查模式范围，NumericSemanticsFor 复制该字段，不执行边界运算。后端须用显式检查或定义良好的表示算法实现所选模式，不能据 WRAPPING 标签在 C 中触发带符号溢出。这是 C 枚举标签，不保证 sizeof(enum)、字节序、padding 或跨 ABI 的二进制布局；持久化/FFI 应按已知逻辑数值编码。COUNT 是范围上界，不能作为有效模式。此头不执行数值运算或改变宿主浮点环境。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:57
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:91
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:199
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:215
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:312
 * - tests/parser/test_ssa_numeric_vector_ir.c:21
 * - tests/parser/test_ssa_numeric_vector_ir.c:62
 * - tests/parser/test_ssa_numeric_vector_ir.c:82
 */

/**
 * @typedef EZrExecIrNumericOverflowMode
 *
 * ## Summary
 * 为 EZrNumericOverflowMode 提供 ExecIr 命名的兼容类型拼写，供公开头与仓外调用方使用。
 *
 * ## Representation
 * 本声明与 EZrNumericOverflowMode 是同一 C 类型，未增加成员、存储或 ABI 保证。
 *
 * ## Constraints and lifecycle
 * 这是同一 C 类型的 typedef 别名，不产生独立 enum/struct、存储、位宽或所有权；有效值、初始化/验证约束及借用生命周期完全沿用 EZrNumericOverflowMode。不能因别名包含 ExecIr 就推断可对 struct 原始字节进行跨 ABI 序列化。当前首方文本检索未发现别名在本头之外的直接使用，仓外公共消费者未知，不据此称为死代码。为整数语义记录选择检查、回绕或饱和的溢出处理意图，防止后端默用宿主带符号溢出行为。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:64
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:57
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:91
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:199
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:215
 */

/**
 * @enum EZrNumericShiftMode
 *
 * ## Summary
 * 声明移位计数采用目的位宽掩码规则，还是把越界计数交给检查错误路径。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_SHIFT_INVALID = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_SHIFT_MASKED = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_SHIFT_CHECKED = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_SHIFT_MODE_COUNT = 3；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * INVALID/COUNT 不可用；MASKED 是初始化默认，CHECKED 表示越界计数须被检查。NumericSemanticsFor 复制模式并仅允许整数种类使用 SHIFT_LEFT/RIGHT。本头不执行移位，不能把标签当作允许负数或大于等于宿主类型宽度的 C 移位；具体位表示和计数处理由执行后端实现。这是 C 枚举标签，不保证 sizeof(enum)、字节序、padding 或跨 ABI 的二进制布局；持久化/FFI 应按已知逻辑数值编码。COUNT 是范围上界，不能作为有效模式。此头不执行数值运算或改变宿主浮点环境。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:66
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:68
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:93
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:200
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:267
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:313
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:154
 * - tests/parser/test_ssa_numeric_vector_ir.c:43
 * - tests/parser/test_ssa_numeric_vector_ir.c:65
 * - tests/parser/test_ssa_numeric_vector_ir.c:68
 */

/**
 * @enum EZrNumericFloatSemantics
 *
 * ## Summary
 * 选择严格 IEEE、确定性或 relaxed 浮点配置，参与 fast-math 的生效规则检查。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_FLOAT_STRICT_IEEE = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_FLOAT_DETERMINISTIC = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_FLOAT_RELAXED = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_FLOAT_SEMANTICS_COUNT = 3；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * STRICT_IEEE 的零值是有效默认，要求保留 NaN/零符号/异常；当前验证对所有非 RELAXED 浮点配置拒绝非零有效 fast-math。DETERMINISTIC 标签不等于任意 libm 调用跨平台逐位相同。RELAXED 也必须通过每个独立许可位和其他模式检查，不能代替目标能力。这是 C 枚举标签，不保证 sizeof(enum)、字节序、padding 或跨 ABI 的二进制布局；持久化/FFI 应按已知逻辑数值编码。COUNT 是范围上界，不能作为有效模式。此头不执行数值运算或改变宿主浮点环境。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:75
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:137
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:169
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:341
 * - tests/parser/test_ssa_numeric_vector_ir.c:22
 * - tests/parser/test_ssa_numeric_vector_ir.c:91
 * - tests/parser/test_ssa_numeric_vector_ir.c:113
 * - tests/parser/test_ssa_numeric_vector_ir.c:153
 */

/**
 * @typedef EZrExecIrNumericFloatSemantics
 *
 * ## Summary
 * 为 EZrNumericFloatSemantics 提供 ExecIr 命名的兼容类型拼写，供公开头与仓外调用方使用。
 *
 * ## Representation
 * 本声明与 EZrNumericFloatSemantics 是同一 C 类型，未增加成员、存储或 ABI 保证。
 *
 * ## Constraints and lifecycle
 * 这是同一 C 类型的 typedef 别名，不产生独立 enum/struct、存储、位宽或所有权；有效值、初始化/验证约束及借用生命周期完全沿用 EZrNumericFloatSemantics。不能因别名包含 ExecIr 就推断可对 struct 原始字节进行跨 ABI 序列化。当前首方文本检索未发现别名在本头之外的直接使用，仓外公共消费者未知，不据此称为死代码。选择严格 IEEE、确定性或 relaxed 浮点配置，参与 fast-math 的生效规则检查。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:81
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:75
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:137
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:169
 */

/**
 * @enum EZrNumericDeterminism
 *
 * ## Summary
 * 在策略中区分严格源顺序、逐位确定性意图和可经许可放宽的重排配置。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_DETERMINISM_STRICT_ORDERED = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_DETERMINISM_BITWISE = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_DETERMINISM_RELAXED = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_DETERMINISM_COUNT = 3；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * STRICT_ORDERED 为有效零值默认；非 RELAXED 配置均禁止有效 REASSOCIATE/FMA/UNORDERED_REDUCTION，严格顺序还使归约选择标量回退。BITWISE 不承诺任意外部数学函数的跨平台结果；RELAXED 不自动授权变换，仍需有效许可。这是 C 枚举标签，不保证 sizeof(enum)、字节序、padding 或跨 ABI 的二进制布局；持久化/FFI 应按已知逻辑数值编码。COUNT 是范围上界，不能作为有效模式。此头不执行数值运算或改变宿主浮点环境。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:83
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:86
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:96
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:150
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:177
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:342
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:341
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:345
 * - tests/parser/test_ssa_numeric_vector_ir.c:23
 * - tests/parser/test_ssa_numeric_vector_ir.c:247
 * - tests/parser/test_ssa_numeric_vector_ir.c:254
 */

/**
 * @typedef EZrExecIrNumericDeterminism
 *
 * ## Summary
 * 为 EZrNumericDeterminism 提供 ExecIr 命名的兼容类型拼写，供公开头与仓外调用方使用。
 *
 * ## Representation
 * 本声明与 EZrNumericDeterminism 是同一 C 类型，未增加成员、存储或 ABI 保证。
 *
 * ## Constraints and lifecycle
 * 这是同一 C 类型的 typedef 别名，不产生独立 enum/struct、存储、位宽或所有权；有效值、初始化/验证约束及借用生命周期完全沿用 EZrNumericDeterminism。不能因别名包含 ExecIr 就推断可对 struct 原始字节进行跨 ABI 序列化。当前首方文本检索未发现别名在本头之外的直接使用，仓外公共消费者未知，不据此称为死代码。在策略中区分严格源顺序、逐位确定性意图和可经许可放宽的重排配置。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:93
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:83
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:86
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:96
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:150
 */

/**
 * @enum EZrNumericDivideMode
 *
 * ## Summary
 * 标记除法应遵循检查路径还是浮点 IEEE 无穷、NaN 和有符号零规则。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_DIVIDE_INVALID = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_DIVIDE_CHECKED = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_DIVIDE_IEEE = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_DIVIDE_MODE_COUNT = 3；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * INVALID/COUNT 不可用；默认策略为 IEEE。NumericSemanticsFor 对浮点固定选择 IEEE，对其余接受的标量固定选择 CHECKED，并非原样传播 policy.divideMode。该标签不在这里实现除零或有符号最小值除以 -1 的处理；执行后端须按语义检查，避免直接触发宿主 UB。这是 C 枚举标签，不保证 sizeof(enum)、字节序、padding 或跨 ABI 的二进制布局；持久化/FFI 应按已知逻辑数值编码。COUNT 是范围上界，不能作为有效模式。此头不执行数值运算或改变宿主浮点环境。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:95
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:98
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:97
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:201
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:314
 * - tests/parser/test_ssa_numeric_vector_ir.c:44
 * - tests/parser/test_ssa_numeric_vector_ir.c:63
 */

/**
 * @enum EZrNumericNaNMode
 *
 * ## Summary
 * 给浮点语义标注保留、规范化或放宽 NaN 表示的模式。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_NAN_PRESERVE = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_NAN_CANONICALIZE = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_NAN_RELAXED = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_NAN_MODE_COUNT = 3；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * PRESERVE 为有效零值默认，另有 CANONICALIZE、RELAXED。当前验证检查枚举范围，不能由此推断所有 NaN 模式与 policy.preserveNaN 的额外一致性均已检查。NumericSemanticsFor 对浮点复制该字段，对非浮点固定 PRESERVE；没有读取或重写实际浮点 payload。这是 C 枚举标签，不保证 sizeof(enum)、字节序、padding 或跨 ABI 的二进制布局；持久化/FFI 应按已知逻辑数值编码。COUNT 是范围上界，不能作为有效模式。此头不执行数值运算或改变宿主浮点环境。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:103
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:99
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:202
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:221
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:316
 * - tests/parser/test_ssa_numeric_vector_ir.c:45
 */

/**
 * @enum EZrNumericRoundingMode
 *
 * ## Summary
 * 声明语义记录选择当前舍入环境、nearest-even 或向零舍入，供后端实现时解释。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_ROUND_CURRENT = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ROUND_NEAREST_EVEN = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ROUND_TOWARD_ZERO = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_ROUND_MODE_COUNT = 3；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * CURRENT 的零值有效，但 Init 默认选择 NEAREST_EVEN；NumericSemanticsFor 对非浮点覆盖为 TOWARD_ZERO。此接口只保存标签，不查询或设置 fenv，也不执行表示转换。不能由标签假设宿主当前舍入模式或跨平台转换行为。这是 C 枚举标签，不保证 sizeof(enum)、字节序、padding 或跨 ABI 的二进制布局；持久化/FFI 应按已知逻辑数值编码。COUNT 是范围上界，不能作为有效模式。此头不执行数值运算或改变宿主浮点环境。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:110
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:100
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:203
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:318
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:322
 * - tests/parser/test_ssa_numeric_vector_ir.c:46
 */

/**
 * @enum EZrNumericExceptionMode
 *
 * ## Summary
 * 标注数值异常是否应被保留观察或允许屏蔽，与策略中的异常保留布尔值分别记录。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_EXCEPTIONS_PRESERVE = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_EXCEPTIONS_MASKED = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_EXCEPTIONS_MODE_COUNT = 2；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * PRESERVE 为有效零值默认，MASKED 为另一有效标签。NumericSemanticsFor 对浮点传播模式，对非浮点固定 PRESERVE；此头不会修改硬件异常屏蔽字或证明后端已经保留异常。policy.preserveExceptions 可因有效变换被求交步骤清除，不能仅凭一个枚举值替代完整策略。这是 C 枚举标签，不保证 sizeof(enum)、字节序、padding 或跨 ABI 的二进制布局；持久化/FFI 应按已知逻辑数值编码。COUNT 是范围上界，不能作为有效模式。此头不执行数值运算或改变宿主浮点环境。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:117
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:101
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:204
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:319
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:323
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:439
 * - tests/parser/test_ssa_numeric_vector_ir.c:47
 * - tests/parser/test_ssa_numeric_vector_ir.c:28
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_NONE
 *
 * ## Summary
 * 以空集合表示没有 fast-math 许可。
 *
 * ## Value and use
 * ((TZrUInt32)0u)
 *
 * ## Constraints and lifecycle
 * 零值用于默认请求/生效/拒绝掩码；但求交函数对 transient requested.allowedFastMath=0 有“未额外指定”语义，而 project/package/target.allowedFastMath=0 会关闭相应交集。不能把所有上下文中的 0 一概解释为同一层授权。 固定位移均以 UInt32 的 1 为左操作数，位号在 0..4 内；本宏无动态参数、分配、借用或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:126
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:118
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:337
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:416
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:421
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:402
 * - tests/parser/test_ssa_numeric_vector_ir.c:24
 * - tests/parser/test_ssa_numeric_vector_ir.c:127
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_REASSOCIATE
 *
 * ## Summary
 * 表示允许改变结合顺序的独立许可位。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 0u)
 *
 * ## Constraints and lifecycle
 * 位于 UInt32 bit 0；不由 FMA 或其他位隐含授予。完整有效规则要求 relaxed 浮点和 relaxed 确定性；求交后该位可清除 preserveExceptions。这里只声明许可，不执行表达式重排。 固定位移均以 UInt32 的 1 为左操作数，位号在 0..4 内；本宏无动态参数、分配、借用或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:127
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:137
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:150
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:439
 * - tests/parser/test_ssa_numeric_vector_ir.c:109
 * - tests/parser/test_ssa_numeric_vector_ir.c:160
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA
 *
 * ## Summary
 * 表示允许融合乘加的独立许可位，与重排和无序归约区分。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 1u)
 *
 * ## Constraints and lifecycle
 * 位于 UInt32 bit 1；生效需通过 relaxed 浮点/确定性校验。NumericSemanticsFor 和 LegalizeVector 的 FMA 路径都显式查询本位，合法化还要求目标 FMA 能力。不会因此自动获得其他许可，也不在此调用宿主 fma。 固定位移均以 UInt32 的 1 为左操作数，位号在 0..4 内；本宏无动态参数、分配、借用或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:128
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:137
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:150
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:300
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:313
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:317
 * - tests/parser/test_ssa_numeric_vector_ir.c:155
 * - tests/parser/test_ssa_numeric_vector_ir.c:158
 * - tests/parser/test_ssa_numeric_vector_ir.c:160
 * - tests/parser/test_ssa_numeric_vector_ir.c:162
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL
 *
 * ## Summary
 * 表示可采用近似倒数/近似除法的独立许可位。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 2u)
 *
 * ## Constraints and lifecycle
 * 位于 UInt32 bit 2；非 relaxed 浮点配置拒绝该有效位，求交后可清除 preserveExceptions。对 DIV 且 requiredFastMath 含本位的请求，合法化另要求目标 APPROX_RECIPROCAL 能力。标签不规定误差界或实际生成的指令。 固定位移均以 UInt32 的 1 为左操作数，位号在 0..4 内；本宏无动态参数、分配、借用或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:129
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:137
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:439
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:323
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:330
 * - tests/parser/test_ssa_numeric_vector_ir.c:123
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO
 *
 * ## Summary
 * 表示允许放弃有符号零可观察性的独立许可位。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 3u)
 *
 * ## Constraints and lifecycle
 * 位于 UInt32 bit 3；不与 preserveSignedZero=TRUE 的有效策略相容，求交接受后显式清除此保留标志。该位不是 C 编译全局 fast-math 开关，也不会在此修改任何实际数值。 固定位移均以 UInt32 的 1 为左操作数，位号在 0..4 内；本宏无动态参数、分配、借用或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:130
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:137
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:161
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:435
 * - tests/parser/test_ssa_numeric_vector_ir.c:87
 * - tests/parser/test_ssa_numeric_vector_ir.c:91
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION
 *
 * ## Summary
 * 表示无序归约的独立许可位，避免将 FMA 许可当作归约重排许可。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 4u)
 *
 * ## Constraints and lifecycle
 * 位于 UInt32 bit 4；完整有效规则要求 relaxed 浮点/确定性，求交后可清除 preserveExceptions，语义行可清除零符号保留。当前 LegalizeVector 只按 requiredFastMath 检查所要求的位，归约顺序门禁另看 preserveSourceOrder/STRICT_ORDERED；不能将本位的存在误写为实现总会自动要求它。无该位组合的许可意图由同接口 TODO 继续核对。 固定位移均以 UInt32 的 1 为左操作数，位号在 0..4 内；本宏无动态参数、分配、借用或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:131
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:137
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:150
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:325
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:439
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:330
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:341
 * - tests/parser/test_ssa_numeric_vector_ir.c:252
 * - tests/parser/test_ssa_numeric_vector_ir.c:257
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_KNOWN_MASK
 *
 * ## Summary
 * 界定 schema 1 可识别的五个 fast-math 位，供所有掩码形状检查。
 *
 * ## Value and use
 * ((TZrUInt32)0x1fu)
 *
 * ## Constraints and lifecycle
 * 值为 UInt32 0x1f；未知位会被策略、Allows 或向量请求拒绝。掩码是已知位全集，不代表这些变换已被请求、授权或实际执行；必须经过各层交集和有效规则。 固定位移均以 UInt32 的 1 为左操作数，位号在 0..4 内；本宏无动态参数、分配、借用或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:132
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:102
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:105
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:454
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:262
 * - tests/parser/test_ssa_numeric_vector_ir.c:76
 * - tests/parser/test_ssa_numeric_vector_ir.c:78
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_ALL
 *
 * ## Summary
 * 为当前 schema 的全部已知 fast-math 位提供组合拼写。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_FAST_MATH_KNOWN_MASK
 *
 * ## Constraints and lifecycle
 * 直接别名 KNOWN_MASK，不含未来未知位，也不是绕过严格模式或包许可的开关。用作 allowedFastMath 仍需逐层求交，用作 requestedFastMath 不保证全部生效。 固定位移均以 UInt32 的 1 为左操作数，位号在 0..4 内；本宏无动态参数、分配、借用或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:133
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:132
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:416
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:424
 * - tests/parser/test_ssa_numeric_vector_ir.c:102
 * - tests/parser/test_ssa_numeric_vector_ir.c:147
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_CONTRACT
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_FAST_MATH_FMA 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:137
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:128
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:102
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:416
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:424
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:138
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:129
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:102
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:416
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:424
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_DIVISION
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_FAST_MATH_APPROX_RECIPROCAL 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:139
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:129
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:102
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:416
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:424
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZEROS
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_FAST_MATH_NO_SIGNED_ZERO 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:141
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:130
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:102
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:416
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:424
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCE
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_FAST_MATH_UNORDERED_REDUCTION 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:143
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:131
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:102
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:416
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:424
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR
 *
 * ## Summary
 * 声明目标具有向量方案的基本能力位。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 0u)
 *
 * ## Constraints and lifecycle
 * bit 0 为合法化最先检查的目标门禁；缺失时选择 TARGET_UNSUPPORTED，不能仅凭元素位进入向量分支。 该宏为 UInt32 固定位，位号小于 32；它不是 CPU 探测或执行指令，不拥有资源。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:148
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:298
 * - tests/parser/test_ssa_numeric_vector_ir.c:179
 * - tests/parser/test_ssa_numeric_vector_ir.c:202
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_F32
 *
 * ## Summary
 * 声明目标支持 F32 元素的能力位。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 1u)
 *
 * ## Constraints and lifecycle
 * bit 1 供 F32 元素分支查询；仍须同时有 VECTOR，不能据此改变 F32 元素为 F64 或推断宿主浮点实现。 该宏为 UInt32 固定位，位号小于 32；它不是 CPU 探测或执行指令，不拥有资源。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:149
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:77
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:298
 * - tests/parser/test_ssa_numeric_vector_ir.c:180
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_F64
 *
 * ## Summary
 * 声明目标支持 F64 元素的能力位。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 2u)
 *
 * ## Constraints and lifecycle
 * bit 2 供 F64 分支查询；仍须基本 VECTOR，不能以 F32 位替代或在此完成精度转换。 该宏为 UInt32 固定位，位号小于 32；它不是 CPU 探测或执行指令，不拥有资源。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:150
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:81
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:298
 * - tests/parser/test_ssa_numeric_vector_ir.c:243
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_INTEGER
 *
 * ## Summary
 * 声明当前目标能力协议中的通用定宽整数支持。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 3u)
 *
 * ## Constraints and lifecycle
 * bit 3 是整数元素门禁；当前实现先要求此位，再接受任意 I8..U64，个别宽度位不能替代此位或收窄它。该元数据不证明目标 CPU 或代码生成已经支持全部宽度。 该宏为 UInt32 固定位，位号小于 32；它不是 CPU 探测或执行指令，不拥有资源。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:151
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:90
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:127
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_MASK
 *
 * ## Summary
 * 声明目标具有逻辑谓词/mask 支持。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 4u)
 *
 * ## Constraints and lifecycle
 * bit 4 用于 BOOL/MASK 元素和 SELECT 的目标能力检查。它不是实际 mask 数组、对象根或地址，不定义物理存储位宽。 该宏为 UInt32 固定位，位号小于 32；它不是 CPU 探测或执行指令，不拥有资源。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:152
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:85
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:305
 * - tests/parser/test_ssa_numeric_vector_ir.c:181
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_FMA
 *
 * ## Summary
 * 声明目标可提供 FMA 方案的能力。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 5u)
 *
 * ## Constraints and lifecycle
 * bit 5 只陈述目标能力，不代替独立 FAST_MATH_FMA 许可；当前 FMA 路径两者都检查。 该宏为 UInt32 固定位，位号小于 32；它不是 CPU 探测或执行指令，不拥有资源。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:153
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:313
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:317
 * - tests/parser/test_ssa_numeric_vector_ir.c:267
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_APPROX_RECIPROCAL
 *
 * ## Summary
 * 声明目标可提供近似倒数方案的能力。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 6u)
 *
 * ## Constraints and lifecycle
 * bit 6 在 DIV 请求显式要求近似倒数位时被检查；有能力仍需策略许可，没有此需求不单独触发本门禁。 该宏为 UInt32 固定位，位号小于 32；它不是 CPU 探测或执行指令，不拥有资源。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:154
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:323
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:330
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_UNALIGNED
 *
 * ## Summary
 * 声明目标接受缺少所需对齐保证的向量内存操作。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 7u)
 *
 * ## Constraints and lifecycle
 * bit 7 可绕过 LOAD/STORE 的对齐回退条件；此处只消费声明元数据，不读取指针、不认证缓冲区安全，也不检查真实地址是否对齐。 该宏为 UInt32 固定位，位号小于 32；它不是 CPU 探测或执行指令，不拥有资源。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:155
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:335
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:338
 * - tests/parser/test_ssa_numeric_vector_ir.c:213
 * - tests/parser/test_ssa_numeric_vector_ir.c:215
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_REDUCE
 *
 * ## Summary
 * 声明目标具有归约操作方案的能力。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 8u)
 *
 * ## Constraints and lifecycle
 * bit 8 对所有 REDUCE_* 检查；它不授权改变源顺序，preserveSourceOrder 或 STRICT_ORDERED 仍会使归约回退，具体许可缺口由 Legalize 同 key TODO 记录。 该宏为 UInt32 固定位，位号小于 32；它不是 CPU 探测或执行指令，不拥有资源。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:156
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:309
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:341
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:345
 * - tests/parser/test_ssa_numeric_vector_ir.c:244
 * - tests/parser/test_ssa_numeric_vector_ir.c:247
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_I8
 *
 * ## Summary
 * 为目标描述提供 I8 lane 的显式宽度能力标签。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 9u)
 *
 * ## Constraints and lifecycle
 * 该 UInt32 位为 bit 9，在 KNOWN_MASK 范围内。当前 vector_element_supported 先要求通用 INTEGER 位，后续返回条件也接受该通用位，因此本位单独存在不能启用整数向量，也不能在同时有 INTEGER 时排除其他整数宽度。它是可记录的能力元数据，不是实际整数对象或位转换函数；没有资源、动态参数或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:157
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:151
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:90
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:127
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:99
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_I16
 *
 * ## Summary
 * 为目标描述提供 I16 lane 的显式宽度能力标签。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 10u)
 *
 * ## Constraints and lifecycle
 * 该 UInt32 位为 bit 10，在 KNOWN_MASK 范围内。当前 vector_element_supported 先要求通用 INTEGER 位，后续返回条件也接受该通用位，因此本位单独存在不能启用整数向量，也不能在同时有 INTEGER 时排除其他整数宽度。它是可记录的能力元数据，不是实际整数对象或位转换函数；没有资源、动态参数或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:158
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:151
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:90
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:127
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:102
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_I32
 *
 * ## Summary
 * 为目标描述提供 I32 lane 的显式宽度能力标签。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 11u)
 *
 * ## Constraints and lifecycle
 * 该 UInt32 位为 bit 11，在 KNOWN_MASK 范围内。当前 vector_element_supported 先要求通用 INTEGER 位，后续返回条件也接受该通用位，因此本位单独存在不能启用整数向量，也不能在同时有 INTEGER 时排除其他整数宽度。它是可记录的能力元数据，不是实际整数对象或位转换函数；没有资源、动态参数或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:159
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:151
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:90
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:127
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:105
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_I64
 *
 * ## Summary
 * 为目标描述提供 I64 lane 的显式宽度能力标签。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 12u)
 *
 * ## Constraints and lifecycle
 * 该 UInt32 位为 bit 12，在 KNOWN_MASK 范围内。当前 vector_element_supported 先要求通用 INTEGER 位，后续返回条件也接受该通用位，因此本位单独存在不能启用整数向量，也不能在同时有 INTEGER 时排除其他整数宽度。它是可记录的能力元数据，不是实际整数对象或位转换函数；没有资源、动态参数或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:160
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:151
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:90
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:127
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:108
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_U8
 *
 * ## Summary
 * 为目标描述提供 U8 lane 的显式宽度能力标签。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 13u)
 *
 * ## Constraints and lifecycle
 * 该 UInt32 位为 bit 13，在 KNOWN_MASK 范围内。当前 vector_element_supported 先要求通用 INTEGER 位，后续返回条件也接受该通用位，因此本位单独存在不能启用整数向量，也不能在同时有 INTEGER 时排除其他整数宽度。它是可记录的能力元数据，不是实际整数对象或位转换函数；没有资源、动态参数或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:161
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:151
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:90
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:127
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:111
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_U16
 *
 * ## Summary
 * 为目标描述提供 U16 lane 的显式宽度能力标签。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 14u)
 *
 * ## Constraints and lifecycle
 * 该 UInt32 位为 bit 14，在 KNOWN_MASK 范围内。当前 vector_element_supported 先要求通用 INTEGER 位，后续返回条件也接受该通用位，因此本位单独存在不能启用整数向量，也不能在同时有 INTEGER 时排除其他整数宽度。它是可记录的能力元数据，不是实际整数对象或位转换函数；没有资源、动态参数或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:162
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:151
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:90
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:127
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:114
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_U32
 *
 * ## Summary
 * 为目标描述提供 U32 lane 的显式宽度能力标签。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 15u)
 *
 * ## Constraints and lifecycle
 * 该 UInt32 位为 bit 15，在 KNOWN_MASK 范围内。当前 vector_element_supported 先要求通用 INTEGER 位，后续返回条件也接受该通用位，因此本位单独存在不能启用整数向量，也不能在同时有 INTEGER 时排除其他整数宽度。它是可记录的能力元数据，不是实际整数对象或位转换函数；没有资源、动态参数或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:163
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:151
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:90
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:127
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:117
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_U64
 *
 * ## Summary
 * 为目标描述提供 U64 lane 的显式宽度能力标签。
 *
 * ## Value and use
 * ((TZrUInt32)1u << 16u)
 *
 * ## Constraints and lifecycle
 * 该 UInt32 位为 bit 16，在 KNOWN_MASK 范围内。当前 vector_element_supported 先要求通用 INTEGER 位，后续返回条件也接受该通用位，因此本位单独存在不能启用整数向量，也不能在同时有 INTEGER 时排除其他整数宽度。它是可记录的能力元数据，不是实际整数对象或位转换函数；没有资源、动态参数或运行期失败。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:164
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:151
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:90
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:95
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:127
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:120
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_KNOWN_MASK
 *
 * ## Summary
 * 界定 schema 1 可接受的 17 个目标能力位。
 *
 * ## Value and use
 * ((TZrUInt32)0x1ffffu)
 *
 * ## Constraints and lifecycle
 * UInt32 值 0x1ffff，覆盖 bit 0..16。未知能力位在策略/请求形状校验中被拒绝；已知不等于已支持、已授权或已执行。这个掩码不查询硬件，也不意味着同时设置所有位在每个后端都可兑现。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:165
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:106
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:260
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:298
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_INT
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_TARGET_CAP_INTEGER 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_TARGET_CAP_INTEGER
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_TARGET_CAP_INTEGER 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:166
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:151
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:90
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:127
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_SCALAR
 *
 * ## Summary
 * 以空目标能力集合表达没有声明向量能力的保守起点。
 *
 * ## Value and use
 * ((TZrUInt32)0u)
 *
 * ## Constraints and lifecycle
 * UInt32 零值，并非一个独立 scalar bit。对有效请求，缺少 VECTOR 先产生目标不支持回退；allowScalarFallback 决定该说明是成功回退还是失败。初始化为零不查询 CPU，也不证明标量运算已执行。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:167
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:215
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:298
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:394
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:406
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR_F32
 *
 * ## Summary
 * 组合声明基本向量能力和 F32 元素能力，方便构造对应请求。
 *
 * ## Value and use
 * (ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR | ZR_EXEC_IR_NUMERIC_TARGET_CAP_F32)
 *
 * ## Constraints and lifecycle
 * 宏仅将 VECTOR 与 F32 两个位作或，不包含 MASK/FMA/REDUCE/UNALIGNED，也不授予 fast-math。不能据此承诺目标机器或代码生成实现；请求仍须通过形状与所有后续操作门禁。无参数、分配或借用。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:168
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:148
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:149
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:77
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:298
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR_F64
 *
 * ## Summary
 * 组合声明基本向量能力和 F64 元素能力，方便构造对应请求。
 *
 * ## Value and use
 * (ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR | ZR_EXEC_IR_NUMERIC_TARGET_CAP_F64)
 *
 * ## Constraints and lifecycle
 * 宏仅将 VECTOR 与 F64 两个位作或，不包含 MASK/FMA/REDUCE/UNALIGNED，也不授予 fast-math。不能据此承诺目标机器或代码生成实现；请求仍须通过形状与所有后续操作门禁。无参数、分配或借用。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:170
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:148
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:150
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:81
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:298
 */

/**
 * @enum EZrNumericVectorOperation
 *
 * ## Summary
 * 描述请求规划的内存、算术、比较、选择、归约、整数移位或余数操作，使合法化可以按种类筛选。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_VECTOR_INVALID = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_LOAD = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_STORE = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_ADD = 3；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_SUB = 4；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_MUL = 5；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_DIV = 6；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_FMA = 7；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_NEG = 8；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_COMPARE = 9；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_SELECT = 10；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_ADD = 11；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MIN = 12；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MAX = 13；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_SHIFT_LEFT = 14；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_SHIFT_RIGHT = 15；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_MOD = 16；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_VECTOR_OPERATION_COUNT = 17；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * INVALID/COUNT 不可用。FMA 只接受浮点并另需 FMA 许可；移位/MOD 只接受整数；算术/归约不接受 BOOL/MASK，SELECT 不接受 MASK。NumericSemanticsFor 的标量元素范围还排除 MASK。枚举不是 ExecIR opcode，也不生成或执行指令。这是 C 枚举标签，不保证 sizeof(enum)、字节序、padding 或跨 ABI 的二进制布局；持久化/FFI 应按已知逻辑数值编码。COUNT 是范围上界，不能作为有效模式。此头不执行数值运算或改变宿主浮点环境。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:249
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:267
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:274
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:286
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:293
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:300
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:150
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:154
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:160
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:164
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_vectorize.c:431
 * - tests/parser/test_ssa_numeric_vector_ir.c:296
 * - tests/parser/test_ssa_numeric_vector_ir.c:301
 */

/**
 * @typedef EZrExecIrNumericVectorOperation
 *
 * ## Summary
 * 为 EZrNumericVectorOperation 提供 ExecIr 命名的兼容类型拼写，供公开头与仓外调用方使用。
 *
 * ## Representation
 * 本声明与 EZrNumericVectorOperation 是同一 C 类型，未增加成员、存储或 ABI 保证。
 *
 * ## Constraints and lifecycle
 * 这是同一 C 类型的 typedef 别名，不产生独立 enum/struct、存储、位宽或所有权；有效值、初始化/验证约束及借用生命周期完全沿用 EZrNumericVectorOperation。不能因别名包含 ExecIr 就推断可对 struct 原始字节进行跨 ABI 序列化。当前首方文本检索未发现别名在本头之外的直接使用，仓外公共消费者未知，不据此称为死代码。描述请求规划的内存、算术、比较、选择、归约、整数移位或余数操作，使合法化可以按种类筛选。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:193
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:249
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:267
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:274
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_VECTOR_REDUCTION_ADD
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_ADD 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_ADD
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_ADD 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:195
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_VECTOR_REDUCTION_MIN
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MIN 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MIN
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MIN 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:197
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_VECTOR_REDUCTION_MAX
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MAX 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MAX
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_VECTOR_REDUCE_MAX 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:199
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_VECTOR_ARITH_ADD
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_VECTOR_ADD 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_VECTOR_ADD
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_VECTOR_ADD 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:201
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_VECTOR_ARITH_SUB
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_VECTOR_SUB 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_VECTOR_SUB
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_VECTOR_SUB 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:202
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_VECTOR_ARITH_MUL
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_VECTOR_MUL 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_VECTOR_MUL
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_VECTOR_MUL 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:203
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 */

/**
 * @def ZR_EXEC_IR_NUMERIC_VECTOR_ARITH_DIV
 *
 * ## Summary
 * 为 ZR_EXEC_IR_NUMERIC_VECTOR_DIV 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZR_EXEC_IR_NUMERIC_VECTOR_DIV
 *
 * ## Constraints and lifecycle
 * 这是 ZR_EXEC_IR_NUMERIC_VECTOR_DIV 的完全相同数值拼写，不增加位、不改变 enum 类型或目标语义，不执行转换、算术或目标探测；有效域与目标常量相同。用于枚举/掩码上下文时不能把兼容拼写当作独立能力或独立许可。当前未检出仓内展开时仍保留公开仓外消费未知的边界。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:204
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:173
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:142
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 */

/**
 * @enum EZrNumericLegalizationDecision
 *
 * ## Summary
 * 区分尚未决策的结果、向量描述与显式标量回退描述，要求消费者按返回值与结果状态共同判读。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_NONE = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_VECTOR = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_SCALAR_FALLBACK = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_DECISION_COUNT = 3；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * NONE 为 Init 后状态；合法请求可返回 VECTOR 或 SCALAR_FALLBACK，成功不等于实际执行 SIMD。禁止回退的失败分支也会先填充 SCALAR_FALLBACK 结果，因此仅检查 decision 不能判断 API 成功；先看布尔返回。COUNT 只是枚举上界。C enum 布局不构成线格式承诺。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:206
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:225
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:394
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:397
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:406
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:420
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_vectorize.c:696
 * - tests/parser/test_ssa_numeric_vector_ir.c:195
 * - tests/parser/test_ssa_numeric_vector_ir.c:204
 * - tests/parser/test_ssa_numeric_vector_ir.c:248
 */

/**
 * @typedef EZrExecIrNumericLegalizationDecision
 *
 * ## Summary
 * 为 EZrNumericLegalizationDecision 提供 ExecIr 命名的兼容类型拼写，供公开头与仓外调用方使用。
 *
 * ## Representation
 * 本声明与 EZrNumericLegalizationDecision 是同一 C 类型，未增加成员、存储或 ABI 保证。
 *
 * ## Constraints and lifecycle
 * 这是同一 C 类型的 typedef 别名，不产生独立 enum/struct、存储、位宽或所有权；有效值、初始化/验证约束及借用生命周期完全沿用 EZrNumericLegalizationDecision。不能因别名包含 ExecIr 就推断可对 struct 原始字节进行跨 ABI 序列化。当前首方文本检索未发现别名在本头之外的直接使用，仓外公共消费者未知，不据此称为死代码。区分尚未决策的结果、向量描述与显式标量回退描述，要求消费者按返回值与结果状态共同判读。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:212
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:206
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:225
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:394
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:397
 */

/**
 * @enum EZrNumericLegalizationReason
 *
 * ## Summary
 * 为合法化或回退描述保存可读的有限原因码，便于上层将目标、操作、许可、对齐和顺序拒绝分别展示。
 *
 * ## Values and representation
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_NONE = 0；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_TARGET_UNSUPPORTED = 1；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ALIGNMENT = 2；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_LANE_UNSUPPORTED = 3；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_OPERATION_UNSUPPORTED = 4；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_FAST_MATH_DENIED = 5；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ORDER_REQUIRED = 6；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_EMPTY_INPUT = 7；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_INVALID = 8；逻辑标签；按本类型的约束使用。
 * ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_COUNT = 9；范围上界，不能作为有效值。
 *
 * ## Constraints and lifecycle
 * NONE 表示没有回退原因，不是诊断错误码。当前 vector_unsupported_reason 按源码顺序返回目标、操作、fast-math、对齐或顺序原因；LANE_UNSUPPORTED/EMPTY_INPUT/INVALID 有名字但当前正常合法化不发出它们，lane 形状错误走 INVALID_ARGUMENT，零长度可成功。COUNT 为上界，超界名称查询返回 invalid。枚举未保证固定 ABI 宽度。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:257
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:298
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:305
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:317
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:330
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:335
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:341
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:476
 * - tests/parser/test_ssa_numeric_vector_ir.c:205
 * - tests/parser/test_ssa_numeric_vector_ir.c:217
 * - tests/parser/test_ssa_numeric_vector_ir.c:249
 * - tests/parser/test_ssa_numeric_vector_ir.c:279
 */

/**
 * @typedef EZrExecIrNumericLegalizationReason
 *
 * ## Summary
 * 为 EZrNumericLegalizationReason 提供 ExecIr 命名的兼容类型拼写，供公开头与仓外调用方使用。
 *
 * ## Representation
 * 本声明与 EZrNumericLegalizationReason 是同一 C 类型，未增加成员、存储或 ABI 保证。
 *
 * ## Constraints and lifecycle
 * 这是同一 C 类型的 typedef 别名，不产生独立 enum/struct、存储、位宽或所有权；有效值、初始化/验证约束及借用生命周期完全沿用 EZrNumericLegalizationReason。不能因别名包含 ExecIr 就推断可对 struct 原始字节进行跨 ABI 序列化。当前首方文本检索未发现别名在本头之外的直接使用，仓外公共消费者未知，不据此称为死代码。为合法化或回退描述保存可读的有限原因码，便于上层将目标、操作、许可、对齐和顺序拒绝分别展示。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:226
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:257
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:298
 */

/**
 * @struct SZrNumericSemanticContract
 *
 * ## Summary
 * 保存一项操作所需的标量数值语义标签，作为后端实现及诊断的输入，而非运行时数值容器。
 *
 * ## Fields and invariants
 * schemaVersion 标明模型版本；elementKind 标明逻辑位宽/浮点域，不是 enum 对象位数。
 * overflowMode/shiftMode/divideMode/nanMode/roundingMode/exceptionMode 与 preserveSignedZero 共同描述语义。
 * Init 为严格 F32 默认；For 按元素覆盖除法、舍入、异常等字段，不保证任意自填字段组合对应可执行后端。
 *
 * ## Constraints and lifecycle
 * 由调用方持有的纯字段记录，不拥有数组、对象或动态资源；复制不延长任何执行对象寿命。使用 Init/Validate 或 NumericSemanticsFor 取得可用记录。schemaVersion 必须匹配 1，各枚举/布尔需符合验证范围；MASK 被排除。记录未保存 operation 或来源策略地址，必须配合调用方的操作上下文解释。枚举和 padding 不保证固定二进制布局，不能直接将 sizeof(struct) 字节作为跨 ABI 格式；该契约不执行算术，后端须自行避免带符号溢出、非法移位及除法 UB。共享只读可并行，对同一记录写入须同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:231
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:191
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:208
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:234
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:310
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:330
 * - tests/parser/test_ssa_numeric_vector_ir.c:35
 */

/**
 * @struct SZrNumericPolicy
 *
 * ## Summary
 * 保存请求、项目、包或目标的数值配置、独立 fast-math 许可及身份摘要，用于校验、求交和合法化。
 *
 * ## Fields and invariants
 * schemaVersion、overflow/shift/float/determinism/divide/NaN/round/exception 构成配置标签。
 * requestedFastMath 是请求集合，allowedFastMath 是作用域许可，effectiveFastMath 是生效子集，deniedFastMath 是被拒请求；均仅含 KNOWN_MASK。
 * targetCapabilities 只陈述目标能力，不能代替 fast-math 许可或 CPU 探测。
 * preserveNaN/preserveSignedZero/preserveExceptions 描述可观察语义；reserved0 为合法布尔。
 * projectHash/packageHash/targetHash 为调用方的身份输入，求交复制各层来源，校验不认证对应实体。
 *
 * ## Constraints and lifecycle
 * 该记录不含宿主指针、不拥有堆资源。必须初始化全部被读字段，Validate 对版本/枚举/布尔/已知位和若干有效许可关系做检查，不能替代数值执行或目标探测。effective 是 requested&allowed 的子集；denied 非零时精确对应未生效请求，零 effective/denied 可为暂未求交的请求。保留语义布尔与枚举是不同字段，不能推断所有潜在组合都被交叉验证。reserved0 当前只要求 0/1，Init 写 0。记录可复制或共享稳定只读，修改须同步；C enum/padding 不能作为稳定线格式。Hash 按字段混合并有碰撞可能。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:243
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:73
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:118
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:133
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:333
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:358
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:388
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:288
 * - tests/parser/test_ssa_numeric_vector_ir.c:95
 */

/**
 * @typedef SZrExecIrNumericPolicy
 *
 * ## Summary
 * 为 SZrNumericPolicy 提供 ExecIr 命名的兼容类型拼写，供公开头与仓外调用方使用。
 *
 * ## Representation
 * 本声明与 SZrNumericPolicy 是同一 C 类型，未增加成员、存储或 ABI 保证。
 *
 * ## Constraints and lifecycle
 * 这是同一 C 类型的 typedef 别名，不产生独立 enum/struct、存储、位宽或所有权；有效值、初始化/验证约束及借用生命周期完全沿用 SZrNumericPolicy。不能因别名包含 ExecIr 就推断可对 struct 原始字节进行跨 ABI 序列化。当前首方文本检索未发现别名在本头之外的直接使用，仓外公共消费者未知，不据此称为死代码。保存请求、项目、包或目标的数值配置、独立 fast-math 许可及身份摘要，用于校验、求交和合法化。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:266
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:243
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:73
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:118
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:133
 */

/**
 * @fn TZrBool ZrParser_ExecIr_NumericSemanticsFor( EZrNumericVectorOperation operation, EZrNumericElementKind elementKind, const SZrNumericPolicy *policy, SZrNumericSemanticContract *contract, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 把操作、元素种类与已验证的数值策略整理为标量语义记录，供后端决定如何实现该操作；本接口不执行算术或生成向量指令。
 *
 * ## Parameters
 * @param[in] operation 待描述的操作；须位于 INVALID 与 OPERATION_COUNT 之间，另按元素种类检查算术、整数专用、FMA 和归约组合。
 * @param[in] elementKind 标量元素标签；接受 BOOL 至 F64，MASK 不是本接口的标量语义元素。
 * @param[in] policy 调用期借用的策略；可为空，空时使用局部严格默认策略。非空时先执行完整策略校验。
 * @param[out] contract 输出记录；可为空，空时返回 FALSE。非空时入口先清零，随后才进行输入检查和字段填充。
 * @param[out] diagnostic 可选独立诊断槽；非空时入口清零，失败时记录首个拒绝原因。
 *
 * ## Returns
 * @return 操作、元素和策略均被接受且结果记录通过形状校验时返回 TRUE；否则返回 FALSE。FALSE 后不得使用 contract 作为有效语义记录，输出可能保持入口清零状态，也可能已填充后被末次校验拒绝。
 *
 * ## Constraints and lifecycle
 * 非空输入须在调用期间稳定可读，输出须有正确对齐和完整可写存储；诊断槽须与输入及其他输出分离。实现不加锁，共享只读输入可以并行读取，写入或释放同一记录须由调用方同步。 contract 不得覆盖 policy。结果不保留 policy 地址，也不保留 operation，调用方应保留对应操作上下文。divideMode 根据元素是否浮点重新选择 IEEE 或 CHECKED，并非直接复制 policy.divideMode；非浮点改用向零舍入和保留异常，允许无序归约时可清除 signed-zero 保留标志。FMA 另要求独立 FMA 许可。模式只是后端契约，不能用宿主带符号溢出、越界移位或未检查除法替代其实现；此函数没有运行期数值、内存对象或资源所有权转移。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:234
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:245
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:249
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:261
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:267
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:300
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:310
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:314
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:321
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:325
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:330
 * - tests/parser/test_ssa_numeric_vector_ir.c:58
 * - tests/parser/test_ssa_numeric_vector_ir.c:64
 */

/**
 * @struct SZrVectorLegalizationRequest
 *
 * ## Summary
 * 在一次同步合法化调用中提供操作、元素、逻辑长度、目标描述和可选借用策略，生成后端可消费的计划记录。
 *
 * ## Fields and invariants
 * schemaVersion/operation/elementKind 指定模型和操作域，按操作进行元素组合校验。
 * length 为 UInt64 逻辑元素数；targetVectorLanes 为 UInt32 lane 数。
 * targetCapabilities、sourceAlignment、requiredAlignment、requiredFastMath 为声明性约束，不读取真实源地址。
 * targetHash/targetSignatureHash/targetLayoutHash 和 functionToken/blockId/instructionId/sourceId 参与摘要或定位，无所有权。
 * policy 仅调用期借用；allowScalarFallback 控制是否允许非向量方案；preserveSourceOrder 可强制归约回退。
 * reserved0/reserved1 初始化为 0；当前校验接受任意合法布尔值。
 *
 * ## Constraints and lifecycle
 * request.policy 是真实的宿主借用指针，可为 NULL 使用默认策略；该结构不是整体无指针的持久化记录。它不拥有 policy，初始化或丢弃请求都不会 free；非空 policy 及请求本身须在整个调用中稳定可读，不能与 result/diagnostic 的写区重叠。lanes 有效范围 1..4096，两个 alignment 为 0 或 2 的幂；能力和 fast-math 只允许已知位，四个布尔包含 reserved 均按 0/1 校验。默认 Init 的 INVALID 操作/元素需由调用方填妥。没有实际数组地址、容量或内存所有权，length 不证明缓冲区可读；0 长度合法。ID/hash 只用于结果位置和摘要，没有匹配函数或布局验证。C enum、匿名联合以外的字段对齐及指针位宽使原始 struct 不能当跨 ABI 线格式。共享只读与写入须由调用方同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:275
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:292
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:180
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:210
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:241
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:257
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:274
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:288
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:353
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_vectorize.c:677
 * - tests/parser/test_ssa_numeric_vector_ir.c:166
 */

/**
 * @typedef SZrExecIrVectorLegalizationRequest
 *
 * ## Summary
 * 为 SZrVectorLegalizationRequest 提供 ExecIr 命名的兼容类型拼写，供公开头与仓外调用方使用。
 *
 * ## Representation
 * 本声明与 SZrVectorLegalizationRequest 是同一 C 类型，未增加成员、存储或 ABI 保证。
 *
 * ## Constraints and lifecycle
 * 这是同一 C 类型的 typedef 别名，不产生独立 enum/struct、存储、位宽或所有权；有效值、初始化/验证约束及借用生命周期完全沿用 SZrVectorLegalizationRequest。不能因别名包含 ExecIr 就推断可对 struct 原始字节进行跨 ABI 序列化。当前首方文本检索未发现别名在本头之外的直接使用，仓外公共消费者未知，不据此称为死代码。在一次同步合法化调用中提供操作、元素、逻辑长度、目标描述和可选借用策略，生成后端可消费的计划记录。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:298
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:275
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:292
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:180
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:210
 */

/**
 * @struct SZrVectorLegalizationResult
 *
 * ## Summary
 * 保存一次合法化产生的决策、原因、形状、许可后果和位置摘要，使上层能区分向量方案与显式标量回退。
 *
 * ## Fields and invariants
 * schemaVersion/decision/reason 表示版本与状态；operation/elementKind 由合法请求复制，Init 时为 0。
 * chosenLanes/vectorLanes/vectorWidth 共享 UInt32 存储；VECTOR 取目标 lane，fallback 为 1。
 * 已形成 VECTOR 方案时，vectorIterations 为 length/lanes，tailLanes 为 length%lanes；已形成 SCALAR_FALLBACK 方案时，这两项均为 0；上述两种方案的 scalarIterations 均保存完整 length，不能当 vector 尾部元素数。
 * appliedFastMath 在 VECTOR 保存整个 policy.effectiveFastMath，fallback 清零；preservesNaN/Zero/Exceptions 复制策略，SourceOrder 复制请求并可在顺序回退时置 TRUE。
 * contractHash 混合请求和策略内容而非 policy 地址，非密码学/无碰撞保证；位置 ID 复制而不认证实体。
 * usesScalarFallback 指示回退描述；reserved0/1/2 由 Init 清零，没有结果校验 API。
 *
 * ## Constraints and lifecycle
 * 该结果只含逻辑标量，不拥有缓冲区、策略或运行时对象，复制不延长外部生命周期。必须结合 LegalizeVector 的返回值判读：失败可留下 Init 默认或已填充 fallback 描述，不保留旧值。三个 lane 成员是同一匿名 union 的等宽拼写，不是三个独立计数。结果中的保留标志和 fast-math 是计划说明，不证明实际执行；当前 vector 分支不将 scalarIterations 改为尾数。枚举/padding 不保证稳定 ABI 或线格式；共享只读可并行，覆盖同一记录须同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:300
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:306
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:225
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:380
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:386
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:387
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:388
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:392
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:394
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:406
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:420
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:424
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_vectorize.c:695
 * - tests/parser/test_ssa_numeric_vector_ir.c:187
 */

/**
 * @typedef SZrExecIrVectorLegalizationResult
 *
 * ## Summary
 * 为 SZrVectorLegalizationResult 提供 ExecIr 命名的兼容类型拼写，供公开头与仓外调用方使用。
 *
 * ## Representation
 * 本声明与 SZrVectorLegalizationResult 是同一 C 类型，未增加成员、存储或 ABI 保证。
 *
 * ## Constraints and lifecycle
 * 这是同一 C 类型的 typedef 别名，不产生独立 enum/struct、存储、位宽或所有权；有效值、初始化/验证约束及借用生命周期完全沿用 SZrVectorLegalizationResult。不能因别名包含 ExecIr 就推断可对 struct 原始字节进行跨 ABI 序列化。当前首方文本检索未发现别名在本头之外的直接使用，仓外公共消费者未知，不据此称为死代码。保存一次合法化产生的决策、原因、形状、许可后果和位置摘要，使上层能区分向量方案与显式标量回退。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:329
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:300
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:306
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:225
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:380
 */

/**
 * @fn void ZrParser_ExecIr_NumericSemanticContractInit( SZrNumericSemanticContract *contract)
 *
 * ## Summary
 * 将调用方提供的语义记录重置为 schema 1 的严格 F32 默认值，适合填充或校验具体操作记录之前使用。
 *
 * ## Parameters
 * @param[out] contract 待初始化的可写记录；为空时直接返回。
 *
 * ## Returns
 * 无 C 返回值；非空记录先整体清零，再设置 F32、CHECKED 溢出、MASKED 移位、IEEE 除法、保留 NaN/异常/有符号零和 nearest-even 舍入。
 *
 * ## Constraints and lifecycle
 * 不分配或释放资源，不读取记录旧值；重复调用会覆盖已有字段。记录自身由调用方管理，对同一记录的写入须串行。初始化不证明任何后端已实现这些数值语义，也不执行 F32 转换。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:191
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:196
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:198
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:205
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:310
 * - tests/parser/test_ssa_numeric_vector_ir.c:40
 */

/**
 * @fn TZrBool ZrParser_ExecIr_ValidateNumericSemanticContract( const SZrNumericSemanticContract *contract, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 检查语义记录的版本、标量元素范围、各模式枚举及布尔表示，作为消费该记录前的形状门禁。
 *
 * ## Parameters
 * @param[in] contract 调用期借用记录；可为空，空值被拒绝。非空时须稳定可读。
 * @param[out] diagnostic 可选独立诊断槽；入口清零，不能与 contract 重叠。
 *
 * ## Returns
 * @return 记录通过本函数列出的形状检查时返回 TRUE；空记录、版本错误、MASK/无效元素、模式范围错误或非 0/1 布尔值返回 FALSE，并在非空诊断槽写 INVALID_ARGUMENT。
 *
 * ## Constraints and lifecycle
 * 非空输入须在调用期间稳定可读，输出须有正确对齐和完整可写存储；诊断槽须与输入及其他输出分离。实现不加锁，共享只读输入可以并行读取，写入或释放同一记录须由调用方同步。 不修改 contract，不分配资源；成功仅证明这些字段范围，未执行算术，未验证操作与元素组合、CPU 能力或模式间所有语义关系。检查不能认证任意地址可读，也不规定 C enum 对象的物理布局。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:208
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:211
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:212
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:224
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:225
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:330
 * - tests/parser/test_ssa_numeric_vector_ir.c:49
 * - tests/parser/test_ssa_numeric_vector_ir.c:53
 */

/**
 * @fn void ZrParser_ExecIr_NumericPolicyInit( SZrNumericPolicy *policy)
 *
 * ## Summary
 * 初始化请求或作用域策略的严格默认值，使未显式授予的 fast-math 许可保持关闭。
 *
 * ## Parameters
 * @param[out] policy 待初始化的可写策略；为空时直接返回。
 *
 * ## Returns
 * 无 C 返回值；非空策略整体清零后设 schema 1、CHECKED/MASKED、STRICT_IEEE、STRICT_ORDERED、IEEE 除法、保留 NaN/有符号零/异常及 nearest-even 舍入。
 *
 * ## Constraints and lifecycle
 * 不分配或释放资源，不读取旧字段。各 fast-math 掩码、目标能力和身份哈希初始为 0；初始化不查询宿主 CPU。可用于构造 requested/project/package/target，再由调用方填写各自许可；重复调用覆盖原值，对同一记录须同步写入。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:333
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:337
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:339
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:350
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:259
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:377
 * - tests/parser/test_ssa_numeric_vector_ir.c:18
 * - tests/parser/test_ssa_numeric_vector_ir.c:104
 */

/**
 * @fn TZrBool ZrParser_ExecIr_ValidateNumericPolicy( const SZrNumericPolicy *policy, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在策略被语义查询或向量合法化消费前，检查版本、枚举/布尔/已知位，以及 effective/denied 掩码与严格配置的相容关系。
 *
 * ## Parameters
 * @param[in] policy 调用期借用策略；可为空，空值被拒绝；非空时须稳定可读。
 * @param[out] diagnostic 可选独立诊断槽；入口先清零，不能与 policy 重叠。
 *
 * ## Returns
 * @return 策略通过形状和有效许可规则时返回 TRUE；否则返回 FALSE。可选诊断区分空参数、版本、字段范围及许可矛盾等现有分支；策略自身不被修改。
 *
 * ## Constraints and lifecycle
 * 非空输入须在调用期间稳定可读，输出须有正确对齐和完整可写存储；诊断槽须与输入及其他输出分离。实现不加锁，共享只读输入可以并行读取，写入或释放同一记录须由调用方同步。 effectiveFastMath 必须是 requestedFastMath 与 allowedFastMath 交集的子集；deniedFastMath 非零时须恰为 requestedFastMath 去掉 effectiveFastMath 后的位。零 effective/denied 可表示尚未求交的请求。严格浮点配置不接受有效 fast-math，非 relaxed 的确定性还限制重排/FMA/无序归约；NO_SIGNED_ZERO 与保留零符号不能并存。此校验不探测 CPU、不执行算术，也不证明全部模式字段之间存在额外未实现的关系。reserved0 当前按合法布尔值检查，并非强制必须为零。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:73
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:78
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:84
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:91
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:118
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:123
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:133
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:150
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:161
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:169
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:352
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:354
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:288
 * - tests/parser/test_ssa_numeric_vector_ir.c:29
 * - tests/parser/test_ssa_numeric_vector_ir.c:78
 * - tests/parser/test_ssa_numeric_vector_ir.c:83
 * - tests/parser/test_ssa_numeric_vector_ir.c:91
 */

/**
 * @fn TZrUInt64 ZrParser_ExecIr_NumericPolicyHash( const SZrNumericPolicy *policy)
 *
 * ## Summary
 * 按确定的字段和低字节到高字节次序混合策略内容，为缓存或诊断提供 64 位身份摘要。
 *
 * ## Parameters
 * @param[in] policy 调用期借用策略；可为空。非空时必须完整初始化且稳定可读；本函数不先验证字段合法性。
 *
 * ## Returns
 * @return 空指针返回 0；非空时返回 schema、模式、四组许可掩码、目标能力、四个布尔字段及项目/包/目标哈希的混合结果。
 *
 * ## Constraints and lifecycle
 * 只读策略，不分配或保存其地址；不对结构体 padding 或指针地址作原始字节哈希。乘法在 TZrUInt64 无符号域按模 2^64 进行。结果不是密码学摘要或无碰撞证明，不能承诺每个非空输入结果非零，也不能以哈希代替 ValidateNumericPolicy。C enum 先转成 UInt32 再混合；跨平台一致性以相同逻辑字段值和本版本算法为条件，不以 struct 原始布局为条件。稳定输入可并行只读；修改须同步。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:9
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:14
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:20
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:358
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:361
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:364
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:381
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:384
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:205
 * - tests/parser/test_ssa_numeric_vector_ir.c:30
 * - tests/parser/test_ssa_numeric_vector_ir.c:32
 */

/**
 * @fn TZrBool ZrParser_ExecIr_NumericPolicyIntersect( const SZrNumericPolicy *requested, const SZrNumericPolicy *project, const SZrNumericPolicy *package, const SZrNumericPolicy *target, SZrNumericPolicy *effective, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 将请求与项目、包和目标三层许可求交，显式发布 effective/denied 掩码及其保留语义后果，避免上层宽松许可绕过较窄作用域。
 *
 * ## Parameters
 * @param[in] requested 调用期借用的请求策略；可为空，空时返回 FALSE。它提供所请求变换及语义基线，allowedFastMath 为 0 时在本步骤表示未再收窄。
 * @param[in] project 调用期借用项目策略；可为空，空时返回 FALSE；其 allowedFastMath 参与交集。
 * @param[in] package 调用期借用包策略；可为空，空时返回 FALSE；其 allowedFastMath 参与交集。
 * @param[in] target 调用期借用目标策略；可为空，空时返回 FALSE；提供目标许可、能力及 targetHash。
 * @param[out] effective 独立输出记录；可为空，空时返回 FALSE。非空时入口清零，不得与四个输入记录重叠。
 * @param[out] diagnostic 可选独立诊断槽；入口清零，不得覆盖任何输入或 effective。
 *
 * ## Returns
 * @return 四个输入通过请求形状检查、交集结果通过完整有效规则时返回 TRUE；否则返回 FALSE。前置失败后 effective 已清零；末次规则检查失败时 effective 可能已经包含未被接受的候选，不提供旧输出保留或失败回滚保证。
 *
 * ## Constraints and lifecycle
 * 非空输入须在调用期间稳定可读，输出须有正确对齐和完整可写存储；诊断槽须与输入及其他输出分离。实现不加锁，共享只读输入可以并行读取，写入或释放同一记录须由调用方同步。 四个只读输入可互相同址，但必须与 effective/diagnostic 分离。先以 requested 为基线，再相交 project/package/target.allowedFastMath；requested.allowedFastMath 非零时继续收窄。目标能力来自 target，三层身份哈希取对应来源；其余模式不进行四层逐字段求交。NO_SIGNED_ZERO 可清除保留零符号，重排/近似倒数/无序归约可清除保留异常。无动态分配、无资源转移；输出记录由调用方管理。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:388
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:397
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:399
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:401
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:408
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:415
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:416
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:421
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:424
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:428
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:435
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:439
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:445
 * - tests/parser/test_ssa_numeric_vector_ir.c:117
 * - tests/parser/test_ssa_numeric_vector_ir.c:128
 * - tests/parser/test_ssa_numeric_vector_ir.c:155
 * - tests/parser/test_ssa_numeric_vector_ir.c:237
 */

/**
 * @fn TZrBool ZrParser_ExecIr_NumericPolicyAllows( const SZrNumericPolicy *policy, TZrUInt32 fastMathFlags)
 *
 * ## Summary
 * 查询一个已生效策略是否同时授予给定的全部 fast-math 位，供 FMA 等变换单独检查许可。
 *
 * ## Parameters
 * @param[in] policy 调用期借用策略；为空或验证失败时返回 FALSE。
 * @param[in] fastMathFlags 要同时满足的 UInt32 许可位集合；只能含 KNOWN_MASK 中的位，0 表示空集合查询。
 *
 * ## Returns
 * @return 策略有效且 fastMathFlags 是 effectiveFastMath 的子集时返回 TRUE；空策略、未知位或策略无效时返回 FALSE。合法策略对空集合返回 TRUE。
 *
 * ## Constraints and lifecycle
 * 只读、不分配、不写诊断，也不放宽策略。内部再次 ValidateNumericPolicy；它不是 requested/allowed 位的直接查询，不以目标 CPU 能力代替许可。输入须稳定可读，可共享只读；与同记录修改并发须同步。兼容名字 NumericPolicy_Allows 展开到本函数，未引入另一份实现。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:451
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:453
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:457
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:460
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:301
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:319
 * - tests/parser/test_ssa_numeric_vector_ir.c:158
 * - tests/parser/test_ssa_numeric_vector_ir.c:160
 * - tests/parser/test_ssa_numeric_vector_ir.c:162
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:359
 */

/**
 * @def ZrParser_ExecIr_ValidateNumeric
 *
 * ## Summary
 * 为 ZrParser_ExecIr_ValidateNumericPolicy 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZrParser_ExecIr_ValidateNumericPolicy
 *
 * ## Parameters
 * 此对象宏不定义形参；展开后的参数及方向沿用 ZrParser_ExecIr_ValidateNumericPolicy 的声明。
 *
 * ## Returns
 * 宏本身没有独立 C 返回值；作为调用使用时返回 ZrParser_ExecIr_ValidateNumericPolicy 的结果，失败输出约束也沿用该接口。
 *
 * ## Constraints and lifecycle
 * 这是无形参的对象宏，预处理后引用 ZrParser_ExecIr_ValidateNumericPolicy 的同一函数声明，不产生包装函数、额外参数求值、独立链接符号或另一份实现。作为调用或函数地址使用时，参数方向、返回、NULL/诊断别名、失败输出及借用约束完全沿用目标接口。不能因为兼容短名就省略目标接口的校验与返回值判断。当前未检出展开不代表仓外没有使用。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:353
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:338
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:73
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:78
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:84
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:91
 */

/**
 * @def ZrParser_ExecIr_NumericPolicy_Effective
 *
 * ## Summary
 * 为 ZrParser_ExecIr_NumericPolicyIntersect 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZrParser_ExecIr_NumericPolicyIntersect
 *
 * ## Parameters
 * 此对象宏不定义形参；展开后的参数及方向沿用 ZrParser_ExecIr_NumericPolicyIntersect 的声明。
 *
 * ## Returns
 * 宏本身没有独立 C 返回值；作为调用使用时返回 ZrParser_ExecIr_NumericPolicyIntersect 的结果，失败输出约束也沿用该接口。
 *
 * ## Constraints and lifecycle
 * 这是无形参的对象宏，预处理后引用 ZrParser_ExecIr_NumericPolicyIntersect 的同一函数声明，不产生包装函数、额外参数求值、独立链接符号或另一份实现。作为调用或函数地址使用时，参数方向、返回、NULL/诊断别名、失败输出及借用约束完全沿用目标接口。不能因为兼容短名就省略目标接口的校验与返回值判断。当前未检出展开不代表仓外没有使用。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:354
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:342
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:388
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:397
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:399
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:401
 */

/**
 * @def ZrParser_ExecIr_NumericPolicy_Intersect
 *
 * ## Summary
 * 为 ZrParser_ExecIr_NumericPolicyIntersect 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZrParser_ExecIr_NumericPolicyIntersect
 *
 * ## Parameters
 * 此对象宏不定义形参；展开后的参数及方向沿用 ZrParser_ExecIr_NumericPolicyIntersect 的声明。
 *
 * ## Returns
 * 宏本身没有独立 C 返回值；作为调用使用时返回 ZrParser_ExecIr_NumericPolicyIntersect 的结果，失败输出约束也沿用该接口。
 *
 * ## Constraints and lifecycle
 * 这是无形参的对象宏，预处理后引用 ZrParser_ExecIr_NumericPolicyIntersect 的同一函数声明，不产生包装函数、额外参数求值、独立链接符号或另一份实现。作为调用或函数地址使用时，参数方向、返回、NULL/诊断别名、失败输出及借用约束完全沿用目标接口。不能因为兼容短名就省略目标接口的校验与返回值判断。当前未检出展开不代表仓外没有使用。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:356
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:342
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:388
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:397
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:399
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:401
 */

/**
 * @def ZrParser_ExecIr_NumericPolicy_Hash
 *
 * ## Summary
 * 为 ZrParser_ExecIr_NumericPolicyHash 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZrParser_ExecIr_NumericPolicyHash
 *
 * ## Parameters
 * 此对象宏不定义形参；展开后的参数及方向沿用 ZrParser_ExecIr_NumericPolicyHash 的声明。
 *
 * ## Returns
 * 宏本身没有独立 C 返回值；作为调用使用时返回 ZrParser_ExecIr_NumericPolicyHash 的结果，失败输出约束也沿用该接口。
 *
 * ## Constraints and lifecycle
 * 这是无形参的对象宏，预处理后引用 ZrParser_ExecIr_NumericPolicyHash 的同一函数声明，不产生包装函数、额外参数求值、独立链接符号或另一份实现。作为调用或函数地址使用时，参数方向、返回、NULL/诊断别名、失败输出及借用约束完全沿用目标接口。不能因为兼容短名就省略目标接口的校验与返回值判断。当前未检出展开不代表仓外没有使用。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:358
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:340
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:9
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:14
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:20
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:358
 */

/**
 * @def ZrParser_ExecIr_NumericPolicy_Allows
 *
 * ## Summary
 * 为 ZrParser_ExecIr_NumericPolicyAllows 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZrParser_ExecIr_NumericPolicyAllows
 *
 * ## Parameters
 * 此对象宏不定义形参；展开后的参数及方向沿用 ZrParser_ExecIr_NumericPolicyAllows 的声明。
 *
 * ## Returns
 * 宏本身没有独立 C 返回值；作为调用使用时返回 ZrParser_ExecIr_NumericPolicyAllows 的结果，失败输出约束也沿用该接口。
 *
 * ## Constraints and lifecycle
 * 这是无形参的对象宏，预处理后引用 ZrParser_ExecIr_NumericPolicyAllows 的同一函数声明，不产生包装函数、额外参数求值、独立链接符号或另一份实现。作为调用或函数地址使用时，参数方向、返回、NULL/诊断别名、失败输出及借用约束完全沿用目标接口。不能因为兼容短名就省略目标接口的校验与返回值判断。当前未检出展开不代表仓外没有使用。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:359
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:349
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:451
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:453
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:457
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:460
 * - tests/parser/test_ssa_numeric_vector_ir.c:158
 * - tests/parser/test_ssa_numeric_vector_ir.c:160
 * - tests/parser/test_ssa_numeric_vector_ir.c:162
 */

/**
 * @fn void ZrParser_ExecIr_VectorLegalizationRequestInit( SZrVectorLegalizationRequest *request)
 *
 * ## Summary
 * 把请求记录置为可继续填充的保守起点，在提供操作、元素及目标描述之前使用。
 *
 * ## Parameters
 * @param[out] request 待初始化的可写请求；为空时直接返回。
 *
 * ## Returns
 * 无 C 返回值；整体清零后设置 schema 1、INVALID 操作和元素、1 lane、requiredAlignment=1、允许标量回退及保留源顺序。
 *
 * ## Constraints and lifecycle
 * 不分配、释放或延长 policy 的生命周期；重复调用只覆盖借用地址和标量字段，不释放旧 policy。初始化后的 INVALID 操作/元素尚不能通过 LegalizeVector。sourceAlignment=0 表示未给出保证，targetCapabilities=0 不代表探测后的宿主能力。记录写入须由调用方同步。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:210
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:215
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:217
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:219
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:222
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_vectorize.c:680
 */

/**
 * @fn void ZrParser_ExecIr_VectorLegalizationResultInit( SZrVectorLegalizationResult *result)
 *
 * ## Summary
 * 重置合法化结果为尚未作出决策的记录，清除前次计数和身份，不将默认值视作已经通过的向量计划。
 *
 * ## Parameters
 * @param[out] result 待初始化的可写结果；为空时直接返回。
 *
 * ## Returns
 * 无 C 返回值；整体清零后设置 schema 1、decision/reason 为 NONE、chosenLanes=1，并将四个 preserves 标志置 TRUE。
 *
 * ## Constraints and lifecycle
 * 不分配或释放资源，结果没有借用指针。重复调用覆盖旧结果；三个 lane 成员是同一匿名 union 的拼写，不能当作三个独立字段。对同一记录的写入须同步；调用后应以 LegalizeVector 返回值与 decision 判断结果是否可用。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:225
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:230
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:234
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:235
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:239
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:364
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:306
 */

/**
 * @fn TZrBool ZrParser_ExecIr_LegalizeVector( const SZrVectorLegalizationRequest *request, SZrVectorLegalizationResult *result, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 根据显式目标能力、数值策略、对齐及顺序要求，生成向量或标量回退描述；只规划，不读取数组、执行算术或重写 ExecIR。
 *
 * ## Parameters
 * @param[in] request 调用期借用请求；可为空，空时返回 FALSE。policy 可为空并使用局部严格默认策略；非空策略须在整个调用期间稳定可读。
 * @param[out] result 独立可写输出；可为空，空时返回 FALSE。非空时入口先 ResultInit，不能覆盖 request 或其 policy。
 * @param[out] diagnostic 可选独立诊断槽；入口先清零，不得与 request、policy 或 result 重叠。
 *
 * ## Returns
 * @return 合法请求可被描述为 VECTOR，或允许回退时可被描述为 SCALAR_FALLBACK，均返回 TRUE；输出为空、请求形状错误或禁止所需标量回退时返回 FALSE。FALSE 不承诺保留旧 result：它可能为初始化默认值，也可能已填充完整 fallback 说明。成功回退仍保持诊断 NONE。
 *
 * ## Constraints and lifecycle
 * 非空输入须在调用期间稳定可读，输出须有正确对齐和完整可写存储；诊断槽须与输入及其他输出分离。实现不加锁，共享只读输入可以并行读取，写入或释放同一记录须由调用方同步。 lanes 须为 1..4096；length 为 UInt64，向量计数使用除法，tail 为模后的 UInt32，因余数小于 lanes 而不会截断有效余数；本接口不计算 length*sizeof(element) 或检查真实缓冲区大小。0 长度是合法请求，当前不单独产生 EMPTY_INPUT 原因。对齐只检查元数据为 0 或 2 的幂，不认证真实地址；能力来自调用方而非 CPU 探测。整数种类要求通用 INTEGER 能力，单独宽度位不能替代它。请求 policy 仅同步借用，结果不保留地址；其三个身份哈希与位置只是复制/混合，不核验实际函数、布局或代码。必须检查返回值及 decision，不可把所有 FALSE 输出当事务回滚，也不可把成功规划等同实际 SIMD/算术语义已经执行。 当前归约顺序门禁只检查 preserveSourceOrder 与 STRICT_ORDERED，其他变换位通过调用方填写的 requiredFastMath 检查；不得把本实现描述成总会自动要求 UNORDERED_REDUCTION 位。
 *
 * TODO: zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:330 与 zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:341–348 需核对归约许可边界：当前只核 requiredFastMath 子集和源顺序/确定性标签；relaxed 且 preserveSourceOrder=FALSE、requiredFastMath=0 时没有显式要求 UNORDERED_REDUCTION 位。tests/parser/test_ssa_numeric_vector_ir.c:252–259 仅覆盖显式有该位的正例，缺少无该位组合；docs/instruction-generation/numeric-vector-contract.md:44 及 docs/instruction-generation/numeric-vector-contract.md:104–106 的许可措辞需与调用协议核对。尚未执行该组合，不据此标运行时 BUG。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:241
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:257
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:274
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:288
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:335
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:341
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:353
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:362
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:364
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:366
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:377
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:379
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:387
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:394
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:406
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:420
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:424
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:425
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_vectorize.c:691
 * - tests/parser/test_ssa_numeric_vector_ir.c:194
 * - tests/parser/test_ssa_numeric_vector_ir.c:203
 * - tests/parser/test_ssa_numeric_vector_ir.c:298
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:330
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:345
 * - tests/parser/test_ssa_numeric_vector_ir.c:252
 * - tests/parser/test_ssa_numeric_vector_ir.c:257
 * - docs/instruction-generation/numeric-vector-contract.md:44
 * - docs/instruction-generation/numeric-vector-contract.md:104
 */

/**
 * @def ZrParser_ExecIr_VectorLegalize
 *
 * ## Summary
 * 为 ZrParser_ExecIr_LegalizeVector 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZrParser_ExecIr_LegalizeVector
 *
 * ## Parameters
 * 此对象宏不定义形参；展开后的参数及方向沿用 ZrParser_ExecIr_LegalizeVector 的声明。
 *
 * ## Returns
 * 宏本身没有独立 C 返回值；作为调用使用时返回 ZrParser_ExecIr_LegalizeVector 的结果，失败输出约束也沿用该接口。
 *
 * ## Constraints and lifecycle
 * 这是无形参的对象宏，预处理后引用 ZrParser_ExecIr_LegalizeVector 的同一函数声明，不产生包装函数、额外参数求值、独立链接符号或另一份实现。作为调用或函数地址使用时，参数方向、返回、NULL/诊断别名、失败输出及借用约束完全沿用目标接口。不能因为兼容短名就省略目标接口的校验与返回值判断。当前未检出展开不代表仓外没有使用。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:370
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:365
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:241
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:257
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:274
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:288
 */

/**
 * @def ZrParser_ExecIr_Legalize_Vector
 *
 * ## Summary
 * 为 ZrParser_ExecIr_LegalizeVector 提供兼容拼写，保持既有调用方或数据构造的名称入口。
 *
 * ## Value and use
 * ZrParser_ExecIr_LegalizeVector
 *
 * ## Parameters
 * 此对象宏不定义形参；展开后的参数及方向沿用 ZrParser_ExecIr_LegalizeVector 的声明。
 *
 * ## Returns
 * 宏本身没有独立 C 返回值；作为调用使用时返回 ZrParser_ExecIr_LegalizeVector 的结果，失败输出约束也沿用该接口。
 *
 * ## Constraints and lifecycle
 * 这是无形参的对象宏，预处理后引用 ZrParser_ExecIr_LegalizeVector 的同一函数声明，不产生包装函数、额外参数求值、独立链接符号或另一份实现。作为调用或函数地址使用时，参数方向、返回、NULL/诊断别名、失败输出及借用约束完全沿用目标接口。不能因为兼容短名就省略目标接口的校验与返回值判断。当前未检出展开不代表仓外没有使用。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:371
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:365
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:241
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:257
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:274
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:288
 */

/**
 * @fn const TZrChar *ZrParser_ExecIr_NumericElementName( EZrNumericElementKind kind)
 *
 * ## Summary
 * 把元素标签映射为固定诊断拼写，供日志或优化备注展示，避免把枚举对象的内存布局当文本。
 *
 * ## Parameters
 * @param[in] kind 待显示的元素标签；已知标签含 INVALID、BOOL、整数、浮点及 MASK，超出范围也有回退字符串。
 *
 * ## Returns
 * @return 返回 NUL 结尾的只读静态字符串；合法标签返回对应拼写，越界标签返回 "invalid"。
 *
 * ## Constraints and lifecycle
 * 不分配，不返回调用方拥有的内存；不得写入或 free。借用可持续到包含该实现的模块卸载，稳定只读调用可并行。名称是诊断标签，不表示发生了数值转换、验证或目标能力检查；仓内本次未检出实际调用，公开仓外消费者未知。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:463
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:465
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:469
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:473
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:373
 */

/**
 * @fn const TZrChar *ZrParser_ExecIr_NumericLegalizationReasonName( EZrNumericLegalizationReason reason)
 *
 * ## Summary
 * 把合法化原因标签映射为固定诊断拼写，既能显示当前发出的原因，也能表示保留枚举值。
 *
 * ## Parameters
 * @param[in] reason 待显示的原因标签；范围外值使用 "invalid" 回退。
 *
 * ## Returns
 * @return 返回 NUL 结尾只读静态字符串；NONE 对应 "none"，已知原因对应固定短名，无效或越界原因对应 "invalid"。
 *
 * ## Constraints and lifecycle
 * 不分配，不修改结果状态，不转移字符串所有权；调用方不能写入或 free，借用有效到实现模块卸载。此查询不保证每个已命名原因都由当前 LegalizeVector 发出；LANE_UNSUPPORTED、EMPTY_INPUT、INVALID 当前没有对应的正常发出分支。仓内未检出调用，不能据此称为死代码。
 *
 * ## References
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:476
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:478
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:483
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c:487
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:257
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:295
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c:420
 * - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h:375
 */
