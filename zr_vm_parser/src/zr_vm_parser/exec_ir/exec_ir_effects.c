#include "zr_vm_parser/exec_ir_builder.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void zr_parser_exec_ir_effect_diag(SZrExecIrDiagnostic *diagnostic,
                                           const SZrExecIrFunction *function,
                                           EZrExecutionDiagnosticCode code,
                                           TZrUInt32 instructionId,
                                           TZrUInt32 expected,
                                           TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->blockId = ZR_EXEC_IR_BLOCK_ID_ENTRY;
    diagnostic->instructionId = instructionId;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

static TZrUInt16 zr_parser_exec_ir_required_flags(
        const SZrExecIrOpcodeInfo *info) {
    TZrUInt16 flags = 0u;
    if (info == ZR_NULL) return flags;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE) != 0u)
        flags = (TZrUInt16)(flags | ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u)
        flags = (TZrUInt16)(flags | ZR_EXEC_IR_FLAG_MAY_THROW);
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC) != 0u)
        flags = (TZrUInt16)(flags | ZR_EXEC_IR_FLAG_MAY_GC);
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND) != 0u)
        flags = (TZrUInt16)(flags | ZR_EXEC_IR_FLAG_MAY_SUSPEND);
    return flags;
}

static TZrBool zr_parser_exec_ir_is_observable(
        const SZrExecIrOpcodeInfo *info) {
    TZrUInt16 required = zr_parser_exec_ir_required_flags(info);
    return (TZrBool)(info != ZR_NULL &&
                     (info->memoryWrites != 0u || required != 0u ||
                      (info->effects & ZR_EXEC_IR_EFFECT_DROP) != 0u));
}

TZrBool ZrParser_ExecIr_SynthesizeLinearEffects(
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrRange *memoryInRanges = ZR_NULL;
    SZrExecIrRange *memoryOutRanges = ZR_NULL;
    TZrUInt16 *requiredFlags = ZR_NULL;
    TZrExecIrEffectTokenId *effectIns = ZR_NULL;
    TZrExecIrEffectTokenId *effectOuts = ZR_NULL;
    TZrExecIrMemoryTokenId *tokens = ZR_NULL;
    TZrExecIrMemoryTokenId versions[ZR_EXEC_IR_MEMORY_CLASS_COUNT] = {0};
    TZrUInt32 tokenCount = 0u;
    TZrExecIrEffectTokenId effectToken = 1u;
    TZrUInt32 instructionIndex;
    TZrUInt32 region;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL) {
        zr_parser_exec_ir_effect_diag(diagnostic, ZR_NULL,
                                      ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 0u, 0u);
        return ZR_FALSE;
    }

    /* Phi-aware production owns every CFG with more than one block.  This
     * narrow producer is deliberately a no-op when another producer already
     * supplied any token or range, so it cannot overwrite established facts. */
    if (function->blockCount != 1u || function->instructionCount == 0u ||
        function->memoryTokenCount != 0u) {
        return ZR_TRUE;
    }
    if (function->blocks == ZR_NULL || function->instructions == ZR_NULL ||
        function->blocks[0].instructionRange.start != 0u ||
        function->blocks[0].instructionRange.count != function->instructionCount) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                      0u, function->instructionCount,
                                      function->blocks != ZR_NULL
                                          ? function->blocks[0].instructionRange.count
                                          : 0u);
        return ZR_FALSE;
    }
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        if (instruction->memoryIn.start != 0u || instruction->memoryIn.count != 0u ||
            instruction->memoryOut.start != 0u || instruction->memoryOut.count != 0u ||
            instruction->effectIn != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
            instruction->effectOut != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) {
            return ZR_TRUE;
        }
        if (ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode) == ZR_NULL) {
            zr_parser_exec_ir_effect_diag(diagnostic, function,
                                          ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
                                          instructionIndex + 1u, 0u,
                                          instruction->opcode);
            return ZR_FALSE;
        }
    }
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(
                (EZrExecIrOpcode)function->instructions[instructionIndex].opcode);
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            TZrUInt32 bit = (TZrUInt32)1u << region;
            if ((info->memoryReads & bit) != 0u) {
                if (tokenCount == UINT32_MAX) {
                    zr_parser_exec_ir_effect_diag(
                            diagnostic, function,
                            ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                            instructionIndex + 1u, UINT32_MAX, tokenCount);
                    return ZR_FALSE;
                }
                ++tokenCount;
            }
            if ((info->memoryWrites & bit) != 0u) {
                if (tokenCount == UINT32_MAX) {
                    zr_parser_exec_ir_effect_diag(
                            diagnostic, function,
                            ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                            instructionIndex + 1u, UINT32_MAX, tokenCount);
                    return ZR_FALSE;
                }
                ++tokenCount;
            }
        }
    }

#if SIZE_MAX <= UINT32_MAX
    if (function->instructionCount > SIZE_MAX / sizeof(*memoryInRanges) ||
        function->instructionCount > SIZE_MAX / sizeof(*memoryOutRanges) ||
        function->instructionCount > SIZE_MAX / sizeof(*requiredFlags)) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                      0u, UINT32_MAX, function->instructionCount);
        return ZR_FALSE;
    }
#endif
    memoryInRanges = (SZrExecIrRange *)calloc(function->instructionCount,
                                               sizeof(*memoryInRanges));
    memoryOutRanges = (SZrExecIrRange *)calloc(function->instructionCount,
                                                sizeof(*memoryOutRanges));
    requiredFlags = (TZrUInt16 *)calloc(function->instructionCount,
                                         sizeof(*requiredFlags));
    effectIns = (TZrExecIrEffectTokenId *)calloc(function->instructionCount,
                                                  sizeof(*effectIns));
    effectOuts = (TZrExecIrEffectTokenId *)calloc(function->instructionCount,
                                                   sizeof(*effectOuts));
    if (memoryInRanges == ZR_NULL || memoryOutRanges == ZR_NULL ||
        requiredFlags == ZR_NULL || effectIns == ZR_NULL ||
        effectOuts == ZR_NULL) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      0u, 0u, 0u);
        free(memoryInRanges);
        free(memoryOutRanges);
        free(requiredFlags);
        free(effectIns);
        free(effectOuts);
        return ZR_FALSE;
    }
#if SIZE_MAX <= UINT32_MAX
    if (tokenCount != 0u && tokenCount > SIZE_MAX / sizeof(*tokens)) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                      0u, UINT32_MAX, tokenCount);
        free(memoryInRanges);
        free(memoryOutRanges);
        free(requiredFlags);
        free(effectIns);
        free(effectOuts);
        return ZR_FALSE;
    }
#endif
    if (tokenCount != 0u) {
        tokens = (TZrExecIrMemoryTokenId *)malloc(
                (size_t)tokenCount * sizeof(*tokens));
        if (tokens == ZR_NULL) {
            zr_parser_exec_ir_effect_diag(diagnostic, function,
                                          ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                          0u, 0u, 0u);
            free(memoryInRanges);
            free(memoryOutRanges);
            free(requiredFlags);
            free(effectIns);
            free(effectOuts);
            return ZR_FALSE;
        }
    }

    tokenCount = 0u;
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(
                (EZrExecIrOpcode)instruction->opcode);
        TZrUInt32 inputCount = 0u;
        TZrUInt32 outputCount = 0u;
        TZrUInt32 inputStart = tokenCount;
        TZrUInt32 outputStart;

        requiredFlags[instructionIndex] = zr_parser_exec_ir_required_flags(info);
        if (zr_parser_exec_ir_is_observable(info)) {
            if (effectToken == UINT32_MAX) {
                zr_parser_exec_ir_effect_diag(
                        diagnostic, function,
                        ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                        instructionIndex + 1u, UINT32_MAX, effectToken);
                free(tokens);
                free(memoryInRanges);
                free(memoryOutRanges);
                free(requiredFlags);
                free(effectIns);
                free(effectOuts);
                return ZR_FALSE;
            }
            effectIns[instructionIndex] = effectToken;
            effectOuts[instructionIndex] = effectToken + 1u;
            effectToken += 1u;
        }
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            TZrUInt32 bit = (TZrUInt32)1u << region;
            if ((info->memoryReads & bit) != 0u) {
                if (versions[region] == 0u) versions[region] = 1u;
                tokens[tokenCount++] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                        (EZrExecIrMemoryClass)region, versions[region]);
                ++inputCount;
            }
        }
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            TZrUInt32 bit = (TZrUInt32)1u << region;
            if ((info->memoryWrites & bit) != 0u) {
                TZrUInt32 nextVersion = versions[region] == 0u
                    ? 1u : versions[region] + 1u;
                if (versions[region] == ZR_EXEC_IR_MEMORY_TOKEN_VERSION_MASK) {
                    zr_parser_exec_ir_effect_diag(
                            diagnostic, function,
                            ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                            instructionIndex + 1u,
                            ZR_EXEC_IR_MEMORY_TOKEN_VERSION_MASK,
                            versions[region]);
                    free(tokens);
                    free(memoryInRanges);
                    free(memoryOutRanges);
                    free(requiredFlags);
                    free(effectIns);
                    free(effectOuts);
                    return ZR_FALSE;
                }
                versions[region] = nextVersion;
                tokens[tokenCount++] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                        (EZrExecIrMemoryClass)region, versions[region]);
                ++outputCount;
            }
        }
        memoryInRanges[instructionIndex].start = inputStart;
        memoryInRanges[instructionIndex].count = inputCount;
        outputStart = inputStart + inputCount;
        memoryOutRanges[instructionIndex].start = outputStart;
        memoryOutRanges[instructionIndex].count = outputCount;
    }

    if (tokenCount != 0u &&
        !ZrCore_ExecIr_FunctionAppendMemoryTokens(function, tokens, tokenCount,
                                                  ZR_NULL)) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      0u, 0u, 0u);
        free(tokens);
        free(memoryInRanges);
        free(memoryOutRanges);
        free(requiredFlags);
        free(effectIns);
        free(effectOuts);
        return ZR_FALSE;
    }
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        function->instructions[instructionIndex].memoryIn =
                memoryInRanges[instructionIndex];
        function->instructions[instructionIndex].memoryOut =
                memoryOutRanges[instructionIndex];
        function->instructions[instructionIndex].effectIn =
                effectIns[instructionIndex];
        function->instructions[instructionIndex].effectOut =
                effectOuts[instructionIndex];
        function->instructions[instructionIndex].flags =
                (TZrUInt16)(function->instructions[instructionIndex].flags |
                            requiredFlags[instructionIndex]);
    }
    free(tokens);
    free(memoryInRanges);
    free(memoryOutRanges);
    free(requiredFlags);
    free(effectIns);
    free(effectOuts);
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_SynthesizeCfgEffects(
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrMemoryTokenId *entryMemory = ZR_NULL;
    TZrExecIrMemoryTokenId *terminalMemory = ZR_NULL;
    TZrExecIrEffectTokenId *entryEffects = ZR_NULL;
    TZrExecIrEffectTokenId *terminalEffects = ZR_NULL;
    TZrBool *memoryPhiNeeded = ZR_NULL;
    TZrBool *effectPhiNeeded = ZR_NULL;
    SZrExecIrRange *memoryIns = ZR_NULL;
    SZrExecIrRange *memoryOuts = ZR_NULL;
    TZrExecIrEffectTokenId *effectIns = ZR_NULL;
    TZrExecIrEffectTokenId *effectOuts = ZR_NULL;
    TZrUInt16 *requiredFlags = ZR_NULL;
    TZrExecIrMemoryTokenId *tokens = ZR_NULL;
    SZrExecIrPhiIncoming *phiIncomings = ZR_NULL;
    SZrExecIrRange appendedPhiRange;
    TZrUInt32 tokenCount = 0u;
    TZrUInt32 tokenCapacity = 0u;
    TZrUInt32 phiIncomingCount = 0u;
    TZrUInt32 phiCursor = 0u;
    TZrUInt32 oldPhiIncomingCount;
    size_t blockRegionCount = 0u;
    TZrUInt32 blockIndex;
    TZrUInt32 instructionIndex;
    TZrUInt32 nextInstruction = 0u;
    TZrExecIrBlockId loopHeader = ZR_EXEC_IR_BLOCK_ID_INVALID;
    TZrExecIrBlockId loopLatch = ZR_EXEC_IR_BLOCK_ID_INVALID;
    TZrUInt32 loopWrites = 0u;
    TZrBool loopHasEffect = ZR_FALSE;
    TZrUInt32 region;
    TZrBool result = ZR_TRUE;
    TZrUInt32 lastVersion[ZR_EXEC_IR_MEMORY_CLASS_COUNT] = {0};
    TZrExecIrEffectTokenId lastEffect = 1u;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL) {
        zr_parser_exec_ir_effect_diag(diagnostic, ZR_NULL,
                                      ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (function->blockCount <= 1u) {
        return ZrParser_ExecIr_SynthesizeLinearEffects(function, diagnostic);
    }
    if (function->memoryTokenCount != 0u) {
        return ZR_TRUE;
    }
    if (function->blockCount > function->blockCapacity ||
        function->instructionCount > function->instructionCapacity ||
        function->predecessorCount > function->predecessorCapacity ||
        function->blocks == ZR_NULL ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL)) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                      0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (function->instructionCount == 0u) return ZR_TRUE;

    /* Version one represents the untouched entry state on every path. */
    for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
        lastVersion[region] = 1u;
    }

    /* Existing token facts, including an explicitly authored CFG phi, belong
     * to the producer that authored them.  Do not partially overwrite them. */
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        if (block->id != blockIndex + 1u ||
            block->instructions.start != nextInstruction ||
            block->instructions.start > function->instructionCount ||
            block->instructions.count > function->instructionCount -
                                         block->instructions.start ||
            block->predecessors.start > function->predecessorCount ||
            block->predecessors.count > function->predecessorCount -
                                         block->predecessors.start) {
            zr_parser_exec_ir_effect_diag(diagnostic, function,
                                          ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                          block->id, 0u, 0u);
            result = ZR_FALSE;
            goto cleanup;
        }
        nextInstruction += block->instructions.count;
        if (block->effectPhiResult != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
            block->effectPhiIncomings.count != 0u) {
            goto cleanup;
        }
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            if (block->memoryPhiResults[region] !=
                        ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID ||
                block->memoryPhiIncomings[region].count != 0u) {
                goto cleanup;
            }
        }
        for (instructionIndex = block->predecessors.start;
             instructionIndex < block->predecessors.start +
                                   block->predecessors.count;
             ++instructionIndex) {
            TZrExecIrBlockId predecessor = function->predecessors[instructionIndex];
            if (predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
                predecessor > function->blockCount) {
                goto cleanup;
            }
            if (predecessor >= block->id) {
                if (loopHeader != ZR_EXEC_IR_BLOCK_ID_INVALID ||
                    block->id == ZR_EXEC_IR_BLOCK_ID_ENTRY ||
                    block->predecessors.count != 2u ||
                    predecessor != block->id + 1u) {
                    goto cleanup;
                }
                loopHeader = block->id;
                loopLatch = predecessor;
            }
        }
    }
    if (loopHeader != ZR_EXEC_IR_BLOCK_ID_INVALID) {
        const SZrExecIrBlock *latch = &function->blocks[loopLatch - 1u];
        if (latch->predecessors.count != 1u ||
            function->predecessors[latch->predecessors.start] != loopHeader) {
            goto cleanup;
        }
    }
    if (nextInstruction != function->instructionCount) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                      function->blockCount,
                                      function->instructionCount,
                                      nextInstruction);
        result = ZR_FALSE;
        goto cleanup;
    }
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        if (instruction->memoryIn.start != 0u || instruction->memoryIn.count != 0u ||
            instruction->memoryOut.start != 0u || instruction->memoryOut.count != 0u ||
            instruction->effectIn != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
            instruction->effectOut != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) {
            goto cleanup;
        }
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(
                (EZrExecIrOpcode)instruction->opcode);
        if (info == ZR_NULL) {
            zr_parser_exec_ir_effect_diag(diagnostic, function,
                                          ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
                                          instructionIndex + 1u, 0u,
                                          instruction->opcode);
            result = ZR_FALSE;
            goto cleanup;
        }
        if (loopHeader != ZR_EXEC_IR_BLOCK_ID_INVALID &&
            instructionIndex >= function->blocks[loopHeader - 1u].instructions.start &&
            instructionIndex < function->blocks[loopLatch - 1u].instructions.start +
                                       function->blocks[loopLatch - 1u].instructions.count) {
            loopWrites |= info->memoryWrites;
            if (zr_parser_exec_ir_is_observable(info)) loopHasEffect = ZR_TRUE;
        }
    }

    blockRegionCount = (size_t)function->blockCount *
                       ZR_EXEC_IR_MEMORY_CLASS_COUNT;
#if SIZE_MAX <= UINT32_MAX
    if (function->blockCount > SIZE_MAX / ZR_EXEC_IR_MEMORY_CLASS_COUNT ||
        blockRegionCount > SIZE_MAX / sizeof(*entryMemory) ||
        function->instructionCount > SIZE_MAX / sizeof(*memoryIns)) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                      0u, UINT32_MAX, function->blockCount);
        result = ZR_FALSE;
        goto cleanup;
    }
#endif
    entryMemory = (TZrExecIrMemoryTokenId *)calloc(blockRegionCount,
                                                   sizeof(*entryMemory));
    terminalMemory = (TZrExecIrMemoryTokenId *)calloc(blockRegionCount,
                                                       sizeof(*terminalMemory));
    entryEffects = (TZrExecIrEffectTokenId *)calloc(function->blockCount,
                                                     sizeof(*entryEffects));
    terminalEffects = (TZrExecIrEffectTokenId *)calloc(function->blockCount,
                                                        sizeof(*terminalEffects));
    memoryPhiNeeded = (TZrBool *)calloc(blockRegionCount,
                                        sizeof(*memoryPhiNeeded));
    effectPhiNeeded = (TZrBool *)calloc(function->blockCount,
                                         sizeof(*effectPhiNeeded));
    memoryIns = (SZrExecIrRange *)calloc(function->instructionCount,
                                          sizeof(*memoryIns));
    memoryOuts = (SZrExecIrRange *)calloc(function->instructionCount,
                                           sizeof(*memoryOuts));
    effectIns = (TZrExecIrEffectTokenId *)calloc(function->instructionCount,
                                                  sizeof(*effectIns));
    effectOuts = (TZrExecIrEffectTokenId *)calloc(function->instructionCount,
                                                   sizeof(*effectOuts));
    requiredFlags = (TZrUInt16 *)calloc(function->instructionCount,
                                         sizeof(*requiredFlags));
    if (entryMemory == ZR_NULL || terminalMemory == ZR_NULL ||
        entryEffects == ZR_NULL || terminalEffects == ZR_NULL ||
        memoryPhiNeeded == ZR_NULL || effectPhiNeeded == ZR_NULL ||
        memoryIns == ZR_NULL || memoryOuts == ZR_NULL ||
        effectIns == ZR_NULL || effectOuts == ZR_NULL ||
        requiredFlags == ZR_NULL) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      0u, 0u, 0u);
        result = ZR_FALSE;
        goto cleanup;
    }

    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(
                (EZrExecIrOpcode)function->instructions[instructionIndex].opcode);
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            TZrUInt32 bit = (TZrUInt32)1u << region;
            if ((info->memoryReads & bit) != 0u) {
                if (tokenCapacity == UINT32_MAX) {
                    zr_parser_exec_ir_effect_diag(
                            diagnostic, function,
                            ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                            instructionIndex + 1u, UINT32_MAX, tokenCapacity);
                    result = ZR_FALSE;
                    goto cleanup;
                }
                ++tokenCapacity;
            }
            if ((info->memoryWrites & bit) != 0u) {
                if (tokenCapacity == UINT32_MAX) {
                    zr_parser_exec_ir_effect_diag(
                            diagnostic, function,
                            ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                            instructionIndex + 1u, UINT32_MAX, tokenCapacity);
                    result = ZR_FALSE;
                    goto cleanup;
                }
                ++tokenCapacity;
            }
        }
    }
#if SIZE_MAX <= UINT32_MAX
    if (tokenCapacity != 0u && tokenCapacity > SIZE_MAX / sizeof(*tokens)) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                      0u, UINT32_MAX, tokenCapacity);
        result = ZR_FALSE;
        goto cleanup;
    }
#endif
    if (tokenCapacity != 0u) {
        tokens = (TZrExecIrMemoryTokenId *)malloc(
                (size_t)tokenCapacity * sizeof(*tokens));
        if (tokens == ZR_NULL) {
            zr_parser_exec_ir_effect_diag(diagnostic, function,
                                          ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                          0u, 0u, 0u);
            result = ZR_FALSE;
            goto cleanup;
        }
    }

    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrExecIrMemoryTokenId currentMemory[ZR_EXEC_IR_MEMORY_CLASS_COUNT] = {0};
        TZrExecIrEffectTokenId currentEffect = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
        TZrUInt32 predecessorIndex;

        if (block->predecessors.count != 0u) {
            TZrBool haveMemory[ZR_EXEC_IR_MEMORY_CLASS_COUNT] = {0};
            TZrBool haveEffect = ZR_FALSE;
            TZrExecIrMemoryTokenId incomingMemory[ZR_EXEC_IR_MEMORY_CLASS_COUNT] = {0};
            TZrExecIrEffectTokenId incomingEffect =
                    ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
            TZrUInt32 maximumVersion[ZR_EXEC_IR_MEMORY_CLASS_COUNT] = {0};
            TZrExecIrEffectTokenId maximumEffect =
                    ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;

            for (predecessorIndex = block->predecessors.start;
                 predecessorIndex < block->predecessors.start +
                                       block->predecessors.count;
                 ++predecessorIndex) {
                TZrExecIrBlockId predecessor = function->predecessors[predecessorIndex];
                TZrUInt32 predecessorBlock = predecessor - 1u;
                if (block->id == loopHeader && predecessor == loopLatch) continue;
                for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                    TZrExecIrMemoryTokenId token = terminalMemory[
                            (size_t)predecessorBlock * ZR_EXEC_IR_MEMORY_CLASS_COUNT +
                            region];
                    if (!haveMemory[region]) {
                        incomingMemory[region] = token;
                        haveMemory[region] = ZR_TRUE;
                    } else if (incomingMemory[region] != token) {
                        if (ZR_EXEC_IR_MEMORY_TOKEN_VERSION(token) >
                            maximumVersion[region]) {
                            maximumVersion[region] =
                                    ZR_EXEC_IR_MEMORY_TOKEN_VERSION(token);
                        }
                        if (ZR_EXEC_IR_MEMORY_TOKEN_VERSION(incomingMemory[region]) >
                            maximumVersion[region]) {
                            maximumVersion[region] =
                                    ZR_EXEC_IR_MEMORY_TOKEN_VERSION(incomingMemory[region]);
                        }
                        memoryPhiNeeded[(size_t)blockIndex *
                                        ZR_EXEC_IR_MEMORY_CLASS_COUNT + region] = ZR_TRUE;
                    }
                }
                if (!haveEffect) {
                    incomingEffect = terminalEffects[predecessorBlock];
                    haveEffect = ZR_TRUE;
                } else if (incomingEffect != terminalEffects[predecessorBlock]) {
                    if (terminalEffects[predecessorBlock] > maximumEffect)
                        maximumEffect = terminalEffects[predecessorBlock];
                    if (incomingEffect > maximumEffect)
                        maximumEffect = incomingEffect;
                    effectPhiNeeded[blockIndex] = ZR_TRUE;
                }
            }
            if (block->id == loopHeader) {
                for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                    if ((loopWrites & ((TZrUInt32)1u << region)) != 0u) {
                        TZrUInt32 forwardVersion = ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                                incomingMemory[region]);
                        if (forwardVersion > maximumVersion[region])
                            maximumVersion[region] = forwardVersion;
                        memoryPhiNeeded[(size_t)blockIndex *
                                        ZR_EXEC_IR_MEMORY_CLASS_COUNT + region] = ZR_TRUE;
                    }
                }
                if (loopHasEffect) {
                    if (incomingEffect > maximumEffect) maximumEffect = incomingEffect;
                    effectPhiNeeded[blockIndex] = ZR_TRUE;
                }
            }
            for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                if (memoryPhiNeeded[(size_t)blockIndex *
                                    ZR_EXEC_IR_MEMORY_CLASS_COUNT + region]) {
                    TZrUInt32 version = maximumVersion[region] + 1u;
                    if (version <= lastVersion[region])
                        version = lastVersion[region] + 1u;
                    if (version == 0u || version > ZR_EXEC_IR_MEMORY_TOKEN_VERSION_MASK) {
                        zr_parser_exec_ir_effect_diag(
                                diagnostic, function,
                                ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                block->id,
                                ZR_EXEC_IR_MEMORY_TOKEN_VERSION_MASK, version);
                        result = ZR_FALSE;
                        goto cleanup;
                    }
                    currentMemory[region] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                            (EZrExecIrMemoryClass)region, version);
                    entryMemory[(size_t)blockIndex *
                                ZR_EXEC_IR_MEMORY_CLASS_COUNT + region] =
                            currentMemory[region];
                    if (version > lastVersion[region]) lastVersion[region] = version;
                } else {
                    currentMemory[region] = incomingMemory[region];
                    entryMemory[(size_t)blockIndex *
                                ZR_EXEC_IR_MEMORY_CLASS_COUNT + region] =
                            currentMemory[region];
                }
            }
            if (effectPhiNeeded[blockIndex]) {
                if (maximumEffect < lastEffect) maximumEffect = lastEffect;
                if (maximumEffect == UINT32_MAX) {
                    zr_parser_exec_ir_effect_diag(
                            diagnostic, function,
                            ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                            block->id, UINT32_MAX, maximumEffect);
                    result = ZR_FALSE;
                    goto cleanup;
                }
                currentEffect = maximumEffect + 1u;
                lastEffect = currentEffect;
            } else {
                currentEffect = incomingEffect;
            }
            entryEffects[blockIndex] = currentEffect;
        }

        for (instructionIndex = block->instructions.start;
             instructionIndex < block->instructions.start +
                                   block->instructions.count;
             ++instructionIndex) {
            const SZrExecIrInstruction *instruction =
                    &function->instructions[instructionIndex];
            const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(
                    (EZrExecIrOpcode)instruction->opcode);
            TZrUInt32 inputCount = 0u;
            TZrUInt32 outputCount = 0u;
            TZrUInt32 inputStart = tokenCount;

            requiredFlags[instructionIndex] = zr_parser_exec_ir_required_flags(info);
            for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                TZrUInt32 bit = (TZrUInt32)1u << region;
                if ((info->memoryReads & bit) != 0u) {
                    if (currentMemory[region] ==
                            ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) {
                        currentMemory[region] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                                (EZrExecIrMemoryClass)region, 1u);
                    }
                    if (ZR_EXEC_IR_MEMORY_TOKEN_VERSION(currentMemory[region]) >
                            lastVersion[region]) {
                        lastVersion[region] = ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                                currentMemory[region]);
                    }
                    tokens[tokenCount] = currentMemory[region];
                    ++tokenCount;
                    ++inputCount;
                }
            }
            memoryIns[instructionIndex].start = inputStart;
            memoryIns[instructionIndex].count = inputCount;
            memoryOuts[instructionIndex].start = tokenCount;
            for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                TZrUInt32 bit = (TZrUInt32)1u << region;
                if ((info->memoryWrites & bit) != 0u) {
                    TZrUInt32 currentVersion =
                            ZR_EXEC_IR_MEMORY_TOKEN_VERSION(currentMemory[region]);
                    TZrUInt32 nextVersion = currentVersion + 1u;
                    if (nextVersion <= lastVersion[region])
                        nextVersion = lastVersion[region] + 1u;
                    if (currentVersion >= ZR_EXEC_IR_MEMORY_TOKEN_VERSION_MASK ||
                        nextVersion == 0u ||
                        nextVersion > ZR_EXEC_IR_MEMORY_TOKEN_VERSION_MASK) {
                        zr_parser_exec_ir_effect_diag(
                                diagnostic, function,
                                ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                instructionIndex + 1u,
                                ZR_EXEC_IR_MEMORY_TOKEN_VERSION_MASK,
                                nextVersion);
                        result = ZR_FALSE;
                        goto cleanup;
                    }
                    currentMemory[region] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                            (EZrExecIrMemoryClass)region, nextVersion);
                    lastVersion[region] = nextVersion;
                    tokens[tokenCount] = currentMemory[region];
                    ++tokenCount;
                    ++outputCount;
                }
            }
            memoryOuts[instructionIndex].count = outputCount;
            if (zr_parser_exec_ir_is_observable(info)) {
                TZrExecIrEffectTokenId input = currentEffect;
                TZrExecIrEffectTokenId outputBase;
                if (input == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) input = 1u;
                outputBase = input > lastEffect ? input : lastEffect;
                if (outputBase == UINT32_MAX) {
                    zr_parser_exec_ir_effect_diag(
                            diagnostic, function,
                            ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                            instructionIndex + 1u, UINT32_MAX, outputBase);
                    result = ZR_FALSE;
                    goto cleanup;
                }
                effectIns[instructionIndex] = input;
                effectOuts[instructionIndex] = outputBase + 1u;
                currentEffect = outputBase + 1u;
                lastEffect = currentEffect;
            }
        }
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            terminalMemory[(size_t)blockIndex * ZR_EXEC_IR_MEMORY_CLASS_COUNT +
                           region] = currentMemory[region];
        }
        terminalEffects[blockIndex] = currentEffect;
    }

    if (tokenCount != tokenCapacity) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                      0u, tokenCapacity, tokenCount);
        result = ZR_FALSE;
        goto cleanup;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 predecessorCount = block->predecessors.count;
        TZrUInt32 phiCount = effectPhiNeeded[blockIndex]
                ? predecessorCount : 0u;
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            if (memoryPhiNeeded[(size_t)blockIndex *
                                ZR_EXEC_IR_MEMORY_CLASS_COUNT + region]) {
                if (phiCount > UINT32_MAX - predecessorCount) {
                    zr_parser_exec_ir_effect_diag(
                            diagnostic, function,
                            ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                            block->id, UINT32_MAX, phiCount);
                    result = ZR_FALSE;
                    goto cleanup;
                }
                phiCount += predecessorCount;
            }
        }
        if (phiCount > UINT32_MAX - phiIncomingCount ||
            phiCount > UINT32_MAX - function->phiIncomingCount -
                               phiIncomingCount) {
            zr_parser_exec_ir_effect_diag(
                    diagnostic, function,
                    ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                    block->id, UINT32_MAX, phiCount);
            result = ZR_FALSE;
            goto cleanup;
        }
        phiIncomingCount += phiCount;
    }
    if (phiIncomingCount != 0u) {
#if SIZE_MAX <= UINT32_MAX
        if ((size_t)phiIncomingCount > SIZE_MAX / sizeof(*phiIncomings)) {
            zr_parser_exec_ir_effect_diag(
                    diagnostic, function,
                    ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                    0u, UINT32_MAX, phiIncomingCount);
            result = ZR_FALSE;
            goto cleanup;
        }
#endif
        phiIncomings = (SZrExecIrPhiIncoming *)malloc(
                (size_t)phiIncomingCount * sizeof(*phiIncomings));
        if (phiIncomings == ZR_NULL) {
            zr_parser_exec_ir_effect_diag(diagnostic, function,
                                          ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                          0u, 0u, 0u);
            result = ZR_FALSE;
            goto cleanup;
        }
        for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
            const SZrExecIrBlock *block = &function->blocks[blockIndex];
            TZrUInt32 i;
            if (effectPhiNeeded[blockIndex]) {
                for (i = 0u; i < block->predecessors.count; ++i) {
                    TZrExecIrBlockId predecessor =
                            function->predecessors[block->predecessors.start + i];
                    phiIncomings[phiCursor].predecessor = predecessor;
                    phiIncomings[phiCursor++].value = terminalEffects[predecessor - 1u];
                }
            }
            for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                if (!memoryPhiNeeded[(size_t)blockIndex *
                                     ZR_EXEC_IR_MEMORY_CLASS_COUNT + region]) continue;
                for (i = 0u; i < block->predecessors.count; ++i) {
                    TZrExecIrBlockId predecessor =
                            function->predecessors[block->predecessors.start + i];
                    phiIncomings[phiCursor].predecessor = predecessor;
                    phiIncomings[phiCursor++].value = terminalMemory[
                            (size_t)(predecessor - 1u) *
                            ZR_EXEC_IR_MEMORY_CLASS_COUNT + region];
                }
            }
        }
    }

    oldPhiIncomingCount = function->phiIncomingCount;
    appendedPhiRange.start = oldPhiIncomingCount;
    if (phiIncomingCount != 0u &&
        !ZrCore_ExecIr_FunctionAppendPhiIncoming(
                function, phiIncomings, phiIncomingCount, &appendedPhiRange)) {
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      0u, 0u, 0u);
        result = ZR_FALSE;
        goto cleanup;
    }
    if (tokenCount != 0u && !ZrCore_ExecIr_FunctionAppendMemoryTokens(
                function, tokens, tokenCount, ZR_NULL)) {
        function->phiIncomingCount = oldPhiIncomingCount;
        zr_parser_exec_ir_effect_diag(diagnostic, function,
                                      ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      0u, 0u, 0u);
        result = ZR_FALSE;
        goto cleanup;
    }
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        function->instructions[instructionIndex].memoryIn = memoryIns[instructionIndex];
        function->instructions[instructionIndex].memoryOut = memoryOuts[instructionIndex];
        function->instructions[instructionIndex].effectIn = effectIns[instructionIndex];
        function->instructions[instructionIndex].effectOut = effectOuts[instructionIndex];
        function->instructions[instructionIndex].flags =
                (TZrUInt16)(function->instructions[instructionIndex].flags |
                            requiredFlags[instructionIndex]);
    }
    phiCursor = appendedPhiRange.start;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        SZrExecIrBlock *block = &function->blocks[blockIndex];
        if (effectPhiNeeded[blockIndex]) {
            block->effectPhiResult = entryEffects[blockIndex];
            block->effectPhiIncomings.start = phiCursor;
            block->effectPhiIncomings.count = block->predecessors.count;
            phiCursor += block->predecessors.count;
        }
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            if (!memoryPhiNeeded[(size_t)blockIndex *
                                 ZR_EXEC_IR_MEMORY_CLASS_COUNT + region]) continue;
            block->memoryPhiResults[region] = entryMemory[
                    (size_t)blockIndex * ZR_EXEC_IR_MEMORY_CLASS_COUNT + region];
            block->memoryPhiIncomings[region].start = phiCursor;
            block->memoryPhiIncomings[region].count = block->predecessors.count;
            phiCursor += block->predecessors.count;
        }
    }

cleanup:
    free(entryMemory);
    free(terminalMemory);
    free(entryEffects);
    free(terminalEffects);
    free(memoryPhiNeeded);
    free(effectPhiNeeded);
    free(memoryIns);
    free(memoryOuts);
    free(effectIns);
    free(effectOuts);
    free(requiredFlags);
    free(phiIncomings);
    free(tokens);
    return result;
}

TZrBool ZrParser_ExecIr_VerifyFunction(const SZrExecIrFunction *function,
                                       EZrExecIrVerifyLevel level,
                                       SZrExecIrDiagnostic *diagnostic) {
    return ZrCore_ExecIr_VerifyFunction(function, level, diagnostic);
}

TZrBool ZrParser_ExecIr_Verify(const SZrExecIrModule *module,
                               SZrExecIrDiagnostic *diagnostic) {
    return ZrCore_ExecIr_VerifyModule(module, diagnostic);
}
