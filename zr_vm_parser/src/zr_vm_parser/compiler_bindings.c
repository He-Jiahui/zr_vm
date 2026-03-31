//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

SZrString *extract_simple_type_name_from_type_node(SZrAstNode *typeNode) {
    if (typeNode == ZR_NULL || typeNode->type != ZR_AST_TYPE) {
        return ZR_NULL;
    }

    SZrType *type = &typeNode->data.type;
    if (type->name == ZR_NULL || type->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }

    return type->name->data.identifier.name;
}

TZrBool compiler_type_has_constructor(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
        if (info == ZR_NULL || info->name == ZR_NULL || !ZrCore_String_Equal(info->name, typeName)) {
            continue;
        }

        for (TZrSize memberIndex = 0; memberIndex < info->members.length; memberIndex++) {
            SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, memberIndex);
            if (memberInfo != ZR_NULL && memberInfo->isMetaMethod &&
                memberInfo->metaType == ZR_META_CONSTRUCTOR) {
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }

    return ZR_FALSE;
}

void emit_constant_to_slot(SZrCompilerState *cs, TZrUInt32 slot, const SZrTypeValue *value) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return;
    }

    SZrTypeValue constantValue = *value;
    TZrUInt32 constantIndex = add_constant(cs, &constantValue);
    TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)slot,
                                               (TZrInt32)constantIndex);
    emit_instruction(cs, inst);
}

void emit_string_constant_to_slot(SZrCompilerState *cs, TZrUInt32 slot, SZrString *value) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return;
    }

    SZrTypeValue constantValue;
    ZrCore_Value_InitAsRawObject(cs->state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
    constantValue.type = ZR_VALUE_TYPE_STRING;
    emit_constant_to_slot(cs, slot, &constantValue);
}

void compiler_register_function_type_binding(SZrCompilerState *cs, SZrFunctionDeclaration *funcDecl) {
    SZrInferredType returnType;
    SZrArray paramTypes;

    if (cs == ZR_NULL || funcDecl == ZR_NULL || cs->typeEnv == ZR_NULL ||
        funcDecl->name == ZR_NULL || funcDecl->name->name == ZR_NULL || cs->hasError) {
        return;
    }

    if (funcDecl->returnType != ZR_NULL) {
        if (ZrParser_AstTypeToInferredType_Convert(cs, funcDecl->returnType, &returnType)) {
            ZrCore_Array_Init(cs->state, &paramTypes, sizeof(SZrInferredType), 8);
            if (funcDecl->params != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    SZrAstNode *paramNode = funcDecl->params->nodes[i];
                    if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                        SZrParameter *param = &paramNode->data.parameter;
                        if (param->typeInfo != ZR_NULL) {
                            SZrInferredType paramType;
                            if (ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                                ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
                            }
                        } else {
                            SZrInferredType paramType;
                            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                            ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
                        }
                    }
                }
            }
            ZrParser_TypeEnvironment_RegisterFunction(cs->state, cs->typeEnv, funcDecl->name->name, &returnType, &paramTypes);
            ZrParser_InferredType_Free(cs->state, &returnType);
            for (TZrSize i = 0; i < paramTypes.length; i++) {
                SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, i);
                if (paramType != ZR_NULL) {
                    ZrParser_InferredType_Free(cs->state, paramType);
                }
            }
            ZrCore_Array_Free(cs->state, &paramTypes);
        }
    } else {
        ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Init(cs->state, &paramTypes, sizeof(SZrInferredType), 8);
        if (funcDecl->params != ZR_NULL) {
            for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                SZrAstNode *paramNode = funcDecl->params->nodes[i];
                if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                    SZrParameter *param = &paramNode->data.parameter;
                    SZrInferredType paramType;

                    if (param->typeInfo != ZR_NULL) {
                        if (!ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                            continue;
                        }
                    } else {
                        ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                    }
                    ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
                }
            }
        }
        ZrParser_TypeEnvironment_RegisterFunction(cs->state, cs->typeEnv, funcDecl->name->name, &returnType, &paramTypes);
        ZrParser_InferredType_Free(cs->state, &returnType);
        for (TZrSize i = 0; i < paramTypes.length; i++) {
            SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, i);
            if (paramType != ZR_NULL) {
                ZrParser_InferredType_Free(cs->state, paramType);
            }
        }
        ZrCore_Array_Free(cs->state, &paramTypes);
    }
}

void compiler_register_named_value_binding_to_env(SZrCompilerState *cs,
                                                         SZrTypeEnvironment *env,
                                                         SZrString *name,
                                                         SZrString *typeName) {
    SZrInferredType existingType;
    SZrInferredType inferredType;

    if (cs == ZR_NULL || env == ZR_NULL || name == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &existingType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_TypeEnvironment_LookupVariable(cs->state, env, name, &existingType)) {
        ZrParser_InferredType_Free(cs->state, &existingType);
        return;
    }
    ZrParser_InferredType_Free(cs->state, &existingType);

    if (typeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
    } else {
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    }
    ZrParser_TypeEnvironment_RegisterVariable(cs->state, env, name, &inferredType);
    ZrParser_InferredType_Free(cs->state, &inferredType);
}

void compiler_register_extern_function_type_binding_to_env(SZrCompilerState *cs,
                                                                  SZrTypeEnvironment *env,
                                                                  SZrExternFunctionDeclaration *functionDecl) {
    SZrInferredType returnType;
    SZrArray paramTypes;

    if (cs == ZR_NULL || env == ZR_NULL || functionDecl == ZR_NULL ||
        functionDecl->name == ZR_NULL || functionDecl->name->name == ZR_NULL) {
        return;
    }

    if (functionDecl->returnType != ZR_NULL) {
        if (!ZrParser_AstTypeToInferredType_Convert(cs, functionDecl->returnType, &returnType)) {
            return;
        }
    } else {
        ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_NULL);
    }

    ZrCore_Array_Init(cs->state, &paramTypes, sizeof(SZrInferredType), functionDecl->params != ZR_NULL
                                                                         ? functionDecl->params->count
                                                                         : 0);
    if (functionDecl->params != ZR_NULL) {
        for (TZrSize i = 0; i < functionDecl->params->count; i++) {
            SZrAstNode *paramNode = functionDecl->params->nodes[i];
            SZrInferredType paramType;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            if (paramNode->data.parameter.typeInfo != ZR_NULL) {
                if (!ZrParser_AstTypeToInferredType_Convert(cs, paramNode->data.parameter.typeInfo, &paramType)) {
                    continue;
                }
            } else {
                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            }
            ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
        }
    }

    ZrParser_TypeEnvironment_RegisterFunction(cs->state, env, functionDecl->name->name, &returnType, &paramTypes);

    ZrParser_InferredType_Free(cs->state, &returnType);
    for (TZrSize i = 0; i < paramTypes.length; i++) {
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, i);
        if (paramType != ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, paramType);
        }
    }
    ZrCore_Array_Free(cs->state, &paramTypes);
}

TZrUInt32 find_local_var_in_current_scope(SZrCompilerState *cs, SZrString *name) {
    SZrScope *scope;
    TZrSize startIndex;

    if (cs == ZR_NULL || name == ZR_NULL || cs->scopeStack.length == 0) {
        return (TZrUInt32)-1;
    }

    scope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
    startIndex = scope != ZR_NULL ? scope->startVarIndex : 0;
    if (startIndex > cs->localVars.length) {
        startIndex = cs->localVars.length;
    }

    for (TZrSize i = cs->localVars.length; i > startIndex; i--) {
        SZrFunctionLocalVariable *var =
                (SZrFunctionLocalVariable *)ZrCore_Array_Get(&cs->localVars, i - 1);
        if (var != ZR_NULL && var->name != ZR_NULL && ZrCore_String_Equal(var->name, name)) {
            return var->stackSlot;
        }
    }

    return (TZrUInt32)-1;
}

void ZrParser_Compiler_PredeclareFunctionBindings(SZrCompilerState *cs, SZrAstNodeArray *statements) {
    if (cs == ZR_NULL || statements == ZR_NULL || cs->hasError) {
        return;
    }

    for (TZrSize i = 0; i < statements->count; i++) {
        SZrAstNode *stmt = statements->nodes[i];
        SZrFunctionDeclaration *funcDecl;
        TZrUInt32 slot;
        SZrTypeValue nullValue;

        if (stmt == ZR_NULL || stmt->type != ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }

        funcDecl = &stmt->data.functionDeclaration;
        if (funcDecl->name == ZR_NULL || funcDecl->name->name == ZR_NULL) {
            continue;
        }

        compiler_register_function_type_binding(cs, funcDecl);
        if (cs->hasError) {
            return;
        }

        if (find_local_var_in_current_scope(cs, funcDecl->name->name) != (TZrUInt32)-1) {
            continue;
        }

        slot = allocate_local_var(cs, funcDecl->name->name);
        ZrCore_Value_ResetAsNull(&nullValue);
        emit_constant_to_slot(cs, slot, &nullValue);
        if (cs->hasError) {
            return;
        }
    }
}

TZrUInt32 emit_load_global_identifier(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    TZrUInt32 globalSlot = allocate_stack_slot(cs);
    TZrInstruction getGlobalInst = create_instruction_0(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TZrUInt16)globalSlot);
    emit_instruction(cs, getGlobalInst);

    TZrUInt32 keySlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, keySlot, name);

    TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)globalSlot,
                                                       (TZrUInt16)globalSlot, (TZrUInt16)keySlot);
    emit_instruction(cs, getTableInst);
    ZrParser_Compiler_TrimStackToSlot(cs, globalSlot);
    return globalSlot;
}

ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitImportModuleExpression(SZrCompilerState *cs,
                                                                     SZrString *moduleName,
                                                                     SZrFileRange location) {
    SZrClosureNative *importClosure;
    SZrTypeValue importCallable;
    TZrUInt32 functionSlot;
    TZrUInt32 argumentSlot;

    if (cs == ZR_NULL || moduleName == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    importClosure = ZrCore_ClosureNative_New(cs->state, 0);
    if (importClosure == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to create internal import callable", location);
        return (TZrUInt32)-1;
    }
    importClosure->nativeFunction = ZrCore_Module_ImportNativeEntry;
    ZrCore_RawObject_MarkAsPermanent(cs->state, ZR_CAST_RAW_OBJECT_AS_SUPER(importClosure));

    ZrCore_Value_InitAsRawObject(cs->state, &importCallable, ZR_CAST_RAW_OBJECT_AS_SUPER(importClosure));
    importCallable.isNative = ZR_TRUE;

    functionSlot = allocate_stack_slot(cs);
    emit_constant_to_slot(cs, functionSlot, &importCallable);

    argumentSlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, argumentSlot, moduleName);

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          1));
    ZrParser_Compiler_TrimStackToSlot(cs, functionSlot);
    return functionSlot;
}

