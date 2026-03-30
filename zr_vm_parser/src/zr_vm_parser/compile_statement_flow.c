//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "compiler_internal.h"
#include "compile_statement_internal.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

static SZrAstNodeArray *catch_clause_pattern(SZrAstNode *catchClauseNode) {
    if (catchClauseNode == ZR_NULL || catchClauseNode->type != ZR_AST_CATCH_CLAUSE) {
        return ZR_NULL;
    }
    return catchClauseNode->data.catchClause.pattern;
}

static SZrAstNode *catch_clause_block(SZrAstNode *catchClauseNode) {
    if (catchClauseNode == ZR_NULL || catchClauseNode->type != ZR_AST_CATCH_CLAUSE) {
        return ZR_NULL;
    }
    return catchClauseNode->data.catchClause.block;
}

TZrBool try_context_find_innermost_finally(const SZrCompilerState *cs, SZrCompilerTryContext *outContext) {
    if (cs == ZR_NULL || outContext == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = cs->tryContextStack.length; index > 0; index--) {
        SZrCompilerTryContext *context =
                (SZrCompilerTryContext *)ZrCore_Array_Get((SZrArray *)&cs->tryContextStack, index - 1);
        if (context != ZR_NULL && context->finallyLabelId != (TZrSize)-1) {
            *outContext = *context;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrString *catch_clause_type_name(SZrAstNode *catchClauseNode) {
    SZrAstNodeArray *pattern = catch_clause_pattern(catchClauseNode);

    if (pattern == ZR_NULL || pattern->count == 0) {
        return ZR_NULL;
    }

    if (pattern->count != 1) {
        return ZR_NULL;
    }

    {
        SZrAstNode *paramNode = pattern->nodes[0];
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            return ZR_NULL;
        }

        if (paramNode->data.parameter.typeInfo == ZR_NULL || paramNode->data.parameter.typeInfo->name == ZR_NULL) {
            return ZR_NULL;
        }

        if (paramNode->data.parameter.typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL) {
            return paramNode->data.parameter.typeInfo->name->data.identifier.name;
        }
    }

    return ZR_NULL;
}

TZrUInt32 bind_existing_stack_slot_as_local_var(SZrCompilerState *cs,
                                                SZrString *name,
                                                TZrUInt32 stackSlot) {
    SZrFunctionLocalVariable localVar;

    if (cs == ZR_NULL || cs->hasError || name == ZR_NULL) {
        return 0;
    }

    if (cs->stackSlotCount == 0 || stackSlot >= cs->stackSlotCount) {
        return 0;
    }

    localVar.name = name;
    localVar.stackSlot = stackSlot;
    localVar.offsetActivate = (TZrMemoryOffset)cs->instructionCount;
    localVar.offsetDead = 0;

    ZrCore_Array_Push(cs->state, &cs->localVars, &localVar);
    cs->localVarCount = cs->localVars.length;

    if (cs->scopeStack.length > 0) {
        SZrScope *scope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
        if (scope != ZR_NULL) {
            scope->varCount++;
        }
    }

    return stackSlot;
}

static TZrUInt32 catch_clause_binding_slot(SZrCompilerState *cs, SZrAstNode *catchClauseNode) {
    SZrAstNodeArray *pattern = catch_clause_pattern(catchClauseNode);

    if (pattern == ZR_NULL || pattern->count == 0) {
        return allocate_stack_slot(cs);
    }

    if (pattern->count != 1) {
        ZrParser_Compiler_Error(cs, "Multiple catch parameters are not supported", catchClauseNode->location);
        return allocate_stack_slot(cs);
    }

    {
        SZrAstNode *paramNode = pattern->nodes[0];
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER ||
            paramNode->data.parameter.name == ZR_NULL || paramNode->data.parameter.name->name == ZR_NULL) {
            return allocate_stack_slot(cs);
        }
        return allocate_local_var(cs, paramNode->data.parameter.name->name);
    }
}

void emit_jump_to_label(SZrCompilerState *cs, TZrSize labelId) {
    TZrInstruction jumpInst;
    TZrSize jumpIndex;

    jumpInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    jumpIndex = cs->instructionCount;
    emit_instruction(cs, jumpInst);
    add_pending_jump(cs, jumpIndex, labelId);
}


void compile_while_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_WHILE_LOOP) {
        ZrParser_Compiler_Error(cs, "Expected while statement", node->location);
        return;
    }
    
    SZrWhileLoop *whileLoop = &node->data.whileLoop;
    
    // 创建循环标签（用于 break/continue）
    SZrLoopLabel loopLabel;
    loopLabel.breakLabelId = create_label(cs);
    loopLabel.continueLabelId = create_label(cs);
    ZrCore_Array_Push(cs->state, &cs->loopLabelStack, &loopLabel);
    
    // 创建循环开始标签（continue 跳转到这里）
    // 注意：标签位置应该在编译条件表达式之前，但不要立即解析
    // 因为此时还没有指向它的跳转指令
    TZrSize loopStartLabelId = loopLabel.continueLabelId;
    
    // 记录循环开始位置（用于后续解析标签）
    TZrSize loopStartInstructionIndex = cs->instructionCount;
    
    // 编译条件表达式
    ZrParser_Expression_Compile(cs, whileLoop->cond);
    TZrUInt32 condSlot = cs->stackSlotCount - 1;
    
    // 创建循环结束标签（break 跳转到这里）
    TZrSize loopEndLabelId = loopLabel.breakLabelId;
    
    // JUMP_IF false -> end
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)condSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, loopEndLabelId);
    
    // 编译循环体
    if (whileLoop->block != ZR_NULL) {
        ZrParser_Statement_Compile(cs, whileLoop->block);
    }
    
    // JUMP -> loop start
    TZrInstruction jumpLoopInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpLoopIndex = cs->instructionCount;
    emit_instruction(cs, jumpLoopInst);
    add_pending_jump(cs, jumpLoopIndex, loopStartLabelId);
    
    // 现在解析循环开始标签（此时已经有了指向它的跳转指令）
    // 标签位置应该是条件表达式编译前的第一条指令位置
    SZrLabel *loopStartLabel = (SZrLabel *)ZrCore_Array_Get(&cs->labels, loopStartLabelId);
    if (loopStartLabel != ZR_NULL) {
        loopStartLabel->instructionIndex = loopStartInstructionIndex;
        loopStartLabel->isResolved = ZR_TRUE;
        // 填充所有指向该标签的跳转指令的偏移量
        for (TZrSize i = 0; i < cs->pendingJumps.length; i++) {
            SZrPendingJump *pendingJump = (SZrPendingJump *) ZrCore_Array_Get(&cs->pendingJumps, i);
            if (pendingJump != ZR_NULL && pendingJump->labelId == loopStartLabelId &&
                pendingJump->instructionIndex < cs->instructions.length) {
                TZrInstruction *jumpInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, pendingJump->instructionIndex);
                if (jumpInst != ZR_NULL) {
                    // 计算相对偏移：目标指令索引 - (当前指令索引 + 1)
                    TZrInt32 offset = (TZrInt32) loopStartInstructionIndex - (TZrInt32) pendingJump->instructionIndex - 1;
                    jumpInst->instruction.operand.operand2[0] = offset;
                }
            }
        }
    }
    
    // 解析循环结束标签
    resolve_label(cs, loopEndLabelId);
    
    // 弹出循环标签栈
    ZrCore_Array_Pop(&cs->loopLabelStack);
}

// 编译 for 语句
void compile_for_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_FOR_LOOP) {
        ZrParser_Compiler_Error(cs, "Expected for statement", node->location);
        return;
    }
    
    SZrForLoop *forLoop = &node->data.forLoop;
    
    // 进入新作用域
    enter_scope(cs);
    
    // 创建循环标签（用于 break/continue）
    SZrLoopLabel loopLabel;
    loopLabel.breakLabelId = create_label(cs);
    loopLabel.continueLabelId = create_label(cs);
    ZrCore_Array_Push(cs->state, &cs->loopLabelStack, &loopLabel);
    
    // 编译初始化表达式
    if (forLoop->init != ZR_NULL) {
        ZrParser_Statement_Compile(cs, forLoop->init);
    }
    
    // 创建循环开始标签（continue 跳转到这里，在 step 之后）
    TZrSize loopStartLabelId = loopLabel.continueLabelId;
    resolve_label(cs, loopStartLabelId);
    
        // 编译条件表达式
        if (forLoop->cond != ZR_NULL) {
            ZrParser_Expression_Compile(cs, forLoop->cond);
            TZrUInt32 condSlot = cs->stackSlotCount - 1;
            
            // 创建循环结束标签（break 跳转到这里）
            TZrSize loopEndLabelId = loopLabel.breakLabelId;
        
        // JUMP_IF false -> end
        TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)condSlot, 0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, loopEndLabelId);
        
        // 编译循环体
        if (forLoop->block != ZR_NULL) {
            ZrParser_Statement_Compile(cs, forLoop->block);
        }
        
        // 编译增量表达式
        if (forLoop->step != ZR_NULL) {
            ZrParser_Expression_Compile(cs, forLoop->step);
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
            ZrParser_Statement_Compile(cs, forLoop->block);
        }
        
        // 编译增量表达式
        if (forLoop->step != ZR_NULL) {
            ZrParser_Expression_Compile(cs, forLoop->step);
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
    ZrCore_Array_Pop(&cs->loopLabelStack);
    
    // 退出作用域
    exit_scope(cs);
}

// 编译 foreach 语句
void compile_foreach_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_FOREACH_LOOP) {
        ZrParser_Compiler_Error(cs, "Expected foreach statement", node->location);
        return;
    }
    
    SZrForeachLoop *foreachLoop = &node->data.foreachLoop;
    
    // 进入新作用域（用于pattern变量）
    enter_scope(cs);
    
    // 创建循环标签（用于 break/continue）
    SZrLoopLabel loopLabel;
    loopLabel.breakLabelId = create_label(cs);
    loopLabel.continueLabelId = create_label(cs);
    ZrCore_Array_Push(cs->state, &cs->loopLabelStack, &loopLabel);
    
    // 编译迭代表达式
    ZrParser_Expression_Compile(cs, foreachLoop->expr);
    TZrUInt32 iterableSlot = cs->stackSlotCount - 1;
    
    // 分配迭代器槽位和当前值槽位
    TZrUInt32 iteratorSlot = allocate_stack_slot(cs);
    TZrUInt32 hasNextSlot = allocate_stack_slot(cs);
    
    // 创建循环开始标签（continue 跳转到这里）
    TZrSize loopStartLabelId = loopLabel.continueLabelId;
    resolve_label(cs, loopStartLabelId);
    
    // TODO: 获取迭代器（简化实现：假设对象有 iterator 方法）
    // 创建 "iterator" 字符串常量
    SZrString *iteratorName = ZrCore_String_Create(cs->state, "iterator", 8);
    if (iteratorName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create iterator name string", node->location);
        return;
    }
    SZrTypeValue iteratorNameValue;
    ZrCore_Value_InitAsRawObject(cs->state, &iteratorNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(iteratorName));
    iteratorNameValue.type = ZR_VALUE_TYPE_STRING;
    TZrUInt32 iteratorNameConstantIndex = add_constant(cs, &iteratorNameValue);
    
    // 将 "iterator" 字符串压栈
    TZrUInt32 iteratorNameSlot = allocate_stack_slot(cs);
    TZrInstruction getIteratorNameInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), iteratorNameSlot, (TZrInt32)iteratorNameConstantIndex);
    emit_instruction(cs, getIteratorNameInst);
    
    // 调用 iterator 方法获取迭代器
    TZrUInt32 iteratorResultSlot = allocate_stack_slot(cs);
    TZrInstruction callIteratorInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), iteratorResultSlot, (TZrUInt16)iterableSlot, 1);
    emit_instruction(cs, callIteratorInst);
    
    // 将迭代器保存到 iteratorSlot
    TZrInstruction moveIteratorInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), iteratorSlot, (TZrInt32)iteratorResultSlot);
    emit_instruction(cs, moveIteratorInst);
    
    // 释放临时栈槽（iteratorNameSlot, iteratorResultSlot）
    ZrParser_Compiler_TrimStackBy(cs, 2);
    
    // 创建 "hasNext" 字符串常量
    SZrString *hasNextName = ZrCore_String_Create(cs->state, "hasNext", 7);
    if (hasNextName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create hasNext name string", node->location);
        return;
    }
    SZrTypeValue hasNextNameValue;
    ZrCore_Value_InitAsRawObject(cs->state, &hasNextNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(hasNextName));
    hasNextNameValue.type = ZR_VALUE_TYPE_STRING;
    TZrUInt32 hasNextNameConstantIndex = add_constant(cs, &hasNextNameValue);
    
    // 检查是否有下一个元素（调用 iterator.hasNext()）
    // 将 "hasNext" 字符串压栈
    TZrUInt32 hasNextNameSlot = allocate_stack_slot(cs);
    TZrInstruction getHasNextNameInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), hasNextNameSlot, (TZrInt32)hasNextNameConstantIndex);
    emit_instruction(cs, getHasNextNameInst);
    
    // 调用 hasNext 方法
    TZrUInt32 hasNextResultSlot = allocate_stack_slot(cs);
    TZrInstruction callHasNextInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), hasNextResultSlot, (TZrUInt16)iteratorSlot, 1);
    emit_instruction(cs, callHasNextInst);
    
    // 将结果保存到 hasNextSlot
    TZrInstruction moveHasNextInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), hasNextSlot, (TZrInt32)hasNextResultSlot);
    emit_instruction(cs, moveHasNextInst);
    
    // 释放临时栈槽
    ZrParser_Compiler_TrimStackBy(cs, 2);
    
    // 如果 hasNext 为 false，跳转到循环结束
    TZrSize loopEndLabelId = loopLabel.breakLabelId;
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)hasNextSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, loopEndLabelId);
    
    // 获取当前元素（调用 iterator.next()）
    // 创建 "next" 字符串常量
    SZrString *nextName = ZrCore_String_Create(cs->state, "next", 4);
    if (nextName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create next name string", node->location);
        return;
    }
    SZrTypeValue nextNameValue;
    ZrCore_Value_InitAsRawObject(cs->state, &nextNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nextName));
    nextNameValue.type = ZR_VALUE_TYPE_STRING;
    TZrUInt32 nextNameConstantIndex = add_constant(cs, &nextNameValue);
    
    // 将 "next" 字符串压栈
    TZrUInt32 nextNameSlot = allocate_stack_slot(cs);
    TZrInstruction getNextNameInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), nextNameSlot, (TZrInt32)nextNameConstantIndex);
    emit_instruction(cs, getNextNameInst);
    
    // 调用 next 方法获取当前元素
    TZrUInt32 currentValueSlot = allocate_stack_slot(cs);
    TZrInstruction callNextInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), currentValueSlot, (TZrUInt16)iteratorSlot, 1);
    emit_instruction(cs, callNextInst);
    
    // 释放临时栈槽（nextNameSlot）
    ZrParser_Compiler_TrimStackBy(cs, 1);
    
    // 处理 pattern（将当前值绑定到变量）
    if (foreachLoop->pattern != ZR_NULL) {
        if (foreachLoop->pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
            // 简单标识符：分配局部变量并赋值
            SZrString *varName = foreachLoop->pattern->data.identifier.name;
            if (varName != ZR_NULL) {
                TZrUInt32 varIndex = allocate_local_var(cs, varName);
                TZrInstruction setVarInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)varIndex, (TZrInt32)currentValueSlot);
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
    ZrParser_Compiler_TrimStackBy(cs, 1);
    
    // 编译循环体
    if (foreachLoop->block != ZR_NULL) {
        ZrParser_Statement_Compile(cs, foreachLoop->block);
    }
    
    // 跳转到循环开始
    TZrInstruction jumpLoopInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpLoopIndex = cs->instructionCount;
    emit_instruction(cs, jumpLoopInst);
    add_pending_jump(cs, jumpLoopIndex, loopStartLabelId);
    
    // 解析循环结束标签
    resolve_label(cs, loopEndLabelId);
    
    // 释放临时栈槽（iterableSlot, iteratorSlot, hasNextSlot）
    ZrParser_Compiler_TrimStackBy(cs, 3);
    
    // 弹出循环标签栈
    ZrCore_Array_Pop(&cs->loopLabelStack);
    
    // 退出作用域
    exit_scope(cs);
}

// 编译 switch 语句
void compile_switch_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    // switch 语句和 switch 表达式使用相同的 AST 节点类型
    if (node->type != ZR_AST_SWITCH_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected switch statement", node->location);
        return;
    }
    
    SZrSwitchExpression *switchExpr = &node->data.switchExpression;
    
    // 编译 switch 表达式
    ZrParser_Expression_Compile(cs, switchExpr->expr);
    TZrUInt32 exprSlot = cs->stackSlotCount - 1;
    
    // 创建结束标签
    TZrSize endLabelId = create_label(cs);
    
    // 编译所有 case
    if (switchExpr->cases != ZR_NULL) {
        for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
            SZrAstNode *caseNode = switchExpr->cases->nodes[i];
            if (caseNode != ZR_NULL && caseNode->type == ZR_AST_SWITCH_CASE) {
                SZrSwitchCase *switchCase = &caseNode->data.switchCase;
                
                // 编译 case 值
                ZrParser_Expression_Compile(cs, switchCase->value);
                TZrUInt32 caseValueSlot = cs->stackSlotCount - 1;
                
                // 比较表达式和 case 值
                TZrUInt32 compareSlot = allocate_stack_slot(cs);
                TZrInstruction compareInst = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL), compareSlot, (TZrUInt16)exprSlot, (TZrUInt16)caseValueSlot);
                emit_instruction(cs, compareInst);
                
                // 创建下一个 case 标签
                TZrSize nextCaseLabelId = create_label(cs);
                
                // JUMP_IF false -> next case
                TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)compareSlot, 0);
                TZrSize jumpIfIndex = cs->instructionCount;
                emit_instruction(cs, jumpIfInst);
                add_pending_jump(cs, jumpIfIndex, nextCaseLabelId);
                
                // 编译 case 块
                if (switchCase->block != ZR_NULL) {
                    ZrParser_Statement_Compile(cs, switchCase->block);
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
            ZrParser_Statement_Compile(cs, defaultCase->block);
        }
    }
    
    // 解析结束标签
    resolve_label(cs, endLabelId);
}

// 编译 break/continue 语句
void compile_break_continue_statement(SZrCompilerState *cs, SZrAstNode *node) {
    SZrCompilerTryContext finallyContext;
    TZrBool hasFinallyContext = ZR_FALSE;
    SZrBreakContinueStatement *stmt;
    SZrLoopLabel *loopLabel;
    TZrSize targetLabelId;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_BREAK_CONTINUE_STATEMENT) {
        ZrParser_Compiler_Error(cs, "Expected break/continue statement", node->location);
        return;
    }

    stmt = &node->data.breakContinueStatement;
    if (cs->loopLabelStack.length == 0) {
        ZrParser_Compiler_Error(cs,
                                stmt->isBreak ? "break statement not inside a loop"
                                              : "continue statement not inside a loop",
                                node->location);
        return;
    }

    loopLabel = (SZrLoopLabel *)ZrCore_Array_Get(&cs->loopLabelStack, cs->loopLabelStack.length - 1);
    if (loopLabel == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Invalid loop label stack", node->location);
        return;
    }

    targetLabelId = stmt->isBreak ? loopLabel->breakLabelId : loopLabel->continueLabelId;
    hasFinallyContext = try_context_find_innermost_finally(cs, &finallyContext);

    if (stmt->expr != ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                stmt->isBreak ? "break statement does not support return value"
                                              : "continue statement does not support return value",
                                node->location);
        return;
    }

    if (hasFinallyContext) {
        TZrInstruction pendingInst =
                create_instruction_1(stmt->isBreak ? ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK)
                                                   : ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE),
                                     0,
                                     0);
        TZrSize pendingIndex = cs->instructionCount;
        emit_instruction(cs, pendingInst);
        add_pending_absolute_patch(cs, pendingIndex, targetLabelId);
        emit_jump_to_label(cs, finallyContext.finallyLabelId);
        return;
    }

    emit_jump_to_label(cs, targetLabelId);
}

// 编译 OUT 语句（用于生成器表达式）
void compile_out_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_OUT_STATEMENT) {
        ZrParser_Compiler_Error(cs, "Expected out statement", node->location);
        return;
    }
    
    SZrOutStatement *stmt = &node->data.outStatement;
    
    // 编译表达式
    if (stmt->expr != ZR_NULL) {
        ZrParser_Expression_Compile(cs, stmt->expr);
        TZrUInt32 valueSlot = cs->stackSlotCount - 1;
        
        // 生成生成器输出指令（用于生成器）
        // 注意：生成器机制需要运行时支持，这里先实现基本框架
        // 生成器函数会在运行时通过特殊的调用约定来处理 yield/out
        // 目前先使用 RETURN 指令返回值，后续可以扩展为专门的生成器指令
        // 生成器函数应该标记为可暂停的函数类型
        TZrInstruction returnInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16)valueSlot, 0);
        emit_instruction(cs, returnInst);
        
        // 释放值栈槽（YIELD 会处理值的传递）
        ZrParser_Compiler_TrimStackBy(cs, 1);
    } else {
        ZrParser_Compiler_Error(cs, "Out statement requires an expression", node->location);
    }
}

// 编译 throw 语句
void compile_throw_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_THROW_STATEMENT) {
        ZrParser_Compiler_Error(cs, "Expected throw statement", node->location);
        return;
    }
    
    SZrThrowStatement *stmt = &node->data.throwStatement;
    
    // 编译异常表达式
    if (stmt->expr != ZR_NULL) {
        ZrParser_Expression_Compile(cs, stmt->expr);
        TZrUInt32 exceptionSlot = cs->stackSlotCount - 1;
        
        // 生成 THROW 指令
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(THROW), (TZrUInt16)exceptionSlot, 0);
        emit_instruction(cs, inst);
    }
}

// 编译 try-catch-finally 语句
void compile_try_catch_finally_statement(SZrCompilerState *cs, SZrAstNode *node) {
    SZrTryCatchFinallyStatement *stmt;
    TZrUInt32 catchClauseStartIndex;
    TZrUInt32 handlerIndex;
    SZrCompilerExceptionHandlerInfo handlerInfo;
    TZrSize finallyLabelId;
    TZrSize afterFinallyLabelId;
    TZrBool hasFinally;
    TZrBool pushedTryContext = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        ZrParser_Compiler_Error(cs, "Expected try-catch-finally statement", node->location);
        return;
    }

    stmt = &node->data.tryCatchFinallyStatement;
    catchClauseStartIndex = (TZrUInt32)cs->catchClauseInfos.length;
    hasFinally = (TZrBool)(stmt->finallyBlock != ZR_NULL);
    finallyLabelId = hasFinally ? create_label(cs) : (TZrSize)-1;
    afterFinallyLabelId = create_label(cs);

    if (stmt->catchClauses != ZR_NULL) {
        for (TZrSize index = 0; index < stmt->catchClauses->count; index++) {
            SZrAstNode *catchClauseNode = stmt->catchClauses->nodes[index];
            SZrCompilerCatchClauseInfo catchInfo;

            if (catchClauseNode == ZR_NULL) {
                continue;
            }

            catchInfo.typeName = catch_clause_type_name(catchClauseNode);
            catchInfo.targetLabelId = create_label(cs);
            ZrCore_Array_Push(cs->state, &cs->catchClauseInfos, &catchInfo);
        }
    }

    handlerInfo.protectedStartInstructionOffset = (TZrMemoryOffset)cs->instructionCount;
    handlerInfo.finallyLabelId = finallyLabelId;
    handlerInfo.afterFinallyLabelId = afterFinallyLabelId;
    handlerInfo.catchClauseStartIndex = catchClauseStartIndex;
    handlerInfo.catchClauseCount = (TZrUInt32)(cs->catchClauseInfos.length - catchClauseStartIndex);
    handlerInfo.hasFinally = hasFinally;
    handlerIndex = (TZrUInt32)cs->exceptionHandlerInfos.length;
    ZrCore_Array_Push(cs->state, &cs->exceptionHandlerInfos, &handlerInfo);

    emit_instruction(cs, create_instruction_0(ZR_INSTRUCTION_ENUM(TRY), (TZrUInt16)handlerIndex));

    if (hasFinally) {
        SZrCompilerTryContext tryContext;
        tryContext.handlerIndex = handlerIndex;
        tryContext.finallyLabelId = finallyLabelId;
        ZrCore_Array_Push(cs->state, &cs->tryContextStack, &tryContext);
        pushedTryContext = ZR_TRUE;
    }

    if (stmt->block != ZR_NULL) {
        ZrParser_Statement_Compile(cs, stmt->block);
    }

    emit_instruction(cs, create_instruction_0(ZR_INSTRUCTION_ENUM(END_TRY), (TZrUInt16)handlerIndex));
    emit_jump_to_label(cs, hasFinally ? finallyLabelId : afterFinallyLabelId);

    if (stmt->catchClauses != ZR_NULL) {
        for (TZrSize index = 0; index < stmt->catchClauses->count; index++) {
            SZrAstNode *catchClauseNode = stmt->catchClauses->nodes[index];
            SZrCompilerCatchClauseInfo *catchInfo =
                    (SZrCompilerCatchClauseInfo *)ZrCore_Array_Get(&cs->catchClauseInfos,
                                                                   catchClauseStartIndex + index);
            TZrUInt32 bindingSlot;

            if (catchClauseNode == ZR_NULL || catchInfo == ZR_NULL) {
                continue;
            }

            resolve_label(cs, catchInfo->targetLabelId);
            enter_scope(cs);
            bindingSlot = catch_clause_binding_slot(cs, catchClauseNode);
            emit_instruction(cs,
                             create_instruction_0(ZR_INSTRUCTION_ENUM(CATCH), (TZrUInt16)bindingSlot));
            if (!cs->hasError && catch_clause_block(catchClauseNode) != ZR_NULL) {
                ZrParser_Statement_Compile(cs, catch_clause_block(catchClauseNode));
            }
            exit_scope(cs);
            emit_instruction(cs, create_instruction_0(ZR_INSTRUCTION_ENUM(END_TRY), (TZrUInt16)handlerIndex));
            emit_jump_to_label(cs, hasFinally ? finallyLabelId : afterFinallyLabelId);
        }
    }

    if (pushedTryContext && cs->tryContextStack.length > 0) {
        cs->tryContextStack.length--;
    }

    if (hasFinally) {
        resolve_label(cs, finallyLabelId);
        ZrParser_Statement_Compile(cs, stmt->finallyBlock);
        emit_instruction(cs, create_instruction_0(ZR_INSTRUCTION_ENUM(END_FINALLY), (TZrUInt16)handlerIndex));
    }

    resolve_label(cs, afterFinallyLabelId);
}

// 主编译语句函数

void compile_destructuring_object(SZrCompilerState *cs, SZrAstNode *pattern, SZrAstNode *value) {
    if (cs == ZR_NULL || pattern == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (pattern->type != ZR_AST_DESTRUCTURING_OBJECT) {
        ZrParser_Compiler_Error(cs, "Expected destructuring object pattern", pattern->location);
        return;
    }
    
    // 1. 编译右侧表达式（例如 %import("math")）
    // 如果 value 为 NULL，表示值已经在栈上（例如在 foreach 中）
    TZrUInt32 sourceSlot;
    if (value != ZR_NULL) {
        ZrParser_Expression_Compile(cs, value);
        sourceSlot = cs->stackSlotCount - 1;  // 源对象在栈顶
    } else {
        // 值已经在栈上，使用栈顶的值
        if (cs->stackSlotCount == 0) {
            ZrParser_Compiler_Error(cs, "Destructuring assignment requires a value on stack", pattern->location);
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
            TZrUInt32 varIndex = allocate_local_var(cs, keyName);
            
            // 创建键名字符串常量
            SZrTypeValue keyValue;
            ZrCore_Value_InitAsRawObject(cs->state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyName));
            keyValue.type = ZR_VALUE_TYPE_STRING;
            TZrUInt32 keyConstantIndex = add_constant(cs, &keyValue);
            
            // 将键名压栈
            TZrUInt32 keySlot = allocate_stack_slot(cs);
            TZrInstruction getKeyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)keySlot, (TZrInt32)keyConstantIndex);
            emit_instruction(cs, getKeyInst);
            
            // 使用 GETTABLE 从对象中获取值
            TZrUInt32 valueSlot = allocate_stack_slot(cs);
            TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)valueSlot, (TZrUInt16)sourceSlot, (TZrUInt16)keySlot);
            emit_instruction(cs, getTableInst);
            
            // 将值存储到局部变量
            TZrInstruction setStackInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)varIndex, (TZrInt32)valueSlot);
            emit_instruction(cs, setStackInst);
            
            // 释放临时栈槽（keySlot 和 valueSlot）
            ZrParser_Compiler_TrimStackBy(cs, 2);
        }
    }
    
    // 3. 释放源对象栈槽（只有在 value 不为 NULL 时才释放，因为如果是 NULL，值已经在栈上且需要保留）
    if (value != ZR_NULL) {
        ZrParser_Compiler_TrimStackBy(cs, 1);
    }
}

// 编译解构数组赋值：var [elem1, elem2, ...] = value;
void compile_destructuring_array(SZrCompilerState *cs, SZrAstNode *pattern, SZrAstNode *value) {
    if (cs == ZR_NULL || pattern == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (pattern->type != ZR_AST_DESTRUCTURING_ARRAY) {
        ZrParser_Compiler_Error(cs, "Expected destructuring array pattern", pattern->location);
        return;
    }
    
    // 1. 编译右侧表达式（例如 arr3）
    // 如果 value 为 NULL，表示值已经在栈上（例如在 foreach 中）
    TZrUInt32 sourceSlot;
    if (value != ZR_NULL) {
        ZrParser_Expression_Compile(cs, value);
        sourceSlot = cs->stackSlotCount - 1;  // 源数组在栈顶
    } else {
        // 值已经在栈上，使用栈顶的值
        if (cs->stackSlotCount == 0) {
            ZrParser_Compiler_Error(cs, "Destructuring assignment requires a value on stack", pattern->location);
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
            TZrUInt32 varIndex = allocate_local_var(cs, elemName);
            
            // 创建索引常量（整数 i）
            SZrTypeValue indexValue;
            ZrCore_Value_InitAsInt(cs->state, &indexValue, (TZrInt64)i);
            TZrUInt32 indexConstantIndex = add_constant(cs, &indexValue);
            
            // 将索引压栈
            TZrUInt32 indexSlot = allocate_stack_slot(cs);
            TZrInstruction getIndexInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)indexSlot, (TZrInt32)indexConstantIndex);
            emit_instruction(cs, getIndexInst);
            
            // 使用 GETTABLE 从数组中获取值
            TZrUInt32 valueSlot = allocate_stack_slot(cs);
            TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)valueSlot, (TZrUInt16)sourceSlot, (TZrUInt16)indexSlot);
            emit_instruction(cs, getTableInst);
            
            // 将值存储到局部变量
            TZrInstruction setStackInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)varIndex, (TZrInt32)valueSlot);
            emit_instruction(cs, setStackInst);
            
            // 释放临时栈槽（indexSlot 和 valueSlot）
            ZrParser_Compiler_TrimStackBy(cs, 2);
        }
    }
    
    // 3. 释放源数组栈槽（只有在 value 不为 NULL 时才释放，因为如果是 NULL，值已经在栈上且需要保留）
    if (value != ZR_NULL) {
        ZrParser_Compiler_TrimStackBy(cs, 1);
    }
}
