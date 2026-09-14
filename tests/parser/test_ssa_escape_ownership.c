#include "zr_vm_parser/exec_ir_escape.h"

#include <assert.h>
#include <string.h>

static void init_function(SZrExecIrFunction *function) {
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 17u;
    function->signatureHash = 0x55u;
}

static TZrExecIrValueId add_value(SZrExecIrFunction *function,
                                  TZrUInt32 typeToken) {
    TZrExecIrValueId id = ZrCore_ExecIr_FunctionAddValue(
            function, typeToken, ZR_EXEC_IR_OWNERSHIP_UNIQUE,
            ZR_EXEC_IR_NULLABILITY_NONNULL);
    assert(id != ZR_EXEC_IR_VALUE_ID_INVALID);
    return id;
}

static TZrExecIrValueId add_gc_value(SZrExecIrFunction *function,
                                      TZrUInt32 typeToken) {
    TZrExecIrValueId id = ZrCore_ExecIr_FunctionAddValue(
            function, typeToken, ZR_EXEC_IR_OWNERSHIP_GC,
            ZR_EXEC_IR_NULLABILITY_NONNULL);
    assert(id != ZR_EXEC_IR_VALUE_ID_INVALID);
    return id;
}

static void append_instruction(SZrExecIrFunction *function,
                               EZrExecIrOpcode opcode,
                               const TZrExecIrValueId *operands,
                               TZrUInt32 operandCount,
                               const TZrExecIrValueId *results,
                               TZrUInt32 resultCount,
                               TZrUInt32 layoutId,
                               TZrUInt16 flags,
                               TZrUInt32 bindingRow,
                               TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.flags = flags;
    instruction.layoutId = layoutId;
    instruction.bindingRow = bindingRow;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, operands, operandCount, &instruction.operands));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, results, resultCount, &instruction.results));
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                    ZR_NULL));
}

static const SZrExecIrEscapeFact *fact(
        const SZrExecIrEscapeSummary *summary, TZrExecIrValueId valueId) {
    const SZrExecIrEscapeFact *result =
            ZrParser_ExecIr_EscapeFactAt(summary, valueId);
    assert(result != ZR_NULL);
    return result;
}

static void test_local_alloc_is_stack_candidate(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;

    init_function(&function);
    value = add_gc_value(&function, 9u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &value, 1u, 4u,
                       (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_ALLOCATE |
                                   ZR_EXEC_IR_FLAG_MAY_GC),
                       0u, 101u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(summary.allocationCount == 1u);
    assert(fact(&summary, value)->state == ZR_EXEC_IR_ESCAPE_LOCAL);
    assert(fact(&summary, value)->decision == ZR_EXEC_IR_ALLOC_STACK);
    assert(fact(&summary, value)->liveStart == 1u);
    assert(fact(&summary, value)->liveEnd == 1u);
    assert(fact(&summary, value)->lastUseInstructionId == 0u);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_alloc_without_concrete_layout_stays_on_heap(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;

    init_function(&function);
    value = add_gc_value(&function, 9u);
    /* A type token is not enough to establish byte size/alignment. */
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &value, 1u, 0u, 0u, 0u, 102u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, value)->decision == ZR_EXEC_IR_ALLOC_HEAP);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_undefined_value_is_function_lifetime_parameter(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId parameter;

    init_function(&function);
    parameter = add_value(&function, 10u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, parameter)->definitionInstructionId ==
           ZR_EXEC_IR_INSTRUCTION_ID_INVALID);
    assert(fact(&summary, parameter)->state == ZR_EXEC_IR_ESCAPE_FUNCTION);
    assert(strcmp(ZrParser_ExecIr_EscapeStateName(
                         ZR_EXEC_IR_ESCAPE_FUNCTION),
                  "function") == 0);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_parameter_lifetime_flows_through_alias_without_sink(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId parameter;
    TZrExecIrValueId alias;

    init_function(&function);
    parameter = add_value(&function, 11u);
    alias = add_value(&function, 11u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_COPY, &parameter, 1u,
                       &alias, 1u, 0u, 0u, 0u, 18u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, parameter)->state == ZR_EXEC_IR_ESCAPE_FUNCTION);
    assert(fact(&summary, alias)->state == ZR_EXEC_IR_ESCAPE_FUNCTION);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_return_flow_propagates_to_allocation(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId allocation;
    TZrExecIrValueId forwarded;
    TZrExecIrValueId returnOperand;

    init_function(&function);
    allocation = add_value(&function, 3u);
    forwarded = add_value(&function, 3u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &allocation, 1u, 3u, 0u, 0u, 111u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_COPY, &allocation, 1u,
                       &forwarded, 1u, 0u, 0u, 0u, 112u);
    returnOperand = forwarded;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN, &returnOperand, 1u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 113u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    /* The source store must taint the copied alias in the forward direction. */
    assert(fact(&summary, forwarded)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    assert(fact(&summary, allocation)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    assert(fact(&summary, allocation)->decision == ZR_EXEC_IR_ALLOC_HEAP);
    assert(fact(&summary, allocation)->firstReasonEdge !=
           ZR_EXEC_IR_ESCAPE_EDGE_INVALID);
    assert(summary.edges[fact(&summary, allocation)->firstReasonEdge].kind ==
           ZR_EXEC_IR_ESCAPE_KIND_VALUE_FLOW);
    assert(summary.edges[fact(&summary, allocation)->firstReasonEdge].parentEdgeIndex !=
           ZR_EXEC_IR_ESCAPE_EDGE_INVALID);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_conflicting_heap_and_caller_observations_use_upper_bound(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId object;
    TZrExecIrValueId returnOperand;

    init_function(&function);
    object = add_value(&function, 21u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &object, 1u, 1u, 0u, 0u, 116u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_BARRIER, &object, 1u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 117u);
    returnOperand = object;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN, &returnOperand, 1u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 118u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    /* The canonical escape lattice joins caller + heap/static at the
     * stronger heap/static bound; only an unknown operation reaches UNKNOWN. */
    assert(fact(&summary, object)->state == ZR_EXEC_IR_ESCAPE_HEAP_STATIC);
    assert(fact(&summary, object)->decision == ZR_EXEC_IR_ALLOC_HEAP);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_escape_propagates_forward_through_copy(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId payload;
    TZrExecIrValueId copy;
    TZrExecIrValueId returnOperand;

    init_function(&function);
    payload = add_gc_value(&function, 23u);
    copy = add_value(&function, 23u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &payload, 1u, 1u, 0u, 0u, 126u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_COPY, &payload, 1u,
                       &copy, 1u, 0u, 0u, 0u, 127u);
    returnOperand = payload;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN, &returnOperand, 1u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 128u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, payload)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    assert(fact(&summary, copy)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_unknown_call_and_native_retention_are_conservative(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;
    TZrExecIrValueId callResult;
    TZrUInt32 edgeIndex;
    TZrBool sawUnknown = ZR_FALSE;
    TZrBool sawNative = ZR_FALSE;

    init_function(&function);
    value = add_value(&function, 4u);
    callResult = add_value(&function, 5u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &value, 1u, 2u, 0u, 0u, 121u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CALL, &value, 1u,
                       &callResult, 1u, 0u,
                       (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_ALLOCATE |
                                   ZR_EXEC_IR_FLAG_MAY_THROW),
                       7u, 122u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, value)->state == ZR_EXEC_IR_ESCAPE_UNKNOWN);
    assert(fact(&summary, value)->nativeRetained == ZR_TRUE);
    assert(fact(&summary, value)->decision == ZR_EXEC_IR_ALLOC_HEAP);
    for (edgeIndex = 0u; edgeIndex < summary.edgeCount; ++edgeIndex) {
        const SZrExecIrEscapeEdge *edge =
                ZrParser_ExecIr_EscapeEdgeAt(&summary, edgeIndex);
        if (edge->kind == ZR_EXEC_IR_ESCAPE_KIND_UNKNOWN_CALL) sawUnknown = ZR_TRUE;
        if (edge->kind == ZR_EXEC_IR_ESCAPE_KIND_NATIVE_CAPTURE) sawNative = ZR_TRUE;
    }
    assert(sawUnknown && sawNative);
    assert(strcmp(ZrParser_ExecIr_EscapeKindName(
                         ZR_EXEC_IR_ESCAPE_KIND_NATIVE_CAPTURE),
                  "native-retained") == 0);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_worker_parameter_escape_is_explicit(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;
    TZrExecIrValueId result;
    TZrUInt32 edgeIndex;
    TZrBool sawWorker = ZR_FALSE;

    init_function(&function);
    value = add_gc_value(&function, 18u);
    result = add_value(&function, 19u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &value, 1u, 5u, 0u, 0u, 123u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CALL, &value, 1u,
                       &result, 1u, 0u, ZR_EXEC_IR_FLAG_MAY_SUSPEND,
                       0u, 124u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, value)->state == ZR_EXEC_IR_ESCAPE_UNKNOWN);
    assert(fact(&summary, value)->workerEscaped == ZR_TRUE);
    for (edgeIndex = 0u; edgeIndex < summary.edgeCount; ++edgeIndex) {
        const SZrExecIrEscapeEdge *edge =
                ZrParser_ExecIr_EscapeEdgeAt(&summary, edgeIndex);
        if (edge->kind == ZR_EXEC_IR_ESCAPE_KIND_WORKER_PARAMETER) {
            sawWorker = ZR_TRUE;
            break;
        }
    }
    assert(sawWorker);
    assert(strcmp(ZrParser_ExecIr_EscapeStateName(
                         ZR_EXEC_IR_ESCAPE_HEAP_STATIC),
                  "heap/static") == 0);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_store_and_suspend_mark_identity_and_lifetime(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId object;
    TZrExecIrValueId payload;
    TZrExecIrValueId suspended;
    TZrExecIrValueId storeOperands[2];
    TZrExecIrValueId afterSuspend;

    init_function(&function);
    object = add_value(&function, 6u);
    payload = add_value(&function, 7u);
    suspended = add_value(&function, 8u);
    afterSuspend = add_value(&function, 6u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &object, 1u, 1u, 0u, 0u, 131u);
    storeOperands[0] = object;
    storeOperands[1] = payload;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_STORE, storeOperands, 2u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 132u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_SUSPEND, ZR_NULL, 0u,
                       &suspended, 1u, 0u, ZR_EXEC_IR_FLAG_MAY_SUSPEND,
                       0u, 133u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_COPY, &object, 1u,
                       &afterSuspend, 1u, 0u, 0u, 0u, 134u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, object)->identityObserved == ZR_TRUE);
    assert(fact(&summary, payload)->identityObserved == ZR_TRUE);
    assert(fact(&summary, object)->crossesSuspend == ZR_TRUE);
    assert(fact(&summary, object)->workerEscaped == ZR_FALSE);
    assert(fact(&summary, object)->state == ZR_EXEC_IR_ESCAPE_UNKNOWN);
    assert(fact(&summary, object)->liveEnd == 5u);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_parameter_live_after_suspend_is_marked_crossing(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId parameter;
    TZrExecIrValueId suspendResult;

    init_function(&function);
    parameter = add_value(&function, 34u);
    suspendResult = add_value(&function, 35u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_SUSPEND, ZR_NULL, 0u,
                       &suspendResult, 1u, 0u, ZR_EXEC_IR_FLAG_MAY_SUSPEND,
                       0u, 232u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN, &parameter, 1u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 233u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, parameter)->crossesSuspend == ZR_TRUE);
    assert(fact(&summary, parameter)->state == ZR_EXEC_IR_ESCAPE_UNKNOWN);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_unread_value_is_conservative_across_exception_and_suspend(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId object;
    TZrExecIrValueId callResult;
    TZrExecIrValueId suspendResult;

    init_function(&function);
    object = add_gc_value(&function, 46u);
    callResult = add_value(&function, 47u);
    suspendResult = add_value(&function, 48u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &object, 1u, 1u, 0u, 0u, 221u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CALL, ZR_NULL, 0u,
                       &callResult, 1u, 0u, ZR_EXEC_IR_FLAG_MAY_THROW,
                       0u, 222u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_SUSPEND, ZR_NULL, 0u,
                       &suspendResult, 1u, 0u, ZR_EXEC_IR_FLAG_MAY_SUSPEND,
                       0u, 223u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, object)->crossesSuspend == ZR_TRUE);
    assert(fact(&summary, object)->state == ZR_EXEC_IR_ESCAPE_UNKNOWN);
    assert(fact(&summary, object)->decision == ZR_EXEC_IR_ALLOC_HEAP);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_checked_operation_operand_is_exception_observable(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId object;
    TZrExecIrValueId divisor;
    TZrExecIrValueId result;
    TZrExecIrValueId operands[2];

    init_function(&function);
    object = add_gc_value(&function, 49u);
    divisor = add_value(&function, 50u);
    result = add_value(&function, 51u);
    operands[0] = object;
    operands[1] = divisor;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &object, 1u, 1u, 0u, 0u, 224u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_DIV, operands, 2u,
                       &result, 1u, 0u, 0u, 0u, 225u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, object)->state == ZR_EXEC_IR_ESCAPE_UNKNOWN);
    assert(fact(&summary, object)->firstReasonEdge !=
           ZR_EXEC_IR_ESCAPE_EDGE_INVALID);
    assert(summary.edges[fact(&summary, object)->firstReasonEdge].kind ==
           ZR_EXEC_IR_ESCAPE_KIND_EXCEPTION);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_closure_capture_reason_is_explicit(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId object;
    TZrExecIrValueId payload;
    TZrExecIrValueId operands[2];
    TZrUInt32 edgeIndex;
    TZrBool sawCapture = ZR_FALSE;
    TZrBool sawStore = ZR_FALSE;

    init_function(&function);
    object = add_value(&function, 13u);
    payload = add_value(&function, 14u);
    operands[0] = object;
    operands[1] = payload;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &object, 1u, 1u, 0u, 0u, 151u);
    /* A non-zero binding row is the canonical lightweight model marker for
     * a closure capture/store site; the edge still carries the source ID. */
    append_instruction(&function, ZR_EXEC_IR_OPCODE_STORE, operands, 2u,
                       ZR_NULL, 0u, 0u, 0u, 9u, 152u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    for (edgeIndex = 0u; edgeIndex < summary.edgeCount; ++edgeIndex) {
        const SZrExecIrEscapeEdge *edge =
                ZrParser_ExecIr_EscapeEdgeAt(&summary, edgeIndex);
        if (edge->kind == ZR_EXEC_IR_ESCAPE_KIND_CLOSURE_CAPTURE) {
            sawCapture = ZR_TRUE;
        }
        if (edge->kind == ZR_EXEC_IR_ESCAPE_KIND_HEAP_STORE) sawStore = ZR_TRUE;
    }
    assert(sawCapture && sawStore);
    assert(fact(&summary, object)->identityObserved == ZR_FALSE);
    assert(fact(&summary, object)->state == ZR_EXEC_IR_ESCAPE_LOCAL);
    assert(fact(&summary, payload)->identityObserved == ZR_TRUE);
    assert(fact(&summary, payload)->state == ZR_EXEC_IR_ESCAPE_HEAP);
    assert(strcmp(ZrParser_ExecIr_EscapeStateName(
                         ZR_EXEC_IR_ESCAPE_LOCAL),
                  "block") == 0);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_drop_observation_blocks_stack_candidate(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId object;
    TZrExecIrValueId ownershipToken;

    init_function(&function);
    object = add_gc_value(&function, 15u);
    ownershipToken = add_value(&function, 16u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &object, 1u, 1u, 0u, 0u, 171u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_DROP, &object, 1u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 172u);
    /* Keep a second value in the fixture so the test also exercises that a
     * non-owning token is not implicitly treated as a drop root. */
    (void)ownershipToken;
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, object)->dropObservable == ZR_TRUE);
    assert(fact(&summary, object)->decision == ZR_EXEC_IR_ALLOC_HEAP);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_drop_through_alias_blocks_stack_candidate(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId object;
    TZrExecIrValueId alias;

    init_function(&function);
    object = add_gc_value(&function, 42u);
    alias = add_value(&function, 42u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &object, 1u, 1u, 0u, 0u, 173u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_COPY, &object, 1u,
                       &alias, 1u, 0u, 0u, 0u, 174u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_DROP, &alias, 1u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 175u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, alias)->dropObservable == ZR_TRUE);
    assert(fact(&summary, object)->dropObservable == ZR_TRUE);
    assert(fact(&summary, object)->decision == ZR_EXEC_IR_ALLOC_HEAP);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_phi_flow_propagates_return_and_records_definition(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrBlock *entry;
    SZrExecIrBlock *merge;
    TZrExecIrValueId left;
    TZrExecIrValueId right;
    TZrExecIrValueId merged;
    TZrExecIrValueId returnOperand;
    TZrExecIrValueId incomingValues[2];
    SZrExecIrPhiIncoming incoming[2];
    SZrExecIrPhi phi;
    SZrExecIrRange phiRange;
    TZrExecIrBlockId successor = 2u;
    TZrExecIrBlockId predecessor = 1u;

    init_function(&function);
    left = add_value(&function, 43u);
    right = add_value(&function, 43u);
    merged = add_value(&function, 43u);
    assert(ZrCore_ExecIr_FunctionAddBlock(
                   &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u);
    assert(ZrCore_ExecIr_FunctionAddBlock(&function, 0u) == 2u);
    entry = ZrCore_ExecIr_FunctionBlockAt(&function, 1u);
    merge = ZrCore_ExecIr_FunctionBlockAt(&function, 2u);
    assert(entry != ZR_NULL && merge != ZR_NULL);
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(
                   &function, &successor, 1u, &entry->successorRange));
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(
                   &function, &predecessor, 1u, &merge->predecessorRange));
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONSTANT, ZR_NULL, 0u,
                       &left, 1u, 1u, 0u, 0u, 191u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_BRANCH, ZR_NULL, 0u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 192u);
    returnOperand = merged;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN, &returnOperand, 1u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 193u);
    entry->instructionRange.start = 0u;
    entry->instructionRange.count = 2u;
    entry->terminatorInstructionId = 2u;
    merge->instructionRange.start = 2u;
    merge->instructionRange.count = 1u;
    merge->terminatorInstructionId = 3u;
    incomingValues[0] = left;
    incomingValues[1] = right;
    incoming[0].predecessor = 1u;
    incoming[0].value = incomingValues[0];
    incoming[1].predecessor = 1u;
    incoming[1].value = incomingValues[1];
    assert(ZrCore_ExecIr_FunctionAppendPhiIncoming(
                   &function, incoming, 2u, ZR_NULL));
    memset(&phi, 0, sizeof(phi));
    phi.result = merged;
    phi.incomings.start = 0u;
    phi.incomings.count = 2u;
    assert(ZrCore_ExecIr_FunctionAppendPhis(&function, &phi, 1u,
                                            &phiRange));
    merge->phis = phiRange;
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, merged)->definitionInstructionId == 3u);
    assert(fact(&summary, merged)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    assert(fact(&summary, left)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    assert(fact(&summary, right)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_explicit_phi_instruction_flows_all_incomings(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId first;
    TZrExecIrValueId second;
    TZrExecIrValueId merged;
    TZrExecIrValueId phiOperands[2];
    TZrExecIrValueId returnOperand;

    init_function(&function);
    first = add_value(&function, 45u);
    second = add_value(&function, 45u);
    merged = add_value(&function, 45u);
    phiOperands[0] = first;
    phiOperands[1] = second;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_PHI, phiOperands, 2u,
                       &merged, 1u, 0u, 0u, 0u, 211u);
    returnOperand = merged;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN, &returnOperand, 1u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 212u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, merged)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    assert(fact(&summary, first)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    assert(fact(&summary, second)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_valid_cfg_is_analyzed_and_hashed(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrBlock *entry;
    TZrExecIrValueId object;
    TZrUInt64 firstHash;

    init_function(&function);
    object = add_value(&function, 12u);
    assert(ZrCore_ExecIr_FunctionAddBlock(
                   &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) ==
           ZR_EXEC_IR_BLOCK_ID_ENTRY);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &object, 1u, 2u, 0u, 0u, 141u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN, &object, 1u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 142u);
    entry = ZrCore_ExecIr_FunctionBlockAt(&function,
                                          ZR_EXEC_IR_BLOCK_ID_ENTRY);
    assert(entry != ZR_NULL);
    entry->instructionRange.start = 0u;
    entry->instructionRange.count = 2u;
    entry->terminatorInstructionId = 2u;
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, object)->state == ZR_EXEC_IR_ESCAPE_CALLER);
    assert(summary.irHash != 0u);
    assert(summary.irHash == ZrParser_ExecIr_EscapeInputHash(&function));
    firstHash = summary.irHash;
    entry->flags ^= ZR_EXEC_IR_BLOCK_FLAG_COLD;
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(summary.irHash != firstHash);
    firstHash = summary.irHash;
    function.values[0].typeToken = 99u;
    assert(ZrParser_ExecIr_EscapeInputHash(&function) != firstHash);
    function.values[0].typeToken = 12u;
    assert(ZrParser_ExecIr_EscapeInputHash(&function) == firstHash);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_invalid_input_is_diagnosed_without_mutation(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt64 beforeHash;

    init_function(&function);
    function.instructionCount = 1u;
    function.instructionCapacity = 1u;
    function.instructions = ZR_NULL;
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    beforeHash = 0x1234u;
    summary.irHash = beforeHash;
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(!ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    assert(summary.irHash == beforeHash);
    assert(ZrParser_ExecIr_EscapeInputHash(&function) == 0u);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_stale_value_definition_is_diagnosed(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;

    init_function(&function);
    (void)add_value(&function, 32u);
    function.values[0].definition = 9u;
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(!ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    assert(diagnostic.functionToken == function.functionToken);
    assert(diagnostic.instructionId == 9u);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_in_range_definition_without_result_is_diagnosed(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;

    init_function(&function);
    value = add_value(&function, 33u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_NOP, ZR_NULL, 0u,
                       ZR_NULL, 0u, 0u, 0u, 0u, 231u);
    /* The ID is in range, but no instruction actually defines this value. */
    function.values[value - 1u].definition = 1u;
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(!ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    assert(diagnostic.functionToken == function.functionToken);
    assert(diagnostic.instructionId == 1u);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_uninitialized_output_is_replaced_safely(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;

    init_function(&function);
    value = add_gc_value(&function, 41u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &value, 1u, 3u, 0u, 0u, 181u);
    memset(&summary, 0xa5, sizeof(summary));
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(summary.magic == ZR_EXEC_IR_ESCAPE_SUMMARY_MAGIC);
    assert(fact(&summary, value)->valueId == value);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_summary_init_is_idempotent_after_analysis(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;

    init_function(&function);
    value = add_gc_value(&function, 44u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, ZR_NULL, 0u,
                       &value, 1u, 4u, 0u, 0u, 201u);
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(summary.factCount == function.valueCount);
    /* Reinitialising owns and releases the prior arrays exactly once. */
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(summary.magic == ZR_EXEC_IR_ESCAPE_SUMMARY_MAGIC);
    assert(summary.factCount == 0u && summary.edgeCount == 0u);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_malformed_operand_range_reports_location(void) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;

    init_function(&function);
    value = add_value(&function, 31u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONSTANT, ZR_NULL, 0u,
                       &value, 1u, 1u, 0u, 0u, 161u);
    function.instructions[0].operands.start = 1u;
    function.instructions[0].operands.count = 1u;
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(!ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    assert(diagnostic.functionToken == function.functionToken);
    assert(diagnostic.instructionId == 1u);
    assert(diagnostic.sourceId == 161u);
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_allocation_and_ownership_plans_are_hash_bound(void) {
    SZrExecIrFunction allocationFunction;
    SZrExecIrEscapeSummary allocationSummary;
    SZrExecIrAllocationPlan allocationPlan;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId allocationValue;
    SZrExecIrFunction ownershipFunction;
    SZrExecIrEscapeSummary ownershipSummary;
    SZrExecIrOwnershipElisionPlan ownershipPlan;
    TZrBool changed = ZR_FALSE;
    TZrExecIrValueId sourceValue;
    TZrExecIrValueId destinationValue;

    /* A local managed allocation produces a publishable stack-placement
     * witness, while ApplyAllocationPlan deliberately leaves the semantic
     * ALLOC instruction untouched for the backend/frame lowering boundary. */
    init_function(&allocationFunction);
    allocationValue = add_gc_value(&allocationFunction, 61u);
    append_instruction(&allocationFunction, ZR_EXEC_IR_OPCODE_ALLOC,
                       ZR_NULL, 0u, &allocationValue, 1u, 7u,
                       (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_ALLOCATE |
                                   ZR_EXEC_IR_FLAG_MAY_GC),
                       0u, 301u);
    memset(&allocationSummary, 0, sizeof(allocationSummary));
    memset(&allocationPlan, 0, sizeof(allocationPlan));
    ZrParser_ExecIr_EscapeSummaryInit(&allocationSummary);
    ZrParser_ExecIr_AllocationPlanInit(&allocationPlan);
    assert(ZrParser_ExecIr_AnalyzeEscape(&allocationFunction,
                                         &allocationSummary, &diagnostic));
    assert(ZrParser_ExecIr_BuildAllocationPlan(&allocationFunction,
                                               &allocationSummary,
                                               &allocationPlan, &diagnostic));
    assert(allocationPlan.siteCount == 1u);
    assert(allocationPlan.stackCount == 1u);
    assert(allocationPlan.sites[0].decision == ZR_EXEC_IR_ALLOC_STACK);
    assert(ZrParser_ExecIr_ApplyAllocationPlan(&allocationFunction,
                                               &allocationPlan, &diagnostic));
    assert(allocationFunction.instructions[0].opcode ==
           ZR_EXEC_IR_OPCODE_ALLOC);
    allocationFunction.signatureHash ^= 1u;
    assert(!ZrParser_ExecIr_ApplyAllocationPlan(&allocationFunction,
                                                &allocationPlan, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION);
    ZrParser_ExecIr_AllocationPlanFree(&allocationPlan);
    ZrParser_ExecIr_EscapeSummaryFree(&allocationSummary);
    ZrCore_ExecIr_FreeFunction(&allocationFunction);

    /* A unique source whose only remaining use is a COPY feeding RETURN can
     * be changed to MOVE.  The plan is rejected after the IR hash changes,
     * proving that stale ownership facts cannot rewrite a new function. */
    init_function(&ownershipFunction);
    sourceValue = add_value(&ownershipFunction, 62u);
    destinationValue = add_value(&ownershipFunction, 62u);
    append_instruction(&ownershipFunction, ZR_EXEC_IR_OPCODE_COPY,
                       &sourceValue, 1u, &destinationValue, 1u, 0u, 0u,
                       0u, 302u);
    append_instruction(&ownershipFunction, ZR_EXEC_IR_OPCODE_RETURN,
                       &destinationValue, 1u, ZR_NULL, 0u, 0u, 0u, 0u,
                       303u);
    memset(&ownershipSummary, 0, sizeof(ownershipSummary));
    memset(&ownershipPlan, 0, sizeof(ownershipPlan));
    ZrParser_ExecIr_EscapeSummaryInit(&ownershipSummary);
    ZrParser_ExecIr_OwnershipElisionPlanInit(&ownershipPlan);
    assert(ZrParser_ExecIr_AnalyzeEscape(&ownershipFunction,
                                         &ownershipSummary, &diagnostic));
    assert(ZrParser_ExecIr_BuildOwnershipElisionPlan(&ownershipFunction,
                                                     &ownershipSummary,
                                                     &ownershipPlan,
                                                     &diagnostic));
    assert(ownershipPlan.itemCount == 1u);
    assert(ownershipPlan.items[0].kind ==
           ZR_EXEC_IR_OWNERSHIP_ELISION_RETURN_FORWARD);
    assert(ZrParser_ExecIr_ApplyOwnershipElisionPlan(
            &ownershipFunction, &ownershipPlan, &changed, &diagnostic));
    assert(changed == ZR_TRUE);
    assert(ownershipFunction.instructions[0].opcode == ZR_EXEC_IR_OPCODE_MOVE);

    /* Rebuild facts for the now-mutated function before checking the stale
     * plan path; this also exercises the source-map-preserving rollback
     * boundary without relying on undefined pointer state. */
    ownershipFunction.instructions[0].opcode = ZR_EXEC_IR_OPCODE_COPY;
    ownershipFunction.signatureHash ^= 1u;
    changed = ZR_TRUE;
    assert(!ZrParser_ExecIr_ApplyOwnershipElisionPlan(
            &ownershipFunction, &ownershipPlan, &changed, &diagnostic));
    assert(changed == ZR_FALSE);
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION);
    ZrParser_ExecIr_OwnershipElisionPlanFree(&ownershipPlan);
    ZrParser_ExecIr_EscapeSummaryFree(&ownershipSummary);
    ZrCore_ExecIr_FreeFunction(&ownershipFunction);
}

int main(void) {
    test_local_alloc_is_stack_candidate();
    test_alloc_without_concrete_layout_stays_on_heap();
    test_undefined_value_is_function_lifetime_parameter();
    test_parameter_lifetime_flows_through_alias_without_sink();
    test_return_flow_propagates_to_allocation();
    test_conflicting_heap_and_caller_observations_use_upper_bound();
    test_escape_propagates_forward_through_copy();
    test_unknown_call_and_native_retention_are_conservative();
    test_worker_parameter_escape_is_explicit();
    test_store_and_suspend_mark_identity_and_lifetime();
    test_parameter_live_after_suspend_is_marked_crossing();
    test_unread_value_is_conservative_across_exception_and_suspend();
    test_checked_operation_operand_is_exception_observable();
    test_closure_capture_reason_is_explicit();
    test_drop_observation_blocks_stack_candidate();
    test_drop_through_alias_blocks_stack_candidate();
    test_phi_flow_propagates_return_and_records_definition();
    test_explicit_phi_instruction_flows_all_incomings();
    test_valid_cfg_is_analyzed_and_hashed();
    test_invalid_input_is_diagnosed_without_mutation();
    test_stale_value_definition_is_diagnosed();
    test_in_range_definition_without_result_is_diagnosed();
    test_uninitialized_output_is_replaced_safely();
    test_summary_init_is_idempotent_after_analysis();
    test_malformed_operand_range_reports_location();
    test_allocation_and_ownership_plans_are_hash_bound();
    return 0;
}
