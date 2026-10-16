#ifndef ZR_VM_PARSER_EXEC_IR_INTERPROCEDURAL_H
#define ZR_VM_PARSER_EXEC_IR_INTERPROCEDURAL_H

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_parser/conf.h"

/*
 * Cross-function optimization facts are deliberately represented by scalar
 * identities only.  A call graph can therefore be retained in an analysis
 * cache (or written to a diagnostic) without retaining a runtime function
 * pointer or a pointer into a module arena.
 */

#define ZR_EXEC_IR_CALL_GRAPH_VERSION ((TZrUInt32)1u)

/* Conservative first-pass policy defaults.  They are intentionally parser
 * policy constants rather than runtime limits: callers can override them via
 * SZrExecIrInlineBudget before building a graph. */
#define ZR_EXEC_IR_INLINE_DEFAULT_MAX_COST_PER_CALL ((TZrUInt32)8u)
#define ZR_EXEC_IR_INLINE_DEFAULT_MAX_GROWTH_PER_FUNCTION ((TZrUInt32)64u)
#define ZR_EXEC_IR_INLINE_DEFAULT_MAX_GROWTH_PER_MODULE ((TZrUInt32)256u)
#define ZR_EXEC_IR_INLINE_DEFAULT_MAX_RECURSIVE_DEPTH ((TZrUInt32)0u)

/* Optional compact encoding used by parser-only fixtures when no binding
 * facts row table is attached.  The low bits carry an in-module function id;
 * high bits preserve the original dispatch shape.  Production projections
 * may instead leave bindingRow as a row index and provide metadata tokens. */
#define ZR_EXEC_IR_BINDING_TARGET_MASK ((TZrUInt32)0x1fffffffu)
#define ZR_EXEC_IR_BINDING_HINT_VIRTUAL ((TZrUInt32)0x20000000u)
#define ZR_EXEC_IR_BINDING_HINT_INTERFACE ((TZrUInt32)0x40000000u)
#define ZR_EXEC_IR_BINDING_HINT_TYPED ((TZrUInt32)0x80000000u)

typedef enum EZrExecIrCallEdgeKind {
    ZR_EXEC_IR_CALL_EDGE_UNKNOWN = 0,
    ZR_EXEC_IR_CALL_EDGE_DIRECT,
    ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT,
    ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT,
    ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION,
    ZR_EXEC_IR_CALL_EDGE_NATIVE
} EZrExecIrCallEdgeKind;

/* Short aliases used by pass diagnostics and older prototype notes. */
#define ZR_EXEC_IR_CALL_EDGE_VIRTUAL ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT
#define ZR_EXEC_IR_CALL_EDGE_INTERFACE ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT
#define ZR_EXEC_IR_CALL_EDGE_TYPED ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION
#define ZR_EXEC_IR_CALL_EDGE_STATIC ZR_EXEC_IR_CALL_EDGE_DIRECT
#define ZR_EXEC_IR_CALL_EDGE_KIND_UNKNOWN ZR_EXEC_IR_CALL_EDGE_UNKNOWN
#define ZR_EXEC_IR_CALL_EDGE_KIND_DIRECT ZR_EXEC_IR_CALL_EDGE_DIRECT
#define ZR_EXEC_IR_CALL_EDGE_KIND_VIRTUAL ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT
#define ZR_EXEC_IR_CALL_EDGE_KIND_INTERFACE ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT
#define ZR_EXEC_IR_CALL_EDGE_KIND_TYPED ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION
#define ZR_EXEC_IR_CALL_EDGE_KIND_NATIVE ZR_EXEC_IR_CALL_EDGE_NATIVE

typedef enum EZrExecIrSummaryValidity {
    ZR_EXEC_IR_SUMMARY_UNKNOWN = 0,
    /* The body and all imported summaries were verified and hash matched. */
    ZR_EXEC_IR_SUMMARY_STABLE,
    /* A result is usable for diagnostics, but contains conservative unknown
     * effects and must not be used as an optimization proof. */
    ZR_EXEC_IR_SUMMARY_CONSERVATIVE,
    ZR_EXEC_IR_SUMMARY_INVALID
} EZrExecIrSummaryValidity;

typedef enum EZrExecIrSummaryUnknownReason {
    ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE = 0,
    ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE,
    ZR_EXEC_IR_SUMMARY_UNKNOWN_INDIRECT,
    ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY
} EZrExecIrSummaryUnknownReason;

#define ZR_EXEC_IR_SUMMARY_VALIDITY_UNKNOWN ZR_EXEC_IR_SUMMARY_UNKNOWN
#define ZR_EXEC_IR_SUMMARY_VALIDITY_STABLE ZR_EXEC_IR_SUMMARY_STABLE
#define ZR_EXEC_IR_SUMMARY_VALIDITY_CONSERVATIVE ZR_EXEC_IR_SUMMARY_CONSERVATIVE
#define ZR_EXEC_IR_SUMMARY_VALIDITY_INVALID ZR_EXEC_IR_SUMMARY_INVALID
#define ZR_EXEC_IR_SUMMARY_UNKNOWN_REASON_NONE ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE
#define ZR_EXEC_IR_SUMMARY_UNKNOWN_REASON_NATIVE ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE
#define ZR_EXEC_IR_SUMMARY_UNKNOWN_REASON_INDIRECT ZR_EXEC_IR_SUMMARY_UNKNOWN_INDIRECT
#define ZR_EXEC_IR_SUMMARY_UNKNOWN_REASON_INVALID_BODY \
    ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY
#define ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE_EFFECTS \
    ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE
#define ZR_EXEC_IR_SUMMARY_UNKNOWN_INDIRECT_CALL \
    ZR_EXEC_IR_SUMMARY_UNKNOWN_INDIRECT

typedef enum EZrExecIrInlineBlockReason {
    ZR_EXEC_IR_INLINE_REASON_NONE = 0,
    ZR_EXEC_IR_INLINE_REASON_PATCHABLE_TARGET,
    ZR_EXEC_IR_INLINE_REASON_RECURSIVE,
    ZR_EXEC_IR_INLINE_REASON_COST,
    ZR_EXEC_IR_INLINE_REASON_EFFECTS,
    ZR_EXEC_IR_INLINE_REASON_STATE_MAP,
    ZR_EXEC_IR_INLINE_REASON_SIGNATURE,
    ZR_EXEC_IR_INLINE_REASON_UNSUPPORTED,
    ZR_EXEC_IR_INLINE_REASON_INVALID
} EZrExecIrInlineBlockReason;

#define ZR_EXEC_IR_INLINE_BLOCK_NONE ZR_EXEC_IR_INLINE_REASON_NONE
#define ZR_EXEC_IR_INLINE_BLOCK_PATCHABLE_TARGET \
    ZR_EXEC_IR_INLINE_REASON_PATCHABLE_TARGET
#define ZR_EXEC_IR_INLINE_BLOCK_RECURSIVE ZR_EXEC_IR_INLINE_REASON_RECURSIVE
#define ZR_EXEC_IR_INLINE_BLOCK_COST ZR_EXEC_IR_INLINE_REASON_COST
#define ZR_EXEC_IR_INLINE_BLOCK_EFFECTS ZR_EXEC_IR_INLINE_REASON_EFFECTS
#define ZR_EXEC_IR_INLINE_BLOCK_STATE_MAP ZR_EXEC_IR_INLINE_REASON_STATE_MAP
#define ZR_EXEC_IR_INLINE_BLOCK_SIGNATURE ZR_EXEC_IR_INLINE_REASON_SIGNATURE
#define ZR_EXEC_IR_INLINE_BLOCK_UNSUPPORTED ZR_EXEC_IR_INLINE_REASON_UNSUPPORTED
#define ZR_EXEC_IR_INLINE_BLOCK_INVALID ZR_EXEC_IR_INLINE_REASON_INVALID

/* Summary effects use the shared execution-contract bits.  Keeping aliases
 * here makes call-graph users independent of the spelling used by runtime
 * contract consumers. */
#define ZR_EXEC_IR_SUMMARY_EFFECT_READ ZR_EXECUTION_EFFECT_READ_MEMORY
#define ZR_EXEC_IR_SUMMARY_EFFECT_WRITE ZR_EXECUTION_EFFECT_WRITE_MEMORY
#define ZR_EXEC_IR_SUMMARY_EFFECT_ALLOCATE ZR_EXECUTION_EFFECT_ALLOCATE
#define ZR_EXEC_IR_SUMMARY_EFFECT_THROW ZR_EXECUTION_EFFECT_THROW
#define ZR_EXEC_IR_SUMMARY_EFFECT_SUSPEND ZR_EXECUTION_EFFECT_SUSPEND
#define ZR_EXEC_IR_SUMMARY_EFFECT_ALL ZR_EXECUTION_EFFECT_KNOWN_MASK
#define ZR_EXEC_IR_SUMMARY_EFFECT_READ_MEMORY ZR_EXEC_IR_SUMMARY_EFFECT_READ
#define ZR_EXEC_IR_SUMMARY_EFFECT_WRITE_MEMORY ZR_EXEC_IR_SUMMARY_EFFECT_WRITE
#define ZR_EXEC_IR_SUMMARY_EFFECT_ALLOC ZR_EXEC_IR_SUMMARY_EFFECT_ALLOCATE

typedef struct SZrExecIrFunctionSummary {
    TZrExecIrFunctionId functionId;
    TZrMetadataToken functionToken;
    /* Published contract identity may be relocated from the local function
     * token.  BuildCallGraph fills this when contract.targetToken is set;
     * zero remains a backward-compatible spelling for functionToken. */
    TZrMetadataToken targetToken;
    TZrUInt64 signatureHash;
    TZrUInt64 targetGeneration;
    union {
        TZrUInt64 bodyHash;
        TZrUInt64 irHash;
    };
    union {
        TZrUInt64 importedHash;
        TZrUInt64 dependencyHash;
    };
    union {
        TZrUInt64 summaryHash;
        TZrUInt64 stableHash;
    };
    TZrUInt32 effects;
    TZrUInt32 sccId;
    TZrUInt32 fixedPointIterations;
    EZrExecIrSummaryUnknownReason unknownReason;
    TZrExecIrInstructionId firstUnknownInstructionId;
    EZrExecIrSummaryValidity validity;
    TZrBool pure;
    TZrBool receiverMutates;
    TZrBool allocates;
    TZrBool mayThrow;
    TZrBool maySuspend;
    TZrBool mayEscape;
    TZrBool sendSync;
    TZrBool unknownEffects;
    TZrBool targetFrozen;
    TZrBool patchable;
} SZrExecIrFunctionSummary;

/* Descriptive aliases used by callers that name the enclosing graph in the
 * record type (the representation is identical and remains pointer-free). */
typedef SZrExecIrFunctionSummary SZrExecIrCallGraphSummary;

typedef struct SZrExecIrCallEdge {
    TZrExecIrFunctionId callerId;
    TZrExecIrInstructionId callInstructionId;
    TZrExecIrFunctionId calleeId;
    union {
        TZrMetadataToken targetToken;
        TZrMetadataToken targetMetadataToken;
    };
    union {
        TZrUInt64 expectedSignatureHash;
        TZrUInt64 signatureHash;
    };
    union {
        TZrUInt64 targetGeneration;
        TZrUInt64 generation;
    };
    EZrExecIrCallEdgeKind kind;
    TZrBool resolved;
    TZrBool exactReceiver;
    TZrBool guarded;
    TZrBool patchableTarget;
    TZrBool nativeEffectsUnknown;
    TZrBool inlineEligible;
    EZrExecIrInlineBlockReason inlineReason;
} SZrExecIrCallEdge;

typedef SZrExecIrCallEdge SZrExecIrCallGraphEdge;

typedef struct SZrExecIrInlineBudget {
    TZrUInt32 maxCostPerCall;
    TZrUInt32 maxGrowthPerFunction;
    TZrUInt32 maxGrowthPerModule;
    TZrUInt32 maxRecursiveDepth;
} SZrExecIrInlineBudget;

typedef SZrExecIrInlineBudget SZrExecIrInlinePolicy;

typedef struct SZrExecIrCallTargetRequest {
    EZrCallBindingKind bindingKind;
    TZrExecIrFunctionId targetFunctionId;
    TZrMetadataToken targetToken;
    TZrUInt64 signatureHash;
    TZrUInt64 moduleHash;
    TZrUInt64 generation;
    TZrUInt32 dispatchSlot;
    TZrExecIrTypeToken receiverTypeToken;
    TZrExecIrTypeToken expectedReceiverTypeToken;
    TZrBool exactReceiver;
    TZrBool profileMonomorphic;
    TZrBool deoptMapAvailable;
    TZrBool targetFrozen;
    TZrBool targetPatchable;
    TZrBool closureContextValid;
} SZrExecIrCallTargetRequest;

typedef SZrExecIrCallTargetRequest SZrExecIrTargetRequest;

typedef struct SZrExecIrCallTargetResult {
    EZrExecIrCallEdgeKind kind;
    TZrExecIrFunctionId targetFunctionId;
    TZrUInt32 dispatchSlot;
    TZrBool resolved;
    TZrBool guarded;
    TZrBool inlineAllowed;
    EZrExecIrInlineBlockReason inlineReason;
} SZrExecIrCallTargetResult;

typedef SZrExecIrCallTargetResult SZrExecIrTargetResult;

typedef struct SZrExecIrCallGraph {
    TZrUInt32 schemaVersion;
    TZrUInt64 moduleHash;
    TZrUInt64 graphHash;
    TZrUInt64 revision;
    SZrExecIrFunctionSummary *summaries;
    TZrUInt32 summaryCount;
    TZrUInt32 summaryCapacity;
    SZrExecIrCallEdge *edges;
    TZrUInt32 edgeCount;
    TZrUInt32 edgeCapacity;
    SZrExecIrInlineBudget inlineBudget;
} SZrExecIrCallGraph;

ZR_PARSER_API void ZrParser_ExecIr_CallGraphInit(SZrExecIrCallGraph *graph);
ZR_PARSER_API void ZrParser_ExecIr_CallGraphFree(SZrExecIrCallGraph *graph);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_CallGraphHash(
        const SZrExecIrCallGraph *graph);
ZR_PARSER_API TZrBool ZrParser_ExecIr_CallGraphValidate(
        const SZrExecIrCallGraph *graph, const SZrExecIrModule *module,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API const SZrExecIrFunctionSummary *ZrParser_ExecIr_CallGraphSummaryAt(
        const SZrExecIrCallGraph *graph, TZrExecIrFunctionId functionId);
ZR_PARSER_API const SZrExecIrCallEdge *ZrParser_ExecIr_CallGraphEdgeAt(
        const SZrExecIrCallGraph *graph, TZrUInt32 edgeIndex);
ZR_PARSER_API TZrBool ZrParser_ExecIr_CallGraphSetInlineBudget(
        SZrExecIrCallGraph *graph, const SZrExecIrInlineBudget *budget,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ResolveCallTarget(
        const SZrExecIrCallGraph *graph,
        const SZrExecIrCallTargetRequest *request,
        SZrExecIrCallTargetResult *result,
        SZrExecIrDiagnostic *diagnostic);

/* BuildCallGraph performs call-site resolution, SCC discovery, and the
 * monotone summary fixed point.  A unique layoutId metadata match is
 * preferred as the static token proof used by CallBinding facts.  typeToken
 * is normally a result/callable type; it is considered only as an indirect
 * candidate when a virtual/interface/typed hint is present.  For the
 * lightweight model, a bindingRow equal to an in-module function id is also
 * accepted when no target metadata is present.  An unmatched target token
 * never falls through to a row-id guess.  All other calls are unknown/native
 * and receive conservative effects. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildCallGraph(
        SZrExecIrModule *module, SZrExecIrCallGraph *graph,
        SZrExecIrDiagnostic *diagnostic);

/* Devirtualization only rewrites a mutable, unsealed function when an exact
 * in-module target proof exists.  Otherwise the slot call remains intact and
 * the edge is marked guarded/unknown in the graph.  A target is frozen only
 * after its body is sealed and its generation is non-zero; an unsealed body
 * remains patchable even at generation one. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_DevirtualizeCalls(
        SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
        SZrExecIrDiagnostic *diagnostic);

/* InlineCalls intentionally starts with a small, fully verifiable subset:
 * frozen, effect-free callees whose body is a linear sequence of scalar value
 * operations (CONSTANT/COPY/CONVERT/arithmetic) followed by RETURN.  It
 * rewrites only the call instruction, preserving call-site source identity.
 * Calls that need cloning of cleanup/state/root/deopt maps are rejected with
 * an explicit inline reason. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_InlineCalls(
        SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
        SZrExecIrDiagnostic *diagnostic);

/* Descriptive aliases used by pass-oriented callers. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_RunDevirtualize(
        SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_RunInline(
        SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
        SZrExecIrDiagnostic *diagnostic);

/* Noun-first aliases mirror the other parser-owned analysis headers. */
#define ZrParser_ExecIr_CallGraph_Init ZrParser_ExecIr_CallGraphInit
#define ZrParser_ExecIr_CallGraph_Free ZrParser_ExecIr_CallGraphFree
#define ZrParser_ExecIr_CallGraph_Hash ZrParser_ExecIr_CallGraphHash
#define ZrParser_ExecIr_CallGraph_Validate ZrParser_ExecIr_CallGraphValidate
#define ZrParser_ExecIr_CallGraph_Build ZrParser_ExecIr_BuildCallGraph
#define ZrParser_ExecIr_CallGraph_SetInlineBudget \
    ZrParser_ExecIr_CallGraphSetInlineBudget
#define ZrParser_ExecIr_CallGraph_ResolveCallTarget \
    ZrParser_ExecIr_ResolveCallTarget
#define ZrParser_ExecIr_CallGraph_Devirtualize \
    ZrParser_ExecIr_DevirtualizeCalls
#define ZrParser_ExecIr_CallGraph_Inline ZrParser_ExecIr_InlineCalls

#endif /* ZR_VM_PARSER_EXEC_IR_INTERPROCEDURAL_H */
