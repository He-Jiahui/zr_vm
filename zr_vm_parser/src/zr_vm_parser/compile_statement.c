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

#include <string.h>

// 前向声明
extern void compile_expression(SZrCompilerState *cs, SZrAstNode *node);
void compile_statement(SZrCompilerState *cs, SZrAstNode *node);
static void compile_destructuring_object(SZrCompilerState *cs, SZrAstNode *pattern, SZrAstNode *value);
static void compile_destructuring_array(SZrCompilerState *cs, SZrAstNode *pattern, SZrAstNode *value);

// 辅助函数声明（在 compiler.c 中实现）
extern void emit_instruction(SZrCompilerState *cs, TZrInstruction instruction);
extern TZrInstruction create_instruction_0(EZrInstructionCode opcode, TUInt16 operandExtra);
extern TZrInstruction create_instruction_1(EZrInstructionCode opcode, TUInt16 operandExtra, TInt32 operand);
extern TZrInstruction create_instruction_2(EZrInstructionCode opcode, TUInt16 operandExtra, TUInt16 operand1, TUInt16 operand2);
extern TUInt32 allocate_local_var(SZrCompilerState *cs, SZrString *name);
extern TUInt32 find_local_var(SZrCompilerState *cs, SZrString *name);
extern TUInt32 allocate_stack_slot(SZrCompilerState *cs);
extern TUInt32 add_constant(SZrCompilerState *cs, SZrTypeValue *value);
extern void enter_scope(SZrCompilerState *cs);
extern void exit_scope(SZrCompilerState *cs);
extern TZrSize create_label(SZrCompilerState *cs);
extern void resolve_label(SZrCompilerState *cs, TZrSize labelId);
extern void add_pending_jump(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId);

// 编译变量声明
static void compile_variable_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_VARIABLE_DECLARATION) {
        ZrCompilerError(cs, "Expected variable declaration", node->location);
        return;
    }
    
    SZrVariableDeclaration *decl = &node->data.variableDeclaration;
    
    // 检查 pattern 是否存在
    if (decl->pattern == ZR_NULL) {
        ZrCompilerError(cs, "Variable declaration pattern is null", node->location);
        return;
    }
    
    // 处理单个变量声明（标识符 pattern）
    if (decl->pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *varName = decl->pattern->data.identifier.name;
        if (varName == ZR_NULL) {
            ZrCompilerError(cs, "Variable name is null", node->location);
            return;
        }
        
        // 分配局部变量槽位
        TUInt32 varIndex = allocate_local_var(cs, varName);
        
        // 如果有初始值，编译初始值表达式
        if (decl->value != ZR_NULL) {
            compile_expression(cs, decl->value);
            TUInt32 initSlot = cs->stackSlotCount - 1;
            
            // 生成 SET_STACK 指令
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)varIndex, (TInt32)initSlot);
            emit_instruction(cs, inst);
        } else {
            // 没有初始值，设置为 null
            SZrTypeValue nullValue;
            ZrValueResetAsNull(&nullValue);
            TUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)varIndex, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            
            TZrInstruction setInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)varIndex, (TInt32)varIndex);
            emit_instruction(cs, setInst);
        }
    } else if (decl->pattern->type == ZR_AST_DESTRUCTURING_OBJECT) {
        // 处理解构对象赋值：var {key1, key2, ...} = value;
        compile_destructuring_object(cs, decl->pattern, decl->value);
    } else if (decl->pattern->type == ZR_AST_DESTRUCTURING_ARRAY) {
        // 处理解构数组赋值：var [elem1, elem2, ...] = value;
        compile_destructuring_array(cs, decl->pattern, decl->value);
    } else {
        // 未知的 pattern 类型
        ZrCompilerError(cs, "Unknown variable declaration pattern type", node->location);
    }
}

// 编译表达式语句
static void compile_expression_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_EXPRESSION_STATEMENT) {
        ZrCompilerError(cs, "Expected expression statement", node->location);
        return;
    }
    
    SZrExpressionStatement *stmt = &node->data.expressionStatement;
    if (stmt->expr != ZR_NULL) {
        // 编译表达式
        compile_expression(cs, stmt->expr);
        // 结果留在栈上，可以被后续使用或自动丢弃
    }
}

// 编译返回语句
static void compile_return_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_RETURN_STATEMENT) {
        ZrCompilerError(cs, "Expected return statement", node->location);
        return;
    }
    
    SZrReturnStatement *stmt = &node->data.returnStatement;
    
    TUInt32 resultSlot = 0;
    TUInt32 resultCount = 0;
    
    if (stmt->expr != ZR_NULL) {
        // 编译返回值表达式
        compile_expression(cs, stmt->expr);
        resultSlot = cs->stackSlotCount - 1;
        resultCount = 1;
    } else {
        // 没有返回值，返回 null
        resultSlot = allocate_stack_slot(cs);
        SZrTypeValue nullValue;
        ZrValueResetAsNull(&nullValue);
        TUInt32 constantIndex = 0;  // TODO: 添加 null 常量到常量池
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)resultSlot, (TInt32)constantIndex);
        emit_instruction(cs, inst);
        resultCount = 1;
    }
    
    // 生成 FUNCTION_RETURN 指令
    // FUNCTION_RETURN 的参数：resultCount, resultSlot
    TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), (TUInt16)resultCount, (TUInt16)resultSlot, 0);
    emit_instruction(cs, inst);
}

// 编译块语句
static void compile_block_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_BLOCK) {
        ZrCompilerError(cs, "Expected block statement", node->location);
        return;
    }
    
    SZrBlock *block = &node->data.block;
    
    // 进入新作用域
    enter_scope(cs);
    
    // 编译块内所有语句
    if (block->body != ZR_NULL) {
        for (TZrSize i = 0; i < block->body->count; i++) {
            SZrAstNode *stmt = block->body->nodes[i];
            if (stmt != ZR_NULL) {
                compile_statement(cs, stmt);
                if (cs->hasError) {
                    break;
                }
            }
        }
    }
    
    // 退出作用域
    exit_scope(cs);
}

// 编译 if 语句
static void compile_if_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    // if 语句和 if 表达式使用相同的 AST 节点类型
    if (node->type != ZR_AST_IF_EXPRESSION) {
        ZrCompilerError(cs, "Expected if statement", node->location);
        return;
    }
    
    SZrIfExpression *ifExpr = &node->data.ifExpression;
    
    // 编译条件表达式
    compile_expression(cs, ifExpr->condition);
    TUInt32 condSlot = cs->stackSlotCount - 1;
    
    // 创建 else 标签
    TZrSize elseLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)condSlot, 0);  // 偏移将在后面填充
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, elseLabelId);
    
    // 编译 then 分支
    if (ifExpr->thenExpr != ZR_NULL) {
        compile_statement(cs, ifExpr->thenExpr);
    }
    
    // JUMP -> end
    TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);  // 偏移将在后面填充
    TZrSize jumpEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpEndInst);
    add_pending_jump(cs, jumpEndIndex, endLabelId);
    
    // 解析 else 标签
    resolve_label(cs, elseLabelId);
    
    // 编译 else 分支
    if (ifExpr->elseExpr != ZR_NULL) {
        compile_statement(cs, ifExpr->elseExpr);
    }
    
    // 解析 end 标签
    resolve_label(cs, endLabelId);
}

// 编译 while 语句
static void compile_while_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_WHILE_LOOP) {
        ZrCompilerError(cs, "Expected while statement", node->location);
        return;
    }
    
    SZrWhileLoop *whileLoop = &node->data.whileLoop;
    
    // 创建循环标签（用于 break/continue）
    SZrLoopLabel loopLabel;
    loopLabel.breakLabelId = create_label(cs);
    loopLabel.continueLabelId = create_label(cs);
    ZrArrayPush(cs->state, &cs->loopLabelStack, &loopLabel);
    
    // 创建循环开始标签（continue 跳转到这里）
    TZrSize loopStartLabelId = loopLabel.continueLabelId;
    resolve_label(cs, loopStartLabelId);
    
    // 编译条件表达式
    compile_expression(cs, whileLoop->cond);
    TUInt32 condSlot = cs->stackSlotCount - 1;
    
    // 创建循环结束标签（break 跳转到这里）
    TZrSize loopEndLabelId = loopLabel.breakLabelId;
    
    // JUMP_IF false -> end
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)condSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, loopEndLabelId);
    
    // 编译循环体
    if (whileLoop->block != ZR_NULL) {
        compile_statement(cs, whileLoop->block);
    }
    
    // JUMP -> loop start
    TZrInstruction jumpLoopInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpLoopIndex = cs->instructionCount;
    emit_instruction(cs, jumpLoopInst);
    add_pending_jump(cs, jumpLoopIndex, loopStartLabelId);
    
    // 解析循环结束标签
    resolve_label(cs, loopEndLabelId);
    
    // 弹出循环标签栈
    ZrArrayPop(&cs->loopLabelStack);
}

// 编译 for 语句
static void compile_for_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_FOR_LOOP) {
        ZrCompilerError(cs, "Expected for statement", node->location);
        return;
    }
    
    SZrForLoop *forLoop = &node->data.forLoop;
    
    // 进入新作用域
    enter_scope(cs);
    
    // 创建循环标签（用于 break/continue）
    SZrLoopLabel loopLabel;
    loopLabel.breakLabelId = create_label(cs);
    loopLabel.continueLabelId = create_label(cs);
    ZrArrayPush(cs->state, &cs->loopLabelStack, &loopLabel);
    
    // 编译初始化表达式
    if (forLoop->init != ZR_NULL) {
        compile_statement(cs, forLoop->init);
    }
    
    // 创建循环开始标签（continue 跳转到这里，在 step 之后）
    TZrSize loopStartLabelId = loopLabel.continueLabelId;
    resolve_label(cs, loopStartLabelId);
    
        // 编译条件表达式
        if (forLoop->cond != ZR_NULL) {
            compile_expression(cs, forLoop->cond);
            TUInt32 condSlot = cs->stackSlotCount - 1;
            
            // 创建循环结束标签（break 跳转到这里）
            TZrSize loopEndLabelId = loopLabel.breakLabelId;
        
        // JUMP_IF false -> end
        TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)condSlot, 0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, loopEndLabelId);
        
        // 编译循环体
        if (forLoop->block != ZR_NULL) {
            compile_statement(cs, forLoop->block);
        }
        
        // 编译增量表达式
        if (forLoop->step != ZR_NULL) {
            compile_expression(cs, forLoop->step);
            // 丢弃结果
        }
        
        // JUMP -> loop start
        TZrInstruction jumpLoopInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpLoopIndex = cs->instructionCount;
        emit_instruction(cs, jumpLoopInst);
        add_pending_jump(cs, jumpLoopIndex, loopStartLabelId);
        
        // 解析循环结束标签
        resolve_label(cs, loopEndLabelId);
    } else {
        // 无限循环
        // 编译循环体
        if (forLoop->block != ZR_NULL) {
            compile_statement(cs, forLoop->block);
        }
        
        // 编译增量表达式
        if (forLoop->step != ZR_NULL) {
            compile_expression(cs, forLoop->step);
        }
        
        // JUMP -> loop start
        TZrInstruction jumpLoopInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpLoopIndex = cs->instructionCount;
        emit_instruction(cs, jumpLoopInst);
        add_pending_jump(cs, jumpLoopIndex, loopStartLabelId);
        
        // 解析循环结束标签（虽然不会到达，但为了完整性）
        resolve_label(cs, loopLabel.breakLabelId);
    }
    
    // 弹出循环标签栈
    ZrArrayPop(&cs->loopLabelStack);
    
    // 退出作用域
    exit_scope(cs);
}

// 编译 foreach 语句
static void compile_foreach_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_FOREACH_LOOP) {
        ZrCompilerError(cs, "Expected foreach statement", node->location);
        return;
    }
    
    // TODO: 实现 foreach 语句编译
    // 1. 编译迭代表达式
    // 2. 创建迭代器
    // 3. 实现迭代循环
    ZrCompilerError(cs, "Foreach statement compilation not fully implemented yet", node->location);
}

// 编译 switch 语句
static void compile_switch_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    // switch 语句和 switch 表达式使用相同的 AST 节点类型
    if (node->type != ZR_AST_SWITCH_EXPRESSION) {
        ZrCompilerError(cs, "Expected switch statement", node->location);
        return;
    }
    
    SZrSwitchExpression *switchExpr = &node->data.switchExpression;
    
    // 编译 switch 表达式
    compile_expression(cs, switchExpr->expr);
    TUInt32 exprSlot = cs->stackSlotCount - 1;
    
    // 创建结束标签
    TZrSize endLabelId = create_label(cs);
    
    // 编译所有 case
    if (switchExpr->cases != ZR_NULL) {
        for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
            SZrAstNode *caseNode = switchExpr->cases->nodes[i];
            if (caseNode != ZR_NULL && caseNode->type == ZR_AST_SWITCH_CASE) {
                SZrSwitchCase *switchCase = &caseNode->data.switchCase;
                
                // 编译 case 值
                compile_expression(cs, switchCase->value);
                TUInt32 caseValueSlot = cs->stackSlotCount - 1;
                
                // 比较表达式和 case 值
                TUInt32 compareSlot = allocate_stack_slot(cs);
                TZrInstruction compareInst = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL), compareSlot, (TUInt16)exprSlot, (TUInt16)caseValueSlot);
                emit_instruction(cs, compareInst);
                
                // 创建下一个 case 标签
                TZrSize nextCaseLabelId = create_label(cs);
                
                // JUMP_IF false -> next case
                TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)compareSlot, 0);
                TZrSize jumpIfIndex = cs->instructionCount;
                emit_instruction(cs, jumpIfInst);
                add_pending_jump(cs, jumpIfIndex, nextCaseLabelId);
                
                // 编译 case 块
                if (switchCase->block != ZR_NULL) {
                    compile_statement(cs, switchCase->block);
                }
                
                // JUMP -> end
                TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
                TZrSize jumpEndIndex = cs->instructionCount;
                emit_instruction(cs, jumpEndInst);
                add_pending_jump(cs, jumpEndIndex, endLabelId);
                
                // 解析下一个 case 标签
                resolve_label(cs, nextCaseLabelId);
            }
        }
    }
    
    // 编译 default case
    if (switchExpr->defaultCase != ZR_NULL) {
        SZrSwitchDefault *defaultCase = &switchExpr->defaultCase->data.switchDefault;
        if (defaultCase->block != ZR_NULL) {
            compile_statement(cs, defaultCase->block);
        }
    }
    
    // 解析结束标签
    resolve_label(cs, endLabelId);
}

// 编译 break/continue 语句
static void compile_break_continue_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_BREAK_CONTINUE_STATEMENT) {
        ZrCompilerError(cs, "Expected break/continue statement", node->location);
        return;
    }
    
    SZrBreakContinueStatement *stmt = &node->data.breakContinueStatement;
    
    // 检查循环标签栈是否为空
    if (cs->loopLabelStack.length == 0) {
        ZrCompilerError(cs, stmt->isBreak ? "break statement not inside a loop" : "continue statement not inside a loop", node->location);
        return;
    }
    
    // 获取最内层循环的标签
    SZrLoopLabel *loopLabel = (SZrLoopLabel *)ZrArrayGet(&cs->loopLabelStack, cs->loopLabelStack.length - 1);
    if (loopLabel == ZR_NULL) {
        ZrCompilerError(cs, "Invalid loop label stack", node->location);
        return;
    }
    
    // 选择目标标签
    TZrSize targetLabelId = stmt->isBreak ? loopLabel->breakLabelId : loopLabel->continueLabelId;
    
    // 如果有表达式，编译它（用于 break value 或 continue value）
    if (stmt->expr != ZR_NULL) {
        compile_expression(cs, stmt->expr);
        // 注意：break/continue 的值通常会被丢弃，但这里先编译表达式
    }
    
    // 生成跳转指令
    TZrInstruction jumpInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpIndex = cs->instructionCount;
    emit_instruction(cs, jumpInst);
    add_pending_jump(cs, jumpIndex, targetLabelId);
}

// 编译 OUT 语句（用于生成器表达式）
static void compile_out_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_OUT_STATEMENT) {
        ZrCompilerError(cs, "Expected out statement", node->location);
        return;
    }
    
    SZrOutStatement *stmt = &node->data.outStatement;
    
    // 编译表达式
    if (stmt->expr != ZR_NULL) {
        compile_expression(cs, stmt->expr);
        // 结果留在栈上，用于生成器
        // TODO: 实现完整的生成器机制（yield/out）
    } else {
        ZrCompilerError(cs, "Out statement requires an expression", node->location);
    }
}

// 编译 throw 语句
static void compile_throw_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_THROW_STATEMENT) {
        ZrCompilerError(cs, "Expected throw statement", node->location);
        return;
    }
    
    SZrThrowStatement *stmt = &node->data.throwStatement;
    
    // 编译异常表达式
    if (stmt->expr != ZR_NULL) {
        compile_expression(cs, stmt->expr);
        TUInt32 exceptionSlot = cs->stackSlotCount - 1;
        
        // 生成 THROW 指令
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(THROW), (TUInt16)exceptionSlot, 0);
        emit_instruction(cs, inst);
    }
}

// 编译 try-catch-finally 语句
static void compile_try_catch_finally_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        ZrCompilerError(cs, "Expected try-catch-finally statement", node->location);
        return;
    }
    
    SZrTryCatchFinallyStatement *stmt = &node->data.tryCatchFinallyStatement;
    
    // 生成 TRY 指令
    TZrInstruction tryInst = create_instruction_0(ZR_INSTRUCTION_ENUM(TRY), 0);
    emit_instruction(cs, tryInst);
    
    // 编译 try 块
    if (stmt->block != ZR_NULL) {
        compile_statement(cs, stmt->block);
    }
    
    // 创建 catch 标签
    TZrSize catchLabelId = create_label(cs);
    resolve_label(cs, catchLabelId);
    
    // 编译 catch 块
    if (stmt->catchBlock != ZR_NULL) {
        // 生成 CATCH 指令
        TZrInstruction catchInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CATCH), 0);
        emit_instruction(cs, catchInst);
        
        // 处理 catch 参数
        if (stmt->catchPattern != ZR_NULL && stmt->catchPattern->count > 0) {
            // TODO: 将异常值绑定到 catch 参数
        }
        
        compile_statement(cs, stmt->catchBlock);
    }
    
    // 编译 finally 块
    if (stmt->finallyBlock != ZR_NULL) {
        compile_statement(cs, stmt->finallyBlock);
    }
    
    // TODO: 实现完整的异常处理机制
}

// 主编译语句函数
void compile_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    switch (node->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            compile_variable_declaration(cs, node);
            break;
        
        case ZR_AST_EXPRESSION_STATEMENT:
            compile_expression_statement(cs, node);
            break;
        
        case ZR_AST_RETURN_STATEMENT:
            compile_return_statement(cs, node);
            break;
        
        case ZR_AST_BLOCK:
            compile_block_statement(cs, node);
            break;
        
        case ZR_AST_IF_EXPRESSION:
            // 检查是否是语句（isStatement 标志）
            if (node->data.ifExpression.isStatement) {
                compile_if_statement(cs, node);
            } else {
                // 作为表达式处理
                compile_expression(cs, node);
            }
            break;
        
        case ZR_AST_WHILE_LOOP:
            // 检查是否是语句
            if (node->data.whileLoop.isStatement) {
                compile_while_statement(cs, node);
            } else {
                // 作为表达式处理（暂不支持）
                ZrCompilerError(cs, "While loop as expression is not supported", node->location);
            }
            break;
        
        case ZR_AST_FOR_LOOP:
            compile_for_statement(cs, node);
            break;
        
        case ZR_AST_FOREACH_LOOP:
            compile_foreach_statement(cs, node);
            break;
        
        case ZR_AST_SWITCH_EXPRESSION:
            // 检查是否是语句
            if (node->data.switchExpression.isStatement) {
                compile_switch_statement(cs, node);
            } else {
                // 作为表达式处理
                compile_expression(cs, node);
            }
            break;
        
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            compile_break_continue_statement(cs, node);
            break;
        
        case ZR_AST_THROW_STATEMENT:
            compile_throw_statement(cs, node);
            break;
        
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            compile_try_catch_finally_statement(cs, node);
            break;
        
        case ZR_AST_OUT_STATEMENT:
            compile_out_statement(cs, node);
            break;
        
        default:
            // 尝试作为表达式编译
            compile_expression(cs, node);
            break;
    }
}

// 编译解构对象赋值：var {key1, key2, ...} = value;
static void compile_destructuring_object(SZrCompilerState *cs, SZrAstNode *pattern, SZrAstNode *value) {
    if (cs == ZR_NULL || pattern == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (pattern->type != ZR_AST_DESTRUCTURING_OBJECT) {
        ZrCompilerError(cs, "Expected destructuring object pattern", pattern->location);
        return;
    }
    
    // 1. 编译右侧表达式（例如 import("math")）
    if (value == ZR_NULL) {
        ZrCompilerError(cs, "Destructuring assignment requires a value", pattern->location);
        return;
    }
    
    compile_expression(cs, value);
    TUInt32 sourceSlot = cs->stackSlotCount - 1;  // 源对象在栈顶
    
    // 2. 遍历所有键，为每个键分配局部变量并获取值
    SZrDestructuringObject *destruct = &pattern->data.destructuringObject;
    if (destruct->keys != ZR_NULL) {
        for (TZrSize i = 0; i < destruct->keys->count; i++) {
            SZrAstNode *keyNode = destruct->keys->nodes[i];
            if (keyNode == ZR_NULL || keyNode->type != ZR_AST_IDENTIFIER_LITERAL) {
                continue;  // 跳过无效的键
            }
            
            SZrString *keyName = keyNode->data.identifier.name;
            if (keyName == ZR_NULL) {
                continue;
            }
            
            // 分配局部变量槽位
            TUInt32 varIndex = allocate_local_var(cs, keyName);
            
            // 创建键名字符串常量
            SZrTypeValue keyValue;
            ZrValueInitAsRawObject(cs->state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyName));
            keyValue.type = ZR_VALUE_TYPE_STRING;
            TUInt32 keyConstantIndex = add_constant(cs, &keyValue);
            
            // 将键名压栈
            TUInt32 keySlot = allocate_stack_slot(cs);
            TZrInstruction getKeyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)keySlot, (TInt32)keyConstantIndex);
            emit_instruction(cs, getKeyInst);
            
            // 使用 GETTABLE 从对象中获取值
            TUInt32 valueSlot = allocate_stack_slot(cs);
            TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TUInt16)valueSlot, (TUInt16)sourceSlot, (TUInt16)keySlot);
            emit_instruction(cs, getTableInst);
            
            // 将值存储到局部变量
            TZrInstruction setStackInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)varIndex, (TInt32)valueSlot);
            emit_instruction(cs, setStackInst);
            
            // 释放临时栈槽（keySlot 和 valueSlot）
            cs->stackSlotCount -= 2;
        }
    }
    
    // 3. 释放源对象栈槽
    cs->stackSlotCount--;
}

// 编译解构数组赋值：var [elem1, elem2, ...] = value;
static void compile_destructuring_array(SZrCompilerState *cs, SZrAstNode *pattern, SZrAstNode *value) {
    if (cs == ZR_NULL || pattern == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (pattern->type != ZR_AST_DESTRUCTURING_ARRAY) {
        ZrCompilerError(cs, "Expected destructuring array pattern", pattern->location);
        return;
    }
    
    // 1. 编译右侧表达式（例如 arr3）
    if (value == ZR_NULL) {
        ZrCompilerError(cs, "Destructuring assignment requires a value", pattern->location);
        return;
    }
    
    compile_expression(cs, value);
    TUInt32 sourceSlot = cs->stackSlotCount - 1;  // 源数组在栈顶
    
    // 2. 遍历所有索引，为每个元素分配局部变量并获取值
    SZrDestructuringArray *destruct = &pattern->data.destructuringArray;
    if (destruct->keys != ZR_NULL) {
        for (TZrSize i = 0; i < destruct->keys->count; i++) {
            SZrAstNode *elemNode = destruct->keys->nodes[i];
            if (elemNode == ZR_NULL || elemNode->type != ZR_AST_IDENTIFIER_LITERAL) {
                continue;  // 跳过无效的元素
            }
            
            SZrString *elemName = elemNode->data.identifier.name;
            if (elemName == ZR_NULL) {
                continue;
            }
            
            // 分配局部变量槽位
            TUInt32 varIndex = allocate_local_var(cs, elemName);
            
            // 创建索引常量（整数 i）
            SZrTypeValue indexValue;
            ZrValueInitAsInt(cs->state, &indexValue, (TInt64)i);
            TUInt32 indexConstantIndex = add_constant(cs, &indexValue);
            
            // 将索引压栈
            TUInt32 indexSlot = allocate_stack_slot(cs);
            TZrInstruction getIndexInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)indexSlot, (TInt32)indexConstantIndex);
            emit_instruction(cs, getIndexInst);
            
            // 使用 GETTABLE 从数组中获取值
            TUInt32 valueSlot = allocate_stack_slot(cs);
            TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TUInt16)valueSlot, (TUInt16)sourceSlot, (TUInt16)indexSlot);
            emit_instruction(cs, getTableInst);
            
            // 将值存储到局部变量
            TZrInstruction setStackInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)varIndex, (TInt32)valueSlot);
            emit_instruction(cs, setStackInst);
            
            // 释放临时栈槽（indexSlot 和 valueSlot）
            cs->stackSlotCount -= 2;
        }
    }
    
    // 3. 释放源数组栈槽
    cs->stackSlotCount--;
}

