//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_vm_conf.h"

#include <string.h>

// 前向声明
void compile_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_literal(SZrCompilerState *cs, SZrAstNode *node);
static void compile_identifier(SZrCompilerState *cs, SZrAstNode *node);
static void compile_unary_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_binary_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_logical_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_assignment_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_conditional_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_function_call(SZrCompilerState *cs, SZrAstNode *node);
static void compile_member_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_primary_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_array_literal(SZrCompilerState *cs, SZrAstNode *node);
static void compile_object_literal(SZrCompilerState *cs, SZrAstNode *node);
static void compile_lambda_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_if_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_switch_expression(SZrCompilerState *cs, SZrAstNode *node);

// 辅助函数声明（在 compiler.c 中实现，需要声明为 extern 或包含头文件）
// 为了简化，我们直接在 compile_expression.c 中重新声明这些函数
// 注意：这些函数应该在同一编译单元中，或者通过头文件共享

// 前向声明辅助函数（实际实现在 compiler.c 中）
TZrInstruction create_instruction_0(EZrInstructionCode opcode, TUInt16 operandExtra);
TZrInstruction create_instruction_1(EZrInstructionCode opcode, TUInt16 operandExtra, TInt32 operand);
TZrInstruction create_instruction_2(EZrInstructionCode opcode, TUInt16 operandExtra, TUInt16 operand1, TUInt16 operand2);
void emit_instruction(SZrCompilerState *cs, TZrInstruction instruction);
TUInt32 add_constant(SZrCompilerState *cs, SZrTypeValue *value);
TUInt32 find_local_var(SZrCompilerState *cs, SZrString *name);
TUInt32 allocate_stack_slot(SZrCompilerState *cs);
TUInt32 allocate_local_var(SZrCompilerState *cs, SZrString *name);
TZrSize create_label(SZrCompilerState *cs);
void resolve_label(SZrCompilerState *cs, TZrSize labelId);
void add_pending_jump(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId);
void enter_scope(SZrCompilerState *cs);
void exit_scope(SZrCompilerState *cs);
void compile_statement(SZrCompilerState *cs, SZrAstNode *node);

// 编译字面量
static void compile_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    SZrTypeValue constantValue;
    ZrValueResetAsNull(&constantValue);
    TUInt32 constantIndex = 0;
    TUInt32 destSlot = allocate_stack_slot(cs);
    
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL: {
            TBool value = node->data.booleanLiteral.value;
            ZrValueInitAsInt(cs->state, &constantValue, value ? 1 : 0);
            constantValue.type = ZR_VALUE_TYPE_BOOL;
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_INTEGER_LITERAL: {
            TInt64 value = node->data.integerLiteral.value;
            ZrValueInitAsInt(cs->state, &constantValue, value);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_FLOAT_LITERAL: {
            TDouble value = node->data.floatLiteral.value;
            ZrValueInitAsFloat(cs->state, &constantValue, value);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_STRING_LITERAL: {
            SZrString *value = node->data.stringLiteral.value;
            if (value != ZR_NULL) {
                ZrValueInitAsRawObject(cs->state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
                constantValue.type = ZR_VALUE_TYPE_STRING;
                constantIndex = add_constant(cs, &constantValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
                emit_instruction(cs, inst);
            }
            break;
        }
        
        case ZR_AST_CHAR_LITERAL: {
            TChar value = node->data.charLiteral.value;
            ZrValueInitAsInt(cs->state, &constantValue, (TInt64)value);
            constantValue.type = ZR_VALUE_TYPE_INT8;
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_NULL_LITERAL: {
            ZrValueResetAsNull(&constantValue);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        default:
            ZrCompilerError(cs, "Unexpected literal type", node->location);
            break;
    }
}

// 编译标识符
static void compile_identifier(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_IDENTIFIER_LITERAL) {
        ZrCompilerError(cs, "Expected identifier", node->location);
        return;
    }
    
    SZrString *name = node->data.identifier.name;
    if (name == ZR_NULL) {
        ZrCompilerError(cs, "Identifier name is null", node->location);
        return;
    }
    
    // 查找局部变量
    TUInt32 localVarIndex = find_local_var(cs, name);
    if (localVarIndex != (TUInt32)-1) {
        // 局部变量：使用 GET_STACK
        TUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), destSlot, (TInt32)localVarIndex);
        emit_instruction(cs, inst);
        return;
    }
    
    // 检查是否是全局关键字 "zr"
    TNativeString varNameStr;
    TZrSize nameLen;
    if (name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        varNameStr = ZrStringGetNativeStringShort(name);
        nameLen = name->shortStringLength;
    } else {
        varNameStr = ZrStringGetNativeString(name);
        nameLen = name->longStringLength;
    }
    
    // 检查是否是 "zr" 全局对象
    if (nameLen == 2 && varNameStr[0] == 'z' && varNameStr[1] == 'r') {
        // 生成访问全局注册表的代码
        // 使用 GET_STACK 指令，偏移量为 ZR_VM_STACK_GLOBAL_MODULE_REGISTRY
        // 注意：这里需要将全局注册表的值放到栈上
        TUInt32 destSlot = allocate_stack_slot(cs);
        // 使用负偏移量访问全局注册表
        // ZR_VM_STACK_GLOBAL_MODULE_REGISTRY 是一个负偏移量
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), destSlot, ZR_VM_STACK_GLOBAL_MODULE_REGISTRY);
        emit_instruction(cs, inst);
        return;
    }
    
    // TODO: 查找闭包变量
    // TODO: 查找其他全局变量
    
    // 如果找不到，报告错误（包含变量名）
    // 创建详细的错误消息
    static TChar errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg), "Undefined variable: %.*s", 
             (int)nameLen, varNameStr);
    
    ZrCompilerError(cs, errorMsg, node->location);
}

// 编译一元表达式
static void compile_unary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_UNARY_EXPRESSION) {
        ZrCompilerError(cs, "Expected unary expression", node->location);
        return;
    }
    
    const TChar *op = node->data.unaryExpression.op.op;
    SZrAstNode *arg = node->data.unaryExpression.argument;
    
    // 先编译操作数
    compile_expression(cs, arg);
    
    TUInt32 argSlot = cs->stackSlotCount - 1;
    TUInt32 destSlot = allocate_stack_slot(cs);
    
    if (strcmp(op, "!") == 0) {
        // 逻辑非
        TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_NOT), destSlot, (TUInt16)argSlot, 0);
        emit_instruction(cs, inst);
    } else if (strcmp(op, "~") == 0) {
        // 位非
        TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_NOT), destSlot, (TUInt16)argSlot, 0);
        emit_instruction(cs, inst);
    } else if (strcmp(op, "-") == 0) {
        // 取负
        TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(NEG), destSlot, (TUInt16)argSlot, 0);
        emit_instruction(cs, inst);
    } else if (strcmp(op, "+") == 0) {
        // 正号：直接使用操作数（不需要额外指令）
        // 将结果复制到目标槽位
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), destSlot, (TInt32)argSlot);
        emit_instruction(cs, inst);
    } else {
        ZrCompilerError(cs, "Unknown unary operator", node->location);
    }
}

// 编译二元表达式
static void compile_binary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_BINARY_EXPRESSION) {
        ZrCompilerError(cs, "Expected binary expression", node->location);
        return;
    }
    
    const TChar *op = node->data.binaryExpression.op.op;
    SZrAstNode *left = node->data.binaryExpression.left;
    SZrAstNode *right = node->data.binaryExpression.right;
    
    // 编译左操作数
    compile_expression(cs, left);
    TUInt32 leftSlot = cs->stackSlotCount - 1;
    
    // 编译右操作数
    compile_expression(cs, right);
    TUInt32 rightSlot = cs->stackSlotCount - 1;
    
    TUInt32 destSlot = allocate_stack_slot(cs);
    EZrInstructionCode opcode = ZR_INSTRUCTION_OP_ADD;  // 默认
    
    // 根据操作符选择指令
    if (strcmp(op, "+") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(ADD_INT);  // TODO: 类型推断选择 ADD_INT/ADD_FLOAT/ADD_STRING
    } else if (strcmp(op, "-") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(SUB_INT);  // TODO: 类型推断
    } else if (strcmp(op, "*") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);  // TODO: 类型推断
    } else if (strcmp(op, "/") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);  // TODO: 类型推断
    } else if (strcmp(op, "%") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);  // TODO: 类型推断
    } else if (strcmp(op, "**") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(POW_SIGNED);  // TODO: 类型推断
    } else if (strcmp(op, "<<") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT);
    } else if (strcmp(op, ">>") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT);
    } else if (strcmp(op, "==") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL);
    } else if (strcmp(op, "!=") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL);
    } else if (strcmp(op, "<") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED);  // TODO: 类型推断
    } else if (strcmp(op, ">") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED);  // TODO: 类型推断
    } else if (strcmp(op, "<=") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED);  // TODO: 类型推断
    } else if (strcmp(op, ">=") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED);  // TODO: 类型推断
    } else if (strcmp(op, "&&") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_AND);
    } else if (strcmp(op, "||") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_OR);
    } else if (strcmp(op, "&") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_AND);
    } else if (strcmp(op, "|") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_OR);
    } else if (strcmp(op, "^") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_XOR);
    } else {
        ZrCompilerError(cs, "Unknown binary operator", node->location);
        return;
    }
    
    TZrInstruction inst = create_instruction_2(opcode, destSlot, (TUInt16)leftSlot, (TUInt16)rightSlot);
    emit_instruction(cs, inst);
}

// 编译赋值表达式
static void compile_assignment_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        ZrCompilerError(cs, "Expected assignment expression", node->location);
        return;
    }
    
    const TChar *op = node->data.assignmentExpression.op.op;
    SZrAstNode *left = node->data.assignmentExpression.left;
    SZrAstNode *right = node->data.assignmentExpression.right;
    
    // 编译右值
    compile_expression(cs, right);
    TUInt32 rightSlot = cs->stackSlotCount - 1;
    
    // 处理左值（标识符）
    if (left->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *name = left->data.identifier.name;
        TUInt32 localVarIndex = find_local_var(cs, name);
        
        if (localVarIndex != (TUInt32)-1) {
            // 简单赋值
            if (strcmp(op, "=") == 0) {
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)localVarIndex, (TInt32)rightSlot);
                emit_instruction(cs, inst);
            } else {
                // 复合赋值：先读取左值，执行运算，再赋值
                TUInt32 leftSlot = allocate_stack_slot(cs);
                TZrInstruction getInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), leftSlot, (TInt32)localVarIndex);
                emit_instruction(cs, getInst);
                
                // 执行运算
                EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                if (strcmp(op, "+=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                } else if (strcmp(op, "-=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                } else if (strcmp(op, "*=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                } else if (strcmp(op, "/=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                } else if (strcmp(op, "%=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                }
                
                TUInt32 resultSlot = allocate_stack_slot(cs);
                TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TUInt16)leftSlot, (TUInt16)rightSlot);
                emit_instruction(cs, opInst);
                
                // 赋值
                TZrInstruction setInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)localVarIndex, (TInt32)resultSlot);
                emit_instruction(cs, setInst);
            }
        } else {
            // TODO: 处理闭包变量和全局变量
            ZrCompilerError(cs, "Undefined variable in assignment", node->location);
        }
    } else {
        // TODO: 处理成员访问等复杂左值
        ZrCompilerError(cs, "Complex left-hand side not supported yet", node->location);
    }
}

// 编译逻辑表达式（&& 和 ||，支持短路求值）
static void compile_logical_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_LOGICAL_EXPRESSION) {
        ZrCompilerError(cs, "Expected logical expression", node->location);
        return;
    }
    
    const TChar *op = node->data.logicalExpression.op;
    SZrAstNode *left = node->data.logicalExpression.left;
    SZrAstNode *right = node->data.logicalExpression.right;
    
    // 编译左操作数
    compile_expression(cs, left);
    TUInt32 leftSlot = cs->stackSlotCount - 1;
    
    // 分配结果槽位
    TUInt32 destSlot = allocate_stack_slot(cs);
    
    // 创建标签用于短路求值
    TZrSize shortCircuitLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    if (strcmp(op, "&&") == 0) {
        // && 运算符：如果左操作数为false，短路返回false
        // 复制左操作数到结果槽位
        TZrInstruction copyLeftInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)destSlot, (TInt32)leftSlot);
        emit_instruction(cs, copyLeftInst);
        
        // 如果左操作数为false，跳转到短路标签
        TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)leftSlot, 0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, shortCircuitLabelId);
        
        // 编译右操作数
        compile_expression(cs, right);
        TUInt32 rightSlot = cs->stackSlotCount - 1;
        
        // 将右操作数复制到结果槽位
        TZrInstruction copyRightInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)destSlot, (TInt32)rightSlot);
        emit_instruction(cs, copyRightInst);
        
        // 跳转到结束标签
        TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpEndIndex = cs->instructionCount;
        emit_instruction(cs, jumpEndInst);
        add_pending_jump(cs, jumpEndIndex, endLabelId);
        
        // 短路标签：左操作数为false，结果就是false（已经在destSlot中）
        resolve_label(cs, shortCircuitLabelId);
        
        // 结束标签
        resolve_label(cs, endLabelId);
    } else if (strcmp(op, "||") == 0) {
        // || 运算符：如果左操作数为true，短路返回true
        // 复制左操作数到结果槽位
        TZrInstruction copyLeftInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)destSlot, (TInt32)leftSlot);
        emit_instruction(cs, copyLeftInst);
        
        // 如果左操作数为true，跳转到结束标签（短路返回true）
        TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)leftSlot, 0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, endLabelId);
        
        // 左操作数为false，需要计算右操作数
        // 编译右操作数
        compile_expression(cs, right);
        TUInt32 rightSlot = cs->stackSlotCount - 1;
        
        // 将右操作数复制到结果槽位
        TZrInstruction copyRightInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)destSlot, (TInt32)rightSlot);
        emit_instruction(cs, copyRightInst);
        
        // 结束标签：左操作数为true时跳转到这里（结果已经是true）
        resolve_label(cs, endLabelId);
    } else {
        ZrCompilerError(cs, "Unknown logical operator", node->location);
        return;
    }
}

// 编译条件表达式（三元运算符）
static void compile_conditional_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_CONDITIONAL_EXPRESSION) {
        ZrCompilerError(cs, "Expected conditional expression", node->location);
        return;
    }
    
    SZrAstNode *test = node->data.conditionalExpression.test;
    SZrAstNode *consequent = node->data.conditionalExpression.consequent;
    SZrAstNode *alternate = node->data.conditionalExpression.alternate;
    
    // 编译条件
    compile_expression(cs, test);
    TUInt32 testSlot = cs->stackSlotCount - 1;
    
    // 创建 else 和 end 标签
    TZrSize elseLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)testSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, elseLabelId);
    
    // 编译 then 分支
    compile_expression(cs, consequent);
    
    // JUMP -> end
    TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpEndInst);
    add_pending_jump(cs, jumpEndIndex, endLabelId);
    
    // 解析 else 标签
    resolve_label(cs, elseLabelId);
    
    // 编译 else 分支
    compile_expression(cs, alternate);
    
    // 解析 end 标签
    resolve_label(cs, endLabelId);
}

// 编译函数调用表达式
static void compile_function_call(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_FUNCTION_CALL) {
        ZrCompilerError(cs, "Expected function call", node->location);
        return;
    }
    
    // 函数调用在 primary expression 中处理
    // 这里只处理参数列表
    SZrAstNodeArray *args = node->data.functionCall.args;
    if (args != ZR_NULL) {
        // 编译所有参数表达式（从右到左压栈，或从左到右，取决于调用约定）
        for (TZrSize i = 0; i < args->count; i++) {
            SZrAstNode *arg = args->nodes[i];
            if (arg != ZR_NULL) {
                compile_expression(cs, arg);
            }
        }
    }
    
    // 注意：实际的函数调用指令（FUNCTION_CALL）应该在 primary expression 中生成
    // 因为需要先编译 callee（被调用表达式）
}

// 编译成员访问表达式
static void compile_member_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_MEMBER_EXPRESSION) {
        ZrCompilerError(cs, "Expected member expression", node->location);
        return;
    }
    
    // 成员访问在 primary expression 中处理
    // 这里只处理属性访问
    // 注意：实际的 GETTABLE/SETTABLE 指令应该在 primary expression 中生成
    // 因为需要先编译对象表达式
}

// 编译主表达式（属性访问链和函数调用链）
static void compile_primary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_PRIMARY_EXPRESSION) {
        ZrCompilerError(cs, "Expected primary expression", node->location);
        return;
    }
    
    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    
    // 1. 编译基础属性（标识符或表达式）
    if (primary->property != ZR_NULL) {
        compile_expression(cs, primary->property);
    } else {
        ZrCompilerError(cs, "Primary expression property is null", node->location);
        return;
    }
    
    TUInt32 currentSlot = cs->stackSlotCount - 1;
    
    // 2. 依次处理成员访问链和函数调用链
    if (primary->members != ZR_NULL) {
        for (TZrSize i = 0; i < primary->members->count; i++) {
            SZrAstNode *member = primary->members->nodes[i];
            if (member == ZR_NULL) {
                continue;
            }
            
            if (member->type == ZR_AST_MEMBER_EXPRESSION) {
                // 成员访问：obj.member 或 obj[key]
                SZrMemberExpression *memberExpr = &member->data.memberExpression;
                
                // 编译键表达式（标识符或计算属性）
                if (memberExpr->property != ZR_NULL) {
                    compile_expression(cs, memberExpr->property);
                    TUInt32 keySlot = cs->stackSlotCount - 1;
                    
                    // 使用 GETTABLE 获取值
                    TUInt32 resultSlot = allocate_stack_slot(cs);
                    TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TUInt16)resultSlot, (TUInt16)currentSlot, (TUInt16)keySlot);
                    emit_instruction(cs, getTableInst);
                    
                    // 释放临时栈槽
                    cs->stackSlotCount -= 2;  // keySlot 和 currentSlot
                    currentSlot = resultSlot;
                }
            } else if (member->type == ZR_AST_FUNCTION_CALL) {
                // 函数调用：obj.method(args)
                SZrFunctionCall *call = &member->data.functionCall;
                
                // 编译参数
                if (call->args != ZR_NULL) {
                    for (TZrSize j = 0; j < call->args->count; j++) {
                        SZrAstNode *arg = call->args->nodes[j];
                        if (arg != ZR_NULL) {
                            compile_expression(cs, arg);
                        }
                    }
                }
                
                // 生成函数调用指令
                TUInt32 argCount = (call->args != ZR_NULL) ? (TUInt32)call->args->count : 0;
                TUInt32 resultSlot = allocate_stack_slot(cs);
                TZrInstruction callInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), (TUInt16)resultSlot, (TUInt16)currentSlot, (TUInt16)argCount);
                emit_instruction(cs, callInst);
                
                // 释放参数栈槽和函数槽
                cs->stackSlotCount -= (argCount + 1);  // 参数 + 函数
                currentSlot = resultSlot;
            }
        }
    }
}

// 编译数组字面量
static void compile_array_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_ARRAY_LITERAL) {
        ZrCompilerError(cs, "Expected array literal", node->location);
        return;
    }
    
    SZrArrayLiteral *arrayLiteral = &node->data.arrayLiteral;
    
    // 1. 创建空数组对象
    TUInt32 destSlot = allocate_stack_slot(cs);
    TZrInstruction createArrayInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_ARRAY), (TUInt16)destSlot);
    emit_instruction(cs, createArrayInst);
    
    // 2. 编译每个元素并设置到数组中
    if (arrayLiteral->elements != ZR_NULL) {
        for (TZrSize i = 0; i < arrayLiteral->elements->count; i++) {
            SZrAstNode *elemNode = arrayLiteral->elements->nodes[i];
            if (elemNode == ZR_NULL) {
                continue;
            }
            
            // 编译元素值
            compile_expression(cs, elemNode);
            TUInt32 valueSlot = cs->stackSlotCount - 1;
            
            // 创建索引常量
            SZrTypeValue indexValue;
            ZrValueInitAsInt(cs->state, &indexValue, (TInt64)i);
            TUInt32 indexConstantIndex = add_constant(cs, &indexValue);
            
            // 将索引压栈
            TUInt32 indexSlot = allocate_stack_slot(cs);
            TZrInstruction getIndexInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)indexSlot, (TInt32)indexConstantIndex);
            emit_instruction(cs, getIndexInst);
            
            // 使用 SETTABLE 设置数组元素
            // SETTABLE 的格式: operandExtra = valueSlot (destination/value), operand1[0] = tableSlot, operand1[1] = keySlot
            TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TUInt16)valueSlot, (TUInt16)destSlot, (TUInt16)indexSlot);
            emit_instruction(cs, setTableInst);
            
            // 释放临时栈槽（indexSlot 和 valueSlot）
            cs->stackSlotCount -= 2;
        }
    }
    
    // 3. 数组对象已经在 destSlot，结果留在 destSlot
}

// 编译对象字面量
static void compile_object_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_OBJECT_LITERAL) {
        ZrCompilerError(cs, "Expected object literal", node->location);
        return;
    }
    
    SZrObjectLiteral *objectLiteral = &node->data.objectLiteral;
    
    // 1. 创建空对象
    TUInt32 destSlot = allocate_stack_slot(cs);
    TZrInstruction createObjectInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), (TUInt16)destSlot);
    emit_instruction(cs, createObjectInst);
    
    // 2. 编译每个键值对并设置到对象中
    if (objectLiteral->properties != ZR_NULL) {
        for (TZrSize i = 0; i < objectLiteral->properties->count; i++) {
            SZrAstNode *kvNode = objectLiteral->properties->nodes[i];
            if (kvNode == ZR_NULL || kvNode->type != ZR_AST_KEY_VALUE_PAIR) {
                continue;
            }
            
            SZrKeyValuePair *kv = &kvNode->data.keyValuePair;
            
            // 编译键
            if (kv->key != ZR_NULL) {
                // 键可能是标识符、字符串字面量或表达式（计算键）
                if (kv->key->type == ZR_AST_IDENTIFIER_LITERAL) {
                    // 标识符键：转换为字符串常量
                    SZrString *keyName = kv->key->data.identifier.name;
                    if (keyName != ZR_NULL) {
                        SZrTypeValue keyValue;
                        ZrValueInitAsRawObject(cs->state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyName));
                        keyValue.type = ZR_VALUE_TYPE_STRING;
                        TUInt32 keyConstantIndex = add_constant(cs, &keyValue);
                        
                        TUInt32 keySlot = allocate_stack_slot(cs);
                        TZrInstruction getKeyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)keySlot, (TInt32)keyConstantIndex);
                        emit_instruction(cs, getKeyInst);
                        
                        // 编译值
                        compile_expression(cs, kv->value);
                        TUInt32 valueSlot = cs->stackSlotCount - 1;
                        
                        // 使用 SETTABLE 设置对象属性
                        TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TUInt16)valueSlot, (TUInt16)destSlot, (TUInt16)keySlot);
                        emit_instruction(cs, setTableInst);
                        
                        // 释放临时栈槽
                        cs->stackSlotCount -= 2;
                    }
                } else {
                    // 键是表达式（字符串字面量或计算键）
                    compile_expression(cs, kv->key);
                    TUInt32 keySlot = cs->stackSlotCount - 1;
                    
                    // 编译值
                    compile_expression(cs, kv->value);
                    TUInt32 valueSlot = cs->stackSlotCount - 1;
                    
                    // 使用 SETTABLE 设置对象属性
                    TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TUInt16)valueSlot, (TUInt16)destSlot, (TUInt16)keySlot);
                    emit_instruction(cs, setTableInst);
                    
                    // 释放临时栈槽
                    cs->stackSlotCount -= 2;
                }
            }
        }
    }
    
    // 3. 对象已经在 destSlot，结果留在 destSlot
}

// 编译 Lambda 表达式
static void compile_lambda_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_LAMBDA_EXPRESSION) {
        ZrCompilerError(cs, "Expected lambda expression", node->location);
        return;
    }
    
    SZrLambdaExpression *lambda = &node->data.lambdaExpression;
    
    // Lambda 表达式类似于匿名函数，需要创建一个嵌套函数
    // 1. 创建一个临时的函数声明节点来复用函数编译逻辑
    // 2. 或者直接在这里实现类似的逻辑
    
    // 保存当前编译器状态
    SZrFunction *oldFunction = cs->currentFunction;
    TZrSize oldInstructionCount = cs->instructionCount;
    TZrSize oldStackSlotCount = cs->stackSlotCount;
    TZrSize oldLocalVarCount = cs->localVarCount;
    TZrSize oldConstantCount = cs->constantCount;
    TZrSize oldClosureVarCount = cs->closureVarCount;
    
    // 创建新的函数对象
    cs->currentFunction = ZrFunctionNew(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrCompilerError(cs, "Failed to create lambda function object", node->location);
        return;
    }
    
    // 重置编译器状态（为新函数）
    cs->instructionCount = 0;
    cs->stackSlotCount = 0;
    cs->localVarCount = 0;
    cs->constantCount = 0;
    cs->closureVarCount = 0;
    
    // 清空数组（但保留已分配的内存）
    cs->instructions.length = 0;
    cs->localVars.length = 0;
    cs->constants.length = 0;
    cs->closureVars.length = 0;
    
    // 进入函数作用域
    enter_scope(cs);
    
    // 1. 编译参数列表
    TUInt32 parameterCount = 0;
    if (lambda->params != ZR_NULL) {
        for (TZrSize i = 0; i < lambda->params->count; i++) {
            SZrAstNode *paramNode = lambda->params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
                    if (paramName != ZR_NULL) {
                        // 分配参数槽位
                        TUInt32 paramIndex = allocate_local_var(cs, paramName);
                        parameterCount++;
                        
                        // 如果有默认值，编译默认值表达式
                        if (param->defaultValue != ZR_NULL) {
                            compile_expression(cs, param->defaultValue);
                            TUInt32 defaultSlot = cs->stackSlotCount - 1;
                            
                            // 生成 SET_STACK 指令
                            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)paramIndex, (TInt32)defaultSlot);
                            emit_instruction(cs, inst);
                        }
                    }
                }
            }
        }
    }
    
    // 检查是否有可变参数
    TBool hasVariableArguments = (lambda->args != ZR_NULL);
    
    // 2. 编译函数体（block）
    if (lambda->block != ZR_NULL) {
        compile_statement(cs, lambda->block);
    }
    
    // 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回 null
            TUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrValueResetAsNull(&nullValue);
            TUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)resultSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            
            TZrInstruction returnInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16)resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst = (TZrInstruction *)ZrArrayGet(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode)lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrValueResetAsNull(&nullValue);
                        TUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)resultSlot, (TInt32)constantIndex);
                        emit_instruction(cs, inst);
                        
                        TZrInstruction returnInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16)resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            }
        }
    }
    
    // 退出函数作用域
    exit_scope(cs);
    
    // 3. 将编译结果复制到函数对象
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;
    
    // 复制指令列表
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList = (TZrInstruction *)ZrMemoryRawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TUInt32)cs->instructions.length;
            cs->instructionCount = cs->instructions.length;
        }
    }
    
    // 复制常量列表
    if (cs->constantCount > 0) {
        TZrSize constSize = cs->constantCount * sizeof(SZrTypeValue);
        newFunc->constantValueList = (SZrTypeValue *)ZrMemoryRawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TUInt32)cs->constantCount;
        }
    }
    
    // 复制局部变量列表
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *)ZrMemoryRawMallocWithType(global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TUInt32)cs->localVars.length;
            cs->localVarCount = cs->localVars.length;
        }
    }
    
    // 复制闭包变量列表
    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *)ZrMemoryRawMallocWithType(global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TUInt32)cs->closureVarCount;
        }
    }
    
    // 设置函数元数据
    newFunc->stackSize = (TUInt32)cs->stackSlotCount;
    newFunc->parameterCount = (TUInt16)parameterCount;
    newFunc->hasVariableArguments = hasVariableArguments;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TUInt32)node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TUInt32)node->location.end.line : 0;
    
    // 将新函数添加到子函数列表
    if (oldFunction != ZR_NULL) {
        ZrArrayPush(cs->state, &cs->childFunctions, &newFunc);
    }
    
    // 4. 生成 CREATE_CLOSURE 指令（在恢复状态之前）
    // 将函数对象添加到常量池，然后生成 CREATE_CLOSURE 指令
    // 注意：这里简化处理，直接将函数对象作为常量
    // TODO: 完整的闭包处理需要捕获外部变量
    SZrTypeValue funcValue;
    ZrValueInitAsRawObject(cs->state, &funcValue, ZR_CAST_RAW_OBJECT_AS_SUPER(newFunc));
    // 函数对象在 VM 中通常作为 CLOSURE 类型处理
    funcValue.type = ZR_VALUE_TYPE_CLOSURE;
    TUInt32 funcConstantIndex = add_constant(cs, &funcValue);
    
    // 分配目标槽位
    TUInt32 destSlot = allocate_stack_slot(cs);
    
    // 保存闭包变量数量（在恢复状态之前）
    TUInt32 closureVarCount = (TUInt32)newFunc->closureValueLength;
    
    // 恢复旧的编译器状态
    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
    
    // 生成 CREATE_CLOSURE 指令
    // CREATE_CLOSURE 的格式: operandExtra = destSlot, operand1[0] = functionConstantIndex, operand1[1] = closureVarCount
    TZrInstruction createClosureInst = create_instruction_2(ZR_INSTRUCTION_ENUM(CREATE_CLOSURE), (TUInt16)destSlot, (TUInt16)funcConstantIndex, (TUInt16)closureVarCount);
    emit_instruction(cs, createClosureInst);
}

// 编译 If 表达式
static void compile_if_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_IF_EXPRESSION) {
        ZrCompilerError(cs, "Expected if expression", node->location);
        return;
    }
    
    SZrIfExpression *ifExpr = &node->data.ifExpression;
    
    // 编译条件表达式
    compile_expression(cs, ifExpr->condition);
    TUInt32 condSlot = cs->stackSlotCount - 1;
    
    // 创建 else 和 end 标签
    TZrSize elseLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)condSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, elseLabelId);
    
    // 编译 then 分支
    if (ifExpr->thenExpr != ZR_NULL) {
        compile_expression(cs, ifExpr->thenExpr);
    } else {
        // 如果没有then分支，使用null值
        SZrTypeValue nullValue;
        ZrValueResetAsNull(&nullValue);
        TUInt32 constantIndex = add_constant(cs, &nullValue);
        TUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
        emit_instruction(cs, inst);
    }
    
    // JUMP -> end
    TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpEndInst);
    add_pending_jump(cs, jumpEndIndex, endLabelId);
    
    // 解析 else 标签
    resolve_label(cs, elseLabelId);
    
    // 编译 else 分支
    if (ifExpr->elseExpr != ZR_NULL) {
        compile_expression(cs, ifExpr->elseExpr);
    } else {
        // 如果没有else分支，使用null值
        SZrTypeValue nullValue;
        ZrValueResetAsNull(&nullValue);
        TUInt32 constantIndex = add_constant(cs, &nullValue);
        TUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
        emit_instruction(cs, inst);
    }
    
    // 解析 end 标签
    resolve_label(cs, endLabelId);
}

// 编译 Switch 表达式
static void compile_switch_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    // TODO: 实现 Switch 表达式编译
    ZrCompilerError(cs, "Switch expression compilation not implemented yet", node->location);
}

// 主编译表达式函数
void compile_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    switch (node->type) {
        // 字面量
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            compile_literal(cs, node);
            break;
        
        case ZR_AST_IDENTIFIER_LITERAL:
            compile_identifier(cs, node);
            break;
        
        // 表达式
        case ZR_AST_UNARY_EXPRESSION:
            compile_unary_expression(cs, node);
            break;
        
        case ZR_AST_BINARY_EXPRESSION:
            compile_binary_expression(cs, node);
            break;
        
        case ZR_AST_LOGICAL_EXPRESSION:
            compile_logical_expression(cs, node);
            break;
        
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            compile_assignment_expression(cs, node);
            break;
        
        case ZR_AST_CONDITIONAL_EXPRESSION:
            compile_conditional_expression(cs, node);
            break;
        
        case ZR_AST_FUNCTION_CALL:
            compile_function_call(cs, node);
            break;
        
        case ZR_AST_MEMBER_EXPRESSION:
            compile_member_expression(cs, node);
            break;
        
        case ZR_AST_PRIMARY_EXPRESSION:
            compile_primary_expression(cs, node);
            break;
        
        case ZR_AST_ARRAY_LITERAL:
            compile_array_literal(cs, node);
            break;
        
        case ZR_AST_OBJECT_LITERAL:
            compile_object_literal(cs, node);
            break;
        
        case ZR_AST_LAMBDA_EXPRESSION:
            compile_lambda_expression(cs, node);
            break;
        
        case ZR_AST_IF_EXPRESSION:
            compile_if_expression(cs, node);
            break;
        
        case ZR_AST_SWITCH_EXPRESSION:
            compile_switch_expression(cs, node);
            break;
        
        // 循环和语句不应该作为表达式编译，应该先转换为语句
        case ZR_AST_WHILE_LOOP:
        case ZR_AST_FOR_LOOP:
        case ZR_AST_FOREACH_LOOP:
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
        case ZR_AST_OUT_STATEMENT:
        case ZR_AST_THROW_STATEMENT:
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            ZrCompilerError(cs, "Loop or statement cannot be used as expression", node->location);
            break;
        
        // 解构对象和数组不是表达式，不应该在这里处理
        case ZR_AST_DESTRUCTURING_OBJECT:
        case ZR_AST_DESTRUCTURING_ARRAY:
            // 这些类型应该在变量声明中处理，不应该作为表达式编译
            ZrCompilerError(cs, "Destructuring pattern cannot be used as expression", node->location);
            break;
        
        default:
            // 未处理的表达式类型
            if (node->type == ZR_AST_DESTRUCTURING_OBJECT || node->type == ZR_AST_DESTRUCTURING_ARRAY) {
                // 这不应该作为表达式编译，应该在变量声明中处理
                ZrCompilerError(cs, "Destructuring pattern cannot be used as expression", node->location);
            } else {
                // 创建详细的错误消息，包含类型编号
                static TChar errorMsg[128];
                snprintf(errorMsg, sizeof(errorMsg), "Unexpected expression type: %d", node->type);
                ZrCompilerError(cs, errorMsg, node->location);
            }
            break;
    }
}

