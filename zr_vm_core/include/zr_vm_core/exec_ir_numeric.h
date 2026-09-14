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
