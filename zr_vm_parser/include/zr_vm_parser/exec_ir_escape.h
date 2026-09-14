#ifndef ZR_VM_PARSER_EXEC_IR_ESCAPE_H
#define ZR_VM_PARSER_EXEC_IR_ESCAPE_H

#include <stdint.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

/*
 * ExecIR escape analysis is deliberately pointer-free in its result.  The
 * summary can therefore be retained by a pass cache or copied into a
 * diagnostic without retaining parser/runtime objects.
 */
typedef enum EZrExecIrEscapeState {
    ZR_EXEC_IR_ESCAPE_LOCAL = 0,
    /* Function/frame lifetime is distinct from a static heap lifetime. */
    ZR_EXEC_IR_ESCAPE_FUNCTION,
    ZR_EXEC_IR_ESCAPE_CALLER,
    ZR_EXEC_IR_ESCAPE_HEAP_STATIC,
    ZR_EXEC_IR_ESCAPE_UNKNOWN
} EZrExecIrEscapeState;

#define ZR_EXEC_IR_ESCAPE_STATE_LOCAL ZR_EXEC_IR_ESCAPE_LOCAL
#define ZR_EXEC_IR_ESCAPE_BLOCK ZR_EXEC_IR_ESCAPE_LOCAL
#define ZR_EXEC_IR_ESCAPE_STATE_BLOCK ZR_EXEC_IR_ESCAPE_LOCAL
#define ZR_EXEC_IR_ESCAPE_HEAP ZR_EXEC_IR_ESCAPE_HEAP_STATIC
#define ZR_EXEC_IR_ESCAPE_STATE_HEAP ZR_EXEC_IR_ESCAPE_HEAP_STATIC
#define ZR_EXEC_IR_ESCAPE_STATE_FUNCTION ZR_EXEC_IR_ESCAPE_FUNCTION
#define ZR_EXEC_IR_ESCAPE_STATE_HEAP_STATIC ZR_EXEC_IR_ESCAPE_HEAP_STATIC
#define ZR_EXEC_IR_ESCAPE_STATE_CALLER ZR_EXEC_IR_ESCAPE_CALLER
#define ZR_EXEC_IR_ESCAPE_STATE_UNKNOWN ZR_EXEC_IR_ESCAPE_UNKNOWN

typedef enum EZrExecIrEscapeKind {
    ZR_EXEC_IR_ESCAPE_KIND_VALUE_FLOW = 0,
    ZR_EXEC_IR_ESCAPE_KIND_HEAP_STORE,
    ZR_EXEC_IR_ESCAPE_KIND_RETURN,
    ZR_EXEC_IR_ESCAPE_KIND_CLOSURE_CAPTURE,
    ZR_EXEC_IR_ESCAPE_KIND_UNKNOWN_CALL,
    ZR_EXEC_IR_ESCAPE_KIND_NATIVE_CAPTURE,
    ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND,
    ZR_EXEC_IR_ESCAPE_KIND_WORKER_PARAMETER,
    ZR_EXEC_IR_ESCAPE_KIND_EXCEPTION,
    ZR_EXEC_IR_ESCAPE_KIND_UNKNOWN
} EZrExecIrEscapeKind;

/* Readable compatibility spellings used by downstream pass prototypes. */
#define ZR_EXEC_IR_ESCAPE_KIND_CAPTURE ZR_EXEC_IR_ESCAPE_KIND_CLOSURE_CAPTURE
#define ZR_EXEC_IR_ESCAPE_KIND_CLOSURE ZR_EXEC_IR_ESCAPE_KIND_CLOSURE_CAPTURE
#define ZR_EXEC_IR_ESCAPE_KIND_STORE ZR_EXEC_IR_ESCAPE_KIND_HEAP_STORE
#define ZR_EXEC_IR_ESCAPE_KIND_HEAP ZR_EXEC_IR_ESCAPE_KIND_HEAP_STORE
#define ZR_EXEC_IR_ESCAPE_KIND_NATIVE ZR_EXEC_IR_ESCAPE_KIND_NATIVE_CAPTURE
#define ZR_EXEC_IR_ESCAPE_KIND_NATIVE_RETAINED ZR_EXEC_IR_ESCAPE_KIND_NATIVE_CAPTURE
#define ZR_EXEC_IR_ESCAPE_KIND_SUSPEND ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND
#define ZR_EXEC_IR_ESCAPE_KIND_SUSPENSION ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND
#define ZR_EXEC_IR_ESCAPE_KIND_CROSS_SUSPEND ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND
#define ZR_EXEC_IR_ESCAPE_KIND_WORKER ZR_EXEC_IR_ESCAPE_KIND_WORKER_PARAMETER
#define ZR_EXEC_IR_ESCAPE_KIND_RETURNED ZR_EXEC_IR_ESCAPE_KIND_RETURN

/* The values are part of the parser/backend contract.  Region placement is
 * intentionally not produced by the first (summary-only) implementation. */
typedef enum EZrExecIrAllocationDecision {
    ZR_EXEC_IR_ALLOC_HEAP = 0,
    ZR_EXEC_IR_ALLOC_STACK,
    ZR_EXEC_IR_ALLOC_REGION
} EZrExecIrAllocationDecision;

#define ZR_EXEC_IR_ALLOCATION_HEAP ZR_EXEC_IR_ALLOC_HEAP
#define ZR_EXEC_IR_ALLOCATION_STACK ZR_EXEC_IR_ALLOC_STACK
#define ZR_EXEC_IR_ALLOCATION_REGION ZR_EXEC_IR_ALLOC_REGION

#define ZR_EXEC_IR_ESCAPE_EDGE_INVALID ((TZrUInt32)UINT32_MAX)
#define ZR_EXEC_IR_ESCAPE_SUMMARY_MAGIC ((TZrUInt32)0x45534331u)

typedef struct SZrExecIrEscapeEdge {
    TZrExecIrValueId sourceValueId;
    /* A non-zero target denotes a value-flow edge.  A zero target is an
     * observation/sink (return, call, store, suspend, ...). */
    TZrExecIrValueId targetValueId;
    TZrExecIrInstructionId instructionId;
    TZrExecIrSourceId sourceId;
    EZrExecIrEscapeKind kind;
    TZrUInt32 parentEdgeIndex;
} SZrExecIrEscapeEdge;

typedef struct SZrExecIrEscapeFact {
    TZrExecIrValueId valueId;
    EZrExecIrOwnership ownership;
    TZrExecIrInstructionId definitionInstructionId;
    TZrExecIrInstructionId lastUseInstructionId;
    TZrUInt32 allocationInstructionId;
    EZrExecIrEscapeState state;
    EZrExecIrAllocationDecision decision;
    TZrUInt32 firstReasonEdge;
    /* Half-open lifetime in instruction-id order.  Equal bounds denote an
     * empty lifetime; lastUseInstructionId remains zero when no ordinary use
     * was observed so consumers can distinguish an empty range from a value
     * whose lifetime was conservatively extended by a boundary. */
    TZrUInt32 liveStart;
    TZrUInt32 liveEnd;
    TZrBool isAllocationCandidate;
    TZrBool identityObserved;
    TZrBool crossesSuspend;
    TZrBool nativeRetained;
    TZrBool workerEscaped;
    TZrBool dropObservable;
} SZrExecIrEscapeFact;

typedef struct SZrExecIrEscapeSummary {
    TZrUInt32 magic;
    SZrExecIrEscapeFact *facts;
    TZrUInt32 factCount;
    TZrUInt32 factCapacity;
    TZrUInt32 allocationCount;
    SZrExecIrEscapeEdge *edges;
    TZrUInt32 edgeCount;
    TZrUInt32 edgeCapacity;
    TZrUInt64 irHash;
} SZrExecIrEscapeSummary;

/*
 * Allocation and ownership passes consume an escape summary through a
 * pointer-free, hash-bound plan.  The current ExecIR schema has no physical
 * stack/region allocation opcode or placement field, so an allocation plan is
 * deliberately a backend-facing publication contract: applying it validates
 * that the IR is unchanged and leaves the semantic ALLOC instruction intact.
 * A later frame/GC lowering pass can consume the same plan without having to
 * rediscover escape facts.
 */
#define ZR_EXEC_IR_ALLOCATION_PLAN_MAGIC ((TZrUInt32)0x414c5031u)
#define ZR_EXEC_IR_OWNERSHIP_PLAN_MAGIC ((TZrUInt32)0x4f574e31u)

typedef enum EZrExecIrAllocationReason {
    ZR_EXEC_IR_ALLOCATION_REASON_PROVEN_LOCAL = 0,
    ZR_EXEC_IR_ALLOCATION_REASON_ESCAPE,
    ZR_EXEC_IR_ALLOCATION_REASON_IDENTITY_OBSERVED,
    ZR_EXEC_IR_ALLOCATION_REASON_CROSSED_SUSPEND,
    ZR_EXEC_IR_ALLOCATION_REASON_NATIVE_RETAINED,
    ZR_EXEC_IR_ALLOCATION_REASON_DROP_OBSERVABLE,
    ZR_EXEC_IR_ALLOCATION_REASON_UNKNOWN_ESCAPE,
    ZR_EXEC_IR_ALLOCATION_REASON_NO_LAYOUT,
    ZR_EXEC_IR_ALLOCATION_REASON_NOT_CANDIDATE,
    ZR_EXEC_IR_ALLOCATION_REASON_MATERIALIZATION,
    ZR_EXEC_IR_ALLOCATION_REASON_INVALID,
    ZR_EXEC_IR_ALLOCATION_REASON_COUNT
} EZrExecIrAllocationReason;

typedef struct SZrExecIrAllocationSite {
    TZrExecIrValueId valueId;
    TZrExecIrInstructionId instructionId;
    EZrExecIrAllocationDecision decision;
    EZrExecIrAllocationReason reason;
    TZrUInt32 liveStart;
    TZrUInt32 liveEnd;
    TZrBool requiresGcRoot;
    TZrBool requiresMaterialization;
    TZrBool identityPreserved;
} SZrExecIrAllocationSite;

typedef struct SZrExecIrAllocationPlan {
    TZrUInt32 magic;
    TZrUInt64 irHash;
    SZrExecIrAllocationSite *sites;
    TZrUInt32 siteCount;
    TZrUInt32 siteCapacity;
    TZrUInt32 heapCount;
    TZrUInt32 stackCount;
    TZrUInt32 regionCount;
    TZrBool hasUnknownEscape;
} SZrExecIrAllocationPlan;

typedef enum EZrExecIrOwnershipElisionKind {
    ZR_EXEC_IR_OWNERSHIP_ELISION_NONE = 0,
    ZR_EXEC_IR_OWNERSHIP_ELISION_COPY_TO_MOVE,
    ZR_EXEC_IR_OWNERSHIP_ELISION_RETURN_FORWARD,
    ZR_EXEC_IR_OWNERSHIP_ELISION_KIND_COUNT
} EZrExecIrOwnershipElisionKind;

typedef enum EZrExecIrOwnershipElisionReason {
    ZR_EXEC_IR_OWNERSHIP_REASON_UNIQUE_LAST_USE = 0,
    ZR_EXEC_IR_OWNERSHIP_REASON_RETURN_FORWARD,
    ZR_EXEC_IR_OWNERSHIP_REASON_UNKNOWN_OWNERSHIP,
    ZR_EXEC_IR_OWNERSHIP_REASON_SOURCE_ESCAPES,
    ZR_EXEC_IR_OWNERSHIP_REASON_DROP_OBSERVABLE,
    ZR_EXEC_IR_OWNERSHIP_REASON_METADATA_USE,
    ZR_EXEC_IR_OWNERSHIP_REASON_ALIAS_UNPROVEN,
    ZR_EXEC_IR_OWNERSHIP_REASON_NOT_LAST_USE,
    ZR_EXEC_IR_OWNERSHIP_REASON_INVALID,
    ZR_EXEC_IR_OWNERSHIP_REASON_COUNT
} EZrExecIrOwnershipElisionReason;

typedef struct SZrExecIrOwnershipElision {
    TZrExecIrInstructionId instructionId;
    TZrExecIrValueId sourceValueId;
    TZrExecIrValueId destinationValueId;
    EZrExecIrOwnershipElisionKind kind;
    EZrExecIrOwnershipElisionReason reason;
} SZrExecIrOwnershipElision;

typedef struct SZrExecIrOwnershipElisionPlan {
    TZrUInt32 magic;
    TZrUInt64 irHash;
    SZrExecIrOwnershipElision *items;
    TZrUInt32 itemCount;
    TZrUInt32 itemCapacity;
    TZrBool changed;
} SZrExecIrOwnershipElisionPlan;

ZR_PARSER_API void ZrParser_ExecIr_EscapeSummaryInit(
        SZrExecIrEscapeSummary *summary);
ZR_PARSER_API void ZrParser_ExecIr_EscapeSummaryFree(
        SZrExecIrEscapeSummary *summary);
ZR_PARSER_API TZrBool ZrParser_ExecIr_AnalyzeEscape(
        const SZrExecIrFunction *function,
        SZrExecIrEscapeSummary *summary,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_EscapeInputHash(
        const SZrExecIrFunction *function);
ZR_PARSER_API const SZrExecIrEscapeFact *ZrParser_ExecIr_EscapeFactAt(
        const SZrExecIrEscapeSummary *summary, TZrExecIrValueId valueId);
ZR_PARSER_API const SZrExecIrEscapeEdge *ZrParser_ExecIr_EscapeEdgeAt(
        const SZrExecIrEscapeSummary *summary, TZrUInt32 edgeIndex);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_EscapeKindName(
        EZrExecIrEscapeKind kind);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_EscapeStateName(
        EZrExecIrEscapeState state);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_AllocationDecisionName(
        EZrExecIrAllocationDecision decision);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_AllocationReasonName(
        EZrExecIrAllocationReason reason);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_OwnershipElisionReasonName(
        EZrExecIrOwnershipElisionReason reason);

ZR_PARSER_API void ZrParser_ExecIr_AllocationPlanInit(
        SZrExecIrAllocationPlan *plan);
ZR_PARSER_API void ZrParser_ExecIr_AllocationPlanFree(
        SZrExecIrAllocationPlan *plan);
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildAllocationPlan(
        const SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        SZrExecIrAllocationPlan *plan,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_AnalyzeAllocation(
        const SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        SZrExecIrAllocationPlan *plan,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ApplyAllocationPlan(
        SZrExecIrFunction *function,
        const SZrExecIrAllocationPlan *plan,
        SZrExecIrDiagnostic *diagnostic);

ZR_PARSER_API void ZrParser_ExecIr_OwnershipElisionPlanInit(
        SZrExecIrOwnershipElisionPlan *plan);
ZR_PARSER_API void ZrParser_ExecIr_OwnershipElisionPlanFree(
        SZrExecIrOwnershipElisionPlan *plan);
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildOwnershipElisionPlan(
        const SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        SZrExecIrOwnershipElisionPlan *plan,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ApplyOwnershipElisionPlan(
        SZrExecIrFunction *function,
        const SZrExecIrOwnershipElisionPlan *plan,
        TZrBool *changed,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ElideAllocationAndCopies(
        SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        SZrExecIrDiagnostic *diagnostic);

/* Noun-first spellings match the other parser-owned analysis APIs while the
 * compact camel-case names above remain the canonical ABI symbols. */
#define ZrParser_ExecIr_EscapeSummary_Init ZrParser_ExecIr_EscapeSummaryInit
#define ZrParser_ExecIr_EscapeSummary_Free ZrParser_ExecIr_EscapeSummaryFree
#define ZrParser_ExecIr_EscapeSummary_Hash ZrParser_ExecIr_EscapeInputHash
#define ZrParser_ExecIr_EscapeFact_At ZrParser_ExecIr_EscapeFactAt
#define ZrParser_ExecIr_EscapeEdge_At ZrParser_ExecIr_EscapeEdgeAt

#endif /* ZR_VM_PARSER_EXEC_IR_ESCAPE_H */
