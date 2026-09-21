#include "zr_vm_parser/exec_ir_state_maps.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef enum EZrStateMapBuildResult {
    ZR_STATE_MAP_BUILD_OK = 0,
    ZR_STATE_MAP_BUILD_OUT_OF_MEMORY,
    ZR_STATE_MAP_BUILD_BORROWED,
    ZR_STATE_MAP_BUILD_INVALID
} EZrStateMapBuildResult;

static TZrBool zr_state_map_allocation_size_valid(TZrUInt32 count,
                                                  size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     (TZrUInt64)count <= (TZrUInt64)(SIZE_MAX / elementSize));
}

static TZrBool zr_state_map_phase_valid(EZrExecIrStateMapPhase phase) {
    switch (phase) {
        case ZR_EXEC_IR_STATE_BEFORE_EFFECT:
        case ZR_EXEC_IR_STATE_AFTER_EFFECT:
        case ZR_EXEC_IR_STATE_CLEANUP_COMPLETE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void zr_state_map_clear_diagnostic(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static void zr_state_map_set_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                        EZrExecutionDiagnosticCode code,
                                        const SZrExecIrFunction *function,
                                        TZrUInt32 instructionId,
                                        TZrUInt32 sourceId) {
    TZrUInt32 blockIndex;

    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = sourceId;
    diagnostic->blockId = ZR_EXEC_IR_BLOCK_ID_INVALID;
    if (function == ZR_NULL || instructionId == 0u || function->blocks == ZR_NULL) {
        return;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 instructionIndex = instructionId - 1u;
        if (instructionIndex >= block->instructionRange.start &&
            instructionIndex - block->instructionRange.start < block->instructionRange.count) {
            diagnostic->blockId = block->id;
            return;
        }
    }
}

static TZrBool zr_state_map_reserve(void **storage,
                                    TZrUInt32 *capacity,
                                    TZrUInt32 count,
                                    TZrUInt32 needed,
                                    size_t elementSize) {
    TZrUInt32 next;
    void *memory;

    if (storage == ZR_NULL || capacity == ZR_NULL || elementSize == 0u ||
        needed < count || (size_t)needed > SIZE_MAX / elementSize) {
        return ZR_FALSE;
    }
    if (needed <= *capacity) {
        return ZR_TRUE;
    }
    next = *capacity == 0u ? 4u : *capacity;
    while (next < needed) {
        if (next > UINT32_MAX / 2u) {
            next = needed;
            break;
        }
        next *= 2u;
    }
    if ((size_t)next > SIZE_MAX / elementSize) {
        return ZR_FALSE;
    }
    memory = realloc(*storage, (size_t)next * elementSize);
    if (memory == ZR_NULL) {
        return ZR_FALSE;
    }
    *storage = memory;
    *capacity = next;
    return ZR_TRUE;
}

static TZrBool zr_state_map_append_values(SZrExecIrStateMap *map,
                                          const TZrExecIrValueId *values,
                                          TZrUInt32 count,
                                          SZrExecIrRange *range,
                                          TZrBool roots) {
    TZrExecIrValueId **storage = roots ? &map->rootPool : &map->valuePool;
    TZrUInt32 *used = roots ? &map->rootCount : &map->valueCount;
    TZrUInt32 *capacity = roots ? &map->rootCapacity : &map->valueCapacity;
    TZrUInt32 old;

    if (map == ZR_NULL || (count != 0u && values == ZR_NULL) ||
        count > UINT32_MAX - *used) {
        return ZR_FALSE;
    }
    old = *used;
    if (!zr_state_map_reserve((void **)storage, capacity, old, old + count,
                              sizeof(**storage))) {
        return ZR_FALSE;
    }
    if (count != 0u) {
        memcpy(*storage + old, values, (size_t)count * sizeof(**storage));
    }
    *used = old + count;
    if (range != ZR_NULL) {
        range->start = old;
        range->count = count;
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_append_owner_states(SZrExecIrStateMap *map,
                                                 const TZrUInt32 *states,
                                                 TZrUInt32 count,
                                                 SZrExecIrRange *range) {
    TZrUInt32 old;

    if (map == ZR_NULL || (count != 0u && states == ZR_NULL) ||
        count > UINT32_MAX - map->ownerStateCount) {
        return ZR_FALSE;
    }
    old = map->ownerStateCount;
    if (!zr_state_map_reserve((void **)&map->ownerStatePool,
                              &map->ownerStateCapacity, old, old + count,
                              sizeof(*map->ownerStatePool))) {
        return ZR_FALSE;
    }
    if (count != 0u) {
        memcpy(map->ownerStatePool + old, states,
               (size_t)count * sizeof(*map->ownerStatePool));
    }
    map->ownerStateCount = old + count;
    if (range != ZR_NULL) {
        range->start = old;
        range->count = count;
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_append_entry(SZrExecIrStateMap *map,
                                         const SZrExecIrStateMapEntry *entry) {
    if (map == ZR_NULL || entry == ZR_NULL || map->entryCount == UINT32_MAX ||
        !zr_state_map_reserve((void **)&map->entries, &map->entryCapacity,
                              map->entryCount, map->entryCount + 1u,
                              sizeof(*map->entries))) {
        return ZR_FALSE;
    }
    map->entries[map->entryCount++] = *entry;
    return ZR_TRUE;
}

static TZrBool zr_state_map_is_boundary(const SZrExecIrInstruction *instruction,
                                        const SZrExecIrOpcodeInfo *info,
                                        TZrUInt32 *flags) {
    TZrUInt32 boundary = 0u;

    if (instruction == ZR_NULL || info == ZR_NULL || flags == ZR_NULL) {
        return ZR_FALSE;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_ALLOCATE) != 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE) != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_ALLOCATE;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_GC) != 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC) != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_SUSPEND) != 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND) != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_DEBUG_POLL) != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEBUG_POLL;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_GUARD_EXIT) != 0u ||
        instruction->deoptId != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT;
    }
    if (instruction->deoptId != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT;
    }
    if ((EZrExecIrOpcode)instruction->opcode == ZR_EXEC_IR_OPCODE_DROP) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP;
    }
    *flags = boundary;
    return (TZrBool)(boundary != 0u);
}

static TZrBool zr_state_map_value_is_used(const SZrExecIrFunction *function,
                                          TZrExecIrValueId valueId,
                                          TZrUInt32 firstInstructionId) {
    TZrUInt32 instructionIndex;

    /* Instruction IDs are one-based while ranges are zero-based.  Iterating
     * over the index keeps the UINT32_MAX case from wrapping the loop back to
     * zero and accidentally indexing before the instruction pool. */
    if (firstInstructionId == 0u) {
        firstInstructionId = 1u;
    }
    for (instructionIndex = firstInstructionId - 1u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction = &function->instructions[instructionIndex];
        TZrUInt32 index;
        for (index = instruction->operandRange.start;
             index < instruction->operandRange.start + instruction->operandRange.count;
             ++index) {
            if (function->operandPool[index] == valueId) {
                return ZR_TRUE;
            }
        }
    }
    /* Phi inputs are edge uses and may be reached without a later linear use. */
    for (instructionIndex = 0u;
         instructionIndex < function->phiIncomingCount;
         ++instructionIndex) {
        if (function->phiIncoming[instructionIndex].value == valueId) {
            return ZR_TRUE;
        }
    }
    for (instructionIndex = 0u;
         instructionIndex < function->deoptStateCount;
         ++instructionIndex) {
        const SZrExecIrDeoptState *state = &function->deoptStates[instructionIndex];
        TZrUInt32 index;
        for (index = state->valueRange.start;
             index < state->valueRange.start + state->valueRange.count;
             ++index) {
            if (function->deoptValues[index] == valueId) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static EZrExecIrStateMapOwnerState zr_state_map_owner_state_at(
        const SZrExecIrFunction *function,
        TZrExecIrValueId valueId,
        TZrUInt32 instructionId,
        TZrBool includeCurrent) {
    EZrExecIrStateMapOwnerState state = ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
    TZrUInt32 end;
    TZrUInt32 instructionIndex;

    if (function->values[valueId - 1u].ownership == ZR_EXEC_IR_OWNERSHIP_UNKNOWN) {
        state = ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN;
    }
    if (instructionId == 0u) {
        return state;
    }
    end = includeCurrent ? instructionId : instructionId - 1u;
    if (end == 0u) {
        return state;
    }
    if (end > function->instructionCount) {
        end = function->instructionCount;
    }
    for (instructionIndex = 0u;
         instructionIndex < end;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction = &function->instructions[instructionIndex];
        TZrUInt32 index;
        if ((EZrExecIrOpcode)instruction->opcode != ZR_EXEC_IR_OPCODE_DROP &&
            (EZrExecIrOpcode)instruction->opcode != ZR_EXEC_IR_OPCODE_MOVE) {
            continue;
        }
        for (index = instruction->operandRange.start;
             index < instruction->operandRange.start + instruction->operandRange.count;
             ++index) {
            if (function->operandPool[index] == valueId) {
                state = (EZrExecIrOpcode)instruction->opcode == ZR_EXEC_IR_OPCODE_DROP
                            ? ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED
                            : ZR_EXEC_IR_STATE_MAP_OWNER_MOVED;
            }
        }
    }
    return state;
}

static TZrExecIrBlockId zr_state_map_handler_block(const SZrExecIrFunction *function,
                                                   TZrUInt32 instructionId,
                                                   TZrUInt32 boundaryFlags) {
    TZrUInt32 blockIndex;

    if (function == ZR_NULL || instructionId == 0u ||
        instructionId > function->instructionCount ||
        (boundaryFlags & ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW) == 0u) {
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 instructionIndex = instructionId - 1u;
        TZrUInt32 successorIndex;
        if (instructionIndex < block->instructionRange.start ||
            instructionIndex - block->instructionRange.start >= block->instructionRange.count) {
            continue;
        }
        for (successorIndex = block->successorRange.start;
             successorIndex < block->successorRange.start + block->successorRange.count;
             ++successorIndex) {
            TZrExecIrBlockId successor = function->successors[successorIndex];
            const SZrExecIrBlock *successorBlock =
                ZrCore_ExecIr_FunctionBlockAtConst(function, successor);
            if (successorBlock != ZR_NULL &&
                (successorBlock->flags & (ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION |
                                          ZR_EXEC_IR_BLOCK_FLAG_CLEANUP)) != 0u) {
                return successor;
            }
        }
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static EZrStateMapBuildResult zr_state_map_add_checkpoint(
        SZrExecIrStateMap *map,
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction,
        TZrUInt32 instructionId,
        TZrUInt32 boundaryFlags,
        EZrExecIrStateMapPhase phase,
        TZrUInt32 resumeId,
        SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrValueId *live = ZR_NULL;
    TZrExecIrValueId *roots = ZR_NULL;
    TZrUInt32 *owners = ZR_NULL;
    TZrUInt32 liveCount = 0u;
    TZrUInt32 rootCount = 0u;
    TZrUInt32 ownerCount = 0u;
    TZrUInt32 valueIndex;
    TZrBool includeCurrent = (TZrBool)(phase != ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    SZrExecIrStateMapEntry entry;

    if (map == ZR_NULL || function == ZR_NULL || instruction == ZR_NULL ||
        instructionId == 0u || resumeId == 0u || !zr_state_map_phase_valid(phase)) {
        return ZR_STATE_MAP_BUILD_INVALID;
    }
    if (function->valueCount != 0u) {
        if (!zr_state_map_allocation_size_valid(function->valueCount, sizeof(*live)) ||
            !zr_state_map_allocation_size_valid(function->valueCount, sizeof(*owners))) {
            return ZR_STATE_MAP_BUILD_INVALID;
        }
        live = (TZrExecIrValueId *)malloc((size_t)function->valueCount * sizeof(*live));
        roots = (TZrExecIrValueId *)malloc((size_t)function->valueCount * sizeof(*roots));
        owners = (TZrUInt32 *)malloc((size_t)function->valueCount * sizeof(*owners));
        if (live == ZR_NULL || roots == ZR_NULL || owners == ZR_NULL) {
            free(live); free(roots); free(owners);
            return ZR_STATE_MAP_BUILD_OUT_OF_MEMORY;
        }
    }
    for (valueIndex = 0u; valueIndex < function->valueCount; ++valueIndex) {
        const SZrExecIrValue *value = &function->values[valueIndex];
        TZrUInt32 definition = value->definition;
        TZrUInt32 firstUse = instructionId;
        EZrExecIrStateMapOwnerState ownerState;
        if (includeCurrent && instructionId != UINT32_MAX) {
            /* Once the boundary has committed, the current instruction's
             * operands are consumed.  Keep only later uses (plus edge/deopt
             * uses discovered by the conservative side scans). */
            firstUse = instructionId + 1u;
        }
        if (definition != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
            (includeCurrent ? definition > instructionId : definition >= instructionId)) {
            continue;
        }
        if (!zr_state_map_value_is_used(function, value->id, firstUse)) {
            continue;
        }
        ownerState = zr_state_map_owner_state_at(
                function, value->id, instructionId, includeCurrent);
        if (includeCurrent &&
            (ownerState == ZR_EXEC_IR_STATE_MAP_OWNER_MOVED ||
             ownerState == ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED)) {
            /* A moved/dropped source is no longer a materializable live
             * value after the ownership effect has committed. */
            continue;
        }
        live[liveCount] = value->id;
        owners[ownerCount++] = (TZrUInt32)ownerState;
        ++liveCount;
        if (value->ownership == ZR_EXEC_IR_OWNERSHIP_BORROWED &&
            (boundaryFlags & ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND) != 0u) {
            free(live); free(roots); free(owners);
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND,
                                        function, instructionId, instruction->sourceId);
            return ZR_STATE_MAP_BUILD_BORROWED;
        }
        if (value->ownership == ZR_EXEC_IR_OWNERSHIP_GC ||
            value->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
            value->ownership == ZR_EXEC_IR_OWNERSHIP_SHARED) {
            roots[rootCount++] = value->id;
        }
    }
    memset(&entry, 0, sizeof(entry));
    entry.sourceId = instruction->sourceId != 0u ? instruction->sourceId : instructionId;
    entry.instructionId = instructionId;
    entry.deoptId = instruction->deoptId;
    entry.resumeId = resumeId;
    entry.cleanupState = phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT
                             ? 0u
                             : (phase == ZR_EXEC_IR_STATE_AFTER_EFFECT ? 1u : 2u);
    entry.boundaryFlags = boundaryFlags;
    entry.phase = phase;
    entry.effectIn = instruction->effectIn;
    entry.effectOut = instruction->effectOut;
    entry.handlerBlockId = zr_state_map_handler_block(function, instructionId,
                                                       boundaryFlags);
    entry.exceptionState = boundaryFlags & ZR_EXEC_IR_STATE_MAP_EXCEPTION_MASK;
    if (!zr_state_map_append_values(map, live, liveCount, &entry.liveValues, ZR_FALSE) ||
        !zr_state_map_append_values(map, roots, rootCount, &entry.rootValues, ZR_TRUE) ||
        !zr_state_map_append_owner_states(map, owners, ownerCount, &entry.ownerStates) ||
        !zr_state_map_append_entry(map, &entry)) {
        free(live); free(roots); free(owners);
        return ZR_STATE_MAP_BUILD_OUT_OF_MEMORY;
    }
    free(live); free(roots); free(owners);
    return ZR_STATE_MAP_BUILD_OK;
}

TZrBool ZrParser_ExecIr_BuildStateMaps(SZrExecIrFunction *function,
                                       SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrStateMap candidate;
    TZrUInt32 instructionId;
    TZrUInt32 resumeId = 1u;

    zr_state_map_clear_diagnostic(diagnostic);
    if (function == ZR_NULL) {
        zr_state_map_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                    ZR_NULL, 0u, 0u);
        return ZR_FALSE;
    }
    if (function->sealed) {
        zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_SEALED,
                                    function, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, diagnostic)) {
        return ZR_FALSE;
    }
    ZrCore_ExecIr_StateMapInit(&candidate);
    candidate.functionToken = function->functionToken;
    candidate.signatureHash = function->signatureHash;
    candidate.generation = function->contract.generation;
    for (instructionId = 1u; instructionId <= function->instructionCount; ++instructionId) {
        const SZrExecIrInstruction *instruction = &function->instructions[instructionId - 1u];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(
                (EZrExecIrOpcode)instruction->opcode);
        TZrUInt32 boundaryFlags = 0u;
        TZrUInt32 currentResume;
        EZrStateMapBuildResult result;

        if (!zr_state_map_is_boundary(instruction, info, &boundaryFlags)) {
            continue;
        }
        if (resumeId == UINT32_MAX) {
            ZrCore_ExecIr_StateMapFree(&candidate);
            zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                        function, instructionId, instruction->sourceId);
            return ZR_FALSE;
        }
        currentResume = resumeId++;
        result = zr_state_map_add_checkpoint(&candidate, function, instruction,
                                             instructionId, boundaryFlags,
                                             ZR_EXEC_IR_STATE_BEFORE_EFFECT,
                                             currentResume, diagnostic);
        if (result == ZR_STATE_MAP_BUILD_OK) {
            result = zr_state_map_add_checkpoint(&candidate, function, instruction,
                                                 instructionId, boundaryFlags,
                                                 ZR_EXEC_IR_STATE_AFTER_EFFECT,
                                                 currentResume, diagnostic);
        }
        if (result == ZR_STATE_MAP_BUILD_OK &&
            (boundaryFlags & (ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW |
                              ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND |
                              ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP)) != 0u) {
            result = zr_state_map_add_checkpoint(&candidate, function, instruction,
                                                 instructionId, boundaryFlags,
                                                 ZR_EXEC_IR_STATE_CLEANUP_COMPLETE,
                                                 currentResume, diagnostic);
        }
        if (result != ZR_STATE_MAP_BUILD_OK) {
            if (result == ZR_STATE_MAP_BUILD_OUT_OF_MEMORY) {
                zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                            function, instructionId, instruction->sourceId);
            } else if (result == ZR_STATE_MAP_BUILD_INVALID) {
                zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                            function, instructionId, instruction->sourceId);
            }
            ZrCore_ExecIr_StateMapFree(&candidate);
            return ZR_FALSE;
        }
    }
    {
        SZrExecIrStateMap *committed = (SZrExecIrStateMap *)malloc(sizeof(*committed));
        if (committed == ZR_NULL) {
            ZrCore_ExecIr_StateMapFree(&candidate);
            zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                        function, 0u, 0u);
            return ZR_FALSE;
        }
        *committed = candidate;
        if (function->stateMap != ZR_NULL) {
            ZrCore_ExecIr_StateMapFree(function->stateMap);
            free(function->stateMap);
        }
        function->stateMap = committed;
    }
    return ZR_TRUE;
}
