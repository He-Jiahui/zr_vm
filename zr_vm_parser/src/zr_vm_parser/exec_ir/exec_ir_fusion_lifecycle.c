#include "exec_ir_fusion_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static TZrBool zr_fusion_word_is_fused(
        const SZrExecBcFusionPlan *plan, TZrUInt32 pc) {
    TZrUInt32 opcode;
    if (plan == ZR_NULL || pc >= plan->instructionCount) {
        return ZR_FALSE;
    }
    opcode = plan->instructions[pc].operationCode;
    return (TZrBool)(opcode >= (TZrUInt32)ZR_EXEC_BC_FUSION_OPCODE_BASE &&
                     opcode < (TZrUInt32)ZR_EXEC_BC_FUSION_OPCODE_BASE +
                                      ZR_EXEC_BC_FUSION_PATTERN_COUNT);
}

static TZrUInt32 zr_fusion_expected_code_bytes(TZrUInt32 instructionCount) {
    return instructionCount > UINT32_MAX / sizeof(SZrInstruction)
               ? UINT32_MAX
               : instructionCount * (TZrUInt32)sizeof(SZrInstruction);
}

TZrBool ZrParser_ExecBcFusion_Validate(
        const SZrExecBcFusionPlan *plan,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 expectedDispatches = 0u;
    zr_fusion_diag_clear(diagnostic);
    if (!zr_fusion_plan_storage_is_valid(plan)) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           ZR_NULL, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (!plan->valid ||
        plan->invalidationReason != ZR_EXEC_BC_FUSION_INVALIDATION_NONE) {
        /* Invalidation deliberately leaves the original words available for
         * rollback, but that record is no longer publishable as a generated
         * plan.  Keep Validate aligned with the public lifecycle contract;
         * CheckValidity reports the richer invalidation reason separately. */
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           ZR_NULL, 0u, 0u, 1u,
                           (TZrUInt32)plan->invalidationReason);
        return ZR_FALSE;
    }
    if (plan->originalInstructionCount != 0u &&
        plan->originalInstructions == ZR_NULL) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           ZR_NULL, 0u, 0u, plan->originalInstructionCount, 0u);
        return ZR_FALSE;
    }
    if (plan->fusedCount != plan->sideEntryCount ||
        plan->instructionCount > plan->originalInstructionCount ||
        plan->fusedCount != plan->originalInstructionCount -
                            plan->instructionCount ||
        plan->estimatedCodeBytes !=
                zr_fusion_expected_code_bytes(plan->instructionCount) ||
        (plan->valid && plan->sourceMapCount !=
                                plan->originalInstructionCount)) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           ZR_NULL, 0u, 0u, plan->originalInstructionCount,
                           plan->instructionCount);
        return ZR_FALSE;
    }
    for (index = 0u; index < plan->instructionCount; ++index) {
        TZrUInt32 opcode = plan->instructions[index].operationCode;
        TZrUInt32 base = (TZrUInt32)ZR_EXEC_BC_FUSION_OPCODE_BASE;
        if (opcode >= base &&
            opcode < base + ZR_EXEC_BC_FUSION_PATTERN_COUNT &&
            (plan->instructions[index].operandExtra >= plan->sideEntryCount ||
             (TZrUInt32)plan->instructions[index].operand.operand2[0] !=
                    plan->instructions[index].operandExtra ||
             (TZrUInt32)plan->sideEntries[
                     plan->instructions[index].operandExtra].pattern !=
                    opcode - base)) {
            zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           ZR_NULL, 0u, 0u, plan->sideEntryCount,
                           plan->instructions[index].operandExtra);
            return ZR_FALSE;
        }
    }
    /* Every side record must have exactly one fixed-width dispatch word.  A
     * duplicate or orphaned record would make operandExtra alias a different
     * contract after a cache compaction, even when the individual records
     * themselves look well formed. */
    for (index = 0u; index < plan->sideEntryCount; ++index) {
        TZrUInt32 references = 0u;
        TZrUInt32 pc;
        for (pc = 0u; pc < plan->instructionCount; ++pc) {
            if (zr_fusion_word_is_fused(plan, pc) &&
                plan->instructions[pc].operandExtra == index) {
                ++references;
            }
        }
        if (references != 1u) {
            zr_fusion_diag_set(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                               ZR_NULL, 0u, 0u, 1u, references);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < plan->sideEntryCount; ++index) {
        const SZrExecBcFusionSideEntry *entry = &plan->sideEntries[index];
        const SZrExecBcFusionPatternInfo *info;
        const SZrExecIrOpcodeInfo *headInfo;
        const SZrExecIrOpcodeInfo *tailInfo;
        TZrUInt32 branchIndex;
        if ((TZrUInt32)entry->pattern >= ZR_EXEC_BC_FUSION_PATTERN_COUNT ||
            entry->headInstructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            entry->headInstructionId > plan->originalInstructionCount ||
            entry->tailInstructionId != entry->headInstructionId + 1u ||
            entry->tailInstructionId > plan->originalInstructionCount ||
            entry->headResumeId == 0u || entry->tailResumeId == 0u ||
            entry->operandCount > ZR_EXEC_BC_FUSION_MAX_OPERANDS ||
            entry->headOperandCount > ZR_EXEC_BC_FUSION_MAX_OPERANDS ||
            entry->tailOperandCount > ZR_EXEC_BC_FUSION_MAX_OPERANDS ||
            entry->headOperandCount + entry->tailOperandCount !=
                    entry->operandCount ||
            (entry->guardMask &
             ~(TZrUInt32)(ZR_EXEC_BC_FUSION_BOUNDARY_DEBUG |
                          ZR_EXEC_BC_FUSION_BOUNDARY_SAFEPOINT |
                          ZR_EXEC_BC_FUSION_BOUNDARY_EXCEPTION |
                          ZR_EXEC_BC_FUSION_BOUNDARY_REENTRANT)) != 0u) {
            zr_fusion_diag_set(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                               ZR_NULL, entry->headInstructionId, 0u,
                               plan->originalInstructionCount,
                               entry->tailInstructionId);
            return ZR_FALSE;
        }
        info = ZrParser_ExecBcFusion_PatternInfo(entry->pattern);
        if (info == ZR_NULL) {
            zr_fusion_diag_set(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                               ZR_NULL, entry->headInstructionId, 0u, 1u, 0u);
            return ZR_FALSE;
        }
        if (ZrCore_ExecIr_OpcodeInfo(entry->headOpcode) == ZR_NULL ||
            ZrCore_ExecIr_OpcodeInfo(entry->tailOpcode) == ZR_NULL ||
            entry->headOpcode != info->headOpcode ||
            (entry->tailOpcode != info->tailOpcode &&
             !((info->constraints &
                ZR_EXEC_BC_FUSION_CONSTRAINT_INDEX_STORE_VARIANT) != 0u &&
               entry->tailOpcode == ZR_EXEC_IR_OPCODE_STORE))) {
            zr_fusion_diag_set(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                               ZR_NULL, entry->headInstructionId, 0u,
                               (TZrUInt32)info->tailOpcode,
                               (TZrUInt32)entry->tailOpcode);
            return ZR_FALSE;
        }
        headInfo = ZrCore_ExecIr_OpcodeInfo(entry->headOpcode);
        tailInfo = ZrCore_ExecIr_OpcodeInfo(entry->tailOpcode);
        expectedDispatches =
                UINT32_MAX - expectedDispatches < info->dispatchBenefit
                    ? UINT32_MAX
                    : expectedDispatches + info->dispatchBenefit;
        if (entry->generation != plan->generation ||
            entry->signatureHash != plan->signatureHash ||
            entry->moduleHash != plan->moduleHash ||
            entry->layoutHash != plan->layoutHash ||
            (entry->guardMask != ZR_EXEC_BC_FUSION_BOUNDARY_NONE &&
             (info->constraints &
              ZR_EXEC_BC_FUSION_CONSTRAINT_PRESERVE_BOUNDARY) == 0u) ||
            (entry->guardMask & ~info->boundaryMask) != 0u) {
            zr_fusion_diag_set(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                               ZR_NULL, entry->headInstructionId, 0u, 0u, 0u);
            return ZR_FALSE;
        }
        if ((info->constraints & ZR_EXEC_BC_FUSION_CONSTRAINT_BRANCH_TARGET) !=
                    0u) {
            const TZrUInt32 expected =
                    info->tailOpcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH
                        ? 2u
                        : (info->tailOpcode == ZR_EXEC_IR_OPCODE_BRANCH ? 1u
                                                                        : 0u);
            if (expected == 0u || entry->branchTargetCount != expected ||
                entry->branchTargetCount > ZR_EXEC_BC_FUSION_MAX_BRANCH_TARGETS ||
                entry->branchTarget == ZR_EXEC_BC_FUSION_INVALID_INDEX ||
                entry->branchTargetPc == ZR_EXEC_BC_FUSION_INVALID_INDEX ||
                entry->branchTarget != entry->branchTargets[0] ||
                entry->branchTargetPc != entry->branchTargetPcs[0]) {
                zr_fusion_diag_set(diagnostic,
                                   ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                                   ZR_NULL, entry->headInstructionId, 0u,
                                   expected, entry->branchTargetCount);
                return ZR_FALSE;
            }
        } else if (entry->branchTargetCount != 0u ||
                   entry->branchTarget != ZR_EXEC_BC_FUSION_INVALID_INDEX ||
                   entry->branchTargetPc != ZR_EXEC_BC_FUSION_INVALID_INDEX) {
            zr_fusion_diag_set(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                               ZR_NULL, entry->headInstructionId, 0u, 0u,
                               entry->branchTargetCount);
            return ZR_FALSE;
        }
        if ((headInfo->resultArity != 0u &&
             headInfo->resultArity != ZR_EXEC_IR_VARIADIC &&
             (entry->headResult == 0u ||
              entry->headResult == ZR_EXEC_BC_FUSION_INVALID_INDEX)) ||
            ((headInfo->resultArity == 0u ||
              headInfo->resultArity == ZR_EXEC_IR_VARIADIC) &&
             entry->headResult != ZR_EXEC_BC_FUSION_INVALID_INDEX) ||
            (tailInfo->resultArity != 0u &&
             tailInfo->resultArity != ZR_EXEC_IR_VARIADIC &&
             (entry->tailResult == 0u ||
              entry->tailResult == ZR_EXEC_BC_FUSION_INVALID_INDEX)) ||
            ((tailInfo->resultArity == 0u ||
              tailInfo->resultArity == ZR_EXEC_IR_VARIADIC) &&
             entry->tailResult != ZR_EXEC_BC_FUSION_INVALID_INDEX) ||
            (info->resultForm == ZR_EXEC_BC_FUSION_RESULT_OF_HEAD &&
             entry->headResult == ZR_EXEC_BC_FUSION_INVALID_INDEX) ||
            (info->resultForm == ZR_EXEC_BC_FUSION_RESULT_OF_TAIL &&
             tailInfo->resultArity != 0u &&
             tailInfo->resultArity != ZR_EXEC_IR_VARIADIC &&
             entry->tailResult == ZR_EXEC_BC_FUSION_INVALID_INDEX)) {
            zr_fusion_diag_set(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                               ZR_NULL, entry->headInstructionId, 0u, 1u, 0u);
            return ZR_FALSE;
        }
        for (branchIndex = 0u;
             branchIndex < ZR_EXEC_BC_FUSION_MAX_BRANCH_TARGETS;
             ++branchIndex) {
            if (branchIndex < entry->branchTargetCount) {
                if (entry->branchTargets[branchIndex] ==
                            ZR_EXEC_BC_FUSION_INVALID_INDEX ||
                    entry->branchTargetPcs[branchIndex] ==
                            ZR_EXEC_BC_FUSION_INVALID_INDEX ||
                    entry->branchTargetPcs[branchIndex] >=
                            plan->instructionCount) {
                    zr_fusion_diag_set(
                            diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION, ZR_NULL,
                            entry->headInstructionId, 0u,
                            plan->instructionCount,
                            entry->branchTargetPcs[branchIndex]);
                    return ZR_FALSE;
                }
            } else if (entry->branchTargets[branchIndex] !=
                               ZR_EXEC_BC_FUSION_INVALID_INDEX ||
                       entry->branchTargetPcs[branchIndex] !=
                               ZR_EXEC_BC_FUSION_INVALID_INDEX) {
                zr_fusion_diag_set(
                        diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION, ZR_NULL,
                        entry->headInstructionId, 0u, 0u,
                        entry->branchTargetCount);
                return ZR_FALSE;
            }
        }
        for (branchIndex = entry->operandCount;
             branchIndex < ZR_EXEC_BC_FUSION_MAX_OPERANDS;
             ++branchIndex) {
            if (entry->operands[branchIndex] != 0u) {
                zr_fusion_diag_set(
                        diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION, ZR_NULL,
                        entry->headInstructionId, 0u, 0u,
                        entry->operands[branchIndex]);
                return ZR_FALSE;
            }
        }
        for (branchIndex = 0u; branchIndex < entry->operandCount;
             ++branchIndex) {
            if (entry->operands[branchIndex] == 0u ||
                entry->operands[branchIndex] ==
                        ZR_EXEC_BC_FUSION_INVALID_INDEX) {
                zr_fusion_diag_set(
                        diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION, ZR_NULL,
                        entry->headInstructionId, 0u, 1u,
                        entry->operands[branchIndex]);
                return ZR_FALSE;
            }
        }
    }
    /* Fallback rows are part of the deterministic artifact as well.  Keep
     * their IDs ordered and bounded so a capped/serialized table cannot point
     * at an unrelated operation or claim a successful match as a fallback. */
    {
        TZrUInt32 previousHead = 0u;
        for (index = 0u; index < plan->fallbackCount; ++index) {
            const SZrExecBcFusionFallback *fallback = &plan->fallbacks[index];
            const TZrBool validTail =
                    fallback->tailInstructionId ==
                            ZR_EXEC_BC_FUSION_INVALID_INDEX ||
                    (fallback->tailInstructionId ==
                             fallback->headInstructionId + 1u &&
                     fallback->tailInstructionId <=
                             plan->originalInstructionCount);
            if (fallback->headInstructionId ==
                        ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
                fallback->headInstructionId > plan->originalInstructionCount ||
                (index != 0u && fallback->headInstructionId <= previousHead) ||
                !validTail ||
                (TZrUInt32)fallback->pattern >
                        ZR_EXEC_BC_FUSION_PATTERN_COUNT ||
                fallback->reason == ZR_EXEC_BC_FUSION_FALLBACK_NONE ||
                (TZrUInt32)fallback->reason >=
                        ZR_EXEC_BC_FUSION_FALLBACK_COUNT) {
                zr_fusion_diag_set(
                        diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION, ZR_NULL,
                        fallback->headInstructionId, 0u, 1u,
                        (TZrUInt32)fallback->reason);
                return ZR_FALSE;
            }
            previousHead = fallback->headInstructionId;
        }
    }
    if (plan->dispatchesSaved != expectedDispatches) {
        zr_fusion_diag_set(diagnostic,
                           ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION, ZR_NULL,
                           0u, 0u, expectedDispatches,
                           plan->dispatchesSaved);
        return ZR_FALSE;
    }
    for (index = 0u; index < plan->sourceMapCount; ++index) {
        const SZrExecBcFusionSourceMap *map = &plan->sourceMaps[index];
        const SZrExecBcFusionSourceMap *previous =
                index == 0u ? ZR_NULL : &plan->sourceMaps[index - 1u];
        const SZrExecBcFusionSourceMap *next =
                index + 1u < plan->sourceMapCount
                    ? &plan->sourceMaps[index + 1u]
                    : ZR_NULL;
        if (map->fusedPc >= plan->instructionCount ||
            map->originalInstructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            map->originalInstructionId > plan->originalInstructionCount ||
            map->resumeId == 0u || map->ordinal > 1u ||
            (map->boundaryMask &
             ~(TZrUInt32)(ZR_EXEC_BC_FUSION_BOUNDARY_DEBUG |
                          ZR_EXEC_BC_FUSION_BOUNDARY_SAFEPOINT |
                          ZR_EXEC_BC_FUSION_BOUNDARY_EXCEPTION |
                          ZR_EXEC_BC_FUSION_BOUNDARY_REENTRANT)) != 0u) {
            zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                               ZR_NULL, map->originalInstructionId, 0u,
                               plan->instructionCount, map->fusedPc);
            return ZR_FALSE;
        }
        /* The builder emits source events in original instruction order.  A
         * fused word has exactly the ordinal-0/ordinal-1 pair; an unfused word
         * has only ordinal zero.  Enforcing that shape keeps resume lookup
         * deterministic even if a caller edits records and recomputes the
         * generated hash. */
        if (map->originalInstructionId != index + 1u ||
            (previous != ZR_NULL && map->fusedPc < previous->fusedPc) ||
            (map->ordinal == 1u &&
             (previous == ZR_NULL || previous->fusedPc != map->fusedPc ||
              previous->ordinal != 0u || !zr_fusion_word_is_fused(plan,
                                                                    map->fusedPc))) ||
            (map->ordinal == 0u && next != ZR_NULL &&
             next->fusedPc == map->fusedPc &&
             (next->ordinal != 1u || !zr_fusion_word_is_fused(plan,
                                                                map->fusedPc))) ||
            (map->ordinal == 0u &&
             (next == ZR_NULL || next->fusedPc != map->fusedPc) &&
             zr_fusion_word_is_fused(plan, map->fusedPc))) {
            zr_fusion_diag_set(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                               ZR_NULL, map->originalInstructionId, 0u,
                               index + 1u, map->fusedPc);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index + 1u < plan->sourceMapCount; ++index) {
        const SZrExecBcFusionSourceMap *head = &plan->sourceMaps[index];
        const SZrExecBcFusionSourceMap *tail = &plan->sourceMaps[index + 1u];
        if (head->ordinal == 0u && tail->ordinal == 1u &&
            head->fusedPc == tail->fusedPc) {
            TZrUInt32 sideIndex =
                    plan->instructions[head->fusedPc].operandExtra;
            const SZrExecBcFusionSideEntry *entry;
            if (sideIndex >= plan->sideEntryCount) {
                zr_fusion_diag_set(diagnostic,
                                   ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                                   ZR_NULL, head->originalInstructionId, 0u,
                                   plan->sideEntryCount, sideIndex);
                return ZR_FALSE;
            }
            entry = &plan->sideEntries[sideIndex];
            if (entry->headInstructionId != head->originalInstructionId ||
                entry->tailInstructionId != tail->originalInstructionId ||
                entry->headSourceId != head->sourceId ||
                entry->tailSourceId != tail->sourceId ||
                entry->headResumeId != head->resumeId ||
                entry->tailResumeId != tail->resumeId ||
                entry->guardMask != (head->boundaryMask | tail->boundaryMask)) {
                zr_fusion_diag_set(diagnostic,
                                   ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                                   ZR_NULL, head->originalInstructionId, 0u,
                                   entry->headInstructionId,
                                   head->originalInstructionId);
                return ZR_FALSE;
            }
        }
    }
    if (plan->generatedHash != ZrParser_ExecBcFusion_Hash(plan)) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           ZR_NULL, 0u, 0u, 0u, 0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = plan->generatedHash;
            diagnostic->actualHash = ZrParser_ExecBcFusion_Hash(plan);
        }
        return ZR_FALSE;
    }
    if (plan->inputHash == 0u || plan->patternSchemaHash == 0u ||
        plan->generatedHash == 0u) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           ZR_NULL, 0u, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (plan->patternSchemaHash != ZrParser_ExecBcFusion_PatternSchemaHash()) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           ZR_NULL, 0u, 0u, 0u, 0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = plan->patternSchemaHash;
            diagnostic->actualHash =
                    ZrParser_ExecBcFusion_PatternSchemaHash();
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

EZrExecBcFusionInvalidationReason ZrParser_ExecBcFusion_CheckValidity(
        const SZrExecBcFusionPlan *plan,
        const SZrExecIrFunction *function,
        TZrUInt64 activeGeneration,
        SZrExecBcFusionDiagnostic *diagnostic) {
    TZrUInt64 functionSignature;
    TZrUInt64 inputHash;
    SZrExecIrDiagnostic planDiagnostic;
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (!zr_fusion_plan_storage_is_valid(plan)) {
        zr_fusion_diag_set_rich(
                diagnostic, ZR_EXEC_BC_FUSION_INVALID_PLAN,
                ZR_EXEC_BC_FUSION_FALLBACK_INVALID_INPUT,
                ZR_EXEC_BC_FUSION_PATTERN_COUNT, 0u, 0u, 0u, 1u, 0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->invalidationReason =
                    ZR_EXEC_BC_FUSION_INVALIDATION_INVALID_PLAN;
        }
        return ZR_EXEC_BC_FUSION_INVALIDATION_INVALID_PLAN;
    }
    if (!plan->valid) {
        EZrExecBcFusionInvalidationReason reason = plan->invalidationReason;
        EZrExecBcFusionStatus status = ZR_EXEC_BC_FUSION_INVALID_PLAN;
        if (reason == ZR_EXEC_BC_FUSION_INVALIDATION_NONE) {
            reason = ZR_EXEC_BC_FUSION_INVALIDATION_INVALID_PLAN;
        }
        if (reason == ZR_EXEC_BC_FUSION_INVALIDATION_GENERATION) {
            status = ZR_EXEC_BC_FUSION_STALE_GENERATION;
        } else if (reason == ZR_EXEC_BC_FUSION_INVALIDATION_SIGNATURE ||
                   reason == ZR_EXEC_BC_FUSION_INVALIDATION_MODULE ||
                   reason == ZR_EXEC_BC_FUSION_INVALIDATION_LAYOUT) {
            status = ZR_EXEC_BC_FUSION_CONTRACT_MISMATCH;
        } else if (reason == ZR_EXEC_BC_FUSION_INVALIDATION_INPUT_CHANGED ||
                   reason == ZR_EXEC_BC_FUSION_INVALIDATION_GUARD) {
            status = ZR_EXEC_BC_FUSION_INVALID_INPUT;
        }
        zr_fusion_diag_set_rich(
                diagnostic, status,
                ZR_EXEC_BC_FUSION_FALLBACK_INVALID_INPUT,
                ZR_EXEC_BC_FUSION_PATTERN_COUNT, 0u, 0u, 0u, 1u, 0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->invalidationReason = reason;
        }
        return reason;
    }
    memset(&planDiagnostic, 0, sizeof(planDiagnostic));
    if (!ZrParser_ExecBcFusion_Validate(plan, &planDiagnostic)) {
        zr_fusion_diag_set_rich(
                diagnostic, ZR_EXEC_BC_FUSION_INVALID_PLAN,
                ZR_EXEC_BC_FUSION_FALLBACK_INVALID_INPUT,
                ZR_EXEC_BC_FUSION_PATTERN_COUNT,
                planDiagnostic.instructionId, 0u, planDiagnostic.sourceId,
                planDiagnostic.expectedVersion, planDiagnostic.actualVersion);
        if (diagnostic != ZR_NULL) {
            diagnostic->invalidationReason =
                    ZR_EXEC_BC_FUSION_INVALIDATION_INVALID_PLAN;
            diagnostic->expectedHash = plan->generatedHash;
            diagnostic->actualHash = 0u;
        }
        return ZR_EXEC_BC_FUSION_INVALIDATION_INVALID_PLAN;
    }
    if (function == ZR_NULL ||
        !zr_fusion_function_storage_is_valid(function)) {
        zr_fusion_diag_set_rich(
                diagnostic, ZR_EXEC_BC_FUSION_INVALID_INPUT,
                ZR_EXEC_BC_FUSION_FALLBACK_INVALID_INPUT,
                ZR_EXEC_BC_FUSION_PATTERN_COUNT, 0u, 0u, 0u, 1u, 0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->invalidationReason =
                    ZR_EXEC_BC_FUSION_INVALIDATION_INPUT_CHANGED;
        }
        return ZR_EXEC_BC_FUSION_INVALIDATION_INPUT_CHANGED;
    }
    if (activeGeneration != 0u && activeGeneration != plan->generation) {
        zr_fusion_diag_set_rich(
                diagnostic, ZR_EXEC_BC_FUSION_STALE_GENERATION,
                ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN,
                ZR_EXEC_BC_FUSION_PATTERN_COUNT, 0u, 0u, 0u,
                (TZrUInt32)plan->generation, (TZrUInt32)activeGeneration);
        if (diagnostic != ZR_NULL) {
            diagnostic->invalidationReason =
                    ZR_EXEC_BC_FUSION_INVALIDATION_GENERATION;
        }
        return ZR_EXEC_BC_FUSION_INVALIDATION_GENERATION;
    }
    if (function->contract.generation != plan->generation) {
        zr_fusion_diag_set_rich(
                diagnostic, ZR_EXEC_BC_FUSION_STALE_GENERATION,
                ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN,
                ZR_EXEC_BC_FUSION_PATTERN_COUNT, 0u, 0u, 0u,
                (TZrUInt32)plan->generation,
                (TZrUInt32)function->contract.generation);
        if (diagnostic != ZR_NULL) {
            diagnostic->invalidationReason =
                    ZR_EXEC_BC_FUSION_INVALIDATION_GENERATION;
            diagnostic->expectedHash = plan->generation;
            diagnostic->actualHash = function->contract.generation;
        }
        return ZR_EXEC_BC_FUSION_INVALIDATION_GENERATION;
    }
    functionSignature = function->contract.signatureHash != 0u
                            ? function->contract.signatureHash
                            : function->signatureHash;
    if (plan->signatureHash != functionSignature) {
        zr_fusion_diag_set_rich(
                diagnostic, ZR_EXEC_BC_FUSION_CONTRACT_MISMATCH,
                ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN,
                ZR_EXEC_BC_FUSION_PATTERN_COUNT, 0u, 0u, 0u,
                (TZrUInt32)plan->signatureHash,
                (TZrUInt32)functionSignature);
        if (diagnostic != ZR_NULL) {
            diagnostic->invalidationReason =
                    ZR_EXEC_BC_FUSION_INVALIDATION_SIGNATURE;
        }
        return ZR_EXEC_BC_FUSION_INVALIDATION_SIGNATURE;
    }
    if (plan->moduleHash != function->contract.moduleHash) {
        zr_fusion_diag_set_rich(
                diagnostic, ZR_EXEC_BC_FUSION_CONTRACT_MISMATCH,
                ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN,
                ZR_EXEC_BC_FUSION_PATTERN_COUNT, 0u, 0u, 0u,
                (TZrUInt32)plan->moduleHash,
                (TZrUInt32)function->contract.moduleHash);
        if (diagnostic != ZR_NULL) {
            diagnostic->invalidationReason =
                    ZR_EXEC_BC_FUSION_INVALIDATION_MODULE;
            diagnostic->expectedHash = plan->moduleHash;
            diagnostic->actualHash = function->contract.moduleHash;
        }
        return ZR_EXEC_BC_FUSION_INVALIDATION_MODULE;
    }
    if (plan->layoutHash != function->contract.layoutHash) {
        zr_fusion_diag_set_rich(
                diagnostic, ZR_EXEC_BC_FUSION_CONTRACT_MISMATCH,
                ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN,
                ZR_EXEC_BC_FUSION_PATTERN_COUNT, 0u, 0u, 0u,
                (TZrUInt32)plan->layoutHash,
                (TZrUInt32)function->contract.layoutHash);
        if (diagnostic != ZR_NULL) {
            diagnostic->invalidationReason =
                    ZR_EXEC_BC_FUSION_INVALIDATION_LAYOUT;
            diagnostic->expectedHash = plan->layoutHash;
            diagnostic->actualHash = function->contract.layoutHash;
        }
        return ZR_EXEC_BC_FUSION_INVALIDATION_LAYOUT;
    }
    inputHash = ZrParser_ExecBcFusion_InputHash(function);
    if (inputHash == 0u || inputHash != plan->inputHash) {
        zr_fusion_diag_set_rich(
                diagnostic, ZR_EXEC_BC_FUSION_INVALID_INPUT,
                ZR_EXEC_BC_FUSION_FALLBACK_INVALID_INPUT,
                ZR_EXEC_BC_FUSION_PATTERN_COUNT, 0u, 0u, 0u,
                (TZrUInt32)plan->inputHash, (TZrUInt32)inputHash);
        if (diagnostic != ZR_NULL) {
            diagnostic->invalidationReason =
                    ZR_EXEC_BC_FUSION_INVALIDATION_INPUT_CHANGED;
            diagnostic->expectedHash = plan->inputHash;
            diagnostic->actualHash = inputHash;
        }
        return ZR_EXEC_BC_FUSION_INVALIDATION_INPUT_CHANGED;
    }
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXEC_BC_FUSION_OK;
        diagnostic->invalidationReason = ZR_EXEC_BC_FUSION_INVALIDATION_NONE;
    }
    return ZR_EXEC_BC_FUSION_INVALIDATION_NONE;
}

void ZrParser_ExecBcFusion_Invalidate(
        SZrExecBcFusionPlan *plan,
        EZrExecBcFusionInvalidationReason reason) {
    if (!zr_fusion_plan_storage_is_valid(plan)) {
        return;
    }
    if ((TZrUInt32)reason >= ZR_EXEC_BC_FUSION_INVALIDATION_COUNT) {
        reason = ZR_EXEC_BC_FUSION_INVALIDATION_EXPLICIT;
    }
    if (plan->originalInstructionCount != 0u &&
        plan->originalInstructions != ZR_NULL) {
        if (plan->instructionCapacity < plan->originalInstructionCount ||
            plan->instructions == ZR_NULL) {
            SZrExecBcFusionInstruction *resized =
                    (SZrExecBcFusionInstruction *)realloc(
                            plan->instructions,
                            (size_t)plan->originalInstructionCount *
                                sizeof(*plan->instructions));
            if (resized == ZR_NULL) {
                return;
            }
            plan->instructions = resized;
            plan->instructionCapacity = plan->originalInstructionCount;
        }
        memcpy(plan->instructions, plan->originalInstructions,
               (size_t)plan->originalInstructionCount *
                   sizeof(*plan->instructions));
    }
    plan->instructionCount = plan->originalInstructionCount;
    plan->fusedCount = 0u;
    plan->dispatchesSaved = 0u;
    plan->estimatedCodeBytes =
            plan->instructionCount > UINT32_MAX / sizeof(SZrInstruction)
                ? UINT32_MAX
                : plan->instructionCount * (TZrUInt32)sizeof(SZrInstruction);
    plan->sideEntryCount = 0u;
    plan->sourceMapCount = 0u;
    plan->fallbackCount = 0u;
    plan->valid = ZR_FALSE;
    plan->invalidationReason = reason == ZR_EXEC_BC_FUSION_INVALIDATION_NONE
                                   ? ZR_EXEC_BC_FUSION_INVALIDATION_EXPLICIT
                                   : reason;
    plan->generatedHash = ZrParser_ExecBcFusion_Hash(plan);
}

const TZrChar *ZrParser_ExecBcFusion_PatternName(
        EZrExecBcFusionPattern pattern) {
    const SZrExecBcFusionPatternInfo *info =
            ZrParser_ExecBcFusion_PatternInfo(pattern);
    return info != ZR_NULL ? info->name : "unknown";
}

const TZrChar *ZrParser_ExecBcFusion_FallbackReasonName(
        EZrExecBcFusionFallbackReason reason) {
    switch (reason) {
        case ZR_EXEC_BC_FUSION_FALLBACK_NONE: return "none";
        case ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN: return "no-pattern";
        case ZR_EXEC_BC_FUSION_FALLBACK_TYPE_MISMATCH: return "type-mismatch";
        case ZR_EXEC_BC_FUSION_FALLBACK_RESULT_MISMATCH: return "result-mismatch";
        case ZR_EXEC_BC_FUSION_FALLBACK_EFFECT_MISMATCH: return "effect-mismatch";
        case ZR_EXEC_BC_FUSION_FALLBACK_MULTIPLE_USE: return "multiple-use";
        case ZR_EXEC_BC_FUSION_FALLBACK_LAYOUT_UNKNOWN: return "layout-unknown";
        case ZR_EXEC_BC_FUSION_FALLBACK_BINDING_MISSING: return "binding-missing";
        case ZR_EXEC_BC_FUSION_FALLBACK_BRANCH_TARGET: return "branch-target";
        case ZR_EXEC_BC_FUSION_FALLBACK_DEBUG_BOUNDARY: return "debug-boundary";
        case ZR_EXEC_BC_FUSION_FALLBACK_SAFEPOINT: return "safepoint";
        case ZR_EXEC_BC_FUSION_FALLBACK_EXCEPTION_BOUNDARY: return "exception-boundary";
        case ZR_EXEC_BC_FUSION_FALLBACK_REENTRANT_BOUNDARY: return "reentrant-boundary";
        case ZR_EXEC_BC_FUSION_FALLBACK_OPERAND_OVERFLOW: return "operand-overflow";
        case ZR_EXEC_BC_FUSION_FALLBACK_SIDE_TABLE: return "side-table";
        case ZR_EXEC_BC_FUSION_FALLBACK_BUDGET: return "budget";
        case ZR_EXEC_BC_FUSION_FALLBACK_DISABLED: return "disabled";
        case ZR_EXEC_BC_FUSION_FALLBACK_INVALID_INPUT: return "invalid-input";
        default: return "unknown";
    }
}

const TZrChar *ZrParser_ExecBcFusion_InvalidationReasonName(
        EZrExecBcFusionInvalidationReason reason) {
    switch (reason) {
        case ZR_EXEC_BC_FUSION_INVALIDATION_NONE: return "none";
        case ZR_EXEC_BC_FUSION_INVALIDATION_GENERATION: return "generation";
        case ZR_EXEC_BC_FUSION_INVALIDATION_SIGNATURE: return "signature";
        case ZR_EXEC_BC_FUSION_INVALIDATION_MODULE: return "module";
        case ZR_EXEC_BC_FUSION_INVALIDATION_LAYOUT: return "layout";
        case ZR_EXEC_BC_FUSION_INVALIDATION_INPUT_CHANGED: return "input-changed";
        case ZR_EXEC_BC_FUSION_INVALIDATION_GUARD: return "guard";
        case ZR_EXEC_BC_FUSION_INVALIDATION_EXPLICIT: return "explicit";
        case ZR_EXEC_BC_FUSION_INVALIDATION_INVALID_PLAN: return "invalid-plan";
        default: return "unknown";
    }
}

const TZrChar *ZrParser_ExecBcFusion_StatusName(
        EZrExecBcFusionStatus status) {
    switch (status) {
        case ZR_EXEC_BC_FUSION_OK: return "ok";
        case ZR_EXEC_BC_FUSION_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_EXEC_BC_FUSION_INVALID_INPUT: return "invalid-input";
        case ZR_EXEC_BC_FUSION_SEALED: return "sealed";
        case ZR_EXEC_BC_FUSION_CONTRACT_MISMATCH: return "contract-mismatch";
        case ZR_EXEC_BC_FUSION_STALE_GENERATION: return "stale-generation";
        case ZR_EXEC_BC_FUSION_OUT_OF_MEMORY: return "out-of-memory";
        case ZR_EXEC_BC_FUSION_CAPACITY_OVERFLOW: return "capacity-overflow";
        case ZR_EXEC_BC_FUSION_INVALID_PLAN: return "invalid-plan";
        default: return "unknown";
    }
}
