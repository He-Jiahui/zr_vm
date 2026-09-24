#include "exec_ir_interpreter_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define ZR_ORACLE_DEFAULT_STEP_MULTIPLIER ((TZrUInt32)1024u)

void ZrCore_ExecIr_OracleResultInit(SZrExecIrOracleExecutionResult *r) {
    if (r != ZR_NULL) {
        memset(r, 0, sizeof(*r));
        r->ownershipTag = ZR_EXEC_IR_ORACLE_RESULT_TAG;
    }
}

void ZrCore_ExecIr_OracleResultFree(SZrExecIrOracleExecutionResult *r) {
    if (r != ZR_NULL && r->ownershipTag == ZR_EXEC_IR_ORACLE_RESULT_TAG) {
        free(r->values);
        free(r->events);
        free(r->ownerStates);
        memset(r, 0, sizeof(*r));
    }
}


static TZrBool zr_oracle_input_valid(const SZrExecIrOracleInput *input,
                                     TZrBool initial,
                                     SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i;
    const SZrExecIrFunction *f = input != ZR_NULL ? input->function : ZR_NULL;
    if (input == ZR_NULL || !zr_oracle_validate(f, diagnostic)) return ZR_FALSE;
    if ((initial && (input->initialValueCount > f->valueCount ||
         (input->initialValueCount != 0u && input->initialValues == ZR_NULL))) ||
        (input->constantCount != 0u && input->constants == ZR_NULL)) {
        zr_oracle_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                       f, 0u, 0u, 0u, f->valueCount, input->initialValueCount);
        return ZR_FALSE;
    }
    for (i = 0u; initial && i < input->initialValueCount; ++i) {
        if (!zr_oracle_value_kind_valid(input->initialValues[i].kind)) {
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                           f, 0u, 0u, 0u, ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                           (TZrUInt32)input->initialValues[i].kind);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < input->constantCount; ++i) {
        if (!zr_oracle_value_kind_valid(input->constants[i].kind)) {
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                           f, 0u, 0u, 0u, ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                           (TZrUInt32)input->constants[i].kind);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_oracle_stop_valid(const SZrExecIrOracleInput *input,
                                    SZrExecIrStateMapEntry *stop,
                                    SZrExecIrDiagnostic *diagnostic) {
    TZrBool ok;
    memset(stop, 0, sizeof(*stop));
    if (input->stopAt == ZR_NULL) return ZR_TRUE;
    ok = zr_oracle_select_checkpoint(input, input->stopAt, ZR_NULL, ZR_NULL,
                                     ZR_NULL, stop, diagnostic);
    return ok;
}

/* Fresh execution and resume use the same dispatcher and edge-entry loop.
 * A restored in-block cursor already contains phi results; only a new edge
 * enters phis. Stop metadata is copied before any result replacement. */
static TZrBool zr_oracle_run(const SZrExecIrOracleInput *input,
                             SZrExecIrOracleExecutionResult *result,
                             SZrOracleCursor cursor,
                             const SZrExecIrStateMapEntry *stop,
                             SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrFunction *f = input->function;
    TZrUInt32 maxSteps = input->maxSteps, steps = 0u;
    if (maxSteps == 0u) {
        maxSteps = f->instructionCount > (UINT32_MAX - 1u) / ZR_ORACLE_DEFAULT_STEP_MULTIPLIER
                ? UINT32_MAX : f->instructionCount * ZR_ORACLE_DEFAULT_STEP_MULTIPLIER + 1u;
    }
    while (!cursor.done) {
        TZrUInt32 end = f->instructionCount;
        TZrBool terminated = ZR_FALSE;
        TZrExecIrBlockId next = 0u;
        TZrUInt32 ordinal = 0u;
        const SZrExecIrBlock *block = ZR_NULL;
        result->currentBlock = cursor.block;
        if (steps >= maxSteps) goto step_limit;
        if (f->blockCount != 0u) {
            if (cursor.block == 0u || cursor.block > f->blockCount) goto invalid_block;
            block = &f->blocks[cursor.block - 1u];
            end = block->instructionRange.start + block->instructionRange.count;
            if (cursor.enterBlock) {
                if (!zr_oracle_enter(f, cursor.block, cursor.previous,
                                     cursor.successorOrdinal, result, diagnostic)) return ZR_FALSE;
                cursor.instructionIndex = block->instructionRange.start;
            }
        }
        cursor.enterBlock = ZR_FALSE;
        for (; cursor.instructionIndex < end; ++cursor.instructionIndex) {
            TZrExecIrInstructionId id = cursor.instructionIndex + 1u;
            const SZrExecIrInstruction *instruction = &f->instructions[id - 1u];
            if (!cursor.skipFirstStop && stop->instructionId == id &&
                stop->phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT) {
                return zr_oracle_capture_checkpoint(input, result, stop,
                                                     ZR_FALSE, 0u, 0u, diagnostic);
            }
            cursor.skipFirstStop = ZR_FALSE;
            if (steps >= maxSteps || result->executedInstructionCount == UINT32_MAX) {
                zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_STEP_LIMIT,
                               f, cursor.block, id, instruction->sourceId, maxSteps, steps);
                return ZR_FALSE;
            }
            ++steps;
            ++result->executedInstructionCount;
            if (!zr_oracle_exec(input, result, instruction, id, cursor.block,
                                &terminated, &next, &ordinal, diagnostic)) return ZR_FALSE;
            if (terminated && ordinal != 0u &&
                (ZrCore_ExecIr_OpcodeInfo(instruction->opcode)->flags &
                 ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u) {
                TZrUInt32 at;
                /* Exceptional edges do not define normal results. Clear any
                 * value left from an earlier iteration before checkpointing. */
                for (at = 0u; at < instruction->resultRange.count; ++at) {
                    TZrExecIrValueId value = f->results[instruction->resultRange.start + at];
                    zr_oracle_undefined(&result->values[value - 1u]);
                    result->ownerStates[value - 1u] = ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED;
                }
            }
            if (stop->instructionId == id && stop->phase != ZR_EXEC_IR_STATE_BEFORE_EFFECT) {
                return zr_oracle_capture_checkpoint(input, result, stop,
                                                     terminated, next, ordinal, diagnostic);
            }
            if (terminated) break;
        }
        if (result->returned || result->terminatedByThrow || result->suspended ||
            block == ZR_NULL) return ZR_TRUE;
        if (!terminated) {
            if (block->successorRange.count != 1u) {
                zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR,
                               f, cursor.block, 0u, 0u, 1u, block->successorRange.count);
                return ZR_FALSE;
            }
            next = f->successors[block->successorRange.start];
        }
        if (next == 0u || next > f->blockCount) goto invalid_block;
        cursor.previous = cursor.block;
        cursor.successorOrdinal = ordinal;
        cursor.block = next;
        cursor.enterBlock = ZR_TRUE;
        continue;
step_limit:
        zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_STEP_LIMIT,
                       f, cursor.block, 0u, 0u, maxSteps, steps);
        return ZR_FALSE;
invalid_block:
        zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                       f, cursor.block, 0u, 0u, f->blockCount, next);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_RunOracleEx(const SZrExecIrOracleInput *input,
                                  SZrExecIrOracleExecutionResult *result,
                                  SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrFunction *f;
    SZrExecIrOracleExecutionResult prepared;
    SZrExecIrStateMapEntry stop;
    SZrOracleCursor cursor = {0};
    size_t bytes;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (input == ZR_NULL || result == ZR_NULL) {
        zr_oracle_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                       input != ZR_NULL ? input->function : ZR_NULL, 0u, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (!zr_oracle_input_valid(input, ZR_TRUE, diagnostic) ||
        !zr_oracle_stop_valid(input, &stop, diagnostic)) return ZR_FALSE;
    f = input->function;
    ZrCore_ExecIr_OracleResultInit(&prepared);
    prepared.instructionCount = f->instructionCount;
    prepared.valueCount = prepared.valueCapacity = f->valueCount;
    prepared.ownerStateCount = prepared.ownerStateCapacity = f->valueCount;
    if (f->valueCount != 0u) {
        if (!zr_oracle_bytes(f->valueCount, sizeof(*prepared.values), &bytes)) {
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                           f, 0u, 0u, 0u, UINT32_MAX, f->valueCount);
            return ZR_FALSE;
        }
        prepared.values = (SZrExecIrOracleValue *)calloc(1u, bytes);
        prepared.ownerStates = (TZrUInt32 *)calloc(f->valueCount, sizeof(*prepared.ownerStates));
        if (prepared.values == ZR_NULL || prepared.ownerStates == ZR_NULL) {
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                           f, 0u, 0u, 0u, f->valueCount, 0u);
            ZrCore_ExecIr_OracleResultFree(&prepared);
            return ZR_FALSE;
        }
        if (input->initialValueCount != 0u) {
            memcpy(prepared.values, input->initialValues,
                   (size_t)input->initialValueCount * sizeof(*prepared.values));
        }
        for (TZrUInt32 index = 0u; index < f->valueCount; ++index) {
            const SZrExecIrValue *value = &f->values[index];
            prepared.ownerStates[index] = (value->flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u
                    ? (value->ownership == ZR_EXEC_IR_OWNERSHIP_UNKNOWN
                        ? ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN : ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED)
                    : ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED;
            if ((value->flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) == 0u &&
                (value->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
                 value->ownership == ZR_EXEC_IR_OWNERSHIP_SHARED))
                zr_oracle_undefined(&prepared.values[index]);
        }
    }
    cursor.block = f->blockCount != 0u
            ? (f->entryBlockId != 0u ? f->entryBlockId : ZR_EXEC_IR_BLOCK_ID_ENTRY) : 0u;
    cursor.enterBlock = ZR_TRUE;
    if (!zr_oracle_run(input, &prepared, cursor, &stop, diagnostic)) {
        ZrCore_ExecIr_OracleResultFree(&prepared);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_OracleResultFree(result);
    *result = prepared;
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_ResumeOracleEx(const SZrExecIrOracleInput *input,
                                    SZrExecIrOracleExecutionResult *state,
                                    SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOracleExecutionResult prepared;
    SZrExecIrStateMapEntry stop;
    SZrOracleCursor cursor = {0};
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (input == ZR_NULL || state == ZR_NULL) {
        zr_oracle_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                       input != ZR_NULL ? input->function : ZR_NULL, 0u, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (!zr_oracle_input_valid(input, ZR_FALSE, diagnostic) ||
        !zr_oracle_prepare_resume(input, state, &prepared, &cursor, diagnostic)) return ZR_FALSE;
    if (!zr_oracle_stop_valid(input, &stop, diagnostic)) {
        ZrCore_ExecIr_OracleResultFree(&prepared);
        return ZR_FALSE;
    }
    /* Commit before running any provider. A later execution failure must not
     * leave a retryable copy of the previous effect boundary. */
    ZrCore_ExecIr_OracleResultFree(state);
    *state = prepared;
    return zr_oracle_run(input, state, cursor, &stop, diagnostic);
}

TZrBool ZrCore_ExecIr_RunOracle(const SZrExecIrFunction *function, SZrExecIrOracleResult *result, SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOracleInput input; SZrExecIrOracleExecutionResult execution; TZrBool ok;
    memset(&input, 0, sizeof(input)); memset(&execution, 0, sizeof(execution)); if (result != ZR_NULL) memset(result, 0, sizeof(*result)); input.function = function;
    ok = ZrCore_ExecIr_RunOracleEx(&input, &execution, diagnostic);
    if (result != ZR_NULL) { result->instructionCount = execution.instructionCount; result->supportedInstructionCount = execution.supportedInstructionCount; result->unsupportedInstructionId = execution.unsupportedInstructionId; }
    ZrCore_ExecIr_OracleResultFree(&execution); return ok;
}
