#include "zr_vm_parser/exec_ir_escape.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/exec_ir_state_map.h"

/*
 * Ownership elision is intentionally a very small, proof-driven rewrite.
 * A COPY is changed to MOVE only when both values are UNIQUE, the source's
 * sole use is the copy itself, and no metadata or observable drop can still
 * refer to the source.  The result value and instruction identity remain
 * unchanged, so source maps, deopt IDs, and state-map checkpoints stay valid.
 */

static void zr_ownership_diag(SZrExecIrDiagnostic *diagnostic,
                              EZrExecutionDiagnosticCode code,
                              const SZrExecIrFunction *function,
                              TZrExecIrInstructionId instructionId,
                              TZrExecIrSourceId sourceId) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = sourceId;
}

static void zr_ownership_hash_diag(SZrExecIrDiagnostic *diagnostic,
                                   const SZrExecIrFunction *function,
                                   TZrUInt64 expected,
                                   TZrUInt64 actual) {
    zr_ownership_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,
                      function, 0u, 0u);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
}

static TZrBool zr_ownership_range_valid(SZrExecIrRange range,
                                        TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool zr_ownership_plan_storage_valid(
        const SZrExecIrOwnershipElisionPlan *plan) {
    TZrUInt32 magic = 0u;
    TZrUInt32 count = 0u;
    TZrUInt32 capacity = 0u;
    SZrExecIrOwnershipElision *items = ZR_NULL;

    if (plan == ZR_NULL) return ZR_FALSE;
    memcpy(&magic, &plan->magic, sizeof(magic));
    memcpy(&count, &plan->itemCount, sizeof(count));
    memcpy(&capacity, &plan->itemCapacity, sizeof(capacity));
    memcpy(&items, &plan->items, sizeof(items));
    if (magic != ZR_EXEC_IR_OWNERSHIP_PLAN_MAGIC || count > capacity) {
        return ZR_FALSE;
    }
    return (TZrBool)((capacity == 0u && items == ZR_NULL) ||
                     (capacity != 0u && items != ZR_NULL));
}

static void zr_ownership_plan_release(SZrExecIrOwnershipElisionPlan *plan) {
    SZrExecIrOwnershipElision *items = ZR_NULL;
    if (!zr_ownership_plan_storage_valid(plan)) return;
    memcpy(&items, &plan->items, sizeof(items));
    free(items);
}

void ZrParser_ExecIr_OwnershipElisionPlanInit(
        SZrExecIrOwnershipElisionPlan *plan) {
    if (plan == ZR_NULL) return;
    zr_ownership_plan_release(plan);
    memset(plan, 0, sizeof(*plan));
    plan->magic = ZR_EXEC_IR_OWNERSHIP_PLAN_MAGIC;
}

void ZrParser_ExecIr_OwnershipElisionPlanFree(
        SZrExecIrOwnershipElisionPlan *plan) {
    if (plan == ZR_NULL) return;
    zr_ownership_plan_release(plan);
    memset(plan, 0, sizeof(*plan));
}

const TZrChar *ZrParser_ExecIr_OwnershipElisionReasonName(
        EZrExecIrOwnershipElisionReason reason) {
    switch (reason) {
        case ZR_EXEC_IR_OWNERSHIP_REASON_UNIQUE_LAST_USE:
            return "unique-last-use";
        case ZR_EXEC_IR_OWNERSHIP_REASON_RETURN_FORWARD:
            return "return-forward";
        case ZR_EXEC_IR_OWNERSHIP_REASON_UNKNOWN_OWNERSHIP:
            return "unknown-ownership";
        case ZR_EXEC_IR_OWNERSHIP_REASON_SOURCE_ESCAPES:
            return "source-escapes";
        case ZR_EXEC_IR_OWNERSHIP_REASON_DROP_OBSERVABLE:
            return "drop-observable";
        case ZR_EXEC_IR_OWNERSHIP_REASON_METADATA_USE:
            return "metadata-use";
        case ZR_EXEC_IR_OWNERSHIP_REASON_ALIAS_UNPROVEN:
            return "alias-unproven";
        case ZR_EXEC_IR_OWNERSHIP_REASON_NOT_LAST_USE:
            return "not-last-use";
        case ZR_EXEC_IR_OWNERSHIP_REASON_INVALID:
        case ZR_EXEC_IR_OWNERSHIP_REASON_COUNT:
        default:
            return "invalid";
    }
}

static TZrBool zr_ownership_summary_valid(
        const SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 magic = 0u;
    TZrUInt32 factCount = 0u;
    TZrUInt32 factCapacity = 0u;
    SZrExecIrEscapeFact *facts = ZR_NULL;
    TZrUInt64 summaryHash = 0u;
    TZrUInt64 inputHash;

    if (function == ZR_NULL || summary == ZR_NULL) {
        zr_ownership_diag(diagnostic,
                          ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          function, 0u, 0u);
        return ZR_FALSE;
    }
    memcpy(&magic, &summary->magic, sizeof(magic));
    memcpy(&factCount, &summary->factCount, sizeof(factCount));
    memcpy(&factCapacity, &summary->factCapacity, sizeof(factCapacity));
    memcpy(&facts, &summary->facts, sizeof(facts));
    memcpy(&summaryHash, &summary->irHash, sizeof(summaryHash));
    if (magic != ZR_EXEC_IR_ESCAPE_SUMMARY_MAGIC ||
        factCount > factCapacity || factCount != function->valueCount ||
        (factCount != 0u && facts == ZR_NULL)) {
        zr_ownership_diag(diagnostic,
                          ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          function, 0u, 0u);
        return ZR_FALSE;
    }
    inputHash = ZrParser_ExecIr_EscapeInputHash(function);
    if (inputHash == 0u || summaryHash != inputHash) {
        zr_ownership_hash_diag(diagnostic, function, inputHash, summaryHash);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_ownership_metadata_valid(
        const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (function == ZR_NULL) return ZR_FALSE;
    if ((function->gcRootCount != 0u && function->gcRoots == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValues == ZR_NULL) ||
        (function->deoptStateCount != 0u && function->deoptStates == ZR_NULL) ||
        function->deoptStateCount > function->deoptStateCapacity) {
        zr_ownership_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          function, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_ValidateDeoptAggregates(function, diagnostic)) return ZR_FALSE;
    for (index = 0u; index < function->deoptStateCount; ++index) {
        if (!zr_ownership_range_valid(function->deoptStates[index].reconstruction,
                                      function->deoptValueCount)) {
            zr_ownership_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                              function, 0u, 0u);
            return ZR_FALSE;
        }
    }
    if (function->stateMap != ZR_NULL) {
        const SZrExecIrStateMap *map = function->stateMap;
        if ((map->entryCount != 0u && map->entries == ZR_NULL) ||
            (map->valueCount != 0u && map->valuePool == ZR_NULL) ||
            (map->rootCount != 0u && map->rootPool == ZR_NULL) ||
            (map->ownerStateCount != 0u && map->ownerStatePool == ZR_NULL) ||
            map->entryCount > map->entryCapacity ||
            map->valueCount > map->valueCapacity ||
            map->rootCount > map->rootCapacity ||
            map->ownerStateCount > map->ownerStateCapacity) {
            zr_ownership_diag(diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                              function, 0u, 0u);
            return ZR_FALSE;
        }
        for (index = 0u; index < map->entryCount; ++index) {
            const SZrExecIrStateMapEntry *entry = &map->entries[index];
            if (!zr_ownership_range_valid(entry->liveValues, map->valueCount) ||
                !zr_ownership_range_valid(entry->rootValues, map->rootCount) ||
                !zr_ownership_range_valid(entry->ownerStates,
                                          map->ownerStateCount) ||
                (entry->instructionId != 0u &&
                 entry->instructionId > function->instructionCount)) {
                zr_ownership_diag(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                  function, entry->instructionId,
                                  entry->sourceId);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_ownership_metadata_references(
        const SZrExecIrFunction *function, TZrExecIrValueId valueId) {
    TZrUInt32 index;
    if (function == ZR_NULL) return ZR_TRUE;
    if (ZrCore_ExecIr_DeoptAggregateValueReferenced(function, valueId)) return ZR_TRUE;
    for (index = 0u; index < function->gcRootCount; ++index) {
        if (function->gcRoots[index] == valueId) return ZR_TRUE;
    }
    for (index = 0u; index < function->deoptValueCount; ++index) {
        if (function->deoptValues[index] == valueId) return ZR_TRUE;
    }
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        TZrUInt32 at;
        for (at = state->reconstruction.start;
             at < state->reconstruction.start + state->reconstruction.count;
             ++at) {
            if (function->deoptValues[at] == valueId) return ZR_TRUE;
        }
    }
    if (function->stateMap != ZR_NULL) {
        const SZrExecIrStateMap *map = function->stateMap;
        for (index = 0u; index < map->entryCount; ++index) {
            const SZrExecIrStateMapEntry *entry = &map->entries[index];
            TZrUInt32 at;
            for (at = entry->liveValues.start;
                 at < entry->liveValues.start + entry->liveValues.count; ++at) {
                if (map->valuePool[at] == valueId) return ZR_TRUE;
            }
            for (at = entry->rootValues.start;
                 at < entry->rootValues.start + entry->rootValues.count; ++at) {
                if (map->rootPool[at] == valueId) return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_ownership_instruction_valid(
        const SZrExecIrFunction *function,
        TZrExecIrInstructionId instructionId,
        const SZrExecIrInstruction **outInstruction,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrInstruction *instruction;
    if (outInstruction != ZR_NULL) *outInstruction = ZR_NULL;
    if (function == ZR_NULL || instructionId == 0u ||
        instructionId > function->instructionCount ||
        function->instructions == ZR_NULL) {
        zr_ownership_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                          function, instructionId, 0u);
        return ZR_FALSE;
    }
    instruction = &function->instructions[instructionId - 1u];
    if (!zr_ownership_range_valid(instruction->operands,
                                  function->operandCount) ||
        !zr_ownership_range_valid(instruction->results,
                                  function->resultCount) ||
        !zr_ownership_range_valid(instruction->phiRange,
                                  function->phiIncomingCount) ||
        !zr_ownership_range_valid(instruction->successorRange,
                                  function->successorCount) ||
        !zr_ownership_range_valid(instruction->memoryIn,
                                  function->memoryTokenCount) ||
        !zr_ownership_range_valid(instruction->memoryOut,
                                  function->memoryTokenCount)) {
        zr_ownership_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                          function, instructionId, instruction->sourceId);
        return ZR_FALSE;
    }
    if (outInstruction != ZR_NULL) *outInstruction = instruction;
    return ZR_TRUE;
}

static TZrBool zr_ownership_count_uses(
        const SZrExecIrFunction *function, TZrExecIrValueId valueId,
        TZrExecIrInstructionId ignoredInstruction,
        TZrUInt32 *count, TZrExecIrInstructionId *lastInstruction,
        EZrExecIrOpcode *lastOpcode) {
    TZrUInt32 instructionIndex;
    if (count != ZR_NULL) *count = 0u;
    if (lastInstruction != ZR_NULL) *lastInstruction = 0u;
    if (lastOpcode != ZR_NULL) *lastOpcode = ZR_EXEC_IR_OPCODE_INVALID;
    if (function == ZR_NULL || valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
        valueId > function->valueCount) return ZR_FALSE;
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction = &function->instructions[instructionIndex];
        TZrUInt32 operandIndex;
        if (!zr_ownership_range_valid(instruction->operands,
                                      function->operandCount)) return ZR_FALSE;
        for (operandIndex = instruction->operands.start;
             operandIndex < instruction->operands.start + instruction->operands.count;
             ++operandIndex) {
            if (function->operands[operandIndex] == valueId &&
                instructionIndex + 1u != ignoredInstruction) {
                if (count != ZR_NULL) ++*count;
                if (lastInstruction != ZR_NULL) *lastInstruction = instructionIndex + 1u;
                if (lastOpcode != ZR_NULL) {
                    *lastOpcode = (EZrExecIrOpcode)instruction->opcode;
                }
            }
        }
    }
    /* Phi incoming values are uses at the merge instruction.  They are kept
     * separate from ordinary operand uses so a copy feeding a phi can never
     * be mistaken for a simple last-use transfer. */
    for (instructionIndex = 0u; instructionIndex < function->blockCount;
         ++instructionIndex) {
        const SZrExecIrBlock *block = &function->blocks[instructionIndex];
        TZrUInt32 phiIndex;
        if (!zr_ownership_range_valid(block->phis, function->phiCount)) return ZR_FALSE;
        for (phiIndex = block->phis.start;
             phiIndex < block->phis.start + block->phis.count; ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            TZrUInt32 incomingIndex;
            if (!zr_ownership_range_valid(phi->incomings,
                                          function->phiIncomingCount)) return ZR_FALSE;
            for (incomingIndex = phi->incomings.start;
                 incomingIndex < phi->incomings.start + phi->incomings.count;
                 ++incomingIndex) {
                if (function->phiIncoming[incomingIndex].value == valueId &&
                    block->instructionRange.start + 1u != ignoredInstruction) {
                    if (count != ZR_NULL) ++*count;
                    if (lastInstruction != ZR_NULL) {
                        *lastInstruction = block->instructionRange.start + 1u;
                    }
                    if (lastOpcode != ZR_NULL) *lastOpcode = ZR_EXEC_IR_OPCODE_PHI;
                }
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_ownership_copy_candidate(
        const SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        TZrExecIrInstructionId instructionId,
        SZrExecIrOwnershipElision *item,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrInstruction *instruction;
    TZrExecIrValueId sourceValue;
    TZrExecIrValueId destinationValue;
    const SZrExecIrEscapeFact *sourceFact;
    const SZrExecIrEscapeFact *destinationFact;
    TZrUInt32 sourceUses = 0u;
    TZrUInt32 destinationUses = 0u;
    TZrExecIrInstructionId destinationLastUse = 0u;
    EZrExecIrOpcode destinationLastOpcode = ZR_EXEC_IR_OPCODE_INVALID;

    if (!zr_ownership_instruction_valid(function, instructionId, &instruction,
                                        diagnostic)) return ZR_FALSE;
    if ((EZrExecIrOpcode)instruction->opcode != ZR_EXEC_IR_OPCODE_COPY ||
        instruction->operands.count != 1u || instruction->results.count != 1u ||
        instruction->flags != 0u || instruction->deoptId != 0u ||
        instruction->bindingRow != 0u || instruction->effectIn != 0u ||
        instruction->effectOut != 0u || instruction->memoryIn.count != 0u ||
        instruction->memoryOut.count != 0u || instruction->phiRange.count != 0u ||
        instruction->successorRange.count != 0u || function->operands == ZR_NULL ||
        function->results == ZR_NULL) return ZR_FALSE;
    sourceValue = function->operands[instruction->operands.start];
    destinationValue = function->results[instruction->results.start];
    if (sourceValue == ZR_EXEC_IR_VALUE_ID_INVALID ||
        destinationValue == ZR_EXEC_IR_VALUE_ID_INVALID ||
        sourceValue > function->valueCount || destinationValue > function->valueCount ||
        sourceValue == destinationValue) return ZR_FALSE;
    sourceFact = ZrParser_ExecIr_EscapeFactAt(summary, sourceValue);
    destinationFact = ZrParser_ExecIr_EscapeFactAt(summary, destinationValue);
    if (sourceFact == ZR_NULL || destinationFact == ZR_NULL) return ZR_FALSE;
    if (function->values[sourceValue - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
        function->values[destinationValue - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNIQUE) {
        return ZR_FALSE;
    }
    if (sourceFact->state == ZR_EXEC_IR_ESCAPE_UNKNOWN ||
        sourceFact->crossesSuspend || sourceFact->nativeRetained ||
        sourceFact->workerEscaped || sourceFact->dropObservable ||
        destinationFact->crossesSuspend || destinationFact->nativeRetained ||
        destinationFact->workerEscaped || destinationFact->dropObservable) {
        return ZR_FALSE;
    }
    if (zr_ownership_metadata_references(function, sourceValue)) return ZR_FALSE;
    if (!zr_ownership_count_uses(function, sourceValue, instructionId,
                                 &sourceUses, ZR_NULL, ZR_NULL) ||
        sourceUses != 0u || sourceFact->lastUseInstructionId != instructionId) {
        /* Count uses while ignoring the candidate copy.  Any remaining use
         * proves that moving from the source would change ownership state. */
        return ZR_FALSE;
    }
    if (!zr_ownership_count_uses(function, destinationValue, 0u,
                                 &destinationUses, &destinationLastUse,
                                 &destinationLastOpcode)) return ZR_FALSE;
    if (destinationUses == 0u) return ZR_FALSE;
    if (destinationFact->identityObserved &&
        !(destinationUses == 1u && destinationLastOpcode == ZR_EXEC_IR_OPCODE_RETURN)) {
        return ZR_FALSE;
    }
    if (destinationUses != 1u ||
        (destinationLastOpcode != ZR_EXEC_IR_OPCODE_RETURN &&
         destinationLastOpcode != ZR_EXEC_IR_OPCODE_MOVE &&
         destinationLastOpcode != ZR_EXEC_IR_OPCODE_DROP)) {
        return ZR_FALSE;
    }
    if (destinationLastUse <= instructionId) return ZR_FALSE;
    if (item != ZR_NULL) {
        item->instructionId = instructionId;
        item->sourceValueId = sourceValue;
        item->destinationValueId = destinationValue;
        item->kind = destinationLastOpcode == ZR_EXEC_IR_OPCODE_RETURN
                         ? ZR_EXEC_IR_OWNERSHIP_ELISION_RETURN_FORWARD
                         : ZR_EXEC_IR_OWNERSHIP_ELISION_COPY_TO_MOVE;
        item->reason = destinationLastOpcode == ZR_EXEC_IR_OPCODE_RETURN
                           ? ZR_EXEC_IR_OWNERSHIP_REASON_RETURN_FORWARD
                           : ZR_EXEC_IR_OWNERSHIP_REASON_UNIQUE_LAST_USE;
    }
    return ZR_TRUE;
}

static TZrBool zr_ownership_append(SZrExecIrOwnershipElisionPlan *plan,
                                   const SZrExecIrOwnershipElision *item,
                                   const SZrExecIrFunction *function,
                                   SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOwnershipElision *items;
    TZrUInt32 capacity;
    if (plan == ZR_NULL || item == ZR_NULL || plan->itemCount == UINT32_MAX) {
        zr_ownership_diag(diagnostic,
                          ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                          function, item != ZR_NULL ? item->instructionId : 0u,
                          0u);
        return ZR_FALSE;
    }
    if (plan->itemCount == plan->itemCapacity) {
        capacity = plan->itemCapacity == 0u ? 8u : plan->itemCapacity;
        if (capacity > UINT32_MAX / 2u) capacity = UINT32_MAX;
        else capacity *= 2u;
#if SIZE_MAX < UINT32_MAX
        if (capacity > (TZrUInt32)(SIZE_MAX / sizeof(*items))) {
            zr_ownership_diag(diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                              function, item->instructionId, 0u);
            return ZR_FALSE;
        }
#endif
        items = (SZrExecIrOwnershipElision *)realloc(
                plan->items, (size_t)capacity * sizeof(*items));
        if (items == ZR_NULL) {
            zr_ownership_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                              function, item->instructionId, 0u);
            return ZR_FALSE;
        }
        plan->items = items;
        plan->itemCapacity = capacity;
    }
    plan->items[plan->itemCount++] = *item;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_BuildOwnershipElisionPlan(
        const SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        SZrExecIrOwnershipElisionPlan *plan,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOwnershipElisionPlan candidate;
    TZrUInt32 instructionIndex;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL || plan == ZR_NULL ||
        !ZrCore_ExecIr_VerifyFunction(
                function,
                (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                       ZR_EXEC_IR_VERIFY_SSA),
                diagnostic) || !zr_ownership_metadata_valid(function, diagnostic) ||
        !zr_ownership_summary_valid(function, summary, diagnostic)) {
        return ZR_FALSE;
    }
    memset(&candidate, 0, sizeof(candidate));
    ZrParser_ExecIr_OwnershipElisionPlanInit(&candidate);
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        SZrExecIrOwnershipElision item;
        if (!zr_ownership_copy_candidate(function, summary,
                                          instructionIndex + 1u, &item,
                                          diagnostic)) {
            if (diagnostic != ZR_NULL &&
                diagnostic->code != ZR_EXECUTION_DIAGNOSTIC_NONE) {
                goto fail;
            }
            continue;
        }
        if (!zr_ownership_append(&candidate, &item, function, diagnostic)) goto fail;
    }
    candidate.irHash = summary->irHash;
    candidate.changed = (TZrBool)(candidate.itemCount != 0u);
    ZrParser_ExecIr_OwnershipElisionPlanFree(plan);
    *plan = candidate;
    return ZR_TRUE;

fail:
    ZrParser_ExecIr_OwnershipElisionPlanFree(&candidate);
    return ZR_FALSE;
}

static TZrBool zr_ownership_apply_item(
        SZrExecIrFunction *function,
        const SZrExecIrOwnershipElision *item,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrInstruction *constInstruction;
    SZrExecIrInstruction *instruction;
    TZrExecIrValueId sourceValue;
    TZrExecIrValueId destinationValue;
    if (item == ZR_NULL || item->instructionId == 0u ||
        item->kind <= ZR_EXEC_IR_OWNERSHIP_ELISION_NONE ||
        item->kind >= ZR_EXEC_IR_OWNERSHIP_ELISION_KIND_COUNT ||
        item->reason >= ZR_EXEC_IR_OWNERSHIP_REASON_COUNT ||
        !zr_ownership_instruction_valid(function, item->instructionId,
                                         &constInstruction, diagnostic)) {
        return ZR_FALSE;
    }
    instruction = &function->instructions[item->instructionId - 1u];
    if ((EZrExecIrOpcode)instruction->opcode != ZR_EXEC_IR_OPCODE_COPY ||
        instruction->operands.count != 1u || instruction->results.count != 1u ||
        instruction->flags != 0u || instruction->deoptId != 0u ||
        instruction->bindingRow != 0u || instruction->effectIn != 0u ||
        instruction->effectOut != 0u || instruction->memoryIn.count != 0u ||
        instruction->memoryOut.count != 0u || instruction->phiRange.count != 0u ||
        instruction->successorRange.count != 0u || function->operands == ZR_NULL ||
        function->results == ZR_NULL) {
        zr_ownership_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                          function, item->instructionId, instruction->sourceId);
        return ZR_FALSE;
    }
    sourceValue = function->operands[instruction->operands.start];
    destinationValue = function->results[instruction->results.start];
    if (sourceValue != item->sourceValueId || destinationValue != item->destinationValueId ||
        sourceValue == ZR_EXEC_IR_VALUE_ID_INVALID ||
        destinationValue == ZR_EXEC_IR_VALUE_ID_INVALID ||
        sourceValue > function->valueCount || destinationValue > function->valueCount ||
        function->values[sourceValue - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
        function->values[destinationValue - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNIQUE) {
        zr_ownership_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                          function, item->instructionId, instruction->sourceId);
        return ZR_FALSE;
    }
    instruction->opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_MOVE;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ApplyOwnershipElisionPlan(
        SZrExecIrFunction *function,
        const SZrExecIrOwnershipElisionPlan *plan,
        TZrBool *changed,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrFunction snapshot;
    TZrUInt32 magic = 0u;
    TZrUInt64 planHash = 0u;
    TZrUInt64 inputHash;
    TZrUInt32 index;

    if (changed != ZR_NULL) *changed = ZR_FALSE;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL || plan == ZR_NULL || function->sealed ||
        !zr_ownership_plan_storage_valid(plan) ||
        !ZrCore_ExecIr_VerifyFunction(
                function,
                (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                       ZR_EXEC_IR_VERIFY_SSA),
                diagnostic)) {
        if (function != ZR_NULL && function->sealed) {
            zr_ownership_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_SEALED,
                              function, 0u, 0u);
        } else if (diagnostic != ZR_NULL &&
                   diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE) {
            zr_ownership_diag(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              function, 0u, 0u);
        }
        return ZR_FALSE;
    }
    memcpy(&magic, &plan->magic, sizeof(magic));
    memcpy(&planHash, &plan->irHash, sizeof(planHash));
    if (magic != ZR_EXEC_IR_OWNERSHIP_PLAN_MAGIC) {
        zr_ownership_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          function, 0u, 0u);
        return ZR_FALSE;
    }
    inputHash = ZrParser_ExecIr_EscapeInputHash(function);
    if (inputHash == 0u || inputHash != planHash) {
        zr_ownership_hash_diag(diagnostic, function, inputHash, planHash);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_FunctionInit(&snapshot);
    if (!ZrCore_ExecIr_CloneFunction(function, &snapshot, diagnostic)) return ZR_FALSE;
    for (index = 0u; index < plan->itemCount; ++index) {
        if (!zr_ownership_apply_item(function, &plan->items[index], diagnostic)) {
            goto rollback;
        }
    }
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL,
                                      diagnostic)) goto rollback;
    if (changed != ZR_NULL) *changed = (TZrBool)(plan->itemCount != 0u);
    ZrCore_ExecIr_FreeFunction(&snapshot);
    return ZR_TRUE;

rollback:
    ZrCore_ExecIr_FreeFunction(function);
    *function = snapshot;
    ZrCore_ExecIr_FunctionInit(&snapshot);
    ZrCore_ExecIr_FreeFunction(&snapshot);
    if (changed != ZR_NULL) *changed = ZR_FALSE;
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_ElideAllocationAndCopies(
        SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOwnershipElisionPlan plan;
    TZrBool changed = ZR_FALSE;
    TZrBool result;

    memset(&plan, 0, sizeof(plan));
    ZrParser_ExecIr_OwnershipElisionPlanInit(&plan);
    if (!ZrParser_ExecIr_BuildOwnershipElisionPlan(function, summary, &plan,
                                                   diagnostic)) {
        ZrParser_ExecIr_OwnershipElisionPlanFree(&plan);
        return ZR_FALSE;
    }
    result = ZrParser_ExecIr_ApplyOwnershipElisionPlan(function, &plan,
                                                       &changed, diagnostic);
    ZrParser_ExecIr_OwnershipElisionPlanFree(&plan);
    return result;
}
