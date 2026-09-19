#include "zr_vm_parser/exec_ir_gvn.h"

#include <stdlib.h>
#include <string.h>

static TZrBool range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrUInt32 containing_block(const SZrExecIrFunction *function,
                                  TZrUInt32 instructionIndex) {
    TZrUInt32 index;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (instructionIndex >= block->instructionRange.start &&
            instructionIndex - block->instructionRange.start <
                    block->instructionRange.count) {
            return block->id;
        }
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static TZrBool pure_instruction(const SZrExecIrFunction *function,
                                const SZrExecIrInstruction *instruction) {
    const SZrExecIrOpcodeInfo *info =
            ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
    if (info == ZR_NULL || info->effects != 0u || instruction->memoryIn.count != 0u ||
        instruction->memoryOut.count != 0u || instruction->effectIn != 0u ||
        instruction->effectOut != 0u ||
        (instruction->flags & (ZR_EXEC_IR_FLAG_MAY_ALLOCATE |
                               ZR_EXEC_IR_FLAG_MAY_THROW |
                               ZR_EXEC_IR_FLAG_MAY_GC |
                               ZR_EXEC_IR_FLAG_MAY_SUSPEND)) != 0u) {
        (void)function;
        return ZR_FALSE;
    }
    switch ((EZrExecIrOpcode)instruction->opcode) {
        case ZR_EXEC_IR_OPCODE_CONSTANT:
        case ZR_EXEC_IR_OPCODE_CONVERT:
        case ZR_EXEC_IR_OPCODE_TYPE_TEST:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC:
        case ZR_EXEC_IR_OPCODE_COPY:
        case ZR_EXEC_IR_OPCODE_MOVE:
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_SUB:
        case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_NEG:
        case ZR_EXEC_IR_OPCODE_COMPARE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool same_key(const SZrExecIrFunction *function,
                        const SZrExecIrInstruction *left,
                        const SZrExecIrInstruction *right) {
    TZrUInt32 index;
    if (!range_valid(left->operandRange, function->operandCount) ||
        !range_valid(right->operandRange, function->operandCount) ||
        !range_valid(left->resultRange, function->resultCount) ||
        !range_valid(right->resultRange, function->resultCount)) {
        return ZR_FALSE;
    }
    if (left->opcode != right->opcode || left->flags != right->flags ||
        left->typeToken != right->typeToken ||
        left->matchTypeToken != right->matchTypeToken ||
        left->layoutId != right->layoutId ||
        left->operandRange.count != right->operandRange.count ||
        left->resultRange.count != 1u || right->resultRange.count != 1u) {
        return ZR_FALSE;
    }
    for (index = 0u; index < left->operandRange.count; ++index) {
        if (function->operandPool[left->operandRange.start + index] !=
            function->operandPool[right->operandRange.start + index]) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static void emit_remark(SZrExecIrRemarkSink *remarks,
                        const SZrExecIrInstruction *instruction,
                        EZrExecIrRemarkOutcome outcome,
                        TZrUInt32 reason) {
    SZrExecIrOptimizationRemark *items;
    TZrUInt32 capacity;
    if (remarks == ZR_NULL) {
        return;
    }
    if (remarks->count == remarks->capacity) {
        if (remarks->capacity > UINT32_MAX / 2u) {
            return;
        }
        capacity = remarks->capacity == 0u ? 8u : remarks->capacity * 2u;
        items = (SZrExecIrOptimizationRemark *)realloc(
                remarks->items, (size_t)capacity * sizeof(*items));
        if (items == ZR_NULL) {
            return;
        }
        remarks->items = items;
        remarks->capacity = capacity;
    }
    memset(&remarks->items[remarks->count], 0, sizeof(*remarks->items));
    remarks->items[remarks->count].pass = "gvn";
    remarks->items[remarks->count].sourceId = instruction->sourceId;
    remarks->items[remarks->count].outcome = outcome;
    remarks->items[remarks->count].reasonCode = reason;
    ++remarks->count;
}

TZrBool ZrParser_ExecIr_RunGvnCse(
        SZrExecIrFunction *function,
        const SZrExecIrAnalysisFacts *facts,
        SZrExecIrRemarkSink *remarks,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    (void)facts;
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (function == ZR_NULL ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->operandCount != 0u && function->operandPool == ZR_NULL) ||
        (function->resultCount != 0u && function->resultPool == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        function->instructionCount > function->instructionCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->blockCount > function->blockCapacity) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    for (index = 0u; index < function->blockCount; ++index) {
        if (!range_valid(function->blocks[index].instructionRange,
                         function->instructionCount)) {
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                diagnostic->blockId = function->blocks[index].id;
            }
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        SZrExecIrInstruction *current = &function->instructions[index];
        TZrUInt32 previous;
        TZrUInt32 currentBlock;
        if (!range_valid(current->operandRange, function->operandCount) ||
            !range_valid(current->resultRange, function->resultCount) ||
            !pure_instruction(function, current) || current->resultRange.count != 1u) {
            continue;
        }
        currentBlock = containing_block(function, index);
        if (currentBlock == ZR_EXEC_IR_BLOCK_ID_INVALID) {
            continue;
        }
        for (previous = 0u; previous < index; ++previous) {
            SZrExecIrInstruction *candidate = &function->instructions[previous];
            TZrExecIrValueId oldValue;
            TZrExecIrValueId newValue;
            if (containing_block(function, previous) != currentBlock ||
                !pure_instruction(function, candidate) ||
                !same_key(function, candidate, current)) {
                continue;
            }
            oldValue = function->resultPool[current->resultRange.start];
            newValue = function->resultPool[candidate->resultRange.start];
            if (oldValue == 0u || newValue == 0u || oldValue == newValue) {
                continue;
            }
            if (function->operandCount == function->operandCapacity) {
                if (function->operandCapacity > UINT32_MAX / 2u) {
                    if (diagnostic != ZR_NULL) {
                        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
                        diagnostic->instructionId = index + 1u;
                        diagnostic->sourceId = current->sourceId;
                    }
                    return ZR_FALSE;
                }
                TZrUInt32 capacity = function->operandCapacity == 0u
                                             ? 8u
                                             : function->operandCapacity * 2u;
                TZrExecIrValueId *pool = (TZrExecIrValueId *)realloc(
                        function->operandPool, (size_t)capacity * sizeof(*pool));
                if (pool == ZR_NULL) {
                    if (diagnostic != ZR_NULL) {
                        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
                        diagnostic->instructionId = index + 1u;
                        diagnostic->sourceId = current->sourceId;
                    }
                    return ZR_FALSE;
                }
                function->operandPool = pool;
                function->operandCapacity = capacity;
            }
            /* Do not rewrite arbitrary later pool entries here.  Replacing the
             * duplicate with COPY preserves its old SSA result ID, so every
             * existing use remains valid without assuming instruction-array
             * order is a dominance order. */
            current->opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_COPY;
            current->matchTypeToken = 0u;
            current->operandRange.count = 1u;
            current->operandRange.start = function->operandCount;
            function->operandPool[function->operandCount++] = newValue;
            emit_remark(remarks, current, ZR_EXEC_IR_REMARK_SUCCESS, 1u);
            (void)oldValue;
            break;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_CanElideBoundsCheck(
        const SZrExecIrRangeFact *index,
        const SZrExecIrRangeFact *length,
        SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (index == ZR_NULL || length == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    return ZrParser_ExecIr_RangeProvesBounds(index, length);
}
