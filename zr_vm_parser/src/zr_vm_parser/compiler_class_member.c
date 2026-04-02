//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

SZrFunction *compile_class_member_function(SZrCompilerState *cs, SZrAstNode *node,
                                                  SZrString *superTypeName,
                                                  TZrBool injectThis, TZrUInt32 *outParameterCount) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return ZR_NULL;
    }

    SZrAstNodeArray *params = ZR_NULL;
    SZrAstNode *body = ZR_NULL;
    SZrString *functionName = ZR_NULL;
    TZrBool isConstructor = ZR_FALSE;
    SZrString *manualParamName = ZR_NULL;
    SZrType *manualParamType = ZR_NULL;

    if (node->type == ZR_AST_CLASS_METHOD) {
        SZrClassMethod *method = &node->data.classMethod;
        params = method->params;
        body = method->body;
        functionName = method->name != ZR_NULL ? method->name->name : ZR_NULL;
    } else if (node->type == ZR_AST_CLASS_META_FUNCTION) {
        SZrClassMetaFunction *metaFunc = &node->data.classMetaFunction;
        params = metaFunc->params;
        body = metaFunc->body;
        functionName = metaFunc->meta != ZR_NULL ? metaFunc->meta->name : ZR_NULL;
        if (metaFunc->meta != ZR_NULL) {
            TZrNativeString metaName = ZrCore_String_GetNativeStringShort(metaFunc->meta->name);
            if (metaName != ZR_NULL && strcmp(metaName, "constructor") == 0) {
                isConstructor = ZR_TRUE;
            }
        }
    } else if (node->type == ZR_AST_CLASS_PROPERTY) {
        SZrClassProperty *property = &node->data.classProperty;
        if (property->modifier == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Class property modifier is null", node->location);
            return ZR_NULL;
        }

        if (property->modifier->type == ZR_AST_PROPERTY_GET) {
            SZrPropertyGet *getter = &property->modifier->data.propertyGet;
            body = getter->body;
            if (getter->name != ZR_NULL) {
                functionName = compiler_create_hidden_property_accessor_name(cs, getter->name->name, ZR_FALSE);
            }
        } else if (property->modifier->type == ZR_AST_PROPERTY_SET) {
            SZrPropertySet *setter = &property->modifier->data.propertySet;
            body = setter->body;
            if (setter->name != ZR_NULL) {
                functionName = compiler_create_hidden_property_accessor_name(cs, setter->name->name, ZR_TRUE);
            }
            if (setter->param != ZR_NULL) {
                manualParamName = setter->param->name;
            }
            manualParamType = setter->targetType;
        } else {
            ZrParser_Compiler_Error(cs, "Unsupported class property modifier", node->location);
            return ZR_NULL;
        }
    } else {
        ZrParser_Compiler_Error(cs, "Expected class method, class property or class meta function", node->location);
        return ZR_NULL;
    }

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
    TZrBool oldIsInConstructor = cs->isInConstructor;
    SZrAstNode *oldFunctionNode = cs->currentFunctionNode;
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
            ZrParser_Compiler_Error(cs, "Failed to backup parent instructions for class member compilation", node->location);
            return ZR_NULL;
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
            ZrParser_Compiler_Error(cs, "Failed to backup parent locals for class member compilation", node->location);
            return ZR_NULL;
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
            ZrParser_Compiler_Error(cs, "Failed to backup parent constants for class member compilation", node->location);
            return ZR_NULL;
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
            ZrParser_Compiler_Error(cs, "Failed to backup parent closures for class member compilation", node->location);
            return ZR_NULL;
        }
        memcpy(savedParentClosureVars, cs->closureVars.head, savedParentClosureVarsSize);
    }

    cs->isInConstructor = isConstructor ? ZR_TRUE : ZR_FALSE;
    cs->currentFunctionNode = node;
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;

    cs->currentFunction = ZrCore_Function_New(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create class member function object", node->location);
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        return ZR_NULL;
    }

    cs->instructionCount = 0;
    cs->stackSlotCount = 0;
    cs->maxStackSlotCount = 0;
    cs->localVarCount = 0;
    cs->constantCount = 0;
    cs->closureVarCount = 0;
    cs->instructions.length = 0;
    cs->localVars.length = 0;
    cs->constants.length = 0;
    cs->closureVars.length = 0;
    cs->cachedNullConstantIndex = 0;
    cs->hasCachedNullConstantIndex = ZR_FALSE;

    enter_scope(cs);

    TZrUInt32 parameterCount = 0;
    if (injectThis) {
        EZrOwnershipQualifier thisOwnershipQualifier =
                get_implicit_this_ownership_qualifier(get_member_receiver_qualifier(node));
        SZrString *thisName = ZrCore_String_CreateFromNative(cs->state, "this");
        if (thisName != ZR_NULL) {
            allocate_local_var(cs, thisName);
            parameterCount++;
            if (cs->typeEnv != ZR_NULL) {
                SZrInferredType thisType;
                if (cs->currentTypeName != ZR_NULL) {
                    ZrParser_InferredType_InitFull(cs->state, &thisType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE,
                                           cs->currentTypeName);
                } else {
                    ZrParser_InferredType_Init(cs->state, &thisType, ZR_VALUE_TYPE_OBJECT);
                }
                thisType.ownershipQualifier = thisOwnershipQualifier;
                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, thisName, &thisType);
                ZrParser_InferredType_Free(cs->state, &thisType);
            }
        }
    }

    if (params != ZR_NULL) {
        for (TZrSize i = 0; i < params->count; i++) {
            SZrAstNode *paramNode = params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL && param->name->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
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

    if (manualParamName != ZR_NULL) {
        allocate_local_var(cs, manualParamName);
        parameterCount++;

        if (cs->typeEnv != ZR_NULL) {
            SZrInferredType paramType;
            if (manualParamType != ZR_NULL && ZrParser_AstTypeToInferredType_Convert(cs, manualParamType, &paramType)) {
                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, manualParamName, &paramType);
                ZrParser_InferredType_Free(cs->state, &paramType);
            } else {
                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, manualParamName, &paramType);
                ZrParser_InferredType_Free(cs->state, &paramType);
            }
        }
    }

    if (!cs->hasError && isConstructor && injectThis && node->type == ZR_AST_CLASS_META_FUNCTION &&
        superTypeName != ZR_NULL) {
        SZrClassMetaFunction *metaFunc = &node->data.classMetaFunction;
        if (metaFunc->hasSuperCall && compiler_type_has_constructor(cs, superTypeName)) {
            emit_super_constructor_call(cs, superTypeName, metaFunc->superArgs);
        }
    }

    if (body != ZR_NULL) {
        ZrParser_Statement_Compile(cs, body);
        if (!cs->hasError) {
            compiler_validate_out_parameter_definite_assignment(cs, params, body, node->location);
        }
    }

    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            TZrUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)resultSlot,
                                                       (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16)resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            TZrInstruction *lastInst =
                    (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
            if (lastInst != ZR_NULL &&
                (EZrInstructionCode)lastInst->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)resultSlot,
                                                           (TZrInt32)constantIndex);
                emit_instruction(cs, inst);
                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16)resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }

    exit_scope(cs);
    if (!cs->hasError) {
        TZrUInt32 typedLocalBindingCount = 0;
        if (!compiler_build_typed_local_bindings(cs, &cs->currentFunction->typedLocalBindings, &typedLocalBindingCount)) {
            ZrParser_Compiler_Error(cs, "Failed to build typed local metadata for class member", node->location);
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
        if (savedParentConstants != ZR_NULL) {
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
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        cs->constLocalVars.length = 0;
        cs->constParameters.length = 0;
        return ZR_NULL;
    }

    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;

    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList =
                (TZrInstruction *)ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TZrUInt32)cs->instructions.length;
        }
    }

    if (cs->constants.length > 0) {
        TZrSize constSize = cs->constants.length * sizeof(SZrTypeValue);
        newFunc->constantValueList =
                (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TZrUInt32)cs->constants.length;
        }
    }

    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TZrUInt32)cs->localVars.length;
        }
    }

    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(
                global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TZrUInt32)cs->closureVarCount;
        }
    }

    newFunc->stackSize = (TZrUInt32)cs->maxStackSlotCount;
    newFunc->parameterCount = (TZrUInt16)parameterCount;
    newFunc->hasVariableArguments = ZR_FALSE;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TZrUInt32)node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TZrUInt32)node->location.end.line : 0;
    newFunc->functionName = functionName;

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
    cs->isInConstructor = oldIsInConstructor;
    cs->currentFunctionNode = oldFunctionNode;
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;

    if (outParameterCount != ZR_NULL) {
        *outParameterCount = parameterCount;
    }

    return newFunc;
}

// 编译 class 声明
