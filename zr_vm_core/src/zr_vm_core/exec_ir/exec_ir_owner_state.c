#include "zr_vm_core/exec_ir_owner_state.h"

#include <stdlib.h>
#include <string.h>

#define ZR_OWNER_BIT(state) ((TZrUInt8)(1u << (state)))
#define ZR_OWNER_UNINITIALIZED ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED)

static TZrBool zr_owner_has_definition(const SZrExecIrFunction *function,
                                       TZrExecIrValueId value) {
    TZrUInt32 index, at;
    if ((function->values[value - 1u].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u)
        return ZR_TRUE;
    if (function->resultCount > function->resultCapacity ||
        function->phiCount > function->phiCapacity ||
        (function->resultCount != 0u && function->results == ZR_NULL) ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL)) return ZR_FALSE;
    for (index = 0u; index < function->instructionCount; ++index) {
        SZrExecIrRange range = function->instructions[index].resultRange;
        if (range.start > function->resultCount || range.count > function->resultCount - range.start)
            return ZR_FALSE;
        for (at = range.start; at < range.start + range.count; ++at)
            if (function->results[at] == value) return ZR_TRUE;
    }
    for (index = 0u; index < function->blockCount; ++index) {
        SZrExecIrRange range = function->blocks[index].phis;
        if (range.start > function->phiCount || range.count > function->phiCount - range.start)
            return ZR_FALSE;
        for (at = range.start; at < range.start + range.count; ++at)
            if (function->phiPool[at].result == value) return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool ZrCore_ExecIr_ConditionalCleanupValid(
        const SZrExecIrFunction *function, TZrExecIrInstructionId id) {
    const SZrExecIrInstruction *instruction;
    TZrUInt32 index;
    TZrExecIrValueId value;
    EZrExecIrOwnership ownership;
    if (function == ZR_NULL || id == 0u || id > function->instructionCount ||
        function->instructionCount > function->instructionCapacity ||
        function->operandCount > function->operandCapacity ||
        function->blockCount > function->blockCapacity ||
        function->valueCount > function->valueCapacity ||
        function->instructions == ZR_NULL || function->operands == ZR_NULL ||
        function->values == ZR_NULL || function->blocks == ZR_NULL) return ZR_FALSE;
    instruction = &function->instructions[id - 1u];
    if (instruction->opcode != ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED ||
        instruction->operandRange.count != 1u || instruction->resultRange.count != 0u ||
        instruction->operandRange.start >= function->operandCount) return ZR_FALSE;
    value = function->operands[instruction->operandRange.start];
    if (value == 0u || value > function->valueCount) return ZR_FALSE;
    ownership = function->values[value - 1u].ownership;
    if (ownership != ZR_EXEC_IR_OWNERSHIP_UNIQUE &&
        ownership != ZR_EXEC_IR_OWNERSHIP_SHARED) return ZR_FALSE;
    /* A declared ID alone does not create a flag. Require an actual entry,
     * instruction or PHI definition even when payload dominance is waived. */
    if (!zr_owner_has_definition(function, value)) return ZR_FALSE;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (id - 1u >= block->instructionRange.start &&
            id - 1u - block->instructionRange.start < block->instructionRange.count)
            return (TZrBool)((block->flags & ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) != 0u);
    }
    return ZR_FALSE;
}

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
            (instruction->opcode == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED &&
             !ZrCore_ExecIr_ConditionalCleanupValid(function, index + 1u)) ||
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
                if (instruction->opcode == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED) {
                    TZrUInt8 *state = &row[function->operands[instruction->operandRange.start] - 1u];
                    if ((*state & ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED)) != 0u) {
                        *state = (TZrUInt8)((*state & (TZrUInt8)~ZR_OWNER_BIT(
                                ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED)) |
                                ZR_OWNER_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED));
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

TZrUInt8 ZrCore_ExecIr_OwnerStateMaskAt(
        const SZrExecIrOwnerAnalysis *analysis, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase) {
    const TZrUInt8 *rows;
    if (analysis == ZR_NULL || instruction == 0u || instruction > analysis->instructionCount ||
        value == 0u || value > analysis->valueCount ||
        (TZrUInt32)phase >= ZR_EXEC_IR_STATE_PHASE_COUNT ||
        analysis->reachable == ZR_NULL || !analysis->reachable[instruction - 1u]) {
        return 0u;
    }
    rows = phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT ? analysis->before : analysis->after;
    if (rows == ZR_NULL) {
        return 0u;
    }
    return rows[(size_t)(instruction - 1u) * analysis->valueCount + value - 1u];
}

EZrExecIrStateMapOwnerState ZrCore_ExecIr_OwnerStateAt(
        const SZrExecIrOwnerAnalysis *analysis, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase) {
    TZrUInt8 mask = ZrCore_ExecIr_OwnerStateMaskAt(analysis, instruction, value, phase);
    TZrUInt32 state;
    for (state = 0u; state < ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL; ++state) {
        if (mask == ZR_OWNER_BIT(state)) {
            return (EZrExecIrStateMapOwnerState)state;
        }
    }
    return mask != 0u ? ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL
                      : ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT;
}

/**
 * @def ZR_OWNER_BIT(state)
 *
 * ## Summary
 * 为分析内部的 owner may-state 集合生成单个位，位序与公开 owner 枚举对应。此宏只做整数表达式转换，不读取函数或 analysis。
 *
 * ## Parameters
 * @param[in] state 已知具体 owner 状态的整数表达式；本模块使用 UNKNOWN、INITIALIZED、MOVED、DROPPED、UNINITIALIZED。只求值一次，不在宏内检查取值。
 *
 * ## Returns
 * @return 表达式结果为 TZrUInt8 的单个位掩码；不表示实际运行时 payload 或 root。
 *
 * ## Constraints and lifecycle
 * state 表达式只出现一次。调用方须传入已知具体 owner 状态；CONDITIONAL/COUNT 不是具体状态位的查询结果，宏不会拒绝它们。
 * 移位先发生在 unsigned int 上，再缩窄到 TZrUInt8；不得以负值或超过 unsigned int 位宽的移位量调用，也不能靠末尾强制转换补救非法移位。
 * 宏本体不分配、释放或同步；外部传入表达式本身的副作用由调用方负责。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:6
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:345
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:446
 */

/**
 * @def ZR_OWNER_UNINITIALIZED
 *
 * ## Summary
 * 为分析中的“尚未初始化”路径命名单个位掩码，避免把枚举值与 may-state 位集合混用。
 *
 * ## Returns
 * 展开为 UNINITIALIZED 对应的 TZrUInt8 位编码；没有函数返回或失败通道。
 *
 * ## Constraints and lifecycle
 * 这是本 TU 的固定表达式别名，不分配存储。它既可作为入口的未定义值状态，也可作为不相容输入、异常结果或无 PHI 输入的保守状态。
 * 该位描述静态路径可能性；不能据此判断实际 payload 是否存在或执行释放。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:7
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:262
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:322
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:355
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:399
 */

/**
 * @fn static TZrBool zr_owner_has_definition(const SZrExecIrFunction *function, TZrExecIrValueId value)
 *
 * ## Summary
 * 为 guarded cleanup 的形状检查确认某个 ID 具有可识别定义。EXTERNAL_ENTRY 标志可直接满足；否则扫描指令 resultRange 和 block 的 PHI 结果。
 *
 * ## Parameters
 * @param[in] function 借用的函数记录及实际被扫描的值、指令、block、result、PHI 池；本内部调用中记录非空且基础形状已由条件清理入口检查。
 * @param[in] value 已验证的 1 起始 ValueId，须落在 function->valueCount 内。
 *
 * ## Returns
 * @return 找到一种定义返回 ZR_TRUE；未找到，或扫描所需 result/PHI 的形状与范围无效，返回 ZR_FALSE；不写诊断。
 *
 * ## Constraints and lifecycle
 * function、value 及基础 values/instructions/blocks 存储的有效性由唯一调用方先保证。这里会自行检查 result/PHI 数量、非空池及被扫描范围，但不证明地址可读性或分配来源。
 * 只要 external-entry、任意指令或 PHI 声明了该结果即可，不检查定义是否可达或支配 cleanup；此项不能替代后续结构、SSA 或 owner 分析。
 * 同步借用输入，不分配、不保存指针，不修改函数。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:12
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:14
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:19
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:26
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:59
 */

/**
 * @fn TZrBool ZrCore_ExecIr_ConditionalCleanupValid(const SZrExecIrFunction *function, TZrExecIrInstructionId id)
 *
 * ## Summary
 * 供结构验证、owner 输入检查、Parser cleanup elaboration 及 Oracle 共享 guarded DROP 的局部形状判定。只接受 DROP_IF_INITIALIZED 的一个有效 UNIQUE/SHARED owner 操作数、零结果和包含该指令的 cleanup block。
 *
 * ## Parameters
 * @param[in] function 可为 NULL，NULL 返回 ZR_FALSE；非空时记录及被读取的池须真实有效且在调用期间稳定。
 * @param[in] id 1 起始指令 ID；零、越界或不能满足 guarded cleanup 形状时返回 ZR_FALSE。
 *
 * ## Returns
 * @return 所列局部形状及可识别定义均成立返回 ZR_TRUE；NULL、无效 ID、所需基础池/数量、opcode/arity、owner、定义或 block 条件不满足时返回 ZR_FALSE；没有诊断输出。
 *
 * ## Constraints and lifecycle
 * 基础指令、operand、value、block 数量/容量和指针先检查；若需要扫描定义，再由内部 helper 检查相关 result/PHI 范围。形状检查不证明实际地址可读或一般所有权安全。
 * 定义只要求可识别 external-entry、指令结果或 PHI；不要求该定义支配 cleanup，也不验证具体 payload。这是 guarded cleanup 可以处理分支未初始化 owner 的局部条件，不是普通使用放宽。
 * 按 block 数组顺序返回第一个包含该指令的 block 的 CLEANUP 标志；完整 block 范围互斥、覆盖、效果和 SSA 验证由各自调用层承担。
 * 不分配、不修改或保活输入，无内部同步；不能把 ZR_TRUE 当作完整函数或运行期安全认证。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:41
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:49
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:53
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:59
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:60
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:131
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c:47
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:633
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c:256
 */

/**
 * @fn static TZrBool zr_owner_fail(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, TZrUInt32 instruction, TZrUInt32 block)
 *
 * ## Summary
 * 为 owner 输入检查及 Build 的失败分支填写一份重新清零的诊断，并提供可直接 return 的 ZR_FALSE。
 *
 * ## Parameters
 * @param[in] function 可为 NULL 以输出 token 0；instruction 非零时必须是可读取该指令的有效函数。
 * @param[out] diagnostic 可为 NULL；否则为独立可写记录，先被清零再写入 code、token、指令、block 及适用 sourceId。
 * @param[in] code 要发布的失败诊断码。
 * @param[in] instruction 零表示没有指令定位；非零时调用方保证 instructions[instruction - 1] 可读。
 * @param[in] block 直接记录的 block ID，本 helper 不验证它。
 *
 * ## Returns
 * @return 始终返回 ZR_FALSE；diagnostic 为 NULL 时仅返回，不写任何诊断。
 *
 * ## Constraints and lifecycle
 * diagnostic 不得覆盖 function、被读取的指令或调用方仍使用的输出/输入，否则清零和定位读取会破坏这些对象。
 * 此 helper 不校验非零 instruction 的边界；唯一调用路径在请求指令定位之前已经验证对应指令池与当前下标。block 仅作为诊断值复制。
 * 不分配、不释放 analysis，也不恢复其他输出；清理责任仍在相应失败分支。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:73
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:79
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:138
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:282
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:315
 */

/**
 * @fn static TZrBool zr_owner_range_valid(SZrExecIrRange range, TZrUInt32 count)
 *
 * ## Summary
 * 检查一个逻辑范围能否放入给定元素数量。先限制 start，再比较 count - start，避免为判断范围而先计算可能溢出的 start + range.count。
 *
 * ## Parameters
 * @param[in] range 按值传入的零起始 start/count 范围。
 * @param[in] count 目标池的逻辑元素数量。
 *
 * ## Returns
 * @return range.start <= count 且 range.count <= count - range.start 时返回 ZR_TRUE，否则返回 ZR_FALSE；空范围可位于逻辑末尾。
 *
 * ## Constraints and lifecycle
 * 只处理按值整数，不读取池指针，不检查存储容量、分配来源或物理可读性。通过后，调用方才能在同一 count 与稳定元数据下使用范围。
 * 没有分配、诊断输出、所有权转移或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:87
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:132
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:160
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:187
 */

/**
 * @fn static TZrBool zr_owner_input_valid(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在 materializer 不先运行完整 effect verifier 的路径上，独立检查 owner 分析会读取的函数数量、池、范围、ValueId、opcode arity、block 划分、successor 与 PHI 输入。
 *
 * ## Parameters
 * @param[in] function 可为 NULL 并被拒绝；非空记录和实际读取的嵌套池须稳定且真实可读。
 * @param[out] diagnostic 可为 NULL；否则是与输入分离的可写记录，在拒绝时通过 zr_owner_fail 填写 STATE_MAP_INVALID。
 *
 * ## Returns
 * @return 所实现的输入检查通过返回 ZR_TRUE；任一步拒绝返回 ZR_FALSE，并在非空 diagnostic 中写 STATE_MAP_INVALID 及适用的指令/block 定位。
 *
 * ## Constraints and lifecycle
 * 数量/容量、非空池、entry block 和连续 ValueId/ownership 枚举先检查；逐指令检查 opcode、guarded cleanup、operand/result 范围及 arity，再检查各 operand/result ID。
 * block 的指令区间须有效、非重叠；存在 block 时其总覆盖数须等于 instructionCount。successor ID、PHI result、incoming 范围与 incoming 的 value/predecessor ID 也在这里检查。零 block 的线性函数路径被允许。
 * 检查的目标是本分析的安全索引前提；不验证实际分配/可读性、完整前驱/支配关系、effect/memory 协议、nullability/type、constant/deopt 表或所有未使用池。ZR_TRUE 不是 VerifyFunction(ALL) 的替代。
 * 不分配或修改 function；失败诊断会清零后重写，因此 diagnostic 必须独立。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:96
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:118
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:125
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:156
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:166
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:183
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:202
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:286
 */

/**
 * @fn void ZrCore_ExecIr_OwnerAnalysisFree(SZrExecIrOwnerAnalysis *analysis)
 *
 * ## Summary
 * 释放临时 owner 分析的 before、after 和 reachable 三个池，并将非空记录的所有字段清零。
 *
 * ## Parameters
 * @param[in,out] analysis 可为 NULL；非空时须为零初始化的记录，或独占拥有 before、after、reachable 三个可分别由 C free 释放的分配。
 *
 * ## Returns
 * 无 C 返回值；NULL 参数不产生操作，正常完成后非空记录处于可再次 Free 或用于 Build 的空状态。
 *
 * ## Constraints and lifecycle
 * 非空池必须互不重叠、具有独立且唯一的释放所有权，并兼容 C free；analysis 记录的存储也须与池分离。函数不验证分配来源、别名或记录是否合法。
 * 调用期间须独占记录和各旧池的修改/释放权。完成后旧池及其元素的借用指针失效，analysis 记录本身仍存在且为空；不释放源 function 或运行时值。
 * 对已经清零的记录重复调用安全；将未经初始化的任意字节记录传入不受支持。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:211
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:214
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:314
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:419
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:385
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:410
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:656
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:660
 */

/**
 * @fn static TZrUInt8 zr_owner_initialized(const SZrExecIrFunction *function, TZrUInt32 valueIndex)
 *
 * ## Summary
 * 为可用的 value 产生一个具体 owner 状态位：ownership 为 UNKNOWN 时使用 UNKNOWN，否则使用 INITIALIZED。
 *
 * ## Parameters
 * @param[in] function 已通过 zr_owner_input_valid 的稳定函数，values[valueIndex] 真实可读。
 * @param[in] valueIndex 从零开始且小于 function->valueCount 的 value 池下标。
 *
 * ## Returns
 * @return 返回对应状态位的 TZrUInt8 掩码。
 *
 * ## Constraints and lifecycle
 * 函数不分配、不修改输入、不填写诊断，也不自行验证 function、池、下标或 ownership 枚举；唯一上层 Build 的输入检查提供这些访问前提。
 * 返回值只表示静态分析分类，不读取或证明运行时 payload 已初始化，也不延长任何对象生命周期。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:219
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:232
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:260
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:322
 */

/**
 * @fn static void zr_owner_define_results(const SZrExecIrFunction *function, SZrExecIrRange results, TZrUInt8 *row, TZrUInt8 invalidInputs, TZrBool canInitialize)
 *
 * ## Summary
 * 重写结果 ValueId 对应的状态槽，将输入失效信息与结果自身的可用状态合成；异常 successor 路径可用同一入口仅写 UNINITIALIZED。
 *
 * ## Parameters
 * @param[in] function 输入检查已通过的函数，results 范围内各 ValueId 对应有效 row 下标。
 * @param[in] results 已验证的 result 池范围；本函数不重复检查范围。
 * @param[in,out] row 可写且与输入元数据分离的当前状态行，至少覆盖所有 value 槽。
 * @param[in] invalidInputs 先写入每个结果槽的失效状态位；Build 传入 0 或 UNINITIALIZED 位。
 * @param[in] canInitialize 非零时在失效位之外加入结果自身 ownership 对应的可用状态位。
 *
 * ## Returns
 * 无 C 返回值；正常完成后仅 results 列出的状态槽被重写，不分配资源或传达失败码。
 *
 * ## Constraints and lifecycle
 * Build 已验证范围加法、result 池和 ValueId；这里直接遍历与索引，不提供独立输入拒绝路径。row 须稳定可写、足够大，且不得覆盖 function、结果池或其他被读元数据。
 * 正常指令路径使用消费前计算的 invalidInputs/canInitialize，因此 MOVE 先标记旧身份后，新结果仍按输入在消费前是否可用决定状态；不可借此恢复已消费或未初始化输入。
 * 本函数不分配、不持有传入指针，也不检查运行时 payload。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:228
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:230
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:232
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:379
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:398
 */

/**
 * @fn static void zr_owner_phi_edge(const SZrExecIrFunction *function, TZrExecIrBlockId predecessor, const SZrExecIrBlock *successor, const TZrUInt8 *source, TZrUInt8 *destination)
 *
 * ## Summary
 * 按 predecessor 合并每个 successor PHI 的 incoming 状态，并为 PHI result 产生新身份；多个 PHI 共享同一只读边快照。
 *
 * ## Parameters
 * @param[in] function 输入检查已通过的函数，PHI 和 incoming 池及 ValueId 均有效。
 * @param[in] predecessor 当前被传播边的前驱 block 标识，用于选择 matching incoming。
 * @param[in] successor 该边的目标 block；其 PHI 范围及关联存储须稳定可读。
 * @param[in] source 传播前边快照，至少包含 valueCount 个状态槽，须与 destination 不重叠。
 * @param[in,out] destination 独立可写的目标状态行；调用方已复制边快照，本函数只重写 successor 的 PHI result 槽。
 *
 * ## Returns
 * 无 C 返回值；正常完成后 PHI result 槽被重写，其他目标槽保持原值，无分配或失败码。
 *
 * ## Constraints and lifecycle
 * source 与 destination 必须是独立、不重叠的行，且二者不能覆盖函数元数据。Build 分别使用 edgeRow 和 phiRow，避免前面 PHI 的写入被后面 PHI 误当成原始 incoming。
 * 可用位 INITIALIZED/UNKNOWN 被转为结果 ownership 对应的可用位；MOVED/DROPPED/UNINITIALIZED 等失效位保留。没有 matching incoming 而得到零掩码时，结果被设为 UNINITIALIZED。
 * 输入检查保证范围和 ID 可索引，但本函数不证明 PHI 前驱集合完整、支配关系或 CFG 语义正确。所有输入仅在调用内借用。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:249
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:253
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:258
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:262
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:393
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:401
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:402
 */

/**
 * @fn TZrBool ZrCore_ExecIr_OwnerAnalysisBuild(const SZrExecIrFunction *function, SZrExecIrOwnerAnalysis *analysis, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为函数建立按指令、ValueId 索引的 before/after owner 状态位集及可达标记，使用 CFG successor、PHI 与消费/定义规则反复合并至不再变化。
 *
 * ## Parameters
 * @param[in] function 可为 NULL 并被拒绝；非空记录与分析会访问的各池须真实可读、稳定，调用期间不得修改或释放。
 * @param[out] analysis 必须为独立可写的空记录，或已先用 OwnerAnalysisFree 释放旧池的记录；不可覆盖 function 或其池。
 * @param[out] diagnostic 可为 NULL；否则是独立可写的诊断记录，不得与 function、任一输入池、analysis 或其池重叠。
 *
 * ## Returns
 * @return 建立成功返回 ZR_TRUE；拒绝输入、尺寸溢出或分配失败返回 ZR_FALSE，并在非空 diagnostic 中写相应诊断。零指令的合法输入返回 ZR_TRUE，不分配三个池，但仍保存 valueCount/instructionCount。
 *
 * ## Constraints and lifecycle
 * 入口先清零 diagnostic，随后将非空 analysis 直接清零，不释放其原池。因此必须传空对象，或先 Free；本入口不提供对旧非空 analysis 的事务替换。所有输出和诊断须与输入及彼此分离，调用方负责排除并发修改/释放。
 * analysis 为 NULL 时报告 INVALID_ARGUMENT；输入子集检查失败时非空 analysis 全零。通过检查后先写两个 counts；尺寸乘法溢出返回 CAPACITY_OVERFLOW，此时 counts 保留而三个池为 NULL。任意一个分配失败则释放全部五个临时池及已经取得的三个输出池，并将 analysis 全零后报告 OUT_OF_MEMORY。
 * 入口 external-entry value 按 ownership 初始化，其余 value 为 UNINITIALIZED。MOVE/DROP 消费旧身份；guarded DROP 只把 INITIALIZED 位改为 DROPPED，其余位保留。新结果依消费前输入状态形成；PHI 从独立边快照传播，不能恢复已消费输入。
 * 对 MAY_THROW 且 TERMINATOR 的末指令，successor 数组中首项保留正常结果，其余项把该指令结果设为 UNINITIALIZED；这里按数组顺序处理，不根据 block 标志再选优先级。合并保留所有 CFG 路径，不执行常量分支裁剪。
 * 成功时释放五个临时池，analysis 独占三个堆池；由 OwnerAnalysisFree 回收。池只保存静态状态位，不持有 function 指针、不存 token/generation，也不拥有运行时 payload 或 GC 保活对象。结果应用于 IR 时须由调用方保证仍对应同一版本；查询不会检测 IR 已变。
 * 成功只说明本输入检查与分析完成，不证明完整 effect/SSA/CFG 协议或没有非法消费；不可将其替代 VerifyFunction 或语义 liveness/state-map 验证。函数自身无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:278
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:285
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:289
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:297
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:302
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:310
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:318
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:349
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:371
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:394
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:401
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:408
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:419
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:384
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:385
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:611
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:656
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:660
 * - tests/parser/test_ssa_state_map_ownership.c:311
 * - tests/parser/ssa_owner_fault_allocator.c:54
 */

/**
 * @fn TZrUInt8 ZrCore_ExecIr_OwnerStateMaskAt(const SZrExecIrOwnerAnalysis *analysis, TZrExecIrInstructionId instruction, TZrExecIrValueId value, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 读取分析中一个指令、ValueId 和 phase 对应的原始 owner 状态位集，供 conditional cleanup 与语义 liveness 层继续解释。
 *
 * ## Parameters
 * @param[in] analysis 可为 NULL；非空须为合法空对象或 Build 产生且未释放的稳定分析记录及真实池。
 * @param[in] instruction 从 1 开始的指令标识。
 * @param[in] value 从 1 开始且对应分析版本的 ValueId。
 * @param[in] phase 有效 phase；BEFORE_EFFECT 读 before，其他有效 phase 读 after。
 *
 * ## Returns
 * @return 参数边界不合法、指令不可达、reachable 或所选行池为 NULL 时返回 0；否则返回存储的 TZrUInt8 掩码，不填写诊断。
 *
 * ## Constraints and lifecycle
 * BEFORE_EFFECT 选择 before；AFTER_EFFECT 和 CLEANUP_COMPLETE 等其余有效 phase 均选择 after，没有单独 cleanup 行。
 * 只检查记录中 counts、phase、可达标记及所选指针是否为空；不验证实际池大小、分配来源、掩码内容或 IR token/generation。伪造记录或悬空指针不因这些检查而安全。
 * 仅同步借用记录与池，不分配或修改。与 Free/Build 并发不安全；旧池被释放后不能继续查询。输出为整数掩码，不是指针或运行时 payload。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:427
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:433
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:437
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:443
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c:60
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:256
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:272
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:286
 */

/**
 * @fn EZrExecIrStateMapOwnerState ZrCore_ExecIr_OwnerStateAt(const SZrExecIrOwnerAnalysis *analysis, TZrExecIrInstructionId instruction, TZrExecIrValueId value, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 在 OwnerStateMaskAt 的原始位集之上，给出单个具体状态或混合状态标识。
 *
 * ## Parameters
 * @param[in] analysis 可为 NULL；非空须为合法空对象或 Build 产生且未释放的稳定分析记录及真实池。
 * @param[in] instruction 从 1 开始的指令标识。
 * @param[in] value 从 1 开始且对应分析版本的 ValueId。
 * @param[in] phase 有效 phase；BEFORE_EFFECT 读 before，其他有效 phase 读 after。
 *
 * ## Returns
 * @return 恰好匹配 UNKNOWN、INITIALIZED、MOVED、DROPPED、UNINITIALIZED 中某个具体状态位时返回该状态；零掩码返回 OWNER_STATE_COUNT，其他非零掩码返回 OWNER_CONDITIONAL。
 *
 * ## Constraints and lifecycle
 * 输入、边界检查、phase 选行和借用期遵循 OwnerStateMaskAt；本函数不增加分配、诊断、版本校验或同步。
 * COUNT 同时用于不可达、查询无效或零掩码，不能据此区分失败原因。CONDITIONAL 只是原始状态混合，不等同 StateMapOwnerAt 经语义 liveness 判定后的可序列化 owner 状态。
 * 分析记录和池在调用内须保持可读且不被 Build/Free 修改；返回枚举不保有任何对象。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:443
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:445
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:450
 * - tests/parser/test_ssa_gvn_range.c:289
 * - tests/parser/test_ssa_gvn_range.c:291
 * - tests/parser/test_ssa_gvn_range.c:293
 * - tests/parser/test_ssa_gvn_range.c:296
 * - tests/parser/test_ssa_sccp_copy_availability.c:141
 * - tests/parser/test_ssa_sccp_copy_availability.c:143
 * - tests/parser/test_ssa_sccp_copy_availability.c:145
 * - tests/parser/test_ssa_sccp_copy_availability.c:147
 * - tests/parser/ssa_gvn_owned_value_cases.inc:150
 * - tests/parser/ssa_gvn_owned_value_cases.inc:175
 * - tests/parser/ssa_gvn_owned_value_cases.inc:177
 */
