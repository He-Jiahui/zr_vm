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
    
    // 创建 else 标签
    TZrSize elseLabelId = cs->labels.length;  // 将在后面创建
    TZrSize endLabelId = cs->labels.length + 1;
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)testSlot, 0);  // 偏移将在后面填充
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    
    // 编译 then 分支
    compile_expression(cs, consequent);
    TUInt32 thenSlot = cs->stackSlotCount - 1;
    
    // JUMP -> end
    TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);  // 偏移将在后面填充
    TZrSize jumpEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpEndInst);
    
    // 解析 else 标签
    elseLabelId = cs->labels.length;
    // TODO: 创建标签并解析跳转
    
    // 编译 else 分支
    compile_expression(cs, alternate);
    TUInt32 elseSlot = cs->stackSlotCount - 1;
    
    // 解析 end 标签
    endLabelId = cs->labels.length;
    // TODO: 创建标签并解析跳转
    
    // TODO: 填充跳转偏移
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
    
    // TODO: 实现数组字面量编译
    ZrCompilerError(cs, "Array literal compilation not implemented yet", node->location);
}

// 编译对象字面量
static void compile_object_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    // TODO: 实现对象字面量编译
    ZrCompilerError(cs, "Object literal compilation not implemented yet", node->location);
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
    
    // TODO: 实现 Lambda 表达式编译
    // 1. 创建新的编译器状态（嵌套函数）
    // 2. 编译参数列表
    // 3. 编译函数体
    // 4. 创建 SZrFunction 对象
    // 5. 处理闭包变量
    // 6. 生成 CREATE_CLOSURE 指令
    
    // 占位实现
    ZrCompilerError(cs, "Lambda expression compilation not fully implemented yet", node->location);
}

// 编译 If 表达式
static void compile_if_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    // If 表达式与条件表达式类似
    compile_conditional_expression(cs, node);
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

