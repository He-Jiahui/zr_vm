#include "exec_ir_interpreter_internal.h"

#include <stdlib.h>
#include <string.h>

TZrBool zr_oracle_select_checkpoint(const SZrExecIrOracleInput *input,
                                    const SZrExecIrOracleCheckpoint *point,
                                    const SZrExecIrOracleContinuation *identity,
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
    request.target = state;
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
                                          SZrExecIrDiagnostic *diagnostic) {
    if (value == 0u || value > state->valueCount ||
        !zr_oracle_value_kind_valid(state->values[value - 1u].kind) ||
        state->values[value - 1u].kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
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
    SZrExecIrOracleContinuation *continuation = &result->continuation;
    for (index = 0u; index < entry->liveValues.count; ++index) {
        TZrExecIrValueId value = f->stateMap->valuePool[entry->liveValues.start + index];
        if (zr_oracle_value_on_edge(f, entry, terminated, ordinal, value) &&
            !zr_oracle_live_value_valid(f, result, entry, value, diagnostic)) return ZR_FALSE;
    }
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
    size_t valueBytes, eventBytes;
    if (state->ownershipTag != ZR_EXEC_IR_ORACLE_RESULT_TAG || state->paused != ZR_TRUE ||
        state->valueCount != f->valueCount || state->valueCount > state->valueCapacity ||
        state->eventCount > state->eventCapacity ||
        ((state->valueCapacity == 0u) != (state->values == ZR_NULL)) ||
        ((state->eventCapacity == 0u) != (state->events == ZR_NULL))) goto invalid;
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
    if (!zr_oracle_select_checkpoint(input, &saved->checkpoint, saved,
                                     &logical, &entry, diagnostic)) return ZR_FALSE;
    if (!zr_oracle_resume_cursor(f, state, &entry, cursor)) {
        ZrCore_ExecIr_MaterializedStateFree(&logical);
        goto invalid;
    }
    for (index = 0u; index < logical.valueCount; ++index) {
        if (zr_oracle_value_on_edge(f, &entry, saved->terminated, saved->successorOrdinal,
                                    logical.values[index]) &&
            !zr_oracle_live_value_valid(f, state, &entry, logical.values[index], diagnostic)) {
            ZrCore_ExecIr_MaterializedStateFree(&logical);
            return ZR_FALSE;
        }
    }
    if (!zr_oracle_bytes(f->valueCount, sizeof(*prepared->values), &valueBytes) ||
        !zr_oracle_bytes(state->eventCount, sizeof(*prepared->events), &eventBytes)) {
        ZrCore_ExecIr_MaterializedStateFree(&logical);
        zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                       f, state->currentBlock, entry.instructionId, entry.sourceId, 0u, 0u);
        return ZR_FALSE;
    }
    *prepared = *state;
    prepared->values = ZR_NULL;
    prepared->events = ZR_NULL;
    prepared->valueCapacity = prepared->valueCount;
    prepared->eventCapacity = prepared->eventCount;
    if (valueBytes != 0u) prepared->values = (SZrExecIrOracleValue *)calloc(1u, valueBytes);
    if (eventBytes != 0u) prepared->events = (SZrExecIrOracleEvent *)malloc(eventBytes);
    if ((valueBytes != 0u && prepared->values == ZR_NULL) ||
        (eventBytes != 0u && prepared->events == ZR_NULL)) {
        ZrCore_ExecIr_OracleResultFree(prepared);
        ZrCore_ExecIr_MaterializedStateFree(&logical);
        zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                       f, state->currentBlock, entry.instructionId, entry.sourceId, 0u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < logical.valueCount; ++index) {
        TZrUInt32 valueIndex = logical.values[index] - 1u;
        if (zr_oracle_value_on_edge(f, &entry, saved->terminated, saved->successorOrdinal,
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
