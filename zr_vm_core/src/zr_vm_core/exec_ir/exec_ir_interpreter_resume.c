#include "exec_ir_interpreter_internal.h"
#include "exec_ir_state_map_storage.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static TZrBool zr_oracle_value_on_edge(const SZrExecIrFunction *function,
        const SZrExecIrStateMapEntry *entry, TZrBool terminated,
        TZrUInt32 ordinal, TZrExecIrValueId value);

TZrBool zr_oracle_select_checkpoint(const SZrExecIrOracleInput *input,
                                    const SZrExecIrOracleCheckpoint *point,
                                    const SZrExecIrOracleContinuation *identity,
                                    const SZrExecIrOracleExecutionResult *witness,
                                    SZrExecIrMaterializedState *state,
                                    SZrExecIrStateMapEntry *entry,
                                    SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrFunction *f = input->function;
    SZrExecIrResumeRequest request = {0};
    const SZrExecIrStateMapEntry *selected;
    request.function = f;
    request.map = f->stateMap;
    request.functionToken = identity != ZR_NULL ? identity->functionToken : f->functionToken;
    request.generation = identity != ZR_NULL ? identity->generation : f->contract.generation;
    request.signatureHash = identity != ZR_NULL ? identity->signatureHash : f->signatureHash;
    request.sourceId = point->sourceId;
    request.resumeId = point->resumeId;
    request.phase = point->phase;
    request.target = ZR_NULL;
    if (!ZrCore_ExecIr_MaterializeState(&request, diagnostic)) {
        if (identity != ZR_NULL && diagnostic != ZR_NULL) {
            if (diagnostic->instructionId == 0u) diagnostic->instructionId = identity->instructionId;
            if (diagnostic->sourceId == 0u) diagnostic->sourceId = point->sourceId;
        }
        return ZR_FALSE;
    }
    selected = ZrCore_ExecIr_StateMapFind(f->stateMap, point->sourceId,
                                        point->resumeId, point->phase);
    if (selected == ZR_NULL) return ZR_FALSE;
    *entry = *selected;
    if (state != ZR_NULL || witness != ZR_NULL) {
        TZrUInt32 *edgeOwners = ZR_NULL;
        TZrBool ok;
        request.target = state;
        if (witness != ZR_NULL) {
            request.ownerStates = witness->ownerStates;
            request.ownerStateCount = witness->ownerStateCount;
            /* Ordinary INVOKE results in the established static after-map
             * describe the normal edge. They are omitted on an exceptional
             * continuation. Conditional cleanup entries describe both edges
             * and must always retain their actual concrete witness. */
            if (identity != ZR_NULL && identity->terminated && identity->successorOrdinal != 0u &&
                witness->ownerStateCount != 0u) {
                size_t bytes;
                TZrUInt32 index;
                if (!zr_oracle_bytes(witness->ownerStateCount, sizeof(*edgeOwners), &bytes)) {
                    zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                   f, witness->currentBlock, entry->instructionId, entry->sourceId, 0u, 0u);
                    return ZR_FALSE;
                }
                edgeOwners = malloc(bytes);
                if (edgeOwners == ZR_NULL) {
                    zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                   f, witness->currentBlock, entry->instructionId, entry->sourceId, 0u, 0u);
                    return ZR_FALSE;
                }
                memcpy(edgeOwners, witness->ownerStates, bytes);
                for (index = 0u; index < entry->liveValues.count; ++index) {
                    TZrExecIrValueId value = f->stateMap->valuePool[entry->liveValues.start + index];
                    TZrUInt32 owner = f->stateMap->ownerStatePool[entry->ownerStates.start + index];
                    if (owner != ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL &&
                        !zr_oracle_value_on_edge(f, entry, identity->terminated,
                                                identity->successorOrdinal, value))
                        edgeOwners[value - 1u] = owner;
                }
                request.ownerStates = edgeOwners;
            }
        }
        ok = ZrCore_ExecIr_MaterializeState(&request, diagnostic);
        free(edgeOwners);
        return ok;
    }
    return ZR_TRUE;
}

/* A static after-map covers all successors. Its normal result definitions
 * have no runtime value on an exceptional edge; all other live IDs remain
 * required. This is the same result availability rule as CFG ownership. */
static TZrBool zr_oracle_value_on_edge(const SZrExecIrFunction *function,
                                      const SZrExecIrStateMapEntry *entry,
                                      TZrBool terminated, TZrUInt32 ordinal,
                                      TZrExecIrValueId value) {
    const SZrExecIrInstruction *instruction = &function->instructions[entry->instructionId - 1u];
    const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(instruction->opcode);
    TZrUInt32 index;
    if (entry->phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT || !terminated || ordinal == 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) == 0u) return ZR_TRUE;
    for (index = 0u; index < instruction->resultRange.count; ++index) {
        if (function->results[instruction->resultRange.start + index] == value) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_oracle_live_value_valid(const SZrExecIrFunction *function,
                                          const SZrExecIrOracleExecutionResult *state,
                                          const SZrExecIrStateMapEntry *entry,
                                          TZrExecIrValueId value,
                                          TZrUInt32 ownerState,
                                          SZrExecIrDiagnostic *diagnostic) {
    if (value == 0u || value > state->valueCount ||
        !zr_oracle_value_kind_valid(state->values[value - 1u].kind) ||
        ownerState >= ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL ||
        state->ownerStates[value - 1u] != ownerState ||
        ((ownerState == ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED ||
          ownerState == ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN) !=
         (state->values[value - 1u].kind != ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED))) {
        zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                       function, state->currentBlock, entry->instructionId,
                       entry->sourceId, state->valueCount, value);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool zr_oracle_capture_checkpoint(const SZrExecIrOracleInput *input,
                                     SZrExecIrOracleExecutionResult *result,
                                     const SZrExecIrStateMapEntry *entry,
                                     TZrBool terminated, TZrExecIrBlockId next,
                                     TZrUInt32 ordinal,
                                     SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrFunction *f = input->function;
    TZrUInt32 index;
    SZrExecIrMaterializedState logical = {0};
    SZrExecIrStateMapEntry selected;
    SZrExecIrOracleContinuation edge = {0};
    SZrExecIrOracleCheckpoint point = {entry->sourceId, entry->resumeId, entry->phase};
    SZrExecIrOracleContinuation *continuation = &result->continuation;
    edge.functionToken = f->functionToken;
    edge.generation = f->contract.generation;
    edge.signatureHash = f->signatureHash;
    edge.terminated = terminated;
    edge.successorOrdinal = ordinal;
    if (!zr_oracle_select_checkpoint(input, &point, &edge, result,
                                     &logical, &selected, diagnostic)) return ZR_FALSE;
    for (index = 0u; index < logical.valueCount; ++index) {
        TZrExecIrValueId value = logical.values[index];
        TZrUInt32 classification = f->stateMap->ownerStatePool[entry->ownerStates.start + index];
        if ((classification == ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL ||
             zr_oracle_value_on_edge(f, entry, terminated, ordinal, value)) &&
            !zr_oracle_live_value_valid(f, result, entry, value,
                                        logical.ownerStates[index], diagnostic)) {
            ZrCore_ExecIr_MaterializedStateFree(&logical);
            return ZR_FALSE;
        }
    }
    ZrCore_ExecIr_MaterializedStateFree(&logical);
    memset(continuation, 0, sizeof(*continuation));
    continuation->checkpoint.sourceId = entry->sourceId;
    continuation->checkpoint.resumeId = entry->resumeId;
    continuation->checkpoint.phase = entry->phase;
    continuation->functionToken = f->functionToken;
    continuation->signatureHash = f->signatureHash;
    continuation->generation = f->contract.generation;
    continuation->instructionId = entry->instructionId;
    continuation->terminated = terminated;
    continuation->nextBlock = next;
    continuation->successorOrdinal = ordinal;
    result->paused = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool zr_oracle_resume_cursor(const SZrExecIrFunction *f,
                                       const SZrExecIrOracleExecutionResult *state,
                                       const SZrExecIrStateMapEntry *entry,
                                       SZrOracleCursor *cursor) {
    const SZrExecIrOracleContinuation *saved = &state->continuation;
    const SZrExecIrInstruction *instruction = &f->instructions[entry->instructionId - 1u];
    const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(instruction->opcode);
    TZrUInt32 index;
    TZrBool before = entry->phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT;
    TZrBool terminator = (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) != 0u;
    memset(cursor, 0, sizeof(*cursor));
    if (saved->instructionId != entry->instructionId ||
        saved->terminated != (TZrBool)(!before && terminator)) return ZR_FALSE;
    cursor->instructionIndex = entry->instructionId - (before ? 1u : 0u);
    cursor->skipFirstStop = before;
    for (index = 0u; index < f->blockCount; ++index) {
        const SZrExecIrBlock *block = &f->blocks[index];
        if (entry->instructionId - 1u >= block->instructionRange.start &&
            entry->instructionId - 1u - block->instructionRange.start < block->instructionRange.count) {
            cursor->block = block->id;
            break;
        }
    }
    if (cursor->block != state->currentBlock ||
        (f->blockCount != 0u && cursor->block == 0u)) return ZR_FALSE;
    if (before || !terminator) {
        return (TZrBool)(saved->nextBlock == 0u && saved->successorOrdinal == 0u &&
                         !state->returned && !state->terminatedByThrow && !state->suspended);
    }
    if (instruction->opcode == ZR_EXEC_IR_OPCODE_RETURN ||
        instruction->opcode == ZR_EXEC_IR_OPCODE_THROW) {
        cursor->done = ZR_TRUE;
        return (TZrBool)(saved->nextBlock == 0u && saved->successorOrdinal == 0u &&
                state->returned == (instruction->opcode == ZR_EXEC_IR_OPCODE_RETURN) &&
                state->terminatedByThrow == (instruction->opcode == ZR_EXEC_IR_OPCODE_THROW) &&
                !state->suspended);
    }
    if (instruction->opcode == ZR_EXEC_IR_OPCODE_SUSPEND) {
        const SZrExecIrBlock *block;
        if (!state->suspended || state->returned || state->terminatedByThrow ||
            saved->nextBlock != 0u || saved->successorOrdinal != 0u) return ZR_FALSE;
        if (f->blockCount == 0u) return ZR_TRUE;
        block = &f->blocks[cursor->block - 1u];
        if (block->successorRange.count != 1u) return ZR_FALSE;
        cursor->previous = cursor->block;
        cursor->block = f->successors[block->successorRange.start];
        cursor->enterBlock = ZR_TRUE;
        return ZR_TRUE;
    }
    if (state->returned || state->terminatedByThrow || state->suspended ||
        saved->successorOrdinal >= instruction->successorRange.count ||
        f->successors[instruction->successorRange.start + saved->successorOrdinal] != saved->nextBlock) {
        return ZR_FALSE;
    }
    cursor->previous = cursor->block;
    cursor->block = saved->nextBlock;
    cursor->successorOrdinal = saved->successorOrdinal;
    cursor->enterBlock = ZR_TRUE;
    return ZR_TRUE;
}

/* Committing a resume frees the old buffers. Validate their full allocation
 * spans before reading witnesses, including when the checkpoint has no live
 * values. Counts alone cannot distinguish an interior alias from ownership. */
static TZrBool zr_oracle_result_storage_valid(
        const SZrExecIrOracleInput *input, const SZrExecIrOracleExecutionResult *state) {
    const void *storage[] = {state->values, state->events, state->ownerStates, state};
    const void *inputs[] = {input, input->constants, input->stopAt};
    size_t bytes[sizeof(storage) / sizeof(storage[0])];
    size_t inputBytes[sizeof(inputs) / sizeof(inputs[0])];
    size_t index, prior;
    if (!zr_oracle_bytes(state->valueCapacity, sizeof(*state->values), &bytes[0]) ||
        !zr_oracle_bytes(state->eventCapacity, sizeof(*state->events), &bytes[1]) ||
        !zr_oracle_bytes(state->ownerStateCapacity, sizeof(*state->ownerStates), &bytes[2]) ||
        !zr_oracle_bytes(input->constantCount, sizeof(*input->constants), &inputBytes[1]))
        return ZR_FALSE;
    bytes[3] = sizeof(*state);
    inputBytes[0] = sizeof(*input);
    inputBytes[2] = input->stopAt != ZR_NULL ? sizeof(*input->stopAt) : 0u;
    for (index = 0u; index < sizeof(storage) / sizeof(storage[0]); ++index) {
        uintptr_t begin = (uintptr_t)storage[index];
        if (bytes[index] == 0u) continue;
        if (storage[index] == ZR_NULL || begin > UINTPTR_MAX - bytes[index]) return ZR_FALSE;
        if (!zr_state_map_input_span_disjoint(input->function, input->function->stateMap,
                                               storage[index], bytes[index])) return ZR_FALSE;
        for (prior = 0u; prior < sizeof(inputs) / sizeof(inputs[0]); ++prior) {
            uintptr_t other = (uintptr_t)inputs[prior];
            if (inputBytes[prior] == 0u) continue;
            if (inputs[prior] == ZR_NULL || other > UINTPTR_MAX - inputBytes[prior] ||
                (begin < other + inputBytes[prior] && other < begin + bytes[index])) return ZR_FALSE;
        }
        for (prior = 0u; prior < index; ++prior) {
            uintptr_t other = (uintptr_t)storage[prior];
            if (bytes[prior] != 0u && begin < other + bytes[prior] &&
                other < begin + bytes[index]) return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool zr_oracle_prepare_resume(const SZrExecIrOracleInput *input,
                                 const SZrExecIrOracleExecutionResult *state,
                                 SZrExecIrOracleExecutionResult *prepared,
                                 SZrOracleCursor *cursor,
                                 SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrFunction *f = input->function;
    const SZrExecIrOracleContinuation *saved = &state->continuation;
    SZrExecIrMaterializedState logical;
    SZrExecIrStateMapEntry entry;
    TZrUInt32 index;
    size_t valueBytes, eventBytes, ownerBytes;
    if (state->ownershipTag != ZR_EXEC_IR_ORACLE_RESULT_TAG || state->paused != ZR_TRUE ||
        state->valueCount != f->valueCount || state->valueCount > state->valueCapacity ||
        state->eventCount > state->eventCapacity ||
        state->ownerStateCount != f->valueCount || state->ownerStateCount > state->ownerStateCapacity ||
        ((state->ownerStateCapacity == 0u) != (state->ownerStates == ZR_NULL)) ||
        ((state->valueCapacity == 0u) != (state->values == ZR_NULL)) ||
        ((state->eventCapacity == 0u) != (state->events == ZR_NULL)) ||
        !zr_oracle_result_storage_valid(input, state)) goto invalid;
    for (index = 0u; index < state->ownerStateCount; ++index)
        if (state->ownerStates[index] >= ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL) goto invalid;
    /* A captured identity is exact, including zero. The generic materializer
     * also serves requests with optional identity fields, so check here first. */
    if (saved->functionToken != f->functionToken ||
        saved->signatureHash != f->signatureHash ||
        saved->generation != f->contract.generation) {
        EZrExecutionDiagnosticCode code = saved->functionToken != f->functionToken
                ? ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH
                : saved->generation != f->contract.generation
                    ? ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION
                    : ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH;
        zr_oracle_diag(diagnostic, code, f, state->currentBlock,
                       saved->instructionId, saved->checkpoint.sourceId, 0u, 0u);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_MaterializedStateInit(&logical);
    if (!zr_oracle_select_checkpoint(input, &saved->checkpoint, saved, state,
                                     &logical, &entry, diagnostic)) return ZR_FALSE;
    if (!zr_oracle_resume_cursor(f, state, &entry, cursor)) {
        ZrCore_ExecIr_MaterializedStateFree(&logical);
        goto invalid;
    }
    for (index = 0u; index < logical.valueCount; ++index) {
        if ((f->stateMap->ownerStatePool[entry.ownerStates.start + index] ==
                    ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL ||
             zr_oracle_value_on_edge(f, &entry, saved->terminated, saved->successorOrdinal,
                                    logical.values[index])) &&
            !zr_oracle_live_value_valid(f, state, &entry, logical.values[index],
                                        logical.ownerStates[index], diagnostic)) {
            ZrCore_ExecIr_MaterializedStateFree(&logical);
            return ZR_FALSE;
        }
    }
    if (!zr_oracle_bytes(f->valueCount, sizeof(*prepared->values), &valueBytes) ||
        !zr_oracle_bytes(state->eventCount, sizeof(*prepared->events), &eventBytes) ||
        !zr_oracle_bytes(state->ownerStateCount, sizeof(*prepared->ownerStates), &ownerBytes)) {
        ZrCore_ExecIr_MaterializedStateFree(&logical);
        zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                       f, state->currentBlock, entry.instructionId, entry.sourceId, 0u, 0u);
        return ZR_FALSE;
    }
    *prepared = *state;
    prepared->values = ZR_NULL;
    prepared->events = ZR_NULL;
    prepared->ownerStates = ZR_NULL;
    prepared->ownerStateCapacity = prepared->ownerStateCount;
    prepared->valueCapacity = prepared->valueCount;
    prepared->eventCapacity = prepared->eventCount;
    if (valueBytes != 0u) prepared->values = (SZrExecIrOracleValue *)calloc(1u, valueBytes);
    if (eventBytes != 0u) prepared->events = (SZrExecIrOracleEvent *)malloc(eventBytes);
    if (ownerBytes != 0u) prepared->ownerStates = (TZrUInt32 *)malloc(ownerBytes);
    if ((valueBytes != 0u && prepared->values == ZR_NULL) ||
        (eventBytes != 0u && prepared->events == ZR_NULL) ||
        (ownerBytes != 0u && prepared->ownerStates == ZR_NULL)) {
        ZrCore_ExecIr_OracleResultFree(prepared);
        ZrCore_ExecIr_MaterializedStateFree(&logical);
        zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                       f, state->currentBlock, entry.instructionId, entry.sourceId, 0u, 0u);
        return ZR_FALSE;
    }
    if (ownerBytes != 0u) memcpy(prepared->ownerStates, state->ownerStates, ownerBytes);
    for (index = 0u; index < logical.valueCount; ++index) {
        TZrUInt32 valueIndex = logical.values[index] - 1u;
        if ((logical.ownerStates[index] == ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED ||
             logical.ownerStates[index] == ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN) &&
            zr_oracle_value_on_edge(f, &entry, saved->terminated, saved->successorOrdinal,
                                    logical.values[index])) {
            prepared->values[valueIndex] = state->values[valueIndex];
        }
    }
    if (eventBytes != 0u) memcpy(prepared->events, state->events, eventBytes);
    prepared->paused = ZR_FALSE;
    if (prepared->suspended) zr_oracle_undefined(&prepared->returnValue);
    prepared->suspended = ZR_FALSE;
    ZrCore_ExecIr_MaterializedStateFree(&logical);
    return ZR_TRUE;
invalid:
    zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                   f, state->currentBlock, saved->instructionId,
                   saved->checkpoint.sourceId, 0u, 0u);
    return ZR_FALSE;
}
