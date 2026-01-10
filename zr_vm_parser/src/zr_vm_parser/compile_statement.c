//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_instruction_conf.h"

#include <string.h>

// 前向声明
extern void compile_expression(SZrCompilerState *cs, SZrAstNode *node);
void compile_statement(SZrCompilerState *cs, SZrAstNode *node);
extern void compile_function_declaration(SZrCompilerState *cs, SZrAstNode *node);
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
        
        // 注册变量类型到类型环境
        if (cs->typeEnv != ZR_NULL) {
            SZrInferredType varType;
            if (decl->typeInfo != ZR_NULL) {
                // 从类型注解推断类型
                if (convert_ast_type_to_inferred_type(cs, decl->typeInfo, &varType)) {
                    ZrTypeEnvironmentRegisterVariable(cs->state, cs->typeEnv, varName, &varType);
                    ZrInferredTypeFree(cs->state, &varType);
                }
            } else if (decl->value != ZR_NULL) {
                // 从初始值推断类型
                if (infer_expression_type(cs, decl->value, &varType)) {
                    ZrTypeEnvironmentRegisterVariable(cs->state, cs->typeEnv, varName, &varType);
                    ZrInferredTypeFree(cs->state, &varType);
                }
            } else {
                // 没有类型注解和初始值，注册为对象类型
                ZrInferredTypeInit(cs->state, &varType, ZR_VALUE_TYPE_OBJECT);
                ZrTypeEnvironmentRegisterVariable(cs->state, cs->typeEnv, varName, &varType);
                ZrInferredTypeFree(cs->state, &varType);
            }
        }
        
        // 分配局部变量槽位
        TUInt32 varIndex = allocate_local_var(cs, varName);
        
        // 如果是脚本级变量且可见性为 pub 或 pro，记录到导出列表
        if (cs->isScriptLevel && decl->accessModifier != ZR_ACCESS_PRIVATE) {
            SZrExportedVariable exportedVar;
            exportedVar.name = varName;
            exportedVar.stackSlot = varIndex;
            exportedVar.accessModifier = decl->accessModifier;
            
            // pub 变量同时添加到 pub 和 pro 列表
            if (decl->accessModifier == ZR_ACCESS_PUBLIC) {
                ZrArrayPush(cs->state, &cs->pubVariables, &exportedVar);
                ZrArrayPush(cs->state, &cs->proVariables, &exportedVar);
            } else if (decl->accessModifier == ZR_ACCESS_PROTECTED) {
                // pro 变量只添加到 pro 列表（pro 列表已经包含所有 pub）
                ZrArrayPush(cs->state, &cs->proVariables, &exportedVar);
            }
        }
        
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
    
    // 设置尾调用上下文标志（在编译返回值表达式时使用）
    TBool oldTailCallContext = cs->isInTailCallContext;
    cs->isInTailCallContext = ZR_TRUE;
    
    if (stmt->expr != ZR_NULL) {
        // 编译返回值表达式（可能在尾调用上下文中使用FUNCTION_TAIL_CALL）
        compile_expression(cs, stmt->expr);
        resultSlot = cs->stackSlotCount - 1;
        resultCount = 1;
    } else {
        // 没有返回值，返回 null
        resultSlot = allocate_stack_slot(cs);
        SZrTypeValue nullValue;
        ZrValueResetAsNull(&nullValue);
        TUInt32 constantIndex = add_constant(cs, &nullValue);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)resultSlot, (TInt32)constantIndex);
        emit_instruction(cs, inst);
        resultCount = 1;
    }
    
    // 恢复尾调用上下文标志
    cs->isInTailCallContext = oldTailCallContext;
    
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
    // 注意：标签位置应该在编译条件表达式之前，但不要立即解析
    // 因为此时还没有指向它的跳转指令
    TZrSize loopStartLabelId = loopLabel.continueLabelId;
    
    // 记录循环开始位置（用于后续解析标签）
    TZrSize loopStartInstructionIndex = cs->instructionCount;
    
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
    
    // 现在解析循环开始标签（此时已经有了指向它的跳转指令）
    // 标签位置应该是条件表达式编译前的第一条指令位置
    SZrLabel *loopStartLabel = (SZrLabel *)ZrArrayGet(&cs->labels, loopStartLabelId);
    if (loopStartLabel != ZR_NULL) {
        loopStartLabel->instructionIndex = loopStartInstructionIndex;
        loopStartLabel->isResolved = ZR_TRUE;
        // 填充所有指向该标签的跳转指令的偏移量
        for (TZrSize i = 0; i < cs->pendingJumps.length; i++) {
            SZrPendingJump *pendingJump = (SZrPendingJump *) ZrArrayGet(&cs->pendingJumps, i);
            if (pendingJump != ZR_NULL && pendingJump->labelId == loopStartLabelId &&
                pendingJump->instructionIndex < cs->instructions.length) {
                TZrInstruction *jumpInst =
                        (TZrInstruction *) ZrArrayGet(&cs->instructions, pendingJump->instructionIndex);
                if (jumpInst != ZR_NULL) {
                    // 计算相对偏移：目标指令索引 - (当前指令索引 + 1)
                    TInt32 offset = (TInt32) loopStartInstructionIndex - (TInt32) pendingJump->instructionIndex - 1;
                    jumpInst->instruction.operand.operand2[0] = offset;
                }
            }
        }
    }
    
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
    
    SZrForeachLoop *foreachLoop = &node->data.foreachLoop;
    
    // 进入新作用域（用于pattern变量）
    enter_scope(cs);
    
    // 创建循环标签（用于 break/continue）
    SZrLoopLabel loopLabel;
    loopLabel.breakLabelId = create_label(cs);
    loopLabel.continueLabelId = create_label(cs);
    ZrArrayPush(cs->state, &cs->loopLabelStack, &loopLabel);
    
    // 编译迭代表达式
    compile_expression(cs, foreachLoop->expr);
    TUInt32 iterableSlot = cs->stackSlotCount - 1;
    
    // 分配迭代器槽位和当前值槽位
    TUInt32 iteratorSlot = allocate_stack_slot(cs);
    TUInt32 hasNextSlot = allocate_stack_slot(cs);
    
    // 创建循环开始标签（continue 跳转到这里）
    TZrSize loopStartLabelId = loopLabel.continueLabelId;
    resolve_label(cs, loopStartLabelId);
    
    // 获取迭代器（简化实现：假设对象有 iterator 方法）
    // 创建 "iterator" 字符串常量
    SZrString *iteratorName = ZrStringCreate(cs->state, "iterator", 8);
    if (iteratorName == ZR_NULL) {
        ZrCompilerError(cs, "Failed to create iterator name string", node->location);
        return;
    }
    SZrTypeValue iteratorNameValue;
    ZrValueInitAsRawObject(cs->state, &iteratorNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(iteratorName));
    iteratorNameValue.type = ZR_VALUE_TYPE_STRING;
    TUInt32 iteratorNameConstantIndex = add_constant(cs, &iteratorNameValue);
    
    // 将 "iterator" 字符串压栈
    TUInt32 iteratorNameSlot = allocate_stack_slot(cs);
    TZrInstruction getIteratorNameInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), iteratorNameSlot, (TInt32)iteratorNameConstantIndex);
    emit_instruction(cs, getIteratorNameInst);
    
    // 调用 iterator 方法获取迭代器
    TUInt32 iteratorResultSlot = allocate_stack_slot(cs);
    TZrInstruction callIteratorInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), iteratorResultSlot, (TUInt16)iterableSlot, 1);
    emit_instruction(cs, callIteratorInst);
    
    // 将迭代器保存到 iteratorSlot
    TZrInstruction moveIteratorInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), iteratorSlot, (TInt32)iteratorResultSlot);
    emit_instruction(cs, moveIteratorInst);
    
    // 释放临时栈槽（iteratorNameSlot, iteratorResultSlot）
    cs->stackSlotCount -= 2;
    
    // 创建 "hasNext" 字符串常量
    SZrString *hasNextName = ZrStringCreate(cs->state, "hasNext", 7);
    if (hasNextName == ZR_NULL) {
        ZrCompilerError(cs, "Failed to create hasNext name string", node->location);
        return;
    }
    SZrTypeValue hasNextNameValue;
    ZrValueInitAsRawObject(cs->state, &hasNextNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(hasNextName));
    hasNextNameValue.type = ZR_VALUE_TYPE_STRING;
    TUInt32 hasNextNameConstantIndex = add_constant(cs, &hasNextNameValue);
    
    // 检查是否有下一个元素（调用 iterator.hasNext()）
    // 将 "hasNext" 字符串压栈
    TUInt32 hasNextNameSlot = allocate_stack_slot(cs);
    TZrInstruction getHasNextNameInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), hasNextNameSlot, (TInt32)hasNextNameConstantIndex);
    emit_instruction(cs, getHasNextNameInst);
    
    // 调用 hasNext 方法
    TUInt32 hasNextResultSlot = allocate_stack_slot(cs);
    TZrInstruction callHasNextInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), hasNextResultSlot, (TUInt16)iteratorSlot, 1);
    emit_instruction(cs, callHasNextInst);
    
    // 将结果保存到 hasNextSlot
    TZrInstruction moveHasNextInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), hasNextSlot, (TInt32)hasNextResultSlot);
    emit_instruction(cs, moveHasNextInst);
    
    // 释放临时栈槽
    cs->stackSlotCount -= 2;
    
    // 如果 hasNext 为 false，跳转到循环结束
    TZrSize loopEndLabelId = loopLabel.breakLabelId;
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)hasNextSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, loopEndLabelId);
    
    // 获取当前元素（调用 iterator.next()）
    // 创建 "next" 字符串常量
    SZrString *nextName = ZrStringCreate(cs->state, "next", 4);
    if (nextName == ZR_NULL) {
        ZrCompilerError(cs, "Failed to create next name string", node->location);
        return;
    }
    SZrTypeValue nextNameValue;
    ZrValueInitAsRawObject(cs->state, &nextNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nextName));
    nextNameValue.type = ZR_VALUE_TYPE_STRING;
    TUInt32 nextNameConstantIndex = add_constant(cs, &nextNameValue);
    
    // 将 "next" 字符串压栈
    TUInt32 nextNameSlot = allocate_stack_slot(cs);
    TZrInstruction getNextNameInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), nextNameSlot, (TInt32)nextNameConstantIndex);
    emit_instruction(cs, getNextNameInst);
    
    // 调用 next 方法获取当前元素
    TUInt32 currentValueSlot = allocate_stack_slot(cs);
    TZrInstruction callNextInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), currentValueSlot, (TUInt16)iteratorSlot, 1);
    emit_instruction(cs, callNextInst);
    
    // 释放临时栈槽（nextNameSlot）
    cs->stackSlotCount--;
    
    // 处理 pattern（将当前值绑定到变量）
    if (foreachLoop->pattern != ZR_NULL) {
        if (foreachLoop->pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
            // 简单标识符：分配局部变量并赋值
            SZrString *varName = foreachLoop->pattern->data.identifier.name;
            if (varName != ZR_NULL) {
                TUInt32 varIndex = allocate_local_var(cs, varName);
                TZrInstruction setVarInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)varIndex, (TInt32)currentValueSlot);
                emit_instruction(cs, setVarInst);
            }
        } else if (foreachLoop->pattern->type == ZR_AST_DESTRUCTURING_OBJECT) {
            // 解构对象模式：调用解构函数处理
            // currentValueSlot 包含当前迭代的值（对象）
            compile_destructuring_object(cs, foreachLoop->pattern, ZR_NULL);
            // 注意：compile_destructuring_object 需要从栈上读取值
            // 这里 currentValueSlot 已经在栈上，所以可以直接使用
        } else if (foreachLoop->pattern->type == ZR_AST_DESTRUCTURING_ARRAY) {
            // 解构数组模式：调用解构函数处理
            // currentValueSlot 包含当前迭代的值（数组）
            compile_destructuring_array(cs, foreachLoop->pattern, ZR_NULL);
            // 注意：compile_destructuring_array 需要从栈上读取值
            // 这里 currentValueSlot 已经在栈上，所以可以直接使用
        }
    }
    
    // 释放 currentValueSlot
    cs->stackSlotCount--;
    
    // 编译循环体
    if (foreachLoop->block != ZR_NULL) {
        compile_statement(cs, foreachLoop->block);
    }
    
    // 跳转到循环开始
    TZrInstruction jumpLoopInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpLoopIndex = cs->instructionCount;
    emit_instruction(cs, jumpLoopInst);
    add_pending_jump(cs, jumpLoopIndex, loopStartLabelId);
    
    // 解析循环结束标签
    resolve_label(cs, loopEndLabelId);
    
    // 释放临时栈槽（iterableSlot, iteratorSlot, hasNextSlot）
    cs->stackSlotCount -= 3;
    
    // 弹出循环标签栈
    ZrArrayPop(&cs->loopLabelStack);
    
    // 退出作用域
    exit_scope(cs);
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
        TUInt32 valueSlot = cs->stackSlotCount - 1;
        
        // 生成生成器输出指令（用于生成器）
        // 注意：生成器机制需要运行时支持，这里先实现基本框架
        // 生成器函数会在运行时通过特殊的调用约定来处理 yield/out
        // 目前先使用 RETURN 指令返回值，后续可以扩展为专门的生成器指令
        // 生成器函数应该标记为可暂停的函数类型
        TZrInstruction returnInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16)valueSlot, 0);
        emit_instruction(cs, returnInst);
        
        // 释放值栈槽（YIELD 会处理值的传递）
        cs->stackSlotCount--;
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
    
    // 进入新作用域（用于catch参数）
    if (stmt->catchBlock != ZR_NULL && stmt->catchPattern != ZR_NULL && stmt->catchPattern->count > 0) {
        enter_scope(cs);
    }
    
    // 创建跳过catch的标签（如果try块正常执行完成）
    TZrSize tryEndLabelId = create_label(cs);
    
    // 创建finally标签（用于统一finally处理）
    TZrSize finallyLabelId = create_label(cs);
    TZrSize afterFinallyLabelId = create_label(cs);
    
    // 生成 TRY 指令（标记异常处理开始）
    TZrInstruction tryInst = create_instruction_0(ZR_INSTRUCTION_ENUM(TRY), 0);
    emit_instruction(cs, tryInst);
    
    // 编译 try 块
    if (stmt->block != ZR_NULL) {
        compile_statement(cs, stmt->block);
    }
    
    // try块正常完成，跳转到try结束标签
    TZrInstruction jumpTryEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpTryEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpTryEndInst);
    add_pending_jump(cs, jumpTryEndIndex, tryEndLabelId);
    
    // 创建 catch 标签
    TZrSize catchLabelId = create_label(cs);
    resolve_label(cs, catchLabelId);
    
    // 生成 CATCH 指令（捕获异常）
    // 异常值会被压到栈上
    if (stmt->catchBlock != ZR_NULL) {
        TZrInstruction catchInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CATCH), 0);
        emit_instruction(cs, catchInst);
        
        // 异常值现在在栈顶
        TUInt32 exceptionSlot = cs->stackSlotCount - 1;
        
        // 处理 catch 参数（将异常值绑定到变量）
        if (stmt->catchPattern != ZR_NULL && stmt->catchPattern->count > 0) {
            // 目前只支持单个catch参数
            if (stmt->catchPattern->count == 1) {
                SZrAstNode *paramNode = stmt->catchPattern->nodes[0];
                if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                    SZrParameter *param = &paramNode->data.parameter;
                    if (param->name != ZR_NULL) {
                        SZrString *paramName = param->name->name;
                        if (paramName != ZR_NULL) {
                            // 分配局部变量槽位
                            TUInt32 varIndex = allocate_local_var(cs, paramName);
                            
                            // 将异常值存储到局部变量
                            TZrInstruction setVarInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)varIndex, (TInt32)exceptionSlot);
                            emit_instruction(cs, setVarInst);
                            
                            // 释放异常值栈槽
                            cs->stackSlotCount--;
                        }
                    }
                }
            } else {
                // 多个catch参数暂不支持
                ZrCompilerError(cs, "Multiple catch parameters are not supported yet", node->location);
            }
        } else {
            // 没有catch参数，丢弃异常值
            cs->stackSlotCount--;
        }
        
        // 编译 catch 块
        compile_statement(cs, stmt->catchBlock);
    } else {
        // 没有catch块，只生成CATCH指令丢弃异常
        TZrInstruction catchInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CATCH), 0);
        emit_instruction(cs, catchInst);
        // 异常值会被自动丢弃
    }
    
    // 跳转到finally处理（如果有）或结束
    if (stmt->finallyBlock != ZR_NULL) {
        TZrInstruction jumpFinallyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpFinallyIndex = cs->instructionCount;
        emit_instruction(cs, jumpFinallyInst);
        add_pending_jump(cs, jumpFinallyIndex, finallyLabelId);
    } else {
        TZrInstruction jumpAfterInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpAfterIndex = cs->instructionCount;
        emit_instruction(cs, jumpAfterInst);
        add_pending_jump(cs, jumpAfterIndex, afterFinallyLabelId);
    }
    
    // 解析try结束标签
    resolve_label(cs, tryEndLabelId);
    
    // 如果有finally块，跳转到finally处理
    if (stmt->finallyBlock != ZR_NULL) {
        TZrInstruction jumpFinallyInst2 = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpFinallyIndex2 = cs->instructionCount;
        emit_instruction(cs, jumpFinallyInst2);
        add_pending_jump(cs, jumpFinallyIndex2, finallyLabelId);
    }
    
    // 解析finally标签
    if (stmt->finallyBlock != ZR_NULL) {
        resolve_label(cs, finallyLabelId);
        
        // 编译 finally 块
        compile_statement(cs, stmt->finallyBlock);
        
        // 跳转到结束
        TZrInstruction jumpAfterInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpAfterIndex = cs->instructionCount;
        emit_instruction(cs, jumpAfterInst);
        add_pending_jump(cs, jumpAfterIndex, afterFinallyLabelId);
    }
    
    // 解析结束标签
    resolve_label(cs, afterFinallyLabelId);
    
    // 退出作用域（如果进入了catch作用域）
    if (stmt->catchBlock != ZR_NULL && stmt->catchPattern != ZR_NULL && stmt->catchPattern->count > 0) {
        exit_scope(cs);
    }
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
        
        case ZR_AST_FUNCTION_DECLARATION:
            // 函数声明可以作为语句编译（嵌套函数）
            compile_function_declaration(cs, node);
            break;
        
        default:
            // 检查是否是声明类型（不应该作为语句编译）
            if (node->type == ZR_AST_INTERFACE_METHOD_SIGNATURE ||
                node->type == ZR_AST_INTERFACE_FIELD_DECLARATION ||
                node->type == ZR_AST_INTERFACE_PROPERTY_SIGNATURE ||
                node->type == ZR_AST_INTERFACE_META_SIGNATURE ||
                node->type == ZR_AST_STRUCT_FIELD ||
                node->type == ZR_AST_STRUCT_METHOD ||
                node->type == ZR_AST_STRUCT_META_FUNCTION ||
                node->type == ZR_AST_CLASS_FIELD ||
                node->type == ZR_AST_CLASS_METHOD ||
                node->type == ZR_AST_CLASS_PROPERTY ||
                node->type == ZR_AST_CLASS_META_FUNCTION ||
                node->type == ZR_AST_STRUCT_DECLARATION ||
                node->type == ZR_AST_CLASS_DECLARATION ||
                node->type == ZR_AST_INTERFACE_DECLARATION ||
                node->type == ZR_AST_ENUM_DECLARATION ||
                node->type == ZR_AST_ENUM_MEMBER ||
                node->type == ZR_AST_MODULE_DECLARATION ||
                node->type == ZR_AST_SCRIPT) {
                // 这些是声明类型，不应该作为语句编译
                static TChar errorMsg[256];
                const TChar *typeName = "UNKNOWN";
                switch (node->type) {
                    case ZR_AST_INTERFACE_METHOD_SIGNATURE: typeName = "INTERFACE_METHOD_SIGNATURE"; break;
                    case ZR_AST_INTERFACE_FIELD_DECLARATION: typeName = "INTERFACE_FIELD_DECLARATION"; break;
                    case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: typeName = "INTERFACE_PROPERTY_SIGNATURE"; break;
                    case ZR_AST_INTERFACE_META_SIGNATURE: typeName = "INTERFACE_META_SIGNATURE"; break;
                    case ZR_AST_STRUCT_FIELD: typeName = "STRUCT_FIELD"; break;
                    case ZR_AST_STRUCT_METHOD: typeName = "STRUCT_METHOD"; break;
                    case ZR_AST_STRUCT_META_FUNCTION: typeName = "STRUCT_META_FUNCTION"; break;
                    case ZR_AST_CLASS_FIELD: typeName = "CLASS_FIELD"; break;
                    case ZR_AST_CLASS_METHOD: typeName = "CLASS_METHOD"; break;
                    case ZR_AST_CLASS_PROPERTY: typeName = "CLASS_PROPERTY"; break;
                    case ZR_AST_CLASS_META_FUNCTION: typeName = "CLASS_META_FUNCTION"; break;
                    case ZR_AST_STRUCT_DECLARATION: typeName = "STRUCT_DECLARATION"; break;
                    case ZR_AST_CLASS_DECLARATION: typeName = "CLASS_DECLARATION"; break;
                    case ZR_AST_INTERFACE_DECLARATION: typeName = "INTERFACE_DECLARATION"; break;
                    case ZR_AST_ENUM_DECLARATION: typeName = "ENUM_DECLARATION"; break;
                    case ZR_AST_ENUM_MEMBER: typeName = "ENUM_MEMBER"; break;
                    case ZR_AST_MODULE_DECLARATION: typeName = "MODULE_DECLARATION"; break;
                    case ZR_AST_SCRIPT: typeName = "SCRIPT"; break;
                    default: break;
                }
                snprintf(errorMsg, sizeof(errorMsg), 
                        "Declaration type '%s' (type %d) cannot be used as a statement at line %d:%d", 
                        typeName, node->type, 
                        node->location.start.line, node->location.start.column);
                ZrCompilerError(cs, errorMsg, node->location);
                return;
            }
            
            // 其他类型尝试作为表达式编译
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
    // 如果 value 为 NULL，表示值已经在栈上（例如在 foreach 中）
    TUInt32 sourceSlot;
    if (value != ZR_NULL) {
        compile_expression(cs, value);
        sourceSlot = cs->stackSlotCount - 1;  // 源对象在栈顶
    } else {
        // 值已经在栈上，使用栈顶的值
        if (cs->stackSlotCount == 0) {
            ZrCompilerError(cs, "Destructuring assignment requires a value on stack", pattern->location);
            return;
        }
        sourceSlot = cs->stackSlotCount - 1;  // 使用栈顶的值
    }
    
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
    
    // 3. 释放源对象栈槽（只有在 value 不为 NULL 时才释放，因为如果是 NULL，值已经在栈上且需要保留）
    if (value != ZR_NULL) {
        cs->stackSlotCount--;
    }
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
    // 如果 value 为 NULL，表示值已经在栈上（例如在 foreach 中）
    TUInt32 sourceSlot;
    if (value != ZR_NULL) {
        compile_expression(cs, value);
        sourceSlot = cs->stackSlotCount - 1;  // 源数组在栈顶
    } else {
        // 值已经在栈上，使用栈顶的值
        if (cs->stackSlotCount == 0) {
            ZrCompilerError(cs, "Destructuring assignment requires a value on stack", pattern->location);
            return;
        }
        sourceSlot = cs->stackSlotCount - 1;  // 使用栈顶的值
    }
    
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
    
    // 3. 释放源数组栈槽（只有在 value 不为 NULL 时才释放，因为如果是 NULL，值已经在栈上且需要保留）
    if (value != ZR_NULL) {
        cs->stackSlotCount--;
    }
}

