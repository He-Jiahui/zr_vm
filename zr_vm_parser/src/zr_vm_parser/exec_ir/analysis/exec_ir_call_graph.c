#include "zr_vm_parser/exec_ir_interprocedural.h"
#include "zr_vm_core/exec_ir_state_map.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* FNV-1a is used here only as a deterministic cache key.  It is not a
 * cryptographic identity and must never be used as an authorization token. */
#define ZR_EXEC_IR_HASH_OFFSET UINT64_C(1469598103934665603)
#define ZR_EXEC_IR_HASH_PRIME UINT64_C(1099511628211)

static void zr_hash_byte(TZrUInt64 *hash, TZrUInt8 value) {
    *hash ^= (TZrUInt64)value;
    *hash *= ZR_EXEC_IR_HASH_PRIME;
}

static void zr_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 i;
    for (i = 0u; i < 4u; ++i) {
        zr_hash_byte(hash, (TZrUInt8)((value >> (i * 8u)) & 0xffu));
    }
}

static void zr_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    zr_hash_u32(hash, (TZrUInt32)value);
    zr_hash_u32(hash, (TZrUInt32)(value >> 32u));
}

static void zr_hash_range(TZrUInt64 *hash, SZrExecIrRange range) {
    zr_hash_u32(hash, range.offset);
    zr_hash_u32(hash, range.count);
}

static TZrExecIrBlockId block_for_instruction(
        const SZrExecIrFunction *function, TZrExecIrInstructionId instructionId);

static void zr_set_diagnostic(SZrExecIrDiagnostic *diagnostic,
                              EZrExecutionDiagnosticCode code,
                              const SZrExecIrFunction *function,
                              TZrUInt32 instructionId,
                              TZrUInt32 expected,
                              TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->blockId = block_for_instruction(function, instructionId);
    if (function != ZR_NULL && instructionId != 0u &&
        instructionId <= function->instructionCount &&
        instructionId <= function->instructionCapacity &&
        function->instructions != ZR_NULL) {
        diagnostic->sourceId = function->instructions[instructionId - 1u].sourceId;
    }
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

static void zr_set_hash_diagnostic(SZrExecIrDiagnostic *diagnostic,
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
    diagnostic->blockId = block_for_instruction(function, instructionId);
    if (function != ZR_NULL && instructionId != 0u &&
        instructionId <= function->instructionCount &&
        instructionId <= function->instructionCapacity &&
        function->instructions != ZR_NULL) {
        diagnostic->sourceId = function->instructions[instructionId - 1u].sourceId;
    }
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
}

static TZrMetadataToken summary_target_token(
        const SZrExecIrFunctionSummary *summary) {
    if (summary == ZR_NULL) return 0u;
    return summary->targetToken != 0u ? summary->targetToken : summary->functionToken;
}

static TZrBool count_valid(TZrUInt32 count, TZrUInt32 capacity,
                           const void *storage) {
    return (TZrBool)(count <= capacity && (count == 0u || storage != ZR_NULL));
}

static TZrExecIrBlockId block_for_instruction(
        const SZrExecIrFunction *function, TZrExecIrInstructionId instructionId) {
    TZrUInt32 i;
    if (function == ZR_NULL || instructionId == 0u ||
        function->blocks == ZR_NULL || function->blockCount > function->blockCapacity ||
        instructionId > function->instructionCount) {
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
    for (i = 0u; i < function->blockCount; ++i) {
        const SZrExecIrBlock *block = &function->blocks[i];
        if (block->instructionRange.start > function->instructionCount ||
            block->instructionRange.count > function->instructionCount -
                block->instructionRange.start) {
            continue;
        }
        if (instructionId - 1u >= block->instructionRange.start &&
            instructionId - 1u < block->instructionRange.start +
                                  block->instructionRange.count) {
            return block->id;
        }
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

/* Keep allocation guards portable to 32-bit hosts.  Comparing a uint32
 * directly with SIZE_MAX makes GCC diagnose a type-limits warning on the
 * usual 64-bit build (every uint32 is representable), while multiplying
 * first can wrap on a 32-bit build.  The division form is valid on both. */
static TZrBool allocation_count_fits(TZrUInt32 count, size_t elementSize) {
    if (elementSize == 0u) return ZR_FALSE;
    if (count != 0u && elementSize > SIZE_MAX / (size_t)count) return ZR_FALSE;
    return ZR_TRUE;
}

static TZrBool call_graph_storage_valid(const SZrExecIrCallGraph *graph) {
    return (TZrBool)(graph != ZR_NULL &&
                     graph->schemaVersion == ZR_EXEC_IR_CALL_GRAPH_VERSION &&
                     count_valid(graph->summaryCount, graph->summaryCapacity,
                                 graph->summaries) &&
                     count_valid(graph->edgeCount, graph->edgeCapacity,
                                 graph->edges));
}

static TZrUInt64 zr_function_body_hash(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZR_EXEC_IR_HASH_OFFSET;
    TZrUInt64 aggregateHash = ZrCore_ExecIr_DeoptAggregateHash(function);
    TZrUInt32 i;

    if (aggregateHash == 0u ||
        !count_valid(function->valueCount, function->valueCapacity,
                     function->values) ||
        !count_valid(function->instructionCount, function->instructionCapacity,
                     function->instructions) ||
        !count_valid(function->blockCount, function->blockCapacity,
                     function->blocks) ||
        !count_valid(function->operandCount, function->operandCapacity,
                     function->operands) ||
        !count_valid(function->resultCount, function->resultCapacity,
                     function->results) ||
        !count_valid(function->memoryTokenCount, function->memoryTokenCapacity,
                     function->memoryTokenPool) ||
        !count_valid(function->phiCount, function->phiCapacity,
                     function->phiPool) ||
        !count_valid(function->phiIncomingCount, function->phiIncomingCapacity,
                     function->phiIncoming) ||
        !count_valid(function->predecessorCount, function->predecessorCapacity,
                     function->predecessors) ||
        !count_valid(function->successorCount, function->successorCapacity,
                     function->successors) ||
        !count_valid(function->gcMapCount, function->gcMapCapacity,
                     function->gcMap) ||
        !count_valid(function->gcRootCount, function->gcRootCapacity,
                     function->gcRoots) ||
        !count_valid(function->deoptStateCount, function->deoptStateCapacity,
                     function->deoptStates) ||
        !count_valid(function->deoptValueCount, function->deoptValueCapacity,
                     function->deoptValues) ||
        !count_valid(function->sourceMapCount, function->sourceMapCapacity,
                     function->sourceMaps)) {
        return 0u;
    }

    zr_hash_u32(&hash, function->id);
    zr_hash_u32(&hash, function->functionToken);
    zr_hash_u64(&hash, function->signatureHash);
    zr_hash_u32(&hash, function->entryBlockId);
    zr_hash_u32(&hash, function->contract.schemaVersion);
    zr_hash_u32(&hash, function->contract.abiVersion);
    zr_hash_u32(&hash, function->contract.logicalVersion);
    zr_hash_u64(&hash, function->contract.generation);
    zr_hash_u32(&hash, function->contract.targetToken);
    zr_hash_u64(&hash, function->contract.signatureHash);
    zr_hash_u64(&hash, function->contract.layoutHash);
    zr_hash_u64(&hash, function->contract.moduleHash);
    zr_hash_u32(&hash, function->contract.requiredCapabilities);
    zr_hash_u32(&hash, function->contract.declaredEffects);
    zr_hash_u32(&hash, function->sealed);

    zr_hash_u64(&hash, aggregateHash);
    /* Frame layout is part of the ABI contract even though it is not an
     * instruction side table.  Omitting it would allow a stale summary to be
     * reused after a parameter/return-slot layout change. */
    if (function->frameLayout == ZR_NULL) {
        zr_hash_u32(&hash, 0u);
    } else {
        const SZrExecIrFrameLayout *layout = function->frameLayout;
        if (layout->slotCount > layout->slotCapacity ||
            (layout->slotCount != 0u && layout->slots == ZR_NULL)) return 0u;
        zr_hash_u32(&hash, 1u);
        zr_hash_u32(&hash, layout->storageSlotCount);
        zr_hash_u32(&hash, layout->logicalSlotCount);
        zr_hash_u32(&hash, layout->parameterPrefixCount);
        zr_hash_u32(&hash, layout->returnBufferOffset);
        zr_hash_u32(&hash, layout->frameByteSize);
        zr_hash_u32(&hash, layout->frameByteAlign);
        zr_hash_u32(&hash, layout->parameterCount);
        zr_hash_u32(&hash, layout->localCount);
        zr_hash_u64(&hash, layout->layoutHash);
        zr_hash_u32(&hash, layout->slotCount);
        for (i = 0u; i < layout->slotCount; ++i) {
            const SZrExecIrFrameSlot *slot = &layout->slots[i];
            zr_hash_u32(&hash, slot->slotId);
            zr_hash_u32(&hash, slot->byteOffset);
            zr_hash_u32(&hash, slot->byteSize);
            zr_hash_u32(&hash, slot->byteAlign);
            zr_hash_u32(&hash, slot->typeToken);
            zr_hash_u32(&hash, slot->kind);
        }
    }

    zr_hash_u32(&hash, function->valueCount);
    for (i = 0u; i < function->valueCount; ++i) {
        const SZrExecIrValue *value = &function->values[i];
        zr_hash_u32(&hash, value->id);
        zr_hash_u32(&hash, value->definition);
        zr_hash_u32(&hash, value->typeToken);
        zr_hash_u32(&hash, value->ownership);
        zr_hash_u32(&hash, value->nullability);
        zr_hash_u32(&hash, value->flags);
    }
    zr_hash_u32(&hash, function->instructionCount);
    for (i = 0u; i < function->instructionCount; ++i) {
        const SZrExecIrInstruction *instruction = &function->instructions[i];
        zr_hash_u32(&hash, instruction->opcode);
        zr_hash_u32(&hash, instruction->flags);
        zr_hash_range(&hash, instruction->resultRange);
        zr_hash_range(&hash, instruction->operandRange);
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
    zr_hash_u32(&hash, function->operandCount);
    for (i = 0u; i < function->operandCount; ++i) zr_hash_u32(&hash, function->operands[i]);
    zr_hash_u32(&hash, function->resultCount);
    for (i = 0u; i < function->resultCount; ++i) zr_hash_u32(&hash, function->results[i]);
    zr_hash_u32(&hash, function->memoryTokenCount);
    for (i = 0u; i < function->memoryTokenCount; ++i) zr_hash_u32(&hash, function->memoryTokenPool[i]);
    zr_hash_u32(&hash, function->phiCount);
    for (i = 0u; i < function->phiCount; ++i) {
        zr_hash_u32(&hash, function->phiPool[i].result);
        zr_hash_range(&hash, function->phiPool[i].incomings);
    }
    zr_hash_u32(&hash, function->phiIncomingCount);
    for (i = 0u; i < function->phiIncomingCount; ++i) {
        zr_hash_u32(&hash, function->phiIncoming[i].predecessor);
        zr_hash_u32(&hash, function->phiIncoming[i].value);
    }
    zr_hash_u32(&hash, function->predecessorCount);
    for (i = 0u; i < function->predecessorCount; ++i) zr_hash_u32(&hash, function->predecessors[i]);
    zr_hash_u32(&hash, function->successorCount);
    for (i = 0u; i < function->successorCount; ++i) zr_hash_u32(&hash, function->successors[i]);
    zr_hash_u32(&hash, function->blockCount);
    for (i = 0u; i < function->blockCount; ++i) {
        const SZrExecIrBlock *block = &function->blocks[i];
        /* immediateDominator is a derived analysis result and intentionally
         * excluded, so adding a dominator cache does not invalidate body
         * summaries. */
        zr_hash_u32(&hash, block->id);
        zr_hash_u32(&hash, block->flags);
        zr_hash_range(&hash, block->instructionRange);
        zr_hash_range(&hash, block->predecessorRange);
        zr_hash_range(&hash, block->successorRange);
        zr_hash_range(&hash, block->phis);
        zr_hash_u32(&hash, block->terminatorInstructionId);
    }
    zr_hash_u32(&hash, function->gcRootCount);
    for (i = 0u; i < function->gcRootCount; ++i) zr_hash_u32(&hash, function->gcRoots[i]);
    zr_hash_u32(&hash, function->deoptStateCount);
    for (i = 0u; i < function->deoptStateCount; ++i) {
        const SZrExecIrDeoptState *state = &function->deoptStates[i];
        zr_hash_u32(&hash, state->deoptId);
        zr_hash_u32(&hash, state->sourceId);
        zr_hash_u32(&hash, state->resumeId);
        zr_hash_range(&hash, state->valueRange);
        zr_hash_u32(&hash, state->cleanupState);
    }
    zr_hash_u32(&hash, function->deoptValueCount);
    for (i = 0u; i < function->deoptValueCount; ++i) zr_hash_u32(&hash, function->deoptValues[i]);
    zr_hash_u32(&hash, function->sourceMapCount);
    for (i = 0u; i < function->sourceMapCount; ++i) {
        const SZrExecIrSourceMap *map = &function->sourceMaps[i];
        zr_hash_u32(&hash, map->sourceId);
        zr_hash_u32(&hash, map->instructionId);
        zr_hash_u32(&hash, map->startOffset);
        zr_hash_u32(&hash, map->endOffset);
        zr_hash_u32(&hash, map->startLine);
        zr_hash_u32(&hash, map->startColumn);
        zr_hash_u32(&hash, map->endLine);
        zr_hash_u32(&hash, map->endColumn);
    }
    if (function->gcMap == ZR_NULL) {
        zr_hash_u32(&hash, 0u);
    } else {
        const SZrExecIrGcMap *gc = function->gcMap;
        if (gc->entryCount > gc->entryCapacity ||
            (gc->entryCount != 0u && gc->entries == ZR_NULL) ||
            gc->slotIndexCount > gc->slotIndexCapacity ||
            (gc->slotIndexCount != 0u && gc->slotIndexPool == ZR_NULL) ||
            gc->inlineRefOffsetCount > gc->inlineRefOffsetCapacity ||
            (gc->inlineRefOffsetCount != 0u && gc->inlineRefOffsetPool == ZR_NULL)) return 0u;
        zr_hash_u32(&hash, function->gcMapCount);
        zr_hash_u32(&hash, gc->safepointId);
        zr_hash_u32(&hash, gc->sourceId);
        zr_hash_range(&hash, gc->rootRange);
        zr_hash_u32(&hash, gc->entryCount);
        for (i = 0u; i < gc->entryCount; ++i) {
            zr_hash_u32(&hash, gc->entries[i].site);
            zr_hash_range(&hash, gc->entries[i].liveRefSlots);
            zr_hash_range(&hash, gc->entries[i].inlineRefOffsets);
        }
        zr_hash_u32(&hash, gc->slotIndexCount);
        for (i = 0u; i < gc->slotIndexCount; ++i) zr_hash_u32(&hash, gc->slotIndexPool[i]);
        zr_hash_u32(&hash, gc->inlineRefOffsetCount);
        for (i = 0u; i < gc->inlineRefOffsetCount; ++i) zr_hash_u32(&hash, gc->inlineRefOffsetPool[i]);
    }
    if (function->stateMap == ZR_NULL) {
        zr_hash_u32(&hash, 0u);
    } else {
        const SZrExecIrStateMap *stateMap = function->stateMap;
        if (stateMap->entryCount > stateMap->entryCapacity ||
            (stateMap->entryCount != 0u && stateMap->entries == ZR_NULL) ||
            stateMap->valueCount > stateMap->valueCapacity ||
            (stateMap->valueCount != 0u && stateMap->valuePool == ZR_NULL) ||
            stateMap->rootCount > stateMap->rootCapacity ||
            (stateMap->rootCount != 0u && stateMap->rootPool == ZR_NULL) ||
            stateMap->ownerStateCount > stateMap->ownerStateCapacity ||
            (stateMap->ownerStateCount != 0u && stateMap->ownerStatePool == ZR_NULL)) return 0u;
        zr_hash_u32(&hash, stateMap->functionToken);
        zr_hash_u64(&hash, stateMap->signatureHash);
        zr_hash_u64(&hash, stateMap->generation);
        zr_hash_u32(&hash, stateMap->entryCount);
        for (i = 0u; i < stateMap->entryCount; ++i) {
            const SZrExecIrStateMapEntry *entry = &stateMap->entries[i];
            zr_hash_u32(&hash, entry->sourceId);
            zr_hash_u32(&hash, entry->instructionId);
            zr_hash_u32(&hash, entry->deoptId);
            zr_hash_u32(&hash, entry->resumeId);
            zr_hash_u32(&hash, entry->cleanupState);
            zr_hash_u32(&hash, entry->boundaryFlags);
            zr_hash_u32(&hash, entry->phase);
            zr_hash_range(&hash, entry->liveValues);
            zr_hash_range(&hash, entry->rootValues);
            zr_hash_range(&hash, entry->ownerStates);
            zr_hash_u32(&hash, entry->effectIn);
            zr_hash_u32(&hash, entry->effectOut);
            zr_hash_u32(&hash, entry->handlerBlockId);
            zr_hash_u32(&hash, entry->exceptionState);
        }
        zr_hash_u32(&hash, stateMap->valueCount);
        for (i = 0u; i < stateMap->valueCount; ++i) zr_hash_u32(&hash, stateMap->valuePool[i]);
        zr_hash_u32(&hash, stateMap->rootCount);
        for (i = 0u; i < stateMap->rootCount; ++i) zr_hash_u32(&hash, stateMap->rootPool[i]);
        zr_hash_u32(&hash, stateMap->ownerStateCount);
        for (i = 0u; i < stateMap->ownerStateCount; ++i) zr_hash_u32(&hash, stateMap->ownerStatePool[i]);
    }
    return hash;
}

static TZrUInt64 zr_module_hash(const SZrExecIrModule *module) {
    TZrUInt64 hash = ZR_EXEC_IR_HASH_OFFSET;
    TZrUInt32 i;
    if (module == ZR_NULL ||
        !count_valid(module->functionCount, module->functionCapacity,
                     module->functions) ||
        !count_valid(module->constantCount, module->constantCapacity,
                     module->constants) ||
        !count_valid(module->layoutCount, module->layoutCapacity,
                     module->layouts) ||
        !count_valid(module->sourceMapCount, module->sourceMapCapacity,
                     module->sourceMaps)) return 0u;
    zr_hash_u32(&hash, module->id);
    zr_hash_u32(&hash, module->moduleToken);
    zr_hash_u64(&hash, module->moduleHash);
    zr_hash_u32(&hash, module->contract.schemaVersion);
    zr_hash_u32(&hash, module->contract.abiVersion);
    zr_hash_u32(&hash, module->contract.logicalVersion);
    zr_hash_u64(&hash, module->contract.generation);
    zr_hash_u32(&hash, module->contract.targetToken);
    zr_hash_u64(&hash, module->contract.signatureHash);
    zr_hash_u64(&hash, module->contract.layoutHash);
    zr_hash_u64(&hash, module->contract.moduleHash);
    zr_hash_u32(&hash, module->contract.requiredCapabilities);
    zr_hash_u32(&hash, module->contract.declaredEffects);
    zr_hash_u32(&hash, module->constantCount);
    for (i = 0u; i < module->constantCount; ++i) {
        zr_hash_u32(&hash, module->constants[i].typeToken);
        zr_hash_u32(&hash, module->constants[i].flags);
        zr_hash_u64(&hash, module->constants[i].bits);
    }
    zr_hash_u32(&hash, module->layoutCount);
    for (i = 0u; i < module->layoutCount; ++i) {
        zr_hash_u32(&hash, module->layouts[i].id);
        zr_hash_u32(&hash, module->layouts[i].typeToken);
        zr_hash_u32(&hash, module->layouts[i].byteSize);
        zr_hash_u32(&hash, module->layouts[i].byteAlign);
        zr_hash_u64(&hash, module->layouts[i].layoutHash);
    }
    zr_hash_u32(&hash, module->sourceMapCount);
    for (i = 0u; i < module->sourceMapCount; ++i) {
        const SZrExecIrSourceMap *map = &module->sourceMaps[i];
        zr_hash_u32(&hash, map->sourceId);
        zr_hash_u32(&hash, map->instructionId);
        zr_hash_u32(&hash, map->startOffset);
        zr_hash_u32(&hash, map->endOffset);
        zr_hash_u32(&hash, map->startLine);
        zr_hash_u32(&hash, map->startColumn);
        zr_hash_u32(&hash, map->endLine);
        zr_hash_u32(&hash, map->endColumn);
    }
    zr_hash_u32(&hash, module->functionCount);
    for (i = 0u; i < module->functionCount; ++i) {
        zr_hash_u64(&hash, zr_function_body_hash(&module->functions[i]));
    }
    return hash;
}

static TZrBool grow_array(void **storage, TZrUInt32 *capacity,
                          TZrUInt32 count, size_t elementSize) {
    TZrUInt32 next;
    void *replacement;
    if (storage == ZR_NULL || capacity == ZR_NULL || elementSize == 0u ||
        count == UINT32_MAX) return ZR_FALSE;
    if (count < *capacity) return ZR_TRUE;
    next = *capacity == 0u ? 8u : *capacity;
    while (next <= count) {
        if (next > UINT32_MAX / 2u) {
            next = count + 1u;
            break;
        }
        next *= 2u;
    }
    if ((size_t)next > SIZE_MAX / elementSize) return ZR_FALSE;
    replacement = realloc(*storage, (size_t)next * elementSize);
    if (replacement == ZR_NULL) return ZR_FALSE;
    *storage = replacement;
    *capacity = next;
    return ZR_TRUE;
}

void ZrParser_ExecIr_CallGraphInit(SZrExecIrCallGraph *graph) {
    if (graph == ZR_NULL) return;
    /* Match the other parser-owned analysis containers: reinitializing an
     * already valid graph releases its owned arrays first.  The validity
     * check deliberately rejects malformed/partially initialized storage, so
     * Init never attempts to free an untrusted pointer. */
    if (call_graph_storage_valid(graph)) {
        free(graph->summaries);
        free(graph->edges);
    }
    memset(graph, 0, sizeof(*graph));
    graph->schemaVersion = ZR_EXEC_IR_CALL_GRAPH_VERSION;
    graph->inlineBudget.maxCostPerCall = ZR_EXEC_IR_INLINE_DEFAULT_MAX_COST_PER_CALL;
    graph->inlineBudget.maxGrowthPerFunction =
        ZR_EXEC_IR_INLINE_DEFAULT_MAX_GROWTH_PER_FUNCTION;
    graph->inlineBudget.maxGrowthPerModule =
        ZR_EXEC_IR_INLINE_DEFAULT_MAX_GROWTH_PER_MODULE;
    graph->inlineBudget.maxRecursiveDepth =
        ZR_EXEC_IR_INLINE_DEFAULT_MAX_RECURSIVE_DEPTH;
}

void ZrParser_ExecIr_CallGraphFree(SZrExecIrCallGraph *graph) {
    if (graph == ZR_NULL) return;
    if (call_graph_storage_valid(graph)) {
        free(graph->summaries);
        free(graph->edges);
    }
    memset(graph, 0, sizeof(*graph));
}

TZrUInt64 ZrParser_ExecIr_CallGraphHash(const SZrExecIrCallGraph *graph) {
    TZrUInt64 hash = ZR_EXEC_IR_HASH_OFFSET;
    TZrUInt32 i;
    if (!call_graph_storage_valid(graph)) return 0u;
    zr_hash_u32(&hash, graph->schemaVersion);
    zr_hash_u64(&hash, graph->moduleHash);
    /* Revision is an invalidation counter, not content.  Excluding it keeps
     * graphHash stable when the same module is rebuilt in a fresh cache. */
    zr_hash_u32(&hash, graph->inlineBudget.maxCostPerCall);
    zr_hash_u32(&hash, graph->inlineBudget.maxGrowthPerFunction);
    zr_hash_u32(&hash, graph->inlineBudget.maxGrowthPerModule);
    zr_hash_u32(&hash, graph->inlineBudget.maxRecursiveDepth);
    zr_hash_u32(&hash, graph->summaryCount);
    for (i = 0u; i < graph->summaryCount; ++i) {
        const SZrExecIrFunctionSummary *summary = &graph->summaries[i];
        zr_hash_u32(&hash, summary->functionId);
        zr_hash_u32(&hash, summary->functionToken);
        zr_hash_u32(&hash, summary->targetToken);
        zr_hash_u64(&hash, summary->signatureHash);
        zr_hash_u64(&hash, summary->targetGeneration);
        zr_hash_u64(&hash, summary->bodyHash);
        zr_hash_u64(&hash, summary->importedHash);
        zr_hash_u64(&hash, summary->summaryHash);
        zr_hash_u32(&hash, summary->effects);
        zr_hash_u32(&hash, summary->sccId);
        zr_hash_u32(&hash, summary->fixedPointIterations);
        zr_hash_u32(&hash, summary->unknownReason);
        zr_hash_u32(&hash, summary->firstUnknownInstructionId);
        zr_hash_u32(&hash, summary->validity);
        zr_hash_u32(&hash, summary->pure);
        zr_hash_u32(&hash, summary->receiverMutates);
        zr_hash_u32(&hash, summary->allocates);
        zr_hash_u32(&hash, summary->mayThrow);
        zr_hash_u32(&hash, summary->maySuspend);
        zr_hash_u32(&hash, summary->mayEscape);
        zr_hash_u32(&hash, summary->sendSync);
        zr_hash_u32(&hash, summary->unknownEffects);
        zr_hash_u32(&hash, summary->targetFrozen);
        zr_hash_u32(&hash, summary->patchable);
    }
    zr_hash_u32(&hash, graph->edgeCount);
    for (i = 0u; i < graph->edgeCount; ++i) {
        const SZrExecIrCallEdge *edge = &graph->edges[i];
        zr_hash_u32(&hash, edge->callerId);
        zr_hash_u32(&hash, edge->callInstructionId);
        zr_hash_u32(&hash, edge->calleeId);
        zr_hash_u32(&hash, edge->targetToken);
        zr_hash_u64(&hash, edge->expectedSignatureHash);
        zr_hash_u64(&hash, edge->targetGeneration);
        zr_hash_u32(&hash, edge->kind);
        zr_hash_u32(&hash, edge->resolved);
        zr_hash_u32(&hash, edge->exactReceiver);
        zr_hash_u32(&hash, edge->guarded);
        zr_hash_u32(&hash, edge->patchableTarget);
        zr_hash_u32(&hash, edge->nativeEffectsUnknown);
        zr_hash_u32(&hash, edge->inlineEligible);
        zr_hash_u32(&hash, edge->inlineReason);
    }
    return hash;
}

const SZrExecIrFunctionSummary *ZrParser_ExecIr_CallGraphSummaryAt(
        const SZrExecIrCallGraph *graph, TZrExecIrFunctionId functionId) {
    if (!call_graph_storage_valid(graph) ||
        functionId == ZR_EXEC_IR_FUNCTION_ID_INVALID ||
        functionId > graph->summaryCount || graph->summaries == ZR_NULL) return ZR_NULL;
    return &graph->summaries[functionId - 1u];
}

const SZrExecIrCallEdge *ZrParser_ExecIr_CallGraphEdgeAt(
        const SZrExecIrCallGraph *graph, TZrUInt32 edgeIndex) {
    if (!call_graph_storage_valid(graph) || edgeIndex >= graph->edgeCount ||
        graph->edges == ZR_NULL) return ZR_NULL;
    return &graph->edges[edgeIndex];
}

TZrBool ZrParser_ExecIr_CallGraphSetInlineBudget(
        SZrExecIrCallGraph *graph, const SZrExecIrInlineBudget *budget,
        SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (graph == ZR_NULL || budget == ZR_NULL) {
        zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          ZR_NULL, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    /* Match BuildCallGraph's partial-initialization policy: a caller may use
     * a zeroed cache object and set its policy before the first build.  Only
     * the unambiguously empty representation is auto-initialized; a non-zero
     * object with a bad schema still fails instead of discarding ownership. */
    if (graph->schemaVersion == 0u && graph->summaryCount == 0u &&
        graph->summaryCapacity == 0u && graph->summaries == ZR_NULL &&
        graph->edgeCount == 0u && graph->edgeCapacity == 0u &&
        graph->edges == ZR_NULL && graph->moduleHash == 0u &&
        graph->graphHash == 0u && graph->revision == 0u) {
        ZrParser_ExecIr_CallGraphInit(graph);
    }
    if (graph->schemaVersion != ZR_EXEC_IR_CALL_GRAPH_VERSION) {
        zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          ZR_NULL, 0u, ZR_EXEC_IR_CALL_GRAPH_VERSION, 0u);
        return ZR_FALSE;
    }
    if (!call_graph_storage_valid(graph)) {
        zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          ZR_NULL, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (memcmp(&graph->inlineBudget, budget, sizeof(*budget)) != 0 &&
        (graph->moduleHash != 0u || graph->summaryCount != 0u || graph->edgeCount != 0u)) {
        graph->revision = graph->revision == UINT64_MAX ? 1u : graph->revision + 1u;
        if (graph->revision == 0u) graph->revision = 1u;
    }
    graph->inlineBudget = *budget;
    graph->graphHash = ZrParser_ExecIr_CallGraphHash(graph);
    return ZR_TRUE;
}

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

static EZrExecIrCallEdgeKind edge_kind_from_binding_kind(EZrCallBindingKind kind) {
    switch (kind) {
        case ZR_CALL_BINDING_NONE: return ZR_EXEC_IR_CALL_EDGE_UNKNOWN;
        case ZR_CALL_BINDING_DIRECT: return ZR_EXEC_IR_CALL_EDGE_DIRECT;
        case ZR_CALL_BINDING_VIRTUAL: return ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT;
        case ZR_CALL_BINDING_INTERFACE: return ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT;
        case ZR_CALL_BINDING_TYPED_FUNCTION: return ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION;
        default: return ZR_EXEC_IR_CALL_EDGE_UNKNOWN;
    }
}

static TZrBool reachable(const SZrExecIrCallGraph *graph,
                         TZrExecIrFunctionId from, TZrExecIrFunctionId to,
                         TZrBool *available) {
    TZrUInt32 n, sp = 0u, i;
    TZrExecIrFunctionId *stack;
    TZrBool *seen;
    if (available != ZR_NULL) *available = ZR_TRUE;
    if (graph == ZR_NULL || from == 0u || to == 0u || from > graph->summaryCount || to > graph->summaryCount) return ZR_FALSE;
    if (from == to) return ZR_TRUE;
    n = graph->summaryCount;
    if (!allocation_count_fits(n, sizeof(*stack)) ||
        !allocation_count_fits(n, sizeof(*seen))) {
        if (available != ZR_NULL) *available = ZR_FALSE;
        return ZR_FALSE;
    }
    stack = (TZrExecIrFunctionId *)malloc((size_t)n * sizeof(*stack));
    seen = (TZrBool *)calloc(n, sizeof(*seen));
    if (stack == ZR_NULL || seen == ZR_NULL) {
        free(stack);
        free(seen);
        if (available != ZR_NULL) *available = ZR_FALSE;
        return ZR_FALSE;
    }
    seen[from - 1u] = ZR_TRUE;
    stack[sp++] = from;
    while (sp != 0u) {
        TZrExecIrFunctionId current = stack[--sp];
        if (current == 0u || current > n) continue;
        for (i = 0u; i < graph->edgeCount; ++i) {
            const SZrExecIrCallEdge *edge = &graph->edges[i];
            if (edge->callerId != current || !edge->resolved ||
                edge->calleeId == 0u || edge->calleeId > n) continue;
            if (edge->calleeId == to) { free(stack); free(seen); return ZR_TRUE; }
            if (!seen[edge->calleeId - 1u]) {
                seen[edge->calleeId - 1u] = ZR_TRUE;
                stack[sp++] = edge->calleeId;
            }
        }
    }
    free(stack);
    free(seen);
    return ZR_FALSE;
}

static TZrBool assign_sccs(SZrExecIrCallGraph *graph) {
    TZrUInt32 i, j, next = 1u;
    TZrBool *assigned;
    if (graph == ZR_NULL) return ZR_FALSE;
    if (graph->summaryCount == 0u) return ZR_TRUE;
    if (!allocation_count_fits(graph->summaryCount, sizeof(*assigned))) return ZR_FALSE;
    assigned = (TZrBool *)calloc(graph->summaryCount, sizeof(*assigned));
    if (assigned == ZR_NULL) return ZR_FALSE;
    for (i = 0u; i < graph->summaryCount; ++i) {
        TZrBool available;
        if (assigned[i]) continue;
        graph->summaries[i].sccId = next;
        assigned[i] = ZR_TRUE;
        for (j = i + 1u; j < graph->summaryCount; ++j) {
            TZrBool forward;
            TZrBool backward;
            if (assigned[j]) continue;
            forward = reachable(graph, i + 1u, j + 1u, &available);
            if (!available) {
                free(assigned);
                return ZR_FALSE;
            }
            backward = reachable(graph, j + 1u, i + 1u, &available);
            if (!available) {
                free(assigned);
                return ZR_FALSE;
            }
            if (forward && backward) {
                graph->summaries[j].sccId = next;
                assigned[j] = ZR_TRUE;
            }
        }
        if (next != UINT32_MAX) ++next;
    }
    free(assigned);
    return ZR_TRUE;
}

static void summary_local_effects(const SZrExecIrFunction *function,
                                  SZrExecIrFunctionSummary *summary) {
    TZrUInt32 i;
    if (function == ZR_NULL || summary == ZR_NULL) return;
    summary->sendSync = ZR_TRUE;
    /* Summary facts are ABI-sensitive.  A body carrying an unknown contract
     * schema/version cannot be used as a cross-function proof even when its
     * visible opcodes look scalar; keep the boundary conservative until the
     * producer republishes it under the current execution contract. */
    if (function->contract.schemaVersion != ZR_EXECUTION_CONTRACT_SCHEMA_VERSION ||
        function->contract.abiVersion != ZR_EXECUTION_CONTRACT_ABI_VERSION ||
        function->contract.logicalVersion != ZR_EXECUTION_CONTRACT_LOGICAL_VERSION) {
        summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
        summary->unknownEffects = ZR_TRUE;
        summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY;
        summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
        summary->receiverMutates = ZR_TRUE;
        summary->allocates = ZR_TRUE;
        summary->mayThrow = ZR_TRUE;
        summary->mayEscape = ZR_TRUE;
        summary->sendSync = ZR_FALSE;
    }
    /* A body whose published signature disagrees with its local metadata is
     * not a valid optimization proof.  A distinct contract target token is
     * allowed: relocation/linking can intentionally publish a canonical
     * token that differs from the local array/function token. */
    if ((function->contract.signatureHash != 0u &&
         function->contract.signatureHash != function->signatureHash)) {
        summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
        summary->unknownEffects = ZR_TRUE;
        summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY;
        summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
        summary->receiverMutates = ZR_TRUE;
        summary->allocates = ZR_TRUE;
        summary->mayThrow = ZR_TRUE;
        summary->mayEscape = ZR_TRUE;
        summary->sendSync = ZR_FALSE;
    }
    /* A zero signature hash is an unbound/partial publication.  Even when
     * the scalar body looks pure, callers cannot prove argument/result ABI
     * compatibility without a non-zero structural signature key. */
    if (function->signatureHash == 0u) {
        summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
        summary->unknownEffects = ZR_TRUE;
        summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY;
        summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
        summary->receiverMutates = ZR_TRUE;
        summary->allocates = ZR_TRUE;
        summary->mayThrow = ZR_TRUE;
        summary->mayEscape = ZR_TRUE;
        summary->sendSync = ZR_FALSE;
    }
    /* A function with no executable body is either an external declaration or
     * an incomplete fixture.  In both cases there is no proof of a pure
     * return, so callers must treat the boundary as conservative. */
    if (function->instructionCount == 0u) {
        summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
        summary->unknownEffects = ZR_TRUE;
        summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY;
        summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
        summary->receiverMutates = ZR_TRUE;
        summary->allocates = ZR_TRUE;
        summary->mayThrow = ZR_TRUE;
        summary->mayEscape = ZR_TRUE;
        summary->sendSync = ZR_FALSE;
    }
    /* A native/AOT body may be intentionally opaque (zero ExecIR
     * instructions).  Its published contract is still an effect source and
     * must not be mistaken for a pure VM function. */
    summary->effects |= function->contract.declaredEffects &
                        ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
    /* A function published as requiring the native-call capability may have
     * no inspectable ExecIR body.  Even a precise declared-effects subset is
     * not enough to prove purity across an opaque ABI boundary, so retain the
     * full conservative lattice for callers. */
    if ((function->contract.requiredCapabilities &
         ZR_EXECUTION_CAPABILITY_NATIVE_CALL) != 0u) {
        summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
        summary->unknownEffects = ZR_TRUE;
        summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE;
        summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
        summary->receiverMutates = ZR_TRUE;
        summary->allocates = ZR_TRUE;
        summary->mayThrow = ZR_TRUE;
        summary->mayEscape = ZR_TRUE;
        summary->sendSync = ZR_FALSE;
    }
    if ((function->contract.requiredCapabilities &
         ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u) {
        /* Unknown capability bits imply an effect domain this version of the
         * analyzer cannot inspect.  Treat them like an opaque import rather
         * than silently publishing a partial summary. */
        summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
        summary->unknownEffects = ZR_TRUE;
        summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE;
        summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
        summary->receiverMutates = ZR_TRUE;
        summary->allocates = ZR_TRUE;
        summary->mayThrow = ZR_TRUE;
        summary->mayEscape = ZR_TRUE;
        summary->sendSync = ZR_FALSE;
    }
    if ((function->contract.declaredEffects & ~ZR_EXEC_IR_SUMMARY_EFFECT_ALL) != 0u) {
        summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
        summary->unknownEffects = ZR_TRUE;
        summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE;
        summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
        summary->receiverMutates = ZR_TRUE;
        summary->allocates = ZR_TRUE;
        summary->mayThrow = ZR_TRUE;
        summary->mayEscape = ZR_TRUE;
        summary->sendSync = ZR_FALSE;
    }
    for (i = 0u; i < function->instructionCount; ++i) {
        const SZrExecIrInstruction *instruction = &function->instructions[i];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
        TZrBool isCall = (TZrBool)(instruction->opcode == ZR_EXEC_IR_OPCODE_CALL ||
                                   instruction->opcode == ZR_EXEC_IR_OPCODE_INVOKE);
        TZrUInt32 effects = info != ZR_NULL ? info->effects : ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
        /* Ordinary CALL effects come from the resolved edge.  Treating the
         * opcode schema's broad native mask as local there would make even a
         * proven pure VM callee permanently conservative.  INVOKE is
         * different: its exceptional terminator contract is itself a
         * may-throw/may-allocate boundary and must remain visible even when
         * the normal callee body is pure. */
        if (isCall && instruction->opcode == ZR_EXEC_IR_OPCODE_CALL) effects = 0u;
        if (info == ZR_NULL) {
            summary->unknownEffects = ZR_TRUE;
            summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY;
            if (summary->firstUnknownInstructionId == 0u) {
                summary->firstUnknownInstructionId = i + 1u;
            }
            summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
        }
        summary->effects |= effects & ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
        /* Side-table tokens are explicit evidence even when a producer used
         * a lightweight/custom opcode whose schema has no memory mask.  Do
         * not publish such a body as pure merely because the opcode table is
         * silent about the region. */
        if (instruction->memoryIn.count != 0u) {
            summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_READ;
        }
        if (instruction->memoryOut.count != 0u) {
            summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_WRITE;
        }
        if (instruction->effectIn != 0u || instruction->effectOut != 0u) {
            summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
            summary->unknownEffects = ZR_TRUE;
            if (summary->unknownReason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE) {
                summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY;
                summary->firstUnknownInstructionId = i + 1u;
            }
            summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
        }
        /* CALL schema effects are imported from the resolved edge (or widened
         * to unknown for an unresolved edge).  INVOKE keeps its intrinsic
         * exceptional schema effects above.  Concrete instruction flags,
         * when present, remain evidence and are handled below. */
        if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_ALLOCATE) != 0u) {
            summary->allocates = ZR_TRUE;
            summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALLOCATE;
        }
        if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u) {
            summary->mayThrow = ZR_TRUE;
            summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_THROW;
        }
        if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_GC) != 0u) {
            /* A GC boundary may run finalizers/drop hooks and therefore is
             * not a purity proof, even when the opcode itself has no memory
             * effect bit in the compact schema. */
            summary->allocates = ZR_TRUE;
            summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALLOCATE;
            summary->mayEscape = ZR_TRUE;
        }
        if (info != ZR_NULL && !isCall &&
            (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC) != 0u) {
            /* Schema-level GC boundaries remain conservative even when a
             * producer omitted the optional concrete flag in a lightweight
             * fixture. */
            summary->allocates = ZR_TRUE;
            summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALLOCATE;
            summary->mayEscape = ZR_TRUE;
        }
        if (info != ZR_NULL &&
            (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND) != 0u) {
            /* Suspension is an ownership/frame boundary even when the
             * producer omitted the optional concrete MAY_SUSPEND flag. */
            summary->maySuspend = ZR_TRUE;
            summary->mayEscape = ZR_TRUE;
            summary->sendSync = ZR_FALSE;
        }
        if (info != ZR_NULL &&
            (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_DROP) != 0u) {
            /* Ownership drops are not part of the five shared contract
             * effect bits, but they still mutate lifetime state and can run
             * user-visible finalizers.  Keep the summary out of the pure
             * lattice even for a future opcode that carries MAY_DROP without
             * being named DROP below. */
            summary->receiverMutates = ZR_TRUE;
            summary->mayEscape = ZR_TRUE;
        }
        if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_SUSPEND) != 0u) {
            summary->maySuspend = ZR_TRUE;
            summary->mayEscape = ZR_TRUE;
            summary->sendSync = ZR_FALSE;
            summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_SUSPEND;
        }
        if ((instruction->flags & (ZR_EXEC_IR_FLAG_DEBUG_POLL |
                                   ZR_EXEC_IR_FLAG_GUARD_EXIT)) != 0u) {
            /* Debug/guard exits are observable control-state boundaries.  A
             * summary cannot promise that they are side-effect free because
             * the corresponding deopt/state-map protocol is not part of the
             * compact cross-function record. */
            summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
            summary->unknownEffects = ZR_TRUE;
            summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
            if (summary->unknownReason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE) {
                summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY;
                summary->firstUnknownInstructionId = i + 1u;
            }
            summary->mayEscape = ZR_TRUE;
            summary->sendSync = ZR_FALSE;
        }
        if ((instruction->opcode == ZR_EXEC_IR_OPCODE_STORE) ||
            (instruction->opcode == ZR_EXEC_IR_OPCODE_BARRIER)) {
            summary->receiverMutates = ZR_TRUE;
            summary->mayEscape = ZR_TRUE;
        }
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_DROP) {
            /* DROP is represented by the ownership memory class rather than
             * the five execution-contract effect bits.  Keep it visible in
             * the summary's mutation/escape lattice so it cannot be mistaken
             * for a pure value operation. */
            summary->receiverMutates = ZR_TRUE;
            summary->mayEscape = ZR_TRUE;
        }
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_MOVE) {
            /* MOVE changes the ownership state of its source value.  The
             * scalar call summary cannot carry that transfer across a call
             * boundary, so keep it out of the purity lattice. */
            summary->receiverMutates = ZR_TRUE;
        }
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_ALLOC) summary->allocates = ZR_TRUE;
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_RETURN && instruction->operandRange.count != 0u &&
            instruction->operandRange.start < function->operandCount && function->operands != ZR_NULL) {
            TZrUInt32 operand = function->operands[instruction->operandRange.start];
            /* Unknown ownership is commonly used for scalar values in the
             * lightweight model.  Only an explicitly managed/unique value
             * is treated as an escaping return. */
            if (operand != 0u && operand <= function->valueCount && function->values != ZR_NULL &&
                function->values[operand - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
                function->values[operand - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_BORROWED) {
                summary->mayEscape = ZR_TRUE;
            }
        }
        /* CALL/INVOKE are handled after edge resolution below. */
    }
    summary->mayThrow = (TZrBool)(summary->mayThrow ||
        (summary->effects & ZR_EXEC_IR_SUMMARY_EFFECT_THROW) != 0u);
    summary->maySuspend = (TZrBool)(summary->maySuspend ||
        (summary->effects & ZR_EXEC_IR_SUMMARY_EFFECT_SUSPEND) != 0u);
    summary->allocates = (TZrBool)(summary->allocates ||
        (summary->effects & ZR_EXEC_IR_SUMMARY_EFFECT_ALLOCATE) != 0u);
    /* Send/Sync is a stronger publication guarantee than merely having no
     * unknown native effects.  Reads/writes, ownership transitions, escape,
     * allocation, traps, and suspension all cross a boundary that this
     * compact summary cannot prove share-safe. */
    if (summary->effects != 0u || summary->unknownEffects ||
        summary->receiverMutates || summary->allocates || summary->mayThrow ||
        summary->maySuspend || summary->mayEscape) {
        summary->sendSync = ZR_FALSE;
    }
    summary->pure = (TZrBool)(summary->effects == 0u && !summary->unknownEffects &&
                              !summary->receiverMutates && !summary->mayEscape &&
                              !summary->allocates && !summary->mayThrow &&
                              !summary->maySuspend && summary->sendSync);
}

static void compute_summary_hash(SZrExecIrFunctionSummary *summary) {
    TZrUInt64 hash = ZR_EXEC_IR_HASH_OFFSET;
    if (summary == ZR_NULL) return;
    zr_hash_u32(&hash, summary->functionId);
    zr_hash_u32(&hash, summary->functionToken);
    zr_hash_u32(&hash, summary->targetToken);
    zr_hash_u64(&hash, summary->signatureHash);
    zr_hash_u64(&hash, summary->targetGeneration);
    zr_hash_u64(&hash, summary->bodyHash);
    zr_hash_u64(&hash, summary->importedHash);
    zr_hash_u32(&hash, summary->effects);
    zr_hash_u32(&hash, summary->sccId);
    zr_hash_u32(&hash, summary->unknownReason);
    zr_hash_u32(&hash, summary->firstUnknownInstructionId);
    zr_hash_u32(&hash, summary->validity);
    zr_hash_u32(&hash, summary->pure);
    zr_hash_u32(&hash, summary->receiverMutates);
    zr_hash_u32(&hash, summary->allocates);
    zr_hash_u32(&hash, summary->mayThrow);
    zr_hash_u32(&hash, summary->maySuspend);
    zr_hash_u32(&hash, summary->mayEscape);
    zr_hash_u32(&hash, summary->sendSync);
    zr_hash_u32(&hash, summary->unknownEffects);
    zr_hash_u32(&hash, summary->targetFrozen);
    zr_hash_u32(&hash, summary->patchable);
    summary->summaryHash = hash;
}

static TZrBool append_edge(SZrExecIrCallGraph *graph,
                           const SZrExecIrCallEdge *edge) {
    if (graph == ZR_NULL || edge == ZR_NULL ||
        !grow_array((void **)&graph->edges, &graph->edgeCapacity,
                    graph->edgeCount, sizeof(*graph->edges))) return ZR_FALSE;
    graph->edges[graph->edgeCount++] = *edge;
    return ZR_TRUE;
}

static TZrBool summary_fields_equal(const SZrExecIrFunctionSummary *left,
                                    const SZrExecIrFunctionSummary *right) {
    return (TZrBool)(left->effects == right->effects &&
                     left->receiverMutates == right->receiverMutates &&
                     left->allocates == right->allocates &&
                     left->mayThrow == right->mayThrow &&
                     left->maySuspend == right->maySuspend &&
                     left->mayEscape == right->mayEscape &&
                     left->sendSync == right->sendSync &&
                     left->unknownEffects == right->unknownEffects);
}

/* Unknown provenance is itself a small conservative lattice.  A caller that
 * reaches both an indirect slot and an opaque/native boundary should expose
 * the stronger native reason rather than whichever edge happened to appear
 * first in the instruction array.  Keep the first local/propagating source
 * instruction that established uncertainty; once a callee's uncertainty is
 * attached to a caller edge, later fixed-point rounds must not move that
 * location to an earlier edge in a recursive SCC. */
static TZrUInt32 unknown_reason_rank(
        EZrExecIrSummaryUnknownReason reason) {
    switch (reason) {
        case ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE: return 3u;
        case ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY: return 2u;
        case ZR_EXEC_IR_SUMMARY_UNKNOWN_INDIRECT: return 1u;
        case ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE: return 0u;
        default: return 3u;
    }
}

static void merge_unknown_reason(SZrExecIrFunctionSummary *summary,
                                 EZrExecIrSummaryUnknownReason reason,
                                 TZrExecIrInstructionId instructionId) {
    TZrUInt32 currentRank;
    TZrUInt32 incomingRank;
    if (summary == ZR_NULL || reason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE) return;
    currentRank = unknown_reason_rank(summary->unknownReason);
    incomingRank = unknown_reason_rank(reason);
    if (summary->firstUnknownInstructionId == 0u && instructionId != 0u) {
        summary->firstUnknownInstructionId = instructionId;
    }
    if (incomingRank > currentRank ||
        summary->unknownReason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE) {
        summary->unknownReason = reason;
    }
}

static void finish_summaries(SZrExecIrCallGraph *graph) {
    TZrUInt32 i, iteration, limit;
    if (graph == ZR_NULL) return;
    limit = graph->summaryCount > (UINT32_MAX - 8u) / 2u
                ? UINT32_MAX : graph->summaryCount * 2u + 8u;
    for (iteration = 1u;; ++iteration) {
        TZrBool changed = ZR_FALSE;
        for (i = 0u; i < graph->summaryCount; ++i) {
            SZrExecIrFunctionSummary before = graph->summaries[i];
            TZrUInt32 e;
            for (e = 0u; e < graph->edgeCount; ++e) {
                SZrExecIrCallEdge *edge = &graph->edges[e];
                const SZrExecIrFunctionSummary *callee;
                TZrBool nativeBoundary;
                if (edge->callerId != before.functionId) continue;
                /* A resolved slot/typed candidate is not, by itself, a
                 * whole-program effect proof: another receiver or closure
                 * can select a different body.  Until the binder publishes
                 * exact-receiver/closure facts, widen these edges just like
                 * an unresolved indirect call.  Keep the candidate callee id
                 * in the graph for diagnostics and devirtualization policy. */
                if (!edge->resolved || edge->calleeId == 0u || edge->calleeId > graph->summaryCount ||
                    edge->kind == ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION ||
                    ((edge->kind == ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT ||
                      edge->kind == ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT) &&
                     (!edge->exactReceiver || edge->guarded)) ||
                    (edge->resolved && edge->calleeId != 0u &&
                     edge->calleeId <= graph->summaryCount &&
                     graph->summaries[edge->calleeId - 1u].patchable)) {
                    nativeBoundary = (TZrBool)(edge->kind == ZR_EXEC_IR_CALL_EDGE_NATIVE ||
                                               edge->nativeEffectsUnknown ||
                                               (edge->calleeId != 0u &&
                                                edge->calleeId <= graph->summaryCount &&
                                                graph->summaries[edge->calleeId - 1u].unknownReason ==
                                                    ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE));
                    graph->summaries[i].unknownEffects = ZR_TRUE;
                    merge_unknown_reason(
                        &graph->summaries[i],
                        nativeBoundary ? ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE
                                       : ZR_EXEC_IR_SUMMARY_UNKNOWN_INDIRECT,
                        edge->callInstructionId);
                    graph->summaries[i].effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
                    graph->summaries[i].receiverMutates = ZR_TRUE;
                    graph->summaries[i].mayThrow = ZR_TRUE;
                    graph->summaries[i].allocates = ZR_TRUE;
                    graph->summaries[i].mayEscape = ZR_TRUE;
                    graph->summaries[i].sendSync = ZR_FALSE;
                    continue;
                }
                callee = &graph->summaries[edge->calleeId - 1u];
                graph->summaries[i].effects |= callee->effects;
                graph->summaries[i].receiverMutates = (TZrBool)(graph->summaries[i].receiverMutates || callee->receiverMutates);
                graph->summaries[i].allocates = (TZrBool)(graph->summaries[i].allocates || callee->allocates);
                graph->summaries[i].mayThrow = (TZrBool)(graph->summaries[i].mayThrow || callee->mayThrow);
                graph->summaries[i].maySuspend = (TZrBool)(graph->summaries[i].maySuspend || callee->maySuspend);
                graph->summaries[i].mayEscape = (TZrBool)(graph->summaries[i].mayEscape || callee->mayEscape);
                graph->summaries[i].sendSync = (TZrBool)(graph->summaries[i].sendSync && callee->sendSync);
                graph->summaries[i].unknownEffects = (TZrBool)(graph->summaries[i].unknownEffects || callee->unknownEffects);
                /* Surface imported uncertainty on the edge as well as on the
                 * caller summary.  Consumers that only inspect a callsite
                 * must not accidentally treat a native callee as proven. */
                if (callee->unknownEffects &&
                    callee->unknownReason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE) {
                    edge->nativeEffectsUnknown = ZR_TRUE;
                }
                if (callee->unknownEffects) {
                    merge_unknown_reason(
                        &graph->summaries[i],
                        callee->unknownReason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE
                            ? ZR_EXEC_IR_SUMMARY_UNKNOWN_INDIRECT
                            : callee->unknownReason,
                        edge->callInstructionId);
                }
            }
            graph->summaries[i].mayThrow = (TZrBool)(graph->summaries[i].mayThrow ||
                (graph->summaries[i].effects & ZR_EXEC_IR_SUMMARY_EFFECT_THROW) != 0u);
            graph->summaries[i].maySuspend = (TZrBool)(graph->summaries[i].maySuspend ||
                (graph->summaries[i].effects & ZR_EXEC_IR_SUMMARY_EFFECT_SUSPEND) != 0u);
            graph->summaries[i].allocates = (TZrBool)(graph->summaries[i].allocates ||
                (graph->summaries[i].effects & ZR_EXEC_IR_SUMMARY_EFFECT_ALLOCATE) != 0u);
            graph->summaries[i].pure = (TZrBool)(graph->summaries[i].effects == 0u &&
                !graph->summaries[i].unknownEffects && !graph->summaries[i].receiverMutates &&
                !graph->summaries[i].mayEscape && !graph->summaries[i].allocates &&
                !graph->summaries[i].mayThrow && !graph->summaries[i].maySuspend &&
                graph->summaries[i].sendSync);
            if (!summary_fields_equal(&before, &graph->summaries[i])) changed = ZR_TRUE;
            graph->summaries[i].fixedPointIterations = iteration;
        }
        if (!changed || iteration >= limit) break;
    }
    /* Imported hashes form a dependency key, not another recursive summary
     * fact.  Do not include callee->summaryHash here: summaryHash itself
     * contains importedHash, so including it would create a cycle for mutual
     * recursion and make the result depend on array order.  Edges inside one
     * SCC likewise omit the callee imported hash (their body/effect fields are
     * still included); cross-SCC edges are propagated to a fixed point below.
     * This makes a deep A->B->C chain invalidate A when C changes, regardless
     * of function declaration order, while keeping recursive SCC keys finite.
     */
    for (iteration = 0u; iteration < limit; ++iteration) {
        TZrBool importedChanged = ZR_FALSE;
        for (i = 0u; i < graph->summaryCount; ++i) {
            TZrUInt64 imported = ZR_EXEC_IR_HASH_OFFSET;
            TZrUInt32 e;
            TZrUInt32 callerEdgeCount = 0u;
            for (e = 0u; e < graph->edgeCount; ++e) {
                if (graph->edges[e].callerId == graph->summaries[i].functionId &&
                    callerEdgeCount != UINT32_MAX) {
                    ++callerEdgeCount;
                }
            }
            zr_hash_u32(&imported, callerEdgeCount);
            for (e = 0u; e < graph->edgeCount; ++e) {
                const SZrExecIrCallEdge *edge = &graph->edges[e];
                if (edge->callerId != graph->summaries[i].functionId) continue;
                zr_hash_u32(&imported, edge->calleeId);
                zr_hash_u32(&imported, edge->targetToken);
                zr_hash_u64(&imported, edge->expectedSignatureHash);
                zr_hash_u64(&imported, edge->targetGeneration);
                zr_hash_u32(&imported, edge->kind);
                zr_hash_u32(&imported, edge->nativeEffectsUnknown);
                if (edge->resolved && edge->calleeId <= graph->summaryCount) {
                    const SZrExecIrFunctionSummary *callee =
                        &graph->summaries[edge->calleeId - 1u];
                    zr_hash_u64(&imported, callee->bodyHash);
                    if (callee->sccId != graph->summaries[i].sccId) {
                        zr_hash_u64(&imported, callee->importedHash);
                    }
                    zr_hash_u32(&imported, callee->effects);
                    zr_hash_u32(&imported, callee->receiverMutates);
                    zr_hash_u32(&imported, callee->allocates);
                    zr_hash_u32(&imported, callee->mayThrow);
                    zr_hash_u32(&imported, callee->maySuspend);
                    zr_hash_u32(&imported, callee->mayEscape);
                    zr_hash_u32(&imported, callee->sendSync);
                    zr_hash_u32(&imported, callee->unknownEffects);
                    zr_hash_u32(&imported, callee->unknownReason);
                } else {
                    zr_hash_u32(&imported, ZR_EXEC_IR_SUMMARY_EFFECT_ALL);
                }
            }
            if (graph->summaries[i].importedHash != imported) {
                graph->summaries[i].importedHash = imported;
                importedChanged = ZR_TRUE;
            }
        }
        if (!importedChanged) break;
    }
    for (i = 0u; i < graph->summaryCount; ++i) {
        graph->summaries[i].validity = graph->summaries[i].unknownEffects
            ? ZR_EXEC_IR_SUMMARY_CONSERVATIVE : ZR_EXEC_IR_SUMMARY_STABLE;
        compute_summary_hash(&graph->summaries[i]);
    }
}

static TZrBool has_cleanup_or_exception_block(const SZrExecIrFunction *function) {
    TZrUInt32 i;
    if (function == ZR_NULL || function->blocks == ZR_NULL) return ZR_FALSE;
    for (i = 0u; i < function->blockCount; ++i) {
        if ((function->blocks[i].flags &
             (ZR_EXEC_IR_BLOCK_FLAG_CLEANUP | ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION)) != 0u) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool has_cfg_or_phi_state(const SZrExecIrFunction *function) {
    if (function == ZR_NULL) return ZR_TRUE;
    return (TZrBool)(function->phiCount != 0u ||
                     function->phiIncomingCount != 0u ||
                     function->predecessorCount != 0u ||
                     function->successorCount != 0u);
}

static TZrBool has_instruction_deopt(const SZrExecIrFunction *function) {
    TZrUInt32 i;
    if (function == ZR_NULL || function->instructions == ZR_NULL) return ZR_FALSE;
    for (i = 0u; i < function->instructionCount; ++i) {
        if (function->instructions[i].deoptId != 0u) return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool source_maps_well_formed(const SZrExecIrFunction *function) {
    TZrUInt32 i;
    if (function == ZR_NULL ||
        (function->sourceMapCount != 0u && function->sourceMaps == ZR_NULL) ||
        function->sourceMapCount > function->sourceMapCapacity) return ZR_FALSE;
    for (i = 0u; i < function->sourceMapCount; ++i) {
        TZrExecIrInstructionId instructionId = function->sourceMaps[i].instructionId;
        if (instructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            instructionId > function->instructionCount) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool range_is_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrUInt32 parameter_count_for_inline(const SZrExecIrFunction *function) {
    TZrUInt32 i;
    TZrUInt32 count = 0u;
    if (function == ZR_NULL || function->values == ZR_NULL) return 0u;
    for (i = 0u; i < function->valueCount; ++i) {
        if ((function->values[i].flags &
             ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u) {
            if (count == UINT32_MAX) return UINT32_MAX;
            ++count;
        }
    }
    return count;
}

/* Defined below with the scalar-body classifier; the call-shape check uses
 * it to validate the return ABI before advertising an inline edge. */
static TZrBool has_linear_return_shape(const SZrExecIrFunction *function);

static TZrBool scalar_call_shape_compatible(const SZrExecIrFunction *caller,
                                            const SZrExecIrFunction *callee,
                                            const SZrExecIrInstruction *call) {
    TZrUInt32 i;
    TZrUInt32 parameterIndex = 0u;
    TZrExecIrValueId returnValue;
    TZrExecIrValueId callerResult;
    if (caller == ZR_NULL || callee == ZR_NULL || call == ZR_NULL ||
        call->resultRange.count != 1u ||
        !range_is_valid(call->operandRange, caller->operandCount) ||
        !range_is_valid(call->resultRange, caller->resultCount) ||
        (call->operandRange.count != 0u && caller->operands == ZR_NULL) ||
        (call->resultRange.count != 0u && caller->results == ZR_NULL) ||
        !has_linear_return_shape(callee) || callee->values == ZR_NULL ||
        caller->values == ZR_NULL) {
        return ZR_FALSE;
    }
    if (parameter_count_for_inline(callee) != call->operandRange.count) {
        return ZR_FALSE;
    }
    /* Match the scalar ABI contract early so the graph never advertises an
     * edge that inline_one will reject after allocating a rollback copy. */
    for (i = 0u; i < callee->valueCount; ++i) {
        if ((callee->values[i].flags &
             ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) == 0u) continue;
        if (parameterIndex >= call->operandRange.count) return ZR_FALSE;
        {
            TZrExecIrValueId argument =
                caller->operands[call->operandRange.start + parameterIndex];
            if (argument == ZR_EXEC_IR_VALUE_ID_INVALID || argument > caller->valueCount) {
                return ZR_FALSE;
            }
            if (callee->values[i].typeToken != 0u &&
                caller->values[argument - 1u].typeToken != 0u &&
                callee->values[i].typeToken != caller->values[argument - 1u].typeToken) {
                return ZR_FALSE;
            }
            if (callee->values[i].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
                caller->values[argument - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
                callee->values[i].ownership != caller->values[argument - 1u].ownership) {
                return ZR_FALSE;
            }
            if (callee->values[i].nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN &&
                caller->values[argument - 1u].nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN &&
                callee->values[i].nullability !=
                    caller->values[argument - 1u].nullability) {
                return ZR_FALSE;
            }
        }
        ++parameterIndex;
    }
    callerResult = caller->results[call->resultRange.start];
    returnValue = callee->operands[
        callee->instructions[callee->instructionCount - 1u].operandRange.start];
    if (callerResult == ZR_EXEC_IR_VALUE_ID_INVALID || callerResult > caller->valueCount ||
        returnValue == ZR_EXEC_IR_VALUE_ID_INVALID || returnValue > callee->valueCount) {
        return ZR_FALSE;
    }
    if (caller->values[callerResult - 1u].typeToken != 0u &&
        callee->values[returnValue - 1u].typeToken != 0u &&
        caller->values[callerResult - 1u].typeToken !=
            callee->values[returnValue - 1u].typeToken) {
        return ZR_FALSE;
    }
    if (caller->values[callerResult - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
        callee->values[returnValue - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
        caller->values[callerResult - 1u].ownership !=
            callee->values[returnValue - 1u].ownership) {
        return ZR_FALSE;
    }
    if (caller->values[callerResult - 1u].nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN &&
        callee->values[returnValue - 1u].nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN &&
        caller->values[callerResult - 1u].nullability !=
            callee->values[returnValue - 1u].nullability) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrUInt32 inline_cost_for_function(const SZrExecIrFunction *function) {
    if (function == ZR_NULL || function->instructionCount == 0u) return UINT32_MAX;
    /* An identity body consists only of RETURN and is materialized as one
     * synthetic COPY by the inliner. */
    return function->instructionCount == 1u ? 1u : function->instructionCount - 1u;
}

static TZrUInt32 inline_growth_for_function(const SZrExecIrFunction *function) {
    TZrUInt32 cost = inline_cost_for_function(function);
    return cost == UINT32_MAX || cost == 0u ? UINT32_MAX : cost - 1u;
}

static TZrBool has_linear_return_shape(const SZrExecIrFunction *function) {
    const SZrExecIrInstruction *last;
    if (function == ZR_NULL || function->instructionCount == 0u ||
        function->instructions == ZR_NULL) return ZR_FALSE;
    last = &function->instructions[function->instructionCount - 1u];
    /* RETURN is a terminator, but the scalar inliner only knows how to
     * replace a value-producing body.  A producer may still attach an
     * effect/memory/deopt flag to a malformed or richer RETURN; treating that
     * instruction as a pure endpoint would erase the boundary. */
    return (TZrBool)(last->opcode == ZR_EXEC_IR_OPCODE_RETURN &&
                     last->flags == 0u && last->memoryIn.count == 0u &&
                     last->memoryOut.count == 0u && last->effectIn == 0u &&
                     last->effectOut == 0u && last->phiRange.count == 0u &&
                     last->successorRange.count == 0u && last->deoptId == 0u &&
                     last->operandRange.count == 1u &&
                     range_is_valid(last->operandRange, function->operandCount) &&
                     function->operands != ZR_NULL);
}

/* Keep the graph's eligibility bit in sync with the implementation in
 * exec_ir_inline.c.  Summary purity is intentionally broader than the
 * scalar inliner's supported opcode set (for example PLACE_PROJECT can be
 * effect-free but still carries pointer/ownership semantics), so checking
 * only `summary->pure` would advertise an edge that InlineCalls must later
 * reject. */
static TZrBool scalar_inline_body_supported(const SZrExecIrFunction *function) {
    TZrUInt32 i;
    TZrExecIrValueId returnValue;
    TZrBool returnProduced = ZR_FALSE;
    if (function == ZR_NULL || function->instructions == ZR_NULL ||
        !has_linear_return_shape(function)) return ZR_FALSE;
    returnValue = function->operands[
        function->instructions[function->instructionCount - 1u]
            .operandRange.start];
    for (i = 0u; i < function->instructionCount - 1u; ++i) {
        const SZrExecIrInstruction *instruction = &function->instructions[i];
        const SZrExecIrOpcodeInfo *info;
        switch ((EZrExecIrOpcode)instruction->opcode) {
            case ZR_EXEC_IR_OPCODE_CONSTANT:
            case ZR_EXEC_IR_OPCODE_CONVERT:
            case ZR_EXEC_IR_OPCODE_COPY:
            case ZR_EXEC_IR_OPCODE_ADD:
            case ZR_EXEC_IR_OPCODE_SUB:
            case ZR_EXEC_IR_OPCODE_MUL:
            case ZR_EXEC_IR_OPCODE_NEG:
            case ZR_EXEC_IR_OPCODE_COMPARE:
                break;
            default:
                return ZR_FALSE;
        }
        info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
        if (info == ZR_NULL || info->effects != 0u || instruction->flags != 0u ||
            instruction->memoryIn.count != 0u || instruction->memoryOut.count != 0u ||
            instruction->effectIn != 0u || instruction->effectOut != 0u ||
            instruction->phiRange.count != 0u || instruction->successorRange.count != 0u ||
            !range_is_valid(instruction->operandRange, function->operandCount) ||
            !range_is_valid(instruction->resultRange, function->resultCount)) {
            return ZR_FALSE;
        }
        if (instruction->resultRange.count == 1u && function->results != ZR_NULL &&
            function->results[instruction->resultRange.start] == returnValue) {
            returnProduced = ZR_TRUE;
        }
    }
    /* If a non-empty body returns an unmodified parameter, the current
     * splicer would need an ownership-aware forwarding COPY after the body.
     * Keep that shape out of the advertised candidate set until such a map is
     * implemented. */
    return (TZrBool)(function->instructionCount == 1u || returnProduced);
}

static void classify_inline_edges(SZrExecIrCallGraph *graph,
                                  const SZrExecIrModule *module) {
    TZrUInt32 i;
    if (graph == ZR_NULL || module == ZR_NULL) return;
    for (i = 0u; i < graph->edgeCount; ++i) {
        SZrExecIrCallEdge *edge = &graph->edges[i];
        const SZrExecIrFunctionSummary *callee;
        const SZrExecIrFunction *caller;
        const SZrExecIrInstruction *call;
        TZrUInt32 inlineCost;
        TZrUInt32 inlineGrowth;
        edge->inlineEligible = ZR_FALSE;
        edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_UNSUPPORTED;
        if (!edge->resolved || edge->calleeId == 0u || edge->calleeId > module->functionCount) {
            edge->inlineReason = (edge->kind == ZR_EXEC_IR_CALL_EDGE_NATIVE ||
                                  edge->kind == ZR_EXEC_IR_CALL_EDGE_UNKNOWN ||
                                  edge->nativeEffectsUnknown)
                ? ZR_EXEC_IR_INLINE_REASON_EFFECTS
                : ZR_EXEC_IR_INLINE_REASON_SIGNATURE;
            continue;
        }
        callee = &graph->summaries[edge->calleeId - 1u];
        caller = &module->functions[edge->callerId - 1u];
        call = edge->callInstructionId != 0u && edge->callInstructionId <= caller->instructionCount
                   ? &caller->instructions[edge->callInstructionId - 1u] : ZR_NULL;
        edge->patchableTarget = callee->patchable;
        edge->targetGeneration = module->functions[edge->calleeId - 1u].contract.generation;
        if (callee->patchable || !callee->targetFrozen) {
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_PATCHABLE_TARGET;
        } else if (callee->sccId == graph->summaries[edge->callerId - 1u].sccId) {
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_RECURSIVE;
        } else if (edge->kind == ZR_EXEC_IR_CALL_EDGE_UNKNOWN ||
                   edge->kind == ZR_EXEC_IR_CALL_EDGE_NATIVE ||
                   edge->nativeEffectsUnknown) {
            /* An opaque/native boundary is never an inlining proof, even if
             * a hand-built graph happens to attach a local callee id. */
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_EFFECTS;
        } else if (edge->kind == ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION) {
            /* A typed function value carries a closure/context contract that
             * is not represented in the compact call instruction.  Until a
             * binder supplies an explicit closure-valid proof, retaining the
             * indirect call is the only sound choice. */
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_SIGNATURE;
        } else if ((edge->kind == ZR_EXEC_IR_CALL_EDGE_VIRTUAL_SLOT ||
                    edge->kind == ZR_EXEC_IR_CALL_EDGE_INTERFACE_SLOT) &&
                   (!edge->exactReceiver || edge->guarded)) {
            /* A slot target is not a proof that every receiver reaches this
             * body.  The scalar inliner has no guard/deopt emission, so even a
             * profile-monorphic edge must retain its slot baseline. */
            edge->inlineReason = edge->guarded
                ? ZR_EXEC_IR_INLINE_REASON_STATE_MAP
                : ZR_EXEC_IR_INLINE_REASON_SIGNATURE;
        } else if (callee->validity != ZR_EXEC_IR_SUMMARY_STABLE || !callee->pure ||
                   callee->mayThrow || callee->maySuspend || callee->mayEscape) {
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_EFFECTS;
        } else if (callee->bodyHash == 0u || call == ZR_NULL) {
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_INVALID;
        } else if (!scalar_call_shape_compatible(
                           caller, &module->functions[edge->calleeId - 1u], call)) {
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_SIGNATURE;
        } else if (call->flags != 0u || call->memoryIn.count != 0u ||
                   call->memoryOut.count != 0u || call->effectIn != 0u ||
                   call->effectOut != 0u) {
            /* Replacing a call that participates in an effect/memory chain
             * would require token splicing.  The scalar inliner intentionally
             * leaves that contract untouched. */
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_EFFECTS;
        } else if (call->opcode == ZR_EXEC_IR_OPCODE_INVOKE) {
            /* INVOKE carries an exceptional successor contract.  The
             * restricted splicer has no way to clone that edge/cleanup map,
             * even when the current callee summary happens to be no-throw. */
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_STATE_MAP;
        } else if (edge->kind == ZR_EXEC_IR_CALL_EDGE_DIRECT &&
                   call->bindingRow != 0u &&
                   call->bindingRow != ZR_CALL_BINDING_SLOT_NONE &&
                   (call->layoutId != 0u ||
                    (call->bindingRow & ZR_EXEC_IR_BINDING_TARGET_MASK) !=
                        edge->calleeId)) {
            /* Ordinary non-zero rows are normally indices into the separate
             * BindingFacts table.  Without that table this graph cannot
             * prove that the row denotes this callee (even when an index
             * happens to equal the callee id), so do not inline and
             * accidentally erase the row contract.  A compact direct row is
             * recognized only when no layout token accompanies it. */
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_SIGNATURE;
        } else if (caller->sealed) {
            /* A sealed caller cannot be rewritten.  A sealed callee is the
             * opposite: it is an immutable publication and is safe to read
             * for an inline, subject to the generation/effect checks above. */
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_INVALID;
        } else if (caller->gcMap != ZR_NULL || caller->gcMapCount != 0u ||
                   caller->gcRootCount != 0u || caller->deoptStateCount != 0u ||
                   caller->deoptValueCount != 0u || caller->stateMap != ZR_NULL ||
                   caller->deoptAggregateCount != 0u || caller->deoptAggregateFieldCount != 0u ||
                   caller->frameLayout != ZR_NULL ||
                   module->functions[edge->calleeId - 1u].gcMap != ZR_NULL ||
                   module->functions[edge->calleeId - 1u].gcMapCount != 0u ||
                   module->functions[edge->calleeId - 1u].gcRootCount != 0u ||
                   module->functions[edge->calleeId - 1u].deoptStateCount != 0u ||
                   module->functions[edge->calleeId - 1u].deoptValueCount != 0u ||
                   module->functions[edge->calleeId - 1u].deoptAggregateCount != 0u ||
                   module->functions[edge->calleeId - 1u].deoptAggregateFieldCount != 0u ||
                   has_instruction_deopt(caller) ||
                   has_instruction_deopt(&module->functions[edge->calleeId - 1u]) ||
                   !source_maps_well_formed(caller) ||
                   !source_maps_well_formed(&module->functions[edge->calleeId - 1u]) ||
                   module->functions[edge->calleeId - 1u].stateMap != ZR_NULL ||
                   module->functions[edge->calleeId - 1u].frameLayout != ZR_NULL) {
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_STATE_MAP;
        } else if (caller->blockCount > 1u ||
                   module->functions[edge->calleeId - 1u].blockCount > 1u ||
                   has_cfg_or_phi_state(caller) ||
                   has_cfg_or_phi_state(&module->functions[edge->calleeId - 1u]) ||
                   has_cleanup_or_exception_block(caller) ||
                   has_cleanup_or_exception_block(&module->functions[edge->calleeId - 1u])) {
            /* The first implementation clones a single linear block only;
             * branch/cleanup topology needs a full CFG and exception-map
             * remap before it is safe to splice. */
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_UNSUPPORTED;
        } else if (!scalar_inline_body_supported(
                           &module->functions[edge->calleeId - 1u])) {
            /* A summary can still be useful for effect propagation when a
             * body has no terminal return (for example a declaration-only
             * fixture), but the scalar splicer must not advertise it as an
             * inline candidate. */
            edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_UNSUPPORTED;
        } else {
            inlineCost = inline_cost_for_function(
                &module->functions[edge->calleeId - 1u]);
            inlineGrowth = inline_growth_for_function(
                &module->functions[edge->calleeId - 1u]);
            if (inlineCost == UINT32_MAX ||
                inlineCost > graph->inlineBudget.maxCostPerCall ||
                inlineGrowth == UINT32_MAX ||
                inlineGrowth > graph->inlineBudget.maxGrowthPerFunction) {
                edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_COST;
            } else {
                edge->inlineEligible = ZR_TRUE;
                edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_NONE;
            }
        }
    }
}

TZrBool ZrParser_ExecIr_BuildCallGraph(
        SZrExecIrModule *module, SZrExecIrCallGraph *graph,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrCallGraph candidate = {0};
    TZrUInt64 oldRevision;
    SZrExecIrInlineBudget oldBudget;
    TZrUInt32 i, j;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (module == ZR_NULL || graph == ZR_NULL ||
        !ZrCore_ExecIr_ValidateModule(module, diagnostic)) return ZR_FALSE;
    /* Summaries are only proofs over a structurally valid SSA body.  Effect
     * tokens are intentionally checked by the later effect verifier; this
     * early gate keeps lightweight CALL fixtures usable while still
     * rejecting malformed ranges, duplicate definitions, and CFG shape. */
    for (i = 0u; i < module->functionCount; ++i) {
        if (!ZrCore_ExecIr_VerifyFunction(
                    &module->functions[i],
                    (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                            ZR_EXEC_IR_VERIFY_SSA),
                    diagnostic)) {
            return ZR_FALSE;
        }
    }

    ZrParser_ExecIr_CallGraphInit(&candidate);
    /* Treat a zero/uninitialised graph as an empty cache.  This keeps the
     * builder safe for callers that use `SZrExecIrCallGraph graph = {0}`
     * instead of the explicit Init helper, while still preserving budgets and
     * revisions for a properly initialised graph. */
    if (call_graph_storage_valid(graph)) {
        oldRevision = graph->revision;
        oldBudget = graph->inlineBudget;
        /* Preserve an explicitly zeroed budget; zero is a valid hard limit. */
        candidate.inlineBudget = oldBudget;
    } else {
        oldRevision = 0u;
        memset(&oldBudget, 0, sizeof(oldBudget));
    }
    candidate.moduleHash = zr_module_hash(module);
    if (candidate.moduleHash == 0u && module->functionCount != 0u) {
        ZrParser_ExecIr_CallGraphFree(&candidate);
        zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          ZR_NULL, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    candidate.summaryCount = module->functionCount;
    candidate.summaryCapacity = module->functionCount;
    if (module->functionCount != 0u) {
        if (!allocation_count_fits(module->functionCount,
                                    sizeof(*candidate.summaries))) {
            ZrParser_ExecIr_CallGraphFree(&candidate);
            zr_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                              ZR_NULL, 0u, module->functionCount, 0u);
            return ZR_FALSE;
        }
        candidate.summaries = (SZrExecIrFunctionSummary *)calloc(
                module->functionCount, sizeof(*candidate.summaries));
        if (candidate.summaries == ZR_NULL) {
            ZrParser_ExecIr_CallGraphFree(&candidate);
            zr_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                              ZR_NULL, 0u, 0u, 0u);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < module->functionCount; ++i) {
        const SZrExecIrFunction *function = &module->functions[i];
        SZrExecIrFunctionSummary *summary = &candidate.summaries[i];
        if (function->functionToken == 0u) {
            ZrParser_ExecIr_CallGraphFree(&candidate);
            zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              function, 0u, 1u, 0u);
            return ZR_FALSE;
        }
        summary->functionId = function->id;
        summary->functionToken = function->functionToken;
        summary->targetToken = function->contract.targetToken != 0u
            ? function->contract.targetToken : function->functionToken;
        summary->signatureHash = function->signatureHash;
        summary->targetGeneration = function->contract.generation;
        summary->bodyHash = zr_function_body_hash(function);
        /* `sealed` is the immutable publication boundary.  A sealed function
         * may legitimately have generation > 1 after a hot-reload cycle: the
         * generation identifies that publication, while sealing says that
         * this particular body can no longer be patched in place.  An
         * unsealed generation-1 builder body is still mutable and therefore
         * cannot be used as an inline/devirtualization proof.  Generation
         * zero is unpublished regardless of the seal bit. */
        summary->targetFrozen = (TZrBool)(function->contract.generation != 0u &&
                                          function->sealed);
        summary->patchable = (TZrBool)!summary->targetFrozen;
        summary_local_effects(function, summary);
        /* A function contract published for another module must not leak its
         * apparently pure body into this graph.  Zero is the explicit
         * "module hash not supplied" value used by lightweight fixtures. */
        if (function->contract.moduleHash != 0u && module->moduleHash != 0u &&
            function->contract.moduleHash != module->moduleHash) {
            summary->effects |= ZR_EXEC_IR_SUMMARY_EFFECT_ALL;
            summary->unknownEffects = ZR_TRUE;
            summary->unknownReason = ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY;
            summary->validity = ZR_EXEC_IR_SUMMARY_CONSERVATIVE;
            summary->receiverMutates = ZR_TRUE;
            summary->allocates = ZR_TRUE;
            summary->mayThrow = ZR_TRUE;
            summary->mayEscape = ZR_TRUE;
            summary->sendSync = ZR_FALSE;
            summary->pure = ZR_FALSE;
        }
        if (summary->bodyHash == 0u) {
            ZrParser_ExecIr_CallGraphFree(&candidate);
            zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              function, 0u, 0u, 0u);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < module->functionCount; ++i) {
        const SZrExecIrFunction *function = &module->functions[i];
        for (j = 0u; j < function->instructionCount; ++j) {
            const SZrExecIrInstruction *instruction = &function->instructions[j];
            SZrExecIrCallEdge edge;
            TZrExecIrFunctionId target;
            TZrBool layoutTargetProof;
            if (instruction->opcode != ZR_EXEC_IR_OPCODE_CALL &&
                instruction->opcode != ZR_EXEC_IR_OPCODE_INVOKE) continue;
            memset(&edge, 0, sizeof(edge));
            edge.callerId = function->id;
            edge.callInstructionId = j + 1u;
            edge.targetToken = instruction->layoutId != 0u ? instruction->layoutId : instruction->typeToken;
            /* An unresolved edge has no trustworthy target signature.  Keep
             * the field zero rather than leaking the caller signature into a
             * target contract slot; resolved edges fill it from the callee. */
            edge.expectedSignatureHash = 0u;
            target = resolve_instruction_target(module, instruction,
                                                &layoutTargetProof);
            if (target != ZR_EXEC_IR_FUNCTION_ID_INVALID) {
                const SZrExecIrFunction *callee = &module->functions[target - 1u];
                edge.calleeId = target;
                edge.targetToken = callee->contract.targetToken != 0u
                    ? callee->contract.targetToken : callee->functionToken;
                edge.targetGeneration = callee->contract.generation;
                edge.expectedSignatureHash = callee->signatureHash;
                edge.kind = edge_kind_from_binding_row(instruction->bindingRow);
                if (instruction->bindingRow != 0u &&
                    instruction->bindingRow != ZR_CALL_BINDING_SLOT_NONE &&
                    !compact_binding_row_valid(instruction->bindingRow)) {
                    /* A malformed/conflicting row cannot be treated as a
                     * direct proof even when a metadata token happened to
                     * identify a body.  Preserve the candidate only for
                     * diagnostics and widen its effects below. */
                    edge.kind = ZR_EXEC_IR_CALL_EDGE_NATIVE;
                    edge.nativeEffectsUnknown = ZR_TRUE;
                }
                edge.resolved = ZR_TRUE;
                edge.exactReceiver = (TZrBool)(edge.kind == ZR_EXEC_IR_CALL_EDGE_DIRECT ||
                                               /* In the compact ExecIR model
                                                * layoutId is the only field
                                                * that carries receiver-layout
                                                * evidence.  typeToken may be
                                                * the result/callable type and
                                                * must not by itself turn a
                                                * polymorphic slot into an
                                                * exact receiver proof. */
                                               layoutTargetProof);
                edge.patchableTarget = (TZrBool)candidate.summaries[target - 1u].patchable;
            } else {
                edge.kind = instruction->bindingRow == 0u ||
                            instruction->bindingRow == ZR_CALL_BINDING_SLOT_NONE
                                ? ZR_EXEC_IR_CALL_EDGE_UNKNOWN
                                : (!compact_binding_row_valid(instruction->bindingRow)
                                      ? ZR_EXEC_IR_CALL_EDGE_NATIVE
                                      : ((instruction->bindingRow &
                                          (ZR_EXEC_IR_BINDING_HINT_VIRTUAL |
                                           ZR_EXEC_IR_BINDING_HINT_INTERFACE |
                                           ZR_EXEC_IR_BINDING_HINT_TYPED)) != 0u
                                            ? edge_kind_from_binding_row(instruction->bindingRow)
                                            : ZR_EXEC_IR_CALL_EDGE_NATIVE));
                /* A native-shaped unresolved row is explicitly opaque; an
                 * unresolved slot/none row is merely indirect and should
                 * retain that distinction in the summary's unknownReason. */
                edge.nativeEffectsUnknown = (TZrBool)(
                    edge.kind == ZR_EXEC_IR_CALL_EDGE_NATIVE);
            }
            if (!append_edge(&candidate, &edge)) {
                ZrParser_ExecIr_CallGraphFree(&candidate);
                zr_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                  function, j + 1u, 0u, 0u);
                return ZR_FALSE;
            }
        }
    }
    if (!assign_sccs(&candidate)) {
        ZrParser_ExecIr_CallGraphFree(&candidate);
        zr_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                          ZR_NULL, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    finish_summaries(&candidate);
    classify_inline_edges(&candidate, module);
    if (graph->schemaVersion == ZR_EXEC_IR_CALL_GRAPH_VERSION &&
        graph->moduleHash == candidate.moduleHash && oldRevision != 0u &&
        oldBudget.maxCostPerCall == candidate.inlineBudget.maxCostPerCall &&
        oldBudget.maxGrowthPerFunction == candidate.inlineBudget.maxGrowthPerFunction &&
        oldBudget.maxGrowthPerModule == candidate.inlineBudget.maxGrowthPerModule &&
        oldBudget.maxRecursiveDepth == candidate.inlineBudget.maxRecursiveDepth) {
        /* Rebuilding an unchanged module is idempotent; consumers can use
         * revision as an invalidation counter instead of a build counter. */
        candidate.revision = oldRevision;
    } else {
        candidate.revision = oldRevision == UINT64_MAX ? 1u : oldRevision + 1u;
        if (candidate.revision == 0u) candidate.revision = 1u;
    }
    candidate.graphHash = ZrParser_ExecIr_CallGraphHash(&candidate);
    if (call_graph_storage_valid(graph)) {
        ZrParser_ExecIr_CallGraphFree(graph);
    } else {
        /* No owned pointers are trusted from an uninitialised cache object. */
        memset(graph, 0, sizeof(*graph));
    }
    *graph = candidate;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_CallGraphValidate(
        const SZrExecIrCallGraph *graph, const SZrExecIrModule *module,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (!call_graph_storage_valid(graph)) {
        zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          ZR_NULL, 0u, ZR_EXEC_IR_CALL_GRAPH_VERSION,
                          graph == ZR_NULL ? 0u : graph->schemaVersion);
        return ZR_FALSE;
    }
    if (module != ZR_NULL &&
        !ZrCore_ExecIr_ValidateModule(module, diagnostic)) {
        return ZR_FALSE;
    }
    if (module != ZR_NULL) {
        /* Call-site ids and inline side-table rewrites are only meaningful
         * for a structurally valid SSA body.  Keep the effect verifier out of
         * this cache check (lightweight CALL fixtures intentionally omit
         * token chains), but reject malformed ranges/duplicate definitions
         * before any consumer dereferences an edge. */
        for (i = 0u; i < module->functionCount; ++i) {
            if (!ZrCore_ExecIr_VerifyFunction(
                        &module->functions[i],
                        (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                                ZR_EXEC_IR_VERIFY_SSA),
                        diagnostic)) {
                return ZR_FALSE;
            }
        }
    }
    if (module != ZR_NULL && graph->moduleHash != zr_module_hash(module)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH;
            diagnostic->expectedHash = zr_module_hash(module);
            diagnostic->actualHash = graph->moduleHash;
        }
        return ZR_FALSE;
    }
    if (module != ZR_NULL && graph->summaryCount != module->functionCount) {
        zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                          ZR_NULL, 0u, module->functionCount,
                          graph->summaryCount);
        return ZR_FALSE;
    }
    for (i = 0u; i < graph->summaryCount; ++i) {
        const SZrExecIrFunctionSummary *summary = &graph->summaries[i];
        const SZrExecIrFunction *summaryFunction =
            module != ZR_NULL ? &module->functions[i] : ZR_NULL;
        if (summary->functionId != i + 1u || summary->functionToken == 0u ||
            (summary->effects & ~ZR_EXEC_IR_SUMMARY_EFFECT_ALL) != 0u ||
            summary->unknownReason < ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE ||
            summary->unknownReason > ZR_EXEC_IR_SUMMARY_UNKNOWN_INVALID_BODY ||
            summary->validity < ZR_EXEC_IR_SUMMARY_UNKNOWN ||
            summary->validity > ZR_EXEC_IR_SUMMARY_INVALID ||
            summary->pure > ZR_TRUE || summary->receiverMutates > ZR_TRUE ||
            summary->allocates > ZR_TRUE || summary->mayThrow > ZR_TRUE ||
            summary->maySuspend > ZR_TRUE || summary->mayEscape > ZR_TRUE ||
            summary->sendSync > ZR_TRUE || summary->unknownEffects > ZR_TRUE ||
            summary->targetFrozen > ZR_TRUE || summary->patchable > ZR_TRUE ||
            (summary->targetFrozen && summary->patchable) ||
            (summary->pure && (summary->effects != 0u || summary->unknownEffects ||
                               summary->receiverMutates || summary->allocates ||
                               summary->mayThrow || summary->maySuspend ||
                               summary->mayEscape || !summary->sendSync)) ||
            (summary->pure && summary->validity != ZR_EXEC_IR_SUMMARY_STABLE) ||
            (graph->summaryCount != 0u &&
             (summary->sccId == 0u || summary->sccId > graph->summaryCount)) ||
            (summary->validity == ZR_EXEC_IR_SUMMARY_STABLE &&
             summary->unknownEffects) ||
            (graph->graphHash != 0u &&
             summary->validity == ZR_EXEC_IR_SUMMARY_UNKNOWN) ||
            (summary->validity == ZR_EXEC_IR_SUMMARY_INVALID &&
             !summary->unknownEffects) ||
            (!summary->unknownEffects &&
             summary->unknownReason != ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE) ||
            (!summary->unknownEffects && summary->firstUnknownInstructionId != 0u) ||
            (summary->unknownEffects &&
             summary->unknownReason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NONE)) {
            zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              summaryFunction, i + 1u, i + 1u,
                              summary->functionId);
            return ZR_FALSE;
        }
        if (graph->graphHash != 0u && summary->summaryHash == 0u) {
            zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              summaryFunction, i + 1u, 1u, 0u);
            return ZR_FALSE;
        }
        if (summary->summaryHash != 0u) {
            SZrExecIrFunctionSummary copy = *summary;
            compute_summary_hash(&copy);
            if (copy.summaryHash != summary->summaryHash) {
                zr_set_hash_diagnostic(diagnostic,
                                       ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       summaryFunction, i + 1u,
                                       summary->summaryHash,
                                       copy.summaryHash);
                return ZR_FALSE;
            }
        }
        if (module != ZR_NULL &&
            (summary->functionToken != module->functions[i].functionToken ||
             summary_target_token(summary) !=
                 (module->functions[i].contract.targetToken != 0u
                      ? module->functions[i].contract.targetToken
                      : module->functions[i].functionToken) ||
             summary->signatureHash != module->functions[i].signatureHash ||
             summary->targetGeneration != module->functions[i].contract.generation ||
             summary->bodyHash != zr_function_body_hash(&module->functions[i]) ||
             summary->targetFrozen !=
                 (TZrBool)(module->functions[i].contract.generation != 0u &&
                           module->functions[i].sealed) ||
             summary->patchable != (TZrBool)!summary->targetFrozen)) {
            zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                              &module->functions[i], 0u, 0u, 0u);
            return ZR_FALSE;
        }
        if (module != ZR_NULL &&
            summary->firstUnknownInstructionId >
                module->functions[i].instructionCount) {
            zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              &module->functions[i],
                              summary->firstUnknownInstructionId,
                              module->functions[i].instructionCount,
                              summary->firstUnknownInstructionId);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < graph->edgeCount; ++i) {
        const SZrExecIrCallEdge *edge = &graph->edges[i];
        const SZrExecIrFunctionSummary *calleeSummary = ZR_NULL;
        const SZrExecIrFunction *callerFunction =
            module != ZR_NULL && edge->callerId != 0u &&
            edge->callerId <= module->functionCount
                ? &module->functions[edge->callerId - 1u]
                : ZR_NULL;
        if (edge->callerId == 0u || edge->callerId > graph->summaryCount ||
            edge->callInstructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            (!edge->resolved && edge->calleeId != 0u) ||
            (edge->resolved && (edge->calleeId == 0u || edge->calleeId > graph->summaryCount)) ||
            (edge->resolved && edge->kind == ZR_EXEC_IR_CALL_EDGE_UNKNOWN) ||
            (!edge->resolved && (edge->exactReceiver || edge->guarded ||
                                 edge->patchableTarget || edge->inlineEligible)) ||
            edge->kind < ZR_EXEC_IR_CALL_EDGE_UNKNOWN ||
            edge->kind > ZR_EXEC_IR_CALL_EDGE_NATIVE ||
            edge->inlineReason < ZR_EXEC_IR_INLINE_REASON_NONE ||
            edge->inlineReason > ZR_EXEC_IR_INLINE_REASON_INVALID ||
            edge->resolved > ZR_TRUE || edge->exactReceiver > ZR_TRUE ||
            edge->guarded > ZR_TRUE || edge->patchableTarget > ZR_TRUE ||
            edge->nativeEffectsUnknown > ZR_TRUE || edge->inlineEligible > ZR_TRUE) {
            zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              callerFunction, edge->callInstructionId,
                              graph->summaryCount,
                              edge->calleeId);
            return ZR_FALSE;
        }
        if (edge->resolved) calleeSummary = &graph->summaries[edge->calleeId - 1u];
        if (calleeSummary != ZR_NULL && (graph->graphHash != 0u || module != ZR_NULL) &&
            (edge->targetToken != summary_target_token(calleeSummary) ||
             edge->expectedSignatureHash != calleeSummary->signatureHash ||
             edge->targetGeneration != calleeSummary->targetGeneration)) {
            /* A built graph always carries the complete target identity.  A
             * mismatch means a caller is trying to consume a stale edge;
             * never let it silently turn into a different local function. */
            zr_set_hash_diagnostic(diagnostic,
                                   ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                                   module != ZR_NULL
                                       ? &module->functions[edge->callerId - 1u]
                                       : ZR_NULL,
                                   edge->callInstructionId,
                                   summary_target_token(calleeSummary),
                                   edge->targetToken);
            return ZR_FALSE;
        }
        if (graph->graphHash != 0u) {
            if ((!edge->inlineEligible &&
                 edge->inlineReason == ZR_EXEC_IR_INLINE_REASON_NONE) ||
                (edge->inlineEligible &&
                 edge->inlineReason != ZR_EXEC_IR_INLINE_REASON_NONE) ||
                (calleeSummary != ZR_NULL &&
                 edge->patchableTarget != calleeSummary->patchable) ||
                (!edge->resolved &&
                 (edge->expectedSignatureHash != 0u ||
                  edge->targetGeneration != 0u))) {
                zr_set_diagnostic(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  callerFunction, edge->callInstructionId,
                                  0u, 0u);
                return ZR_FALSE;
            }
        }
        if (module != ZR_NULL &&
            (edge->callInstructionId == 0u ||
             edge->callInstructionId > module->functions[edge->callerId - 1u].instructionCount)) {
            zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              &module->functions[edge->callerId - 1u],
                              edge->callInstructionId, 0u,
                              module->functions[edge->callerId - 1u].instructionCount);
            return ZR_FALSE;
        }
        if (module != ZR_NULL && edge->callInstructionId != 0u) {
            TZrUInt16 opcode = module->functions[edge->callerId - 1u]
                                   .instructions[edge->callInstructionId - 1u].opcode;
            if (opcode != ZR_EXEC_IR_OPCODE_CALL &&
                opcode != ZR_EXEC_IR_OPCODE_INVOKE) {
                zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  &module->functions[edge->callerId - 1u],
                                  edge->callInstructionId,
                                  ZR_EXEC_IR_OPCODE_CALL, opcode);
                return ZR_FALSE;
            }
        }
    }
    if (graph->graphHash != 0u && graph->graphHash != ZrParser_ExecIr_CallGraphHash(graph)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
            diagnostic->expectedHash = ZrParser_ExecIr_CallGraphHash(graph);
            diagnostic->actualHash = graph->graphHash;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ResolveCallTarget(
        const SZrExecIrCallGraph *graph,
        const SZrExecIrCallTargetRequest *request,
        SZrExecIrCallTargetResult *result,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrFunctionSummary *summary = ZR_NULL;
    TZrBool ambiguous = ZR_FALSE;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (result != ZR_NULL) memset(result, 0, sizeof(*result));
    if (!call_graph_storage_valid(graph) || request == ZR_NULL || result == ZR_NULL) {
        zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          ZR_NULL, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (request->bindingKind < ZR_CALL_BINDING_NONE ||
        request->bindingKind > ZR_CALL_BINDING_TYPED_FUNCTION ||
        request->exactReceiver > ZR_TRUE ||
        request->profileMonomorphic > ZR_TRUE ||
        request->deoptMapAvailable > ZR_TRUE ||
        request->targetFrozen > ZR_TRUE ||
        request->targetPatchable > ZR_TRUE ||
        request->closureContextValid > ZR_TRUE ||
        ((request->bindingKind == ZR_CALL_BINDING_VIRTUAL ||
          request->bindingKind == ZR_CALL_BINDING_INTERFACE) &&
         request->dispatchSlot == ZR_CALL_BINDING_SLOT_NONE)) {
        zr_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          ZR_NULL, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrParser_ExecIr_CallGraphValidate(graph, ZR_NULL, diagnostic)) {
        return ZR_FALSE;
    }
    if (request->moduleHash != 0u && request->moduleHash != graph->moduleHash) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH;
            diagnostic->expectedHash = graph->moduleHash;
            diagnostic->actualHash = request->moduleHash;
        }
        return ZR_FALSE;
    }
    if (request->bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION &&
        !request->closureContextValid) {
        result->kind = ZR_EXEC_IR_CALL_EDGE_TYPED_FUNCTION;
        result->guarded = ZR_FALSE;
        result->inlineAllowed = ZR_FALSE;
        result->inlineReason = ZR_EXEC_IR_INLINE_REASON_SIGNATURE;
        return ZR_TRUE;
    }
    if (request->expectedReceiverTypeToken != 0u &&
        request->receiverTypeToken != request->expectedReceiverTypeToken) {
        result->kind = edge_kind_from_binding_kind(request->bindingKind);
        result->guarded = ZR_FALSE;
        result->inlineAllowed = ZR_FALSE;
        result->inlineReason = ZR_EXEC_IR_INLINE_REASON_SIGNATURE;
        return ZR_TRUE;
    }
    if (request->targetFunctionId != 0u) {
        summary = ZrParser_ExecIr_CallGraphSummaryAt(graph, request->targetFunctionId);
    } else if (request->targetToken != 0u) {
        TZrUInt32 i;
        for (i = 0u; i < graph->summaryCount; ++i) {
            if (graph->summaries[i].functionToken != request->targetToken &&
                summary_target_token(&graph->summaries[i]) != request->targetToken) continue;
            if (summary != ZR_NULL) { ambiguous = ZR_TRUE; break; }
            summary = &graph->summaries[i];
        }
    }
    if (ambiguous) {
        zr_set_hash_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                               ZR_NULL, 0u, request->targetToken,
                               request->targetToken);
        return ZR_FALSE;
    }
    if (summary == ZR_NULL) {
        result->kind = edge_kind_from_binding_kind(request->bindingKind);
        result->guarded = (TZrBool)(request->profileMonomorphic && request->deoptMapAvailable &&
                                    (request->bindingKind == ZR_CALL_BINDING_VIRTUAL ||
                                     request->bindingKind == ZR_CALL_BINDING_INTERFACE));
        result->inlineReason = result->guarded
            ? ZR_EXEC_IR_INLINE_REASON_STATE_MAP
            : ZR_EXEC_IR_INLINE_REASON_UNSUPPORTED;
        /* Explicit target identities are link-time contracts.  Silently
         * turning a missing direct/token target into a slot call would make a
         * stale call execute a different implementation. */
        if (request->targetFunctionId != 0u || request->targetToken != 0u) {
            zr_set_hash_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                                   ZR_NULL, 0u,
                                   request->targetToken != 0u ? request->targetToken
                                                              : request->targetFunctionId,
                                   0u);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    if (request->targetToken != 0u &&
        request->targetToken != summary->functionToken &&
        request->targetToken != summary_target_token(summary)) {
        zr_set_hash_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                               ZR_NULL, 0u, summary_target_token(summary),
                               request->targetToken);
        return ZR_FALSE;
    }
    if ((request->signatureHash == 0u && summary->signatureHash != 0u) ||
        (request->signatureHash != 0u &&
         request->signatureHash != summary->signatureHash)) {
        zr_set_hash_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                               ZR_NULL, 0u, summary->signatureHash,
                               request->signatureHash);
        return ZR_FALSE;
    }
    if (request->generation != 0u && request->generation != summary->targetGeneration) {
        /* A caller that supplied a generation is asking for that exact
         * publication.  Stale evidence is a structured link failure; only a
         * profile request with generation==0 may fall back to a guard. */
        zr_set_hash_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,
                               ZR_NULL, 0u, summary->targetGeneration,
                               request->generation);
        return ZR_FALSE;
    }
    result->targetFunctionId = summary->functionId;
    result->dispatchSlot = request->dispatchSlot;
    result->resolved = ZR_TRUE;
    /* Compute guard state before choosing the representation.  The previous
     * order could accidentally emit a direct call for a profile-only proof. */
    result->guarded = (TZrBool)(request->profileMonomorphic && request->deoptMapAvailable &&
                                (request->bindingKind == ZR_CALL_BINDING_VIRTUAL ||
                                 request->bindingKind == ZR_CALL_BINDING_INTERFACE));
    result->kind = request->exactReceiver && request->targetFrozen &&
                   summary->targetFrozen && !summary->patchable &&
                   !request->targetPatchable && !result->guarded
                       ? ZR_EXEC_IR_CALL_EDGE_DIRECT
                       : edge_kind_from_binding_kind(request->bindingKind);
    result->inlineAllowed = (TZrBool)(request->targetFrozen && summary->targetFrozen &&
                                      !summary->patchable && summary->validity == ZR_EXEC_IR_SUMMARY_STABLE &&
                                      summary->pure && !summary->mayThrow && !summary->maySuspend &&
                                      !summary->mayEscape && !request->targetPatchable &&
                                      !result->guarded &&
                                      request->bindingKind != ZR_CALL_BINDING_NONE &&
                                      request->bindingKind != ZR_CALL_BINDING_TYPED_FUNCTION &&
                                      (request->exactReceiver ||
                                       request->bindingKind == ZR_CALL_BINDING_DIRECT));
    result->inlineReason = result->inlineAllowed ? ZR_EXEC_IR_INLINE_REASON_NONE
        : (summary->patchable || request->targetPatchable || !request->targetFrozen
               ? ZR_EXEC_IR_INLINE_REASON_PATCHABLE_TARGET
           : (result->guarded ? ZR_EXEC_IR_INLINE_REASON_STATE_MAP
                              : (request->bindingKind == ZR_CALL_BINDING_NONE ||
                                 request->bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION ||
                                 !(request->exactReceiver ||
                                    request->bindingKind == ZR_CALL_BINDING_DIRECT)
                                      ? ZR_EXEC_IR_INLINE_REASON_SIGNATURE
                                      : ZR_EXEC_IR_INLINE_REASON_EFFECTS)));
    return ZR_TRUE;
}
