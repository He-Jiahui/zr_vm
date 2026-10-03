#include "exec_ir_call_target.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static TZrExecIrFunctionId unique_function_token(const SZrExecIrModule *module,
                                                  TZrMetadataToken token) {
    TZrExecIrFunctionId result = ZR_EXEC_IR_FUNCTION_ID_INVALID;
    TZrUInt32 i;
    if (module == ZR_NULL || token == 0u) return result;
    for (i = 0u; i < module->functionCount; ++i) {
        /* Published contracts may carry the canonical target token even
         * when a hand-built ExecIR function's legacy functionToken differs.
         * Treat either identity as evidence, but keep the match unique. */
        if (module->functions[i].functionToken != token &&
            module->functions[i].contract.targetToken != token) continue;
        if (result != ZR_EXEC_IR_FUNCTION_ID_INVALID) return ZR_EXEC_IR_FUNCTION_ID_INVALID;
        result = module->functions[i].id;
    }
    return result;
}

/* Forward declaration: target resolution uses the optional high-bit fixture
 * hint to distinguish a callable type token from a direct row id. */
static EZrExecIrCallEdgeKind edge_kind_from_binding_row(TZrUInt32 row);
static TZrBool compact_binding_row_valid(TZrUInt32 row);

static TZrExecIrFunctionId resolve_instruction_target(
        const SZrExecIrModule *module, const SZrExecIrInstruction *instruction,
        TZrBool *layoutTargetProof) {
    TZrExecIrFunctionId layoutTarget;
    TZrExecIrFunctionId typeTarget;
    TZrExecIrFunctionId rowTarget = ZR_EXEC_IR_FUNCTION_ID_INVALID;
    EZrExecIrCallEdgeKind rowKind;
    TZrUInt32 encoded;
    if (layoutTargetProof != ZR_NULL) *layoutTargetProof = ZR_FALSE;
    if (module == ZR_NULL || instruction == ZR_NULL) return ZR_EXEC_IR_FUNCTION_ID_INVALID;
    /* Metadata tokens are the semantically meaningful representation used by
     * CallBinding facts.  Prefer them over the compact fixture-only row-id
     * convention so a real binding row index cannot accidentally select an
     * unrelated function merely because the numbers happen to match.  When
     * both fields name a unique function they must agree for an explicitly
     * indirect row: silently choosing one of two contradictory static
     * identities would be a link error, not a legal receiver fallback.  On a
     * plain/direct row, typeToken remains a value type and is ignored. */
    layoutTarget = unique_function_token(module, instruction->layoutId);
    typeTarget = unique_function_token(module, instruction->typeToken);
    rowKind = edge_kind_from_binding_row(instruction->bindingRow);
    if (instruction->bindingRow != 0u &&
        instruction->bindingRow != ZR_CALL_BINDING_SLOT_NONE &&
        compact_binding_row_valid(instruction->bindingRow)) {
        encoded = instruction->bindingRow & ZR_EXEC_IR_BINDING_TARGET_MASK;
        if (encoded != 0u && encoded <= module->functionCount) {
            rowTarget = encoded;
        }
    }
    if (layoutTarget != ZR_EXEC_IR_FUNCTION_ID_INVALID &&
        typeTarget != ZR_EXEC_IR_FUNCTION_ID_INVALID &&
        (rowKind == ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT ||
         rowKind == ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT ||
         rowKind == ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION) &&
        layoutTarget != typeTarget) {
        return ZR_EXEC_IR_FUNCTION_ID_INVALID;
    }
    if (layoutTarget != ZR_EXEC_IR_FUNCTION_ID_INVALID &&
        rowTarget != ZR_EXEC_IR_FUNCTION_ID_INVALID &&
        (rowKind == ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT ||
         rowKind == ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT ||
         rowKind == ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION) &&
        layoutTarget != rowTarget) {
        /* In the compact indirect encoding both the row id and layout token
         * are target evidence.  Contradictory values are a link failure, not
         * a reason to guess which receiver body was intended.  Plain direct
         * rows intentionally do not take this branch because a production
         * BindingFacts row index may differ from the metadata target id. */
        return ZR_EXEC_IR_FUNCTION_ID_INVALID;
    }
    if (layoutTarget != ZR_EXEC_IR_FUNCTION_ID_INVALID) {
        if (layoutTargetProof != ZR_NULL) *layoutTargetProof = ZR_TRUE;
        return layoutTarget;
    }
    /* A non-zero layout token is target evidence, not an optimization hint.
     * If it is missing or ambiguous, do not let a coincidental compact row id
     * hide the link failure. */
    if (instruction->layoutId != 0u) return ZR_EXEC_IR_FUNCTION_ID_INVALID;
    /* The compact fixture encoding puts the candidate id in the low bits of
     * a non-zero row.  Prefer that explicit id over a CALL result type token;
     * a type token is not a function identity in the normal lowering.  When
     * an indirect hint carries both and they disagree, retain an unresolved
     * edge rather than silently selecting one of two contracts. */
    if (rowTarget != ZR_EXEC_IR_FUNCTION_ID_INVALID) {
        if ((rowKind == ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT ||
             rowKind == ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT ||
             rowKind == ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION) &&
            typeTarget != ZR_EXEC_IR_FUNCTION_ID_INVALID &&
            typeTarget != rowTarget) {
            return ZR_EXEC_IR_FUNCTION_ID_INVALID;
        }
        return rowTarget;
    }
    /* `typeToken` is normally the CALL result/callable type, not the
     * published function identity.  Only let an explicitly indirect hint
     * use it as a candidate; a plain/direct row must fall through to its
     * compact id (or remain unknown) so an unrelated type id cannot select a
     * function whose token happens to have the same integer value. */
    if (typeTarget != ZR_EXEC_IR_FUNCTION_ID_INVALID &&
        (rowKind == ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT ||
         rowKind == ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT ||
         rowKind == ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION)) {
        return typeTarget;
    }
    /* Once a producer supplied token evidence, an unmatched token is a link
     * failure/unknown edge.  Do not silently fall through to a coincidental
     * binding-row id and execute a different function. */
    if (instruction->layoutId != 0u ||
        (instruction->typeToken != 0u &&
         (rowKind == ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT ||
          rowKind == ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT ||
          rowKind == ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION))) {
        return ZR_EXEC_IR_FUNCTION_ID_INVALID;
    }
    return ZR_EXEC_IR_FUNCTION_ID_INVALID;
}

static EZrExecIrCallEdgeKind edge_kind_from_binding_row(TZrUInt32 row) {
    const TZrUInt32 hints = row & (ZR_EXEC_IR_BINDING_HINT_VIRTUAL |
                                   ZR_EXEC_IR_BINDING_HINT_INTERFACE |
                                   ZR_EXEC_IR_BINDING_HINT_TYPED);
    if (row == ZR_CALL_BINDING_SLOT_NONE) return ZR_EXEC_IR_CALL_EDGE_DIRECT;
    /* Multiple dispatch-shape bits are contradictory evidence.  Keep the
     * edge unresolved rather than letting precedence below silently choose a
     * representation that the binder never published. */
    if (hints != 0u && (hints & (hints - 1u)) != 0u) {
        return ZR_EXEC_IR_CALL_EDGE_UNKNOWN;
    }
    if ((row & ZR_EXEC_IR_BINDING_HINT_TYPED) != 0u) return ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION;
    if ((row & ZR_EXEC_IR_BINDING_HINT_INTERFACE) != 0u) return ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT;
    if ((row & ZR_EXEC_IR_BINDING_HINT_VIRTUAL) != 0u) return ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT;
    return ZR_EXEC_IR_CALL_EDGE_DIRECT;
}

static TZrBool compact_binding_row_valid(TZrUInt32 row) {
    const TZrUInt32 hints = ZR_EXEC_IR_BINDING_HINT_VIRTUAL |
                            ZR_EXEC_IR_BINDING_HINT_INTERFACE |
                            ZR_EXEC_IR_BINDING_HINT_TYPED;
    const TZrUInt32 selected = row & hints;
    return (TZrBool)((row & ~ZR_EXEC_IR_BINDING_TARGET_MASK & ~hints) == 0u &&
                     (selected == 0u || (selected & (selected - 1u)) == 0u));
}


static TZrExecIrFunctionId resolve_owned_direct(
        const SZrExecIrModule *module, const SZrExecIrFunction *caller,
        const SZrExecIrInstruction *instruction) {
    const SZrExecIrBindingRow *row = ZrCore_ExecIr_FunctionBindingRowAt(
        caller, instruction->bindingRow);
    const SZrExecIrFunction *callee = ZR_NULL;
    TZrUInt32 i;
    if (row == ZR_NULL || row->contract.bindingKind != ZR_CALL_BINDING_DIRECT ||
        row->contract.operation != ZR_CALL_BINDING_OPERATION_CALL ||
        row->contract.targetMetadataToken == 0u ||
        ZR_METADATA_TOKEN_TABLE(row->contract.targetMetadataToken) !=
            ZR_METADATA_TABLE_MEMBER_DEF ||
        ZR_METADATA_TOKEN_RID(row->contract.targetMetadataToken) == 0u ||
        row->location.kind != ZR_CALL_BINDING_RELOCATION_NONE ||
        row->location.targetIndex != ZR_CALL_BINDING_SLOT_NONE ||
        row->location.ownerDepth != 0u || row->location.flags != 0u ||
        row->contract.ownerTypeToken != 0u ||
        row->contract.layoutVersion != 0u ||
        row->contract.layoutHash != 0u ||
        row->contract.dispatchSlot != ZR_CALL_BINDING_SLOT_NONE ||
        module->moduleHash == 0u ||
        row->contract.moduleSignatureHash != module->moduleHash ||
        caller->contract.moduleHash != module->moduleHash) {
        return ZR_EXEC_IR_FUNCTION_ID_INVALID;
    }
    /* Count effective publications before inspecting signatures.  A second
     * canonical token is ambiguous even when its signature is different. */
    for (i = 0u; i < module->functionCount; ++i) {
        const SZrExecIrFunction *candidate = &module->functions[i];
        TZrMetadataToken token = candidate->contract.targetToken != 0u
            ? candidate->contract.targetToken : candidate->functionToken;
        if (token != row->contract.targetMetadataToken) continue;
        if (callee != ZR_NULL) return ZR_EXEC_IR_FUNCTION_ID_INVALID;
        callee = candidate;
    }
    if (callee == ZR_NULL || callee->contract.moduleHash != module->moduleHash ||
        callee->contract.generation == 0u || row->contract.signatureHash == 0u ||
        row->contract.signatureHash != callee->signatureHash ||
        row->contract.signatureHash != callee->contract.signatureHash) {
        return ZR_EXEC_IR_FUNCTION_ID_INVALID;
    }
    return callee->id;
}

TZrBool ZrParser_ExecIr_BuildCallEdge(
        const SZrExecIrModule *module, const SZrExecIrFunction *caller,
        TZrExecIrInstructionId instructionId,
        const SZrExecIrFunctionSummary *summaries, TZrUInt32 summaryCount,
        SZrExecIrCallEdge *edge) {
    const SZrExecIrInstruction *instruction;
    TZrExecIrFunctionId target;
    TZrBool layoutTargetProof = ZR_FALSE;
    TZrBool typed;
    if (module == ZR_NULL || caller == ZR_NULL || edge == ZR_NULL ||
        summaryCount != module->functionCount ||
        (summaryCount != 0u && summaries == ZR_NULL) ||
        caller->id == 0u || caller->id > module->functionCount ||
        &module->functions[caller->id - 1u] != caller ||
        instructionId == 0u || instructionId > caller->instructionCount ||
        caller->instructions == ZR_NULL) return ZR_FALSE;
    instruction = &caller->instructions[instructionId - 1u];
    if (instruction->opcode != ZR_EXEC_IR_OPCODE_CALL &&
        instruction->opcode != ZR_EXEC_IR_OPCODE_INVOKE) return ZR_FALSE;
    memset(edge, 0, sizeof(*edge));
    edge->callerId = caller->id;
    edge->callInstructionId = instructionId;
    typed = (TZrBool)(caller->bindingRowsSchemaVersion ==
                     ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED);
    edge->targetToken = typed ? 0u : (instruction->layoutId != 0u
        ? instruction->layoutId : instruction->typeToken);
    target = typed ? resolve_owned_direct(module, caller, instruction)
        : resolve_instruction_target(module, instruction, &layoutTargetProof);
    if (target != ZR_EXEC_IR_FUNCTION_ID_INVALID) {
        const SZrExecIrFunction *callee;
        if (target > summaryCount) return ZR_FALSE;
        callee = &module->functions[target - 1u];
        edge->calleeId = target;
        edge->targetToken = callee->contract.targetToken != 0u
            ? callee->contract.targetToken : callee->functionToken;
        edge->targetGeneration = callee->contract.generation;
        edge->expectedSignatureHash = callee->signatureHash;
        edge->kind = typed ? ZR_EXEC_IR_CALL_EDGE_DIRECT
            : edge_kind_from_binding_row(instruction->bindingRow);
        if (!typed && instruction->bindingRow != 0u &&
            instruction->bindingRow != ZR_CALL_BINDING_SLOT_NONE &&
            !compact_binding_row_valid(instruction->bindingRow)) {
            edge->kind = ZR_EXEC_IR_CALL_EDGE_NATIVE;
            edge->nativeEffectsUnknown = ZR_TRUE;
        }
        edge->resolved = ZR_TRUE;
        edge->exactReceiver = (TZrBool)(edge->kind == ZR_EXEC_IR_CALL_EDGE_DIRECT ||
                                       layoutTargetProof);
        edge->patchableTarget = summaries[target - 1u].patchable;
    } else {
        edge->kind = typed ? ZR_EXEC_IR_CALL_EDGE_UNKNOWN
            : (instruction->bindingRow == 0u ||
               instruction->bindingRow == ZR_CALL_BINDING_SLOT_NONE
                ? ZR_EXEC_IR_CALL_EDGE_UNKNOWN
                : (!compact_binding_row_valid(instruction->bindingRow)
                    ? ZR_EXEC_IR_CALL_EDGE_NATIVE
                    : ((instruction->bindingRow &
                        (ZR_EXEC_IR_BINDING_HINT_VIRTUAL |
                         ZR_EXEC_IR_BINDING_HINT_INTERFACE |
                         ZR_EXEC_IR_BINDING_HINT_TYPED)) != 0u
                        ? edge_kind_from_binding_row(instruction->bindingRow)
                        : ZR_EXEC_IR_CALL_EDGE_NATIVE)));
        edge->nativeEffectsUnknown = (TZrBool)(typed ||
            edge->kind == ZR_EXEC_IR_CALL_EDGE_NATIVE);
    }
    if (typed) edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_UNSUPPORTED;
    return ZR_TRUE;
}

static TZrBool mismatch_at(SZrExecIrTypedCallMismatch *mismatch,
        EZrExecutionDiagnosticCode code, TZrExecIrFunctionId callerId,
        TZrExecIrInstructionId instructionId, TZrUInt64 expected, TZrUInt64 actual) {
    if (mismatch != ZR_NULL) {
        mismatch->code = code;
        mismatch->callerId = callerId;
        mismatch->instructionId = instructionId;
        mismatch->expected = expected;
        mismatch->actual = actual;
    }
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_TypedCallEdgeMatches(
        const SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
        const SZrExecIrCallEdge *edge, SZrExecIrTypedCallMismatch *mismatch) {
    SZrExecIrCallEdge expected;
    const SZrExecIrFunction *caller;
    TZrBool nativeRequired;
    if (module == ZR_NULL || graph == ZR_NULL || edge == ZR_NULL ||
        edge->callerId == 0u || edge->callerId > module->functionCount) return ZR_FALSE;
    caller = &module->functions[edge->callerId - 1u];
    if (caller->bindingRowsSchemaVersion != ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED)
        return ZR_TRUE;
    if (!ZrParser_ExecIr_BuildCallEdge(module, caller, edge->callInstructionId,
            graph->summaries, graph->summaryCount, &expected)) return ZR_FALSE;
#define ZR_COMPARE_TYPED_FIELD(field)     if (expected.field != edge->field)         return mismatch_at(mismatch, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,             edge->callerId, edge->callInstructionId, expected.field, edge->field)
    ZR_COMPARE_TYPED_FIELD(calleeId);
    ZR_COMPARE_TYPED_FIELD(resolved);
    ZR_COMPARE_TYPED_FIELD(kind);
    ZR_COMPARE_TYPED_FIELD(targetToken);
    ZR_COMPARE_TYPED_FIELD(expectedSignatureHash);
    ZR_COMPARE_TYPED_FIELD(targetGeneration);
    ZR_COMPARE_TYPED_FIELD(exactReceiver);
    ZR_COMPARE_TYPED_FIELD(guarded);
    ZR_COMPARE_TYPED_FIELD(patchableTarget);
    ZR_COMPARE_TYPED_FIELD(inlineEligible);
    ZR_COMPARE_TYPED_FIELD(inlineReason);
#undef ZR_COMPARE_TYPED_FIELD
    nativeRequired = expected.nativeEffectsUnknown;
    if (expected.resolved && !expected.patchableTarget) {
        const SZrExecIrFunctionSummary *callee =
            &graph->summaries[expected.calleeId - 1u];
        if (callee->unknownEffects &&
            callee->unknownReason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE)
            nativeRequired = ZR_TRUE;
    }
    /* A conservative true value is legal after summary widening. */
    if (nativeRequired && !edge->nativeEffectsUnknown)
        return mismatch_at(mismatch, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
            edge->callerId, edge->callInstructionId, ZR_TRUE, ZR_FALSE);
    return ZR_TRUE;
}

typedef struct SZrExecIrTypedCallSite {
    TZrExecIrFunctionId callerId;
    TZrExecIrInstructionId instructionId;
} SZrExecIrTypedCallSite;

static int compare_sites(const void *left, const void *right) {
    const SZrExecIrTypedCallSite *a = (const SZrExecIrTypedCallSite *)left;
    const SZrExecIrTypedCallSite *b = (const SZrExecIrTypedCallSite *)right;
    if (a->callerId < b->callerId) return -1;
    if (a->callerId > b->callerId) return 1;
    if (a->instructionId < b->instructionId) return -1;
    if (a->instructionId > b->instructionId) return 1;
    return 0;
}

TZrBool ZrParser_ExecIr_TypedCallSitesMatch(
        const SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
        SZrExecIrTypedCallMismatch *mismatch) {
    SZrExecIrTypedCallSite *sites = ZR_NULL;
    TZrUInt32 count = 0u, cursor = 0u, i, j;
    TZrBool result = ZR_TRUE;
    if (module == ZR_NULL || graph == ZR_NULL) return ZR_FALSE;
    /* Defensive bounds precede filtering and indexing even though the public
     * validator also proves them before calling this private helper. */
    for (i = 0u; i < graph->edgeCount; ++i) {
        const SZrExecIrCallEdge *edge = &graph->edges[i];
        const SZrExecIrFunction *caller;
        if (edge->callerId == 0u || edge->callerId > module->functionCount)
            return mismatch_at(mismatch, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                0u, edge->callInstructionId, module->functionCount, edge->callerId);
        caller = &module->functions[edge->callerId - 1u];
        if (edge->callInstructionId == 0u ||
            edge->callInstructionId > caller->instructionCount)
            return mismatch_at(mismatch, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                edge->callerId, edge->callInstructionId,
                caller->instructionCount, edge->callInstructionId);
        if (caller->bindingRowsSchemaVersion == ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED) {
            if (count == UINT32_MAX)
                return mismatch_at(mismatch, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                    edge->callerId, edge->callInstructionId, 0u, count);
            ++count;
        }
    }
    if (count != 0u) {
        if (sizeof(*sites) > SIZE_MAX / (size_t)count)
            return mismatch_at(mismatch, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                0u, 0u, 0u, count);
        sites = (SZrExecIrTypedCallSite *)malloc((size_t)count * sizeof(*sites));
        if (sites == ZR_NULL)
            return mismatch_at(mismatch, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                0u, 0u, 0u, count);
        for (i = 0u; i < graph->edgeCount; ++i) {
            const SZrExecIrCallEdge *edge = &graph->edges[i];
            if (module->functions[edge->callerId - 1u].bindingRowsSchemaVersion !=
                ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED) continue;
            sites[cursor].callerId = edge->callerId;
            sites[cursor++].instructionId = edge->callInstructionId;
        }
        qsort(sites, count, sizeof(*sites), compare_sites);
        for (i = 1u; i < count; ++i) {
            if (compare_sites(&sites[i - 1u], &sites[i]) == 0) {
                result = mismatch_at(mismatch, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                    sites[i].callerId, sites[i].instructionId, 1u, 2u);
                goto cleanup;
            }
        }
    }
    cursor = 0u;
    for (i = 0u; i < module->functionCount; ++i) {
        const SZrExecIrFunction *caller = &module->functions[i];
        if (caller->bindingRowsSchemaVersion != ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED)
            continue;
        for (j = 0u; j < caller->instructionCount; ++j) {
            TZrUInt16 opcode = caller->instructions[j].opcode;
            if (opcode != ZR_EXEC_IR_OPCODE_CALL && opcode != ZR_EXEC_IR_OPCODE_INVOKE)
                continue;
            if (cursor >= count || sites[cursor].callerId != caller->id ||
                sites[cursor].instructionId != j + 1u) {
                result = mismatch_at(mismatch, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                    caller->id, j + 1u, 1u, 0u);
                goto cleanup;
            }
            ++cursor;
        }
    }
cleanup:
    free(sites);
    return result;
}
