//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

void enter_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    SZrScope scope;
    scope.startVarIndex = cs->localVarCount;
    scope.varCount = 0;
    scope.cleanupRegistrationCount = 0;
    // 如果当前编译器有父编译器，则新作用域的父编译器就是当前编译器
    // 否则，如果当前编译器是顶层编译器，其父编译器为NULL
    scope.parentCompiler = cs->currentFunction != ZR_NULL ? cs : ZR_NULL;

    ZrCore_Array_Push(cs->state, &cs->scopeStack, &scope);
}

// 退出作用域
void exit_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    if (cs->scopeStack.length == 0) {
        return;
    }

    SZrScope *scope = (SZrScope *) ZrCore_Array_Pop(&cs->scopeStack);
    if (scope != ZR_NULL) {
        if (scope->cleanupRegistrationCount > 0) {
            TZrInstruction cleanupInst = create_instruction_0(
                ZR_INSTRUCTION_ENUM(CLOSE_SCOPE),
                (TZrUInt16)scope->cleanupRegistrationCount);
            emit_instruction(cs, cleanupInst);
        }

        // 标记作用域内变量的结束位置
        TZrMemoryOffset endOffset = (TZrMemoryOffset) cs->instructionCount;
        // 使用 localVars.length 而不是 localVarCount，确保同步
        TZrSize varCount = cs->localVars.length;
        for (TZrSize i = scope->startVarIndex; i < scope->startVarIndex + scope->varCount; i++) {
            if (i < varCount) {
                SZrFunctionLocalVariable *var = (SZrFunctionLocalVariable *) ZrCore_Array_Get(&cs->localVars, i);
                if (var != ZR_NULL) {
                    var->offsetDead = endOffset;
                }
            }
        }
    }
}

// 进入类型作用域（推入新环境）
void enter_type_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    
    // 创建新的类型环境
    SZrTypeEnvironment *newEnv = ZrParser_TypeEnvironment_New(cs->state);
    if (newEnv == ZR_NULL) {
        return;
    }
    
    // 设置父环境为当前环境
    newEnv->parent = cs->typeEnv;
    newEnv->semanticContext = (cs->typeEnv != ZR_NULL)
                                  ? cs->typeEnv->semanticContext
                                  : cs->semanticContext;
    
    // 将当前环境推入栈
    if (cs->typeEnv != ZR_NULL) {
        ZrCore_Array_Push(cs->state, &cs->typeEnvStack, &cs->typeEnv);
    }
    
    // 设置新环境为当前环境
    cs->typeEnv = newEnv;
}

// 退出类型作用域（弹出环境）
void exit_type_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (cs->typeEnvStack.length == 0) {
        // 如果栈为空，说明这是最外层环境，只需要释放当前环境
        if (cs->typeEnv != ZR_NULL) {
            ZrParser_TypeEnvironment_Free(cs->state, cs->typeEnv);
            cs->typeEnv = ZR_NULL;
        }
        return;
    }
    
    // 释放当前环境
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(cs->state, cs->typeEnv);
    }
    
    // 从栈中弹出父环境
    SZrTypeEnvironment **parentEnvPtr = (SZrTypeEnvironment **)ZrCore_Array_Pop(&cs->typeEnvStack);
    if (parentEnvPtr != ZR_NULL) {
        cs->typeEnv = *parentEnvPtr;
    } else {
        cs->typeEnv = ZR_NULL;
    }
}

// 创建标签
TZrSize create_label(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return (TZrSize) -1;
    }

    SZrLabel label;
    label.instructionIndex = cs->instructionCount;
    label.isResolved = ZR_FALSE;

    ZrCore_Array_Push(cs->state, &cs->labels, &label);
    return cs->labels.length - 1;
}

// 解析标签
void resolve_label(SZrCompilerState *cs, TZrSize labelId) {
    if (cs == ZR_NULL || cs->hasError || labelId >= cs->labels.length) {
        return;
    }

    SZrLabel *label = (SZrLabel *) ZrCore_Array_Get(&cs->labels, labelId);
    if (label != ZR_NULL) {
        label->instructionIndex = cs->instructionCount;
        label->isResolved = ZR_TRUE;

        // 填充所有指向该标签的跳转指令的偏移量
        for (TZrSize i = 0; i < cs->pendingJumps.length; i++) {
            SZrPendingJump *pendingJump = (SZrPendingJump *) ZrCore_Array_Get(&cs->pendingJumps, i);
            if (pendingJump != ZR_NULL && pendingJump->labelId == labelId &&
                pendingJump->instructionIndex < cs->instructions.length) {
                TZrInstruction *jumpInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, pendingJump->instructionIndex);
                if (jumpInst != ZR_NULL) {
                    // 计算相对偏移：目标指令索引 - (当前指令索引 + 1)
                    // 因为 ZR_INSTRUCTION_FETCH 已经将 PC 指向下一条指令，所以需要 -1
                    TZrInt32 offset = (TZrInt32) label->instructionIndex - (TZrInt32) pendingJump->instructionIndex - 1;
                    jumpInst->instruction.operand.operand2[0] = offset;
                }
            }
        }

        for (TZrSize i = 0; i < cs->pendingAbsolutePatches.length; i++) {
            SZrPendingAbsolutePatch *pendingPatch =
                    (SZrPendingAbsolutePatch *)ZrCore_Array_Get(&cs->pendingAbsolutePatches, i);
            if (pendingPatch != ZR_NULL && pendingPatch->labelId == labelId &&
                pendingPatch->instructionIndex < cs->instructions.length) {
                TZrInstruction *inst =
                        (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, pendingPatch->instructionIndex);
                if (inst != ZR_NULL) {
                    inst->instruction.operand.operand2[0] = (TZrInt32)label->instructionIndex;
                }
            }
        }
    }
}

// 添加待解析的跳转（在 compiler.c 中定义，在 ZrParser_Statement_Compile.c 和 ZrParser_Expression_Compile.c 中使用）
void add_pending_jump(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId) {
    SZrLabel *label;

    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    if (labelId < cs->labels.length) {
        label = (SZrLabel *)ZrCore_Array_Get(&cs->labels, labelId);
        if (label != ZR_NULL && label->isResolved && instructionIndex < cs->instructions.length) {
            TZrInstruction *jumpInst =
                    (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, instructionIndex);
            if (jumpInst != ZR_NULL) {
                jumpInst->instruction.operand.operand2[0] =
                        (TZrInt32)label->instructionIndex - (TZrInt32)instructionIndex - 1;
            }
            return;
        }
    }

    SZrPendingJump pendingJump;
    pendingJump.instructionIndex = instructionIndex;
    pendingJump.labelId = labelId;

    ZrCore_Array_Push(cs->state, &cs->pendingJumps, &pendingJump);
}

void add_pending_absolute_patch(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId) {
    SZrPendingAbsolutePatch pendingPatch;

    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    pendingPatch.instructionIndex = instructionIndex;
    pendingPatch.labelId = labelId;
    ZrCore_Array_Push(cs->state, &cs->pendingAbsolutePatches, &pendingPatch);
}

// 外部变量分析辅助函数（用于闭包捕获）

// TODO: 记录引用的外部变量（简化实现：直接添加到列表）
