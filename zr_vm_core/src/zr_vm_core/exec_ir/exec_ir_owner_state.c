#include "zr_vm_core/exec_ir_owner_state.h"

#include <stdlib.h>
#include <string.h>

#define ZR_OWNER_BIT(state) ((TZrUInt8)(1u << (state)))
#define ZR_OWNER_UNINITIALIZED ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT)

static TZrBool zr_owner_fail(const SZrExecIrFunction *function,
                             SZrExecIrDiagnostic *diagnostic,
                             EZrExecutionDiagnosticCode code,
                             TZrUInt32 instruction, TZrUInt32 block) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
        diagnostic->instructionId = instruction;
        diagnostic->blockId = block;
        if (instruction != 0u) {
            diagnostic->sourceId = function->instructions[instruction - 1u].sourceId;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_owner_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

/* The materializer accepts external metadata without invoking the full effect
 * verifier. Validate every pool/range/ID used by this analysis independently. */
static TZrBool zr_owner_input_valid(const SZrExecIrFunction *function,
                                    SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt64 covered = 0u;
    if (function == ZR_NULL ||
        function->valueCount > function->valueCapacity ||
        function->instructionCount > function->instructionCapacity ||
        function->blockCount > function->blockCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->phiCount > function->phiCapacity ||
        function->phiIncomingCount > function->phiIncomingCapacity ||
        function->successorCount > function->successorCapacity ||
        (function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->operandCount != 0u && function->operands == ZR_NULL) ||
        (function->resultCount != 0u && function->results == ZR_NULL) ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL) ||
        (function->phiIncomingCount != 0u && function->phiIncoming == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL) ||
        (function->blockCount != 0u &&
         (function->entryBlockId == 0u || function->entryBlockId > function->blockCount))) {
        return zr_owner_fail(function, diagnostic,
                ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, 0u, 0u);
    }
    for (index = 0u; index < function->valueCount; ++index) {
        if (function->values[index].id != index + 1u ||
            (TZrUInt32)function->values[index].ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT) {
            return zr_owner_fail(function, diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, 0u, 0u);
        }
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(instruction->opcode);
        TZrUInt32 at;
        if (info == ZR_NULL ||
            !zr_owner_range_valid(instruction->operandRange, function->operandCount) ||
            !zr_owner_range_valid(instruction->resultRange, function->resultCount) ||
            instruction->operandRange.count < info->minimumOperands ||
            instruction->operandRange.count > info->maximumOperands ||
            (info->resultArity != ZR_EXEC_IR_VARIADIC &&
             instruction->resultRange.count != info->resultArity)) {
            return zr_owner_fail(function, diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, index + 1u, 0u);
        }
        for (at = instruction->operandRange.start;
             at < instruction->operandRange.start + instruction->operandRange.count; ++at) {
            if (function->operands[at] == 0u || function->operands[at] > function->valueCount) {
                return zr_owner_fail(function, diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, index + 1u, 0u);
            }
        }
        for (at = instruction->resultRange.start;
             at < instruction->resultRange.start + instruction->resultRange.count; ++at) {
            if (function->results[at] == 0u || function->results[at] > function->valueCount) {
                return zr_owner_fail(function, diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, index + 1u, 0u);
            }
        }
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        TZrUInt32 at;
        if (block->id != index + 1u ||
            !zr_owner_range_valid(block->instructionRange, function->instructionCount) ||
            !zr_owner_range_valid(block->successorRange, function->successorCount) ||
            !zr_owner_range_valid(block->phis, function->phiCount)) {
            return zr_owner_fail(function, diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, 0u, index + 1u);
        }
        covered += block->instructionRange.count;
        for (at = 0u; at < index; ++at) {
            SZrExecIrRange other = function->blocks[at].instructionRange;
            if (other.count != 0u && block->instructionRange.count != 0u &&
                other.start < block->instructionRange.start + block->instructionRange.count &&
                block->instructionRange.start < other.start + other.count) {
                return zr_owner_fail(function, diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, 0u, index + 1u);
            }
        }
        for (at = block->successorRange.start;
             at < block->successorRange.start + block->successorRange.count; ++at) {
            if (function->successors[at] == 0u || function->successors[at] > function->blockCount) {
                return zr_owner_fail(function, diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, 0u, index + 1u);
            }
        }
        for (at = block->phis.start; at < block->phis.start + block->phis.count; ++at) {
            const SZrExecIrPhi *phi = &function->phiPool[at];
            TZrUInt32 incoming;
            if (phi->result == 0u || phi->result > function->valueCount ||
                !zr_owner_range_valid(phi->incomings, function->phiIncomingCount)) {
                return zr_owner_fail(function, diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, 0u, index + 1u);
            }
            for (incoming = phi->incomings.start;
                 incoming < phi->incomings.start + phi->incomings.count; ++incoming) {
                const SZrExecIrPhiIncoming *input = &function->phiIncoming[incoming];
                if (input->value == 0u || input->value > function->valueCount ||
                    input->predecessor == 0u || input->predecessor > function->blockCount) {
                    return zr_owner_fail(function, diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, 0u, index + 1u);
                }
            }
        }
    }
    if (function->blockCount != 0u && covered != function->instructionCount) {
        return zr_owner_fail(function, diagnostic,
                ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, 0u, 0u);
    }
    return ZR_TRUE;
}

void ZrCore_ExecIr_OwnerAnalysisFree(SZrExecIrOwnerAnalysis *analysis) {
    if (analysis != ZR_NULL) {
        free(analysis->before);
        free(analysis->after);
        free(analysis->reachable);
        memset(analysis, 0, sizeof(*analysis));
    }
}

static TZrUInt8 zr_owner_initialized(const SZrExecIrFunction *function, TZrUInt32 valueIndex) {
    return function->values[valueIndex].ownership == ZR_EXEC_IR_OWNERSHIP_UNKNOWN
            ? ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN)
            : ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED);
}

static void zr_owner_define_results(const SZrExecIrFunction *function,
                                    SZrExecIrRange results, TZrUInt8 *row,
                                    TZrUInt8 invalidInputs, TZrBool canInitialize) {
    TZrUInt32 at;
    for (at = results.start; at < results.start + results.count; ++at) {
        TZrUInt32 valueIndex = function->results[at] - 1u;
        row[valueIndex] = invalidInputs;
        if (canInitialize) {
            row[valueIndex] |= zr_owner_initialized(function, valueIndex);
        }
    }
}

static void zr_owner_phi_edge(const SZrExecIrFunction *function,
                              TZrExecIrBlockId predecessor,
                              const SZrExecIrBlock *successor,
                              const TZrUInt8 *source, TZrUInt8 *destination) {
    TZrUInt32 index;
    TZrUInt8 available = (TZrUInt8)(ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED) |
                                   ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN));
    for (index = successor->phis.start;
         index < successor->phis.start + successor->phis.count; ++index) {
        const SZrExecIrPhi *phi = &function->phiPool[index];
        TZrUInt32 incoming;
        TZrUInt8 mask = 0u;
        for (incoming = phi->incomings.start;
             incoming < phi->incomings.start + phi->incomings.count; ++incoming) {
            const SZrExecIrPhiIncoming *input = &function->phiIncoming[incoming];
            if (input->predecessor == predecessor) {
                mask |= source[input->value - 1u];
            }
        }
        /* PHIs define fresh identities, but cannot restore consumed or
         * uninitialized inputs. Read every input from the same edge snapshot. */
        destination[phi->result - 1u] = (TZrUInt8)(mask & (TZrUInt8)~available);
        if ((mask & available) != 0u) {
            destination[phi->result - 1u] |= zr_owner_initialized(function, phi->result - 1u);
        } else if (mask == 0u) {
            destination[phi->result - 1u] = ZR_OWNER_UNINITIALIZED;
        }
    }
}

TZrBool ZrCore_ExecIr_OwnerAnalysisBuild(
        const SZrExecIrFunction *function, SZrExecIrOwnerAnalysis *analysis,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt8 *inputs = ZR_NULL;
    TZrUInt8 *blockReachable = ZR_NULL;
    TZrUInt8 *row = ZR_NULL;
    TZrUInt8 *edgeRow = ZR_NULL;
    TZrUInt8 *phiRow = ZR_NULL;
    TZrUInt32 blockCount, entry, index;
    size_t stride;
    TZrBool changed;
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (analysis == ZR_NULL) {
        return zr_owner_fail(function, diagnostic,
                ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u, 0u);
    }
    memset(analysis, 0, sizeof(*analysis));
    if (!zr_owner_input_valid(function, diagnostic)) {
        return ZR_FALSE;
    }
    analysis->valueCount = function->valueCount;
    analysis->instructionCount = function->instructionCount;
    if (function->instructionCount == 0u) {
        return ZR_TRUE;
    }
    blockCount = function->blockCount != 0u ? function->blockCount : 1u;
    entry = function->blockCount != 0u ? function->entryBlockId - 1u : 0u;
    stride = function->valueCount != 0u ? function->valueCount : 1u;
    if ((size_t)function->instructionCount > SIZE_MAX / stride ||
        (size_t)blockCount > SIZE_MAX / stride) {
        return zr_owner_fail(function, diagnostic,
                ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u, 0u);
    }
    analysis->before = (TZrUInt8 *)calloc(function->instructionCount, stride);
    analysis->after = (TZrUInt8 *)calloc(function->instructionCount, stride);
    analysis->reachable = (TZrUInt8 *)calloc(function->instructionCount, 1u);
    inputs = (TZrUInt8 *)calloc(blockCount, stride);
    blockReachable = (TZrUInt8 *)calloc(blockCount, 1u);
    row = (TZrUInt8 *)malloc(stride);
    edgeRow = (TZrUInt8 *)malloc(stride);
    phiRow = (TZrUInt8 *)malloc(stride);
    if (analysis->before == ZR_NULL || analysis->after == ZR_NULL ||
        analysis->reachable == ZR_NULL || inputs == ZR_NULL ||
        blockReachable == ZR_NULL || row == ZR_NULL || edgeRow == ZR_NULL || phiRow == ZR_NULL) {
        free(inputs); free(blockReachable); free(row); free(edgeRow); free(phiRow);
        ZrCore_ExecIr_OwnerAnalysisFree(analysis);
        return zr_owner_fail(function, diagnostic,
                ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u, 0u);
    }
    blockReachable[entry] = 1u;
    for (index = 0u; index < function->valueCount; ++index) {
        inputs[(size_t)entry * stride + index] =
                (function->values[index].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u
                        ? zr_owner_initialized(function, index) : ZR_OWNER_UNINITIALIZED;
    }
    do {
        TZrUInt32 blockIndex;
        changed = ZR_FALSE;
        for (blockIndex = 0u; blockIndex < blockCount; ++blockIndex) {
            const SZrExecIrBlock *block;
            SZrExecIrRange instructions = {0};
            if (!blockReachable[blockIndex]) {
                continue;
            }
            block = function->blockCount != 0u ? &function->blocks[blockIndex] : ZR_NULL;
            memcpy(row, inputs + (size_t)blockIndex * stride, stride);
            if (block != ZR_NULL) {
                instructions = block->instructionRange;
            } else {
                instructions.count = function->instructionCount;
            }
            for (index = instructions.start; index < instructions.start + instructions.count; ++index) {
                const SZrExecIrInstruction *instruction = &function->instructions[index];
                TZrUInt32 at;
                TZrUInt8 invalidInputs = 0u;
                TZrBool canInitialize = ZR_TRUE;
                TZrUInt8 available = (TZrUInt8)(ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED) |
                                               ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN));
                analysis->reachable[index] = 1u;
                memcpy(analysis->before + (size_t)index * stride, row, stride);
                /* A fresh result requires available operands. In particular,
                 * copying or moving an already consumed value cannot restore it. */
                for (at = instruction->operandRange.start;
                     at < instruction->operandRange.start + instruction->operandRange.count; ++at) {
                    TZrUInt8 mask = row[function->operands[at] - 1u];
                    if ((mask & (TZrUInt8)~available) != 0u || mask == 0u) {
                        invalidInputs = ZR_OWNER_UNINITIALIZED;
                    }
                    if ((mask & available) == 0u) {
                        canInitialize = ZR_FALSE;
                    }
                }
                if (instruction->opcode == ZR_EXEC_IR_OPCODE_MOVE ||
                    instruction->opcode == ZR_EXEC_IR_OPCODE_DROP) {
                    TZrUInt8 state = instruction->opcode == ZR_EXEC_IR_OPCODE_MOVE
                            ? ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_MOVED)
                            : ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED);
                    for (at = instruction->operandRange.start;
                         at < instruction->operandRange.start + instruction->operandRange.count; ++at) {
                        row[function->operands[at] - 1u] = state;
                    }
                }
                zr_owner_define_results(function, instruction->resultRange, row,
                                        invalidInputs, canInitialize);
                memcpy(analysis->after + (size_t)index * stride, row, stride);
            }
            if (block != ZR_NULL) {
                const SZrExecIrInstruction *terminator = instructions.count != 0u
                        ? &function->instructions[instructions.start + instructions.count - 1u] : ZR_NULL;
                const SZrExecIrOpcodeInfo *info = terminator != ZR_NULL
                        ? ZrCore_ExecIr_OpcodeInfo(terminator->opcode) : ZR_NULL;
                for (index = block->successorRange.start;
                     index < block->successorRange.start + block->successorRange.count; ++index) {
                    TZrUInt32 successor = function->successors[index] - 1u;
                    TZrUInt8 *input = inputs + (size_t)successor * stride;
                    size_t at;
                    memcpy(edgeRow, row, stride);
                    if (info != ZR_NULL &&
                        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u &&
                        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) != 0u &&
                        index != block->successorRange.start) {
                        zr_owner_define_results(function, terminator->resultRange, edgeRow,
                                                ZR_OWNER_UNINITIALIZED, ZR_FALSE);
                    }
                    memcpy(phiRow, edgeRow, stride);
                    zr_owner_phi_edge(function, block->id,
                                      &function->blocks[successor], edgeRow, phiRow);
                    if (!blockReachable[successor]) {
                        blockReachable[successor] = 1u;
                        changed = ZR_TRUE;
                    }
                    for (at = 0u; at < stride; ++at) {
                        TZrUInt8 combined = (TZrUInt8)(input[at] | phiRow[at]);
                        if (input[at] != combined) {
                            input[at] = combined;
                            changed = ZR_TRUE;
                        }
                    }
                }
            }
        }
    } while (changed);
    free(inputs); free(blockReachable); free(row); free(edgeRow); free(phiRow);
    return ZR_TRUE;
}

EZrExecIrStateMapOwnerState ZrCore_ExecIr_OwnerStateAt(
        const SZrExecIrOwnerAnalysis *analysis, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase) {
    const TZrUInt8 *rows;
    TZrUInt8 mask;
    TZrUInt32 state;
    if (analysis == ZR_NULL || instruction == 0u || instruction > analysis->instructionCount ||
        value == 0u || value > analysis->valueCount ||
        (TZrUInt32)phase >= ZR_EXEC_IR_STATE_PHASE_COUNT ||
        analysis->reachable == ZR_NULL || !analysis->reachable[instruction - 1u]) {
        return ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT;
    }
    rows = phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT ? analysis->before : analysis->after;
    if (rows == ZR_NULL) {
        return ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT;
    }
    mask = rows[(size_t)(instruction - 1u) * analysis->valueCount + value - 1u];
    for (state = 0u; state < ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT; ++state) {
        if (mask == ZR_OWNER_BIT(state)) {
            return (EZrExecIrStateMapOwnerState)state;
        }
    }
    return ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT;
}
