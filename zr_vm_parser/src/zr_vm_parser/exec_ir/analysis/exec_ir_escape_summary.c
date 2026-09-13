#include "zr_vm_parser/exec_ir_escape.h"

#include <stdlib.h>
#include <string.h>

/* Summary storage/lifecycle is separate from the graph walk so consumers can
 * retain or query facts without pulling the analyzer's worklist helpers into
 * their own translation unit. */

/* A summary is intentionally plain C storage, so callers sometimes place it
 * in zeroed/stack memory before the first Init.  Only release buffers when the
 * complete lifecycle invariant is visible; a coincidental magic value in an
 * uninitialised object must never turn Free/Init into an invalid free. */
static TZrBool zr_escape_summary_storage_is_valid(
        const SZrExecIrEscapeSummary *summary) {
    TZrUInt32 magic = 0u;
    TZrUInt32 factCount = 0u;
    TZrUInt32 factCapacity = 0u;
    TZrUInt32 edgeCount = 0u;
    TZrUInt32 edgeCapacity = 0u;
    SZrExecIrEscapeFact *facts = ZR_NULL;
    SZrExecIrEscapeEdge *edges = ZR_NULL;

    if (summary == ZR_NULL) return ZR_FALSE;
    /* Copy object representations into initialized locals.  This makes a
     * first Init/Free on arbitrary caller storage well-defined without
     * evaluating an indeterminate scalar directly. */
    memcpy(&magic, &summary->magic, sizeof(magic));
    memcpy(&factCount, &summary->factCount, sizeof(factCount));
    memcpy(&factCapacity, &summary->factCapacity, sizeof(factCapacity));
    memcpy(&edgeCount, &summary->edgeCount, sizeof(edgeCount));
    memcpy(&edgeCapacity, &summary->edgeCapacity, sizeof(edgeCapacity));
    if (magic != ZR_EXEC_IR_ESCAPE_SUMMARY_MAGIC ||
        factCount > factCapacity || edgeCount > edgeCapacity) return ZR_FALSE;
    /* Do not materialize arbitrary pointer bytes from an uninitialized or
     * caller-corrupted object until the scalar lifecycle marker is trusted. */
    memcpy(&facts, &summary->facts, sizeof(facts));
    memcpy(&edges, &summary->edges, sizeof(edges));
    return (TZrBool)(((factCapacity == 0u && facts == ZR_NULL) ||
                      (factCapacity != 0u && facts != ZR_NULL)) &&
                     ((edgeCapacity == 0u && edges == ZR_NULL) ||
                      (edgeCapacity != 0u && edges != ZR_NULL)));
}

static void zr_escape_summary_release(SZrExecIrEscapeSummary *summary) {
    SZrExecIrEscapeFact *facts = ZR_NULL;
    SZrExecIrEscapeEdge *edges = ZR_NULL;

    if (!zr_escape_summary_storage_is_valid(summary)) return;
    memcpy(&facts, &summary->facts, sizeof(facts));
    memcpy(&edges, &summary->edges, sizeof(edges));
    free(facts);
    free(edges);
}

void ZrParser_ExecIr_EscapeSummaryInit(SZrExecIrEscapeSummary *summary) {
    if (summary == ZR_NULL) return;
    zr_escape_summary_release(summary);
    memset(summary, 0, sizeof(*summary));
    summary->magic = ZR_EXEC_IR_ESCAPE_SUMMARY_MAGIC;
}

void ZrParser_ExecIr_EscapeSummaryFree(SZrExecIrEscapeSummary *summary) {
    if (summary == ZR_NULL) return;
    zr_escape_summary_release(summary);
    memset(summary, 0, sizeof(*summary));
}

const SZrExecIrEscapeFact *ZrParser_ExecIr_EscapeFactAt(
    const SZrExecIrEscapeSummary *summary, TZrExecIrValueId valueId) {
    if (!zr_escape_summary_storage_is_valid(summary) ||
        valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
        valueId > summary->factCount || summary->facts == ZR_NULL) return ZR_NULL;
    return &summary->facts[valueId - 1u];
}

const SZrExecIrEscapeEdge *ZrParser_ExecIr_EscapeEdgeAt(
    const SZrExecIrEscapeSummary *summary, TZrUInt32 edgeIndex) {
    if (!zr_escape_summary_storage_is_valid(summary) ||
        edgeIndex >= summary->edgeCount ||
        summary->edges == ZR_NULL) {
        return ZR_NULL;
    }
    return &summary->edges[edgeIndex];
}

const TZrChar *ZrParser_ExecIr_EscapeKindName(EZrExecIrEscapeKind kind) {
    switch (kind) {
        case ZR_EXEC_IR_ESCAPE_KIND_VALUE_FLOW: return "value-flow";
        case ZR_EXEC_IR_ESCAPE_KIND_HEAP_STORE: return "heap-store";
        case ZR_EXEC_IR_ESCAPE_KIND_RETURN: return "return";
        case ZR_EXEC_IR_ESCAPE_KIND_CLOSURE_CAPTURE: return "closure-capture";
        case ZR_EXEC_IR_ESCAPE_KIND_UNKNOWN_CALL: return "unknown-call";
        case ZR_EXEC_IR_ESCAPE_KIND_NATIVE_CAPTURE: return "native-retained";
        case ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND: return "escape-across-suspend";
        case ZR_EXEC_IR_ESCAPE_KIND_WORKER_PARAMETER: return "worker-parameter";
        case ZR_EXEC_IR_ESCAPE_KIND_EXCEPTION: return "exception";
        case ZR_EXEC_IR_ESCAPE_KIND_UNKNOWN:
        default: return "unknown";
    }
}

const TZrChar *ZrParser_ExecIr_EscapeStateName(EZrExecIrEscapeState state) {
    switch (state) {
        /* Keep the public diagnostic vocabulary aligned with SemIR, where
         * the bottom escape bound is called ``block`` (the enum's LOCAL name
         * is retained for source compatibility). */
        case ZR_EXEC_IR_ESCAPE_LOCAL: return "block";
        case ZR_EXEC_IR_ESCAPE_FUNCTION: return "function";
        case ZR_EXEC_IR_ESCAPE_CALLER: return "caller";
        case ZR_EXEC_IR_ESCAPE_HEAP_STATIC: return "heap/static";
        case ZR_EXEC_IR_ESCAPE_UNKNOWN:
        default: return "unknown";
    }
}

const TZrChar *ZrParser_ExecIr_AllocationDecisionName(
        EZrExecIrAllocationDecision decision) {
    switch (decision) {
        case ZR_EXEC_IR_ALLOC_STACK: return "stack";
        case ZR_EXEC_IR_ALLOC_REGION: return "region";
        case ZR_EXEC_IR_ALLOC_HEAP:
        default: return "heap";
    }
}
