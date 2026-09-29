#include "compile_expression_internal.h"

/** @brief 为 if、循环、条件表达式和短路逻辑的退出边统一选择“条件为假”跳转指令。
 *  @pre conditionExpression 与 conditionSlot 指向同一次已编译条件；conditionSlot 可用 16 位编码，调用方随后发布指令并补跳转目标。
 *  @note 只有能静态确认 bool 时才使用直接 bool 分支；类型未知时保留运行时 truthiness 语义。本函数仅创建指令，不修改控制流图或标签。
 *  TODO: 此处将 32 位 conditionSlot 缩为 16 位但不检查范围；需核对各调用方的栈槽上限，否则高位会被截断并读取错误条件槽。 */
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
