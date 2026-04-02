//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

void compile_meta_function(SZrCompilerState *cs, SZrAstNode *node, EZrMetaType metaType) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    SZrStructMetaFunction *metaFunc = ZR_NULL;
    SZrClassMetaFunction *classMetaFunc = ZR_NULL;
    
    if (node->type == ZR_AST_STRUCT_META_FUNCTION) {
        metaFunc = &node->data.structMetaFunction;
    } else if (node->type == ZR_AST_CLASS_META_FUNCTION) {
        classMetaFunc = &node->data.classMetaFunction;
    } else {
        ZrParser_Compiler_Error(cs, "Expected meta function node", node->location);
        return;
    }
    
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
    TZrBool oldIsInConstructor = cs->isInConstructor;
    
    // 设置 isInConstructor 标志（如果是构造函数）
    cs->isInConstructor = (metaType == ZR_META_CONSTRUCTOR) ? ZR_TRUE : ZR_FALSE;
    cs->currentFunctionNode = node;
    
    // 清空 const 变量跟踪（为新函数）
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;
    
    // 创建新的函数对象
    cs->currentFunction = ZrCore_Function_New(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create meta function object", node->location);
        cs->isInConstructor = oldIsInConstructor;
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
    
    // 进入函数作用域
    enter_scope(cs);
    
    // 编译参数列表
    TZrUInt32 parameterCount = 0;
    SZrAstNodeArray *params = ZR_NULL;
    if (metaFunc != ZR_NULL) {
        params = metaFunc->params;
    } else if (classMetaFunc != ZR_NULL) {
        params = classMetaFunc->params;
    }
    
    if (params != ZR_NULL) {
        for (TZrSize i = 0; i < params->count; i++) {
            SZrAstNode *paramNode = params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
                    if (paramName != ZR_NULL) {
                        // 分配参数槽位
                        allocate_local_var(cs, paramName);
                        parameterCount++;
                        
                        compiler_register_readonly_parameter_name(cs, param, paramName);
                        
                        // 注册参数类型到类型环境
                        if (cs->typeEnv != ZR_NULL) {
                            SZrInferredType paramType;
                            if (param->typeInfo != ZR_NULL) {
                                if (ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                                    ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                                    ZrParser_InferredType_Free(cs->state, &paramType);
                                }
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
    
    // 编译函数体
    SZrAstNode *body = ZR_NULL;
    if (metaFunc != ZR_NULL) {
        body = metaFunc->body;
    } else if (classMetaFunc != ZR_NULL) {
        body = classMetaFunc->body;
    }
    
    if (body != ZR_NULL) {
        ZrParser_Statement_Compile(cs, body);
        if (!cs->hasError) {
            compiler_validate_out_parameter_definite_assignment(cs, params, body, node->location);
        }
    }
    
    // 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
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
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
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
            }
        }
    }
    
    // 退出函数作用域
    exit_scope(cs);
    if (!cs->hasError) {
        TZrUInt32 typedLocalBindingCount = 0;
        if (!compiler_build_typed_local_bindings(cs, &cs->currentFunction->typedLocalBindings, &typedLocalBindingCount)) {
            ZrParser_Compiler_Error(cs, "Failed to build typed local metadata for meta function", node->location);
        } else {
            cs->currentFunction->typedLocalBindingLength = typedLocalBindingCount;
        }
    }
    
    // 检查是否有编译错误
    if (cs->hasError) {
        // 如果有错误，释放函数对象并恢复状态
        if (cs->currentFunction != ZR_NULL) {
            ZrCore_Function_Free(cs->state, cs->currentFunction);
            cs->currentFunction = ZR_NULL;
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
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = ZR_NULL;
        cs->constLocalVars.length = 0;
        cs->constParameters.length = 0;
        cs->instructions.length = 0;
        cs->localVars.length = 0;
        cs->constants.length = 0;
        cs->closureVars.length = 0;
        return;
    }
    
    // 清空 const 变量跟踪
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;
    cs->currentFunctionNode = ZR_NULL;
    
    // 将编译结果复制到函数对象（类似 compile_function_declaration）
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;
    
    // 复制指令列表
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList =
                (TZrInstruction *) ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TZrUInt32) cs->instructions.length;
        }
    }
    
    // 复制常量列表
    if (cs->constants.length > 0) {
        TZrSize constSize = cs->constants.length * sizeof(SZrTypeValue);
        newFunc->constantValueList =
                (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TZrUInt32) cs->constants.length;
        }
    }
    
    // 复制局部变量列表
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *) ZrCore_Memory_RawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TZrUInt32) cs->localVars.length;
        }
    }
    
    // 设置函数元数据
    newFunc->stackSize = (TZrUInt32) cs->maxStackSlotCount;
    newFunc->parameterCount = (TZrUInt16) parameterCount;
    newFunc->hasVariableArguments = ZR_FALSE;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TZrUInt32) node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TZrUInt32) node->location.end.line : 0;
    
    // 将新函数添加到子函数列表
    // 注意：对于元函数，即使 oldFunction 为 ZR_NULL（在类型声明编译时），
    // 也应该将函数添加到当前函数的 childFunctions 中（如果存在）
    // 但如果没有父函数，元函数可能无法正确管理，这里暂时不添加到列表
    // TODO: 需要更完善的元函数管理机制
    if (oldFunction != ZR_NULL) {
        ZrCore_Array_Push(cs->state, &cs->childFunctions, &newFunc);
    } else {
        // 如果没有父函数，元函数暂时不添加到列表
        // 注意：这可能导致元函数无法被正确访问，需要后续完善
        // 但是，如果没有父函数，我们需要释放这个函数对象，避免内存泄漏
        // 因为元函数在类型声明编译时不应该被保留（它们会在运行时通过类型原型创建）
        if (newFunc != ZR_NULL) {
            ZrCore_Function_Free(cs->state, newFunc);
            newFunc = ZR_NULL;
        }
    }
    
    // 恢复编译器状态
    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->maxStackSlotCount = oldMaxStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
    cs->cachedNullConstantIndex = oldCachedNullConstantIndex;
    cs->hasCachedNullConstantIndex = oldHasCachedNullConstantIndex;
    cs->isInConstructor = oldIsInConstructor;
    
    // 清空数组（但保留已分配的内存）
    cs->instructions.length = 0;
    cs->localVars.length = 0;
    cs->constants.length = 0;
    cs->closureVars.length = 0;
}

