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

TZrBool ZrParser_ExecIr_VerifyFunction(const SZrExecIrFunction *function,
                                       EZrExecIrVerifyLevel level,
                                       SZrExecIrDiagnostic *diagnostic) {
    return ZrCore_ExecIr_VerifyFunction(function, level, diagnostic);
}

TZrBool ZrParser_ExecIr_Verify(const SZrExecIrModule *module,
                               SZrExecIrDiagnostic *diagnostic) {
    return ZrCore_ExecIr_VerifyModule(module, diagnostic);
}
