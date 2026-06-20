//
// Minimal SemIR metadata builder for ownership-aware and dynamic bytecode.
//

#include <string.h>

#include "compiler_internal.h"

#define ZR_SEMIR_TYPE_TABLE_DEFAULT_ENTRY_COUNT 1U
#define ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX 0U
#define ZR_SEMIR_OWNERSHIP_STATE_TABLE_CAPACITY 8U
#define ZR_SEMIR_OWNERSHIP_STATE_INDEX_FIRST 0U
#define ZR_SEMIR_DEOPT_ID_FIRST (ZR_RUNTIME_SEMIR_DEOPT_ID_NONE + 1U)
#define ZR_SEMIR_MAPPED_INSTRUCTION_LIST_CAPACITY 2U

typedef struct SZrSemIrMappedInstruction {
    TZrUInt32 opcode;
    TZrUInt32 effectKind;
    TZrUInt32 ownershipInput;
    TZrUInt32 ownershipOutput;
    TZrBool needsDeopt;
    TZrBool hasExplicitOperands;
    TZrBool hasExplicitStaticCType;
    EZrStaticCType staticCType;
    TZrUInt32 destinationSlot;
    TZrUInt32 operand0;
    TZrUInt32 operand1;
} SZrSemIrMappedInstruction;

typedef struct SZrSemIrMappedInstructionList {
    SZrSemIrMappedInstruction entries[ZR_SEMIR_MAPPED_INSTRUCTION_LIST_CAPACITY];
    TZrUInt32 count;
} SZrSemIrMappedInstructionList;

static void semir_mapped_instruction_list_init(SZrSemIrMappedInstructionList *list) {
    if (list == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(list, 0, sizeof(*list));
}

static SZrSemIrMappedInstruction *semir_mapped_instruction_list_append(SZrSemIrMappedInstructionList *list) {
    SZrSemIrMappedInstruction *entry;

    if (list == ZR_NULL || list->count >= ZR_SEMIR_MAPPED_INSTRUCTION_LIST_CAPACITY) {
        return ZR_NULL;
    }

    entry = &list->entries[list->count];
    ZrCore_Memory_RawSet(entry, 0, sizeof(*entry));
    list->count++;
    return entry;
}

static TZrBool semir_slot_has_inline_struct_layout(const SZrFunction *function, TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *layout = &function->frameSlotLayouts[index];
        if (layout->stackSlot == stackSlot && layout->slotKind == ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const SZrFunctionFrameSlotLayout *semir_find_frame_slot_layout(const SZrFunction *function,
                                                                      TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *layout = &function->frameSlotLayouts[index];
        if (layout->stackSlot == stackSlot) {
            return layout;
        }
    }

    return ZR_NULL;
}

static TZrUInt32 semir_resolve_inline_struct_source_slot(const SZrFunction *function,
                                                         TZrUInt32 instructionIndex,
                                                         TZrUInt32 sourceSlot) {
    const TZrInstruction *previousInstruction;

    if (semir_slot_has_inline_struct_layout(function, sourceSlot)) {
        return sourceSlot;
    }
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || instructionIndex == 0) {
        return sourceSlot;
    }

    previousInstruction = &function->instructionsList[instructionIndex - 1];
    if ((EZrInstructionCode)previousInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        previousInstruction->instruction.operandExtra != sourceSlot) {
        return sourceSlot;
    }

    return previousInstruction->instruction.operand.operand1[0];
}

static void semir_mapped_instruction_set_operands(SZrSemIrMappedInstruction *mapped,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 operand0,
                                                  TZrUInt32 operand1) {
    if (mapped == ZR_NULL) {
        return;
    }

    mapped->hasExplicitOperands = ZR_TRUE;
    mapped->destinationSlot = destinationSlot;
    mapped->operand0 = operand0;
    mapped->operand1 = operand1;
}

static TZrBool semir_instruction_has_type_conflict(const SZrFunction *function, const TZrInstruction *instruction);

static EZrStaticCType semir_static_c_type_from_integral_type_ref(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_STATIC_C_TYPE_DYNAMIC;
    }

    switch (typeRef->staticCType) {
        case ZR_STATIC_C_TYPE_I8:
        case ZR_STATIC_C_TYPE_I16:
        case ZR_STATIC_C_TYPE_I32:
        case ZR_STATIC_C_TYPE_I64:
            return ZR_STATIC_C_TYPE_I64;
        case ZR_STATIC_C_TYPE_U8:
        case ZR_STATIC_C_TYPE_U16:
        case ZR_STATIC_C_TYPE_U32:
        case ZR_STATIC_C_TYPE_U64:
            return ZR_STATIC_C_TYPE_U64;
        default:
            break;
    }

    switch (typeRef->baseType) {
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            return ZR_STATIC_C_TYPE_I64;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return ZR_STATIC_C_TYPE_U64;
        default:
            return ZR_STATIC_C_TYPE_DYNAMIC;
    }
}

static EZrStaticCType semir_static_c_type_for_slot_integral_hint(const SZrFunction *function, TZrUInt32 slot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_STATIC_C_TYPE_DYNAMIC;
    }

    for (index = 0u; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        EZrStaticCType staticCType;

        if (binding->stackSlot != slot) {
            continue;
        }

        staticCType = semir_static_c_type_from_integral_type_ref(&binding->type);
        if (staticCType != ZR_STATIC_C_TYPE_DYNAMIC) {
            return staticCType;
        }
    }

    return ZR_STATIC_C_TYPE_DYNAMIC;
}

static EZrStaticCType semir_static_c_type_for_typed_bitwise_instruction(const SZrFunction *function,
                                                                        const TZrInstruction *instruction,
                                                                        EZrInstructionCode opcode) {
    EZrStaticCType leftType;
    EZrStaticCType rightType;

    if (instruction == ZR_NULL) {
        return ZR_STATIC_C_TYPE_DYNAMIC;
    }

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
            leftType = semir_static_c_type_for_slot_integral_hint(
                    function,
                    instruction->instruction.operand.operand1[0]);
            return leftType != ZR_STATIC_C_TYPE_DYNAMIC ? leftType : ZR_STATIC_C_TYPE_I64;

        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
            leftType = semir_static_c_type_for_slot_integral_hint(
                    function,
                    instruction->instruction.operand.operand1[0]);
            rightType = semir_static_c_type_for_slot_integral_hint(
                    function,
                    instruction->instruction.operand.operand1[1]);
            if (leftType == ZR_STATIC_C_TYPE_U64 || rightType == ZR_STATIC_C_TYPE_U64) {
                return ZR_STATIC_C_TYPE_U64;
            }
            if (leftType == ZR_STATIC_C_TYPE_I64 || rightType == ZR_STATIC_C_TYPE_I64) {
                return ZR_STATIC_C_TYPE_I64;
            }
            return ZR_STATIC_C_TYPE_I64;

        default:
            return ZR_STATIC_C_TYPE_DYNAMIC;
    }
}

static EZrStaticCType semir_static_c_type_for_typed_scalar_instruction(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
            return ZR_STATIC_C_TYPE_I64;

        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
            return ZR_STATIC_C_TYPE_U64;

        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
            return ZR_STATIC_C_TYPE_F64;

        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
            return ZR_STATIC_C_TYPE_BOOL;

        default:
            return ZR_STATIC_C_TYPE_DYNAMIC;
    }
}

static TZrBool semir_map_typed_scalar_instruction(const SZrFunction *function,
                                                  const TZrInstruction *instruction,
                                                  SZrSemIrMappedInstruction *outMapped) {
    EZrInstructionCode opcode;
    EZrStaticCType staticCType;

    if (instruction == ZR_NULL || outMapped == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    staticCType = semir_static_c_type_for_typed_scalar_instruction(opcode);
    if (staticCType == ZR_STATIC_C_TYPE_DYNAMIC) {
        staticCType = semir_static_c_type_for_typed_bitwise_instruction(function, instruction, opcode);
    }

    ZrCore_Memory_RawSet(outMapped, 0, sizeof(*outMapped));
    if (semir_instruction_has_type_conflict(function, instruction)) {
        outMapped->opcode = ZR_SEMIR_OPCODE_DYN_ARITHMETIC;
        outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
        outMapped->needsDeopt = ZR_TRUE;
        semir_mapped_instruction_set_operands(outMapped,
                                              instruction->instruction.operandExtra,
                                              instruction->instruction.operand.operand1[0],
                                              instruction->instruction.operand.operand1[1]);
        return ZR_TRUE;
    }

    outMapped->hasExplicitStaticCType = (TZrBool)(staticCType != ZR_STATIC_C_TYPE_DYNAMIC);
    outMapped->staticCType = staticCType;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
            outMapped->opcode = ZR_SEMIR_OPCODE_ADD;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
            outMapped->opcode = ZR_SEMIR_OPCODE_SUB;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
            outMapped->opcode = ZR_SEMIR_OPCODE_MUL;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
            outMapped->opcode = ZR_SEMIR_OPCODE_DIV;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
            outMapped->opcode = ZR_SEMIR_OPCODE_MOD;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
            outMapped->opcode = ZR_SEMIR_OPCODE_EQ;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
            outMapped->opcode = ZR_SEMIR_OPCODE_NE;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
            outMapped->opcode = ZR_SEMIR_OPCODE_LT;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
            outMapped->opcode = ZR_SEMIR_OPCODE_LE;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
            outMapped->opcode = ZR_SEMIR_OPCODE_GT;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
            outMapped->opcode = ZR_SEMIR_OPCODE_GE;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
            outMapped->opcode = ZR_SEMIR_OPCODE_BIT_NOT;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
            outMapped->opcode = ZR_SEMIR_OPCODE_BIT_AND;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
            outMapped->opcode = ZR_SEMIR_OPCODE_BIT_OR;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            outMapped->opcode = ZR_SEMIR_OPCODE_BIT_XOR;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
            outMapped->opcode = ZR_SEMIR_OPCODE_SHL;
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
            outMapped->opcode = ZR_SEMIR_OPCODE_SHR;
            return ZR_TRUE;

        default:
            return ZR_FALSE;
    }
}

static TZrBool semir_callsite_cache_kind_matches(const SZrFunctionCallSiteCacheEntry *cacheEntry,
                                                 EZrFunctionCallSiteCacheKind expectedKind) {
    if (cacheEntry == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(cacheEntry->kind == (TZrUInt32)expectedKind);
}

static TZrBool semir_resolve_member_slot_member_entry(const SZrFunction *function,
                                                      TZrUInt32 instructionIndex,
                                                      TZrUInt32 memberSlotOperand,
                                                      EZrFunctionCallSiteCacheKind expectedKind,
                                                      TZrUInt32 *outMemberEntryIndex) {
    const SZrFunctionCallSiteCacheEntry *cacheEntry;

    if (outMemberEntryIndex != ZR_NULL) {
        *outMemberEntryIndex = memberSlotOperand;
    }
    if (function == ZR_NULL || outMemberEntryIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->callSiteCaches != ZR_NULL && memberSlotOperand < function->callSiteCacheLength) {
        cacheEntry = &function->callSiteCaches[memberSlotOperand];
        if (semir_callsite_cache_kind_matches(cacheEntry, expectedKind) &&
            function->memberEntries != ZR_NULL &&
            cacheEntry->memberEntryIndex < function->memberEntryLength) {
            *outMemberEntryIndex = cacheEntry->memberEntryIndex;
            return ZR_TRUE;
        }
    }

    (void)instructionIndex;
    return (TZrBool)(function->memberEntries != ZR_NULL &&
                     memberSlotOperand < function->memberEntryLength);
}

static SZrString *semir_resolve_member_symbol(const SZrFunction *function, TZrUInt16 memberId) {
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberId >= function->memberEntryLength) {
        return ZR_NULL;
    }

    return function->memberEntries[memberId].symbol;
}

static TZrBool semir_string_has_prefix(SZrString *stringValue, const TZrChar *prefix) {
    TZrNativeString text;
    TZrSize prefixLength;

    if (stringValue == ZR_NULL || prefix == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_GetNativeString(stringValue);
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    prefixLength = strlen(prefix);
    return strncmp(text, prefix, prefixLength) == 0;
}

static TZrBool semir_match_meta_accessor_call(const SZrFunction *function,
                                              TZrUInt32 instructionIndex,
                                              SZrSemIrMappedInstruction *outMapped) {
    const TZrInstruction *callInstruction;
    const TZrInstruction *getMemberInstruction;
    const TZrInstruction *nextInstruction;
    SZrString *memberName;
    TZrUInt32 parameterCount;

    if (function == ZR_NULL || outMapped == ZR_NULL || function->instructionsList == ZR_NULL ||
        instructionIndex >= function->instructionsLength || instructionIndex == 0) {
        return ZR_FALSE;
    }

    callInstruction = &function->instructionsList[instructionIndex];
    if ((EZrInstructionCode)callInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_CALL)) {
        return ZR_FALSE;
    }

    getMemberInstruction = &function->instructionsList[instructionIndex - 1];
    if ((EZrInstructionCode)getMemberInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_MEMBER)) {
        return ZR_FALSE;
    }
    if (getMemberInstruction->instruction.operandExtra != callInstruction->instruction.operandExtra ||
        getMemberInstruction->instruction.operand.operand1[0] != callInstruction->instruction.operand.operand1[0]) {
        return ZR_FALSE;
    }

    memberName = semir_resolve_member_symbol(function, getMemberInstruction->instruction.operand.operand1[1]);
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    parameterCount = callInstruction->instruction.operand.operand1[1];
    if (semir_string_has_prefix(memberName, "__get_")) {
        if (parameterCount > 1) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(outMapped, 0, sizeof(*outMapped));
        outMapped->opcode = ZR_SEMIR_OPCODE_META_GET;
        outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
        outMapped->needsDeopt = ZR_TRUE;
        outMapped->hasExplicitOperands = ZR_TRUE;
        outMapped->destinationSlot = callInstruction->instruction.operandExtra;
        outMapped->operand0 = callInstruction->instruction.operand.operand1[0];
        outMapped->operand1 = getMemberInstruction->instruction.operand.operand1[1];
        return ZR_TRUE;
    }

    if (!semir_string_has_prefix(memberName, "__set_")) {
        return ZR_FALSE;
    }
    if (parameterCount == 0 || instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    nextInstruction = &function->instructionsList[instructionIndex + 1];
    if ((EZrInstructionCode)nextInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        nextInstruction->instruction.operandExtra != callInstruction->instruction.operandExtra) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outMapped, 0, sizeof(*outMapped));
    outMapped->opcode = ZR_SEMIR_OPCODE_META_SET;
    outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
    outMapped->needsDeopt = ZR_TRUE;
    outMapped->hasExplicitOperands = ZR_TRUE;
    outMapped->destinationSlot = nextInstruction->instruction.operandExtra;
    outMapped->operand0 = callInstruction->instruction.operand.operand1[0];
    outMapped->operand1 = getMemberInstruction->instruction.operand.operand1[1];
    return ZR_TRUE;
}

static void semir_init_default_type_ref(SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->staticCType = ZR_STATIC_C_TYPE_DYNAMIC;
    typeRef->staticCTypeId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
}

static TZrBool semir_type_ref_equals(const SZrFunctionTypedTypeRef *lhs, const SZrFunctionTypedTypeRef *rhs) {
    if (lhs == rhs) {
        return ZR_TRUE;
    }
    if (lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }

    return lhs->baseType == rhs->baseType &&
           lhs->isNullable == rhs->isNullable &&
           lhs->ownershipQualifier == rhs->ownershipQualifier &&
           lhs->isArray == rhs->isArray &&
           lhs->typeName == rhs->typeName &&
           lhs->elementBaseType == rhs->elementBaseType &&
           lhs->elementTypeName == rhs->elementTypeName;
}

static TZrUInt32 semir_find_inline_struct_type_layout_id_for_type_ref(const SZrFunction *function,
                                                                      const SZrFunctionTypedTypeRef *typeRef) {
    if (function == ZR_NULL || typeRef == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    }

    for (TZrUInt32 index = 0u; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        const SZrFunctionFrameSlotLayout *slotLayout;

        if (!semir_type_ref_equals(&binding->type, typeRef)) {
            continue;
        }

        slotLayout = semir_find_frame_slot_layout(function, binding->stackSlot);
        if (slotLayout != ZR_NULL &&
            slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
            slotLayout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
            return slotLayout->typeLayoutId;
        }
    }

    return ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
}

static void semir_apply_static_c_type_annotation(const SZrFunction *function, SZrFunctionTypedTypeRef *typeRef) {
    TZrUInt32 typeLayoutId;

    if (typeRef == ZR_NULL) {
        return;
    }

    typeRef->staticCType = ZR_STATIC_C_TYPE_DYNAMIC;
    typeRef->staticCTypeId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;

    typeLayoutId = semir_find_inline_struct_type_layout_id_for_type_ref(function, typeRef);
    if (typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        typeRef->staticCType = ZR_STATIC_C_TYPE_STRUCT;
        typeRef->staticCTypeId = typeLayoutId;
        return;
    }

    switch (typeRef->baseType) {
        case ZR_VALUE_TYPE_BOOL:
            typeRef->staticCType = ZR_STATIC_C_TYPE_BOOL;
            return;
        case ZR_VALUE_TYPE_INT8:
            typeRef->staticCType = ZR_STATIC_C_TYPE_I8;
            return;
        case ZR_VALUE_TYPE_INT16:
            typeRef->staticCType = ZR_STATIC_C_TYPE_I16;
            return;
        case ZR_VALUE_TYPE_INT32:
            typeRef->staticCType = ZR_STATIC_C_TYPE_I32;
            return;
        case ZR_VALUE_TYPE_INT64:
            typeRef->staticCType = ZR_STATIC_C_TYPE_I64;
            return;
        case ZR_VALUE_TYPE_UINT8:
            typeRef->staticCType = ZR_STATIC_C_TYPE_U8;
            return;
        case ZR_VALUE_TYPE_UINT16:
            typeRef->staticCType = ZR_STATIC_C_TYPE_U16;
            return;
        case ZR_VALUE_TYPE_UINT32:
            typeRef->staticCType = ZR_STATIC_C_TYPE_U32;
            return;
        case ZR_VALUE_TYPE_UINT64:
            typeRef->staticCType = ZR_STATIC_C_TYPE_U64;
            return;
        case ZR_VALUE_TYPE_FLOAT:
            typeRef->staticCType = ZR_STATIC_C_TYPE_F32;
            return;
        case ZR_VALUE_TYPE_DOUBLE:
            typeRef->staticCType = ZR_STATIC_C_TYPE_F64;
            return;
        case ZR_VALUE_TYPE_STRING:
        case ZR_VALUE_TYPE_BUFFER:
        case ZR_VALUE_TYPE_ARRAY:
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE_VALUE:
        case ZR_VALUE_TYPE_CLOSURE:
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_THREAD:
            typeRef->staticCType = ZR_STATIC_C_TYPE_GC_REF;
            return;
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            typeRef->staticCType = ZR_STATIC_C_TYPE_NATIVE_POINTER;
            return;
        case ZR_VALUE_TYPE_NATIVE_DATA:
            typeRef->staticCType = ZR_STATIC_C_TYPE_NATIVE_DATA;
            return;
        default:
            return;
    }
}

static TZrBool semir_slot_has_type_conflict(const SZrFunction *function, TZrUInt32 slot) {
    SZrFunctionTypedTypeRef firstType;
    TZrBool hasFirstType = ZR_FALSE;
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(&firstType, 0, sizeof(firstType));
    for (index = 0u; index < function->typedLocalBindingLength; index++) {
        SZrFunctionTypedTypeRef annotatedType;
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];

        if (binding->stackSlot != slot) {
            continue;
        }

        annotatedType = binding->type;
        semir_apply_static_c_type_annotation(function, &annotatedType);
        if (!hasFirstType) {
            firstType = annotatedType;
            hasFirstType = ZR_TRUE;
            continue;
        }

        if (!semir_type_ref_equals(&firstType, &annotatedType) ||
            firstType.staticCType != annotatedType.staticCType ||
            firstType.staticCTypeId != annotatedType.staticCTypeId) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool semir_instruction_has_type_conflict(const SZrFunction *function, const TZrInstruction *instruction) {
    if (function == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(semir_slot_has_type_conflict(function, instruction->instruction.operandExtra) ||
                     semir_slot_has_type_conflict(function, instruction->instruction.operand.operand1[0]) ||
                     semir_slot_has_type_conflict(function, instruction->instruction.operand.operand1[1]));
}

static void semir_release_existing_metadata(SZrState *state, SZrFunction *function) {
    SZrGlobalState *global;

    if (state == ZR_NULL || function == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    global = state->global;
    if (function->semIrTypeTable != ZR_NULL && function->semIrTypeTableLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrTypeTable,
                                      sizeof(SZrFunctionTypedTypeRef) * function->semIrTypeTableLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrOwnershipTable != ZR_NULL && function->semIrOwnershipTableLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrOwnershipTable,
                                      sizeof(SZrSemIrOwnershipEntry) * function->semIrOwnershipTableLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrEffectTable != ZR_NULL && function->semIrEffectTableLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrEffectTable,
                                      sizeof(SZrSemIrEffectEntry) * function->semIrEffectTableLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrBlockTable != ZR_NULL && function->semIrBlockTableLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrBlockTable,
                                      sizeof(SZrSemIrBlockEntry) * function->semIrBlockTableLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrInstructions != ZR_NULL && function->semIrInstructionLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrInstructions,
                                      sizeof(SZrSemIrInstruction) * function->semIrInstructionLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrDeoptTable != ZR_NULL && function->semIrDeoptTableLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrDeoptTable,
                                      sizeof(SZrSemIrDeoptEntry) * function->semIrDeoptTableLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    function->semIrTypeTable = ZR_NULL;
    function->semIrTypeTableLength = 0;
    function->semIrOwnershipTable = ZR_NULL;
    function->semIrOwnershipTableLength = 0;
    function->semIrEffectTable = ZR_NULL;
    function->semIrEffectTableLength = 0;
    function->semIrBlockTable = ZR_NULL;
    function->semIrBlockTableLength = 0;
    function->semIrInstructions = ZR_NULL;
    function->semIrInstructionLength = 0;
    function->semIrDeoptTable = ZR_NULL;
    function->semIrDeoptTableLength = 0;
}

static TZrUInt32 semir_ensure_type_entry(const SZrFunction *function,
                                         SZrFunctionTypedTypeRef *typeTable,
                                         TZrUInt32 *ioCount,
                                         const SZrFunctionTypedTypeRef *typeRef) {
    TZrUInt32 index;
    SZrFunctionTypedTypeRef annotatedType;

    if (typeTable == ZR_NULL || ioCount == ZR_NULL || typeRef == ZR_NULL) {
        return ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX;
    }

    annotatedType = *typeRef;
    semir_apply_static_c_type_annotation(function, &annotatedType);

    for (index = 0; index < *ioCount; index++) {
        if (semir_type_ref_equals(&typeTable[index], &annotatedType)) {
            return index;
        }
    }

    typeTable[*ioCount] = annotatedType;
    (*ioCount)++;
    return (*ioCount) - 1;
}

static TZrUInt32 semir_find_type_index_for_slot(const SZrFunction *function,
                                                TZrUInt32 slot,
                                                const SZrFunctionTypedTypeRef *typeTable,
                                                TZrUInt32 typeCount) {
    TZrUInt32 bindingIndex;
    TZrUInt32 typeIndex;

    if (function == ZR_NULL || typeTable == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX;
    }

    for (bindingIndex = 0; bindingIndex < function->typedLocalBindingLength; bindingIndex++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[bindingIndex];
        if (binding->stackSlot != slot) {
            continue;
        }

        for (typeIndex = 0; typeIndex < typeCount; typeIndex++) {
            if (semir_type_ref_equals(&typeTable[typeIndex], &binding->type)) {
                return typeIndex;
            }
        }
    }

    {
        const SZrFunctionFrameSlotLayout *slotLayout = semir_find_frame_slot_layout(function, slot);
        if (slotLayout != ZR_NULL &&
            slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
            slotLayout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
            for (bindingIndex = 0; bindingIndex < function->typedLocalBindingLength; bindingIndex++) {
                const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[bindingIndex];
                const SZrFunctionFrameSlotLayout *bindingLayout =
                        semir_find_frame_slot_layout(function, binding->stackSlot);
                if (bindingLayout == ZR_NULL ||
                    bindingLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                    bindingLayout->typeLayoutId != slotLayout->typeLayoutId) {
                    continue;
                }

                for (typeIndex = 0; typeIndex < typeCount; typeIndex++) {
                    if (semir_type_ref_equals(&typeTable[typeIndex], &binding->type)) {
                        return typeIndex;
                    }
                }
            }
        }
    }

    return ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX;
}

static TZrUInt32 semir_find_type_index_for_static_c_type(const SZrFunctionTypedTypeRef *typeTable,
                                                         TZrUInt32 typeCount,
                                                         EZrStaticCType staticCType) {
    TZrUInt32 index;

    if (typeTable == ZR_NULL || staticCType == ZR_STATIC_C_TYPE_DYNAMIC) {
        return ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX;
    }

    for (index = 0; index < typeCount; index++) {
        if (typeTable[index].staticCType == staticCType) {
            return index;
        }
    }

    return ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX;
}

static TZrUInt32 semir_ensure_ownership_state(TZrUInt32 *stateTable,
                                              TZrUInt32 *ioCount,
                                              TZrUInt32 stateValue) {
    TZrUInt32 index;

    if (stateTable == ZR_NULL || ioCount == ZR_NULL) {
        return ZR_SEMIR_OWNERSHIP_STATE_INDEX_FIRST;
    }

    for (index = 0; index < *ioCount; index++) {
        if (stateTable[index] == stateValue) {
            return index;
        }
    }

    stateTable[*ioCount] = stateValue;
    (*ioCount)++;
    return (*ioCount) - 1;
}

static TZrBool semir_map_exec_instruction(const TZrInstruction *instruction, SZrSemIrMappedInstruction *outMapped) {
    if (instruction == ZR_NULL || outMapped == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outMapped, 0, sizeof(*outMapped));
    switch ((EZrInstructionCode)instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_ARITHMETIC;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            semir_mapped_instruction_set_operands(outMapped,
                                                  instruction->instruction.operandExtra,
                                                  instruction->instruction.operand.operand1[0],
                                                  instruction->instruction.operand.operand1[1]);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
            outMapped->opcode = ZR_SEMIR_OPCODE_META_GET;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            semir_mapped_instruction_set_operands(outMapped,
                                                  instruction->instruction.operandExtra,
                                                  instruction->instruction.operand.operand1[0],
                                                  instruction->instruction.operand.operand1[1]);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
            outMapped->opcode = ZR_SEMIR_OPCODE_META_SET;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            semir_mapped_instruction_set_operands(outMapped,
                                                  instruction->instruction.operandExtra,
                                                  instruction->instruction.operand.operand1[0],
                                                  instruction->instruction.operand.operand1[1]);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_INDEX_GET;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            semir_mapped_instruction_set_operands(outMapped,
                                                  instruction->instruction.operandExtra,
                                                  instruction->instruction.operand.operand1[0],
                                                  instruction->instruction.operand.operand1[1]);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_INDEX_SET;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            semir_mapped_instruction_set_operands(outMapped,
                                                  instruction->instruction.operandExtra,
                                                  instruction->instruction.operand.operand1[0],
                                                  instruction->instruction.operand.operand1[1]);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_UNIQUE;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_PLAIN_GC;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_UNIQUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_BORROW):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_BORROW;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_BORROW_SHARED;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_LOAN):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_LOAN;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_UNIQUE;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_BORROW_MUT;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_SHARE):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_SHARE;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_UNIQUE;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_WEAK):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_WEAK;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_WEAK;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_DETACH):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_DETACH;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_PLAIN_GC;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_UPGRADE;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_WEAK;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_RELEASE;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_PLAIN_GC;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_RETURN_LOAN;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_BORROW_MUT;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_UNIQUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(TYPEOF):
            outMapped->opcode = ZR_SEMIR_OPCODE_TYPEOF;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_CALL;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            semir_mapped_instruction_set_operands(outMapped,
                                                  instruction->instruction.operandExtra,
                                                  instruction->instruction.operand.operand1[0],
                                                  instruction->instruction.operand.operand1[1]);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_TAIL_CALL;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            semir_mapped_instruction_set_operands(outMapped,
                                                  instruction->instruction.operandExtra,
                                                  instruction->instruction.operand.operand1[0],
                                                  instruction->instruction.operand.operand1[1]);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_CALL;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_TAIL_CALL;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(META_CALL):
            outMapped->opcode = ZR_SEMIR_OPCODE_META_CALL;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            outMapped->opcode = ZR_SEMIR_OPCODE_META_TAIL_CALL;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(META_GET):
            outMapped->opcode = ZR_SEMIR_OPCODE_META_GET;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            outMapped->hasExplicitOperands = ZR_TRUE;
            outMapped->destinationSlot = instruction->instruction.operandExtra;
            outMapped->operand0 = instruction->instruction.operand.operand1[0];
            outMapped->operand1 = instruction->instruction.operand.operand1[1];
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(META_SET):
            outMapped->opcode = ZR_SEMIR_OPCODE_META_SET;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            outMapped->hasExplicitOperands = ZR_TRUE;
            outMapped->destinationSlot = instruction->instruction.operandExtra;
            outMapped->operand0 = instruction->instruction.operandExtra;
            outMapped->operand1 = instruction->instruction.operand.operand1[1];
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_ITER_INIT;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            semir_mapped_instruction_set_operands(outMapped,
                                                  instruction->instruction.operandExtra,
                                                  instruction->instruction.operand.operand1[0],
                                                  instruction->instruction.operand.operand1[1]);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            semir_mapped_instruction_set_operands(outMapped,
                                                  instruction->instruction.operandExtra,
                                                  instruction->instruction.operand.operand1[0],
                                                  instruction->instruction.operand.operand1[1]);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_ITER_INIT;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool semir_map_value_type_instruction(const SZrFunction *function,
                                                TZrUInt32 instructionIndex,
                                                SZrSemIrMappedInstructionList *outList) {
    const TZrInstruction *instruction;
    EZrInstructionCode opcode;
    TZrUInt32 valueOrDestinationSlot;
    TZrUInt32 receiverSlot;
    TZrUInt32 memberEntryIndex;
    SZrSemIrMappedInstruction *mapped;

    if (function == ZR_NULL || outList == ZR_NULL || function->instructionsList == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
            valueOrDestinationSlot = instruction->instruction.operandExtra;
            receiverSlot = semir_resolve_inline_struct_source_slot(function,
                                                                   instructionIndex,
                                                                   instruction->instruction.operand.operand1[0]);
            if (!semir_resolve_member_slot_member_entry(function,
                                                        instructionIndex,
                                                        instruction->instruction.operand.operand1[1],
                                                        ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET,
                                                        &memberEntryIndex)) {
                return ZR_FALSE;
            }
            if (!semir_slot_has_inline_struct_layout(function, receiverSlot)) {
                return ZR_FALSE;
            }

            mapped = semir_mapped_instruction_list_append(outList);
            if (mapped == ZR_NULL) {
                return ZR_FALSE;
            }
            mapped->opcode = ZR_SEMIR_OPCODE_FIELD_ADDR;
            semir_mapped_instruction_set_operands(mapped, receiverSlot, receiverSlot, memberEntryIndex);

            mapped = semir_mapped_instruction_list_append(outList);
            if (mapped == ZR_NULL) {
                return ZR_FALSE;
            }
            mapped->opcode = ZR_SEMIR_OPCODE_LOAD_VALUE;
            semir_mapped_instruction_set_operands(mapped, valueOrDestinationSlot, receiverSlot, memberEntryIndex);
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
            valueOrDestinationSlot = instruction->instruction.operandExtra;
            receiverSlot = semir_resolve_inline_struct_source_slot(function,
                                                                   instructionIndex,
                                                                   instruction->instruction.operand.operand1[0]);
            if (!semir_resolve_member_slot_member_entry(function,
                                                        instructionIndex,
                                                        instruction->instruction.operand.operand1[1],
                                                        ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET,
                                                        &memberEntryIndex)) {
                return ZR_FALSE;
            }
            if (!semir_slot_has_inline_struct_layout(function, receiverSlot)) {
                return ZR_FALSE;
            }

            mapped = semir_mapped_instruction_list_append(outList);
            if (mapped == ZR_NULL) {
                return ZR_FALSE;
            }
            mapped->opcode = ZR_SEMIR_OPCODE_FIELD_ADDR;
            semir_mapped_instruction_set_operands(mapped, receiverSlot, receiverSlot, memberEntryIndex);

            mapped = semir_mapped_instruction_list_append(outList);
            if (mapped == ZR_NULL) {
                return ZR_FALSE;
            }
            mapped->opcode = ZR_SEMIR_OPCODE_STORE_VALUE;
            semir_mapped_instruction_set_operands(mapped, receiverSlot, valueOrDestinationSlot, memberEntryIndex);
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(SET_STACK): {
            TZrUInt32 destinationSlot = instruction->instruction.operandExtra;
            TZrUInt32 sourceSlot = semir_resolve_inline_struct_source_slot(function,
                                                                           instructionIndex,
                                                                           instruction->instruction.operand
                                                                                   .operand1[0]);
            if (!semir_slot_has_inline_struct_layout(function, destinationSlot) ||
                !semir_slot_has_inline_struct_layout(function, sourceSlot)) {
                return ZR_FALSE;
            }

            mapped = semir_mapped_instruction_list_append(outList);
            if (mapped == ZR_NULL) {
                return ZR_FALSE;
            }
            mapped->opcode = ZR_SEMIR_OPCODE_COPY_VALUE;
            semir_mapped_instruction_set_operands(mapped, destinationSlot, sourceSlot, 0);
            return ZR_TRUE;
        }

        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL): {
            TZrUInt32 resultSlot = instruction->instruction.operandExtra;
            TZrUInt32 calleeSlot = instruction->instruction.operand.operand1[0];
            TZrUInt32 argumentCount = instruction->instruction.operand.operand1[1];
            if (!semir_slot_has_inline_struct_layout(function, resultSlot)) {
                return ZR_FALSE;
            }

            mapped = semir_mapped_instruction_list_append(outList);
            if (mapped == ZR_NULL) {
                return ZR_FALSE;
            }
            mapped->opcode = ZR_SEMIR_OPCODE_CALL_TYPED;
            semir_mapped_instruction_set_operands(mapped, resultSlot, calleeSlot, argumentCount);
            return ZR_TRUE;
        }

        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN): {
            TZrUInt32 resultCount = instruction->instruction.operandExtra;
            TZrUInt32 sourceSlot = instruction->instruction.operand.operand1[0];
            if (resultCount != 1u || !semir_slot_has_inline_struct_layout(function, sourceSlot)) {
                return ZR_FALSE;
            }

            mapped = semir_mapped_instruction_list_append(outList);
            if (mapped == ZR_NULL) {
                return ZR_FALSE;
            }
            mapped->opcode = ZR_SEMIR_OPCODE_RETURN_TYPED;
            semir_mapped_instruction_set_operands(mapped, sourceSlot, sourceSlot, 0);
            return ZR_TRUE;
        }

        default:
            return ZR_FALSE;
    }
}

static TZrBool semir_allocate_type_table(SZrState *state,
                                         SZrFunction *function,
                                         SZrFunctionTypedTypeRef **outTypeTable,
                                         TZrUInt32 *outTypeCount) {
    SZrGlobalState *global;
    TZrUInt32 typeCapacity;
    TZrUInt32 typeCount = 0;
    SZrFunctionTypedTypeRef *typeTable;
    SZrFunctionTypedTypeRef *finalTypeTable;
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || outTypeTable == ZR_NULL || outTypeCount == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    typeCapacity = function->typedLocalBindingLength + ZR_SEMIR_TYPE_TABLE_DEFAULT_ENTRY_COUNT;
    typeTable = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrFunctionTypedTypeRef) * typeCapacity,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (typeTable == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(typeTable, 0, sizeof(SZrFunctionTypedTypeRef) * typeCapacity);
    semir_init_default_type_ref(&typeTable[typeCount]);
    typeCount++;
    for (index = 0; index < function->typedLocalBindingLength; index++) {
        if (function->typedLocalBindings == ZR_NULL) {
            break;
        }
        semir_ensure_type_entry(function, typeTable, &typeCount, &function->typedLocalBindings[index].type);
    }

    finalTypeTable = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrFunctionTypedTypeRef) * typeCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (finalTypeTable == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                      typeTable,
                                      sizeof(SZrFunctionTypedTypeRef) * typeCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    ZrCore_Memory_RawCopy(finalTypeTable, typeTable, sizeof(SZrFunctionTypedTypeRef) * typeCount);
    ZrCore_Memory_RawFreeWithType(global,
                                  typeTable,
                                  sizeof(SZrFunctionTypedTypeRef) * typeCapacity,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);

    *outTypeTable = finalTypeTable;
    *outTypeCount = typeCount;
    return ZR_TRUE;
}

static TZrBool semir_map_instruction_list(const SZrFunction *function,
                                          TZrUInt32 instructionIndex,
                                          SZrSemIrMappedInstructionList *outList) {
    SZrSemIrMappedInstruction mapped;
    SZrSemIrMappedInstruction *entry;

    if (function == ZR_NULL || outList == ZR_NULL || function->instructionsList == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    semir_mapped_instruction_list_init(outList);
    if (semir_match_meta_accessor_call(function, instructionIndex, &mapped)) {
        entry = semir_mapped_instruction_list_append(outList);
        if (entry == ZR_NULL) {
            return ZR_FALSE;
        }
        *entry = mapped;
        return ZR_TRUE;
    }

    if (semir_map_value_type_instruction(function, instructionIndex, outList)) {
        return ZR_TRUE;
    }

    if (semir_map_typed_scalar_instruction(function, &function->instructionsList[instructionIndex], &mapped)) {
        entry = semir_mapped_instruction_list_append(outList);
        if (entry == ZR_NULL) {
            return ZR_FALSE;
        }
        *entry = mapped;
        return ZR_TRUE;
    }

    if (!semir_map_exec_instruction(&function->instructionsList[instructionIndex], &mapped)) {
        return ZR_FALSE;
    }

    entry = semir_mapped_instruction_list_append(outList);
    if (entry == ZR_NULL) {
        return ZR_FALSE;
    }
    *entry = mapped;
    return ZR_TRUE;
}

static TZrBool semir_build_for_single_function(SZrState *state, SZrFunction *function) {
    SZrGlobalState *global;
    TZrUInt32 semirInstructionCount = 0;
    TZrUInt32 deoptCount = 0;
    TZrUInt32 ownershipStates[ZR_SEMIR_OWNERSHIP_STATE_TABLE_CAPACITY] = {0};
    TZrUInt32 ownershipCount = 0;
    SZrFunctionTypedTypeRef *typeTable = ZR_NULL;
    TZrUInt32 typeCount = 0;
    SZrSemIrEffectEntry *effectTable = ZR_NULL;
    SZrSemIrInstruction *instructionTable = ZR_NULL;
    SZrSemIrDeoptEntry *deoptTable = ZR_NULL;
    TZrUInt32 semirIndex = 0;
    TZrUInt32 nextDeoptId = ZR_SEMIR_DEOPT_ID_FIRST;
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    semir_release_existing_metadata(state, function);

    if (function->instructionsList == ZR_NULL || function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        SZrSemIrMappedInstructionList mappedList;
        TZrUInt32 mappedIndex;
        if (semir_map_instruction_list(function, index, &mappedList)) {
            semirInstructionCount += mappedList.count;
            for (mappedIndex = 0; mappedIndex < mappedList.count; mappedIndex++) {
                if (mappedList.entries[mappedIndex].needsDeopt) {
                    deoptCount++;
                }
            }
        }
    }

    if (semirInstructionCount == 0) {
        return ZR_TRUE;
    }

    if (!semir_allocate_type_table(state, function, &typeTable, &typeCount)) {
        return ZR_FALSE;
    }

    effectTable = (SZrSemIrEffectEntry *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrSemIrEffectEntry) * semirInstructionCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    instructionTable = (SZrSemIrInstruction *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrSemIrInstruction) * semirInstructionCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (deoptCount > 0) {
        deoptTable = (SZrSemIrDeoptEntry *)ZrCore_Memory_RawMallocWithType(
                global,
                sizeof(SZrSemIrDeoptEntry) * deoptCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    if (effectTable == ZR_NULL || instructionTable == ZR_NULL || (deoptCount > 0 && deoptTable == ZR_NULL)) {
        if (typeTable != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          typeTable,
                                          sizeof(SZrFunctionTypedTypeRef) * typeCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (effectTable != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          effectTable,
                                          sizeof(SZrSemIrEffectEntry) * semirInstructionCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (instructionTable != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          instructionTable,
                                          sizeof(SZrSemIrInstruction) * semirInstructionCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (deoptTable != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          deoptTable,
                                          sizeof(SZrSemIrDeoptEntry) * deoptCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(effectTable, 0, sizeof(SZrSemIrEffectEntry) * semirInstructionCount);
    ZrCore_Memory_RawSet(instructionTable, 0, sizeof(SZrSemIrInstruction) * semirInstructionCount);
    if (deoptTable != ZR_NULL) {
        ZrCore_Memory_RawSet(deoptTable, 0, sizeof(SZrSemIrDeoptEntry) * deoptCount);
    }

    function->semIrTypeTable = typeTable;
    function->semIrTypeTableLength = typeCount;
    function->semIrEffectTable = effectTable;
    function->semIrEffectTableLength = semirInstructionCount;
    function->semIrInstructions = instructionTable;
    function->semIrInstructionLength = semirInstructionCount;
    function->semIrDeoptTable = deoptTable;
    function->semIrDeoptTableLength = deoptCount;

    for (index = 0; index < function->instructionsLength; index++) {
        SZrSemIrMappedInstructionList mappedList;
        TZrUInt32 mappedIndex;

        if (!semir_map_instruction_list(function, index, &mappedList)) {
            continue;
        }

        for (mappedIndex = 0; mappedIndex < mappedList.count; mappedIndex++) {
            SZrSemIrMappedInstruction *mapped = &mappedList.entries[mappedIndex];
            TZrUInt32 deoptId = ZR_RUNTIME_SEMIR_DEOPT_ID_NONE;

            function->semIrEffectTable[semirIndex].kind = mapped->effectKind;
            function->semIrEffectTable[semirIndex].instructionIndex = semirIndex;
            if (mapped->effectKind == ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION) {
                function->semIrEffectTable[semirIndex].ownershipInputIndex =
                        semir_ensure_ownership_state(ownershipStates, &ownershipCount, mapped->ownershipInput);
                function->semIrEffectTable[semirIndex].ownershipOutputIndex =
                        semir_ensure_ownership_state(ownershipStates, &ownershipCount, mapped->ownershipOutput);
            }

            if (mapped->needsDeopt && function->semIrDeoptTable != ZR_NULL) {
                TZrUInt32 deoptIndex = nextDeoptId - ZR_SEMIR_DEOPT_ID_FIRST;
                deoptId = nextDeoptId++;
                function->semIrDeoptTable[deoptIndex].deoptId = deoptId;
                function->semIrDeoptTable[deoptIndex].execInstructionIndex = index;
            }

            function->semIrInstructions[semirIndex].opcode = mapped->opcode;
            function->semIrInstructions[semirIndex].execInstructionIndex = index;
            if (mapped->hasExplicitStaticCType) {
                function->semIrInstructions[semirIndex].typeTableIndex =
                        semir_find_type_index_for_static_c_type(function->semIrTypeTable,
                                                                function->semIrTypeTableLength,
                                                                mapped->staticCType);
            } else {
                function->semIrInstructions[semirIndex].typeTableIndex =
                        semir_find_type_index_for_slot(function,
                                                       mapped->hasExplicitOperands
                                                               ? mapped->destinationSlot
                                                               : function->instructionsList[index].instruction
                                                                         .operandExtra,
                                                       function->semIrTypeTable,
                                                       function->semIrTypeTableLength);
            }
            function->semIrInstructions[semirIndex].effectTableIndex = semirIndex;
            function->semIrInstructions[semirIndex].destinationSlot =
                    mapped->hasExplicitOperands ? mapped->destinationSlot
                                                : function->instructionsList[index].instruction.operandExtra;
            function->semIrInstructions[semirIndex].operand0 =
                    mapped->hasExplicitOperands ? mapped->operand0
                                                : function->instructionsList[index].instruction.operand.operand1[0];
            function->semIrInstructions[semirIndex].operand1 =
                    mapped->hasExplicitOperands ? mapped->operand1
                                                : function->instructionsList[index].instruction.operand.operand1[1];
            function->semIrInstructions[semirIndex].deoptId = deoptId;
            semirIndex++;
        }
    }

    if (ownershipCount > 0) {
        function->semIrOwnershipTable = (SZrSemIrOwnershipEntry *)ZrCore_Memory_RawMallocWithType(
                global,
                sizeof(SZrSemIrOwnershipEntry) * ownershipCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->semIrOwnershipTable == ZR_NULL) {
            semir_release_existing_metadata(state, function);
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(function->semIrOwnershipTable,
                             0,
                             sizeof(SZrSemIrOwnershipEntry) * ownershipCount);
        for (index = 0; index < ownershipCount; index++) {
            function->semIrOwnershipTable[index].state = ownershipStates[index];
        }
        function->semIrOwnershipTableLength = ownershipCount;
    }

    function->semIrBlockTable = (SZrSemIrBlockEntry *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrSemIrBlockEntry),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->semIrBlockTable == ZR_NULL) {
        semir_release_existing_metadata(state, function);
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(function->semIrBlockTable, 0, sizeof(SZrSemIrBlockEntry));
    function->semIrBlockTable[0].blockId = 0;
    function->semIrBlockTable[0].firstInstructionIndex = 0;
    function->semIrBlockTable[0].instructionCount = semirInstructionCount;
    function->semIrBlockTableLength = 1;
    return ZR_TRUE;
}

static TZrBool compiler_build_function_semir_metadata_internal(SZrState *state,
                                                               SZrFunction *function,
                                                               TZrBool recurseChildren) {
    TZrUInt32 childIndex;
    TZrUInt32 constantIndex;

    if (function == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!semir_build_for_single_function(state, function)) {
        return ZR_FALSE;
    }

    if (recurseChildren && !function->childFunctionGraphIsBorrowed) {
        for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
            if (!compiler_build_function_semir_metadata_internal(state,
                                                                 &function->childFunctionList[childIndex],
                                                                 ZR_TRUE)) {
                return ZR_FALSE;
            }
        }
    }

    if (recurseChildren && function->constantValueList != ZR_NULL) {
        for (constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
            SZrTypeValue *constant = &function->constantValueList[constantIndex];
            SZrRawObject *rawObject;
            SZrFunction *constantFunction = ZR_NULL;

            if ((constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE) ||
                constant->isNative ||
                constant->value.object == ZR_NULL) {
                continue;
            }

            rawObject = constant->value.object;
            if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                constantFunction = ZR_CAST_FUNCTION(state, rawObject);
            } else if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
                SZrClosure *closure = ZR_CAST(SZrClosure *, rawObject);
                constantFunction = closure != ZR_NULL ? closure->function : ZR_NULL;
            }

            if (constantFunction == ZR_NULL || constantFunction == function) {
                continue;
            }
            if (!compiler_build_function_semir_metadata_internal(state, constantFunction, ZR_TRUE)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

TZrBool compiler_build_function_semir_metadata(SZrState *state, SZrFunction *function) {
    return compiler_build_function_semir_metadata_internal(state, function, ZR_TRUE);
}

TZrBool compiler_build_function_semir_metadata_shallow(SZrState *state, SZrFunction *function) {
    return compiler_build_function_semir_metadata_internal(state, function, ZR_FALSE);
}
