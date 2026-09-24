#include "zr_vm_parser/exec_ir_state_maps.h"

#include "zr_vm_core/exec_ir_state_map_liveness.h"
#include "zr_vm_core/exec_ir_owner_state.h"

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
        const SZrStateMapLiveness *liveness,
        const SZrExecIrOwnerAnalysis *ownership,
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
        EZrExecIrStateMapOwnerState ownerState;
        if (!ZrCore_ExecIr_StateMapLivenessContains(liveness, instructionId,
                                            value->id, includeCurrent)) {
            continue;
        }
        ownerState = ZrCore_ExecIr_StateMapOwnerAt(
                function, ownership, liveness, instructionId, value->id, phase);
        if (ownerState == ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT) {
            free(live); free(roots); free(owners);
            return ZR_STATE_MAP_BUILD_INVALID;
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
        if ((value->ownership == ZR_EXEC_IR_OWNERSHIP_GC ||
             value->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
             value->ownership == ZR_EXEC_IR_OWNERSHIP_SHARED) &&
            (ZrCore_ExecIr_StateMapOwnerMaskAt(function, ownership, liveness,
                    instructionId, value->id, phase) &
             ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED)) != 0u) {
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

static TZrBool zr_state_map_resume_used(const SZrExecIrStateMap *map,
                                        TZrUInt32 resumeId) {
    TZrUInt32 index;

    if (map == ZR_NULL || resumeId == 0u) {
        return ZR_FALSE;
    }
    for (index = 0u; index < map->entryCount; ++index) {
        if (map->entries[index].resumeId == resumeId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_state_map_deopt_resume_id(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction,
        TZrExecIrSourceId sourceId,
        TZrUInt32 *resumeId) {
    TZrUInt32 index;
    TZrUInt32 matchingStates = 0u;
    const SZrExecIrDeoptState *matchingState = ZR_NULL;

    if (function == ZR_NULL || instruction == ZR_NULL || resumeId == ZR_NULL ||
        instruction->deoptId == 0u || function->deoptStates == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        if (state->id == instruction->deoptId) {
            ++matchingStates;
            matchingState = state;
        }
    }
    if (matchingStates != 1u || matchingState->sourceId != sourceId ||
        matchingState->resumeId == 0u) {
        return ZR_FALSE;
    }
    *resumeId = matchingState->resumeId;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_BuildStateMaps(SZrExecIrFunction *function,
                                       SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrStateMap candidate;
    SZrStateMapLiveness liveness;
    SZrExecIrOwnerAnalysis ownership = {0};
    EZrExecutionDiagnosticCode livenessResult;
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
    livenessResult = ZrCore_ExecIr_StateMapLivenessBuild(function, &liveness);
    if (livenessResult != ZR_EXECUTION_DIAGNOSTIC_NONE) {
        zr_state_map_set_diagnostic(diagnostic, livenessResult, function, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_OwnerAnalysisBuild(function, &ownership, diagnostic)) {
        ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
        ZrCore_ExecIr_StateMapLivenessFree(&liveness);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_StateMapInit(&candidate);
    candidate.functionToken = function->functionToken;
    candidate.signatureHash = function->signatureHash;
    candidate.generation = function->contract.generation;
    for (instructionId = 1u; instructionId <= function->instructionCount; ++instructionId) {
        const SZrExecIrInstruction *instruction = &function->instructions[instructionId - 1u];
        TZrUInt32 boundaryFlags = 0u;
        TZrUInt32 currentResume;
        EZrStateMapBuildResult result;

        if (!ownership.reachable[instructionId - 1u]) {
            continue;
        }
        if (!ZrCore_ExecIr_StateMapBoundaryFlags(instruction, &boundaryFlags)) {
            continue;
        }
        if (instruction->deoptId != 0u) {
            TZrExecIrSourceId sourceId =
                instruction->sourceId != 0u ? instruction->sourceId : instructionId;
            if (!zr_state_map_deopt_resume_id(function, instruction, sourceId,
                                              &currentResume)) {
                ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
                ZrCore_ExecIr_StateMapLivenessFree(&liveness);
                ZrCore_ExecIr_StateMapFree(&candidate);
                zr_state_map_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                        function, instructionId, instruction->sourceId);
                return ZR_FALSE;
            }
            if (zr_state_map_resume_used(&candidate, currentResume)) {
                ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
                ZrCore_ExecIr_StateMapLivenessFree(&liveness);
                ZrCore_ExecIr_StateMapFree(&candidate);
                zr_state_map_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                        function, instructionId, instruction->sourceId);
                return ZR_FALSE;
            }
            if (currentResume >= resumeId) {
                if (currentResume == UINT32_MAX) {
                    resumeId = UINT32_MAX;
                } else {
                    resumeId = currentResume + 1u;
                }
            }
        } else {
            if (resumeId == UINT32_MAX) {
                ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
                ZrCore_ExecIr_StateMapLivenessFree(&liveness);
                ZrCore_ExecIr_StateMapFree(&candidate);
                zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                            function, instructionId, instruction->sourceId);
                return ZR_FALSE;
            }
            currentResume = resumeId++;
        }
        result = zr_state_map_add_checkpoint(&candidate, function, &liveness, &ownership, instruction,
                                             instructionId, boundaryFlags,
                                             ZR_EXEC_IR_STATE_BEFORE_EFFECT,
                                             currentResume, diagnostic);
        if (result == ZR_STATE_MAP_BUILD_OK) {
            result = zr_state_map_add_checkpoint(&candidate, function, &liveness, &ownership, instruction,
                                                 instructionId, boundaryFlags,
                                                 ZR_EXEC_IR_STATE_AFTER_EFFECT,
                                                 currentResume, diagnostic);
        }
        if (result == ZR_STATE_MAP_BUILD_OK &&
            (boundaryFlags & (ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW |
                              ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND |
                              ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP)) != 0u) {
            result = zr_state_map_add_checkpoint(&candidate, function, &liveness, &ownership, instruction,
                                                 instructionId, boundaryFlags,
                                                 ZR_EXEC_IR_STATE_CLEANUP_COMPLETE,
                                                 currentResume, diagnostic);
        }
        if (result != ZR_STATE_MAP_BUILD_OK) {
            ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
            ZrCore_ExecIr_StateMapLivenessFree(&liveness);
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
    ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
    ZrCore_ExecIr_StateMapLivenessFree(&liveness);
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
