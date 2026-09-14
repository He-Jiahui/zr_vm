#include "exec_ir_fusion_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_parser/exec_ir_binding_facts.h"

static TZrUInt32 zr_fusion_code_bytes_for_count(TZrUInt32 instructionCount) {
    return instructionCount > UINT32_MAX / sizeof(SZrInstruction)
               ? UINT32_MAX
               : instructionCount * (TZrUInt32)sizeof(SZrInstruction);
}

static TZrBool zr_fusion_options_check(
        const SZrExecIrFunction *function,
        const SZrExecBcPatternOptions *options,
        SZrExecIrDiagnostic *diagnostic) {
    if (!zr_fusion_options_valid(options)) {
        zr_fusion_diag_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                           function, 0u, 0u,
                           ZR_EXEC_BC_FUSION_SCHEMA_VERSION,
                           options != ZR_NULL ? options->schemaVersion : 0u);
        return ZR_FALSE;
    }
    if (options == ZR_NULL) {
        return ZR_TRUE;
    }
    if (options->bindingFacts != ZR_NULL) {
        SZrExecIrDiagnostic factsDiagnostic;
        if (ZrParser_ExecIr_BindingFacts_ValidateEx(
                    options->bindingFacts, function, &factsDiagnostic) !=
            ZR_EXEC_IR_BINDING_FACTS_OK) {
            if (diagnostic != ZR_NULL) {
                *diagnostic = factsDiagnostic;
            }
            return ZR_FALSE;
        }
    }
    if (options->requireSealed && !function->sealed) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_SEALED,
                           function, 0u, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (options->activeGeneration != 0u &&
        options->activeGeneration != function->contract.generation) {
        zr_fusion_diag_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,
                           function, 0u, 0u,
                           (TZrUInt32)options->activeGeneration,
                           (TZrUInt32)function->contract.generation);
        return ZR_FALSE;
    }
    if (options->expectedSignatureHash != 0u &&
        options->expectedSignatureHash !=
            (function->contract.signatureHash != 0u
                 ? function->contract.signatureHash
                 : function->signatureHash)) {
        zr_fusion_set_contract_diagnostic(
                diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                function, options->expectedSignatureHash,
                function->contract.signatureHash != 0u
                    ? function->contract.signatureHash
                    : function->signatureHash);
        return ZR_FALSE;
    }
    if (options->expectedModuleHash != 0u &&
        options->expectedModuleHash != function->contract.moduleHash) {
        zr_fusion_set_contract_diagnostic(
                diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, function,
                options->expectedModuleHash, function->contract.moduleHash);
        return ZR_FALSE;
    }
    if (options->expectedLayoutHash != 0u &&
        options->expectedLayoutHash != function->contract.layoutHash) {
        zr_fusion_set_contract_diagnostic(
                diagnostic, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, function,
                options->expectedLayoutHash, function->contract.layoutHash);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_fusion_build_internal(
        const SZrExecIrFunction *function,
        const SZrExecBcPatternOptions *options,
        SZrExecBcFusionPlan *candidate,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 *instructionToPc = ZR_NULL;
    TZrUInt32 index;
    TZrUInt64 inputHash;
    TZrUInt32 maxFallback;
    if (!zr_fusion_function_storage_is_valid(function)) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    /* The projected map uses instructionCount + 1 entries.  Reject the
     * representational limit before the addition/allocation can wrap; this
     * is a capacity error, not an ordinary no-match fallback. */
    if (function->instructionCount == UINT32_MAX
#if SIZE_MAX < UINT32_MAX
        || function->instructionCount >
                   (TZrUInt32)(SIZE_MAX / sizeof(SZrExecBcFusionInstruction))
        || function->instructionCount + 1u >
                   (TZrUInt32)(SIZE_MAX / sizeof(TZrUInt32))
#endif
    ) {
        zr_fusion_diag_set(diagnostic,
                           ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, function,
                           0u, 0u, SIZE_MAX > UINT32_MAX ? UINT32_MAX : 0u,
                           function->instructionCount);
        return ZR_FALSE;
    }
    if (!zr_fusion_options_check(function, options, diagnostic)) {
        return ZR_FALSE;
    }
    inputHash = ZrParser_ExecBcFusion_InputHash(function);
    if (inputHash == 0u) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    ZrParser_ExecBcFusionPlan_Init(candidate);
    candidate->functionToken = function->functionToken;
    candidate->generation = function->contract.generation;
    candidate->signatureHash = function->contract.signatureHash != 0u
                                   ? function->contract.signatureHash
                                   : function->signatureHash;
    candidate->moduleHash = function->contract.moduleHash;
    candidate->layoutHash = function->contract.layoutHash;
    candidate->inputHash = inputHash;
    candidate->patternSchemaHash =
            ZrParser_ExecBcFusion_PatternSchemaHash();
    candidate->originalInstructionCount = function->instructionCount;
    candidate->codeBudgetBytes = options != ZR_NULL
                                     ? options->maxCodeBytes
                                     : 0u;

    if (function->instructionCount != 0u) {
        candidate->originalInstructions = (SZrExecBcFusionInstruction *)calloc(
                function->instructionCount,
                sizeof(*candidate->originalInstructions));
        if (candidate->originalInstructions == ZR_NULL) {
            zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                               function, 0u, 0u, function->instructionCount, 0u);
            return ZR_FALSE;
        }
        candidate->instructions = (SZrExecBcFusionInstruction *)calloc(
                function->instructionCount,
                sizeof(*candidate->instructions));
        if (candidate->instructions == ZR_NULL) {
            zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                               function, 0u, 0u, function->instructionCount, 0u);
            return ZR_FALSE;
        }
        candidate->instructionCapacity = function->instructionCount;
        for (index = 0u; index < function->instructionCount; ++index) {
            candidate->originalInstructions[index] =
                    zr_fusion_encode_original(function,
                                              &function->instructions[index]);
        }
    }
    instructionToPc = (TZrUInt32 *)calloc(
            (size_t)function->instructionCount + 1u,
            sizeof(*instructionToPc));
    /* The sentinel slot is initialized even for an empty function.  Treat a
     * NULL result as allocation failure unconditionally; otherwise the
     * initialization loop would dereference NULL when a zero-sized fixture
     * happens to be denied an allocation by the host allocator. */
    if (instructionToPc == ZR_NULL) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                           function, 0u, 0u, function->instructionCount, 0u);
        goto fail;
    }
    for (index = 0u; index <= function->instructionCount; ++index) {
        instructionToPc[index] = ZR_EXEC_BC_FUSION_INVALID_INDEX;
    }
    maxFallback = options != ZR_NULL
                      ? zr_fusion_option_or_default(options->maxFallbackEntries,
                                                    UINT32_MAX)
                      : UINT32_MAX;
    index = 0u;
    while (index < function->instructionCount) {
        TZrUInt32 headId = index + 1u;
        TZrUInt32 tailId = headId + 1u;
        EZrExecBcFusionPattern pattern = ZR_EXEC_BC_FUSION_PATTERN_COUNT;
        EZrExecBcFusionFallbackReason reason = ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN;
        const SZrExecBcFusionPatternInfo *info = ZR_NULL;
        TZrBool accepted = ZR_FALSE;

        if (tailId <= function->instructionCount) {
            reason = zr_fusion_reason_for_pair(
                    function, headId, tailId, options, &pattern);
            if (reason == ZR_EXEC_BC_FUSION_FALLBACK_NONE &&
                (TZrUInt32)pattern < ZR_EXEC_BC_FUSION_PATTERN_COUNT) {
                info = ZrParser_ExecBcFusion_PatternInfo(pattern);
                if (!zr_fusion_budget_allows(candidate, options, info)) {
                    const TZrBool codeBudgetExceeded =
                            (TZrBool)(options != ZR_NULL &&
                                      options->maxCodeBytes != 0u &&
                                      (candidate->estimatedCodeBytes >
                                               options->maxCodeBytes ||
                                       info->codeCostBytes >
                                               options->maxCodeBytes -
                                                       (candidate->estimatedCodeBytes <=
                                                                options->maxCodeBytes
                                                            ? candidate->estimatedCodeBytes
                                                            : options->maxCodeBytes)));
                    reason = (candidate->fusedCount >=
                                      zr_fusion_option_or_default(
                                              options != ZR_NULL
                                                  ? options->maxFusedCount
                                                  : 0u,
                                              UINT32_MAX) || codeBudgetExceeded ||
                              info->dispatchBenefit <
                                  zr_fusion_option_or_default(
                                      options != ZR_NULL
                                          ? options->minDispatchBenefit
                                          : 0u,
                                      1u))
                                  ? ZR_EXEC_BC_FUSION_FALLBACK_BUDGET
                                  : ZR_EXEC_BC_FUSION_FALLBACK_SIDE_TABLE;
                } else {
                    SZrExecBcFusionSideEntry side;
                    TZrUInt32 sideIndex = candidate->sideEntryCount;
                    if (!zr_fusion_fill_side_entry(function, options, pattern,
                                                   headId, tailId, &side)) {
                        reason = ZR_EXEC_BC_FUSION_FALLBACK_OPERAND_OVERFLOW;
                    } else if (!zr_fusion_append_side(candidate, &side)) {
                        zr_fusion_diag_set(
                                diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                function, headId, 0u, 0u, 0u);
                        goto fail;
                    } else {
                        TZrUInt32 fusedPc = candidate->instructionCount;
                        if (!zr_fusion_append_instruction(
                                    candidate,
                                    zr_fusion_encode_fused(pattern, sideIndex)) ||
                            !zr_fusion_append_source_event(
                                    candidate, fusedPc, headId,
                                    zr_fusion_instruction(function, headId), 0u,
                                    function) ||
                            !zr_fusion_append_source_event(
                                    candidate, fusedPc, tailId,
                                    zr_fusion_instruction(function, tailId), 1u,
                                    function)) {
                            zr_fusion_diag_set(
                                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                    function, headId, 0u, 0u, 0u);
                            goto fail;
                        }
                        instructionToPc[headId] = fusedPc;
                        instructionToPc[tailId] = fusedPc;
                        if (candidate->fusedCount != UINT32_MAX) {
                            ++candidate->fusedCount;
                        }
                        candidate->dispatchesSaved =
                                UINT32_MAX - candidate->dispatchesSaved <
                                                info->dispatchBenefit
                                    ? UINT32_MAX
                                    : candidate->dispatchesSaved +
                                              info->dispatchBenefit;
                        accepted = ZR_TRUE;
                        candidate->estimatedCodeBytes =
                                zr_fusion_code_bytes_for_count(
                                        candidate->instructionCount);
                        index += 2u;
                    }
                }
            }
        }
        if (accepted) {
            continue;
        }
        {
            const SZrExecIrInstruction *instruction =
                    &function->instructions[index];
            TZrUInt32 fusedPc = candidate->instructionCount;
            SZrExecBcFusionFallback fallback;
            if (!zr_fusion_append_instruction(
                        candidate, zr_fusion_encode_original(function, instruction)) ||
                !zr_fusion_append_source_event(candidate, fusedPc, headId,
                                               instruction, 0u, function)) {
                zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                   function, headId, 0u, 0u, 0u);
                goto fail;
            }
            instructionToPc[headId] = fusedPc;
            memset(&fallback, 0, sizeof(fallback));
            fallback.headInstructionId = headId;
            fallback.tailInstructionId = tailId <= function->instructionCount
                                             ? tailId
                                             : ZR_EXEC_BC_FUSION_INVALID_INDEX;
            fallback.pattern = (TZrUInt32)pattern < ZR_EXEC_BC_FUSION_PATTERN_COUNT
                                   ? pattern
                                   : ZR_EXEC_BC_FUSION_PATTERN_COUNT;
            fallback.reason = reason;
            fallback.sourceId = instruction->sourceId;
            if (!zr_fusion_append_fallback(candidate, &fallback, maxFallback)) {
                zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                   function, headId, 0u, 0u, 0u);
                goto fail;
            }
            index += 1u;
            candidate->estimatedCodeBytes = zr_fusion_code_bytes_for_count(
                    candidate->instructionCount);
        }
    }

    /* Resolve every branch edge to a projected PC after all windows are
     * known.  Conditional branches have two explicit successors; mapping only
     * element zero would lose the fall-through edge. */
    for (index = 0u; index < candidate->sideEntryCount; ++index) {
        SZrExecBcFusionSideEntry *entry = &candidate->sideEntries[index];
        TZrUInt32 targetIndex;
        for (targetIndex = 0u;
             targetIndex < entry->branchTargetCount &&
             targetIndex < ZR_EXEC_BC_FUSION_MAX_BRANCH_TARGETS;
             ++targetIndex) {
            TZrUInt32 targetInstruction = zr_fusion_block_target_instruction(
                    function, entry->branchTargets[targetIndex]);
            entry->branchTargetPcs[targetIndex] = zr_fusion_find_output_pc(
                    instructionToPc, function->instructionCount,
                    targetInstruction);
        }
        if (entry->branchTargetCount != 0u) {
            entry->branchTarget = entry->branchTargets[0];
            entry->branchTargetPc = entry->branchTargetPcs[0];
        }
    }
    candidate->valid = ZR_TRUE;
    candidate->invalidationReason = ZR_EXEC_BC_FUSION_INVALIDATION_NONE;
    candidate->generatedHash = ZrParser_ExecBcFusion_Hash(candidate);
    if (!ZrParser_ExecBcFusion_Validate(candidate, diagnostic)) {
        goto fail;
    }
    free(instructionToPc);
    return ZR_TRUE;

fail:
    free(instructionToPc);
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_BuildExecBcFusion(
        const SZrExecIrFunction *function,
        const SZrExecBcPatternOptions *options,
        SZrExecBcFusionPlan *output,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecBcFusionPlan candidate;
    TZrBool built;
    zr_fusion_diag_clear(diagnostic);
    if (output == ZR_NULL) {
        zr_fusion_diag_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (output->storageTag == ZR_EXEC_BC_FUSION_STORAGE_TAG &&
        !zr_fusion_plan_storage_is_valid(output)) {
        zr_fusion_diag_set(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    memset(&candidate, 0, sizeof(candidate));
    built = zr_fusion_build_internal(function, options, &candidate, diagnostic);
    if (!built) {
        ZrParser_ExecBcFusionPlan_Free(&candidate);
        return ZR_FALSE;
    }
    ZrParser_ExecBcFusionPlan_Free(output);
    *output = candidate;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_SelectExecBcPatterns(
        SZrExecIrFunction *function,
        const SZrExecBcPatternOptions *options,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecBcFusionPlan temporary;
    SZrExecBcFusionPlan *destination;
    TZrBool result;
    if (function == ZR_NULL) {
        zr_fusion_diag_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           ZR_NULL, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (options != ZR_NULL && options->output != ZR_NULL) {
        return ZrParser_ExecIr_BuildExecBcFusion(
                function, options, options->output, diagnostic);
    }
    ZrParser_ExecBcFusionPlan_Init(&temporary);
    destination = &temporary;
    result = ZrParser_ExecIr_BuildExecBcFusion(function, options, destination,
                                               diagnostic);
    ZrParser_ExecBcFusionPlan_Free(destination);
    return result;
}
