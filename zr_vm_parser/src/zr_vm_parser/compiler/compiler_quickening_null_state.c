#include <stdint.h>
#include <stdlib.h>

#include "compiler_quickening_null_state.h"
#include "zr_vm_common/zr_instruction_conf.h"

typedef enum EZrQuickeningNullState {
    ZR_QUICKENING_NULL_UNREACHED = 0,
    ZR_QUICKENING_NULL_VALUE = 1,
    ZR_QUICKENING_NULL_UNKNOWN = 2
} EZrQuickeningNullState;

typedef enum EZrQuickeningNullEffect {
    ZR_QUICKENING_NULL_KEEP = 0,
    ZR_QUICKENING_NULL_WRITE_VALUE = 1,
    ZR_QUICKENING_NULL_WRITE_UNKNOWN = 2
} EZrQuickeningNullEffect;

typedef struct SZrQuickeningNullNode {
    TZrUInt32 successors[2];
    TZrUInt8 successorCount;
    TZrUInt8 effect;
    TZrUInt8 input;
} SZrQuickeningNullNode;

static TZrBool quickening_null_constant_is_plain(
        const SZrFunction *function, TZrUInt32 constantIndex) {
    const SZrTypeValue *value;

    if (function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }
    value = &function->constantValueList[constantIndex];
    return value->type == ZR_VALUE_TYPE_NULL &&
           !value->isGarbageCollectable &&
           value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
           value->ownershipControl == ZR_NULL &&
           value->ownershipWeakRef == ZR_NULL;
}

static TZrBool quickening_null_constant_is_signed_integer(
        const SZrFunction *function, TZrUInt32 constantIndex) {
    const SZrTypeValue *value;

    if (function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }
    value = &function->constantValueList[constantIndex];
    return value->type == ZR_VALUE_TYPE_INT64 && value->isNative &&
           !value->isGarbageCollectable &&
           value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
           value->ownershipControl == ZR_NULL &&
           value->ownershipWeakRef == ZR_NULL;
}

static TZrBool quickening_null_has_isolated_value_frame(
        const SZrFunction *function, TZrUInt32 stackSlot) {
    TZrUInt64 previousEnd;
    const TZrUInt32 valueAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    const TZrUInt32 stackAlign = (TZrUInt32)_Alignof(SZrTypeValueOnStack);

    if (function->frameSlotLayouts == ZR_NULL ||
        function->frameSlotLayoutLength != function->stackSize ||
        stackSlot >= function->stackSize || stackSlot < function->parameterCount ||
        function->frameByteAlign < valueAlign || function->frameByteAlign > stackAlign ||
        (function->frameByteAlign & (function->frameByteAlign - 1u)) != 0u ||
        function->childFunctionLength != 0u || function->closureValueLength != 0u ||
        function->typedClosureBindingLength != 0u || function->exportedVariableLength != 0u ||
        function->typedExportedSymbolLength != 0u ||
        function->semIrOwnershipTableLength != 0u) {
        return ZR_FALSE;
    }

    previousEnd = (TZrUInt64)function->stackSize * sizeof(SZrTypeValueOnStack);
    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *layout = &function->frameSlotLayouts[index];
        TZrUInt64 end = (TZrUInt64)layout->byteOffset + layout->byteSize;

        /* Checked entry initialization uses these actual offsets before/after finalization. */
        if (layout->stackSlot != index ||
            layout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            (layout->reserved0 & (TZrUInt16)~ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) != 0u ||
            layout->byteSize != (TZrUInt32)sizeof(SZrTypeValue) ||
            layout->byteAlign < valueAlign || layout->byteAlign > stackAlign ||
            (layout->byteAlign & (layout->byteAlign - 1u)) != 0u ||
            layout->byteOffset % layout->byteAlign != 0u ||
            layout->byteOffset < previousEnd || end > function->frameByteSize ||
            (index == stackSlot && layout->isParameter)) {
            return ZR_FALSE;
        }
        previousEnd = end;
    }
    return ZR_TRUE;
}

static TZrBool quickening_null_jump_target(
        const SZrFunction *function, TZrUInt32 index,
        const TZrInstruction *instruction, TZrUInt32 *target) {
    TZrInt64 destination = (TZrInt64)index + 1 +
                          (TZrInt32)instruction->instruction.operand.operand2[0];

    if (destination < 0 || (TZrUInt64)destination >= function->instructionsLength) {
        return ZR_FALSE;
    }
    *target = (TZrUInt32)destination;
    return ZR_TRUE;
}

static TZrBool quickening_null_build_node(
        const SZrFunction *function, TZrUInt32 index, TZrUInt32 ignoredClear,
        TZrUInt32 stackSlot, SZrQuickeningNullNode *node) {
    const TZrInstruction *instruction = &function->instructionsList[index];
    EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    TZrUInt32 destination = instruction->instruction.operandExtra;
    TZrUInt32 first = instruction->instruction.operand.operand1[0];
    TZrUInt32 second = instruction->instruction.operand.operand1[1];
    TZrUInt32 stackSize = function->stackSize;
    TZrBool writesDestination = ZR_FALSE;

    node->successorCount = 1u;
    node->successors[0] = index + 1u;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(NOP):
            break;
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            if ((TZrUInt32)instruction->instruction.operand.operand2[0] >= function->constantValueLength ||
                function->constantValueList == ZR_NULL) {
                return ZR_FALSE;
            }
            writesDestination = ZR_TRUE;
            if (destination == stackSlot &&
                quickening_null_constant_is_plain(
                        function, (TZrUInt32)instruction->instruction.operand.operand2[0])) {
                node->effect = ZR_QUICKENING_NULL_WRITE_VALUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            if ((TZrUInt32)instruction->instruction.operand.operand2[0] >= stackSize) {
                return ZR_FALSE;
            }
            writesDestination = ZR_TRUE;
            break;
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
            if (first >= stackSize) {
                return ZR_FALSE;
            }
            writesDestination = ZR_TRUE;
            break;
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
            writesDestination = ZR_TRUE;
            break;
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
            if (first >= stackSize || function->callSiteCaches == ZR_NULL ||
                second >= function->callSiteCacheLength ||
                function->callSiteCaches[second].kind != ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) {
                return ZR_FALSE;
            }
            writesDestination = ZR_TRUE;
            break;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
            if (first >= stackSize || second >= stackSize) {
                return ZR_FALSE;
            }
            writesDestination = destination != ZR_INSTRUCTION_USE_RET_FLAG;
            if (!writesDestination && opcode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)) {
                return ZR_FALSE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
            if (first >= stackSize || !quickening_null_constant_is_signed_integer(function, second)) {
                return ZR_FALSE;
            }
            writesDestination = ZR_TRUE;
            break;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
            if (instruction->instruction.operand.operand0[0] >= stackSize ||
                instruction->instruction.operand.operand0[1] >= stackSize) {
                return ZR_FALSE;
            }
            writesDestination = ZR_TRUE;
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
            if (destination != ZR_INSTRUCTION_USE_RET_FLAG || first >= stackSize ||
                stackSize - first < 4u || !quickening_null_constant_is_signed_integer(function, second)) {
                return ZR_FALSE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
            if (first >= stackSize || destination != first || second >= stackSize - first) {
                return ZR_FALSE;
            }
            /* Callee storage follows the complete caller frame; only result/arguments overlap it. */
            if (stackSlot >= first && stackSlot <= first + second) {
                node->effect = ZR_QUICKENING_NULL_WRITE_UNKNOWN;
            }
            writesDestination = ZR_TRUE;
            break;
        case ZR_INSTRUCTION_ENUM(JUMP):
            return quickening_null_jump_target(function, index, instruction, &node->successors[0]);
        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
            if (destination >= stackSize ||
                !quickening_null_jump_target(function, index, instruction, &node->successors[1])) {
                return ZR_FALSE;
            }
            node->successorCount = 2u;
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED): {
            TZrInt64 target = (TZrInt64)index + 1 + (TZrInt16)second;

            if (destination >= stackSize || first >= stackSize || target < 0 ||
                (TZrUInt64)target >= function->instructionsLength) {
                return ZR_FALSE;
            }
            node->successors[1] = (TZrUInt32)target;
            node->successorCount = 2u;
            break;
        }
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            if (destination > 1u || (destination == 1u && first >= stackSize)) {
                return ZR_FALSE;
            }
            node->successorCount = 0u;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }

    if (writesDestination) {
        if (destination >= stackSize) {
            return ZR_FALSE;
        }
        if (destination == stackSlot && node->effect != ZR_QUICKENING_NULL_WRITE_VALUE) {
            node->effect = ZR_QUICKENING_NULL_WRITE_UNKNOWN;
        }
    }
    if (index == ignoredClear) {
        node->effect = ZR_QUICKENING_NULL_KEEP;
    }
    return node->successorCount == 0u || node->successors[0] < function->instructionsLength;
}

TZrBool compiler_quickening_slot_is_null_without_clear(
        const SZrFunction *function, TZrUInt32 instructionIndex, TZrUInt32 stackSlot) {
    SZrQuickeningNullNode *nodes = ZR_NULL;
    TZrUInt32 *queue = ZR_NULL;
    size_t queueCapacity, head = 0u, tail = 0u;
    TZrBool result = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL ||
        instructionIndex >= function->instructionsLength ||
        !quickening_null_has_isolated_value_frame(function, stackSlot)) {
        return ZR_FALSE;
    }

    const TZrInstruction *clear = &function->instructionsList[instructionIndex];
    if (clear->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        clear->instruction.operandExtra != stackSlot ||
        !quickening_null_constant_is_plain(
                function, (TZrUInt32)clear->instruction.operand.operand2[0]) ||
        function->instructionsLength > SIZE_MAX / sizeof(*nodes) ||
        function->instructionsLength > SIZE_MAX / (2u * sizeof(*queue))) {
        return ZR_FALSE;
    }

    queueCapacity = (size_t)function->instructionsLength * 2u;
    nodes = calloc(function->instructionsLength, sizeof(*nodes));
    queue = malloc(queueCapacity * sizeof(*queue));
    if (nodes == ZR_NULL || queue == ZR_NULL) {
        goto cleanup;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        if (!quickening_null_build_node(function, index, instructionIndex, stackSlot, &nodes[index])) {
            goto cleanup;
        }
    }

    /* Entry is independently initialized NULL; the omitted clear cannot seed its own proof. */
    nodes[0].input = ZR_QUICKENING_NULL_VALUE;
    queue[tail++] = 0u;
    while (head < tail) {
        SZrQuickeningNullNode *node = &nodes[queue[head++]];
        TZrUInt8 output = node->input;

        if (node->effect == ZR_QUICKENING_NULL_WRITE_VALUE) {
            output = ZR_QUICKENING_NULL_VALUE;
        } else if (node->effect == ZR_QUICKENING_NULL_WRITE_UNKNOWN) {
            output = ZR_QUICKENING_NULL_UNKNOWN;
        }
        for (TZrUInt8 edge = 0u; edge < node->successorCount; edge++) {
            SZrQuickeningNullNode *successor = &nodes[node->successors[edge]];

            if (output > successor->input) {
                successor->input = output;
                if (tail >= queueCapacity) {
                    goto cleanup;
                }
                queue[tail++] = node->successors[edge];
            }
        }
    }
    result = nodes[instructionIndex].input == ZR_QUICKENING_NULL_VALUE;

cleanup:
    free(queue);
    free(nodes);
    return result;
}
