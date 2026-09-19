#include "exec_ir_pass_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static TZrBool zr_pass_size_mul_overflow(TZrUInt64 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     count > (TZrUInt64)(SIZE_MAX / elementSize));
}

static void zr_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 index;
    for (index = 0u; index < 4u; ++index) {
        *hash ^= (TZrUInt64)((value >> (index * 8u)) & 0xffu);
        *hash *= UINT64_C(1099511628211);
    }
}

static void zr_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    zr_hash_u32(hash, (TZrUInt32)value);
    zr_hash_u32(hash, (TZrUInt32)(value >> 32u));
}

static void zr_hash_range(TZrUInt64 *hash, SZrExecIrRange range) {
    zr_hash_u32(hash, range.start);
    zr_hash_u32(hash, range.count);
}

static TZrUInt64 zr_constants_hash(const SZrExecIrConstant *constants,
                                   TZrUInt32 count) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 index;
    zr_hash_u32(&hash, count);
    for (index = 0u; index < count; ++index) {
        zr_hash_u32(&hash, constants[index].typeToken);
        zr_hash_u32(&hash, constants[index].flags);
        zr_hash_u64(&hash, constants[index].bits);
    }
    return hash;
}

static TZrUInt64 zr_module_hash(const SZrExecIrModule *module) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 index;
    if (module == ZR_NULL) return 0u;
    if ((module->constantCount != 0u && module->constants == ZR_NULL) ||
        (module->layoutCount != 0u && module->layouts == ZR_NULL) ||
        (module->functionCount != 0u && module->functions == ZR_NULL)) return 0u;
    zr_hash_u32(&hash, module->id);
    zr_hash_u32(&hash, module->moduleToken);
    zr_hash_u64(&hash, module->moduleHash);
    zr_hash_u64(&hash, module->contract.generation);
    zr_hash_u64(&hash, zr_constants_hash(module->constants, module->constantCount));
    zr_hash_u32(&hash, module->constantCount);
    for (index = 0u; index < module->constantCount; ++index) {
        zr_hash_u32(&hash, module->constants[index].typeToken);
        zr_hash_u32(&hash, module->constants[index].flags);
        zr_hash_u64(&hash, module->constants[index].bits);
    }
    zr_hash_u32(&hash, module->layoutCount);
    for (index = 0u; index < module->layoutCount; ++index) {
        zr_hash_u32(&hash, module->layouts[index].id);
        zr_hash_u32(&hash, module->layouts[index].typeToken);
        zr_hash_u32(&hash, module->layouts[index].byteSize);
        zr_hash_u32(&hash, module->layouts[index].byteAlign);
        zr_hash_u64(&hash, module->layouts[index].layoutHash);
    }
    zr_hash_u32(&hash, module->functionCount);
    for (index = 0u; index < module->functionCount; ++index)
        zr_hash_u64(&hash, ZrParser_ExecIr_FunctionHash(&module->functions[index]));
    return hash;
}

TZrUInt64 ZrParser_ExecIr_FunctionHash(const SZrExecIrFunction *function) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 index;
    if (function == ZR_NULL) return 0u;
    if ((function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->operandCount != 0u && function->operands == ZR_NULL) ||
        (function->resultCount != 0u && function->results == ZR_NULL) ||
        (function->memoryTokenCount != 0u && function->memoryTokenPool == ZR_NULL) ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL) ||
        (function->phiIncomingCount != 0u && function->phiIncoming == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL) ||
        (function->gcRootCount != 0u && function->gcRoots == ZR_NULL) ||
        (function->deoptStateCount != 0u && function->deoptStates == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValues == ZR_NULL) ||
        (function->sourceMapCount != 0u && function->sourceMaps == ZR_NULL)) return 0u;

    zr_hash_u32(&hash, function->id);
    zr_hash_u32(&hash, function->functionToken);
    zr_hash_u64(&hash, function->signatureHash);
    zr_hash_u32(&hash, function->entryBlockId);
    zr_hash_u32(&hash, function->valueCount);
    zr_hash_u32(&hash, function->instructionCount);
    zr_hash_u32(&hash, function->blockCount);
    zr_hash_u32(&hash, function->operandCount);
    zr_hash_u32(&hash, function->resultCount);
    zr_hash_u32(&hash, function->memoryTokenCount);
    zr_hash_u32(&hash, function->phiCount);
    zr_hash_u32(&hash, function->phiIncomingCount);
    zr_hash_u32(&hash, function->predecessorCount);
    zr_hash_u32(&hash, function->successorCount);
    zr_hash_u32(&hash, function->gcRootCount);
    zr_hash_u32(&hash, function->deoptStateCount);
    zr_hash_u32(&hash, function->deoptValueCount);
    zr_hash_u32(&hash, function->sourceMapCount);

    for (index = 0u; index < function->valueCount; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        zr_hash_u32(&hash, value->id);
        zr_hash_u32(&hash, value->definition);
        zr_hash_u32(&hash, value->typeToken);
        zr_hash_u32(&hash, (TZrUInt32)value->ownership);
        zr_hash_u32(&hash, (TZrUInt32)value->nullability);
        zr_hash_u32(&hash, value->flags);
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        zr_hash_u32(&hash, instruction->opcode);
        zr_hash_u32(&hash, instruction->flags);
        zr_hash_range(&hash, instruction->results);
        zr_hash_range(&hash, instruction->operands);
        zr_hash_range(&hash, instruction->phiRange);
        zr_hash_range(&hash, instruction->successorRange);
        zr_hash_u32(&hash, instruction->typeToken);
        zr_hash_u32(&hash, instruction->matchTypeToken);
        zr_hash_u32(&hash, instruction->layoutId);
        zr_hash_range(&hash, instruction->memoryIn);
        zr_hash_range(&hash, instruction->memoryOut);
        zr_hash_u32(&hash, instruction->effectIn);
        zr_hash_u32(&hash, instruction->effectOut);
        zr_hash_u32(&hash, instruction->sourceId);
        zr_hash_u32(&hash, instruction->deoptId);
        zr_hash_u32(&hash, instruction->bindingRow);
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        zr_hash_u32(&hash, block->id);
        zr_hash_u32(&hash, block->flags);
        zr_hash_range(&hash, block->instructions);
        zr_hash_range(&hash, block->predecessors);
        zr_hash_range(&hash, block->successors);
        zr_hash_range(&hash, block->phis);
        /* immediateDominator is a derived analysis fact, not part of the IR
         * identity used for cache/revision checks. */
        zr_hash_u32(&hash, block->terminatorInstructionId);
    }
    for (index = 0u; index < function->operandCount; ++index)
        zr_hash_u32(&hash, function->operands[index]);
    for (index = 0u; index < function->resultCount; ++index)
        zr_hash_u32(&hash, function->results[index]);
    for (index = 0u; index < function->memoryTokenCount; ++index)
        zr_hash_u32(&hash, function->memoryTokenPool[index]);
    for (index = 0u; index < function->predecessorCount; ++index)
        zr_hash_u32(&hash, function->predecessors[index]);
    for (index = 0u; index < function->successorCount; ++index)
        zr_hash_u32(&hash, function->successors[index]);
    for (index = 0u; index < function->phiCount; ++index) {
        zr_hash_u32(&hash, function->phiPool[index].result);
        zr_hash_range(&hash, function->phiPool[index].incomings);
    }
    for (index = 0u; index < function->phiIncomingCount; ++index) {
        zr_hash_u32(&hash, function->phiIncoming[index].predecessor);
        zr_hash_u32(&hash, function->phiIncoming[index].value);
    }
    for (index = 0u; index < function->gcRootCount; ++index)
        zr_hash_u32(&hash, function->gcRoots[index]);
    for (index = 0u; index < function->deoptValueCount; ++index)
        zr_hash_u32(&hash, function->deoptValues[index]);
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        zr_hash_u32(&hash, state->id);
        zr_hash_u32(&hash, state->source);
        zr_hash_u32(&hash, state->resumeId);
        zr_hash_range(&hash, state->reconstruction);
        zr_hash_u32(&hash, state->cleanupState);
    }
    for (index = 0u; index < function->sourceMapCount; ++index) {
        const SZrExecIrSourceMap *map = &function->sourceMaps[index];
        zr_hash_u32(&hash, map->sourceId);
        zr_hash_u32(&hash, map->instructionId);
        zr_hash_u32(&hash, map->startOffset);
        zr_hash_u32(&hash, map->endOffset);
        zr_hash_u32(&hash, map->startLine);
        zr_hash_u32(&hash, map->startColumn);
        zr_hash_u32(&hash, map->endLine);
        zr_hash_u32(&hash, map->endColumn);
    }
    return hash;
}

void ZrParser_ExecIr_PassDiagnostic(SZrExecIrDiagnostic *diagnostic,
                                    EZrExecutionDiagnosticCode code,
                                    const SZrExecIrFunction *function,
                                    TZrExecIrBlockId blockId,
                                    TZrExecIrInstructionId instructionId,
                                    TZrUInt32 expected, TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->blockId = blockId;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = function != ZR_NULL && instructionId != 0u &&
                           instructionId <= function->instructionCount &&
                           function->instructions != ZR_NULL
                               ? function->instructions[instructionId - 1u].sourceId
                               : 0u;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

TZrBool ZrParser_ExecIr_PassRangeValid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

TZrBool ZrParser_ExecIr_PassConsumeBudget(SZrExecIrFunction *function,
                                          SZrExecIrPassContext *context,
                                          TZrUInt64 work) {
    if (context == ZR_NULL || context->budget == ZR_NULL) return ZR_TRUE;
    if ((context->budget->maxInstructions != 0u &&
         function->instructionCount > context->budget->maxInstructions) ||
        work > UINT64_MAX - context->workUsed ||
        (context->budget->maxWork != 0u &&
         context->workUsed + work > context->budget->maxWork)) {
        context->budgetExhausted = ZR_TRUE;
        context->lastReasonCode = ZR_EXEC_IR_PASS_REASON_BUDGET;
        return ZR_FALSE;
    }
    context->workUsed += work;
    return ZR_TRUE;
}

void ZrParser_ExecIr_AnalysisCacheInit(SZrExecIrAnalysisCache *cache) {
    if (cache != ZR_NULL) memset(cache, 0, sizeof(*cache));
}

void ZrParser_ExecIr_AnalysisCacheFree(SZrExecIrAnalysisCache *cache) {
    if (cache != ZR_NULL) {
        free(cache->sccpValues);
        memset(cache, 0, sizeof(*cache));
    }
}

void ZrParser_ExecIr_RemarkSinkInit(SZrExecIrRemarkSink *sink) {
    if (sink != ZR_NULL) memset(sink, 0, sizeof(*sink));
}

void ZrParser_ExecIr_RemarkSinkFree(SZrExecIrRemarkSink *sink) {
    if (sink != ZR_NULL) {
        free(sink->items);
        memset(sink, 0, sizeof(*sink));
    }
}

TZrBool ZrParser_ExecIr_EmitRemark(SZrExecIrRemarkSink *sink,
                                   const SZrExecIrOptimizationRemark *remark,
                                   SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOptimizationRemark *items;
    TZrUInt32 capacity;
    if (sink == ZR_NULL) return ZR_TRUE;
    if (remark == ZR_NULL || sink->count > sink->capacity) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       ZR_NULL, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (sink->count == sink->capacity) {
        capacity = sink->capacity == 0u ? 8u : sink->capacity * 2u;
        if (capacity < sink->capacity ||
            zr_pass_size_mul_overflow((TZrUInt64)capacity, sizeof(*items))) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                           ZR_NULL, 0u, 0u, UINT32_MAX, capacity);
            return ZR_FALSE;
        }
        items = (SZrExecIrOptimizationRemark *)realloc(
                sink->items, (size_t)capacity * sizeof(*items));
        if (items == ZR_NULL) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                           ZR_NULL, 0u, 0u, capacity, 0u);
            return ZR_FALSE;
        }
        sink->items = items;
        sink->capacity = capacity;
    }
    sink->items[sink->count++] = *remark;
    return ZR_TRUE;
}

static TZrBool zr_cache_clone(const SZrExecIrAnalysisCache *source,
                              SZrExecIrAnalysisCache *destination,
                              SZrExecIrDiagnostic *diagnostic) {
    ZrParser_ExecIr_AnalysisCacheInit(destination);
    if (source == ZR_NULL) return ZR_TRUE;
    destination->revision = source->revision;
    destination->irHash = source->irHash;
    destination->validMask = source->validMask;
    destination->sccpValueCount = source->sccpValueCount;
    if (source->sccpValueCount == 0u) return ZR_TRUE;
    if (source->sccpValues == ZR_NULL ||
        zr_pass_size_mul_overflow((TZrUInt64)source->sccpValueCount,
                                  sizeof(*source->sccpValues))) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       ZR_NULL, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    destination->sccpValues = (SZrExecIrSccpValue *)malloc(
            (size_t)source->sccpValueCount * sizeof(*destination->sccpValues));
    if (destination->sccpValues == ZR_NULL) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                       ZR_NULL, 0u, 0u, source->sccpValueCount, 0u);
        return ZR_FALSE;
    }
    memcpy(destination->sccpValues, source->sccpValues,
           (size_t)source->sccpValueCount * sizeof(*destination->sccpValues));
    return ZR_TRUE;
}

static void zr_cache_invalidate(SZrExecIrAnalysisCache *cache, TZrUInt32 mask) {
    if (cache == ZR_NULL) return;
    cache->validMask &= ~mask;
    if ((mask & ZR_EXEC_IR_ANALYSIS_SCCP) != 0u) {
        free(cache->sccpValues);
        cache->sccpValues = ZR_NULL;
        cache->sccpValueCount = 0u;
    }
}

static TZrBool zr_ensure_analyses(SZrExecIrFunction *function, TZrUInt32 required,
                                  SZrExecIrPassContext *context,
                                  SZrExecIrDiagnostic *diagnostic) {
    if ((required & ~ZR_EXEC_IR_ANALYSIS_ALL) != 0u ||
        (required & (ZR_EXEC_IR_ANALYSIS_LOOPS | ZR_EXEC_IR_ANALYSIS_LIVENESS)) != 0u) {
        context->lastReasonCode = ZR_EXEC_IR_PASS_REASON_REQUIREMENT;
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                       function, 0u, 0u, ZR_EXEC_IR_ANALYSIS_ALL, required);
        return ZR_FALSE;
    }
    if (required == 0u) return ZR_TRUE;
    if (context->constantCount != 0u && context->constants == ZR_NULL) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic,
                                       ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       function, 0u, 0u, context->constantCount, 0u);
        return ZR_FALSE;
    }
    if (context->cache == ZR_NULL) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    {
        TZrUInt64 currentHash = ZrParser_ExecIr_FunctionHash(function);
        if (context->constants != ZR_NULL || context->constantCount != 0u)
            currentHash ^= zr_constants_hash(context->constants,
                                              context->constantCount);
        if (context->cache->irHash != 0u && context->cache->irHash != currentHash) {
            zr_cache_invalidate(context->cache, ZR_EXEC_IR_ANALYSIS_ALL);
            ++context->cache->revision;
        }
        context->cache->irHash = currentHash;
    }
    if ((required & ZR_EXEC_IR_ANALYSIS_DOMINATORS) != 0u &&
        (context->cache->validMask & ZR_EXEC_IR_ANALYSIS_DOMINATORS) == 0u) {
        if (function->blockCount != 0u &&
            !ZrParser_ExecIr_ComputeDominators(function, diagnostic)) return ZR_FALSE;
        context->cache->validMask |= ZR_EXEC_IR_ANALYSIS_DOMINATORS;
    }
    if ((required & ZR_EXEC_IR_ANALYSIS_SCCP) != 0u &&
        ((context->cache->validMask & ZR_EXEC_IR_ANALYSIS_SCCP) == 0u ||
         context->cache->sccpValueCount != function->valueCount)) {
        zr_cache_invalidate(context->cache, ZR_EXEC_IR_ANALYSIS_SCCP);
        if (!ZrParser_ExecIr_ComputeSccp(function, context, ZR_FALSE, ZR_NULL, diagnostic))
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrExecIrSourceId zr_changed_source(const SZrExecIrFunction *before,
                                           const SZrExecIrFunction *after) {
    TZrUInt32 index;
    TZrUInt32 count = before->instructionCount < after->instructionCount
                          ? before->instructionCount : after->instructionCount;
    for (index = 0u; index < count; ++index) {
        if (memcmp(&before->instructions[index], &after->instructions[index],
                   sizeof(before->instructions[index])) != 0)
            return after->instructions[index].sourceId;
    }
    return 0u;
}

static const SZrExecIrPassInfo zr_scalar_passes[] = {
    {.name = "sccp",
     .requiresAnalysis = ZR_EXEC_IR_ANALYSIS_DOMINATORS,
     /* Rewriting constants and aliases changes the lattice's input.  It is
      * therefore deliberately not preserved; a later pass must recompute it. */
     .preservesAnalysis = 0u,
     .invalidatesAnalysis = ZR_EXEC_IR_ANALYSIS_DOMINATORS |
                            ZR_EXEC_IR_ANALYSIS_LOOPS |
                            ZR_EXEC_IR_ANALYSIS_LIVENESS,
     .run = ZrParser_ExecIr_RunSccpPass},
    {.name = "dce",
     .requiresAnalysis = 0u,
     .preservesAnalysis = 0u,
     .invalidatesAnalysis = ZR_EXEC_IR_ANALYSIS_ALL,
     .run = ZrParser_ExecIr_RunDcePass}
};

const SZrExecIrPassInfo *ZrParser_ExecIr_GetScalarPasses(TZrUInt32 *count) {
    if (count != ZR_NULL)
        *count = (TZrUInt32)(sizeof(zr_scalar_passes) / sizeof(zr_scalar_passes[0]));
    return zr_scalar_passes;
}

TZrBool ZrParser_ExecIr_RunPassPipeline(SZrExecIrFunction *function,
                                        const SZrExecIrPassInfo *passes,
                                        TZrUInt32 passCount,
                                        SZrExecIrPassContext *context,
                                        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrFunction snapshot;
    SZrExecIrAnalysisCache cacheSnapshot;
    TZrUInt32 index;
    TZrUInt32 remarkCount = 0u;
    TZrUInt64 workUsed;
    TZrBool savedBudget;
    TZrUInt32 savedReason;
    TZrExecIrSourceId savedSource;
    TZrUInt32 savedPasses;
    SZrExecIrFunction passSnapshot;
    TZrBool passSnapshotReady = ZR_FALSE;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL || context == ZR_NULL ||
        (passCount != 0u && passes == ZR_NULL)) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (function->sealed) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_SEALED,
                                       function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrParser_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, diagnostic))
        return ZR_FALSE;

    ZrCore_ExecIr_FunctionInit(&snapshot);
    ZrCore_ExecIr_FunctionInit(&passSnapshot);
    ZrParser_ExecIr_AnalysisCacheInit(&cacheSnapshot);
    if (!ZrCore_ExecIr_CloneFunction(function, &snapshot, diagnostic) ||
        !zr_cache_clone(context->cache, &cacheSnapshot, diagnostic)) {
        ZrCore_ExecIr_FreeFunction(&snapshot);
        ZrParser_ExecIr_AnalysisCacheFree(&cacheSnapshot);
        return ZR_FALSE;
    }
    workUsed = context->workUsed;
    savedBudget = context->budgetExhausted;
    savedReason = context->lastReasonCode;
    savedSource = context->lastSourceId;
    savedPasses = context->passesRun;
    if (context->remarks != ZR_NULL) remarkCount = context->remarks->count;

    for (index = 0u; index < passCount; ++index) {
        TZrBool changed = ZR_FALSE;
        TZrUInt64 before;
        TZrUInt64 after;
        clock_t started;
        clock_t ended;
        SZrExecIrOptimizationRemark remark;

        if (!ZrCore_ExecIr_CloneFunction(function, &passSnapshot, diagnostic))
            goto rollback;
        passSnapshotReady = ZR_TRUE;

        if (passes[index].run == ZR_NULL || passes[index].name == ZR_NULL ||
            (passes[index].requiresAnalysis & ~ZR_EXEC_IR_ANALYSIS_ALL) != 0u ||
            (passes[index].preservesAnalysis & ~ZR_EXEC_IR_ANALYSIS_ALL) != 0u ||
            (passes[index].invalidatesAnalysis & ~ZR_EXEC_IR_ANALYSIS_ALL) != 0u) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                           function, 0u, 0u, index, 0u);
            goto rollback;
        }
        if (!ZrParser_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, diagnostic))
            goto rollback;
        before = ZrParser_ExecIr_FunctionHash(function);
        context->budgetExhausted = ZR_FALSE;
        context->lastReasonCode = ZR_EXEC_IR_PASS_REASON_NONE;
        context->lastSourceId = 0u;
        if (!zr_ensure_analyses(function, passes[index].requiresAnalysis,
                                context, diagnostic))
            goto rollback;
        started = clock();
        ++context->passesRun;
        if (!context->budgetExhausted &&
            !passes[index].run(function, context, &changed, diagnostic))
            goto rollback;
        ended = clock();
        if (!ZrParser_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, diagnostic)) {
            context->lastReasonCode = ZR_EXEC_IR_PASS_REASON_VERIFIER;
            goto rollback;
        }
        after = ZrParser_ExecIr_FunctionHash(function);
        if (after != before) changed = ZR_TRUE;
        if (context->cache != ZR_NULL) {
            TZrUInt32 invalidated = passes[index].invalidatesAnalysis;
            if (changed) invalidated |=
                    (ZR_EXEC_IR_ANALYSIS_ALL & ~passes[index].preservesAnalysis);
            if (invalidated != 0u) {
                /* Clear every invalidated payload, not just its bit. */
                zr_cache_invalidate(context->cache, invalidated);
                ++context->cache->revision;
            }
        }
        after = ZrParser_ExecIr_FunctionHash(function);
        memset(&remark, 0, sizeof(remark));
        remark.pass = passes[index].name;
        remark.sourceId = context->lastSourceId != 0u ? context->lastSourceId :
                          (changed ? zr_changed_source(&passSnapshot, function) : 0u);
        remark.outcome = context->budgetExhausted ? ZR_EXEC_IR_REMARK_BLOCKED :
                         (changed ? ZR_EXEC_IR_REMARK_SUCCESS : ZR_EXEC_IR_REMARK_MISSED);
        remark.reasonCode = context->lastReasonCode != ZR_EXEC_IR_PASS_REASON_NONE
                                ? context->lastReasonCode
                                : (changed ? ZR_EXEC_IR_PASS_REASON_NONE
                                           : ZR_EXEC_IR_PASS_REASON_NO_CHANGE);
        remark.beforeHash = before;
        remark.afterHash = after;
        remark.elapsedTicks =
                started == (clock_t)-1 || ended == (clock_t)-1 || ended < started
                    ? 0u : (TZrUInt64)(ended - started);
        if (!ZrParser_ExecIr_EmitRemark(context->remarks, &remark, diagnostic))
            goto rollback;
        ZrCore_ExecIr_FreeFunction(&passSnapshot);
        ZrCore_ExecIr_FunctionInit(&passSnapshot);
        passSnapshotReady = ZR_FALSE;
        if (context->budgetExhausted) break;
    }
    ZrCore_ExecIr_FreeFunction(&snapshot);
    ZrCore_ExecIr_FreeFunction(&passSnapshot);
    ZrParser_ExecIr_AnalysisCacheFree(&cacheSnapshot);
    return ZR_TRUE;

rollback:
    if (passSnapshotReady) {
        ZrCore_ExecIr_FreeFunction(&passSnapshot);
        passSnapshotReady = ZR_FALSE;
    }
    ZrCore_ExecIr_FreeFunction(function);
    *function = snapshot;
    ZrCore_ExecIr_FunctionInit(&snapshot);
    if (context->cache != ZR_NULL) {
        ZrParser_ExecIr_AnalysisCacheFree(context->cache);
        *context->cache = cacheSnapshot;
        ZrParser_ExecIr_AnalysisCacheInit(&cacheSnapshot);
    }
    if (context->remarks != ZR_NULL) context->remarks->count = remarkCount;
    context->workUsed = workUsed;
    context->budgetExhausted = savedBudget;
    context->lastReasonCode = savedReason;
    context->lastSourceId = savedSource;
    context->passesRun = savedPasses;
    ZrParser_ExecIr_AnalysisCacheFree(&cacheSnapshot);
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_OptimizeScalar(SZrExecIrFunction *function,
                                       const SZrExecIrPassBudget *budget,
                                       SZrExecIrRemarkSink *remarks,
                                       SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context;
    TZrUInt32 count;
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    memset(&context, 0, sizeof(context));
    context.cache = &cache;
    context.budget = budget;
    context.remarks = remarks;
    ZrParser_ExecIr_GetScalarPasses(&count);
    if (!ZrParser_ExecIr_RunPassPipeline(function, zr_scalar_passes, count,
                                         &context, diagnostic)) {
        ZrParser_ExecIr_AnalysisCacheFree(&cache);
        return ZR_FALSE;
    }
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_Optimize(SZrExecIrModule *module,
                                 const SZrExecIrOptimizeOptions *options,
                                 SZrExecIrOptimizationResult *result,
                                 SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrModule snapshot;
    SZrExecIrPassInfo selected[2];
    TZrUInt32 selectedCount = 0u;
    TZrUInt32 index;
    TZrUInt32 remarkCount = 0u;
    TZrUInt64 workUsed = 0u;
    TZrBool budgetExhausted = ZR_FALSE;
    TZrUInt64 beforeModuleHash;

    if (result != ZR_NULL) memset(result, 0, sizeof(*result));
    if (module == ZR_NULL ||
        (options != ZR_NULL &&
         (options->disabledPassMask & ~ZR_EXEC_IR_OPTIMIZE_DISABLE_KNOWN_MASK) != 0u) ||
        !ZrParser_ExecIr_Verify(module, diagnostic))
        return ZR_FALSE;
    if (options == ZR_NULL ||
        (options->disabledPassMask & ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP) == 0u)
        selected[selectedCount++] = zr_scalar_passes[0];
    if (options == ZR_NULL ||
        (options->disabledPassMask & ZR_EXEC_IR_OPTIMIZE_DISABLE_DCE) == 0u)
        selected[selectedCount++] = zr_scalar_passes[1];

    ZrCore_ExecIr_ModuleInit(&snapshot);
    if (!ZrCore_ExecIr_CloneModule(module, &snapshot, diagnostic)) return ZR_FALSE;
    beforeModuleHash = zr_module_hash(module);
    if (options != ZR_NULL && options->remarks != ZR_NULL)
        remarkCount = options->remarks->count;
    for (index = 0u; index < module->functionCount; ++index) {
        SZrExecIrAnalysisCache cache;
        SZrExecIrPassContext context;
        TZrUInt64 before = ZrParser_ExecIr_FunctionHash(&module->functions[index]);
        ZrParser_ExecIr_AnalysisCacheInit(&cache);
        memset(&context, 0, sizeof(context));
        context.cache = &cache;
        context.workUsed = workUsed;
        context.budgetExhausted = budgetExhausted;
        context.constants = module->constants;
        context.constantCount = module->constantCount;
        if (options != ZR_NULL) {
            context.budget = options->budget;
            context.remarks = options->remarks;
        }
        if (!ZrParser_ExecIr_RunPassPipeline(&module->functions[index], selected,
                                             selectedCount, &context, diagnostic)) {
            ZrParser_ExecIr_AnalysisCacheFree(&cache);
            goto module_rollback;
        }
        workUsed = context.workUsed;
        budgetExhausted = (TZrBool)(budgetExhausted || context.budgetExhausted);
        if (result != ZR_NULL) {
            TZrUInt64 after = ZrParser_ExecIr_FunctionHash(&module->functions[index]);
            ++result->functionsVisited;
            result->passesRun += context.passesRun;
            result->budgetExhausted = budgetExhausted;
            if (before != after) ++result->functionsChanged;
        }
        ZrParser_ExecIr_AnalysisCacheFree(&cache);
        if (budgetExhausted) break;
    }
    if (result != ZR_NULL) {
        result->beforeHash = beforeModuleHash;
        result->afterHash = zr_module_hash(module);
        result->budgetExhausted = budgetExhausted;
    }
    ZrCore_ExecIr_FreeModule(&snapshot);
    return ZR_TRUE;

module_rollback:
    ZrCore_ExecIr_FreeModule(module);
    *module = snapshot;
    ZrCore_ExecIr_ModuleInit(&snapshot);
    if (options != ZR_NULL && options->remarks != ZR_NULL)
        options->remarks->count = remarkCount;
    if (result != ZR_NULL) memset(result, 0, sizeof(*result));
    return ZR_FALSE;
}
