#include "exec_ir_fusion_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * This unit owns the generated schema, storage guards, and deterministic
 * input/plan hashes.  Fusion is a projection contract, not a second
 * interpreter: source ExecIR remains authoritative and records contain only
 * stable scalar IDs.
 */

static TZrUInt64 zr_fusion_hash_byte(TZrUInt64 hash, TZrUInt8 value) {
    return (hash ^ (TZrUInt64)value) * UINT64_C(1099511628211);
}

static TZrUInt64 zr_fusion_hash_u32(TZrUInt64 hash, TZrUInt32 value) {
    TZrUInt32 index;
    for (index = 0u; index < 4u; ++index) {
        hash = zr_fusion_hash_byte(hash, (TZrUInt8)(value >> (index * 8u)));
    }
    return hash;
}

static TZrUInt64 zr_fusion_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    TZrUInt32 index;
    for (index = 0u; index < 8u; ++index) {
        hash = zr_fusion_hash_byte(hash, (TZrUInt8)(value >> (index * 8u)));
    }
    return hash;
}

TZrBool zr_fusion_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count &&
                     range.count <= count - range.start);
}

TZrBool zr_fusion_pointer_count_valid(const void *pointer,
                                             TZrUInt32 count,
                                             TZrUInt32 capacity) {
    /* A zero-capacity container must not retain a non-owned pointer.  Apart
     * from catching malformed inputs, this lets Plan_Free safely distinguish
     * an initialized empty record from an arbitrary storage-tag spoof. */
    return (TZrBool)(count <= capacity &&
                     (capacity == 0u ? pointer == ZR_NULL
                                     : pointer != ZR_NULL));
}

void zr_fusion_diag_clear(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

void zr_fusion_diag_set(SZrExecIrDiagnostic *diagnostic,
                               EZrExecutionDiagnosticCode code,
                               const SZrExecIrFunction *function,
                               TZrUInt32 instructionId,
                               TZrUInt32 blockId,
                               TZrUInt32 expected,
                               TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->blockId = blockId;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
    if (function != ZR_NULL && instructionId != 0u &&
        instructionId <= function->instructionCount &&
        function->instructions != ZR_NULL) {
        diagnostic->sourceId = function->instructions[instructionId - 1u].sourceId;
    }
}

static const SZrExecBcFusionPatternInfo g_zr_fusion_patterns[] = {
#define ZR_EXEC_BC_FUSION_PATTERN_ROW(name, head, tail, constraints, result, cost, boundary) \
    { ZR_EXEC_BC_FUSION_PATTERN_##name, head, tail, constraints, result, cost, \
      (TZrUInt32)sizeof(SZrInstruction), boundary, #name },
    ZR_EXEC_BC_FUSION_PATTERN_ROWS(ZR_EXEC_BC_FUSION_PATTERN_ROW)
#undef ZR_EXEC_BC_FUSION_PATTERN_ROW
};

_Static_assert(sizeof(g_zr_fusion_patterns) /
                       sizeof(g_zr_fusion_patterns[0]) ==
                       ZR_EXEC_BC_FUSION_PATTERN_COUNT,
               "generated fusion metadata table must match enum count");

TZrBool zr_fusion_plan_storage_is_valid(const SZrExecBcFusionPlan *plan) {
    if (plan == ZR_NULL || plan->storageTag != ZR_EXEC_BC_FUSION_STORAGE_TAG ||
        plan->schemaVersion != ZR_EXEC_BC_FUSION_SCHEMA_VERSION ||
        plan->valid > ZR_TRUE || plan->reserved0 != 0u ||
        plan->reserved1 != 0u ||
        (TZrUInt32)plan->invalidationReason >=
                ZR_EXEC_BC_FUSION_INVALIDATION_COUNT ||
        !zr_fusion_pointer_count_valid(plan->instructions,
                                       plan->instructionCount,
                                       plan->instructionCapacity) ||
        !zr_fusion_pointer_count_valid(plan->originalInstructions,
                                       plan->originalInstructionCount,
                                       plan->originalInstructionCount) ||
        !zr_fusion_pointer_count_valid(plan->sideEntries,
                                       plan->sideEntryCount,
                                       plan->sideEntryCapacity) ||
        !zr_fusion_pointer_count_valid(plan->sourceMaps,
                                       plan->sourceMapCount,
                                       plan->sourceMapCapacity) ||
        !zr_fusion_pointer_count_valid(plan->fallbacks,
                                       plan->fallbackCount,
                                       plan->fallbackCapacity)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

void ZrParser_ExecBcPatternOptions_Init(SZrExecBcPatternOptions *options) {
    if (options == ZR_NULL) {
        return;
    }
    memset(options, 0, sizeof(*options));
    options->schemaVersion = ZR_EXEC_BC_FUSION_SCHEMA_VERSION;
    options->enabledPatternMask =
            ZR_EXEC_BC_FUSION_PATTERN_COUNT >= 32u
                ? UINT32_MAX
                : ((TZrUInt32)1u << ZR_EXEC_BC_FUSION_PATTERN_COUNT) - 1u;
    options->maxFusedCount = UINT32_MAX;
    options->maxSideTableEntries = UINT16_MAX;
    options->maxFallbackEntries = UINT32_MAX;
    options->minDispatchBenefit = 1u;
    options->preserveObservableBoundaries = ZR_TRUE;
    options->allowUnresolvedBinding = ZR_FALSE;
}

void ZrParser_ExecBcFusionPlan_Init(SZrExecBcFusionPlan *plan) {
    if (plan != ZR_NULL) {
        memset(plan, 0, sizeof(*plan));
        plan->storageTag = ZR_EXEC_BC_FUSION_STORAGE_TAG;
        plan->schemaVersion = ZR_EXEC_BC_FUSION_SCHEMA_VERSION;
        plan->invalidationReason = ZR_EXEC_BC_FUSION_INVALIDATION_NONE;
    }
}

void ZrParser_ExecBcFusionPlan_Free(SZrExecBcFusionPlan *plan) {
    if (plan == ZR_NULL) {
        return;
    }
    /*
     * Do not free arbitrary pointers in an uninitialised output record.  This
     * makes partial-init and diagnostic-path cleanup safe while retaining the
     * normal idempotent Free contract.
     */
    if (zr_fusion_plan_storage_is_valid(plan)) {
        free(plan->instructions);
        free(plan->originalInstructions);
        free(plan->sideEntries);
        free(plan->sourceMaps);
        free(plan->fallbacks);
    }
    memset(plan, 0, sizeof(*plan));
}

const SZrExecBcFusionPatternInfo *
ZrParser_ExecBcFusion_PatternInfo(EZrExecBcFusionPattern pattern) {
    if ((TZrUInt32)pattern >= ZR_EXEC_BC_FUSION_PATTERN_COUNT) {
        return ZR_NULL;
    }
    return &g_zr_fusion_patterns[(TZrUInt32)pattern];
}

TZrUInt32 ZrParser_ExecBcFusion_PatternCount(void) {
    return ZR_EXEC_BC_FUSION_PATTERN_COUNT;
}

TZrUInt64 ZrParser_ExecBcFusion_PatternSchemaHash(void) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 index;
    for (index = 0u; index < ZR_EXEC_BC_FUSION_PATTERN_COUNT; ++index) {
        const SZrExecBcFusionPatternInfo *info = &g_zr_fusion_patterns[index];
        const TZrChar *name = info->name;
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)info->pattern);
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)info->headOpcode);
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)info->tailOpcode);
        hash = zr_fusion_hash_u32(hash, info->constraints);
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)info->resultForm);
        hash = zr_fusion_hash_u32(hash, info->dispatchBenefit);
        hash = zr_fusion_hash_u32(hash, info->codeCostBytes);
        hash = zr_fusion_hash_u32(hash, info->boundaryMask);
        while (name != ZR_NULL && *name != '\0') {
            hash = zr_fusion_hash_byte(hash, (TZrUInt8)*name++);
        }
        hash = zr_fusion_hash_byte(hash, 0u);
    }
    return hash == 0u ? UINT64_C(1) : hash;
}

static TZrUInt64 zr_fusion_hash_instruction(TZrUInt64 hash,
                                             const SZrExecIrInstruction *instruction) {
    hash = zr_fusion_hash_u32(hash, instruction->opcode);
    hash = zr_fusion_hash_u32(hash, instruction->flags);
    hash = zr_fusion_hash_u32(hash, instruction->results.start);
    hash = zr_fusion_hash_u32(hash, instruction->results.count);
    hash = zr_fusion_hash_u32(hash, instruction->operands.start);
    hash = zr_fusion_hash_u32(hash, instruction->operands.count);
    hash = zr_fusion_hash_u32(hash, instruction->phiRange.start);
    hash = zr_fusion_hash_u32(hash, instruction->phiRange.count);
    hash = zr_fusion_hash_u32(hash, instruction->successorRange.start);
    hash = zr_fusion_hash_u32(hash, instruction->successorRange.count);
    hash = zr_fusion_hash_u32(hash, instruction->typeToken);
    hash = zr_fusion_hash_u32(hash, instruction->layoutId);
    hash = zr_fusion_hash_u32(hash, instruction->memoryIn.start);
    hash = zr_fusion_hash_u32(hash, instruction->memoryIn.count);
    hash = zr_fusion_hash_u32(hash, instruction->memoryOut.start);
    hash = zr_fusion_hash_u32(hash, instruction->memoryOut.count);
    hash = zr_fusion_hash_u32(hash, instruction->effectIn);
    hash = zr_fusion_hash_u32(hash, instruction->effectOut);
    hash = zr_fusion_hash_u32(hash, instruction->sourceId);
    hash = zr_fusion_hash_u32(hash, instruction->deoptId);
    hash = zr_fusion_hash_u32(hash, instruction->bindingRow);
    return hash;
}

static TZrBool zr_fusion_gc_map_is_valid(const SZrExecIrFunction *function) {
    const SZrExecIrGcMap *map;
    TZrUInt32 index;
    if (function == ZR_NULL || function->gcMapCount > function->gcMapCapacity ||
        (function->gcMapCount != 0u && function->gcMap == ZR_NULL)) {
        return ZR_FALSE;
    }
    map = function->gcMap;
    if (map == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!zr_fusion_pointer_count_valid(map->entries, map->entryCount,
                                       map->entryCapacity) ||
        !zr_fusion_pointer_count_valid(map->slotIndexPool,
                                       map->slotIndexCount,
                                       map->slotIndexCapacity) ||
        !zr_fusion_pointer_count_valid(map->inlineRefOffsetPool,
                                       map->inlineRefOffsetCount,
                                       map->inlineRefOffsetCapacity) ||
        !zr_fusion_range_valid(map->rootRange, map->slotIndexCount)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < map->entryCount; ++index) {
        const SZrExecIrGcMapEntry *entry = &map->entries[index];
        TZrUInt32 slot;
        if (entry->site == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            entry->site > function->instructionCount ||
            !zr_fusion_range_valid(entry->liveRefSlots,
                                   map->slotIndexCount) ||
            !zr_fusion_range_valid(entry->inlineRefOffsets,
                                   map->inlineRefOffsetCount)) {
            return ZR_FALSE;
        }
        if (function->frameLayout != ZR_NULL) {
            for (slot = 0u; slot < entry->liveRefSlots.count; ++slot) {
                if (map->slotIndexPool[entry->liveRefSlots.start + slot] >=
                    function->frameLayout->slotCount) {
                    return ZR_FALSE;
                }
            }
        }
    }
    return ZR_TRUE;
}

TZrBool zr_fusion_state_map_is_valid(const SZrExecIrFunction *function) {
    const SZrExecIrStateMap *map;
    TZrUInt32 index;
    if (function == ZR_NULL || function->stateMap == ZR_NULL) {
        return ZR_TRUE;
    }
    map = function->stateMap;
    if ((map->functionToken != 0u && function->functionToken != 0u &&
         map->functionToken != function->functionToken) ||
        (map->signatureHash != 0u && function->signatureHash != 0u &&
         map->signatureHash != function->signatureHash) ||
        !zr_fusion_pointer_count_valid(map->entries, map->entryCount,
                                       map->entryCapacity) ||
        !zr_fusion_pointer_count_valid(map->valuePool, map->valueCount,
                                       map->valueCapacity) ||
        !zr_fusion_pointer_count_valid(map->rootPool, map->rootCount,
                                       map->rootCapacity) ||
        !zr_fusion_pointer_count_valid(map->ownerStatePool,
                                       map->ownerStateCount,
                                       map->ownerStateCapacity)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < map->entryCount; ++index) {
        const SZrExecIrStateMapEntry *entry = &map->entries[index];
        if ((entry->instructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
             entry->instructionId > function->instructionCount) ||
            (TZrUInt32)entry->phase >= ZR_EXEC_IR_STATE_PHASE_COUNT ||
            (entry->boundaryFlags &
             ~ZR_EXEC_IR_STATE_MAP_BOUNDARY_KNOWN_MASK) != 0u ||
            !zr_fusion_range_valid(entry->liveValues, map->valueCount) ||
            !zr_fusion_range_valid(entry->rootValues, map->rootCount) ||
            !zr_fusion_range_valid(entry->ownerStates,
                                   map->ownerStateCount) ||
            entry->ownerStates.count != entry->liveValues.count ||
            (entry->handlerBlockId != ZR_EXEC_IR_BLOCK_ID_INVALID &&
             entry->handlerBlockId > function->blockCount)) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < map->valueCount; ++index) {
        if (map->valuePool[index] == ZR_EXEC_IR_VALUE_ID_INVALID ||
            map->valuePool[index] > function->valueCount) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < map->rootCount; ++index) {
        if (map->rootPool[index] == ZR_EXEC_IR_VALUE_ID_INVALID ||
            map->rootPool[index] > function->valueCount) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < map->ownerStateCount; ++index) {
        if (map->ownerStatePool[index] >=
            ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool zr_fusion_function_storage_is_valid(const SZrExecIrFunction *function) {
    TZrUInt32 index;
    if (function == ZR_NULL || function->sealed > ZR_TRUE ||
        !zr_fusion_gc_map_is_valid(function) ||
        !zr_fusion_state_map_is_valid(function) ||
        !zr_fusion_pointer_count_valid(function->instructions,
                                       function->instructionCount,
                                       function->instructionCapacity) ||
        !zr_fusion_pointer_count_valid(function->values,
                                       function->valueCount,
                                       function->valueCapacity) ||
        !zr_fusion_pointer_count_valid(function->operands,
                                       function->operandCount,
                                       function->operandCapacity) ||
        !zr_fusion_pointer_count_valid(function->results,
                                       function->resultCount,
                                       function->resultCapacity) ||
        !zr_fusion_pointer_count_valid(function->blocks,
                                       function->blockCount,
                                       function->blockCapacity) ||
        !zr_fusion_pointer_count_valid(function->successors,
                                       function->successorCount,
                                       function->successorCapacity) ||
        !zr_fusion_pointer_count_valid(function->predecessors,
                                       function->predecessorCount,
                                       function->predecessorCapacity) ||
        !zr_fusion_pointer_count_valid(function->phiPool,
                                       function->phiCount,
                                       function->phiCapacity) ||
        !zr_fusion_pointer_count_valid(function->phiIncoming,
                                       function->phiIncomingCount,
                                       function->phiIncomingCapacity) ||
        !zr_fusion_pointer_count_valid(function->memoryTokenPool,
                                       function->memoryTokenCount,
                                       function->memoryTokenCapacity) ||
        !zr_fusion_pointer_count_valid(function->gcRoots,
                                       function->gcRootCount,
                                       function->gcRootCapacity) ||
        !zr_fusion_pointer_count_valid(function->deoptStates,
                                       function->deoptStateCount,
                                       function->deoptStateCapacity) ||
        !zr_fusion_pointer_count_valid(function->deoptValues,
                                       function->deoptValueCount,
                                       function->deoptValueCapacity) ||
        !zr_fusion_pointer_count_valid(function->sourceMaps,
                                       function->sourceMapCount,
                                       function->sourceMapCapacity)) {
        return ZR_FALSE;
    }
    if (function->entryBlockId != ZR_EXEC_IR_BLOCK_ID_INVALID &&
        function->entryBlockId > function->blockCount) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        if (ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode) == ZR_NULL ||
            (instruction->operands.count != ZR_EXEC_IR_VARIADIC &&
             !zr_fusion_range_valid(instruction->operands,
                                    function->operandCount)) ||
            (instruction->operands.count == ZR_EXEC_IR_VARIADIC &&
             instruction->operands.start > function->operandCount) ||
            (instruction->results.count != ZR_EXEC_IR_VARIADIC &&
             !zr_fusion_range_valid(instruction->results,
                                    function->resultCount)) ||
            (instruction->results.count == ZR_EXEC_IR_VARIADIC &&
             instruction->results.start > function->resultCount) ||
            !zr_fusion_range_valid(instruction->successorRange,
                                   function->successorCount) ||
            !zr_fusion_range_valid(instruction->phiRange, function->phiCount) ||
            !zr_fusion_range_valid(instruction->memoryIn,
                                   function->memoryTokenCount) ||
            !zr_fusion_range_valid(instruction->memoryOut,
                                   function->memoryTokenCount)) {
            return ZR_FALSE;
        }
        /* ZR_EXEC_IR_VARIADIC is a legal producer-side arity marker.  Fusion
         * cannot enumerate such an instruction into its bounded side table,
         * but it must retain the function as a valid input so the caller can
         * take the unfused path. */
        if (instruction->operands.count != ZR_EXEC_IR_VARIADIC) {
            for (TZrUInt32 operand = 0u;
                 operand < instruction->operands.count; ++operand) {
                TZrExecIrValueId value = function->operands[
                        instruction->operands.start + operand];
                if (value == ZR_EXEC_IR_VALUE_ID_INVALID ||
                    value > function->valueCount) {
                    return ZR_FALSE;
                }
            }
        }
        if (instruction->results.count != ZR_EXEC_IR_VARIADIC) {
            for (TZrUInt32 result = 0u;
                 result < instruction->results.count; ++result) {
                TZrExecIrValueId value = function->results[
                        instruction->results.start + result];
                if (value == ZR_EXEC_IR_VALUE_ID_INVALID ||
                    value > function->valueCount) {
                    return ZR_FALSE;
                }
            }
        }
    }
    if (function->blockCount != 0u) {
        for (index = 0u; index < function->blockCount; ++index) {
            const SZrExecIrBlock *block = &function->blocks[index];
            if (block->id != index + 1u ||
                !zr_fusion_range_valid(block->instructionRange,
                                       function->instructionCount) ||
                !zr_fusion_range_valid(block->predecessorRange,
                                       function->predecessorCount) ||
                !zr_fusion_range_valid(block->successorRange,
                                       function->successorCount) ||
                !zr_fusion_range_valid(block->phis, function->phiCount)) {
                return ZR_FALSE;
            }
            if (block->terminatorInstructionId !=
                        ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                (block->instructionRange.count == 0u ||
                 block->terminatorInstructionId <=
                         block->instructionRange.start ||
                 block->terminatorInstructionId -
                                 block->instructionRange.start >
                         block->instructionRange.count)) {
                return ZR_FALSE;
            }
        }
    }
    for (index = 0u; index < function->successorCount; ++index) {
        if (function->successors[index] == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            function->successors[index] > function->blockCount) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->gcRootCount; ++index) {
        if (function->gcRoots[index] == ZR_EXEC_IR_VALUE_ID_INVALID ||
            function->gcRoots[index] > function->valueCount) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->memoryTokenCount; ++index) {
        if (function->memoryTokenPool[index] ==
            ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        if (!zr_fusion_range_valid(state->reconstruction,
                                   function->deoptValueCount)) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->deoptValueCount; ++index) {
        if (function->deoptValues[index] == ZR_EXEC_IR_VALUE_ID_INVALID ||
            function->deoptValues[index] > function->valueCount) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->sourceMapCount; ++index) {
        const SZrExecIrSourceMap *map = &function->sourceMaps[index];
        if (map->instructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            map->instructionId > function->instructionCount) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->predecessorCount; ++index) {
        if (function->predecessors[index] == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            function->predecessors[index] > function->blockCount) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->phiCount; ++index) {
        const SZrExecIrPhi *phi = &function->phiPool[index];
        TZrUInt32 incoming;
        if (phi->result == ZR_EXEC_IR_VALUE_ID_INVALID ||
            phi->result > function->valueCount ||
            !zr_fusion_range_valid(phi->incomings,
                                   function->phiIncomingCount)) {
            return ZR_FALSE;
        }
        for (incoming = 0u; incoming < phi->incomings.count; ++incoming) {
            const SZrExecIrPhiIncoming *item = &function->phiIncoming[
                    phi->incomings.start + incoming];
            if (item->predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
                item->predecessor > function->blockCount ||
                item->value == ZR_EXEC_IR_VALUE_ID_INVALID ||
                item->value > function->valueCount) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrUInt64 ZrParser_ExecBcFusion_InputHash(const SZrExecIrFunction *function) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 index;
    if (!zr_fusion_function_storage_is_valid(function)) {
        return 0u;
    }
    hash = zr_fusion_hash_u32(hash, function->id);
    hash = zr_fusion_hash_u32(hash, function->functionToken);
    hash = zr_fusion_hash_u64(hash, function->signatureHash);
    hash = zr_fusion_hash_u32(hash, function->contract.schemaVersion);
    hash = zr_fusion_hash_u32(hash, function->contract.abiVersion);
    hash = zr_fusion_hash_u32(hash, function->contract.logicalVersion);
    hash = zr_fusion_hash_u64(hash, function->contract.generation);
    hash = zr_fusion_hash_u32(hash, function->contract.targetToken);
    hash = zr_fusion_hash_u64(hash, function->contract.signatureHash);
    hash = zr_fusion_hash_u64(hash, function->contract.layoutHash);
    hash = zr_fusion_hash_u64(hash, function->contract.moduleHash);
    hash = zr_fusion_hash_u32(hash, function->contract.requiredCapabilities);
    hash = zr_fusion_hash_u32(hash, function->contract.declaredEffects);
    hash = zr_fusion_hash_u32(hash, function->entryBlockId);
    hash = zr_fusion_hash_u32(hash, function->instructionCount);
    hash = zr_fusion_hash_u32(hash, function->valueCount);
    hash = zr_fusion_hash_u32(hash, function->operandCount);
    hash = zr_fusion_hash_u32(hash, function->resultCount);
    hash = zr_fusion_hash_u32(hash, function->blockCount);
    hash = zr_fusion_hash_u32(hash, function->successorCount);
    hash = zr_fusion_hash_u32(hash, function->predecessorCount);
    for (index = 0u; index < function->valueCount; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        hash = zr_fusion_hash_u32(hash, value->id);
        hash = zr_fusion_hash_u32(hash, value->definition);
        hash = zr_fusion_hash_u32(hash, value->typeToken);
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)value->ownership);
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)value->nullability);
        hash = zr_fusion_hash_u32(hash, value->flags);
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        hash = zr_fusion_hash_instruction(hash, &function->instructions[index]);
    }
    for (index = 0u; index < function->operandCount; ++index) {
        hash = zr_fusion_hash_u32(hash, function->operands[index]);
    }
    for (index = 0u; index < function->resultCount; ++index) {
        hash = zr_fusion_hash_u32(hash, function->results[index]);
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        hash = zr_fusion_hash_u32(hash, block->id);
        hash = zr_fusion_hash_u32(hash, block->flags);
        hash = zr_fusion_hash_u32(hash, block->instructionRange.start);
        hash = zr_fusion_hash_u32(hash, block->instructionRange.count);
        hash = zr_fusion_hash_u32(hash, block->predecessorRange.start);
        hash = zr_fusion_hash_u32(hash, block->predecessorRange.count);
        hash = zr_fusion_hash_u32(hash, block->successorRange.start);
        hash = zr_fusion_hash_u32(hash, block->successorRange.count);
        hash = zr_fusion_hash_u32(hash, block->phis.start);
        hash = zr_fusion_hash_u32(hash, block->phis.count);
        hash = zr_fusion_hash_u32(hash, block->terminatorInstructionId);
    }
    for (index = 0u; index < function->successorCount; ++index) {
        hash = zr_fusion_hash_u32(hash, function->successors[index]);
    }
    for (index = 0u; index < function->predecessorCount; ++index) {
        hash = zr_fusion_hash_u32(hash, function->predecessors[index]);
    }
    for (index = 0u; index < function->memoryTokenCount; ++index) {
        hash = zr_fusion_hash_u32(hash, function->memoryTokenPool[index]);
    }
    if (function->gcMap != ZR_NULL) {
        const SZrExecIrGcMap *map = function->gcMap;
        hash = zr_fusion_hash_u32(hash, 1u);
        hash = zr_fusion_hash_u32(hash, map->entryCount);
        hash = zr_fusion_hash_u32(hash, map->slotIndexCount);
        hash = zr_fusion_hash_u32(hash, map->inlineRefOffsetCount);
        hash = zr_fusion_hash_u32(hash, map->safepointId);
        hash = zr_fusion_hash_u32(hash, map->sourceId);
        hash = zr_fusion_hash_u32(hash, map->rootRange.start);
        hash = zr_fusion_hash_u32(hash, map->rootRange.count);
        for (TZrUInt32 mapIndex = 0u; mapIndex < map->entryCount;
             ++mapIndex) {
            const SZrExecIrGcMapEntry *entry = &map->entries[mapIndex];
            hash = zr_fusion_hash_u32(hash, entry->site);
            hash = zr_fusion_hash_u32(hash, entry->liveRefSlots.start);
            hash = zr_fusion_hash_u32(hash, entry->liveRefSlots.count);
            hash = zr_fusion_hash_u32(hash, entry->inlineRefOffsets.start);
            hash = zr_fusion_hash_u32(hash, entry->inlineRefOffsets.count);
        }
        for (TZrUInt32 mapIndex = 0u; mapIndex < map->slotIndexCount;
             ++mapIndex) {
            hash = zr_fusion_hash_u32(hash, map->slotIndexPool[mapIndex]);
        }
        for (TZrUInt32 mapIndex = 0u; mapIndex < map->inlineRefOffsetCount;
             ++mapIndex) {
            hash = zr_fusion_hash_u32(hash, map->inlineRefOffsetPool[mapIndex]);
        }
    } else {
        hash = zr_fusion_hash_u32(hash, 0u);
    }
    for (index = 0u; index < function->gcRootCount; ++index) {
        hash = zr_fusion_hash_u32(hash, function->gcRoots[index]);
    }
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        hash = zr_fusion_hash_u32(hash, state->id);
        hash = zr_fusion_hash_u32(hash, state->source);
        hash = zr_fusion_hash_u32(hash, state->resumeId);
        hash = zr_fusion_hash_u32(hash, state->reconstruction.start);
        hash = zr_fusion_hash_u32(hash, state->reconstruction.count);
        hash = zr_fusion_hash_u32(hash, state->cleanupState);
    }
    for (index = 0u; index < function->deoptValueCount; ++index) {
        hash = zr_fusion_hash_u32(hash, function->deoptValues[index]);
    }
    for (index = 0u; index < function->sourceMapCount; ++index) {
        const SZrExecIrSourceMap *map = &function->sourceMaps[index];
        hash = zr_fusion_hash_u32(hash, map->sourceId);
        hash = zr_fusion_hash_u32(hash, map->instructionId);
        hash = zr_fusion_hash_u32(hash, map->startOffset);
        hash = zr_fusion_hash_u32(hash, map->endOffset);
        hash = zr_fusion_hash_u32(hash, map->startLine);
        hash = zr_fusion_hash_u32(hash, map->startColumn);
        hash = zr_fusion_hash_u32(hash, map->endLine);
        hash = zr_fusion_hash_u32(hash, map->endColumn);
    }
    if (function->stateMap != ZR_NULL) {
        const SZrExecIrStateMap *map = function->stateMap;
        hash = zr_fusion_hash_u32(hash, 1u);
        hash = zr_fusion_hash_u32(hash, map->functionToken);
        hash = zr_fusion_hash_u64(hash, map->signatureHash);
        hash = zr_fusion_hash_u64(hash, map->generation);
        hash = zr_fusion_hash_u32(hash, map->entryCount);
        hash = zr_fusion_hash_u32(hash, map->valueCount);
        hash = zr_fusion_hash_u32(hash, map->rootCount);
        hash = zr_fusion_hash_u32(hash, map->ownerStateCount);
        for (TZrUInt32 mapIndex = 0u; mapIndex < map->entryCount;
             ++mapIndex) {
            const SZrExecIrStateMapEntry *entry = &map->entries[mapIndex];
            hash = zr_fusion_hash_u32(hash, entry->sourceId);
            hash = zr_fusion_hash_u32(hash, entry->instructionId);
            hash = zr_fusion_hash_u32(hash, entry->deoptId);
            hash = zr_fusion_hash_u32(hash, entry->resumeId);
            hash = zr_fusion_hash_u32(hash, entry->cleanupState);
            hash = zr_fusion_hash_u32(hash, entry->boundaryFlags);
            hash = zr_fusion_hash_u32(hash, (TZrUInt32)entry->phase);
            hash = zr_fusion_hash_u32(hash, entry->liveValues.start);
            hash = zr_fusion_hash_u32(hash, entry->liveValues.count);
            hash = zr_fusion_hash_u32(hash, entry->rootValues.start);
            hash = zr_fusion_hash_u32(hash, entry->rootValues.count);
            hash = zr_fusion_hash_u32(hash, entry->ownerStates.start);
            hash = zr_fusion_hash_u32(hash, entry->ownerStates.count);
            hash = zr_fusion_hash_u32(hash, entry->effectIn);
            hash = zr_fusion_hash_u32(hash, entry->effectOut);
            hash = zr_fusion_hash_u32(hash, entry->handlerBlockId);
            hash = zr_fusion_hash_u32(hash, entry->exceptionState);
        }
        for (TZrUInt32 mapIndex = 0u; mapIndex < map->valueCount;
             ++mapIndex) {
            hash = zr_fusion_hash_u32(hash, map->valuePool[mapIndex]);
        }
        for (TZrUInt32 mapIndex = 0u; mapIndex < map->rootCount;
             ++mapIndex) {
            hash = zr_fusion_hash_u32(hash, map->rootPool[mapIndex]);
        }
        for (TZrUInt32 mapIndex = 0u; mapIndex < map->ownerStateCount;
             ++mapIndex) {
            hash = zr_fusion_hash_u32(hash, map->ownerStatePool[mapIndex]);
        }
    } else {
        hash = zr_fusion_hash_u32(hash, 0u);
    }
    for (index = 0u; index < function->phiCount; ++index) {
        hash = zr_fusion_hash_u32(hash, function->phiPool[index].result);
        hash = zr_fusion_hash_u32(hash, function->phiPool[index].incomings.start);
        hash = zr_fusion_hash_u32(hash, function->phiPool[index].incomings.count);
    }
    for (index = 0u; index < function->phiIncomingCount; ++index) {
        hash = zr_fusion_hash_u32(hash, function->phiIncoming[index].predecessor);
        hash = zr_fusion_hash_u32(hash, function->phiIncoming[index].value);
    }
    hash = zr_fusion_hash_u32(hash, function->sealed);
    hash = zr_fusion_hash_u64(hash, ZrParser_ExecBcFusion_PatternSchemaHash());
    return hash == 0u ? UINT64_C(1) : hash;
}


static TZrUInt64 zr_fusion_hash_plan_records(const SZrExecBcFusionPlan *plan) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 index;
    if (plan == ZR_NULL) {
        return 0u;
    }
    hash = zr_fusion_hash_u32(hash, plan->schemaVersion);
    hash = zr_fusion_hash_u32(hash, plan->functionToken);
    hash = zr_fusion_hash_u64(hash, plan->generation);
    hash = zr_fusion_hash_u64(hash, plan->signatureHash);
    hash = zr_fusion_hash_u64(hash, plan->moduleHash);
    hash = zr_fusion_hash_u64(hash, plan->layoutHash);
    hash = zr_fusion_hash_u64(hash, plan->inputHash);
    hash = zr_fusion_hash_u64(hash, plan->patternSchemaHash);
    /* Lifecycle state is part of the publication contract.  In particular,
     * an invalidated empty plan has no record-count delta to distinguish it
     * from its original form, so hash valid/reason explicitly. */
    hash = zr_fusion_hash_u32(hash, plan->valid);
    hash = zr_fusion_hash_u32(hash, (TZrUInt32)plan->invalidationReason);
    hash = zr_fusion_hash_u32(hash, plan->originalInstructionCount);
    hash = zr_fusion_hash_u32(hash, plan->instructionCount);
    hash = zr_fusion_hash_u32(hash, plan->fusedCount);
    hash = zr_fusion_hash_u32(hash, plan->dispatchesSaved);
    hash = zr_fusion_hash_u32(hash, plan->estimatedCodeBytes);
    hash = zr_fusion_hash_u32(hash, plan->codeBudgetBytes);
    for (index = 0u; index < plan->instructionCount; ++index) {
        const SZrExecBcFusionInstruction *instruction =
                &plan->instructions[index];
        hash = zr_fusion_hash_u32(hash, instruction->operationCode);
        hash = zr_fusion_hash_u32(hash, instruction->operandExtra);
        hash = zr_fusion_hash_u32(hash,
                                  (TZrUInt32)instruction->operand.operand2[0]);
    }
    for (index = 0u; index < plan->sideEntryCount; ++index) {
        const SZrExecBcFusionSideEntry *entry = &plan->sideEntries[index];
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)entry->pattern);
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)entry->headOpcode);
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)entry->tailOpcode);
        hash = zr_fusion_hash_u32(hash, entry->headInstructionId);
        hash = zr_fusion_hash_u32(hash, entry->tailInstructionId);
        hash = zr_fusion_hash_u32(hash, entry->headSourceId);
        hash = zr_fusion_hash_u32(hash, entry->tailSourceId);
        hash = zr_fusion_hash_u32(hash, entry->headResumeId);
        hash = zr_fusion_hash_u32(hash, entry->tailResumeId);
        hash = zr_fusion_hash_u32(hash, entry->headResult);
        hash = zr_fusion_hash_u32(hash, entry->tailResult);
        hash = zr_fusion_hash_u32(hash, entry->branchTarget);
        hash = zr_fusion_hash_u32(hash, entry->branchTargetPc);
        hash = zr_fusion_hash_u32(hash, entry->branchTargetCount);
        for (TZrUInt32 branch = 0u;
             branch < ZR_EXEC_BC_FUSION_MAX_BRANCH_TARGETS; ++branch) {
            hash = zr_fusion_hash_u32(hash, entry->branchTargets[branch]);
            hash = zr_fusion_hash_u32(hash, entry->branchTargetPcs[branch]);
        }
        hash = zr_fusion_hash_u32(hash, entry->bindingRow);
        hash = zr_fusion_hash_u32(hash, entry->operandCount);
        hash = zr_fusion_hash_u32(hash, entry->headOperandCount);
        hash = zr_fusion_hash_u32(hash, entry->tailOperandCount);
        for (TZrUInt32 operand = 0u;
             operand < ZR_EXEC_BC_FUSION_MAX_OPERANDS; ++operand) {
            hash = zr_fusion_hash_u32(hash, entry->operands[operand]);
        }
        hash = zr_fusion_hash_u32(hash, entry->guardMask);
        hash = zr_fusion_hash_u64(hash, entry->generation);
        hash = zr_fusion_hash_u64(hash, entry->signatureHash);
        hash = zr_fusion_hash_u64(hash, entry->moduleHash);
        hash = zr_fusion_hash_u64(hash, entry->layoutHash);
    }
    for (index = 0u; index < plan->sourceMapCount; ++index) {
        const SZrExecBcFusionSourceMap *map = &plan->sourceMaps[index];
        hash = zr_fusion_hash_u32(hash, map->fusedPc);
        hash = zr_fusion_hash_u32(hash, map->originalInstructionId);
        hash = zr_fusion_hash_u32(hash, map->sourceId);
        hash = zr_fusion_hash_u32(hash, map->resumeId);
        hash = zr_fusion_hash_u32(hash, map->ordinal);
        hash = zr_fusion_hash_u32(hash, map->boundaryMask);
    }
    for (index = 0u; index < plan->fallbackCount; ++index) {
        const SZrExecBcFusionFallback *fallback = &plan->fallbacks[index];
        hash = zr_fusion_hash_u32(hash, fallback->headInstructionId);
        hash = zr_fusion_hash_u32(hash, fallback->tailInstructionId);
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)fallback->pattern);
        hash = zr_fusion_hash_u32(hash, (TZrUInt32)fallback->reason);
        hash = zr_fusion_hash_u32(hash, fallback->sourceId);
        hash = zr_fusion_hash_u32(hash, fallback->expected);
        hash = zr_fusion_hash_u32(hash, fallback->actual);
    }
    return hash == 0u ? UINT64_C(1) : hash;
}

TZrUInt64 ZrParser_ExecBcFusion_Hash(const SZrExecBcFusionPlan *plan) {
    if (!zr_fusion_plan_storage_is_valid(plan)) {
        return 0u;
    }
    return zr_fusion_hash_plan_records(plan);
}
