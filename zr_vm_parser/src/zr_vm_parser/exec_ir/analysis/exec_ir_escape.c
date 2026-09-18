#include "zr_vm_parser/exec_ir_escape.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * This is the deliberately conservative first escape pass.  It consumes the
 * already canonical ExecIR only; it does not inspect type names or runtime
 * pointers.  A later allocation pass can use the facts here without having
 * to rediscover call/return/suspend boundaries.
 */

static void zr_escape_diag(SZrExecIrDiagnostic *diagnostic,
                           EZrExecutionDiagnosticCode code,
                           const SZrExecIrFunction *function,
                           TZrUInt32 instructionId,
                           TZrUInt32 sourceId) {
    TZrUInt32 index;

    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL
                                     ? function->functionToken
                                     : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = sourceId;
    diagnostic->blockId = ZR_EXEC_IR_BLOCK_ID_INVALID;
    if (function == ZR_NULL || instructionId == 0u ||
        function->blocks == ZR_NULL ||
        function->blockCount > function->blockCapacity) return;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (instructionId - 1u >= block->instructionRange.start &&
            instructionId - 1u - block->instructionRange.start <
                block->instructionRange.count) {
            diagnostic->blockId = block->id;
            return;
        }
    }
}

static void zr_escape_enrich_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                        const SZrExecIrFunction *function) {
    if (diagnostic == ZR_NULL || function == ZR_NULL ||
        function->instructionCount > function->instructionCapacity ||
        (diagnostic->instructionId != 0u &&
         (diagnostic->instructionId > function->instructionCount ||
          function->instructions == ZR_NULL))) return;
    if (diagnostic->instructionId != 0u && diagnostic->sourceId == 0u) {
        diagnostic->sourceId =
                function->instructions[diagnostic->instructionId - 1u].sourceId;
    }
    if (diagnostic->blockId == ZR_EXEC_IR_BLOCK_ID_INVALID &&
        function->blocks != ZR_NULL &&
        diagnostic->instructionId != 0u &&
        function->blockCount <= function->blockCapacity) {
        TZrUInt32 index;
        for (index = 0u; index < function->blockCount; ++index) {
            const SZrExecIrBlock *block = &function->blocks[index];
            TZrUInt32 instructionIndex = diagnostic->instructionId - 1u;
            if (instructionIndex >= block->instructionRange.start &&
                instructionIndex - block->instructionRange.start <
                    block->instructionRange.count) {
                diagnostic->blockId = block->id;
                break;
            }
        }
    }
    if (diagnostic->sourceId == 0u &&
        diagnostic->blockId != ZR_EXEC_IR_BLOCK_ID_INVALID &&
        function->blocks != ZR_NULL &&
        function->instructions != ZR_NULL &&
        function->blockCount <= function->blockCapacity) {
        TZrUInt32 index;
        for (index = 0u; index < function->blockCount; ++index) {
            const SZrExecIrBlock *block = &function->blocks[index];
            if (block->id == diagnostic->blockId &&
                block->instructionRange.count != 0u &&
                block->instructionRange.start < function->instructionCount) {
                diagnostic->sourceId =
                        function->instructions[
                                block->instructionRange.start].sourceId;
                break;
            }
        }
    }
}

static TZrBool zr_escape_count_fits(TZrUInt32 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     (size_t)count <= SIZE_MAX / elementSize);
}

static TZrBool zr_escape_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count &&
                     range.count <= count - range.start);
}

static TZrUInt32 zr_escape_instruction_after(TZrExecIrInstructionId id) {
    return id == UINT32_MAX ? UINT32_MAX : id + 1u;
}

static void zr_escape_extend_live_end(SZrExecIrEscapeFact *fact,
                                      TZrExecIrInstructionId boundaryId) {
    TZrUInt32 boundaryEnd;

    if (fact == ZR_NULL) return;
    boundaryEnd = zr_escape_instruction_after(boundaryId);
    if (boundaryEnd > fact->liveEnd) fact->liveEnd = boundaryEnd;
}

static TZrBool zr_escape_reserve(void **storage, TZrUInt32 *capacity,
                                 TZrUInt32 needed, size_t elementSize) {
    TZrUInt32 next;
    void *memory;

    if (storage == ZR_NULL || capacity == ZR_NULL || elementSize == 0u ||
        !zr_escape_count_fits(needed, elementSize)) return ZR_FALSE;
    if (needed <= *capacity) return ZR_TRUE;
    next = *capacity == 0u ? 8u : *capacity;
    while (next < needed) {
        if (next > UINT32_MAX / 2u) {
            next = needed;
            break;
        }
        next *= 2u;
    }
    if (!zr_escape_count_fits(next, elementSize)) return ZR_FALSE;
    memory = realloc(*storage, (size_t)next * elementSize);
    if (memory == ZR_NULL) return ZR_FALSE;
    *storage = memory;
    *capacity = next;
    return ZR_TRUE;
}

static TZrBool zr_escape_is_flow_opcode(EZrExecIrOpcode opcode) {
    switch (opcode) {
        case ZR_EXEC_IR_OPCODE_COPY:
        case ZR_EXEC_IR_OPCODE_MOVE:
        case ZR_EXEC_IR_OPCODE_CONVERT:
        case ZR_EXEC_IR_OPCODE_PLACE_BASE:
        case ZR_EXEC_IR_OPCODE_PLACE_PROJECT:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool zr_escape_instruction_may_suspend(
        const SZrExecIrInstruction *instruction) {
    const SZrExecIrOpcodeInfo *info;

    if (instruction == ZR_NULL) return ZR_FALSE;
    info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
    return (TZrBool)((instruction->flags & ZR_EXEC_IR_FLAG_MAY_SUSPEND) != 0u ||
                     (info != ZR_NULL &&
                      (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND) != 0u));
}

static TZrBool zr_escape_instruction_may_throw(
        const SZrExecIrInstruction *instruction) {
    const SZrExecIrOpcodeInfo *info;

    if (instruction == ZR_NULL) return ZR_FALSE;
    info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
    return (TZrBool)((instruction->flags & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u ||
                     (info != ZR_NULL &&
                      (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u));
}

static TZrBool zr_escape_exception_can_observe_unread(
        EZrExecIrOpcode opcode) {
    /* Calls and exceptional terminators may enter arbitrary cleanup code;
     * an otherwise unread value therefore remains conservatively live at
     * that boundary.  A local STORE/checked arithmetic operation does not by
     * itself expose unrelated values that have no def-use edge. */
    return (TZrBool)(opcode == ZR_EXEC_IR_OPCODE_CALL ||
                     opcode == ZR_EXEC_IR_OPCODE_INVOKE ||
                     opcode == ZR_EXEC_IR_OPCODE_THROW);
}

static EZrExecIrEscapeState zr_escape_state_for_kind(
        EZrExecIrEscapeKind kind) {
    switch (kind) {
        case ZR_EXEC_IR_ESCAPE_KIND_RETURN:
            return ZR_EXEC_IR_ESCAPE_CALLER;
        case ZR_EXEC_IR_ESCAPE_KIND_HEAP_STORE:
        case ZR_EXEC_IR_ESCAPE_KIND_CLOSURE_CAPTURE:
            return ZR_EXEC_IR_ESCAPE_HEAP_STATIC;
        case ZR_EXEC_IR_ESCAPE_KIND_UNKNOWN_CALL:
        case ZR_EXEC_IR_ESCAPE_KIND_NATIVE_CAPTURE:
        case ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND:
        case ZR_EXEC_IR_ESCAPE_KIND_WORKER_PARAMETER:
        case ZR_EXEC_IR_ESCAPE_KIND_EXCEPTION:
        case ZR_EXEC_IR_ESCAPE_KIND_UNKNOWN:
        default:
            return ZR_EXEC_IR_ESCAPE_UNKNOWN;
    }
}

static EZrExecIrEscapeState zr_escape_join_state(
        EZrExecIrEscapeState left, EZrExecIrEscapeState right) {
    /* The parser's canonical escape contract is an upper-bound lattice:
     * LOCAL < FUNCTION < CALLER < HEAP_STATIC < UNKNOWN.  Keep the same
     * ordering here so a parameter captured into a heap is reported as
     * HEAP_STATIC (rather than spuriously UNKNOWN), while any genuinely
     * unknown observation remains the top element. */
    return left > right ? left : right;
}

static TZrBool zr_escape_append_edge(SZrExecIrEscapeSummary *summary,
                                     TZrExecIrValueId sourceValueId,
                                     TZrExecIrValueId targetValueId,
                                     TZrExecIrInstructionId instructionId,
                                     TZrExecIrSourceId sourceId,
                                     EZrExecIrEscapeKind kind,
                                     TZrUInt32 *outIndex,
                                     const SZrExecIrFunction *function,
                                     SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrEscapeEdge *edge;
    TZrUInt32 index;

    if (summary == ZR_NULL || sourceValueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
        sourceValueId > summary->factCount ||
        summary->edgeCount == UINT32_MAX ||
        !zr_escape_reserve((void **)&summary->edges, &summary->edgeCapacity,
                           summary->edgeCount + 1u, sizeof(*summary->edges))) {
        zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                       function, instructionId, sourceId);
        return ZR_FALSE;
    }
    index = summary->edgeCount++;
    edge = &summary->edges[index];
    edge->sourceValueId = sourceValueId;
    edge->targetValueId = targetValueId;
    edge->instructionId = instructionId;
    edge->sourceId = sourceId;
    edge->kind = kind;
    edge->parentEdgeIndex = ZR_EXEC_IR_ESCAPE_EDGE_INVALID;
    if (outIndex != ZR_NULL) *outIndex = index;
    return ZR_TRUE;
}

static TZrBool zr_escape_queue_push(TZrUInt32 **queue,
                                    TZrUInt32 *queueCapacity,
                                    TZrUInt32 *queueCount,
                                    TZrUInt32 valueId,
                                    const SZrExecIrFunction *function,
                                    TZrExecIrInstructionId instructionId,
                                    TZrExecIrSourceId sourceId,
                                    SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 nextCapacity;
    TZrUInt32 *next;

    if (queue == ZR_NULL || queueCapacity == ZR_NULL || queueCount == ZR_NULL ||
        *queueCount == UINT32_MAX) {
        zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                       function, instructionId, sourceId);
        return ZR_FALSE;
    }
    if (*queueCount == *queueCapacity) {
        nextCapacity = *queueCapacity == 0u ? 8u : *queueCapacity;
        if (nextCapacity > UINT32_MAX / 2u) {
            nextCapacity = UINT32_MAX;
        } else {
            nextCapacity *= 2u;
        }
        if (!zr_escape_count_fits(nextCapacity, sizeof(**queue))) {
            zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                           function, instructionId, sourceId);
            return ZR_FALSE;
        }
        next = (TZrUInt32 *)realloc(*queue,
                                    (size_t)nextCapacity * sizeof(**queue));
        if (next == ZR_NULL) {
            zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                           function, instructionId, sourceId);
            return ZR_FALSE;
        }
        *queue = next;
        *queueCapacity = nextCapacity;
    }
    (*queue)[(*queueCount)++] = valueId;
    return ZR_TRUE;
}

static TZrBool zr_escape_reason_would_cycle(
        const SZrExecIrEscapeSummary *summary,
        TZrUInt32 edgeIndex,
        TZrUInt32 parentEdgeIndex) {
    TZrUInt32 steps = 0u;
    while (parentEdgeIndex != ZR_EXEC_IR_ESCAPE_EDGE_INVALID &&
           parentEdgeIndex < summary->edgeCount &&
           steps++ <= summary->edgeCount) {
        if (parentEdgeIndex == edgeIndex) return ZR_TRUE;
        parentEdgeIndex = summary->edges[parentEdgeIndex].parentEdgeIndex;
    }
    return ZR_FALSE;
}

static TZrBool zr_escape_mark_sink(SZrExecIrEscapeSummary *summary,
                                    TZrExecIrValueId valueId,
                                    TZrExecIrInstructionId instructionId,
                                    TZrExecIrSourceId sourceId,
                                    EZrExecIrEscapeKind kind,
                                    const SZrExecIrFunction *function,
                                    TZrUInt32 **queue,
                                    TZrUInt32 *queueCapacity,
                                    TZrUInt32 *queueCount,
                                    TZrBool *queued,
                                    SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrEscapeFact *fact;
    TZrUInt32 edgeIndex;
    EZrExecIrEscapeState state;
    EZrExecIrEscapeState previousState;
    EZrExecIrEscapeState joined;
    TZrBool changed;
    TZrBool flagsChanged = ZR_FALSE;

    if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
        valueId > summary->factCount) return ZR_TRUE;
    if (!zr_escape_append_edge(summary, valueId, 0u, instructionId, sourceId,
                               kind, &edgeIndex, function, diagnostic)) {
        return ZR_FALSE;
    }
    fact = &summary->facts[valueId - 1u];
    state = zr_escape_state_for_kind(kind);
    previousState = fact->state;
    joined = zr_escape_join_state(fact->state, state);
    changed = (TZrBool)(joined != fact->state);
    /* Every sink is an observation boundary.  Even an unknown call or an
     * exceptional cleanup may retain, inspect, or drop the value, so do not
     * let a later placement pass treat its identity as unobserved. */
    if (!fact->identityObserved) flagsChanged = ZR_TRUE;
    if (kind == ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND &&
        !fact->crossesSuspend) flagsChanged = ZR_TRUE;
    if (kind == ZR_EXEC_IR_ESCAPE_KIND_NATIVE_CAPTURE &&
        !fact->nativeRetained) flagsChanged = ZR_TRUE;
    if (kind == ZR_EXEC_IR_ESCAPE_KIND_WORKER_PARAMETER &&
        !fact->workerEscaped) flagsChanged = ZR_TRUE;
    /* Prefer a reason that accounts for a newly stronger lattice state.  A
     * value may first be returned and later passed to an unknown call; the
     * latter is the useful explanation for its final UNKNOWN classification. */
    if (fact->firstReasonEdge == ZR_EXEC_IR_ESCAPE_EDGE_INVALID ||
        joined > previousState) {
        fact->firstReasonEdge = edgeIndex;
    }
    fact->identityObserved = ZR_TRUE;
    if (kind == ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND) {
        fact->crossesSuspend = ZR_TRUE;
    }
    if (kind == ZR_EXEC_IR_ESCAPE_KIND_NATIVE_CAPTURE) {
        fact->nativeRetained = ZR_TRUE;
    }
    if (kind == ZR_EXEC_IR_ESCAPE_KIND_WORKER_PARAMETER) {
        fact->workerEscaped = ZR_TRUE;
    }
    if (changed) fact->state = joined;
    if ((changed || flagsChanged) && queue != ZR_NULL &&
        queueCapacity != ZR_NULL &&
        queueCount != ZR_NULL &&
        queued != ZR_NULL && !queued[valueId]) {
        if (!zr_escape_queue_push(queue, queueCapacity, queueCount, valueId,
                                  function, instructionId, sourceId,
                                  diagnostic)) return ZR_FALSE;
        queued[valueId] = ZR_TRUE;
    }
    return ZR_TRUE;
}

static void zr_escape_propagate_flags(SZrExecIrEscapeFact *source,
                                      const SZrExecIrEscapeFact *target) {
    source->identityObserved = (TZrBool)(source->identityObserved ||
                                         target->identityObserved);
    source->crossesSuspend = (TZrBool)(source->crossesSuspend ||
                                       target->crossesSuspend);
    source->nativeRetained = (TZrBool)(source->nativeRetained ||
                                       target->nativeRetained);
    source->workerEscaped = (TZrBool)(source->workerEscaped ||
                                      target->workerEscaped);
    /* A DROP through a copied/moved value is still observable at the
     * allocation origin.  Losing this bit would allow a stack decision for
     * an alias whose destructor/release ordering remains externally visible. */
    source->dropObservable = (TZrBool)(source->dropObservable ||
                                      target->dropObservable);
}

/* summary lifecycle/accessors live in exec_ir_escape_summary.c */
TZrBool ZrParser_ExecIr_AnalyzeEscape(
        const SZrExecIrFunction *function,
        SZrExecIrEscapeSummary *summary,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrEscapeSummary temporary;
    TZrUInt32 *queue = ZR_NULL;
    TZrUInt32 queueCapacity = 0u;
    TZrBool *queued = ZR_NULL;
    TZrUInt32 queueCount = 0u;
    TZrUInt32 queueHead = 0u;
    TZrUInt32 instructionIndex;
    TZrUInt32 valueIndex;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL || summary == ZR_NULL) {
        zr_escape_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                       function, 0u, 0u);
        return ZR_FALSE;
    }
    /* The analysis is read-only.  Structure/SSA verification also makes all
     * side-pool ranges safe to traverse below. */
    if (!ZrCore_ExecIr_VerifyFunction(function,
                                      (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                                              ZR_EXEC_IR_VERIFY_SSA),
                                      diagnostic)) {
        zr_escape_enrich_diagnostic(diagnostic, function);
        return ZR_FALSE;
    }
    /* The core verifier checks result-side definitions, but intentionally
     * leaves the optional value back-pointer untouched.  Reject a stale
     * non-zero back-pointer here rather than using it to derive a bogus live
     * interval or parameter classification. */
    for (valueIndex = 0u; valueIndex < function->valueCount; ++valueIndex) {
        TZrExecIrInstructionId definition =
                function->values[valueIndex].definition;
        if (definition != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
            definition > function->instructionCount) {
            zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                           function, definition, 0u);
            zr_escape_enrich_diagnostic(diagnostic, function);
            return ZR_FALSE;
        }
    }

    /* The temporary is owned entirely by this call.  Clear it before the
     * public idempotent initializer so memory-sanitizer builds never inspect
     * indeterminate stack bytes while deciding whether a prior buffer exists. */
    memset(&temporary, 0, sizeof(temporary));
    ZrParser_ExecIr_EscapeSummaryInit(&temporary);
    if (function->valueCount != 0u) {
        if (!zr_escape_reserve((void **)&temporary.facts,
                               &temporary.factCapacity, function->valueCount,
                               sizeof(*temporary.facts))) {
            zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                           function, 0u, 0u);
            goto fail;
        }
        temporary.factCount = function->valueCount;
        memset(temporary.facts, 0,
               (size_t)temporary.factCount * sizeof(*temporary.facts));
    }
    if (function->valueCount != 0u) {
        if (function->valueCount == UINT32_MAX ||
            !zr_escape_count_fits(function->valueCount + 1u,
                                  sizeof(*queue)) ||
            !zr_escape_count_fits(function->valueCount + 1u,
                                  sizeof(*queued))) {
            zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                           function, 0u, 0u);
            goto fail;
        }
        queue = (TZrUInt32 *)calloc((size_t)function->valueCount + 1u,
                                    sizeof(*queue));
        queued = (TZrBool *)calloc((size_t)function->valueCount + 1u,
                                    sizeof(*queued));
        if (queue == ZR_NULL || queued == ZR_NULL) {
            zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                           function, 0u, 0u);
            goto fail;
        }
        queueCapacity = function->valueCount + 1u;
    }
    for (valueIndex = 0u; valueIndex < temporary.factCount; ++valueIndex) {
        SZrExecIrEscapeFact *fact = &temporary.facts[valueIndex];
        fact->valueId = valueIndex + 1u;
        fact->ownership = function->values[valueIndex].ownership;
        /* External entry values exist before the function body.  Phi results
         * also have no ordinary instruction definition, so the explicit flag
         * is the only sound parameter/capture classification. */
        fact->state = (function->values[valueIndex].flags &
                       ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u
                          ? ZR_EXEC_IR_ESCAPE_FUNCTION
                          : ZR_EXEC_IR_ESCAPE_LOCAL;
        fact->decision = ZR_EXEC_IR_ALLOC_HEAP;
        fact->firstReasonEdge = ZR_EXEC_IR_ESCAPE_EDGE_INVALID;
        fact->liveStart = 0u;
        fact->liveEnd = 0u;
    }
    /* Parameter values already carry FUNCTION lifetime before any sink is
     * encountered.  Seed them into the worklist so a pure COPY/MOVE/PHI
     * chain preserves that fact even when no external observation exists;
     * otherwise an alias queried in isolation would be misreported LOCAL. */
    for (valueIndex = 0u; valueIndex < temporary.factCount; ++valueIndex) {
        if (temporary.facts[valueIndex].state != ZR_EXEC_IR_ESCAPE_LOCAL &&
            queued != ZR_NULL && !queued[valueIndex + 1u]) {
            if (!zr_escape_queue_push(
                        &queue, &queueCapacity, &queueCount, valueIndex + 1u,
                        function, 0u, 0u, diagnostic)) goto fail;
            queued[valueIndex + 1u] = ZR_TRUE;
        }
    }

    /* First collect definitions and ordinary uses. */
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        TZrExecIrInstructionId instructionId = instructionIndex + 1u;
        TZrUInt32 operandIndex;
        TZrUInt32 resultIndex;
        EZrExecIrOpcode opcode = (EZrExecIrOpcode)instruction->opcode;

        if (!zr_escape_range_valid(instruction->results,
                                   function->resultCount) ||
            !zr_escape_range_valid(instruction->operands,
                                   function->operandCount)) {
            zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                           function, instructionId, instruction->sourceId);
            goto fail;
        }
        for (resultIndex = instruction->results.start;
             resultIndex < instruction->results.start + instruction->results.count;
             ++resultIndex) {
            TZrExecIrValueId valueId = function->results[resultIndex];
            if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
                valueId > temporary.factCount) {
                zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                               function, instructionId, instruction->sourceId);
                goto fail;
            }
            if (function->values[valueId - 1u].definition !=
                    ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                function->values[valueId - 1u].definition != instructionId) {
                /* FunctionAppendInstruction normally maintains this
                 * back-pointer.  A valid-looking but stale ID is still a
                 * malformed SSA witness and must not be silently overwritten
                 * by the summary pass. */
                zr_escape_diag(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
                               function, instructionId, instruction->sourceId);
                goto fail;
            }
            temporary.facts[valueId - 1u].definitionInstructionId = instructionId;
            temporary.facts[valueId - 1u].liveStart = instructionId;
            if (temporary.facts[valueId - 1u].lastUseInstructionId == 0u) {
                /* Keep an unused definition as a valid empty half-open range;
                 * lastUseInstructionId is the separate no-use sentinel. */
                temporary.facts[valueId - 1u].liveEnd = instructionId;
            }
            if (opcode == ZR_EXEC_IR_OPCODE_ALLOC) {
                if (!temporary.facts[valueId - 1u].isAllocationCandidate) {
                    if (temporary.allocationCount == UINT32_MAX) {
                        zr_escape_diag(diagnostic,
                                       ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                       function, instructionId,
                                       instruction->sourceId);
                        goto fail;
                    }
                    temporary.facts[valueId - 1u].isAllocationCandidate = ZR_TRUE;
                    temporary.facts[valueId - 1u].allocationInstructionId = instructionId;
                    temporary.allocationCount += 1u;
                }
            }
        }
        for (operandIndex = instruction->operands.start;
             operandIndex < instruction->operands.start + instruction->operands.count;
             ++operandIndex) {
            TZrExecIrValueId valueId = function->operands[operandIndex];
            SZrExecIrEscapeFact *fact;
            if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
                valueId > temporary.factCount) {
                zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                               function, instructionId, instruction->sourceId);
                goto fail;
            }
            fact = &temporary.facts[valueId - 1u];
            if (instructionId > fact->lastUseInstructionId) {
                fact->lastUseInstructionId = instructionId;
                fact->liveEnd = zr_escape_instruction_after(instructionId);
            }
            if (opcode == ZR_EXEC_IR_OPCODE_DROP) {
                if (!fact->dropObservable) {
                    fact->dropObservable = ZR_TRUE;
                    /* DROP is not an escape sink by itself, but it is a
                     * monotonic alias fact.  Wake the fixed-point worklist
                     * so an allocation feeding a copied/moved value inherits
                     * the observable cleanup requirement. */
                    if (queued != ZR_NULL && !queued[valueId]) {
                        if (!zr_escape_queue_push(
                                    &queue, &queueCapacity, &queueCount,
                                    valueId, function, instructionId,
                                    instruction->sourceId, diagnostic)) {
                            goto fail;
                        }
                        queued[valueId] = ZR_TRUE;
                    }
                }
            }
        }

        /* Copy/place operations are backward escape edges: if the result is
         * observed, its source must be kept in a stable allocation too. */
        if (opcode == ZR_EXEC_IR_OPCODE_PHI &&
            instruction->results.count == 1u) {
            TZrExecIrValueId targetValueId =
                    function->results[instruction->results.start];
            for (operandIndex = instruction->operands.start;
                 operandIndex < instruction->operands.start +
                                 instruction->operands.count;
                 ++operandIndex) {
                if (!zr_escape_append_edge(
                            &temporary, function->operands[operandIndex],
                            targetValueId, instructionId, instruction->sourceId,
                            ZR_EXEC_IR_ESCAPE_KIND_VALUE_FLOW, ZR_NULL,
                            function, diagnostic)) goto fail;
            }
        } else if (zr_escape_is_flow_opcode(opcode) &&
                   instruction->results.count == 1u &&
                   instruction->operands.count >= 1u) {
            TZrExecIrValueId sourceValueId =
                    function->operands[instruction->operands.start];
            TZrExecIrValueId targetValueId =
                    function->results[instruction->results.start];
            if (!zr_escape_append_edge(&temporary, sourceValueId, targetValueId,
                                       instructionId, instruction->sourceId,
                                       ZR_EXEC_IR_ESCAPE_KIND_VALUE_FLOW,
                                       ZR_NULL, function, diagnostic)) goto fail;
        }

        /* Direct observations are the roots of the fixed-point graph. */
        {
            EZrExecIrEscapeKind sinkKind = ZR_EXEC_IR_ESCAPE_KIND_UNKNOWN;
            TZrBool isSink = ZR_TRUE;
            switch (opcode) {
                case ZR_EXEC_IR_OPCODE_RETURN:
                    sinkKind = ZR_EXEC_IR_ESCAPE_KIND_RETURN;
                    break;
                case ZR_EXEC_IR_OPCODE_STORE:
                    /* A STORE is always a heap/static observation for the
                     * payload.  A binding row may additionally identify a
                     * closure write-back, but the row table is intentionally
                     * not retained by this parser-owned summary; keep both
                     * reasons instead of guessing that every bound store is
                     * a closure capture. */
                    sinkKind = ZR_EXEC_IR_ESCAPE_KIND_HEAP_STORE;
                    break;
                case ZR_EXEC_IR_OPCODE_BARRIER:
                    sinkKind = ZR_EXEC_IR_ESCAPE_KIND_HEAP_STORE;
                    break;
                case ZR_EXEC_IR_OPCODE_CALL:
                case ZR_EXEC_IR_OPCODE_INVOKE:
                    sinkKind = ZR_EXEC_IR_ESCAPE_KIND_UNKNOWN_CALL;
                    break;
                case ZR_EXEC_IR_OPCODE_THROW:
                    sinkKind = ZR_EXEC_IR_ESCAPE_KIND_EXCEPTION;
                    break;
                case ZR_EXEC_IR_OPCODE_SUSPEND:
                    sinkKind = ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND;
                    break;
                default:
                    isSink = ZR_FALSE;
                    break;
            }
            if (isSink) {
                for (operandIndex = instruction->operands.start;
                     operandIndex < instruction->operands.start +
                                     instruction->operands.count;
                     ++operandIndex) {
                    /* STORE's first operand is the destination place/base;
                     * only the value written into that place escapes.  The
                     * canonical STORE schema is (place, value), while a
                     * variadic call/suspend observes every argument. */
                    if (opcode == ZR_EXEC_IR_OPCODE_STORE &&
                        operandIndex == instruction->operands.start) {
                        continue;
                    }
                    TZrExecIrValueId valueId = function->operands[operandIndex];
                    if (!zr_escape_mark_sink(&temporary, valueId, instructionId,
                                              instruction->sourceId, sinkKind,
                                              function, &queue, &queueCapacity,
                                              &queueCount,
                                              queued, diagnostic)) goto fail;
                    if (opcode == ZR_EXEC_IR_OPCODE_STORE &&
                        operandIndex != instruction->operands.start &&
                        instruction->bindingRow != 0u &&
                        instruction->bindingRow != UINT32_MAX) {
                        if (!zr_escape_mark_sink(
                                    &temporary, valueId, instructionId,
                                    instruction->sourceId,
                                    ZR_EXEC_IR_ESCAPE_KIND_CLOSURE_CAPTURE,
                                    function, &queue, &queueCapacity,
                                    &queueCount, queued, diagnostic)) goto fail;
                    }
                }
                /* A bound call can retain a native pointer; a suspend bound
                 * to a worker can move its arguments into a task frame. */
                if ((opcode == ZR_EXEC_IR_OPCODE_CALL ||
                     opcode == ZR_EXEC_IR_OPCODE_INVOKE) &&
                    instruction->bindingRow != 0u &&
                    instruction->bindingRow != UINT32_MAX) {
                    for (operandIndex = instruction->operands.start;
                         operandIndex < instruction->operands.start +
                                         instruction->operands.count;
                         ++operandIndex) {
                        if (!zr_escape_mark_sink(
                                    &temporary, function->operands[operandIndex],
                                    instructionId, instruction->sourceId,
                                    ZR_EXEC_IR_ESCAPE_KIND_NATIVE_CAPTURE,
                                    function, &queue, &queueCapacity,
                                    &queueCount, queued,
                                    diagnostic)) goto fail;
                    }
                }
                /* A CALL/INVOKE explicitly marked MAY_SUSPEND may transfer
                 * arguments into a worker/task frame.  A bare SUSPEND is a
                 * separate crossed-suspend boundary; do not conflate it with
                 * worker-parameter escape. */
                if ((opcode == ZR_EXEC_IR_OPCODE_CALL ||
                     opcode == ZR_EXEC_IR_OPCODE_INVOKE) &&
                    (instruction->flags & ZR_EXEC_IR_FLAG_MAY_SUSPEND) != 0u) {
                    for (operandIndex = instruction->operands.start;
                         operandIndex < instruction->operands.start +
                                         instruction->operands.count;
                         ++operandIndex) {
                        if (!zr_escape_mark_sink(
                                    &temporary, function->operands[operandIndex],
                                    instructionId, instruction->sourceId,
                                    ZR_EXEC_IR_ESCAPE_KIND_WORKER_PARAMETER,
                                    function, &queue, &queueCapacity,
                                    &queueCount, queued,
                                    diagnostic)) goto fail;
                    }
                }
            }
        }

        /* Schema-level checked operations can take an exceptional edge even
         * when they are not CALL/INVOKE/THROW.  Their explicit operands are
         * observable by cleanup before the operation completes; retain an
         * exception reason rather than relying only on a later-use scan. */
        if (zr_escape_instruction_may_throw(instruction) &&
            opcode != ZR_EXEC_IR_OPCODE_CALL &&
            opcode != ZR_EXEC_IR_OPCODE_INVOKE &&
            opcode != ZR_EXEC_IR_OPCODE_THROW &&
            opcode != ZR_EXEC_IR_OPCODE_STORE) {
            for (operandIndex = instruction->operands.start;
                 operandIndex < instruction->operands.start +
                                     instruction->operands.count;
                 ++operandIndex) {
                if (!zr_escape_mark_sink(
                            &temporary, function->operands[operandIndex],
                            instructionId, instruction->sourceId,
                            ZR_EXEC_IR_ESCAPE_KIND_EXCEPTION, function,
                            &queue, &queueCapacity, &queueCount, queued,
                            diagnostic)) goto fail;
            }
        }

        /* Do the analogous thing for a producer-marked suspend boundary that
         * is not one of the explicit suspend/call opcodes. */
        if (zr_escape_instruction_may_suspend(instruction) &&
            opcode != ZR_EXEC_IR_OPCODE_CALL &&
            opcode != ZR_EXEC_IR_OPCODE_INVOKE &&
            opcode != ZR_EXEC_IR_OPCODE_SUSPEND) {
            for (operandIndex = instruction->operands.start;
                 operandIndex < instruction->operands.start +
                                     instruction->operands.count;
                 ++operandIndex) {
                if (!zr_escape_mark_sink(
                            &temporary, function->operands[operandIndex],
                            instructionId, instruction->sourceId,
                            ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND, function,
                            &queue, &queueCapacity, &queueCount, queued,
                            diagnostic)) goto fail;
            }
        }
    }

    /* Phi incoming values are also backward flow edges.  Their source block
     * is retained on the edge so a reason chain can identify the merge. */
    for (instructionIndex = 0u; instructionIndex < function->blockCount;
         ++instructionIndex) {
        const SZrExecIrBlock *block = &function->blocks[instructionIndex];
        TZrUInt32 phiIndex;
        /* Block ranges are zero-based side-array offsets; diagnostics and
         * value lifetimes use one-based instruction IDs. */
        TZrExecIrInstructionId mergeInstruction =
                block->instructionRange.count == 0u
                    ? ZR_EXEC_IR_INSTRUCTION_ID_INVALID
                    : block->instructionRange.start == UINT32_MAX
                          ? UINT32_MAX
                          : block->instructionRange.start + 1u;
        TZrExecIrSourceId mergeSource =
                (mergeInstruction != 0u &&
                 mergeInstruction <= function->instructionCount)
                    ? function->instructions[mergeInstruction - 1u].sourceId
                    : 0u;
        for (phiIndex = block->phis.start;
             phiIndex < block->phis.start + block->phis.count; ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            TZrUInt32 incomingIndex;
            if (phi->result == ZR_EXEC_IR_VALUE_ID_INVALID ||
                phi->result > temporary.factCount ||
                !zr_escape_range_valid(phi->incomings,
                                       function->phiIncomingCount)) {
                zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                               function, mergeInstruction, 0u);
                goto fail;
            }
            if (function->values[phi->result - 1u].definition !=
                        ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                mergeInstruction != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                mergeInstruction != UINT32_MAX &&
                function->values[phi->result - 1u].definition !=
                        mergeInstruction) {
                zr_escape_diag(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
                               function, mergeInstruction, mergeSource);
                goto fail;
            }
            if (temporary.facts[phi->result - 1u].definitionInstructionId !=
                        ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
                /* A value cannot be produced by an ordinary result and a
                 * block PHI at once.  The core SSA verifier tracks ordinary
                 * results only, so reject this cross-table duplicate here. */
                zr_escape_diag(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
                               function, mergeInstruction, mergeSource);
                goto fail;
            }
            /* PHI results are definitions at block entry even though they do
             * not occupy the ordinary instruction result pool.  Record that
             * synthetic definition so lifetime/suspend checks do not mistake
             * a merged value for an external parameter. */
            if (mergeInstruction != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                mergeInstruction != UINT32_MAX &&
                temporary.facts[phi->result - 1u].definitionInstructionId ==
                    ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
                temporary.facts[phi->result - 1u].definitionInstructionId =
                        mergeInstruction;
                temporary.facts[phi->result - 1u].liveStart = mergeInstruction;
                if (temporary.facts[phi->result - 1u].lastUseInstructionId ==
                            0u) {
                    temporary.facts[phi->result - 1u].liveEnd =
                            mergeInstruction;
                }
                /* A PHI is a definition at the merge, not an incoming
                 * parameter.  Undo the provisional FUNCTION state assigned
                 * to undefined values before the PHI table was traversed.
                 * Preserve any sink already discovered while scanning the
                 * ordinary instruction stream (a PHI can be returned by the
                 * first instruction in its merge block). */
                if (temporary.facts[phi->result - 1u].firstReasonEdge ==
                            ZR_EXEC_IR_ESCAPE_EDGE_INVALID &&
                    !temporary.facts[phi->result - 1u].identityObserved) {
                    temporary.facts[phi->result - 1u].state =
                            ZR_EXEC_IR_ESCAPE_LOCAL;
                }
            }
            for (incomingIndex = phi->incomings.start;
                 incomingIndex < phi->incomings.start + phi->incomings.count;
                 ++incomingIndex) {
                TZrExecIrValueId sourceValueId =
                        function->phiIncoming[incomingIndex].value;
                if (sourceValueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
                    sourceValueId > temporary.factCount ||
                    !zr_escape_append_edge(
                            &temporary, sourceValueId, phi->result,
                            mergeInstruction, mergeSource,
                            ZR_EXEC_IR_ESCAPE_KIND_VALUE_FLOW, ZR_NULL,
                            function, diagnostic)) goto fail;
                if (mergeInstruction != 0u &&
                    mergeInstruction - 1u < function->instructionCount &&
                    mergeInstruction > temporary.facts[sourceValueId - 1u].lastUseInstructionId) {
                    temporary.facts[sourceValueId - 1u].lastUseInstructionId =
                            mergeInstruction;
                    temporary.facts[sourceValueId - 1u].liveEnd =
                            zr_escape_instruction_after(mergeInstruction);
                }
            }
        }
    }

    /* A non-zero value back-pointer is part of the SSA witness, not merely a
     * hint.  The core verifier checks the result-side table but deliberately
     * leaves this optional field untouched; reject an in-range pointer that
     * does not agree with either an ordinary instruction result or a block
     * PHI definition before deriving any lifetime facts from it. */
    for (valueIndex = 0u; valueIndex < temporary.factCount; ++valueIndex) {
        TZrExecIrInstructionId declaredDefinition =
                function->values[valueIndex].definition;
        if (declaredDefinition != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
            temporary.facts[valueIndex].definitionInstructionId !=
                    declaredDefinition) {
            zr_escape_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                           function, declaredDefinition, 0u);
            zr_escape_enrich_diagnostic(diagnostic, function);
            goto fail;
        }
    }

    /* Values live through a suspend are escaping even when they are not an
     * explicit suspend operand (e.g. a later use reloads them).  External
     * entry values have definition id zero and are live from function entry.
     * Instruction IDs are monotonically assigned by the canonical builder, so
     * this check is deterministic; an ambiguous CFG remains conservative
     * through the direct suspend edges above. */
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        TZrUInt32 candidate;
        if (!zr_escape_instruction_may_suspend(instruction)) {
            continue;
        }
        for (candidate = 0u; candidate < temporary.factCount; ++candidate) {
            SZrExecIrEscapeFact *fact = &temporary.facts[candidate];
            TZrExecIrInstructionId suspendId = instructionIndex + 1u;
            if ((fact->definitionInstructionId == 0u ||
                 fact->definitionInstructionId < suspendId) &&
                /* A value with no ordinary use may still need to live in the
                 * task/cleanup frame until the boundary.  Without an
                 * explicit DROP or state-map proof, treat that lifetime as
                 * potentially crossing suspend too; liveEnd is extended below
                 * while lastUse remains the no-ordinary-use sentinel. */
                 (fact->lastUseInstructionId > suspendId ||
                  (fact->lastUseInstructionId == 0u &&
                   fact->definitionInstructionId != 0u)) &&
                 !zr_escape_mark_sink(
                         &temporary, fact->valueId, suspendId,
                         instruction->sourceId,
                         ZR_EXEC_IR_ESCAPE_KIND_CROSSED_SUSPEND,
                         function, &queue, &queueCapacity, &queueCount, queued,
                         diagnostic)) goto fail;
            if (fact->lastUseInstructionId == 0u) {
                zr_escape_extend_live_end(fact, suspendId);
            }
        }
    }

    /* An exceptional edge can run cleanup/finalizer code before control
     * reaches the next ordinary instruction.  Values kept live across a
     * MAY_THROW boundary therefore cannot be treated as frame-local without
     * an exceptional-state-map proof (that proof is a later pass).  This scan
     * follows phi/liveness collection so merged values are covered too. */
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        TZrUInt32 candidate;
        if (!zr_escape_instruction_may_throw(instruction)) {
            continue;
        }
        for (candidate = 0u; candidate < temporary.factCount; ++candidate) {
            SZrExecIrEscapeFact *fact = &temporary.facts[candidate];
            TZrExecIrInstructionId throwId = instructionIndex + 1u;
            if ((fact->definitionInstructionId == 0u ||
                 fact->definitionInstructionId < throwId) &&
                /* Exceptional cleanup can observe a value even when the
                 * normal path has no later read.  Keep that path conservative
                 * until an explicit cleanup/state-map proof exists. */
                 (fact->lastUseInstructionId > throwId ||
                  (fact->lastUseInstructionId == 0u &&
                   zr_escape_exception_can_observe_unread(
                           (EZrExecIrOpcode)instruction->opcode))) &&
                !zr_escape_mark_sink(
                        &temporary, fact->valueId, throwId,
                         instruction->sourceId, ZR_EXEC_IR_ESCAPE_KIND_EXCEPTION,
                         function, &queue, &queueCapacity, &queueCount, queued,
                         diagnostic)) goto fail;
            if (fact->lastUseInstructionId == 0u) {
                zr_escape_extend_live_end(fact, throwId);
            }
        }
    }

    /* Fixed point over source -> result edges.  A source inherits both the
     * strongest state and the explanation flags of its escaped result. */
    while (queueHead < queueCount) {
        TZrExecIrValueId targetValueId = queue[queueHead++];
        TZrUInt32 edgeIndex;
        queued[targetValueId] = ZR_FALSE;
        for (edgeIndex = 0u; edgeIndex < temporary.edgeCount; ++edgeIndex) {
            SZrExecIrEscapeEdge *edge = &temporary.edges[edgeIndex];
            /* Backward propagation: an observed result taints its source. */
            if (edge->targetValueId == targetValueId &&
                edge->targetValueId != ZR_EXEC_IR_VALUE_ID_INVALID &&
                edge->sourceValueId != ZR_EXEC_IR_VALUE_ID_INVALID &&
                edge->sourceValueId <= temporary.factCount) {
                SZrExecIrEscapeFact *source =
                        &temporary.facts[edge->sourceValueId - 1u];
                const SZrExecIrEscapeFact *target =
                        &temporary.facts[targetValueId - 1u];
                EZrExecIrEscapeState joined =
                        zr_escape_join_state(source->state, target->state);
                TZrBool flagsChanged =
                        (TZrBool)(source->identityObserved !=
                                      (TZrBool)(source->identityObserved ||
                                                target->identityObserved) ||
                                  source->crossesSuspend !=
                                      (TZrBool)(source->crossesSuspend ||
                                                target->crossesSuspend) ||
                                  source->nativeRetained !=
                                      (TZrBool)(source->nativeRetained ||
                                                target->nativeRetained) ||
                                  source->workerEscaped !=
                                      (TZrBool)(source->workerEscaped ||
                                                target->workerEscaped) ||
                                  source->dropObservable !=
                                      (TZrBool)(source->dropObservable ||
                                                target->dropObservable));
                if (joined != source->state || flagsChanged) {
                    EZrExecIrEscapeState previousState = source->state;
                    source->state = joined;
                    zr_escape_propagate_flags(source, target);
                    if (target->firstReasonEdge !=
                                ZR_EXEC_IR_ESCAPE_EDGE_INVALID &&
                        !zr_escape_reason_would_cycle(
                                &temporary, edgeIndex,
                                target->firstReasonEdge)) {
                        edge->parentEdgeIndex = target->firstReasonEdge;
                    }
                    if (source->firstReasonEdge ==
                                ZR_EXEC_IR_ESCAPE_EDGE_INVALID ||
                        joined > previousState) {
                        source->firstReasonEdge = edgeIndex;
                    }
                    if (!queued[source->valueId]) {
                        if (!zr_escape_queue_push(
                                    &queue, &queueCapacity, &queueCount,
                                    source->valueId, function,
                                    edge->instructionId, edge->sourceId,
                                    diagnostic)) {
                            goto fail;
                        }
                        queued[source->valueId] = ZR_TRUE;
                    }
                }
            }

            /* Value-flow is an alias relation in both directions.  A source
             * observed by a store/return must taint its copied/phi result as
             * well; otherwise a dead-looking result could be incorrectly
             * offered to a later placement pass. */
            if (edge->sourceValueId == targetValueId &&
                edge->targetValueId != ZR_EXEC_IR_VALUE_ID_INVALID &&
                edge->targetValueId <= temporary.factCount) {
                SZrExecIrEscapeFact *destination =
                        &temporary.facts[edge->targetValueId - 1u];
                const SZrExecIrEscapeFact *origin =
                        &temporary.facts[targetValueId - 1u];
                EZrExecIrEscapeState forwardState =
                        zr_escape_join_state(destination->state,
                                             origin->state);
                TZrBool forwardFlags =
                        (TZrBool)(destination->identityObserved !=
                                      (TZrBool)(destination->identityObserved ||
                                                origin->identityObserved) ||
                                  destination->crossesSuspend !=
                                      (TZrBool)(destination->crossesSuspend ||
                                                origin->crossesSuspend) ||
                                  destination->nativeRetained !=
                                      (TZrBool)(destination->nativeRetained ||
                                                origin->nativeRetained) ||
                                  destination->workerEscaped !=
                                      (TZrBool)(destination->workerEscaped ||
                                                origin->workerEscaped) ||
                                  destination->dropObservable !=
                                      (TZrBool)(destination->dropObservable ||
                                                origin->dropObservable));
                if (forwardState != destination->state || forwardFlags) {
                    EZrExecIrEscapeState previousState = destination->state;
                    destination->state = forwardState;
                    zr_escape_propagate_flags(destination, origin);
                    if (origin->firstReasonEdge !=
                            ZR_EXEC_IR_ESCAPE_EDGE_INVALID &&
                        !zr_escape_reason_would_cycle(
                                &temporary, edgeIndex,
                                origin->firstReasonEdge)) {
                        edge->parentEdgeIndex = origin->firstReasonEdge;
                    }
                    if (destination->firstReasonEdge ==
                                ZR_EXEC_IR_ESCAPE_EDGE_INVALID ||
                        forwardState > previousState) {
                        destination->firstReasonEdge = edgeIndex;
                    }
                    if (!queued[destination->valueId]) {
                        if (!zr_escape_queue_push(
                                    &queue, &queueCapacity, &queueCount,
                                    destination->valueId, function,
                                    edge->instructionId, edge->sourceId,
                                    diagnostic)) goto fail;
                        queued[destination->valueId] = ZR_TRUE;
                    }
                }
            }
        }
    }

    /* Turn the monotonic state into the first placement summary.  Only a
     * proven local allocation with a known layout is eligible for stack;
     * region placement is intentionally deferred to 06.01. */
    for (valueIndex = 0u; valueIndex < temporary.factCount; ++valueIndex) {
        SZrExecIrEscapeFact *fact = &temporary.facts[valueIndex];
        fact->decision = ZR_EXEC_IR_ALLOC_HEAP;
        if (fact->isAllocationCandidate &&
            fact->state == ZR_EXEC_IR_ESCAPE_LOCAL &&
            !fact->dropObservable &&
            !fact->identityObserved && !fact->crossesSuspend &&
            !fact->nativeRetained && !fact->workerEscaped &&
            function->values[valueIndex].ownership == ZR_EXEC_IR_OWNERSHIP_GC) {
            const SZrExecIrInstruction *allocation =
                    fact->allocationInstructionId != 0u &&
                    fact->allocationInstructionId <= function->instructionCount
                        ? &function->instructions[fact->allocationInstructionId - 1u]
                        : ZR_NULL;
            if (allocation != ZR_NULL &&
                /* A type token alone does not prove byte size/alignment;
                 * only a concrete layout-table id is enough for this pass to
                 * offer stack placement. */
                allocation->layoutId != 0u &&
                allocation->layoutId != UINT32_MAX) {
                fact->decision = ZR_EXEC_IR_ALLOC_STACK;
            }
        }
    }
    temporary.irHash = ZrParser_ExecIr_EscapeInputHash(function);
    free(queue);
    free(queued);
    /* Replacement is transactional: a valid prior summary is released, and
     * a zero/randomly initialised destination is simply cleared by Free. */
    ZrParser_ExecIr_EscapeSummaryFree(summary);
    *summary = temporary;
    return ZR_TRUE;

fail:
    free(queue);
    free(queued);
    ZrParser_ExecIr_EscapeSummaryFree(&temporary);
    return ZR_FALSE;
}
