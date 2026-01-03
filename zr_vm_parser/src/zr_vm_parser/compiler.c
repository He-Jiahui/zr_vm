//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include <stdio.h>
#include <string.h>
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/array.h"

#include <string.h>

// 前向声明（这些函数在其他文件中实现）
extern void compile_expression(SZrCompilerState *cs, SZrAstNode *node);
extern void compile_statement(SZrCompilerState *cs, SZrAstNode *node);
static void compile_script(SZrCompilerState *cs, SZrAstNode *node);
static void compile_function_declaration(SZrCompilerState *cs, SZrAstNode *node);
TZrSize create_label(SZrCompilerState *cs);
void resolve_label(SZrCompilerState *cs, TZrSize labelId);

// 初始化编译器状态
void ZrCompilerStateInit(SZrCompilerState *cs, SZrState *state) {
    ZR_ASSERT(cs != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    
    cs->state = state;
    cs->currentFunction = ZR_NULL;
    cs->currentAst = ZR_NULL;
    
    // 初始化常量池
    ZrArrayInit(state, &cs->constants, sizeof(SZrTypeValue), 16);
    cs->constantCount = 0;
    
    // 初始化局部变量数组
    ZrArrayInit(state, &cs->localVars, sizeof(SZrFunctionLocalVariable), 16);
    cs->localVarCount = 0;
    cs->stackSlotCount = 0;
    
    // 初始化闭包变量数组
    ZrArrayInit(state, &cs->closureVars, sizeof(SZrFunctionClosureVariable), 8);
    cs->closureVarCount = 0;
    
    // 初始化指令数组
    ZrArrayInit(state, &cs->instructions, sizeof(TZrInstruction), 64);
    cs->instructionCount = 0;
    
    // 初始化作用域栈
    ZrArrayInit(state, &cs->scopeStack, sizeof(SZrScope), 8);
    
    // 初始化标签数组
    ZrArrayInit(state, &cs->labels, sizeof(SZrLabel), 8);
    
    // 初始化待解析跳转数组
    ZrArrayInit(state, &cs->pendingJumps, sizeof(SZrPendingJump), 8);
    
    // 初始化循环标签栈
    ZrArrayInit(state, &cs->loopLabelStack, sizeof(SZrLoopLabel), 4);
    
    // 初始化子函数数组
    ZrArrayInit(state, &cs->childFunctions, sizeof(SZrFunction*), 8);
    
    // 初始化错误状态
    cs->hasError = ZR_FALSE;
    cs->errorMessage = ZR_NULL;
    cs->errorLocation.start.line = 0;
    cs->errorLocation.start.column = 0;
    cs->errorLocation.end.line = 0;
    cs->errorLocation.end.column = 0;
}

// 清理解译器状态
void ZrCompilerStateFree(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }
    
    SZrState *state = cs->state;
    if (state == ZR_NULL) {
        return;
    }
    
    // 释放常量池（注意：常量值可能包含 GC 对象，但由 SZrFunction 管理）
    if (cs->constants.isValid && cs->constants.head != ZR_NULL && 
        cs->constants.capacity > 0 && cs->constants.elementSize > 0) {
        ZrArrayFree(state, &cs->constants);
    }
    
    // 释放局部变量数组
    if (cs->localVars.isValid && cs->localVars.head != ZR_NULL && 
        cs->localVars.capacity > 0 && cs->localVars.elementSize > 0) {
        ZrArrayFree(state, &cs->localVars);
    }
    
    // 释放闭包变量数组
    if (cs->closureVars.isValid && cs->closureVars.head != ZR_NULL && 
        cs->closureVars.capacity > 0 && cs->closureVars.elementSize > 0) {
        ZrArrayFree(state, &cs->closureVars);
    }
    
    // 释放指令数组
    if (cs->instructions.isValid && cs->instructions.head != ZR_NULL && 
        cs->instructions.capacity > 0 && cs->instructions.elementSize > 0) {
        ZrArrayFree(state, &cs->instructions);
    }
    
    // 释放作用域栈
    if (cs->scopeStack.isValid && cs->scopeStack.head != ZR_NULL && 
        cs->scopeStack.capacity > 0 && cs->scopeStack.elementSize > 0) {
        ZrArrayFree(state, &cs->scopeStack);
    }
    
    // 释放标签数组
    if (cs->labels.isValid && cs->labels.head != ZR_NULL && 
        cs->labels.capacity > 0 && cs->labels.elementSize > 0) {
        ZrArrayFree(state, &cs->labels);
    }
    
    // 释放待解析跳转数组
    if (cs->pendingJumps.isValid && cs->pendingJumps.head != ZR_NULL && 
        cs->pendingJumps.capacity > 0 && cs->pendingJumps.elementSize > 0) {
        ZrArrayFree(state, &cs->pendingJumps);
    }
    
    // 释放循环标签栈
    if (cs->loopLabelStack.isValid && cs->loopLabelStack.head != ZR_NULL && 
        cs->loopLabelStack.capacity > 0 && cs->loopLabelStack.elementSize > 0) {
        ZrArrayFree(state, &cs->loopLabelStack);
    }
    
    // 释放子函数数组（函数本身由 GC 管理）
    if (cs->childFunctions.isValid && cs->childFunctions.head != ZR_NULL && 
        cs->childFunctions.capacity > 0 && cs->childFunctions.elementSize > 0) {
        ZrArrayFree(state, &cs->childFunctions);
    }
}

// 报告编译错误
void ZrCompilerError(SZrCompilerState *cs, const TChar *msg, SZrFileRange location) {
    if (cs == ZR_NULL) {
        return;
    }
    
    cs->hasError = ZR_TRUE;
    cs->errorMessage = msg;
    cs->errorLocation = location;
    
    // 输出详细的错误信息（包含行列号）
    const TChar *sourceName = "unknown";
    TZrSize nameLen = 7;  // "unknown" 的长度
    if (location.source != ZR_NULL) {
        if (location.source->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            sourceName = ZrStringGetNativeStringShort(location.source);
            nameLen = location.source->shortStringLength;
        } else {
            sourceName = ZrStringGetNativeString(location.source);
            nameLen = location.source->longStringLength;
        }
    }
    
    printf("Compiler Error: %s\n", msg);
    printf("  Location: %.*s:%d:%d - %d:%d\n", 
           (int)nameLen, sourceName,
           location.start.line, location.start.column,
           location.end.line, location.end.column);
}

// 创建指令（辅助函数）
TZrInstruction create_instruction_0(EZrInstructionCode opcode, TUInt16 operandExtra) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = 0;
    return instruction;
}

TZrInstruction create_instruction_1(EZrInstructionCode opcode, TUInt16 operandExtra, TInt32 operand) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = operand;
    return instruction;
}

TZrInstruction create_instruction_2(EZrInstructionCode opcode, TUInt16 operandExtra, TUInt16 operand1, TUInt16 operand2) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operand1;
    instruction.instruction.operand.operand1[1] = operand2;
    return instruction;
}

static TZrInstruction create_instruction_4(EZrInstructionCode opcode, TUInt16 operandExtra, TUInt8 op0, TUInt8 op1, TUInt8 op2, TUInt8 op3) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand0[0] = op0;
    instruction.instruction.operand.operand0[1] = op1;
    instruction.instruction.operand.operand0[2] = op2;
    instruction.instruction.operand.operand0[3] = op3;
    return instruction;
}

// 添加指令到当前函数
void emit_instruction(SZrCompilerState *cs, TZrInstruction instruction) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    
    ZrArrayPush(cs->state, &cs->instructions, &instruction);
    // instructionCount 应该与 instructions.length 保持同步
    cs->instructionCount = cs->instructions.length;
}

// 添加常量到常量池
TUInt32 add_constant(SZrCompilerState *cs, SZrTypeValue *value) {
    if (cs == ZR_NULL || cs->hasError || value == ZR_NULL) {
        return 0;
    }
    
    // 检查常量是否已存在（简化处理：总是添加新常量）
    // TODO: 实现常量去重
    
    ZrArrayPush(cs->state, &cs->constants, value);
    TUInt32 index = (TUInt32)cs->constantCount;
    cs->constantCount++;
    return index;
}

// 分配局部变量槽位
TUInt32 allocate_local_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || cs->hasError || name == ZR_NULL) {
        return 0;
    }
    
    SZrFunctionLocalVariable localVar;
    localVar.name = name;
    localVar.offsetActivate = (TZrMemoryOffset)cs->instructionCount;
    localVar.offsetDead = 0;  // 将在变量作用域结束时设置
    
    ZrArrayPush(cs->state, &cs->localVars, &localVar);
    // localVarCount 应该与 localVars.length 保持同步
    cs->localVarCount = cs->localVars.length;
    TUInt32 index = (TUInt32)(cs->localVarCount - 1);
    cs->stackSlotCount++;
    
    return index;
}

// 查找局部变量
TUInt32 find_local_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return (TUInt32)-1;
    }
    
    // 从当前作用域开始查找
    // 使用 localVars.length 而不是 localVarCount，确保同步
    TZrSize varCount = cs->localVars.length;
    for (TZrSize i = varCount; i > 0; i--) {
        TZrSize index = i - 1;
        // 确保索引在有效范围内
        if (index < cs->localVars.length) {
            SZrFunctionLocalVariable *var = (SZrFunctionLocalVariable *)ZrArrayGet(&cs->localVars, index);
            if (var != ZR_NULL && var->name != ZR_NULL) {
                // 比较字符串
                if (ZrStringEqual(var->name, name)) {
                    return (TUInt32)index;
                }
            }
        }
    }
    
    return (TUInt32)-1;
}

// 分配栈槽
TUInt32 allocate_stack_slot(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return 0;
    }
    
    TUInt32 slot = (TUInt32)cs->stackSlotCount;
    cs->stackSlotCount++;
    return slot;
}

// 进入新作用域
void enter_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    
    SZrScope scope;
    scope.startVarIndex = cs->localVarCount;
    scope.varCount = 0;
    scope.parentCompiler = ZR_NULL;  // TODO: 处理嵌套函数
    
    ZrArrayPush(cs->state, &cs->scopeStack, &scope);
}

// 退出作用域
void exit_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (cs->scopeStack.length == 0) {
        return;
    }
    
    SZrScope *scope = (SZrScope *)ZrArrayPop(&cs->scopeStack);
    if (scope != ZR_NULL) {
        // 标记作用域内变量的结束位置
        TZrMemoryOffset endOffset = (TZrMemoryOffset)cs->instructionCount;
        // 使用 localVars.length 而不是 localVarCount，确保同步
        TZrSize varCount = cs->localVars.length;
        for (TZrSize i = scope->startVarIndex; i < scope->startVarIndex + scope->varCount; i++) {
            if (i < varCount) {
                SZrFunctionLocalVariable *var = (SZrFunctionLocalVariable *)ZrArrayGet(&cs->localVars, i);
                if (var != ZR_NULL) {
                    var->offsetDead = endOffset;
                }
            }
        }
    }
}

// 创建标签
TZrSize create_label(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return (TZrSize)-1;
    }
    
    SZrLabel label;
    label.instructionIndex = cs->instructionCount;
    label.isResolved = ZR_FALSE;
    
    ZrArrayPush(cs->state, &cs->labels, &label);
    return cs->labels.length - 1;
}

// 解析标签
void resolve_label(SZrCompilerState *cs, TZrSize labelId) {
    if (cs == ZR_NULL || cs->hasError || labelId >= cs->labels.length) {
        return;
    }
    
    SZrLabel *label = (SZrLabel *)ZrArrayGet(&cs->labels, labelId);
    if (label != ZR_NULL) {
        label->instructionIndex = cs->instructionCount;
        label->isResolved = ZR_TRUE;
        
        // 填充所有指向该标签的跳转指令的偏移量
        for (TZrSize i = 0; i < cs->pendingJumps.length; i++) {
            SZrPendingJump *pendingJump = (SZrPendingJump *)ZrArrayGet(&cs->pendingJumps, i);
            if (pendingJump != ZR_NULL && pendingJump->labelId == labelId && pendingJump->instructionIndex < cs->instructions.length) {
                TZrInstruction *jumpInst = (TZrInstruction *)ZrArrayGet(&cs->instructions, pendingJump->instructionIndex);
                if (jumpInst != ZR_NULL) {
                    // 计算相对偏移：目标指令索引 - 当前指令索引
                    TInt32 offset = (TInt32)label->instructionIndex - (TInt32)pendingJump->instructionIndex;
                    jumpInst->instruction.operand.operand2[0] = offset;
                }
            }
        }
    }
}

// 添加待解析的跳转（在 compiler.c 中定义，在 compile_statement.c 和 compile_expression.c 中使用）
void add_pending_jump(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    
    SZrPendingJump pendingJump;
    pendingJump.instructionIndex = instructionIndex;
    pendingJump.labelId = labelId;
    
    ZrArrayPush(cs->state, &cs->pendingJumps, &pendingJump);
}

// 编译表达式（在 compile_expression.c 中实现）
// 这里只声明，不实现

// 编译语句（在 compile_statement.c 中实现）
// 这里只声明，不实现

// 编译函数声明
static void compile_function_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_FUNCTION_DECLARATION) {
        ZrCompilerError(cs, "Expected function declaration", node->location);
        return;
    }
    
    SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
    
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
        ZrCompilerError(cs, "Failed to create function object", node->location);
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
    if (funcDecl->params != ZR_NULL) {
        for (TZrSize i = 0; i < funcDecl->params->count; i++) {
            SZrAstNode *paramNode = funcDecl->params->nodes[i];
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
    TBool hasVariableArguments = (funcDecl->args != ZR_NULL);
    
    // 2. 编译函数体
    if (funcDecl->body != ZR_NULL) {
        compile_statement(cs, funcDecl->body);
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
            // 使用 length 而不是 instructionCount，因为 length 是数组的实际长度
            // 确保 length > 0 才访问数组
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
            } else {
                // 如果没有任何指令，添加隐式返回 null
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
    
    // 退出函数作用域
    exit_scope(cs);
    
    // 3. 将编译结果复制到函数对象
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;
    
    // 复制指令列表
    // 使用 instructions.length 而不是 instructionCount，确保同步
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList = (TZrInstruction *)ZrMemoryRawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TUInt32)cs->instructions.length;
            // 同步 instructionCount
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
    // 使用 localVars.length 而不是 localVarCount，确保同步
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *)ZrMemoryRawMallocWithType(global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TUInt32)cs->localVars.length;
            // 同步 localVarCount
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
    
    // 恢复旧的编译器状态
    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
}

// 编译脚本
static void compile_script(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_SCRIPT) {
        ZrCompilerError(cs, "Expected script node", node->location);
        return;
    }
    
    SZrScript *script = &node->data.script;
    
    // 1. 编译模块声明（如果有）
    if (script->moduleName != ZR_NULL) {
        // TODO: 处理模块声明（注册模块到全局模块表）
        // 目前先跳过
    }
    
    // 2. 编译顶层语句
    if (script->statements != ZR_NULL) {
        printf("  Compiling %zu top-level statements (statements array: %p)...\n", 
               script->statements->count, (void*)script->statements);
        for (TZrSize i = 0; i < script->statements->count; i++) {
            SZrAstNode *stmt = script->statements->nodes[i];
            if (stmt != ZR_NULL) {
                // 根据语句类型编译
                switch (stmt->type) {
                    case ZR_AST_FUNCTION_DECLARATION:
                        compile_function_declaration(cs, stmt);
                        break;
                    case ZR_AST_VARIABLE_DECLARATION:
                    case ZR_AST_EXPRESSION_STATEMENT:
                    case ZR_AST_BLOCK:
                    case ZR_AST_RETURN_STATEMENT:
                    case ZR_AST_IF_EXPRESSION:
                    case ZR_AST_WHILE_LOOP:
                    case ZR_AST_FOR_LOOP:
                    case ZR_AST_FOREACH_LOOP:
                        compile_statement(cs, stmt);
                        break;
                    default:
                        // 其他顶层声明类型（struct, class, interface, enum, test, intermediate）
                        // 目前先跳过，后续实现
                        printf("    Skipping statement type %d (not implemented yet)\n", stmt->type);
                        break;
                }
                
                // 即使有错误，也继续编译后续语句（除非是致命错误）
                // 这样可以尽可能多地编译成功的语句
                if (cs->hasError) {
                    printf("    Compilation error at statement %zu, resetting error and continuing...\n", i);
                    // 重置错误状态，继续编译后续语句
                    cs->hasError = ZR_FALSE;
                    cs->errorMessage = ZR_NULL;
                }
            }
        }
        printf("  Finished compiling statements, total instructions: %zu\n", cs->instructionCount);
    }
    
    // 3. 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        // 使用 instructions.length 而不是 instructionCount，确保同步
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
                // 使用 length 而不是 instructionCount，因为 length 是数组的实际长度
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
            } else {
                // 如果没有任何指令，添加隐式返回 null
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

// 主编译入口（占位实现）
SZrFunction *ZrCompilerCompile(SZrState *state, SZrAstNode *ast) {
    if (state == ZR_NULL || ast == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrCompilerState cs;
    ZrCompilerStateInit(&cs, state);
    
    // 创建新函数
    cs.currentFunction = ZrFunctionNew(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrCompilerStateFree(&cs);
        return ZR_NULL;
    }
    
    // 编译脚本
    compile_script(&cs, ast);
    
    if (cs.hasError) {
        // 错误信息已在 ZrCompilerError 中输出（包含行列号）
        printf("\n=== Compilation Summary ===\n");
        printf("Status: FAILED\n");
        printf("Reason: %s\n", (cs.errorMessage != ZR_NULL) ? cs.errorMessage : "Unknown error");
        if (cs.currentFunction != ZR_NULL) {
            ZrFunctionFree(state, cs.currentFunction);
        }
        ZrCompilerStateFree(&cs);
        return ZR_NULL;
    }
    
    // 将编译结果复制到 SZrFunction
    SZrFunction *func = cs.currentFunction;
    SZrGlobalState *global = state->global;
    
    // 1. 复制指令列表
    // 使用 instructions.length 而不是 instructionCount，确保同步
    if (cs.instructions.length > 0) {
        TZrSize instSize = cs.instructions.length * sizeof(TZrInstruction);
        func->instructionsList = (TZrInstruction *)ZrMemoryRawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->instructionsList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_NULL;
        }
        memcpy(func->instructionsList, cs.instructions.head, instSize);
        func->instructionsLength = (TUInt32)cs.instructions.length;
        // 同步 instructionCount
        cs.instructionCount = cs.instructions.length;
    }
    
    // 2. 复制常量列表
    if (cs.constantCount > 0) {
        TZrSize constSize = cs.constantCount * sizeof(SZrTypeValue);
        func->constantValueList = (SZrTypeValue *)ZrMemoryRawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->constantValueList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_NULL;
        }
        memcpy(func->constantValueList, cs.constants.head, constSize);
        func->constantValueLength = (TUInt32)cs.constantCount;
    }
    
    // 3. 复制局部变量列表
    // 使用 localVars.length 而不是 localVarCount，确保同步
    if (cs.localVars.length > 0) {
        TZrSize localVarSize = cs.localVars.length * sizeof(SZrFunctionLocalVariable);
        func->localVariableList = (SZrFunctionLocalVariable *)ZrMemoryRawMallocWithType(global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->localVariableList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_NULL;
        }
        memcpy(func->localVariableList, cs.localVars.head, localVarSize);
        func->localVariableLength = (TUInt32)cs.localVars.length;
        // 同步 localVarCount
        cs.localVarCount = cs.localVars.length;
    }
    
    // 4. 复制闭包变量列表
    if (cs.closureVarCount > 0) {
        TZrSize closureVarSize = cs.closureVarCount * sizeof(SZrFunctionClosureVariable);
        func->closureValueList = (SZrFunctionClosureVariable *)ZrMemoryRawMallocWithType(global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->closureValueList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_NULL;
        }
        memcpy(func->closureValueList, cs.closureVars.head, closureVarSize);
        func->closureValueLength = (TUInt32)cs.closureVarCount;
    }
    
    // 5. 复制子函数列表
    if (cs.childFunctions.length > 0) {
        TZrSize childFuncSize = cs.childFunctions.length * sizeof(SZrFunction);
        func->childFunctionList = (struct SZrFunction *)ZrMemoryRawMallocWithType(global, childFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->childFunctionList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_NULL;
        }
        // 从指针数组复制到对象数组
        SZrFunction **srcArray = (SZrFunction **)cs.childFunctions.head;
        for (TZrSize i = 0; i < cs.childFunctions.length; i++) {
            if (srcArray[i] != ZR_NULL) {
                func->childFunctionList[i] = *srcArray[i];
            }
        }
        func->childFunctionLength = (TUInt32)cs.childFunctions.length;
    }
    
    // 6. 设置函数元数据
    func->stackSize = (TUInt32)cs.stackSlotCount;
    func->parameterCount = 0;  // TODO: 从函数声明中获取参数数量
    func->hasVariableArguments = ZR_FALSE;  // TODO: 从函数声明中获取
    func->lineInSourceStart = (ast->location.start.line > 0) ? (TUInt32)ast->location.start.line : 0;
    func->lineInSourceEnd = (ast->location.end.line > 0) ? (TUInt32)ast->location.end.line : 0;
    
    // 确保所有字段都被正确初始化（避免 ZrFunctionFree 中的断言失败）
    if (func->instructionsList == ZR_NULL) {
        func->instructionsLength = 0;
    }
    if (func->constantValueList == ZR_NULL) {
        func->constantValueLength = 0;
    }
    if (func->localVariableList == ZR_NULL) {
        func->localVariableLength = 0;
    }
    if (func->closureValueList == ZR_NULL) {
        func->closureValueLength = 0;
    }
    if (func->childFunctionList == ZR_NULL) {
        func->childFunctionLength = 0;
    }
    if (func->executionLocationInfoList == ZR_NULL) {
        func->executionLocationInfoLength = 0;
    }
    
    ZrCompilerStateFree(&cs);
    return func;
}

