#ifndef ZR_VM_PARSER_EXEC_IR_PASS_MANAGER_H
#define ZR_VM_PARSER_EXEC_IR_PASS_MANAGER_H

#include "zr_vm_parser/exec_ir_builder.h"

typedef enum EZrExecIrAnalysisKind {
    ZR_EXEC_IR_ANALYSIS_DOMINATORS = 1u << 0u,
    ZR_EXEC_IR_ANALYSIS_LOOPS = 1u << 1u,
    ZR_EXEC_IR_ANALYSIS_LIVENESS = 1u << 2u,
    ZR_EXEC_IR_ANALYSIS_SCCP = 1u << 3u
} EZrExecIrAnalysisKind;

#define ZR_EXEC_IR_ANALYSIS_COUNT ((TZrUInt32)4u)

#define ZR_EXEC_IR_ANALYSIS_ALL \
    ((TZrUInt32)(ZR_EXEC_IR_ANALYSIS_DOMINATORS | ZR_EXEC_IR_ANALYSIS_LOOPS | \
                 ZR_EXEC_IR_ANALYSIS_LIVENESS | ZR_EXEC_IR_ANALYSIS_SCCP))

typedef enum EZrExecIrSccpLatticeKind {
    ZR_EXEC_IR_SCCP_UNKNOWN = 0,
    ZR_EXEC_IR_SCCP_CONSTANT,
    ZR_EXEC_IR_SCCP_OVERDEFINED,
    /* A checked arithmetic operation is known to raise on every execution
     * reaching its definition.  It is not a replacement value and must not
     * be folded away. */
    ZR_EXEC_IR_SCCP_MUST_THROW
} EZrExecIrSccpLatticeKind;

typedef struct SZrExecIrSccpValue {
    EZrExecIrSccpLatticeKind kind;
    TZrUInt64 bits;
    TZrExecIrTypeToken typeToken;
} SZrExecIrSccpValue;

typedef struct SZrExecIrAnalysisCache {
    TZrUInt64 revision;
    /* Hash of the IR (and, for module runs, the constant input) for which
     * these facts were computed.  A caller may reuse a cache across runs;
     * validity is never inferred from a changed bit alone. */
    TZrUInt64 irHash;
    TZrUInt32 validMask;
    SZrExecIrSccpValue *sccpValues;
    TZrUInt32 sccpValueCount;
} SZrExecIrAnalysisCache;

typedef struct SZrExecIrPassBudget {
    TZrUInt64 maxWork;
    TZrUInt64 maxInstructions;
} SZrExecIrPassBudget;

typedef enum EZrExecIrRemarkOutcome {
    ZR_EXEC_IR_REMARK_SUCCESS = 0,
    ZR_EXEC_IR_REMARK_MISSED,
    ZR_EXEC_IR_REMARK_BLOCKED
} EZrExecIrRemarkOutcome;

typedef struct SZrExecIrOptimizationRemark {
    const TZrChar *pass;
    TZrExecIrSourceId sourceId;
    EZrExecIrRemarkOutcome outcome;
    TZrUInt32 reasonCode;
    TZrUInt64 beforeHash;
    TZrUInt64 afterHash;
    /* clock() ticks spent in this pass.  It is a diagnostic metric, not a
     * wall-clock deadline or a source of optimization decisions. */
    TZrUInt64 elapsedTicks;
} SZrExecIrOptimizationRemark;

typedef struct SZrExecIrRemarkSink {
    SZrExecIrOptimizationRemark *items;
    TZrUInt32 count;
    TZrUInt32 capacity;
} SZrExecIrRemarkSink;

typedef struct SZrExecIrPassContext {
    SZrExecIrAnalysisCache *cache;
    SZrExecIrRemarkSink *remarks;
    const SZrExecIrPassBudget *budget;
    TZrUInt64 workUsed;
    void *scratchArena;
    /* Optional module constant view used by the strict SCCP evaluator.  A
     * function-only caller can leave this pair null/zero, in which case the
     * instruction's immediate fallback representation is used. */
    const SZrExecIrConstant *constants;
    TZrUInt32 constantCount;
    /* Set by a pass when it stopped without an IR error.  The manager keeps
     * the last valid IR and reports a blocked remark in this case. */
    TZrBool budgetExhausted;
    TZrUInt32 lastReasonCode;
    TZrExecIrSourceId lastSourceId;
    TZrUInt32 passesRun;
} SZrExecIrPassContext;

#define ZR_EXEC_IR_PASS_REASON_NONE ((TZrUInt32)0u)
#define ZR_EXEC_IR_PASS_REASON_BUDGET ((TZrUInt32)1u)
#define ZR_EXEC_IR_PASS_REASON_UNSUPPORTED ((TZrUInt32)2u)
#define ZR_EXEC_IR_PASS_REASON_NO_CHANGE ((TZrUInt32)3u)
#define ZR_EXEC_IR_PASS_REASON_REQUIREMENT ((TZrUInt32)4u)
#define ZR_EXEC_IR_PASS_REASON_VERIFIER ((TZrUInt32)5u)
#define ZR_EXEC_IR_PASS_REASON_WILL_THROW ((TZrUInt32)6u)

/*
 * Module optimization deliberately uses a small option/result contract.
 * A zero-initialized options object enables the complete scalar pipeline.
 */
#define ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP ((TZrUInt32)1u << 0u)
#define ZR_EXEC_IR_OPTIMIZE_DISABLE_DCE ((TZrUInt32)1u << 1u)
#define ZR_EXEC_IR_OPTIMIZE_DISABLE_KNOWN_MASK \
    (ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP | ZR_EXEC_IR_OPTIMIZE_DISABLE_DCE)

typedef struct SZrExecIrOptimizeOptions {
    TZrUInt32 disabledPassMask;
    const SZrExecIrPassBudget *budget;
    SZrExecIrRemarkSink *remarks;
} SZrExecIrOptimizeOptions;

typedef struct SZrExecIrOptimizationResult {
    TZrUInt32 functionsVisited;
    TZrUInt32 functionsChanged;
    TZrUInt32 passesRun;
    TZrBool budgetExhausted;
    TZrUInt64 beforeHash;
    TZrUInt64 afterHash;
} SZrExecIrOptimizationResult;

typedef TZrBool (*TZrExecIrPassRun)(SZrExecIrFunction *, SZrExecIrPassContext *, TZrBool *, SZrExecIrDiagnostic *);

typedef struct SZrExecIrPassInfo {
    const TZrChar *name;
    union { TZrUInt32 requiresAnalysis; TZrUInt32 requires; };
    union { TZrUInt32 preservesAnalysis; TZrUInt32 preservedAnalysisMask; };
    TZrUInt32 invalidatesAnalysis;
    TZrExecIrPassRun run;
} SZrExecIrPassInfo;

ZR_PARSER_API void ZrParser_ExecIr_AnalysisCacheInit(SZrExecIrAnalysisCache *cache);
ZR_PARSER_API void ZrParser_ExecIr_AnalysisCacheFree(SZrExecIrAnalysisCache *cache);
ZR_PARSER_API void ZrParser_ExecIr_RemarkSinkInit(SZrExecIrRemarkSink *sink);
ZR_PARSER_API void ZrParser_ExecIr_RemarkSinkFree(SZrExecIrRemarkSink *sink);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_FunctionHash(
        const SZrExecIrFunction *function);
ZR_PARSER_API TZrBool ZrParser_ExecIr_EmitRemark(
        SZrExecIrRemarkSink *sink, const SZrExecIrOptimizationRemark *remark,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_RunPassPipeline(
        SZrExecIrFunction *function, const SZrExecIrPassInfo *passes,
        TZrUInt32 passCount, SZrExecIrPassContext *context,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API const SZrExecIrPassInfo *ZrParser_ExecIr_GetScalarPasses(TZrUInt32 *count);
ZR_PARSER_API TZrBool ZrParser_ExecIr_OptimizeScalar(
        SZrExecIrFunction *function, const SZrExecIrPassBudget *budget,
        SZrExecIrRemarkSink *remarks, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_Optimize(
        SZrExecIrModule *module, const SZrExecIrOptimizeOptions *options,
        SZrExecIrOptimizationResult *result, SZrExecIrDiagnostic *diagnostic);

#endif
