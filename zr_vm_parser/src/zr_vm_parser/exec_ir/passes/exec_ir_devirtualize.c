#include "zr_vm_parser/exec_ir_interprocedural.h"

#include <string.h>

static TZrExecIrBlockId devirt_block_for_instruction(
        const SZrExecIrFunction *function, TZrExecIrInstructionId instructionId) {
    TZrUInt32 i;
    if (function == ZR_NULL || instructionId == 0u ||
        function->blocks == ZR_NULL || function->blockCount > function->blockCapacity ||
        instructionId > function->instructionCount) return ZR_EXEC_IR_BLOCK_ID_INVALID;
    for (i = 0u; i < function->blockCount; ++i) {
        const SZrExecIrBlock *block = &function->blocks[i];
        if (block->instructionRange.start > function->instructionCount ||
            block->instructionRange.count > function->instructionCount -
                block->instructionRange.start) continue;
        if (instructionId - 1u >= block->instructionRange.start &&
            instructionId - 1u < block->instructionRange.start +
                                  block->instructionRange.count) return block->id;
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static void devirt_diag(SZrExecIrDiagnostic *diagnostic,
                        EZrExecutionDiagnosticCode code,
                        const SZrExecIrFunction *function,
                        TZrUInt32 instructionId,
                        TZrUInt64 expected,
                        TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->blockId = devirt_block_for_instruction(function, instructionId);
    if (function != ZR_NULL && instructionId != 0u &&
        instructionId <= function->instructionCount &&
        instructionId <= function->instructionCapacity &&
        function->instructions != ZR_NULL) {
        diagnostic->sourceId = function->instructions[instructionId - 1u].sourceId;
    }
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
}

static EZrExecIrCallEdgeKind kind_from_row(TZrUInt32 row) {
    if (row == ZR_CALL_BINDING_SLOT_NONE) return ZR_EXEC_IR_CALL_EDGE_DIRECT;
    if ((row & ZR_EXEC_IR_BINDING_HINT_TYPED) != 0u) return ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION;
    if ((row & ZR_EXEC_IR_BINDING_HINT_INTERFACE) != 0u) return ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT;
    if ((row & ZR_EXEC_IR_BINDING_HINT_VIRTUAL) != 0u) return ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT;
    return ZR_EXEC_IR_CALL_EDGE_DIRECT;
}

static TZrBool function_mutable_and_well_formed(const SZrExecIrFunction *function) {
    return (TZrBool)(function != ZR_NULL && !function->sealed &&
                     function->instructionCount <= function->instructionCapacity &&
                     (function->instructionCount == 0u || function->instructions != ZR_NULL));
}

TZrBool ZrParser_ExecIr_DevirtualizeCalls(
        SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (module == ZR_NULL || graph == ZR_NULL ||
        !ZrParser_ExecIr_CallGraphValidate(graph, module, diagnostic)) return ZR_FALSE;

    /* Preflight all link identities before changing any caller.  This keeps a
     * stale later edge from leaving an earlier callsite half-devirtualized. */
    for (i = 0u; i < graph->edgeCount; ++i) {
        const SZrExecIrCallEdge *edge = &graph->edges[i];
        const SZrExecIrFunction *caller;
        const SZrExecIrFunction *callee;
        const SZrExecIrFunctionSummary *summary;
        if (!edge->resolved || edge->callerId == 0u || edge->calleeId == 0u ||
            edge->callerId > module->functionCount || edge->calleeId > module->functionCount ||
            edge->callInstructionId == 0u) continue;
        caller = &module->functions[edge->callerId - 1u];
        callee = &module->functions[edge->calleeId - 1u];
        summary = ZrParser_ExecIr_CallGraphSummaryAt(graph, edge->calleeId);
        if (summary == ZR_NULL || summary->validity != ZR_EXEC_IR_SUMMARY_STABLE ||
            summary->unknownEffects || summary->patchable || !summary->targetFrozen) continue;
        if (edge->targetToken != callee->functionToken &&
            (callee->contract.targetToken == 0u ||
             edge->targetToken != callee->contract.targetToken)) {
            devirt_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                        caller, edge->callInstructionId,
                        callee->contract.targetToken != 0u
                            ? callee->contract.targetToken : callee->functionToken,
                        edge->targetToken);
            return ZR_FALSE;
        }
        if (edge->targetGeneration != callee->contract.generation) {
            devirt_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,
                        caller, edge->callInstructionId,
                        callee->contract.generation, edge->targetGeneration);
            return ZR_FALSE;
        }
        if (edge->expectedSignatureHash != callee->signatureHash) {
            devirt_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                        caller, edge->callInstructionId,
                        callee->signatureHash, edge->expectedSignatureHash);
            return ZR_FALSE;
        }
    }

    for (i = 0u; i < graph->edgeCount; ++i) {
        const SZrExecIrCallEdge *edge = &graph->edges[i];
        SZrExecIrFunction *caller;
        SZrExecIrInstruction *call;
        const SZrExecIrFunctionSummary *summary;
        EZrExecIrCallEdgeKind sourceKind;
        if (!edge->resolved || edge->callerId == 0u || edge->calleeId == 0u ||
            edge->callerId > module->functionCount || edge->calleeId > module->functionCount ||
            edge->callInstructionId == 0u) continue;
        caller = &module->functions[edge->callerId - 1u];
        summary = ZrParser_ExecIr_CallGraphSummaryAt(graph, edge->calleeId);
        if (summary == ZR_NULL || summary->validity != ZR_EXEC_IR_SUMMARY_STABLE ||
            summary->unknownEffects || summary->patchable || !summary->targetFrozen) continue;
        if (!function_mutable_and_well_formed(caller) ||
            edge->callInstructionId > caller->instructionCount) continue;
        call = &caller->instructions[edge->callInstructionId - 1u];
        sourceKind = kind_from_row(call->bindingRow);
        if (sourceKind == ZR_EXEC_IR_CALL_EDGE_DIRECT && edge->kind == ZR_EXEC_IR_CALL_EDGE_DIRECT &&
            (call->bindingRow & ZR_EXEC_IR_BINDING_TARGET_MASK) == edge->calleeId) {
            continue; /* already direct; no representation churn */
        }
        if (sourceKind == ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION ||
            edge->kind == ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION ||
            edge->kind == ZR_EXEC_IR_CALL_EDGE_UNKNOWN ||
            edge->kind == ZR_EXEC_IR_CALL_EDGE_NATIVE) {
            /* Closure context/signature validation belongs to the typed-call
             * binder; a raw function-id rewrite would erase that contract. */
            continue;
        }
        /* A non-zero ordinary row is owned by the binding-facts table in the
         * production projection.  This minimal pass has no row-table input,
         * so only rewrite such a row when it already uses the compact
         * function-id convention; otherwise retaining the row preserves the
         * caller's contract until a facts-aware consumer runs. */
        if (edge->kind == ZR_EXEC_IR_CALL_EDGE_DIRECT &&
            call->bindingRow != 0u &&
            call->bindingRow != ZR_CALL_BINDING_SLOT_NONE &&
            (call->layoutId != 0u ||
             (call->bindingRow & ZR_EXEC_IR_BINDING_TARGET_MASK) != edge->calleeId)) {
            continue;
        }
        /* An exact receiver can be statically bound.  A monomorphic profile
         * is accepted only when the caller has a deopt map; otherwise retain
         * the slot baseline. */
        if (!edge->exactReceiver || edge->guarded || edge->patchableTarget) continue;
        call->bindingRow = edge->calleeId;
    }
    return ZrCore_ExecIr_ValidateModule(module, diagnostic);
}

TZrBool ZrParser_ExecIr_RunDevirtualize(
        SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
        SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_DevirtualizeCalls(module, graph, diagnostic);
}
