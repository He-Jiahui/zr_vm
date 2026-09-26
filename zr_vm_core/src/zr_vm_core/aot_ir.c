#include "zr_vm_core/aot_ir.h"

#include <string.h>

static void aot_ir_diag_clear(SZrAotIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static EZrAotIrStatus aot_ir_fail(SZrAotIrDiagnostic *diagnostic,
                                  EZrAotIrStatus status,
                                  TZrUInt32 functionId,
                                  TZrUInt32 blockId,
                                  TZrUInt32 instructionId,
                                  TZrUInt32 index,
                                  TZrUInt64 expected,
                                  TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->functionId = functionId;
        diagnostic->blockId = blockId;
        diagnostic->instructionId = instructionId;
        diagnostic->index = index;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return status;
}

static TZrBool aot_ir_range_valid(SZrAotIrRange range, TZrUInt32 length) {
    return (TZrBool)(range.offset <= length && range.count <= length - range.offset);
}

static TZrBool aot_ir_exec_range_valid(SZrExecIrRange range,
                                        TZrUInt32 length) {
    return (TZrBool)(range.start <= length && range.count <= length - range.start);
}

static TZrBool aot_ir_alignment_valid(TZrUInt32 alignment) {
    return (TZrBool)(alignment != 0u && (alignment & (alignment - 1u)) == 0u);
}

static TZrBool aot_ir_opcode_is_terminator(TZrUInt32 opcode) {
    switch ((EZrExecIrOpcode)opcode) {
        case ZR_EXEC_IR_OPCODE_BRANCH:
        case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:
        case ZR_EXEC_IR_OPCODE_SWITCH:
        case ZR_EXEC_IR_OPCODE_INVOKE:
        case ZR_EXEC_IR_OPCODE_THROW:
        case ZR_EXEC_IR_OPCODE_SUSPEND:
        case ZR_EXEC_IR_OPCODE_RETURN:
        case ZR_EXEC_IR_OPCODE_ITER_INIT:
        case ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT:
        case ZR_EXEC_IR_OPCODE_ITER_CURRENT:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrUInt32 aot_ir_required_instruction_flags(TZrUInt32 opcode) {
    switch ((EZrExecIrOpcode)opcode) {
        case ZR_EXEC_IR_OPCODE_DIV:
        case ZR_EXEC_IR_OPCODE_STORE:
        case ZR_EXEC_IR_OPCODE_THROW:
            return ZR_EXEC_IR_FLAG_MAY_THROW;
        case ZR_EXEC_IR_OPCODE_CALL:
        case ZR_EXEC_IR_OPCODE_INVOKE:
        case ZR_EXEC_IR_OPCODE_ITER_INIT:
        case ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT:
        case ZR_EXEC_IR_OPCODE_ITER_CURRENT:
            return ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW;
        case ZR_EXEC_IR_OPCODE_ALLOC:
            return ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_GC;
        case ZR_EXEC_IR_OPCODE_BARRIER:
            return ZR_EXEC_IR_FLAG_MAY_GC;
        case ZR_EXEC_IR_OPCODE_SUSPEND:
            return ZR_EXEC_IR_FLAG_MAY_SUSPEND;
        default:
            return 0u;
    }
}

static TZrBool aot_ir_block_id_exists(const SZrAotIrFunction *function,
                                      TZrUInt32 blockId) {
    for (TZrUInt32 index = 0u; index < function->blockCount; ++index) {
        if (function->blocks[index].id == blockId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrUInt32 aot_ir_edge_occurrences(const SZrAotIrFunction *function,
                                          SZrAotIrRange range,
                                          TZrUInt32 targetBlockId) {
    TZrUInt32 count = 0u;
    for (TZrUInt32 index = 0u; index < range.count; ++index) {
        if (function->successorPool[range.offset + index] == targetBlockId) {
            ++count;
        }
    }
    return count;
}

static const SZrAotIrBlock *aot_ir_block_for_instruction(
        const SZrAotIrFunction *function, TZrUInt32 instructionIndex) {
    for (TZrUInt32 index = 0u; index < function->blockCount; ++index) {
        const SZrAotIrBlock *block = &function->blocks[index];
        if (instructionIndex >= block->instructions.offset &&
            instructionIndex - block->instructions.offset < block->instructions.count) {
            return block;
        }
    }
    return ZR_NULL;
}

static TZrUInt64 aot_ir_hash_byte(TZrUInt64 hash, TZrUInt8 byte) {
    return (hash ^ byte) * UINT64_C(1099511628211);
}

static TZrUInt64 aot_ir_hash_u32(TZrUInt64 hash, TZrUInt32 value) {
    for (TZrUInt32 i = 0u; i < 4u; ++i) {
        hash = aot_ir_hash_byte(hash, (TZrUInt8)(value >> (i * 8u)));
    }
    return hash;
}

static TZrUInt64 aot_ir_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    for (TZrUInt32 i = 0u; i < 8u; ++i) {
        hash = aot_ir_hash_byte(hash, (TZrUInt8)(value >> (i * 8u)));
    }
    return hash;
}

static TZrUInt64 aot_ir_hash_contract(TZrUInt64 hash,
                                      const SZrExecutionContract *contract) {
    hash = aot_ir_hash_u32(hash, contract->schemaVersion);
    hash = aot_ir_hash_u32(hash, contract->abiVersion);
    hash = aot_ir_hash_u32(hash, contract->logicalVersion);
    hash = aot_ir_hash_u64(hash, contract->generation);
    hash = aot_ir_hash_u32(hash, contract->targetToken);
    hash = aot_ir_hash_u64(hash, contract->signatureHash);
    hash = aot_ir_hash_u64(hash, contract->layoutHash);
    hash = aot_ir_hash_u64(hash, contract->moduleHash);
    hash = aot_ir_hash_u32(hash, contract->requiredCapabilities);
    return aot_ir_hash_u32(hash, contract->declaredEffects);
}

static TZrBool aot_ir_state_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static const SZrExecIrLayout *aot_ir_find_layout(
        const SZrAotIrModule *module, TZrUInt32 layoutId) {
    if (module == ZR_NULL || module->layoutPool == ZR_NULL || layoutId == 0u) {
        return ZR_NULL;
    }
    for (TZrUInt32 i = 0u; i < module->layoutCount; ++i) {
        if (module->layoutPool[i].id == layoutId) {
            return &module->layoutPool[i];
        }
    }
    return ZR_NULL;
}

static EZrAotIrStatus aot_ir_validate_deopt(
        const SZrAotIrModule *module, const SZrAotIrFunction *function,
        SZrAotIrDiagnostic *diagnostic) {
    if ((function->deoptStateCount != 0u && function->deoptStates == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValuePool == ZR_NULL) ||
        (function->deoptAggregateCount != 0u && function->deoptAggregates == ZR_NULL) ||
        (function->deoptAggregateFieldCount != 0u &&
         function->deoptAggregateFields == ZR_NULL)) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id,
                           0u, 0u, 0u, 1u, 0u);
    }
    for (TZrUInt32 i = 0u; i < function->deoptValueCount; ++i) {
        if (function->deoptValuePool[i] == ZR_AOT_IR_ID_INVALID ||
            function->deoptValuePool[i] > function->valueSlotCount) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                               0u, 0u, i, function->valueSlotCount,
                               function->deoptValuePool[i]);
        }
    }
    for (TZrUInt32 i = 0u; i < function->deoptStateCount; ++i) {
        const SZrExecIrDeoptState *state = &function->deoptStates[i];
        if (state->id == ZR_AOT_IR_ID_INVALID ||
            state->sourceId == ZR_AOT_IR_ID_INVALID ||
            state->resumeId == ZR_AOT_IR_ID_INVALID ||
            !aot_ir_exec_range_valid(state->valueRange, function->deoptValueCount) ||
            !aot_ir_exec_range_valid(state->aggregates,
                                     function->deoptAggregateCount)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id,
                               0u, 0u, i, function->deoptValueCount,
                               state->valueRange.count);
        }
    }
    for (TZrUInt32 i = 0u; i < function->deoptAggregateCount; ++i) {
        const SZrExecIrDeoptAggregate *aggregate = &function->deoptAggregates[i];
        const SZrExecIrLayout *layout =
                aot_ir_find_layout(module, aggregate->layoutId);
        if (aggregate->identityId == ZR_AOT_IR_ID_INVALID ||
            aggregate->typeToken == ZR_AOT_IR_ID_INVALID ||
            aggregate->layoutId == ZR_AOT_IR_ID_INVALID ||
            layout == ZR_NULL || layout->typeToken != aggregate->typeToken ||
            !aot_ir_exec_range_valid(aggregate->fields,
                                     function->deoptAggregateFieldCount)) {
            return aot_ir_fail(diagnostic,
                               layout == ZR_NULL
                                       ? ZR_AOT_IR_INVALID_ID
                                       : ZR_AOT_IR_INVALID_LAYOUT,
                               function->id, 0u, 0u, i,
                               function->deoptAggregateFieldCount,
                               aggregate->layoutId);
        }
    }
    for (TZrUInt32 i = 0u; i < function->deoptAggregateFieldCount; ++i) {
        const SZrExecIrDeoptAggregateField *field =
                &function->deoptAggregateFields[i];
        if (field->kind == ZR_EXEC_IR_DEOPT_FIELD_VALUE) {
            if (field->valueId == ZR_AOT_IR_ID_INVALID ||
                field->valueId > function->valueSlotCount ||
                field->aggregateId != ZR_AOT_IR_ID_INVALID) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                                   0u, 0u, i, function->valueSlotCount,
                                   field->valueId);
            }
        } else if (field->kind == ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE) {
            TZrBool found = ZR_FALSE;
            for (TZrUInt32 aggregateIndex = 0u;
                 aggregateIndex < function->deoptAggregateCount;
                 ++aggregateIndex) {
                if (function->deoptAggregates[aggregateIndex].identityId ==
                    field->aggregateId) {
                    found = ZR_TRUE;
                    break;
                }
            }
            if (field->valueId != ZR_AOT_IR_ID_INVALID ||
                field->aggregateId == ZR_AOT_IR_ID_INVALID || !found) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                                   0u, 0u, i, function->deoptAggregateCount,
                                   field->aggregateId);
            }
        } else if (field->kind != ZR_EXEC_IR_DEOPT_FIELD_UNINITIALIZED ||
                   field->valueId != ZR_AOT_IR_ID_INVALID ||
                   field->aggregateId != ZR_AOT_IR_ID_INVALID) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                               0u, 0u, i, ZR_EXEC_IR_DEOPT_FIELD_KIND_COUNT,
                               (TZrUInt32)field->kind);
        }
    }
    for (TZrUInt32 stateIndex = 0u;
         stateIndex < function->deoptStateCount; ++stateIndex) {
        const SZrExecIrDeoptState *state = &function->deoptStates[stateIndex];
        for (TZrUInt32 aggregateOffset = 0u;
             aggregateOffset < state->aggregates.count; ++aggregateOffset) {
            const SZrExecIrDeoptAggregate *aggregate =
                    &function->deoptAggregates[state->aggregates.start + aggregateOffset];
            for (TZrUInt32 fieldOffset = 0u;
                 fieldOffset < aggregate->fields.count; ++fieldOffset) {
                const SZrExecIrDeoptAggregateField *field =
                        &function->deoptAggregateFields[aggregate->fields.start + fieldOffset];
                if (field->kind == ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE) {
                    TZrBool found = ZR_FALSE;
                    for (TZrUInt32 otherOffset = 0u;
                         otherOffset < state->aggregates.count; ++otherOffset) {
                        if (function->deoptAggregates[state->aggregates.start + otherOffset]
                                    .identityId == field->aggregateId) {
                            found = ZR_TRUE;
                            break;
                        }
                    }
                    if (!found) {
                        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID,
                                           function->id, 0u, 0u,
                                           state->aggregates.start + aggregateOffset,
                                           1u, field->aggregateId);
                    }
                }
            }
        }
    }
    return ZR_AOT_IR_OK;
}

static EZrAotIrStatus aot_ir_validate_logical_map(
        const SZrAotIrFunction *function, SZrAotIrDiagnostic *diagnostic) {
    const SZrExecIrStateMap *map = function->logicalStateMap;
    if (map == ZR_NULL) return ZR_AOT_IR_OK;
    if (map->functionToken != function->functionToken) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id,
                           0u, 0u, 0u, function->functionToken, map->functionToken);
    }
    if (map->signatureHash != function->signatureHash) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id,
                           0u, 0u, 0u, function->signatureHash, map->signatureHash);
    }
    if (map->generation != function->contract.generation) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id,
                           0u, 0u, 0u, function->contract.generation, map->generation);
    }
    if (!ZrCore_ExecIr_StateMapStorageValid(map)) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id,
                           0u, 0u, 0u, 0u, map->entryCount);
    }
    for (TZrUInt32 i = 0u; i < map->entryCount; ++i) {
        const SZrExecIrStateMapEntry *entry = &map->entries[i];
        TZrBool instructionFound = ZR_FALSE;
        if (entry->resumeId == ZR_AOT_IR_ID_INVALID ||
            entry->instructionId == ZR_AOT_IR_ID_INVALID ||
            entry->sourceId == ZR_AOT_IR_ID_INVALID ||
            (TZrUInt32)entry->phase >= ZR_EXEC_IR_STATE_PHASE_COUNT) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                               0u, entry->instructionId, i, 1u, entry->resumeId);
        }
        if (!aot_ir_state_range_valid(entry->liveValues, map->valueCount) ||
            !aot_ir_state_range_valid(entry->rootValues, map->rootCount) ||
            !aot_ir_state_range_valid(entry->ownerStates, map->ownerStateCount) ||
            entry->ownerStates.count != entry->liveValues.count) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id,
                               0u, entry->instructionId, i, entry->liveValues.count,
                               entry->ownerStates.count);
        }
        for (TZrUInt32 j = 0u; j < function->instructionCount; ++j) {
            const SZrAotIrInstruction *instruction = &function->instructions[j];
            if (instruction->id == entry->instructionId &&
                entry->sourceId == (instruction->sourceId != 0u
                        ? instruction->sourceId : instruction->id)) {
                instructionFound = ZR_TRUE;
                break;
            }
        }
        if (!instructionFound ||
            (entry->handlerBlockId != 0u &&
             !aot_ir_block_id_exists(function, entry->handlerBlockId))) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                               0u, entry->instructionId, i, function->instructionCount,
                               entry->instructionId);
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            const SZrExecIrStateMapEntry *previous = &map->entries[j];
            if (previous->resumeId == entry->resumeId &&
                (previous->instructionId != entry->instructionId ||
                 previous->sourceId != entry->sourceId ||
                 previous->phase == entry->phase)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, function->id,
                                   0u, entry->instructionId, i, j, i);
            }
        }
        for (TZrUInt32 j = 0u; j < entry->liveValues.count; ++j) {
            if (map->valuePool[entry->liveValues.start + j] == 0u) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                                   0u, entry->instructionId, i, 1u,
                                   map->valuePool[entry->liveValues.start + j]);
            }
            if (map->ownerStatePool[entry->ownerStates.start + j] >=
                    ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                                   0u, entry->instructionId, i,
                                   ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT - 1u,
                                   map->ownerStatePool[entry->ownerStates.start + j]);
            }
        }
        for (TZrUInt32 j = 0u; j < entry->rootValues.count; ++j) {
            TZrExecIrValueId root = map->rootPool[entry->rootValues.start + j];
            TZrBool live = ZR_FALSE;
            for (TZrUInt32 k = 0u; k < entry->liveValues.count; ++k) {
                if (root != 0u &&
                    root == map->valuePool[entry->liveValues.start + k]) {
                    live = ZR_TRUE;
                    break;
                }
            }
            if (!live) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                                   0u, entry->instructionId, i, 1u, root);
            }
        }
    }
    return ZR_AOT_IR_OK;
}

static TZrUInt64 aot_ir_hash_logical_map(TZrUInt64 hash,
                                         const SZrExecIrStateMap *map) {
    hash = aot_ir_hash_u32(hash, map != ZR_NULL);
    if (map == ZR_NULL) return hash;
    hash = aot_ir_hash_u32(hash, map->functionToken);
    hash = aot_ir_hash_u64(hash, map->signatureHash);
    hash = aot_ir_hash_u64(hash, map->generation);
    hash = aot_ir_hash_u32(hash, map->entryCount);
    for (TZrUInt32 i = 0u; i < map->entryCount; ++i) {
        const SZrExecIrStateMapEntry *entry = &map->entries[i];
        const TZrUInt32 fields[] = {
            entry->sourceId, entry->instructionId, entry->deoptId,
            entry->resumeId, entry->cleanupState, entry->boundaryFlags,
            (TZrUInt32)entry->phase, entry->liveValues.start,
            entry->liveValues.count, entry->rootValues.start,
            entry->rootValues.count, entry->ownerStates.start,
            entry->ownerStates.count, entry->effectIn, entry->effectOut,
            entry->handlerBlockId, entry->exceptionState};
        for (TZrUInt32 j = 0u; j < (TZrUInt32)(sizeof(fields) / sizeof(fields[0])); ++j)
            hash = aot_ir_hash_u32(hash, fields[j]);
    }
    hash = aot_ir_hash_u32(hash, map->valueCount);
    for (TZrUInt32 i = 0u; i < map->valueCount; ++i)
        hash = aot_ir_hash_u32(hash, map->valuePool[i]);
    hash = aot_ir_hash_u32(hash, map->rootCount);
    for (TZrUInt32 i = 0u; i < map->rootCount; ++i)
        hash = aot_ir_hash_u32(hash, map->rootPool[i]);
    hash = aot_ir_hash_u32(hash, map->ownerStateCount);
    for (TZrUInt32 i = 0u; i < map->ownerStateCount; ++i)
        hash = aot_ir_hash_u32(hash, map->ownerStatePool[i]);
    return hash;
}

static EZrAotIrStatus aot_ir_validate_function(const SZrAotIrModule *module,
                                               const SZrAotIrFunction *function,
                                               TZrUInt32 functionIndex,
                                               SZrAotIrDiagnostic *diagnostic) {
    const TZrUInt32 knownBlockFlags =
            ZR_EXEC_IR_BLOCK_FLAG_ENTRY | ZR_EXEC_IR_BLOCK_FLAG_COLD |
            ZR_EXEC_IR_BLOCK_FLAG_CLEANUP | ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
    TZrUInt32 entryBlockCount = 0u;
    if (function->id == ZR_AOT_IR_ID_INVALID || function->functionToken == 0u ||
        function->signatureHash == 0u || function->frameLayout.layoutHash == 0u ||
        !aot_ir_alignment_valid(function->frameLayout.frameByteAlign) ||
        function->blocks == ZR_NULL || function->instructions == ZR_NULL) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, function->id, 0u, 0u,
                           functionIndex, 1u, 0u);
    }
    if (function->contract.schemaVersion != ZR_EXECUTION_CONTRACT_SCHEMA_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, ZR_EXECUTION_CONTRACT_SCHEMA_VERSION,
                           function->contract.schemaVersion);
    }
    if (function->contract.abiVersion != ZR_EXECUTION_CONTRACT_ABI_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, ZR_EXECUTION_CONTRACT_ABI_VERSION,
                           function->contract.abiVersion);
    }
    if (function->contract.logicalVersion != ZR_EXECUTION_CONTRACT_LOGICAL_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, ZR_EXECUTION_CONTRACT_LOGICAL_VERSION,
                           function->contract.logicalVersion);
    }
    if (function->contract.signatureHash != function->signatureHash) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, function->signatureHash,
                           function->contract.signatureHash);
    }
    if (function->contract.layoutHash != function->frameLayout.layoutHash) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, function->frameLayout.layoutHash,
                           function->contract.layoutHash);
    }
    if (function->contract.targetToken != function->functionToken) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, function->functionToken,
                           function->contract.targetToken);
    }
    if (function->contract.moduleHash != module->contract.moduleHash) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, module->contract.moduleHash,
                           function->contract.moduleHash);
    }
    if ((function->contract.requiredCapabilities &
         ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, ZR_EXECUTION_CAPABILITY_KNOWN_MASK,
                           function->contract.requiredCapabilities);
    }
    if ((function->contract.declaredEffects & ~ZR_EXECUTION_EFFECT_KNOWN_MASK) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, function->id, 0u, 0u,
                           functionIndex, ZR_EXECUTION_EFFECT_KNOWN_MASK,
                           function->contract.declaredEffects);
    }
    if (function->frameLayout.storageSlotCount > function->frameLayout.logicalSlotCount) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, function->id, 0u, 0u,
                           functionIndex, function->frameLayout.logicalSlotCount,
                           function->frameLayout.storageSlotCount);
    }
    if ((function->frameSlotCount != 0u && function->frameSlots == ZR_NULL) ||
        (function->valueSlotCount != 0u && function->valueSlotPool == ZR_NULL) ||
        function->frameSlotCount != function->frameLayout.storageSlotCount ||
        function->valueSlotCount != function->frameLayout.logicalSlotCount) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, function->id, 0u, 0u,
                           functionIndex, function->frameLayout.storageSlotCount,
                           function->frameSlotCount);
    }
    for (TZrUInt32 i = 0u; i < function->frameSlotCount; ++i) {
        const SZrAotIrFrameSlot *slot = &function->frameSlots[i];
        if (slot->slotId == ZR_AOT_IR_ID_INVALID ||
            !aot_ir_alignment_valid(slot->byteAlign) ||
            slot->byteOffset > function->frameLayout.frameByteSize ||
            (slot->byteOffset & (slot->byteAlign - 1u)) != 0u ||
            slot->byteSize > function->frameLayout.frameByteSize -
                             slot->byteOffset) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, function->id, 0u, 0u,
                               i, function->frameLayout.frameByteSize, slot->byteOffset);
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (function->frameSlots[j].slotId == slot->slotId) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, function->id,
                                   0u, 0u, i, function->frameSlots[j].slotId,
                                   slot->slotId);
            }
        }
    }
    for (TZrUInt32 i = 0u; i < function->valueSlotCount; ++i) {
        TZrUInt32 slot = function->valueSlotPool[i];
        if (slot >= function->frameSlotCount) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id,
                               0u, 0u, i, function->frameSlotCount, slot);
        }
    }
    if (function->frameLayout.parameterPrefixBytes >
        function->frameLayout.returnAreaOffset) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, function->id, 0u, 0u,
                           functionIndex, function->frameLayout.returnAreaOffset,
                           function->frameLayout.parameterPrefixBytes);
    }
    if (function->frameLayout.frameByteSize < function->frameLayout.returnAreaOffset ||
        (function->frameLayout.frameByteSize &
         (function->frameLayout.frameByteAlign - 1u)) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, function->id, 0u, 0u,
                           functionIndex, function->frameLayout.frameByteAlign,
                           function->frameLayout.frameByteSize);
    }
    if ((function->sourceMapCount != 0u && function->sourceMaps == ZR_NULL) ||
        (function->gcRootCount != 0u && function->gcRootPool == ZR_NULL) ||
        (function->deoptStateCount != 0u && function->deoptStates == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValuePool == ZR_NULL) ||
        (function->deoptAggregateCount != 0u &&
         function->deoptAggregates == ZR_NULL) ||
        (function->deoptAggregateFieldCount != 0u &&
         function->deoptAggregateFields == ZR_NULL) ||
        function->blockCount == 0u || function->instructionCount == 0u ||
        ((function->operandCount > 0u) && (function->operandPool == ZR_NULL)) ||
        ((function->resultCount > 0u) && (function->resultPool == ZR_NULL)) ||
        ((function->phiIncomingCount > 0u) && (function->phiIncomingPool == ZR_NULL)) ||
        ((function->successorCount > 0u) && (function->successorPool == ZR_NULL)) ||
        ((function->memoryTokenCount > 0u) &&
         (function->memoryTokenPool == ZR_NULL))) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, function->id, 0u, 0u,
                           functionIndex, 0u, 0u);
    }
    for (TZrUInt32 i = 0u; i < function->operandCount; ++i) {
        if (function->operandPool[i] == ZR_AOT_IR_ID_INVALID) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id, 0u, 0u,
                               i, 1u, function->operandPool[i]);
        }
    }
    for (TZrUInt32 i = 0u; i < function->resultCount; ++i) {
        if (function->resultPool[i] == ZR_AOT_IR_ID_INVALID) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id, 0u, 0u,
                               i, 1u, function->resultPool[i]);
        }
    }
    for (TZrUInt32 i = 0u; i < function->blockCount; ++i) {
        const SZrAotIrBlock *block = &function->blocks[i];
        if ((block->flags & ZR_EXEC_IR_BLOCK_FLAG_ENTRY) != 0u) {
            ++entryBlockCount;
        }
        if (block->id == ZR_AOT_IR_ID_INVALID ||
            (block->flags & ~knownBlockFlags) != 0u ||
            !aot_ir_range_valid(block->instructions, function->instructionCount) ||
            !aot_ir_range_valid(block->predecessors, function->successorCount) ||
            !aot_ir_range_valid(block->successors, function->successorCount)) {
            return aot_ir_fail(diagnostic,
                               (block->flags & ~knownBlockFlags) != 0u
                                   ? ZR_AOT_IR_INVALID_CFG
                                   : ZR_AOT_IR_INVALID_RANGE,
                               function->id, block->id, 0u, i,
                               (block->flags & ~knownBlockFlags) != 0u
                                   ? knownBlockFlags
                                   : function->instructionCount,
                               (block->flags & ~knownBlockFlags) != 0u
                                   ? block->flags
                                   : block->instructions.offset);
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (function->blocks[j].id == block->id) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, function->id, block->id, 0u,
                                   i, j, i);
            }
        }
        if (block->terminatorInstructionId != ZR_AOT_IR_ID_INVALID) {
            TZrBool found = ZR_FALSE;
            TZrUInt32 terminatorOffset = 0u;
            const SZrAotIrInstruction *terminator = ZR_NULL;
            for (TZrUInt32 j = 0u; j < block->instructions.count; ++j) {
                if (function->instructions[block->instructions.offset + j].id ==
                    block->terminatorInstructionId) {
                    found = ZR_TRUE;
                    terminatorOffset = j;
                    terminator = &function->instructions[block->instructions.offset + j];
                    break;
                }
            }
            if (!found) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id, block->id,
                                   block->terminatorInstructionId, i, 1u, 0u);
            }
            if (!aot_ir_opcode_is_terminator(terminator->opcode)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                                   block->id, terminator->id, i, 1u,
                                   terminator->opcode);
            }
            if (terminatorOffset != block->instructions.count - 1u) {
                const SZrAotIrInstruction *lastInstruction =
                        &function->instructions[block->instructions.offset +
                                                 block->instructions.count - 1u];
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                                   block->id, terminator->id, i, lastInstruction->id,
                                   terminator->id);
            }
        }
        for (TZrUInt32 j = 0u; j < block->successors.count; ++j) {
            TZrUInt32 target = function->successorPool[
                    block->successors.offset + j];
            if (!aot_ir_block_id_exists(function, target)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                                   block->id, 0u, j, function->blockCount, target);
            }
        }
        for (TZrUInt32 j = 0u; j < block->predecessors.count; ++j) {
            TZrUInt32 source = function->successorPool[
                    block->predecessors.offset + j];
            if (!aot_ir_block_id_exists(function, source)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                                   block->id, 0u, j, function->blockCount, source);
            }
        }
    }
    for (TZrUInt32 i = 0u; i < function->blockCount; ++i) {
        const SZrAotIrBlock *source = &function->blocks[i];
        for (TZrUInt32 j = 0u; j < function->blockCount; ++j) {
            const SZrAotIrBlock *target = &function->blocks[j];
            TZrUInt32 outgoing = aot_ir_edge_occurrences(
                    function, source->successors, target->id);
            TZrUInt32 incoming = aot_ir_edge_occurrences(
                    function, target->predecessors, source->id);
            if (outgoing != incoming) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                                   source->id, 0u, j, outgoing, incoming);
            }
        }
    }
    if (entryBlockCount != 1u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id, 0u, 0u,
                           functionIndex, 1u, entryBlockCount);
    }
    for (TZrUInt32 instructionIndex = 0u;
         instructionIndex < function->instructionCount; ++instructionIndex) {
        TZrUInt32 containingBlockCount = 0u;
        for (TZrUInt32 blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
            const SZrAotIrBlock *block = &function->blocks[blockIndex];
            if (instructionIndex >= block->instructions.offset &&
                instructionIndex - block->instructions.offset < block->instructions.count) {
                ++containingBlockCount;
            }
        }
        if (containingBlockCount != 1u) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id, 0u,
                               function->instructions[instructionIndex].id,
                               instructionIndex, 1u, containingBlockCount);
        }
    }
    for (TZrUInt32 i = 0u; i < function->instructionCount; ++i) {
        const SZrAotIrInstruction *instruction = &function->instructions[i];
        const SZrAotIrBlock *containingBlock =
                aot_ir_block_for_instruction(function, i);
        TZrUInt32 requiredFlags =
                aot_ir_required_instruction_flags(instruction->opcode);
        if (instruction->id == ZR_AOT_IR_ID_INVALID ||
            instruction->opcode == ZR_EXEC_IR_OPCODE_INVALID ||
            instruction->opcode >= ZR_EXEC_IR_OPCODE_COUNT ||
            (instruction->flags & ~ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK) != 0u ||
            !aot_ir_range_valid(instruction->results, function->resultCount) ||
            !aot_ir_range_valid(instruction->operands, function->operandCount) ||
            !aot_ir_range_valid(instruction->successors, function->successorCount)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_OPCODE, function->id, 0u,
                               instruction->id, i, ZR_EXEC_IR_OPCODE_COUNT, instruction->opcode);
        }
        if (!aot_ir_range_valid(instruction->memoryIn,
                                function->memoryTokenCount) ||
            !aot_ir_range_valid(instruction->memoryOut,
                                function->memoryTokenCount)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id,
                               containingBlock != ZR_NULL ? containingBlock->id : 0u,
                               instruction->id, i, function->memoryTokenCount,
                               instruction->memoryIn.count + instruction->memoryOut.count);
        }
        if ((instruction->opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST &&
             instruction->matchTypeToken == 0u) ||
            (instruction->opcode != ZR_EXEC_IR_OPCODE_TYPE_TEST &&
             instruction->matchTypeToken != 0u)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                               containingBlock != ZR_NULL ? containingBlock->id : 0u,
                               instruction->id, i,
                               instruction->opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST ? 1u : 0u,
                               instruction->matchTypeToken);
        }
        if ((instruction->flags & requiredFlags) != requiredFlags) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id,
                               containingBlock != ZR_NULL ? containingBlock->id : 0u,
                               instruction->id, i, requiredFlags, instruction->flags);
        }
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_CONSTANT) {
            const SZrExecIrConstant *constant;
            if (module->constantCount == 0u || module->constantPool == ZR_NULL ||
                instruction->layoutId >= module->constantCount) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID,
                                   function->id,
                                   containingBlock != ZR_NULL
                                       ? containingBlock->id : 0u,
                                   instruction->id, i, module->constantCount,
                                   instruction->layoutId);
            }
            constant = &module->constantPool[instruction->layoutId];
            if (instruction->typeToken != 0u &&
                instruction->typeToken != constant->typeToken) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_SIGNATURE,
                                   function->id,
                                   containingBlock != ZR_NULL
                                       ? containingBlock->id : 0u,
                                   instruction->id, i, constant->typeToken,
                                   instruction->typeToken);
            }
        }
        if (requiredFlags != 0u &&
            (instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
             instruction->effectOut == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id,
                               containingBlock != ZR_NULL ? containingBlock->id : 0u,
                               instruction->id, i, 1u,
                               instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID
                                   ? instruction->effectIn
                                   : instruction->effectOut);
        }
        if (!aot_ir_range_valid(instruction->phiIncoming, function->phiIncomingCount)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id, 0u,
                               instruction->id, i, function->phiIncomingCount,
                               instruction->phiIncoming.offset);
        }
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_PHI &&
            instruction->phiIncoming.count != 0u) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id, 0u,
                               instruction->id, i, 0u,
                               instruction->phiIncoming.count);
        }
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_PHI &&
            (containingBlock == ZR_NULL ||
             instruction->phiIncoming.count != containingBlock->predecessors.count)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG, function->id,
                               containingBlock != ZR_NULL ? containingBlock->id : 0u,
                               instruction->id, i,
                               containingBlock != ZR_NULL ? containingBlock->predecessors.count : 0u,
                               instruction->phiIncoming.count);
        }
        for (TZrUInt32 j = 0u; j < instruction->phiIncoming.count; ++j) {
            const SZrAotIrPhiIncoming *incoming = &function->phiIncomingPool[
                    instruction->phiIncoming.offset + j];
            if (!aot_ir_block_id_exists(function, incoming->predecessorBlockId)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG,
                                   function->id,
                                   containingBlock != ZR_NULL ? containingBlock->id : 0u,
                                   instruction->id, j,
                                   function->blockCount,
                                   incoming->predecessorBlockId);
            }
            if (containingBlock == ZR_NULL ||
                function->successorPool[containingBlock->predecessors.offset + j] !=
                        incoming->predecessorBlockId) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG,
                                   function->id,
                                   containingBlock != ZR_NULL ? containingBlock->id : 0u,
                                   instruction->id, j,
                                   containingBlock != ZR_NULL
                                       ? function->successorPool[
                                                 containingBlock->predecessors.offset + j]
                                       : 0u,
                                   incoming->predecessorBlockId);
            }
            if (incoming->valueId == ZR_AOT_IR_ID_INVALID) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID,
                                   function->id, 0u, instruction->id, j, 1u,
                                   incoming->valueId);
            }
        }
        for (TZrUInt32 j = 0u; j < instruction->successors.count; ++j) {
            TZrUInt32 target = function->successorPool[
                    instruction->successors.offset + j];
            if (!aot_ir_block_id_exists(function, target)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CFG,
                                   function->id, 0u, instruction->id, j,
                                   function->blockCount, target);
            }
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (function->instructions[j].id == instruction->id) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, function->id, 0u,
                                   instruction->id, i, j, i);
            }
        }
        if ((instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) !=
            (instruction->effectOut == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id, 0u,
                               instruction->id, i, instruction->effectIn, instruction->effectOut);
        }
        if (instruction->effectIn != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
            instruction->effectOut <= instruction->effectIn) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id, 0u,
                               instruction->id, i,
                               instruction->effectIn == UINT32_MAX
                                   ? instruction->effectIn
                                   : instruction->effectIn + 1u,
                               instruction->effectOut);
        }
    }
    for (TZrUInt32 i = 0u; i < function->sourceMapCount; ++i) {
        const SZrAotIrSourceMap *map = &function->sourceMaps[i];
        TZrBool found = ZR_FALSE;
        if (map->sourceId == ZR_AOT_IR_ID_INVALID ||
            map->instructionId == ZR_AOT_IR_ID_INVALID ||
            map->startOffset > map->endOffset || map->startLine == 0u ||
            map->startColumn == 0u || map->endLine == 0u || map->endColumn == 0u) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id,
                               0u, map->instructionId, i, 1u, map->sourceId);
        }
        for (TZrUInt32 j = 0u; j < function->instructionCount; ++j) {
            if (function->instructions[j].id == map->instructionId &&
                function->instructions[j].sourceId == map->sourceId) {
                found = ZR_TRUE;
                break;
            }
        }
        if (!found) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                               0u, map->instructionId, i, function->instructionCount,
                               map->instructionId);
        }
    }
    if (function->gcMap != ZR_NULL) {
        const SZrExecIrGcMap *map = function->gcMap;
        if ((map->entryCount != 0u && map->entries == ZR_NULL) ||
            (map->slotIndexCount != 0u && map->slotIndexPool == ZR_NULL) ||
            (map->inlineRefOffsetCount != 0u && map->inlineRefOffsetPool == ZR_NULL) ||
            !aot_ir_exec_range_valid(map->rootRange, map->slotIndexCount)) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, function->id,
                               0u, 0u, 0u, map->slotIndexCount, map->rootRange.count);
        }
        for (TZrUInt32 i = 0u; i < map->entryCount; ++i) {
            const SZrExecIrGcMapEntry *entry = &map->entries[i];
            if (entry->site == ZR_AOT_IR_ID_INVALID ||
                entry->site > function->instructionCount ||
                !aot_ir_exec_range_valid(entry->liveRefSlots, map->slotIndexCount) ||
                !aot_ir_exec_range_valid(entry->inlineRefOffsets,
                                         map->inlineRefOffsetCount)) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE,
                                   function->id, 0u, entry->site, i,
                                   function->instructionCount, entry->site);
            }
            for (TZrUInt32 j = 0u; j < entry->liveRefSlots.count; ++j) {
                if (map->slotIndexPool[entry->liveRefSlots.offset + j] >=
                    function->frameSlotCount) {
                    return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT,
                                       function->id, 0u, entry->site, i,
                                       function->frameSlotCount,
                                       map->slotIndexPool[entry->liveRefSlots.offset + j]);
                }
            }
        }
    }
    for (TZrUInt32 i = 0u; i < function->gcRootCount; ++i) {
        if (function->gcRootPool[i] == ZR_AOT_IR_ID_INVALID ||
            function->gcRootPool[i] > function->frameLayout.logicalSlotCount) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                               0u, 0u, i, function->frameLayout.logicalSlotCount,
                               function->gcRootPool[i]);
        }
    }
    {
        EZrAotIrStatus deoptStatus =
                aot_ir_validate_deopt(module, function, diagnostic);
        if (deoptStatus != ZR_AOT_IR_OK) return deoptStatus;
    }
    for (TZrUInt32 blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrAotIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 latestEffect = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
        for (TZrUInt32 offset = 0u; offset < block->instructions.count; ++offset) {
            TZrUInt32 instructionIndex = block->instructions.offset + offset;
            const SZrAotIrInstruction *instruction =
                    &function->instructions[instructionIndex];
            if (aot_ir_required_instruction_flags(instruction->opcode) == 0u) {
                continue;
            }
            if (latestEffect != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
                instruction->effectIn != latestEffect) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_EFFECT, function->id,
                                   block->id, instruction->id, instructionIndex,
                                   latestEffect, instruction->effectIn);
            }
            latestEffect = instruction->effectOut;
        }
    }
    for (TZrUInt32 i = 0u; i < function->memoryTokenCount; ++i) {
        if (function->memoryTokenPool[i] == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, function->id,
                               0u, 0u, i, 1u, function->memoryTokenPool[i]);
        }
    }
    return aot_ir_validate_logical_map(function, diagnostic);
}

EZrAotIrStatus ZrCore_AotIr_ValidateTarget(const SZrAotIrTargetContract *target,
                                           SZrAotIrDiagnostic *diagnostic) {
    aot_ir_diag_clear(diagnostic);
    if (target == ZR_NULL) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, 0u, 0u, 0u, 0u, 0u, 0u);
    }
    if (target->abiVersion != ZR_AOT_IR_TARGET_ABI_VERSION ||
        (target->pointerSize != 4u && target->pointerSize != 8u) ||
        target->endianness > 1u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_TARGET, 0u, 0u, 0u, 0u,
                           ZR_AOT_IR_TARGET_ABI_VERSION, target->abiVersion);
    }
    if (target->targetTripleHash == 0u || target->abiHash == 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_TARGET, 0u, 0u, 0u, 0u,
                           1u, target->targetTripleHash == 0u
                                       ? target->targetTripleHash
                                       : target->abiHash);
    }
    if ((target->requiredCapabilities & ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_TARGET, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_CAPABILITY_KNOWN_MASK,
                           target->requiredCapabilities);
    }
    return ZR_AOT_IR_OK;
}

EZrAotIrStatus ZrCore_AotIr_ValidateModule(const SZrAotIrModule *module,
                                           SZrAotIrDiagnostic *diagnostic) {
    aot_ir_diag_clear(diagnostic);
    if (module == ZR_NULL ||
        module->schemaVersion != ZR_AOT_IR_SCHEMA_VERSION ||
        module->functions == ZR_NULL || module->functionCount == 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, 0u, 0u, 0u, 0u,
                           ZR_AOT_IR_SCHEMA_VERSION, module != ZR_NULL ? module->schemaVersion : 0u);
    }
    if (ZrCore_AotIr_ValidateTarget(&module->target, diagnostic) != ZR_AOT_IR_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status : ZR_AOT_IR_INVALID_TARGET;
    }
    if (module->contract.schemaVersion != ZR_EXECUTION_CONTRACT_SCHEMA_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_CONTRACT_SCHEMA_VERSION,
                           module->contract.schemaVersion);
    }
    if (module->contract.abiVersion != ZR_EXECUTION_CONTRACT_ABI_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_CONTRACT_ABI_VERSION,
                           module->contract.abiVersion);
    }
    if (module->contract.logicalVersion != ZR_EXECUTION_CONTRACT_LOGICAL_VERSION) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_CONTRACT_LOGICAL_VERSION,
                           module->contract.logicalVersion);
    }
    if (module->moduleHash == 0u ||
        module->contract.moduleHash != module->moduleHash) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           module->moduleHash, module->contract.moduleHash);
    }
    if (module->constantCount != 0u && module->constantPool == ZR_NULL) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, 0u, 0u, 0u, 0u,
                           1u, 0u);
    }
    for (TZrUInt32 i = 0u; i < module->constantCount; ++i) {
        if (module->constantPool[i].typeToken == ZR_AOT_IR_ID_INVALID) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_ID, 0u, 0u, 0u, i,
                               1u, module->constantPool[i].typeToken);
        }
    }
    if (module->layoutCount != 0u && module->layoutPool == ZR_NULL) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, 0u, 0u, 0u, 0u,
                           1u, 0u);
    }
    for (TZrUInt32 i = 0u; i < module->layoutCount; ++i) {
        const SZrExecIrLayout *layout = &module->layoutPool[i];
        if (layout->id == ZR_AOT_IR_ID_INVALID ||
            layout->typeToken == ZR_AOT_IR_ID_INVALID ||
            layout->byteSize == 0u || !aot_ir_alignment_valid(layout->byteAlign) ||
            layout->layoutHash == 0u) {
            return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, 0u, 0u, 0u,
                               i, 1u, layout->id);
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (module->layoutPool[j].id == layout->id) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, 0u, 0u, 0u,
                                   i, j, layout->id);
            }
        }
    }
    if ((module->contract.requiredCapabilities &
         ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_CAPABILITY_KNOWN_MASK,
                           module->contract.requiredCapabilities);
    }
    if ((module->contract.declaredEffects & ~ZR_EXECUTION_EFFECT_KNOWN_MASK) != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_INVALID_CONTRACT, 0u, 0u, 0u, 0u,
                           ZR_EXECUTION_EFFECT_KNOWN_MASK,
                           module->contract.declaredEffects);
    }
    for (TZrUInt32 i = 0u; i < module->functionCount; ++i) {
        EZrAotIrStatus status = aot_ir_validate_function(module, &module->functions[i], i, diagnostic);
        if (status != ZR_AOT_IR_OK) {
            return status;
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (module->functions[j].id == module->functions[i].id) {
                return aot_ir_fail(diagnostic, ZR_AOT_IR_DUPLICATE_ID, module->functions[i].id,
                                   0u, 0u, i, j, i);
            }
        }
    }
    if (module->relocationCount != 0u) {
        return aot_ir_fail(diagnostic, ZR_AOT_IR_RELOCATION, 0u, 0u, 0u, 0u, 0u,
                           module->relocationCount);
    }
    return ZR_AOT_IR_OK;
}

TZrBool ZrCore_AotIr_IsRelocationFree(const SZrAotIrModule *module,
                                      SZrAotIrDiagnostic *diagnostic) {
    aot_ir_diag_clear(diagnostic);
    if (module == ZR_NULL || module->functions == ZR_NULL ||
        module->functionCount == 0u || module->relocationCount != 0u) {
        (void)aot_ir_fail(diagnostic, ZR_AOT_IR_RELOCATION, 0u, 0u, 0u, 0u, 0u,
                          module != ZR_NULL ? module->relocationCount : 1u);
        return ZR_FALSE;
    }
    if (module->functions != ZR_NULL) {
        for (TZrUInt32 i = 0u; i < module->functionCount; ++i) {
            if (module->functions[i].relocationCount != 0u) {
                (void)aot_ir_fail(diagnostic, ZR_AOT_IR_RELOCATION,
                                  module->functions[i].id, 0u, 0u, i, 0u,
                                  module->functions[i].relocationCount);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrUInt64 ZrCore_AotIr_HashModule(const SZrAotIrModule *module) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    if (ZrCore_AotIr_ValidateModule(module, ZR_NULL) != ZR_AOT_IR_OK) {
        return 0u;
    }
    hash = aot_ir_hash_u32(hash, module->schemaVersion);
    hash = aot_ir_hash_u32(hash, module->flags);
    hash = aot_ir_hash_u32(hash, module->target.abiVersion);
    hash = aot_ir_hash_u32(hash, module->target.pointerSize);
    hash = aot_ir_hash_u32(hash, module->target.endianness);
    hash = aot_ir_hash_u32(hash, module->target.requiredCapabilities);
    hash = aot_ir_hash_u64(hash, module->target.targetTripleHash);
    hash = aot_ir_hash_u64(hash, module->target.abiHash);
    hash = aot_ir_hash_contract(hash, &module->contract);
    hash = aot_ir_hash_u64(hash, module->moduleHash);
    hash = aot_ir_hash_u32(hash, module->constantCount);
    for (TZrUInt32 i = 0u; i < module->constantCount; ++i) {
        hash = aot_ir_hash_u32(hash, module->constantPool[i].typeToken);
        hash = aot_ir_hash_u32(hash, module->constantPool[i].flags);
        hash = aot_ir_hash_u64(hash, module->constantPool[i].bits);
    }
    hash = aot_ir_hash_u32(hash, module->layoutCount);
    for (TZrUInt32 i = 0u; i < module->layoutCount; ++i) {
        hash = aot_ir_hash_u32(hash, module->layoutPool[i].id);
        hash = aot_ir_hash_u32(hash, module->layoutPool[i].typeToken);
        hash = aot_ir_hash_u32(hash, module->layoutPool[i].byteSize);
        hash = aot_ir_hash_u32(hash, module->layoutPool[i].byteAlign);
        hash = aot_ir_hash_u64(hash, module->layoutPool[i].layoutHash);
    }
    hash = aot_ir_hash_u32(hash, module->functionCount);
    for (TZrUInt32 i = 0u; i < module->functionCount; ++i) {
        const SZrAotIrFunction *function = &module->functions[i];
        hash = aot_ir_hash_u32(hash, function->id);
        hash = aot_ir_hash_u32(hash, function->functionToken);
        hash = aot_ir_hash_contract(hash, &function->contract);
        hash = aot_ir_hash_u64(hash, function->signatureHash);
        hash = aot_ir_hash_u32(hash, function->frameLayout.logicalSlotCount);
        hash = aot_ir_hash_u32(hash, function->frameLayout.storageSlotCount);
        hash = aot_ir_hash_u32(hash, function->frameLayout.parameterPrefixBytes);
        hash = aot_ir_hash_u32(hash, function->frameLayout.returnAreaOffset);
        hash = aot_ir_hash_u32(hash, function->frameLayout.frameByteSize);
        hash = aot_ir_hash_u32(hash, function->frameLayout.frameByteAlign);
        hash = aot_ir_hash_u64(hash, function->frameLayout.layoutHash);
        hash = aot_ir_hash_u32(hash, function->frameSlotCount);
        for (TZrUInt32 j = 0u; j < function->frameSlotCount; ++j) {
            const SZrAotIrFrameSlot *slot = &function->frameSlots[j];
            hash = aot_ir_hash_u32(hash, slot->slotId);
            hash = aot_ir_hash_u32(hash, slot->byteOffset);
            hash = aot_ir_hash_u32(hash, slot->byteSize);
            hash = aot_ir_hash_u32(hash, slot->byteAlign);
            hash = aot_ir_hash_u32(hash, slot->typeToken);
            hash = aot_ir_hash_u32(hash, slot->kind);
        }
        hash = aot_ir_hash_u32(hash, function->valueSlotCount);
        for (TZrUInt32 j = 0u; j < function->valueSlotCount; ++j)
            hash = aot_ir_hash_u32(hash, function->valueSlotPool[j]);
        hash = aot_ir_hash_u32(hash, function->blockCount);
        for (TZrUInt32 j = 0u; j < function->blockCount; ++j) {
            const SZrAotIrBlock *block = &function->blocks[j];
            hash = aot_ir_hash_u32(hash, block->id);
            hash = aot_ir_hash_u32(hash, block->flags);
            hash = aot_ir_hash_u32(hash, block->instructions.offset);
            hash = aot_ir_hash_u32(hash, block->instructions.count);
            hash = aot_ir_hash_u32(hash, block->predecessors.offset);
            hash = aot_ir_hash_u32(hash, block->predecessors.count);
            hash = aot_ir_hash_u32(hash, block->successors.offset);
            hash = aot_ir_hash_u32(hash, block->successors.count);
            hash = aot_ir_hash_u32(hash, block->terminatorInstructionId);
        }
        hash = aot_ir_hash_u32(hash, function->instructionCount);
        for (TZrUInt32 j = 0u; j < function->instructionCount; ++j) {
            const SZrAotIrInstruction *instruction = &function->instructions[j];
            const TZrUInt32 fields[] = {
                instruction->id, instruction->opcode, instruction->flags,
                instruction->results.offset, instruction->results.count,
                instruction->operands.offset, instruction->operands.count,
                instruction->successors.offset, instruction->successors.count,
                instruction->phiIncoming.offset, instruction->phiIncoming.count,
                instruction->effectIn, instruction->effectOut, instruction->sourceId,
                instruction->deoptId, instruction->layoutId, instruction->bindingRow,
                instruction->typeToken, instruction->matchTypeToken,
                instruction->memoryIn.offset, instruction->memoryIn.count,
                instruction->memoryOut.offset, instruction->memoryOut.count};
            for (TZrUInt32 k = 0u; k < (TZrUInt32)(sizeof(fields) / sizeof(fields[0])); ++k) {
                hash = aot_ir_hash_u32(hash, fields[k]);
            }
        }
        hash = aot_ir_hash_u32(hash, function->operandCount);
        for (TZrUInt32 j = 0u; j < function->operandCount; ++j) hash = aot_ir_hash_u32(hash, function->operandPool[j]);
        hash = aot_ir_hash_u32(hash, function->resultCount);
        for (TZrUInt32 j = 0u; j < function->resultCount; ++j) hash = aot_ir_hash_u32(hash, function->resultPool[j]);
        hash = aot_ir_hash_u32(hash, function->phiIncomingCount);
        for (TZrUInt32 j = 0u; j < function->phiIncomingCount; ++j) {
            hash = aot_ir_hash_u32(hash, function->phiIncomingPool[j].predecessorBlockId);
            hash = aot_ir_hash_u32(hash, function->phiIncomingPool[j].valueId);
        }
        hash = aot_ir_hash_u32(hash, function->successorCount);
        for (TZrUInt32 j = 0u; j < function->successorCount; ++j) hash = aot_ir_hash_u32(hash, function->successorPool[j]);
        hash = aot_ir_hash_u32(hash, function->memoryTokenCount);
        for (TZrUInt32 j = 0u; j < function->memoryTokenCount; ++j)
            hash = aot_ir_hash_u32(hash, function->memoryTokenPool[j]);
        hash = aot_ir_hash_u32(hash, function->sourceMapCount);
        for (TZrUInt32 j = 0u; j < function->sourceMapCount; ++j) {
            const SZrAotIrSourceMap *map = &function->sourceMaps[j];
            hash = aot_ir_hash_u32(hash, map->sourceId);
            hash = aot_ir_hash_u32(hash, map->instructionId);
            hash = aot_ir_hash_u32(hash, map->startOffset);
            hash = aot_ir_hash_u32(hash, map->endOffset);
            hash = aot_ir_hash_u32(hash, map->startLine);
            hash = aot_ir_hash_u32(hash, map->startColumn);
            hash = aot_ir_hash_u32(hash, map->endLine);
            hash = aot_ir_hash_u32(hash, map->endColumn);
        }
        hash = aot_ir_hash_u32(hash, function->gcMap != ZR_NULL);
        if (function->gcMap != ZR_NULL) {
            const SZrExecIrGcMap *map = function->gcMap;
            hash = aot_ir_hash_u32(hash, map->entryCount);
            hash = aot_ir_hash_u32(hash, map->slotIndexCount);
            hash = aot_ir_hash_u32(hash, map->inlineRefOffsetCount);
            hash = aot_ir_hash_u32(hash, map->safepointId);
            hash = aot_ir_hash_u32(hash, map->sourceId);
            hash = aot_ir_hash_u32(hash, map->rootRange.start);
            hash = aot_ir_hash_u32(hash, map->rootRange.count);
            for (TZrUInt32 j = 0u; j < map->entryCount; ++j) {
                const SZrExecIrGcMapEntry *entry = &map->entries[j];
                hash = aot_ir_hash_u32(hash, entry->site);
                hash = aot_ir_hash_u32(hash, entry->liveRefSlots.offset);
                hash = aot_ir_hash_u32(hash, entry->liveRefSlots.count);
                hash = aot_ir_hash_u32(hash, entry->inlineRefOffsets.offset);
                hash = aot_ir_hash_u32(hash, entry->inlineRefOffsets.count);
            }
            for (TZrUInt32 j = 0u; j < map->slotIndexCount; ++j)
                hash = aot_ir_hash_u32(hash, map->slotIndexPool[j]);
            for (TZrUInt32 j = 0u; j < map->inlineRefOffsetCount; ++j)
                hash = aot_ir_hash_u32(hash, map->inlineRefOffsetPool[j]);
        }
        hash = aot_ir_hash_u32(hash, function->gcRootCount);
        for (TZrUInt32 j = 0u; j < function->gcRootCount; ++j)
            hash = aot_ir_hash_u32(hash, function->gcRootPool[j]);
        hash = aot_ir_hash_u32(hash, function->deoptStateCount);
        for (TZrUInt32 j = 0u; j < function->deoptStateCount; ++j) {
            const SZrExecIrDeoptState *state = &function->deoptStates[j];
            hash = aot_ir_hash_u32(hash, state->id);
            hash = aot_ir_hash_u32(hash, state->sourceId);
            hash = aot_ir_hash_u32(hash, state->resumeId);
            hash = aot_ir_hash_u32(hash, state->valueRange.start);
            hash = aot_ir_hash_u32(hash, state->valueRange.count);
            hash = aot_ir_hash_u32(hash, state->cleanupState);
            hash = aot_ir_hash_u32(hash, state->aggregates.start);
            hash = aot_ir_hash_u32(hash, state->aggregates.count);
        }
        hash = aot_ir_hash_u32(hash, function->deoptValueCount);
        for (TZrUInt32 j = 0u; j < function->deoptValueCount; ++j)
            hash = aot_ir_hash_u32(hash, function->deoptValuePool[j]);
        hash = aot_ir_hash_u32(hash, function->deoptAggregateCount);
        for (TZrUInt32 j = 0u; j < function->deoptAggregateCount; ++j) {
            const SZrExecIrDeoptAggregate *aggregate =
                    &function->deoptAggregates[j];
            hash = aot_ir_hash_u32(hash, aggregate->identityId);
            hash = aot_ir_hash_u32(hash, aggregate->typeToken);
            hash = aot_ir_hash_u32(hash, aggregate->layoutId);
            hash = aot_ir_hash_u32(hash, aggregate->fields.start);
            hash = aot_ir_hash_u32(hash, aggregate->fields.count);
        }
        hash = aot_ir_hash_u32(hash, function->deoptAggregateFieldCount);
        for (TZrUInt32 j = 0u; j < function->deoptAggregateFieldCount; ++j) {
            const SZrExecIrDeoptAggregateField *field =
                    &function->deoptAggregateFields[j];
            hash = aot_ir_hash_u32(hash, field->fieldIndex);
            hash = aot_ir_hash_u32(hash, (TZrUInt32)field->kind);
            hash = aot_ir_hash_u32(hash, field->valueId);
            hash = aot_ir_hash_u32(hash, field->aggregateId);
        }
        hash = aot_ir_hash_logical_map(hash, function->logicalStateMap);
        hash = aot_ir_hash_u64(hash, function->gcMapHash);
        hash = aot_ir_hash_u64(hash, function->exceptionMapHash);
        hash = aot_ir_hash_u64(hash, function->debugMapHash);
    }
    return hash;
}

const TZrChar *ZrCore_AotIr_StatusName(EZrAotIrStatus status) {
    switch (status) {
        case ZR_AOT_IR_OK: return "ok";
        case ZR_AOT_IR_INVALID_ARGUMENT: return "invalid_argument";
        case ZR_AOT_IR_VERSION_MISMATCH: return "version_mismatch";
        case ZR_AOT_IR_INVALID_TARGET: return "invalid_target";
        case ZR_AOT_IR_INVALID_CONTRACT: return "invalid_contract";
        case ZR_AOT_IR_INVALID_ID: return "invalid_id";
        case ZR_AOT_IR_DUPLICATE_ID: return "duplicate_id";
        case ZR_AOT_IR_INVALID_RANGE: return "invalid_range";
        case ZR_AOT_IR_INVALID_CFG: return "invalid_cfg";
        case ZR_AOT_IR_INVALID_OPCODE: return "invalid_opcode";
        case ZR_AOT_IR_INVALID_EFFECT: return "invalid_effect";
        case ZR_AOT_IR_INVALID_LAYOUT: return "invalid_layout";
        case ZR_AOT_IR_INVALID_SIGNATURE: return "invalid_signature";
        case ZR_AOT_IR_RELOCATION: return "relocation";
        case ZR_AOT_IR_UNSUPPORTED: return "unsupported";
        default: return "unknown";
    }
}
