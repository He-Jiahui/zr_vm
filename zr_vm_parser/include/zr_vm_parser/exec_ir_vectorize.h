#ifndef ZR_VM_PARSER_EXEC_IR_VECTORIZE_H
#define ZR_VM_PARSER_EXEC_IR_VECTORIZE_H

/*
 * Pointer-free vectorization planning contract.
 *
 * ExecIR currently has no variable-width vector opcodes.  This pass therefore
 * produces a checked side-table plan rather than rewriting instructions.  A
 * backend may consume a VECTOR row, while every other row is an explicit
 * scalar fallback.  Keeping the decision separate from code generation makes
 * alias guards, tails, and numeric policy visible to diagnostics and avoids
 * claiming a transformation that the fixed-width IR cannot represent.
 */

#include "zr_vm_core/exec_ir_numeric.h"
#include "zr_vm_parser/exec_ir_loops.h"
#include "zr_vm_parser/exec_ir_pass_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_EXEC_IR_VECTORIZER_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_EXEC_IR_VECTORIZER_MAGIC ((TZrUInt32)0x56504331u)
#define ZR_EXEC_IR_VECTORIZER_DEFAULT_LANES ((TZrUInt32)4u)
#define ZR_EXEC_IR_VECTORIZER_DEFAULT_MIN_TRIP ((TZrUInt32)4u)
#define ZR_EXEC_IR_VECTORIZER_DEFAULT_MAX_CODE_BYTES ((TZrUInt32)4096u)
#define ZR_EXEC_IR_VECTORIZER_DEFAULT_MAX_BRIDGE_COST ((TZrUInt32)16u)
#define ZR_EXEC_IR_VECTORIZER_MAX_LANES ((TZrUInt32)4096u)

/* The shorter VECTORIZE spelling appears in a few out-of-tree pass tables. */
#define ZR_EXEC_IR_VECTORIZE_SCHEMA_VERSION ZR_EXEC_IR_VECTORIZER_SCHEMA_VERSION
#define ZR_EXEC_IR_VECTORIZE_MAGIC ZR_EXEC_IR_VECTORIZER_MAGIC
#define ZR_EXEC_IR_VECTORIZE_DEFAULT_LANES ZR_EXEC_IR_VECTORIZER_DEFAULT_LANES

typedef enum EZrExecIrVectorizeDecision {
    ZR_EXEC_IR_VECTORIZER_DECISION_NONE = 0,
    ZR_EXEC_IR_VECTORIZER_DECISION_VECTOR,
    ZR_EXEC_IR_VECTORIZER_DECISION_SCALAR_FALLBACK,
    ZR_EXEC_IR_VECTORIZER_DECISION_REJECTED,
    ZR_EXEC_IR_VECTORIZER_DECISION_COUNT
} EZrExecIrVectorizeDecision;
typedef EZrExecIrVectorizeDecision EZrExecIrVectorizationDecision;
#define ZR_EXEC_IR_VECTORIZE_DECISION_NONE ZR_EXEC_IR_VECTORIZER_DECISION_NONE
#define ZR_EXEC_IR_VECTORIZE_DECISION_VECTOR ZR_EXEC_IR_VECTORIZER_DECISION_VECTOR
#define ZR_EXEC_IR_VECTORIZE_DECISION_SCALAR_FALLBACK \
    ZR_EXEC_IR_VECTORIZER_DECISION_SCALAR_FALLBACK
#define ZR_EXEC_IR_VECTORIZE_DECISION_REJECTED \
    ZR_EXEC_IR_VECTORIZER_DECISION_REJECTED

typedef enum EZrExecIrVectorizeReason {
    ZR_EXEC_IR_VECTORIZER_REASON_NONE = 0,
    ZR_EXEC_IR_VECTORIZER_REASON_DISABLED,
    ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP,
    ZR_EXEC_IR_VECTORIZER_REASON_NO_PREHEADER,
    ZR_EXEC_IR_VECTORIZER_REASON_MULTIPLE_ENTRY,
    ZR_EXEC_IR_VECTORIZER_REASON_IRREDUCIBLE,
    ZR_EXEC_IR_VECTORIZER_REASON_ZERO_TRIP,
    ZR_EXEC_IR_VECTORIZER_REASON_UNKNOWN_TRIP,
    ZR_EXEC_IR_VECTORIZER_REASON_SMALL_TRIP,
    ZR_EXEC_IR_VECTORIZER_REASON_DEPENDENCE,
    ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNSAFE,
    ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNKNOWN,
    ZR_EXEC_IR_VECTORIZER_REASON_BOUNDS_UNKNOWN,
    ZR_EXEC_IR_VECTORIZER_REASON_STRIDE_UNKNOWN,
    ZR_EXEC_IR_VECTORIZER_REASON_ALIGNMENT,
    ZR_EXEC_IR_VECTORIZER_REASON_EFFECT,
    ZR_EXEC_IR_VECTORIZER_REASON_UNSUPPORTED_OPCODE,
    ZR_EXEC_IR_VECTORIZER_REASON_NUMERIC_POLICY,
    ZR_EXEC_IR_VECTORIZER_REASON_TAIL,
    ZR_EXEC_IR_VECTORIZER_REASON_COST,
    ZR_EXEC_IR_VECTORIZER_REASON_CODE_SIZE,
    ZR_EXEC_IR_VECTORIZER_REASON_BRIDGE_BUDGET,
    ZR_EXEC_IR_VECTORIZER_REASON_VERSION,
    ZR_EXEC_IR_VECTORIZER_REASON_INVALID,
    ZR_EXEC_IR_VECTORIZER_REASON_COUNT
} EZrExecIrVectorizeReason;
typedef EZrExecIrVectorizeReason EZrExecIrVectorizationReason;
#define ZR_EXEC_IR_VECTORIZE_REASON_NONE ZR_EXEC_IR_VECTORIZER_REASON_NONE
#define ZR_EXEC_IR_VECTORIZE_REASON_ALIAS_UNSAFE \
    ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNSAFE
#define ZR_EXEC_IR_VECTORIZE_REASON_ALIAS_UNKNOWN \
    ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNKNOWN
#define ZR_EXEC_IR_VECTORIZE_REASON_DEPENDENCE \
    ZR_EXEC_IR_VECTORIZER_REASON_DEPENDENCE

typedef enum EZrExecIrVectorizeAliasState {
    ZR_EXEC_IR_VECTORIZER_ALIAS_UNKNOWN = 0,
    ZR_EXEC_IR_VECTORIZER_ALIAS_DISJOINT,
    ZR_EXEC_IR_VECTORIZER_ALIAS_MAY_ALIAS,
    ZR_EXEC_IR_VECTORIZER_ALIAS_MUST_ALIAS
} EZrExecIrVectorizeAliasState;
typedef EZrExecIrVectorizeAliasState EZrExecIrVectorizationAliasState;

/* A single loop decision.  All values are logical counts; no host address is
 * retained, so the row can be serialized into an optimization remark. */
typedef struct SZrExecIrVectorizeLoopPlan {
    TZrExecIrLoopId loopId;
    TZrExecIrSourceId sourceId;
    EZrExecIrVectorizeDecision decision;
    EZrExecIrVectorizeReason reason;
    EZrExecIrVectorizeAliasState aliasState;
    EZrNumericVectorOperation operation;
    EZrNumericElementKind elementKind;
    TZrUInt64 tripCount;
    union {
        TZrUInt32 vectorLanes;
        TZrUInt32 vectorWidth;
        TZrUInt32 lanes;
    };
    TZrUInt64 vectorIterations;
    union {
        TZrUInt32 tailLanes;
        TZrUInt32 tail;
    };
    TZrUInt64 estimatedScalarCost;
    TZrUInt64 estimatedVectorCost;
    TZrUInt64 contractHash;
    TZrUInt32 estimatedCodeBytes;
    TZrUInt32 bridgeCost;
    TZrMemoryOffset strideBytes;
    TZrUInt32 elementBytes;
    TZrBool boundsProven;
    TZrBool strideProven;
    TZrBool dependenceProven;
    TZrBool needsAliasGuard;
    TZrBool usesMaskedTail;
    TZrBool usesScalarTail;
    TZrBool preservesSourceOrder;
    TZrBool reserved0;
} SZrExecIrVectorizeLoopPlan;

typedef struct SZrExecIrVectorizePlan {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt64 irHash;
    SZrExecIrVectorizeLoopPlan *loops;
    TZrUInt32 loopCount;
    TZrUInt32 loopCapacity;
    TZrUInt32 vectorizedCount;
    TZrUInt32 scalarFallbackCount;
    TZrUInt32 rejectedCount;
    TZrUInt32 totalCodeBytes;
    TZrUInt32 totalBridgeCost;
    TZrBool changed;
} SZrExecIrVectorizePlan;
typedef SZrExecIrVectorizePlan SZrExecIrVectorizationPlan;

/* Options are intentionally scalar except for borrowed inputs/sinks.  Call
 * Init before use; zero-initialized options are accepted as a shorthand for
 * defaults by VectorizeLoops as well. */
typedef struct SZrExecIrVectorizeOptions {
    TZrUInt32 schemaVersion;
    const SZrExecIrLoopInfo *loopInfo;
    SZrExecIrVectorizePlan *plan;
    const SZrNumericPolicy *numericPolicy;
    EZrNumericElementKind elementKind;
    TZrUInt32 targetVectorLanes;
    TZrUInt32 targetCapabilities;
    TZrUInt32 sourceAlignment;
    TZrUInt32 requiredAlignment;
    TZrUInt32 minTripCount;
    TZrUInt32 maxCodeBytes;
    TZrUInt32 maxBridgeCost;
    TZrUInt32 maxLoops;
    TZrMemoryOffset strideBytes;
    TZrUInt32 elementBytes;
    EZrExecIrVectorizeAliasState aliasState;
    TZrBool boundsProven;
    TZrBool strideProven;
    TZrBool dependenceProven;
    TZrBool allowOrderedStores;
    TZrBool aliasCheckable;
    TZrBool allowRuntimeAliasGuard;
    TZrBool allowMaskedTail;
    TZrBool allowScalarFallback;
    TZrBool preserveSourceOrder;
    TZrBool disableVectorization;
    TZrBool reserved0;
    SZrExecIrRemarkSink *remarks;
} SZrExecIrVectorizeOptions;
typedef SZrExecIrVectorizeOptions SZrExecIrVectorizationOptions;

ZR_PARSER_API void ZrParser_ExecIr_VectorizeOptionsInit(
        SZrExecIrVectorizeOptions *options);
ZR_PARSER_API void ZrParser_ExecIr_VectorizePlanInit(
        SZrExecIrVectorizePlan *plan);
ZR_PARSER_API void ZrParser_ExecIr_VectorizePlanFree(
        SZrExecIrVectorizePlan *plan);
ZR_PARSER_API const SZrExecIrVectorizeLoopPlan *
ZrParser_ExecIr_VectorizePlanAt(const SZrExecIrVectorizePlan *plan,
                                TZrUInt32 index);

/* Explicit form for callers that already ran loop analysis. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_VectorizeLoopsEx(
        const SZrExecIrFunction *function,
        const SZrExecIrLoopInfo *loopInfo,
        const SZrExecIrVectorizeOptions *options,
        SZrExecIrVectorizePlan *plan,
        SZrExecIrDiagnostic *diagnostic);

/* Plan's interface sketch: options->loopInfo/options->plan are optional.  A
 * missing loop analysis is computed transactionally and a missing plan still
 * performs all legality/cost checks and emits remarks. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_VectorizeLoops(
        SZrExecIrFunction *function,
        const SZrExecIrVectorizeOptions *options,
        SZrExecIrDiagnostic *diagnostic);

ZR_PARSER_API const TZrChar *ZrParser_ExecIr_VectorizeReasonName(
        EZrExecIrVectorizeReason reason);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_VectorizeDecisionName(
        EZrExecIrVectorizeDecision decision);

/* Compatibility spellings used by pass registries. */
#define ZrParser_ExecIr_VectorizeOptions_Init ZrParser_ExecIr_VectorizeOptionsInit
#define ZrParser_ExecIr_VectorizePlan_Init ZrParser_ExecIr_VectorizePlanInit
#define ZrParser_ExecIr_VectorizePlan_Free ZrParser_ExecIr_VectorizePlanFree
#define ZrParser_ExecIr_Vectorize_Loops ZrParser_ExecIr_VectorizeLoops
#define ZrParser_ExecIr_Vectorize ZrParser_ExecIr_VectorizeLoops

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_PARSER_EXEC_IR_VECTORIZE_H */
