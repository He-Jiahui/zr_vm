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

/* Noun-first spellings match the other parser-owned analysis APIs while the
 * compact camel-case names above remain the canonical ABI symbols. */
#define ZrParser_ExecIr_EscapeSummary_Init ZrParser_ExecIr_EscapeSummaryInit
#define ZrParser_ExecIr_EscapeSummary_Free ZrParser_ExecIr_EscapeSummaryFree
#define ZrParser_ExecIr_EscapeSummary_Hash ZrParser_ExecIr_EscapeInputHash
#define ZrParser_ExecIr_EscapeFact_At ZrParser_ExecIr_EscapeFactAt
#define ZrParser_ExecIr_EscapeEdge_At ZrParser_ExecIr_EscapeEdgeAt

#endif /* ZR_VM_PARSER_EXEC_IR_ESCAPE_H */
