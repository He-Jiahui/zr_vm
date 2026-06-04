#include "compile_expression_internal.h"

TZrInstruction compiler_create_jump_if_false_for_condition(SZrCompilerState *cs,
                                                           SZrAstNode *conditionExpression,
                                                           TZrUInt32 conditionSlot) {
    EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(JUMP_IF);
    EZrValueType conditionType = ZR_VALUE_TYPE_OBJECT;

    if (compiler_try_infer_expression_base_type(cs, conditionExpression, &conditionType) &&
        ZR_VALUE_IS_TYPE_BOOL(conditionType)) {
        opcode = ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE);
    }

    return create_instruction_1(opcode, ZR_COMPILE_SLOT_U16(conditionSlot), 0);
}
