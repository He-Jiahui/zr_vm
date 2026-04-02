//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

void compile_function_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_FUNCTION_DECLARATION) {
        ZrParser_Compiler_Error(cs, "Expected function declaration", node->location);
        return;
    }

    SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
    
    // 保存当前函数节点（用于访问参数信息）
    cs->currentFunctionNode = node;
    
    // 检查是否是构造函数，设置 isInConstructor 标志
    // 普通函数不是构造函数
    cs->isInConstructor = ZR_FALSE;
    
    // 清空 const 参数列表（为新函数）
    cs->constParameters.length = 0;

    // 注册函数类型到类型环境（在编译函数体之前注册，以便递归调用）
    compiler_register_function_type_binding(cs, funcDecl);

    // 保存当前编译器状态
    SZrFunction *oldFunction = cs->currentFunction;
    TZrSize oldInstructionCount = cs->instructionCount;
    TZrSize oldStackSlotCount = cs->stackSlotCount;
    TZrSize oldMaxStackSlotCount = cs->maxStackSlotCount;
    TZrSize oldLocalVarCount = cs->localVarCount;
    TZrSize oldConstantCount = cs->constantCount;
    TZrSize oldClosureVarCount = cs->closureVarCount;
    TZrUInt32 oldCachedNullConstantIndex = cs->cachedNullConstantIndex;
    TZrBool oldHasCachedNullConstantIndex = cs->hasCachedNullConstantIndex;
    TZrSize oldInstructionLength = cs->instructions.length;
    TZrSize oldLocalVarLength = cs->localVars.length;
    TZrSize oldConstantLength = cs->constants.length;
    TZrSize oldClosureVarLength = cs->closureVars.length;
    TZrSize oldExecutionLocationLength = cs->executionLocations.length;
    TZrSize oldCatchClauseInfoLength = cs->catchClauseInfos.length;
    TZrSize oldExceptionHandlerInfoLength = cs->exceptionHandlerInfos.length;
    TZrSize oldTryContextLength = cs->tryContextStack.length;
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
            ZrParser_Compiler_Error(cs, "Failed to backup parent instructions for function declaration", node->location);
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
            ZrParser_Compiler_Error(cs, "Failed to backup parent locals for function declaration", node->location);
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
            ZrParser_Compiler_Error(cs, "Failed to backup parent constants for function declaration", node->location);
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
            ZrParser_Compiler_Error(cs, "Failed to backup parent closures for function declaration", node->location);
            return;
        }
        memcpy(savedParentClosureVars, cs->closureVars.head, savedParentClosureVarsSize);
    }

    // 创建新的函数对象
    cs->isInConstructor = ZR_FALSE;
    cs->currentFunctionNode = node;
    cs->currentFunction = ZrCore_Function_New(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create function object", node->location);
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
    cs->maxStackSlotCount = 0;
    cs->localVarCount = 0;
    cs->constantCount = 0;
    cs->closureVarCount = 0;

    // 清空数组（但保留已分配的内存）
    cs->instructions.length = 0;
    cs->localVars.length = 0;
    cs->constants.length = 0;
    cs->closureVars.length = 0;
    cs->cachedNullConstantIndex = 0;
    cs->hasCachedNullConstantIndex = ZR_FALSE;

    // 进入函数作用域（嵌套函数需要设置父编译器引用）
    // 保存父编译器引用（如果有）
    SZrCompilerState *parentCompiler = (oldFunction != ZR_NULL) ? cs : ZR_NULL;
    enter_scope(cs);
    // 设置父编译器引用（在 enter_scope 之后，因为需要访问 scopeStack）
    if (parentCompiler != ZR_NULL && cs->scopeStack.length > 0) {
        SZrScope *currentScope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
        if (currentScope != ZR_NULL) {
            currentScope->parentCompiler = parentCompiler;
        }
    }

    // 1. 编译参数列表
    TZrUInt32 parameterCount = 0;
    if (funcDecl->params != ZR_NULL) {
        for (TZrSize i = 0; i < funcDecl->params->count; i++) {
            SZrAstNode *paramNode = funcDecl->params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
                    if (paramName != ZR_NULL) {
                        // 分配参数槽位
                        allocate_local_var(cs, paramName);
                        parameterCount++;
                        
                        compiler_register_readonly_parameter_name(cs, param, paramName);
                        
                        // 注册参数类型到类型环境（用于类型推断）
                        if (cs->typeEnv != ZR_NULL) {
                            SZrInferredType paramType;
                            if (param->typeInfo != ZR_NULL) {
                                // 从类型注解推断类型
                                if (ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                                    ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                                    ZrParser_InferredType_Free(cs->state, &paramType);
                                }
                            } else {
                                // 没有类型注解，注册为对象类型（默认）
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

    if (oldFunction != ZR_NULL && funcDecl->body != ZR_NULL) {
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
        ZrParser_ExternalVariables_Analyze(cs, funcDecl->body, &parentCompilerSnapshot);
    }

    // 检查是否有可变参数
    TZrBool hasVariableArguments = (funcDecl->args != ZR_NULL);

    // 2. 编译函数体
    if (funcDecl->body != ZR_NULL) {
        ZrParser_Statement_Compile(cs, funcDecl->body);
        if (!cs->hasError) {
            compiler_validate_out_parameter_definite_assignment(cs,
                                                                funcDecl->params,
                                                                funcDecl->body,
                                                                node->location);
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
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                       (TZrInt32) constantIndex);
            emit_instruction(cs, inst);

            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            // 使用 length 而不是 instructionCount，因为 length 是数组的实际长度
            // 确保 length > 0 才访问数组
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TZrUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                   (TZrUInt16) resultSlot, (TZrInt32) constantIndex);
                        emit_instruction(cs, inst);

                        TZrInstruction returnInst =
                                create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            } else {
                // 如果没有任何指令，添加隐式返回 null
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                           (TZrInt32) constantIndex);
                emit_instruction(cs, inst);

                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }

    // 退出函数作用域
    exit_scope(cs);
    if (!cs->hasError) {
        TZrUInt32 typedLocalBindingCount = 0;
        if (!compiler_build_typed_local_bindings(cs, &cs->currentFunction->typedLocalBindings, &typedLocalBindingCount)) {
            ZrParser_Compiler_Error(cs, "Failed to build typed local metadata for function declaration", node->location);
        } else {
            cs->currentFunction->typedLocalBindingLength = typedLocalBindingCount;
        }
    }
    
    // 清空 const 变量跟踪（函数编译完成）
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;
    cs->currentFunctionNode = ZR_NULL;

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
        cs->maxStackSlotCount = oldMaxStackSlotCount;
        cs->localVarCount = oldLocalVarCount;
        cs->constantCount = oldConstantCount;
        cs->closureVarCount = oldClosureVarCount;
        cs->cachedNullConstantIndex = oldCachedNullConstantIndex;
        cs->hasCachedNullConstantIndex = oldHasCachedNullConstantIndex;
        cs->instructions.length = oldInstructionLength;
        cs->localVars.length = oldLocalVarLength;
        cs->constants.length = oldConstantLength;
        cs->closureVars.length = oldClosureVarLength;
        cs->executionLocations.length = oldExecutionLocationLength;
        cs->catchClauseInfos.length = oldCatchClauseInfoLength;
        cs->exceptionHandlerInfos.length = oldExceptionHandlerInfoLength;
        cs->tryContextStack.length = oldTryContextLength;
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
    // 使用 instructions.length 而不是 instructionCount，确保同步
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList =
                (TZrInstruction *) ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TZrUInt32) cs->instructions.length;
            // 同步 instructionCount
            cs->instructionCount = cs->instructions.length;
        }
    } else {
        // 如果没有指令，设置为0（函数体可能为空或只有隐式返回）
        newFunc->instructionsLength = 0;
        newFunc->instructionsList = ZR_NULL;
    }

    // 复制常量列表
    // 使用 constants.length 而不是 constantCount，确保同步
    if (cs->constants.length > 0) {
        TZrSize constSize = cs->constants.length * sizeof(SZrTypeValue);
        newFunc->constantValueList =
                (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TZrUInt32) cs->constants.length;
            // 同步 constantCount
            cs->constantCount = cs->constants.length;
        }
    }

    // 复制局部变量列表
    // 使用 localVars.length 而不是 localVarCount，确保同步
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *) ZrCore_Memory_RawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TZrUInt32) cs->localVars.length;
            // 同步 localVarCount
            cs->localVarCount = cs->localVars.length;
        }
    }

    // 复制闭包变量列表
    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *) ZrCore_Memory_RawMallocWithType(
                global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TZrUInt32) cs->closureVarCount;
        }
    }

    // 设置函数元数据
    newFunc->stackSize = (TZrUInt32) cs->maxStackSlotCount;
    newFunc->parameterCount = (TZrUInt16) parameterCount;
    newFunc->hasVariableArguments = hasVariableArguments;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TZrUInt32) node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TZrUInt32) node->location.end.line : 0;
    if (!compiler_copy_function_exception_metadata_slice(cs,
                                                         newFunc,
                                                         oldExecutionLocationLength,
                                                         oldCatchClauseInfoLength,
                                                         oldExceptionHandlerInfoLength,
                                                         node)) {
        ZrParser_Compiler_Error(cs, "Failed to copy function exception metadata", node->location);
    }
    
    // 设置函数名（函数名由函数自身持有）
    // 如果有名称，存储函数名；匿名函数（lambda）为 ZR_NULL
    if (funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
        newFunc->functionName = funcDecl->name->name;
    } else {
        newFunc->functionName = ZR_NULL;  // 匿名函数
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
    cs->maxStackSlotCount = oldMaxStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
    cs->cachedNullConstantIndex = oldCachedNullConstantIndex;
    cs->hasCachedNullConstantIndex = oldHasCachedNullConstantIndex;
    cs->instructions.length = oldInstructionLength;
    cs->localVars.length = oldLocalVarLength;
    cs->constants.length = oldConstantLength;
    cs->closureVars.length = oldClosureVarLength;
    cs->executionLocations.length = oldExecutionLocationLength;
    cs->catchClauseInfos.length = oldCatchClauseInfoLength;
    cs->exceptionHandlerInfos.length = oldExceptionHandlerInfoLength;
    cs->tryContextStack.length = oldTryContextLength;
    cs->isInConstructor = oldIsInConstructor;
    cs->currentFunctionNode = oldFunctionNode;
    cs->constLocalVars.length = 0;
    cs->constParameters.length = oldConstParameterLength;

    // 将新函数添加到子函数列表
    // 注意：这里需要在父编译器的上下文中操作
    // 函数名已经存储在函数对象中（newFunc->functionName），父函数只需要维护子函数列表的索引
    // 检查是否是顶层函数声明（脚本级别的函数声明，而不是嵌套函数）
    // 如果当前编译器是脚本级别（isScriptLevel == ZR_TRUE），则这是顶层函数声明
    if (oldFunction != ZR_NULL) {
        // 无论是嵌套函数还是顶层函数，都添加到父函数的 childFunctions 中
        // 这样它们都可以通过 GET_SUB_FUNCTION 访问
        // 子函数在 childFunctions 中的索引就是添加时的位置（cs->childFunctions.length）
        ZrCore_Array_Push(cs->state, &cs->childFunctions, &newFunc);
        
        // 更新函数名到索引的映射（用于编译时查找）
        SZrChildFunctionNameMap nameMap;
        nameMap.name = funcDecl->name != ZR_NULL ? funcDecl->name->name : ZR_NULL;
        nameMap.childFunctionIndex = (TZrUInt32)(cs->childFunctions.length - 1);
        ZrCore_Array_Push(cs->state, &cs->childFunctionNameMap, &nameMap);
    } else {
        // 如果是顶层函数声明且没有父函数（不应该发生），将其保存到编译器状态
        // 这样 ZrParser_Compiler_Compile 可以返回它
        cs->topLevelFunction = newFunc;
    }

    if (oldFunction != ZR_NULL && funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
        SZrString *functionName = funcDecl->name->name;
        TZrUInt32 functionVarIndex = find_local_var(cs, functionName);
        SZrTypeValue funcValue;
        TZrUInt32 functionConstantIndex;
        TZrUInt32 closureSlot;
        TZrUInt32 closureVarCount = (TZrUInt32)newFunc->closureValueLength;

        if (functionVarIndex == (TZrUInt32)-1) {
            functionVarIndex = allocate_local_var(cs, functionName);
        }

        ZrCore_Value_InitAsRawObject(cs->state, &funcValue, ZR_CAST_RAW_OBJECT_AS_SUPER(newFunc));
        funcValue.type = ZR_VALUE_TYPE_FUNCTION;
        functionConstantIndex = add_constant(cs, &funcValue);
        closureSlot = allocate_stack_slot(cs);

        emit_instruction(
                cs,
                create_instruction_2(ZR_INSTRUCTION_ENUM(CREATE_CLOSURE),
                                     (TZrUInt16)closureSlot,
                                     (TZrUInt16)functionConstantIndex,
                                     (TZrUInt16)closureVarCount));
        emit_instruction(
                cs,
                create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                     (TZrUInt16)functionVarIndex,
                                     (TZrInt32)closureSlot));

        if (cs->isScriptLevel) {
            SZrExportedVariable exportedVar;
            exportedVar.name = functionName;
            exportedVar.stackSlot = functionVarIndex;
            exportedVar.accessModifier = ZR_ACCESS_PUBLIC;
            ZrCore_Array_Push(cs->state, &cs->pubVariables, &exportedVar);
            ZrCore_Array_Push(cs->state, &cs->proVariables, &exportedVar);
        }

    }
}

// 编译测试声明
// 语法：%test("test_name") { ... }
// 测试体按真实脚本语义编译；通过/失败由宿主边界决定。
