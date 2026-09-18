#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_parser/exec_ir_interprocedural.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static SZrExecIrRange empty_range(void) {
    SZrExecIrRange range;
    memset(&range, 0, sizeof(range));
    return range;
}

static TZrExecIrValueId add_value(SZrExecIrFunction *function,
                                  EZrExecIrOwnership ownership) {
    return ZrCore_ExecIr_FunctionAddValue(function, 1u, ownership,
                                           ZR_EXEC_IR_NULLABILITY_UNKNOWN);
}

static TZrExecIrValueId add_external_value(SZrExecIrFunction *function,
                                           EZrExecIrOwnership ownership) {
    return ZrCore_ExecIr_FunctionAddExternalValue(
            function, 1u, ownership, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
}

static void append_constant(SZrExecIrFunction *function,
                            TZrExecIrValueId value, TZrUInt32 bits,
                            TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    SZrExecIrRange results;
    TZrExecIrInstructionId instructionId;
    memset(&instruction, 0, sizeof(instruction));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &value, 1u, &results));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = results;
    instruction.layoutId = bits;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                    &instructionId));
}

static void append_return(SZrExecIrFunction *function,
                          TZrExecIrValueId value,
                          TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    SZrExecIrRange operands;
    TZrExecIrInstructionId instructionId;
    memset(&instruction, 0, sizeof(instruction));
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &value, 1u, &operands));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = operands;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                    &instructionId));
}

static void append_call(SZrExecIrFunction *function,
                        TZrExecIrValueId result,
                        TZrUInt32 bindingRow,
                        TZrUInt32 targetToken,
                        TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    SZrExecIrRange results = empty_range();
    TZrExecIrInstructionId instructionId;
    memset(&instruction, 0, sizeof(instruction));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u, &results));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CALL;
    instruction.results = results;
    instruction.bindingRow = bindingRow;
    instruction.layoutId = targetToken;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                    &instructionId));
}

static void append_call_with_operand(SZrExecIrFunction *function,
                                     TZrExecIrValueId result,
                                     TZrExecIrValueId operand,
                                     TZrUInt32 bindingRow,
                                     TZrUInt32 targetToken,
                                     TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    SZrExecIrRange operands;
    SZrExecIrRange results;
    TZrExecIrInstructionId instructionId;
    memset(&instruction, 0, sizeof(instruction));
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &operand, 1u,
                                                 &operands));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u,
                                                &results));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CALL;
    instruction.operands = operands;
    instruction.results = results;
    instruction.bindingRow = bindingRow;
    instruction.layoutId = targetToken;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                    &instructionId));
}

static void append_binary(SZrExecIrFunction *function,
                          EZrExecIrOpcode opcode,
                          TZrExecIrValueId left,
                          TZrExecIrValueId right,
                          TZrExecIrValueId result,
                          TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    SZrExecIrRange operands;
    SZrExecIrRange results;
    TZrExecIrValueId values[2] = {left, right};
    TZrExecIrInstructionId instructionId;
    memset(&instruction, 0, sizeof(instruction));
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, values, 2u, &operands));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u, &results));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                    &instructionId));
}

static SZrExecIrFunction *add_function(SZrExecIrModule *module,
                                        TZrMetadataToken token,
                                        TZrUInt64 signature,
                                        TZrExecIrFunctionId *outId) {
    assert(ZrCore_ExecIr_ModuleAddFunction(module, token, signature, outId));
    return ZrCore_ExecIr_ModuleFunctionAt(module, *outId);
}

/* Publication is explicit in ExecIR: only a sealed body is a frozen target
 * that a cross-function pass may safely read across a hot-reload boundary. */
static void publish_function(SZrExecIrFunction *function) {
    assert(function != ZR_NULL);
    function->sealed = ZR_TRUE;
}

static void add_source_map(SZrExecIrFunction *function,
                           TZrExecIrInstructionId instructionId,
                           TZrExecIrSourceId sourceId) {
    function->sourceMaps = (SZrExecIrSourceMap *)calloc(1u, sizeof(*function->sourceMaps));
    assert(function->sourceMaps != ZR_NULL);
    function->sourceMapCapacity = 1u;
    function->sourceMapCount = 1u;
    function->sourceMaps[0].instructionId = instructionId;
    function->sourceMaps[0].sourceId = sourceId;
}

static void test_recursive_summary_and_unknown_native(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId fId, gId;
    SZrExecIrFunction *f;
    SZrExecIrFunction *g;
    TZrExecIrValueId fResult, gResult, unknownResult;
    const SZrExecIrFunctionSummary *fSummary;
    const SZrExecIrFunctionSummary *gSummary;
    const SZrExecIrCallEdge *unknownEdge = ZR_NULL;
    TZrUInt32 i;

    ZrCore_ExecIr_ModuleInit(&module);
    f = add_function(&module, 100u, UINT64_C(0x100), &fId);
    g = add_function(&module, 200u, UINT64_C(0x200), &gId);
    fResult = add_value(f, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    gResult = add_value(g, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    unknownResult = add_value(g, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_call(f, fResult, gId, 0u, 11u);
    append_call(g, gResult, fId, 0u, 21u);
    append_call(g, unknownResult, 123u, 0u, 22u);
    publish_function(f);
    publish_function(g);

    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(ZrParser_ExecIr_CallGraphValidate(&graph, &module, &diagnostic));
    fSummary = ZrParser_ExecIr_CallGraphSummaryAt(&graph, fId);
    gSummary = ZrParser_ExecIr_CallGraphSummaryAt(&graph, gId);
    assert(fSummary != ZR_NULL && gSummary != ZR_NULL);
    assert(fSummary->sccId == gSummary->sccId);
    assert(fSummary->unknownEffects == ZR_TRUE);
    assert(fSummary->unknownReason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE);
    assert(gSummary->unknownEffects == ZR_TRUE);
    assert(gSummary->unknownReason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE);
    assert(gSummary->firstUnknownInstructionId == 2u);
    assert((fSummary->effects & ZR_EXEC_IR_SUMMARY_EFFECT_ALL) ==
           ZR_EXEC_IR_SUMMARY_EFFECT_ALL);
    for (i = 0u; i < graph.edgeCount; ++i) {
        if (graph.edges[i].callerId == gId && !graph.edges[i].resolved) {
            unknownEdge = &graph.edges[i];
            break;
        }
    }
    assert(unknownEdge != ZR_NULL);
    assert(unknownEdge->kind == ZR_EXEC_IR_CALL_EDGE_NATIVE);
    assert(unknownEdge->nativeEffectsUnknown == ZR_TRUE);
    assert(graph.graphHash != 0u && graph.revision == 1u);
    {
        TZrUInt64 stableHash = graph.graphHash;
        assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
        assert(graph.revision == 1u);
        assert(graph.graphHash == stableHash);
    }
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_virtual_static_binding_and_guarded_resolution(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId calleeId, callerId;
    SZrExecIrFunction *callee;
    SZrExecIrFunction *caller;
    TZrExecIrValueId calleeValue, callerValue;
    SZrExecIrCallTargetRequest request;
    SZrExecIrCallTargetResult result;

    ZrCore_ExecIr_ModuleInit(&module);
    callee = add_function(&module, 300u, UINT64_C(0x333), &calleeId);
    caller = add_function(&module, 400u, UINT64_C(0x444), &callerId);
    /* A relocated publication token may differ from the local function-array
     * token; target resolution must retain that canonical contract identity. */
    callee->contract.targetToken = 330u;
    calleeValue = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    callerValue = add_value(caller, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_constant(callee, calleeValue, 9u, 31u);
    append_return(callee, calleeValue, 32u);
    publish_function(callee);
    /* The virtual hint carries no target id; layoutId is a stable metadata
     * token and is enough for an exact receiver proof in this model. */
    append_call(caller, callerValue, UINT32_C(0x20000000), 300u, 41u);

    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(graph.edgeCount == 1u);
    assert(graph.edges[0].kind == ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT);
    assert(graph.edges[0].resolved == ZR_TRUE);
    assert(graph.edges[0].exactReceiver == ZR_TRUE);
    assert(ZrParser_ExecIr_DevirtualizeCalls(&module, &graph, &diagnostic));
    assert(caller->instructions[0].bindingRow == calleeId);

    memset(&request, 0, sizeof(request));
    request.bindingKind = ZR_CALL_BINDING_DIRECT;
    request.targetFunctionId = calleeId;
    request.targetToken = callee->contract.targetToken;
    request.signatureHash = callee->signatureHash;
    request.moduleHash = graph.moduleHash;
    request.generation = callee->contract.generation;
    request.exactReceiver = ZR_TRUE;
    request.targetFrozen = ZR_TRUE;
    request.closureContextValid = ZR_TRUE;
    assert(ZrParser_ExecIr_ResolveCallTarget(&graph, &request, &result,
                                              &diagnostic));
    assert(result.resolved == ZR_TRUE);
    assert(result.kind == ZR_EXEC_IR_CALL_EDGE_DIRECT);
    assert(result.guarded == ZR_FALSE);
    assert(result.inlineAllowed == ZR_TRUE);

    /* A profile/deopt proof is a guarded specialization, not a permanent
     * direct edge.  The slot representation must survive until the guard is
     * checked at runtime. */
    request.bindingKind = ZR_CALL_BINDING_VIRTUAL;
    request.targetFrozen = ZR_FALSE;
    request.profileMonomorphic = ZR_TRUE;
    request.deoptMapAvailable = ZR_TRUE;
    request.exactReceiver = ZR_TRUE;
    assert(ZrParser_ExecIr_ResolveCallTarget(&graph, &request, &result,
                                             &diagnostic));
    assert(result.resolved == ZR_TRUE);
    assert(result.kind == ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT);
    assert(result.guarded == ZR_TRUE);
    assert(result.inlineAllowed == ZR_FALSE);

    request.bindingKind = ZR_CALL_BINDING_DIRECT;
    request.targetFrozen = ZR_TRUE;
    request.profileMonomorphic = ZR_FALSE;
    request.deoptMapAvailable = ZR_FALSE;
    request.signatureHash += 1u;
    assert(!ZrParser_ExecIr_ResolveCallTarget(&graph, &request, &result,
                                               &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    request.signatureHash = callee->signatureHash;
    request.generation += 1u;
    assert(!ZrParser_ExecIr_ResolveCallTarget(&graph, &request, &result,
                                               &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION);
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_pure_inline_preserves_callsite_and_source(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId calleeId, callerId;
    SZrExecIrFunction *callee;
    SZrExecIrFunction *caller;
    TZrExecIrValueId calleeValue, callerValue;

    ZrCore_ExecIr_ModuleInit(&module);
    callee = add_function(&module, 500u, UINT64_C(0x555), &calleeId);
    caller = add_function(&module, 600u, UINT64_C(0x666), &callerId);
    calleeValue = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    callerValue = add_value(caller, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_constant(callee, calleeValue, 17u, 51u);
    append_return(callee, calleeValue, 52u);
    add_source_map(callee, 1u, 5101u);
    publish_function(callee);
    append_call(caller, callerValue, calleeId, 0u, 61u);

    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(graph.edges[0].inlineEligible == ZR_TRUE);
    assert(ZrParser_ExecIr_InlineCalls(&module, &graph, &diagnostic));
    assert(caller->instructionCount == 1u);
    assert(caller->instructions[0].opcode == ZR_EXEC_IR_OPCODE_CONSTANT);
    assert(caller->instructions[0].bindingRow == 0u);
    assert(caller->instructions[0].sourceId == 61u);
    assert(caller->sourceMapCount == 1u);
    assert(caller->sourceMaps[0].instructionId == 1u);
    assert(caller->sourceMaps[0].sourceId == 5101u);
    assert(ZrCore_ExecIr_ValidateModule(&module, &diagnostic));
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_identity_return_forwarding_uses_copy(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId calleeId, callerId;
    SZrExecIrFunction *callee;
    SZrExecIrFunction *caller;
    TZrExecIrValueId parameter, argument, result;

    ZrCore_ExecIr_ModuleInit(&module);
    callee = add_function(&module, 650u, UINT64_C(0x650), &calleeId);
    caller = add_function(&module, 651u, UINT64_C(0x651), &callerId);
    parameter = add_external_value(callee, ZR_EXEC_IR_OWNERSHIP_BORROWED);
    argument = add_external_value(caller, ZR_EXEC_IR_OWNERSHIP_BORROWED);
    result = add_value(caller, ZR_EXEC_IR_OWNERSHIP_BORROWED);
    append_return(callee, parameter, 6511u);
    publish_function(callee);
    append_call_with_operand(caller, result, argument, calleeId, 0u, 6512u);
    add_source_map(callee, 1u, 6513u);

    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(graph.edges[0].inlineEligible == ZR_TRUE);
    assert(ZrParser_ExecIr_InlineCalls(&module, &graph, &diagnostic));
    assert(caller->instructionCount == 1u);
    assert(caller->instructions[0].opcode == ZR_EXEC_IR_OPCODE_COPY);
    assert(caller->instructions[0].operandRange.count == 1u);
    assert(caller->operands[caller->instructions[0].operandRange.start] == argument);
    assert(caller->results[caller->instructions[0].resultRange.start] == result);
    assert(caller->sourceMapCount == 1u);
    assert(caller->sourceMaps[0].instructionId == 1u);
    assert(caller->sourceMaps[0].sourceId == 6513u);
    assert(ZrCore_ExecIr_ValidateModule(&module, &diagnostic));
    assert(ZrCore_ExecIr_VerifyFunction(caller,
                                        (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                                                ZR_EXEC_IR_VERIFY_SSA),
                                        &diagnostic));
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_inline_budget_rejects_without_mutation(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInlineBudget budget;
    TZrExecIrFunctionId calleeId, callerId;
    SZrExecIrFunction *callee;
    SZrExecIrFunction *caller;
    TZrExecIrValueId first, second, callerValue;

    ZrCore_ExecIr_ModuleInit(&module);
    callee = add_function(&module, 700u, UINT64_C(0x777), &calleeId);
    caller = add_function(&module, 800u, UINT64_C(0x888), &callerId);
    first = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    second = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    callerValue = add_value(caller, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_constant(callee, first, 1u, 71u);
    append_constant(callee, second, 2u, 72u);
    append_return(callee, second, 73u);
    publish_function(callee);
    append_call(caller, callerValue, calleeId, 0u, 81u);
    ZrParser_ExecIr_CallGraphInit(&graph);
    memset(&budget, 0, sizeof(budget));
    budget.maxCostPerCall = 1u;
    budget.maxGrowthPerFunction = 8u;
    budget.maxGrowthPerModule = 8u;
    budget.maxRecursiveDepth = 0u;
    assert(ZrParser_ExecIr_CallGraphSetInlineBudget(&graph, &budget, &diagnostic));
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(ZrParser_ExecIr_InlineCalls(&module, &graph, &diagnostic));
    assert(caller->instructionCount == 1u);
    assert(caller->instructions[0].opcode == ZR_EXEC_IR_OPCODE_CALL);
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_inline_parameter_and_arithmetic_body(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId calleeId, callerId;
    SZrExecIrFunction *callee;
    SZrExecIrFunction *caller;
    TZrExecIrValueId parameter, constant, sum, argument, result;
    SZrExecIrRange argumentRange;

    ZrCore_ExecIr_ModuleInit(&module);
    callee = add_function(&module, 850u, UINT64_C(0x850), &calleeId);
    caller = add_function(&module, 851u, UINT64_C(0x851), &callerId);
    parameter = add_external_value(callee, ZR_EXEC_IR_OWNERSHIP_BORROWED);
    constant = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    sum = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    argument = add_external_value(caller, ZR_EXEC_IR_OWNERSHIP_BORROWED);
    result = add_value(caller, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_constant(callee, constant, 5u, 8511u);
    append_binary(callee, ZR_EXEC_IR_OPCODE_ADD, parameter, constant, sum, 8512u);
    append_return(callee, sum, 8513u);
    publish_function(callee);
    assert(ZrCore_ExecIr_FunctionAppendOperands(caller, &argument, 1u,
                                                 &argumentRange));
    {
        SZrExecIrInstruction instruction;
        SZrExecIrRange results;
        TZrExecIrInstructionId instructionId;
        memset(&instruction, 0, sizeof(instruction));
        assert(ZrCore_ExecIr_FunctionAppendResults(caller, &result, 1u, &results));
        instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CALL;
        instruction.operands = argumentRange;
        instruction.results = results;
        instruction.bindingRow = calleeId;
        instruction.sourceId = 8514u;
        assert(ZrCore_ExecIr_FunctionAppendInstruction(caller, &instruction,
                                                        &instructionId));
    }
    append_return(caller, result, 8515u);
    add_source_map(caller, 2u, 8515u);
    {
        TZrExecIrBlockId blockId = ZrCore_ExecIr_FunctionAddBlock(
                caller, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
        assert(blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY);
        caller->blocks[0].instructionRange.start = 0u;
        caller->blocks[0].instructionRange.count = caller->instructionCount;
        caller->blocks[0].terminatorInstructionId = caller->instructionCount;
    }
    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(graph.edges[0].inlineEligible == ZR_TRUE);
    assert(ZrParser_ExecIr_InlineCalls(&module, &graph, &diagnostic));
    assert(caller->instructionCount == 3u);
    assert(caller->instructions[0].opcode == ZR_EXEC_IR_OPCODE_CONSTANT);
    assert(caller->instructions[1].opcode == ZR_EXEC_IR_OPCODE_ADD);
    assert(caller->instructions[2].opcode == ZR_EXEC_IR_OPCODE_RETURN);
    assert(caller->blocks[0].instructionRange.count == 3u);
    assert(caller->blocks[0].terminatorInstructionId == 3u);
    assert(caller->sourceMaps[0].instructionId == 3u);
    assert(caller->instructions[1].results.count == 1u);
    assert(caller->results[caller->instructions[1].results.start] == result);
    assert(ZrCore_ExecIr_ValidateModule(&module, &diagnostic));
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_inline_updates_linear_block_metadata(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId calleeId, callerId;
    SZrExecIrFunction *callee;
    SZrExecIrFunction *caller;
    TZrExecIrValueId parameter, constant, sum, argument, result;
    TZrExecIrBlockId blockId;

    ZrCore_ExecIr_ModuleInit(&module);
    callee = add_function(&module, 875u, UINT64_C(0x875), &calleeId);
    caller = add_function(&module, 876u, UINT64_C(0x876), &callerId);
    parameter = add_external_value(callee, ZR_EXEC_IR_OWNERSHIP_BORROWED);
    constant = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    sum = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    argument = add_external_value(caller, ZR_EXEC_IR_OWNERSHIP_BORROWED);
    result = add_value(caller, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_constant(callee, constant, 9u, 8751u);
    append_binary(callee, ZR_EXEC_IR_OPCODE_ADD, parameter, constant, sum, 8752u);
    append_return(callee, sum, 8753u);
    publish_function(callee);
    append_call_with_operand(caller, result, argument, calleeId, 0u, 8761u);
    append_return(caller, result, 8762u);
    blockId = ZrCore_ExecIr_FunctionAddBlock(caller,
                                              ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    assert(blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY);
    caller->blocks[0].instructions.start = 0u;
    caller->blocks[0].instructions.count = 2u;
    caller->blocks[0].terminatorInstructionId = 2u;

    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(graph.edges[0].inlineEligible == ZR_TRUE);
    assert(ZrParser_ExecIr_InlineCalls(&module, &graph, &diagnostic));
    assert(caller->instructionCount == 3u);
    assert(caller->blocks[0].instructions.count == 3u);
    assert(caller->blocks[0].terminatorInstructionId == 3u);
    assert(caller->instructions[2].opcode == ZR_EXEC_IR_OPCODE_RETURN);
    assert(ZrCore_ExecIr_VerifyFunction(
                caller,
                (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                        ZR_EXEC_IR_VERIFY_SSA),
                &diagnostic));
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_patchable_target_stays_on_slot(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId calleeId, callerId;
    SZrExecIrFunction *callee;
    SZrExecIrFunction *caller;
    TZrExecIrValueId calleeValue, callerValue;

    ZrCore_ExecIr_ModuleInit(&module);
    callee = add_function(&module, 900u, UINT64_C(0x999), &calleeId);
    caller = add_function(&module, 901u, UINT64_C(0x99a), &callerId);
    callee->contract.generation = 2u; /* published replacement / patchable */
    calleeValue = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    callerValue = add_value(caller, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_constant(callee, calleeValue, 4u, 91u);
    append_return(callee, calleeValue, 92u);
    append_call(caller, callerValue, UINT32_C(0x20000000) | calleeId, 0u, 93u);
    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(graph.edges[0].patchableTarget == ZR_TRUE);
    assert(graph.edges[0].inlineEligible == ZR_FALSE);
    assert(graph.edges[0].inlineReason == ZR_EXEC_IR_INLINE_REASON_PATCHABLE_TARGET);
    assert(ZrParser_ExecIr_DevirtualizeCalls(&module, &graph, &diagnostic));
    assert(caller->instructions[0].bindingRow == (UINT32_C(0x20000000) | calleeId));
    assert(ZrParser_ExecIr_InlineCalls(&module, &graph, &diagnostic));
    assert(caller->instructions[0].opcode == ZR_EXEC_IR_OPCODE_CALL);
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_unsealed_initial_target_is_not_frozen(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId calleeId, callerId;
    SZrExecIrFunction *callee;
    SZrExecIrFunction *caller;
    TZrExecIrValueId calleeValue, callerValue;

    ZrCore_ExecIr_ModuleInit(&module);
    callee = add_function(&module, 925u, UINT64_C(0x925), &calleeId);
    caller = add_function(&module, 926u, UINT64_C(0x926), &callerId);
    calleeValue = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    callerValue = add_value(caller, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_constant(callee, calleeValue, 12u, 9751u);
    append_return(callee, calleeValue, 9752u);
    /* ModuleAddFunction starts at generation one, but publication is still
     * mutable until the producer seals the body. */
    append_call(caller, callerValue, calleeId, 0u, 9753u);

    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(graph.summaries[calleeId - 1u].targetFrozen == ZR_FALSE);
    assert(graph.summaries[calleeId - 1u].patchable == ZR_TRUE);
    assert(graph.edges[0].inlineEligible == ZR_FALSE);
    assert(graph.edges[0].inlineReason ==
           ZR_EXEC_IR_INLINE_REASON_PATCHABLE_TARGET);
    assert(ZrParser_ExecIr_InlineCalls(&module, &graph, &diagnostic));
    assert(caller->instructions[0].opcode == ZR_EXEC_IR_OPCODE_CALL);
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_polymorphic_slot_without_exact_receiver_is_not_inlined(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId calleeId, callerId;
    SZrExecIrFunction *callee;
    SZrExecIrFunction *caller;
    TZrExecIrValueId calleeValue, callerValue;

    ZrCore_ExecIr_ModuleInit(&module);
    callee = add_function(&module, 950u, UINT64_C(0x950), &calleeId);
    caller = add_function(&module, 951u, UINT64_C(0x951), &callerId);
    calleeValue = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    callerValue = add_value(caller, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_constant(callee, calleeValue, 6u, 101u);
    append_return(callee, calleeValue, 102u);
    publish_function(callee);
    /* The row carries a virtual target hint and id, but no receiver-layout
     * evidence.  Resolving a candidate body is not enough to rewrite all
     * polymorphic receivers to that body. */
    append_call(caller, callerValue,
                UINT32_C(0x20000000) | calleeId, 0u, 103u);

    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(graph.edges[0].kind == ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT);
    assert(graph.edges[0].resolved == ZR_TRUE);
    assert(graph.edges[0].exactReceiver == ZR_FALSE);
    assert(graph.edges[0].inlineEligible == ZR_FALSE);
    assert(graph.edges[0].inlineReason == ZR_EXEC_IR_INLINE_REASON_SIGNATURE);
    assert(ZrParser_ExecIr_DevirtualizeCalls(&module, &graph, &diagnostic));
    assert(caller->instructions[0].bindingRow ==
           (UINT32_C(0x20000000) | calleeId));
    /* No graph mutation means the inline pass remains a successful no-op. */
    assert(ZrParser_ExecIr_InlineCalls(&module, &graph, &diagnostic));
    assert(caller->instructions[0].opcode == ZR_EXEC_IR_OPCODE_CALL);
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_binding_row_without_table_is_not_erased(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId calleeId, callerId;
    SZrExecIrFunction *callee;
    SZrExecIrFunction *caller;
    TZrExecIrValueId calleeValue, callerValue;

    ZrCore_ExecIr_ModuleInit(&module);
    callee = add_function(&module, 975u, UINT64_C(0x975), &calleeId);
    caller = add_function(&module, 976u, UINT64_C(0x976), &callerId);
    calleeValue = add_value(callee, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    callerValue = add_value(caller, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_constant(callee, calleeValue, 7u, 104u);
    append_return(callee, calleeValue, 105u);
    publish_function(callee);
    /* A normal row index plus a target-layout token is the shape emitted by
     * BindingFacts projection.  This API has no row-table argument, so it
     * must not overwrite the row with a raw function id. */
    append_call(caller, callerValue, 7u, callee->functionToken, 106u);

    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(graph.edges[0].resolved == ZR_TRUE);
    assert(graph.edges[0].kind == ZR_EXEC_IR_CALL_EDGE_DIRECT);
    assert(graph.edges[0].inlineEligible == ZR_FALSE);
    assert(graph.edges[0].inlineReason == ZR_EXEC_IR_INLINE_REASON_SIGNATURE);
    assert(ZrParser_ExecIr_DevirtualizeCalls(&module, &graph, &diagnostic));
    assert(caller->instructions[0].bindingRow == 7u);
    assert(ZrParser_ExecIr_InlineCalls(&module, &graph, &diagnostic));
    assert(caller->instructions[0].opcode == ZR_EXEC_IR_OPCODE_CALL);
    assert(caller->instructions[0].bindingRow == 7u);
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_deep_import_hash_invalidates_in_reverse_declaration_order(void) {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrFunctionId aId, bId, cId;
    SZrExecIrFunction *a;
    SZrExecIrFunction *b;
    SZrExecIrFunction *c;
    TZrExecIrValueId aValue, bValue, cValue;
    TZrUInt64 firstGraphHash;
    TZrUInt64 firstAImport;
    TZrUInt64 firstBImport;

    ZrCore_ExecIr_ModuleInit(&module);
    /* Declaration order deliberately differs from dependency order: A calls
     * B and B calls C, while A is declared first.  This catches one-pass
     * imported-hash implementations that only invalidate callers appearing
     * after their callees in the array. */
    a = add_function(&module, 1100u, UINT64_C(0x1100), &aId);
    b = add_function(&module, 1101u, UINT64_C(0x1101), &bId);
    c = add_function(&module, 1102u, UINT64_C(0x1102), &cId);
    cValue = add_value(c, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    bValue = add_value(b, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    aValue = add_value(a, ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    append_constant(c, cValue, 1u, 111u);
    append_return(c, cValue, 112u);
    append_call(b, bValue, cId, 0u, 113u);
    append_call(a, aValue, bId, 0u, 114u);
    publish_function(a);
    publish_function(b);
    publish_function(c);

    ZrParser_ExecIr_CallGraphInit(&graph);
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    firstGraphHash = graph.graphHash;
    firstAImport = graph.summaries[aId - 1u].importedHash;
    firstBImport = graph.summaries[bId - 1u].importedHash;
    assert(firstGraphHash != 0u && firstAImport != 0u && firstBImport != 0u);

    /* A leaf-body change must invalidate both direct and transitive callers. */
    c->instructions[0].layoutId = 2u;
    assert(ZrParser_ExecIr_BuildCallGraph(&module, &graph, &diagnostic));
    assert(graph.graphHash != firstGraphHash);
    assert(graph.summaries[aId - 1u].importedHash != firstAImport);
    assert(graph.summaries[bId - 1u].importedHash != firstBImport);
    ZrParser_ExecIr_CallGraphFree(&graph);
    ZrCore_ExecIr_FreeModule(&module);
}

int main(void) {
    test_recursive_summary_and_unknown_native();
    test_virtual_static_binding_and_guarded_resolution();
    test_pure_inline_preserves_callsite_and_source();
    test_identity_return_forwarding_uses_copy();
    test_inline_budget_rejects_without_mutation();
    test_inline_parameter_and_arithmetic_body();
    test_inline_updates_linear_block_metadata();
    test_patchable_target_stays_on_slot();
    test_unsealed_initial_target_is_not_frozen();
    test_polymorphic_slot_without_exact_receiver_is_not_inlined();
    test_binding_row_without_table_is_not_erased();
    test_deep_import_hash_invalidates_in_reverse_declaration_order();
    return 0;
}
