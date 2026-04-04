//
// Created by Auto on 2025/01/XX.
//

#include "compile_expression_internal.h"

void compile_lambda_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_LAMBDA_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected lambda expression", node->location);
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
    TZrUInt32 oldCachedNullConstantIndex = cs->cachedNullConstantIndex;
    TZrBool oldHasCachedNullConstantIndex = cs->hasCachedNullConstantIndex;
    TZrSize oldInstructionLength = cs->instructions.length;
    TZrSize oldLocalVarLength = cs->localVars.length;
    TZrSize oldConstantLength = cs->constants.length;
    TZrSize oldClosureVarLength = cs->closureVars.length;
    TZrSize oldChildFunctionLength = cs->childFunctions.length;
    TZrSize oldChildFunctionNameMapLength = cs->childFunctionNameMap.length;
    TZrBool oldIsInConstructor = cs->isInConstructor;
    SZrAstNode *oldFunctionNode = cs->currentFunctionNode;
    TZrSize oldConstLocalVarLength = cs->constLocalVars.length;
    TZrSize oldConstParameterLength = cs->constParameters.length;
    TZrInstruction *savedParentInstructions = ZR_NULL;
    SZrFunctionLocalVariable *savedParentLocalVars = ZR_NULL;
    SZrTypeValue *savedParentConstants = ZR_NULL;
    SZrFunctionClosureVariable *savedParentClosureVars = ZR_NULL;
    TZrSize savedParentInstructionsSize = oldInstructionLength * sizeof(TZrInstruction);
    TZrSize savedParentLocalVarsSize = oldLocalVarLength * sizeof(SZrFunctionLocalVariable);
    TZrSize savedParentConstantsSize = oldConstantLength * sizeof(SZrTypeValue);
    TZrSize savedParentClosureVarsSize = oldClosureVarLength * sizeof(SZrFunctionClosureVariable);

    if (savedParentInstructionsSize > 0) {
        savedParentInstructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentInstructionsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentInstructions == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Failed to backup parent instructions for lambda expression", node->location);
            return;
        }
        memcpy(savedParentInstructions, cs->instructions.head, savedParentInstructionsSize);
    }

    if (savedParentLocalVarsSize > 0) {
        savedParentLocalVars = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentLocalVarsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentLocalVars == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent locals for lambda expression", node->location);
            return;
        }
        memcpy(savedParentLocalVars, cs->localVars.head, savedParentLocalVarsSize);
    }

    if (savedParentConstantsSize > 0) {
        savedParentConstants = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentConstantsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentConstants == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentLocalVars != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent constants for lambda expression", node->location);
            return;
        }
        memcpy(savedParentConstants, cs->constants.head, savedParentConstantsSize);
    }

    if (savedParentClosureVarsSize > 0) {
        savedParentClosureVars = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentClosureVarsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentClosureVars == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentLocalVars != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentConstants != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent closures for lambda expression", node->location);
            return;
        }
        memcpy(savedParentClosureVars, cs->closureVars.head, savedParentClosureVarsSize);
    }
    
    // 创建新的函数对象
    cs->isInConstructor = ZR_FALSE;
    cs->currentFunctionNode = node;
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;
    cs->currentFunction = ZrCore_Function_New(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create lambda function object", node->location);
        if (savedParentInstructions != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentLocalVars != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentConstants != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentClosureVars != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        cs->constLocalVars.length = oldConstLocalVarLength;
        cs->constParameters.length = oldConstParameterLength;
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
    cs->childFunctions.length = 0;
    cs->childFunctionNameMap.length = 0;
    cs->cachedNullConstantIndex = 0;
    cs->hasCachedNullConstantIndex = ZR_FALSE;
    
    // 进入函数作用域
    enter_scope(cs);
    
    // 1. 编译参数列表
    TZrUInt32 parameterCount = 0;
    if (lambda->params != ZR_NULL) {
        for (TZrSize i = 0; i < lambda->params->count; i++) {
            SZrAstNode *paramNode = lambda->params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
                    if (paramName != ZR_NULL) {
                        // 分配参数槽位
                        allocate_local_var(cs, paramName);
                        parameterCount++;
                        compiler_register_readonly_parameter_name(cs, param, paramName);

                        if (cs->typeEnv != ZR_NULL) {
                            SZrInferredType paramType;
                            if (param->typeInfo != ZR_NULL &&
                                ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                                ZrParser_InferredType_Free(cs->state, &paramType);
                            } else {
                                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                                ZrParser_InferredType_Free(cs->state, &paramType);
                            }
                        }
                    }
                }
            }
        }
    }

    if (oldFunction != ZR_NULL && lambda->block != ZR_NULL) {
        SZrCompilerState parentCompilerSnapshot = {0};
        parentCompilerSnapshot.localVars.head = (TZrByte *)savedParentLocalVars;
        parentCompilerSnapshot.localVars.elementSize = sizeof(SZrFunctionLocalVariable);
        parentCompilerSnapshot.localVars.length = oldLocalVarLength;
        parentCompilerSnapshot.localVars.capacity = oldLocalVarLength;
        parentCompilerSnapshot.localVars.isValid = ZR_TRUE;
        parentCompilerSnapshot.closureVars.head = (TZrByte *)savedParentClosureVars;
        parentCompilerSnapshot.closureVars.elementSize = sizeof(SZrFunctionClosureVariable);
        parentCompilerSnapshot.closureVars.length = oldClosureVarLength;
        parentCompilerSnapshot.closureVars.capacity = oldClosureVarLength;
        parentCompilerSnapshot.closureVars.isValid = ZR_TRUE;
        ZrParser_ExternalVariables_Analyze(cs, lambda->block, &parentCompilerSnapshot);
    }
    
    // 检查是否有可变参数
    TZrBool hasVariableArguments = (lambda->args != ZR_NULL);
    
    // 2. 编译函数体（block）
    if (lambda->block != ZR_NULL) {
        ZrParser_Statement_Compile(cs, lambda->block);
        if (!cs->hasError) {
            compiler_validate_out_parameter_definite_assignment(cs, lambda->params, lambda->block, node->location);
        }
    }
    
    // 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回 null
            TZrUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)resultSlot, (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            
            TZrInstruction returnInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16)resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode)lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TZrUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)resultSlot, (TZrInt32)constantIndex);
                        emit_instruction(cs, inst);
                        
                        TZrInstruction returnInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16)resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            }
        }
    }
    
    // 退出函数作用域
    exit_scope(cs);
    if (!cs->hasError) {
        TZrUInt32 typedLocalBindingCount = 0;
        if (!compiler_build_typed_local_bindings(cs, &cs->currentFunction->typedLocalBindings, &typedLocalBindingCount)) {
            ZrParser_Compiler_Error(cs, "Failed to build typed local metadata for lambda expression", node->location);
        } else {
            cs->currentFunction->typedLocalBindingLength = typedLocalBindingCount;
        }
    }

    if (cs->hasError) {
        if (cs->currentFunction != ZR_NULL) {
            ZrCore_Function_Free(cs->state, cs->currentFunction);
        }
        if (savedParentInstructions != ZR_NULL && savedParentInstructionsSize > 0) {
            memcpy(cs->instructions.head, savedParentInstructions, savedParentInstructionsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentLocalVars != ZR_NULL && savedParentLocalVarsSize > 0) {
            memcpy(cs->localVars.head, savedParentLocalVars, savedParentLocalVarsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentConstants != ZR_NULL && savedParentConstantsSize > 0) {
            memcpy(cs->constants.head, savedParentConstants, savedParentConstantsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentClosureVars != ZR_NULL && savedParentClosureVarsSize > 0) {
            memcpy(cs->closureVars.head, savedParentClosureVars, savedParentClosureVarsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        cs->currentFunction = oldFunction;
        cs->instructionCount = oldInstructionCount;
        cs->stackSlotCount = oldStackSlotCount;
        cs->localVarCount = oldLocalVarCount;
        cs->constantCount = oldConstantCount;
        cs->closureVarCount = oldClosureVarCount;
        cs->cachedNullConstantIndex = oldCachedNullConstantIndex;
        cs->hasCachedNullConstantIndex = oldHasCachedNullConstantIndex;
        cs->instructions.length = oldInstructionLength;
        cs->localVars.length = oldLocalVarLength;
        cs->constants.length = oldConstantLength;
        cs->closureVars.length = oldClosureVarLength;
        cs->childFunctions.length = oldChildFunctionLength;
        cs->childFunctionNameMap.length = oldChildFunctionNameMapLength;
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        cs->constLocalVars.length = oldConstLocalVarLength;
        cs->constParameters.length = oldConstParameterLength;
        return;
    }
    
    // 3. 将编译结果复制到函数对象
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;
    
    // 复制指令列表
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TZrUInt32)cs->instructions.length;
            cs->instructionCount = cs->instructions.length;
        }
    }
    
    // 复制常量列表
    if (cs->constantCount > 0) {
        TZrSize constSize = cs->constantCount * sizeof(SZrTypeValue);
        newFunc->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TZrUInt32)cs->constantCount;
        }
    }
    
    // 复制局部变量列表
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TZrUInt32)cs->localVars.length;
            cs->localVarCount = cs->localVars.length;
        }
    }
    
    // 复制闭包变量列表
    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TZrUInt32)cs->closureVarCount;
        }
    }

    if (cs->childFunctions.length > 0) {
        TZrSize childFuncSize = cs->childFunctions.length * sizeof(SZrFunction);
        newFunc->childFunctionList =
                (struct SZrFunction *)ZrCore_Memory_RawMallocWithType(global, childFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->childFunctionList != ZR_NULL) {
            SZrFunction **srcArray = (SZrFunction **)cs->childFunctions.head;
            for (TZrSize i = 0; i < cs->childFunctions.length; i++) {
                if (srcArray[i] != ZR_NULL) {
                    newFunc->childFunctionList[i] = *srcArray[i];
                }
            }
            newFunc->childFunctionLength = (TZrUInt32)cs->childFunctions.length;
            ZrCore_Function_RebindConstantFunctionValuesToChildren(newFunc);
        }
    }
    
    // 设置函数元数据
    newFunc->stackSize = (TZrUInt32)cs->stackSlotCount;
    newFunc->parameterCount = (TZrUInt16)parameterCount;
    newFunc->hasVariableArguments = hasVariableArguments;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TZrUInt32)node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TZrUInt32)node->location.end.line : 0;
    
    // 设置函数名（lambda 表达式是匿名函数，所以为 ZR_NULL）
    newFunc->functionName = ZR_NULL;
    
    if (savedParentInstructions != ZR_NULL && savedParentInstructionsSize > 0) {
        memcpy(cs->instructions.head, savedParentInstructions, savedParentInstructionsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentLocalVars != ZR_NULL && savedParentLocalVarsSize > 0) {
        memcpy(cs->localVars.head, savedParentLocalVars, savedParentLocalVarsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentConstants != ZR_NULL && savedParentConstantsSize > 0) {
        memcpy(cs->constants.head, savedParentConstants, savedParentConstantsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentClosureVars != ZR_NULL && savedParentClosureVarsSize > 0) {
        memcpy(cs->closureVars.head, savedParentClosureVars, savedParentClosureVarsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    // 恢复旧的编译器状态
    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
    cs->cachedNullConstantIndex = oldCachedNullConstantIndex;
    cs->hasCachedNullConstantIndex = oldHasCachedNullConstantIndex;
    cs->instructions.length = oldInstructionLength;
    cs->localVars.length = oldLocalVarLength;
    cs->constants.length = oldConstantLength;
    cs->closureVars.length = oldClosureVarLength;
    cs->childFunctions.length = oldChildFunctionLength;
    cs->childFunctionNameMap.length = oldChildFunctionNameMapLength;
    cs->isInConstructor = oldIsInConstructor;
    cs->currentFunctionNode = oldFunctionNode;
    cs->constLocalVars.length = oldConstLocalVarLength;
    cs->constParameters.length = oldConstParameterLength;

    if (oldFunction != ZR_NULL) {
        ZrCore_Array_Push(cs->state, &cs->childFunctions, &newFunc);
    }

    // 4. 在父函数上下文中生成 CREATE_CLOSURE。
    // Lambda 运行时值属于外层函数，常量索引和结果槽也必须从外层函数分配。
    SZrTypeValue funcValue;
    ZrCore_Value_InitAsRawObject(cs->state, &funcValue, ZR_CAST_RAW_OBJECT_AS_SUPER(newFunc));
    funcValue.type = ZR_VALUE_TYPE_FUNCTION;

    TZrUInt32 funcConstantIndex = add_constant(cs, &funcValue);
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    TZrUInt32 closureVarCount = (TZrUInt32)newFunc->closureValueLength;

    // CREATE_CLOSURE 的格式: operandExtra = destSlot, operand1[0] = functionConstantIndex, operand1[1] = closureVarCount
    TZrInstruction createClosureInst = create_instruction_2(
            ZR_INSTRUCTION_ENUM(CREATE_CLOSURE),
            (TZrUInt16)destSlot,
            (TZrUInt16)funcConstantIndex,
            (TZrUInt16)closureVarCount);
    emit_instruction(cs, createClosureInst);
}

// 辅助函数：编译块并提取最后一个表达式的值（用于if表达式等场景）
void compile_block_as_expression(SZrCompilerState *cs, SZrAstNode *blockNode) {
    if (cs == ZR_NULL || blockNode == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (blockNode->type != ZR_AST_BLOCK) {
        // 如果不是块，直接编译为表达式
        ZrParser_Expression_Compile(cs, blockNode);
        return;
    }
    
    SZrBlock *block = &blockNode->data.block;
    
    // 进入新作用域
    enter_scope(cs);
    
    // 编译块内所有语句
    if (block->body != ZR_NULL && block->body->count > 0) {
        // 编译除最后一个语句外的所有语句
        for (TZrSize i = 0; i < block->body->count - 1; i++) {
            SZrAstNode *stmt = block->body->nodes[i];
            if (stmt != ZR_NULL) {
                ZrParser_Statement_Compile(cs, stmt);
                if (cs->hasError) {
                    exit_scope(cs);
                    return;
                }
            }
        }
        
        // 编译最后一个语句，并提取其表达式的值
        SZrAstNode *lastStmt = block->body->nodes[block->body->count - 1];
        if (lastStmt != ZR_NULL) {
            if (lastStmt->type == ZR_AST_EXPRESSION_STATEMENT) {
                // 表达式语句：编译表达式，值留在栈上
                SZrExpressionStatement *exprStmt = &lastStmt->data.expressionStatement;
                if (exprStmt->expr != ZR_NULL) {
                    ZrParser_Expression_Compile(cs, exprStmt->expr);
                } else {
                    // 空表达式语句，返回null
                    SZrTypeValue nullValue;
                    ZrCore_Value_ResetAsNull(&nullValue);
                    TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                    TZrUInt32 destSlot = allocate_stack_slot(cs);
                    TZrInstruction inst = create_instruction_1(
                            ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
                    emit_instruction(cs, inst);
                }
            } else {
                // 其他类型的语句：编译后返回null
                ZrParser_Statement_Compile(cs, lastStmt);
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrUInt32 destSlot = allocate_stack_slot(cs);
                TZrInstruction inst = create_instruction_1(
                        ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
                emit_instruction(cs, inst);
            }
        } else {
            // 最后一个语句为空，返回null
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrUInt32 destSlot = allocate_stack_slot(cs);
            TZrInstruction inst = create_instruction_1(
                    ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
        }
    } else {
        // 空块，返回null
        SZrTypeValue nullValue;
        ZrCore_Value_ResetAsNull(&nullValue);
        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(
                ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
        emit_instruction(cs, inst);
    }
    
    // 退出作用域
    exit_scope(cs);
}

// 编译 If 表达式
void compile_if_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_IF_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected if expression", node->location);
        return;
    }
    
    SZrIfExpression *ifExpr = &node->data.ifExpression;
    
    // 编译条件表达式
    ZrParser_Expression_Compile(cs, ifExpr->condition);
    TZrUInt32 condSlot = ZR_COMPILE_SLOT_U32(cs->stackSlotCount - 1);
    
    // 创建 else 和 end 标签
    TZrSize elseLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)condSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, elseLabelId);
    
    // 编译 then 分支
    if (ifExpr->thenExpr != ZR_NULL) {
        // 检查是否是块，如果是块则编译块并提取最后一个表达式的值
        if (ifExpr->thenExpr->type == ZR_AST_BLOCK) {
            compile_block_as_expression(cs, ifExpr->thenExpr);
        } else {
            ZrParser_Expression_Compile(cs, ifExpr->thenExpr);
        }
    } else {
        // 如果没有then分支，使用null值
        SZrTypeValue nullValue;
        ZrCore_Value_ResetAsNull(&nullValue);
        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(
                ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
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
        // 检查是否是块，如果是块则编译块并提取最后一个表达式的值
        if (ifExpr->elseExpr->type == ZR_AST_BLOCK) {
            compile_block_as_expression(cs, ifExpr->elseExpr);
        } else if (ifExpr->elseExpr->type == ZR_AST_IF_EXPRESSION) {
            // else if 情况：递归编译
            compile_if_expression(cs, ifExpr->elseExpr);
        } else {
            ZrParser_Expression_Compile(cs, ifExpr->elseExpr);
        }
    } else {
        // 如果没有else分支，使用null值
        SZrTypeValue nullValue;
        ZrCore_Value_ResetAsNull(&nullValue);
        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(
                ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
        emit_instruction(cs, inst);
    }
    
    // 解析 end 标签
    resolve_label(cs, endLabelId);
}

// 编译 Switch 表达式
void compile_switch_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_SWITCH_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected switch expression", node->location);
        return;
    }
    
    SZrSwitchExpression *switchExpr = &node->data.switchExpression;
    
    // 编译 switch 表达式
    ZrParser_Expression_Compile(cs, switchExpr->expr);
    TZrUInt32 exprSlot = ZR_COMPILE_SLOT_U32(cs->stackSlotCount - 1);
    
    // 分配结果槽位（用于存储匹配的值）
    TZrUInt32 resultSlot = allocate_stack_slot(cs);
    
    // 创建结束标签
    TZrSize endLabelId = create_label(cs);
    
    // 编译所有 case
    TZrBool hasMatchedCase = ZR_FALSE;
    if (switchExpr->cases != ZR_NULL) {
        for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
            SZrAstNode *caseNode = switchExpr->cases->nodes[i];
            if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE) {
                continue;
            }
            
            SZrSwitchCase *switchCase = &caseNode->data.switchCase;
            
            // 编译 case 值
            ZrParser_Expression_Compile(cs, switchCase->value);
            TZrUInt32 caseValueSlot = ZR_COMPILE_SLOT_U32(cs->stackSlotCount - 1);
            
            // 比较表达式和 case 值
            TZrUInt32 compareSlot = allocate_stack_slot(cs);
            TZrInstruction compareInst = create_instruction_2(
                    ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL),
                    ZR_COMPILE_SLOT_U16(compareSlot),
                    ZR_COMPILE_SLOT_U16(exprSlot),
                    ZR_COMPILE_SLOT_U16(caseValueSlot));
            emit_instruction(cs, compareInst);
            
            // 创建下一个 case 标签
            TZrSize nextCaseLabelId = create_label(cs);
            
            // JUMP_IF false -> next case
            TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)compareSlot, 0);
            TZrSize jumpIfIndex = cs->instructionCount;
            emit_instruction(cs, jumpIfInst);
            add_pending_jump(cs, jumpIfIndex, nextCaseLabelId);
            
            // 释放临时栈槽（compareSlot 和 caseValueSlot）
            ZrParser_Compiler_TrimStackBy(cs, 2);
            
            // 编译 case 块（作为表达式，需要返回值）
            if (switchExpr->isStatement) {
                // 作为语句：编译块
                if (switchCase->block != ZR_NULL) {
                    ZrParser_Statement_Compile(cs, switchCase->block);
                }
                // 语句不需要返回值，直接跳转到结束
                TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
                TZrSize jumpEndIndex = cs->instructionCount;
                emit_instruction(cs, jumpEndInst);
                add_pending_jump(cs, jumpEndIndex, endLabelId);
            } else {
                // 作为表达式：编译块，最后一个表达式作为返回值
                if (switchCase->block != ZR_NULL) {
                    SZrBlock *block = &switchCase->block->data.block;
                    if (block->body != ZR_NULL && block->body->count > 0) {
                        // 编译块中所有语句
                        for (TZrSize j = 0; j < block->body->count - 1; j++) {
                            SZrAstNode *stmt = block->body->nodes[j];
                            if (stmt != ZR_NULL) {
                                ZrParser_Statement_Compile(cs, stmt);
                            }
                        }
                        // 最后一个语句作为返回值（如果是表达式）
                        SZrAstNode *lastStmt = block->body->nodes[block->body->count - 1];
                        if (lastStmt != ZR_NULL) {
                            // 尝试作为表达式编译
                            ZrParser_Expression_Compile(cs, lastStmt);
                            TZrUInt32 lastValueSlot = ZR_COMPILE_SLOT_U32(cs->stackSlotCount - 1);
                            // 复制到结果槽位
                            TZrInstruction copyInst = create_instruction_1(
                                    ZR_INSTRUCTION_ENUM(GET_STACK),
                                    ZR_COMPILE_SLOT_U16(resultSlot),
                                    (TZrInt32)lastValueSlot);
                            emit_instruction(cs, copyInst);
                            ZrParser_Compiler_TrimStackBy(cs, 1); // 释放 lastValueSlot
                        }
                    } else {
                        // 空块，返回 null
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                        TZrInstruction nullInst = create_instruction_1(
                                ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                ZR_COMPILE_SLOT_U16(resultSlot),
                                (TZrInt32)nullConstantIndex);
                        emit_instruction(cs, nullInst);
                    }
                } else {
                    // 空块，返回 null
                    SZrTypeValue nullValue;
                    ZrCore_Value_ResetAsNull(&nullValue);
                    TZrUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                    TZrInstruction nullInst = create_instruction_1(
                            ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                            ZR_COMPILE_SLOT_U16(resultSlot),
                            (TZrInt32)nullConstantIndex);
                    emit_instruction(cs, nullInst);
                }
                hasMatchedCase = ZR_TRUE;
                
                // 跳转到结束标签
                TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
                TZrSize jumpEndIndex = cs->instructionCount;
                emit_instruction(cs, jumpEndInst);
                add_pending_jump(cs, jumpEndIndex, endLabelId);
            }
            
            // 解析下一个 case 标签
            resolve_label(cs, nextCaseLabelId);
        }
    }
    
    // 编译 default case
    if (switchExpr->defaultCase != ZR_NULL) {
        SZrSwitchDefault *defaultCase = &switchExpr->defaultCase->data.switchDefault;
        if (switchExpr->isStatement) {
            // 作为语句：编译块
            if (defaultCase->block != ZR_NULL) {
                ZrParser_Statement_Compile(cs, defaultCase->block);
            }
        } else {
            // 作为表达式：编译块，最后一个表达式作为返回值
            if (defaultCase->block != ZR_NULL) {
                SZrBlock *block = &defaultCase->block->data.block;
                if (block->body != ZR_NULL && block->body->count > 0) {
                    // 编译块中所有语句
                    for (TZrSize j = 0; j < block->body->count - 1; j++) {
                        SZrAstNode *stmt = block->body->nodes[j];
                        if (stmt != ZR_NULL) {
                            ZrParser_Statement_Compile(cs, stmt);
                        }
                    }
                    // 最后一个语句作为返回值
                    SZrAstNode *lastStmt = block->body->nodes[block->body->count - 1];
                    if (lastStmt != ZR_NULL) {
                        ZrParser_Expression_Compile(cs, lastStmt);
                        TZrUInt32 lastValueSlot = ZR_COMPILE_SLOT_U32(cs->stackSlotCount - 1);
                        // 复制到结果槽位
                        TZrInstruction copyInst = create_instruction_1(
                                ZR_INSTRUCTION_ENUM(GET_STACK),
                                ZR_COMPILE_SLOT_U16(resultSlot),
                                (TZrInt32)lastValueSlot);
                        emit_instruction(cs, copyInst);
                        ZrParser_Compiler_TrimStackBy(cs, 1); // 释放 lastValueSlot
                    }
                } else {
                    // 空块，返回 null
                    SZrTypeValue nullValue;
                    ZrCore_Value_ResetAsNull(&nullValue);
                    TZrUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                    TZrInstruction nullInst = create_instruction_1(
                            ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                            ZR_COMPILE_SLOT_U16(resultSlot),
                            (TZrInt32)nullConstantIndex);
                    emit_instruction(cs, nullInst);
                }
            } else {
                // 空块，返回 null
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                TZrInstruction nullInst = create_instruction_1(
                        ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                        ZR_COMPILE_SLOT_U16(resultSlot),
                        (TZrInt32)nullConstantIndex);
                emit_instruction(cs, nullInst);
            }
            hasMatchedCase = ZR_TRUE;
        }
    } else if (!switchExpr->isStatement && !hasMatchedCase) {
        // 作为表达式但没有匹配的 case 也没有 default，返回 null
        SZrTypeValue nullValue;
        ZrCore_Value_ResetAsNull(&nullValue);
        TZrUInt32 nullConstantIndex = add_constant(cs, &nullValue);
        TZrInstruction nullInst = create_instruction_1(
                ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                ZR_COMPILE_SLOT_U16(resultSlot),
                (TZrInt32)nullConstantIndex);
        emit_instruction(cs, nullInst);
    }
    
    // 释放表达式栈槽
    ZrParser_Compiler_TrimStackBy(cs, 1);
    
    // 解析结束标签
    resolve_label(cs, endLabelId);
}

// 主编译表达式函数
