//
// Created by Auto on 2025/01/XX.
//

#include "compile_expression_internal.h"

SZrAstNode *find_function_declaration(SZrCompilerState *cs, SZrString *funcName) {
    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || funcName == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_NULL;
    }
    
    SZrScript *script = &cs->scriptAst->data.script;
    if (script->statements == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历顶层语句，查找函数声明
    for (TZrSize i = 0; i < script->statements->count; i++) {
        SZrAstNode *stmt = script->statements->nodes[i];
        if (stmt != ZR_NULL && stmt->type == ZR_AST_FUNCTION_DECLARATION) {
            SZrFunctionDeclaration *funcDecl = &stmt->data.functionDeclaration;
            if (funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
                if (ZrCore_String_Equal(funcDecl->name->name, funcName)) {
                    return stmt;
                }
            }
        }
    }
    
    return ZR_NULL;
}

// 根据函数参数列表解析调用参数。
// 对已知签名的调用，统一处理命名参数重排和缺失默认值回填。
ZR_PARSER_API SZrAstNodeArray *ZrParser_Compiler_MatchNamedArguments(SZrCompilerState *cs,
                                                                     SZrFunctionCall *call,
                                                                     SZrAstNodeArray *paramList) {
    if (cs == ZR_NULL || call == ZR_NULL ||
        call->args == ZR_NULL || call->argNames == ZR_NULL || paramList == ZR_NULL) {
        return call->args;
    }
    
    // 创建参数映射表：参数名 -> 参数索引
    TZrSize paramCount = paramList->count;
    SZrString **paramNames = ZrCore_Memory_RawMallocWithType(cs->state->global, 
                                                       sizeof(SZrString*) * paramCount, 
                                                       ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (paramNames == ZR_NULL) {
        return call->args;  // 内存分配失败，返回原数组
    }
    
    // 提取参数名
    for (TZrSize i = 0; i < paramCount; i++) {
        SZrAstNode *paramNode = paramList->nodes[i];
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            SZrParameter *param = &paramNode->data.parameter;
            if (param->name != ZR_NULL) {
                paramNames[i] = param->name->name;
            } else {
                paramNames[i] = ZR_NULL;
            }
        } else {
            paramNames[i] = ZR_NULL;
        }
    }
    
    // 创建重新排列的参数数组
    SZrAstNodeArray *reorderedArgs = ZrParser_AstNodeArray_New(cs->state, paramCount);
    if (reorderedArgs == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return call->args;
    }
    
    // 初始化数组，所有位置设为 ZR_NULL（表示未提供）
    // 注意：不能使用 ZrParser_AstNodeArray_Add 因为当 node 为 ZR_NULL 时会直接返回
    // 所以直接设置数组元素并手动更新 count
    for (TZrSize i = 0; i < paramCount; i++) {
        reorderedArgs->nodes[i] = ZR_NULL;
    }
    reorderedArgs->count = paramCount;  // 手动设置 count
    
    // 标记哪些参数已被提供
    TZrBool *provided = ZrCore_Memory_RawMallocWithType(cs->state->global, 
                                                sizeof(TZrBool) * paramCount, 
                                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (provided == ZR_NULL) {
        ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
        ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return call->args;
    }
    for (TZrSize i = 0; i < paramCount; i++) {
        provided[i] = ZR_FALSE;
    }
    
    // 处理位置参数（在命名参数之前）
    TZrSize positionalCount = 0;
    for (TZrSize i = 0; i < call->args->count; i++) {
        SZrString **namePtr = (SZrString**)ZrCore_Array_Get(call->argNames, i);
        if (namePtr != ZR_NULL && *namePtr == ZR_NULL) {
            // 位置参数
            if (positionalCount < paramCount) {
                reorderedArgs->nodes[positionalCount] = call->args->nodes[i];
                provided[positionalCount] = ZR_TRUE;
                positionalCount++;
            } else {
                // 位置参数过多
                ZrParser_Compiler_Error(cs, "Too many positional arguments", call->args->nodes[i]->location);
                ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
                return call->args;
            }
        } else {
            // 遇到命名参数，停止处理位置参数
            break;
        }
    }
    
    // 处理命名参数
    for (TZrSize i = 0; i < call->args->count; i++) {
        SZrString **namePtr = (SZrString**)ZrCore_Array_Get(call->argNames, i);
        if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
            // 命名参数
            SZrString *argName = *namePtr;
            TZrBool found = ZR_FALSE;
            
            // 查找参数名对应的位置
            for (TZrSize j = 0; j < paramCount; j++) {
                if (paramNames[j] != ZR_NULL) {
                    if (ZrCore_String_Equal(argName, paramNames[j])) {
                        if (provided[j]) {
                            // 参数重复
                            ZrParser_Compiler_Error(cs, "Duplicate argument name", call->args->nodes[i]->location);
                            ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                            ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                            ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
                            return call->args;
                        }
                        reorderedArgs->nodes[j] = call->args->nodes[i];
                        provided[j] = ZR_TRUE;
                        found = ZR_TRUE;
                        break;
                    }
                }
            }
            
            if (!found) {
                // 未找到匹配的参数名
                TZrNativeString nameStr = ZrCore_String_GetNativeString(argName);
                TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
                snprintf(errorMsg, sizeof(errorMsg), "Unknown argument name: %s", nameStr ? nameStr : "<null>");
                ZrParser_Compiler_Error(cs, errorMsg, call->args->nodes[i]->location);
                ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
                return call->args;
            }
        }
    }
    
    // 回填缺失参数的默认值；命名参数留下的空洞不能直接交给运行时，
    // 否则运行时只会把尾部参数补 null，无法区分“省略且有默认值”和“显式传 null”。
    for (TZrSize i = 0; i < paramCount; i++) {
        if (provided[i]) {
            continue;
        }

        SZrAstNode *paramNode = paramList->nodes[i];
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            SZrParameter *param = &paramNode->data.parameter;
            if (param->defaultValue != ZR_NULL) {
                reorderedArgs->nodes[i] = param->defaultValue;
                provided[i] = ZR_TRUE;
            }
        }
    }
    
    ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    
    return reorderedArgs;
}

// 编译函数调用表达式
void compile_function_call(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_FUNCTION_CALL) {
        ZrParser_Compiler_Error(cs, "Expected function call", node->location);
        return;
    }
    
    // 函数调用在 primary expression 中处理
    // 这里只处理参数列表
    SZrAstNodeArray *args = node->data.functionCall.args;
    if (args != ZR_NULL) {
        // 编译所有参数表达式（从右到左压栈，或从左到右，取决于调用约定）
        for (TZrSize i = 0; i < args->count; i++) {
            SZrAstNode *arg = args->nodes[i];
            if (arg != ZR_NULL) {
                ZrParser_Expression_Compile(cs, arg);
            }
        }
    }
    
    // 注意：实际的函数调用指令（FUNCTION_CALL）应该在 primary expression 中生成
    // 因为需要先编译 callee（被调用表达式）
}

// 编译成员访问表达式
void compile_member_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_MEMBER_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected member expression", node->location);
        return;
    }
    
    // 成员访问在 primary expression 中处理
    // 这里只处理属性访问
    // 注意：实际的 GET_MEMBER/SET_MEMBER 或 GET_BY_INDEX/SET_BY_INDEX 指令应该在 primary expression 中生成
    // 因为需要先编译对象表达式
}

SZrAstNodeArray *match_named_arguments(SZrCompilerState *cs,
                                       SZrFunctionCall *call,
                                       SZrAstNodeArray *paramList) {
    return ZrParser_Compiler_MatchNamedArguments(cs, call, paramList);
}

void compile_import_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrImportExpression *importExpr;
    SZrAstNode *modulePathNode;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_IMPORT_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected import expression", node->location);
        return;
    }

    importExpr = &node->data.importExpression;
    modulePathNode = importExpr->modulePath;
    if (modulePathNode == ZR_NULL || modulePathNode->type != ZR_AST_STRING_LITERAL ||
        modulePathNode->data.stringLiteral.value == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Import expression requires a normalized string module path", node->location);
        return;
    }

    if (ZrParser_Compiler_EmitImportModuleExpression(cs, modulePathNode->data.stringLiteral.value, node->location) ==
        ZR_PARSER_SLOT_NONE &&
        !cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to compile import expression", node->location);
    }
}

void compile_type_query_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrTypeQueryExpression *typeQueryExpr;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_TYPE_QUERY_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected type query expression", node->location);
        return;
    }

    typeQueryExpr = &node->data.typeQueryExpression;
    if (typeQueryExpr->operand == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Type query expression requires an operand", node->location);
        return;
    }

    if (ZrParser_Compiler_EmitTypeQueryExpression(cs, typeQueryExpr->operand, node->location) == ZR_PARSER_SLOT_NONE &&
        !cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to compile type query expression", node->location);
    }
}

static SZrString *compile_type_literal_mode_name(SZrCompilerState *cs, EZrParameterPassingMode passingMode) {
    const TZrChar *modeName = "value";

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_NULL;
    }

    switch (passingMode) {
        case ZR_PARAMETER_PASSING_MODE_IN:
            modeName = "%in";
            break;
        case ZR_PARAMETER_PASSING_MODE_OUT:
            modeName = "%out";
            break;
        case ZR_PARAMETER_PASSING_MODE_REF:
            modeName = "%ref";
            break;
        case ZR_PARAMETER_PASSING_MODE_VALUE:
        default:
            modeName = "value";
            break;
    }

    return ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)modeName);
}

static SZrString *compile_type_literal_resolve_type_name(SZrCompilerState *cs,
                                                         SZrType *typeInfo,
                                                         const TZrChar *fallbackName) {
    SZrInferredType inferredType;
    TZrChar typeNameBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    SZrString *typeName = ZR_NULL;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (typeInfo != ZR_NULL && ZrParser_AstTypeToInferredType_Convert(cs, typeInfo, &inferredType)) {
        const TZrChar *displayName =
                ZrParser_TypeNameString_Get(cs->state, &inferredType, typeNameBuffer, sizeof(typeNameBuffer));
        if (displayName != ZR_NULL && displayName[0] != '\0') {
            typeName = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)displayName);
        }
        if (typeName == ZR_NULL) {
            typeName = get_type_name_from_inferred_type(cs, &inferredType);
        }
    }
    ZrParser_InferredType_Free(cs->state, &inferredType);

    if (typeName != ZR_NULL) {
        return typeName;
    }

    return ZrCore_String_CreateFromNative(cs->state,
                                          (TZrNativeString)(fallbackName != ZR_NULL ? fallbackName : "any"));
}

static TZrBool compile_type_literal_collect_callable_metadata(SZrCompilerState *cs,
                                                              SZrType *typeInfo,
                                                              SZrFunctionType *functionType,
                                                              SZrString **outCallableName,
                                                              SZrString **outReturnTypeName,
                                                              SZrArray *parameterNames,
                                                              SZrArray *parameterTypeNames,
                                                              SZrArray *parameterModeNames,
                                                              SZrArray *genericParameterNames,
                                                              TZrBool *outIsVariadic) {
    TZrSize parameterCapacity = 0;
    SZrInferredType callableType;

    if (cs == ZR_NULL || typeInfo == ZR_NULL || functionType == ZR_NULL || outCallableName == ZR_NULL ||
        outReturnTypeName == ZR_NULL || parameterNames == ZR_NULL || parameterTypeNames == ZR_NULL ||
        parameterModeNames == ZR_NULL || genericParameterNames == ZR_NULL || outIsVariadic == ZR_NULL) {
        return ZR_FALSE;
    }

    *outCallableName = ZR_NULL;
    *outReturnTypeName = ZR_NULL;
    *outIsVariadic = ZR_FALSE;

    parameterCapacity = (functionType->params != ZR_NULL ? functionType->params->count : 0) +
                        (functionType->args != ZR_NULL ? 1 : 0);
    ZrCore_Array_Init(cs->state,
                      parameterNames,
                      sizeof(SZrString *),
                      parameterCapacity > 0 ? parameterCapacity : 1);
    ZrCore_Array_Init(cs->state,
                      parameterTypeNames,
                      sizeof(SZrString *),
                      parameterCapacity > 0 ? parameterCapacity : 1);
    ZrCore_Array_Init(cs->state,
                      parameterModeNames,
                      sizeof(SZrString *),
                      parameterCapacity > 0 ? parameterCapacity : 1);
    ZrCore_Array_Init(cs->state,
                      genericParameterNames,
                      sizeof(SZrString *),
                      functionType->generic != ZR_NULL && functionType->generic->params != ZR_NULL &&
                                      functionType->generic->params->count > 0
                              ? functionType->generic->params->count
                              : 1);

    ZrParser_InferredType_Init(cs->state, &callableType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_AstTypeToInferredType_Convert(cs, typeInfo, &callableType)) {
        ZrParser_InferredType_Free(cs->state, &callableType);
        return ZR_FALSE;
    }

    *outCallableName = get_type_name_from_inferred_type(cs, &callableType);
    ZrParser_InferredType_Free(cs->state, &callableType);
    *outReturnTypeName = compile_type_literal_resolve_type_name(cs, functionType->returnType, "void");
    *outIsVariadic = functionType->args != ZR_NULL ? ZR_TRUE : ZR_FALSE;

    if (*outCallableName == ZR_NULL || *outReturnTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (functionType->generic != ZR_NULL && functionType->generic->params != ZR_NULL) {
        for (TZrSize genericIndex = 0; genericIndex < functionType->generic->params->count; genericIndex++) {
            SZrAstNode *genericNode = functionType->generic->params->nodes[genericIndex];
            SZrString *genericName = ZR_NULL;

            if (genericNode != ZR_NULL && genericNode->type == ZR_AST_PARAMETER &&
                genericNode->data.parameter.name != ZR_NULL) {
                genericName = genericNode->data.parameter.name->name;
            }
            if (genericName != ZR_NULL) {
                ZrCore_Array_Push(cs->state, genericParameterNames, &genericName);
            }
        }
    }

    if (functionType->params != ZR_NULL) {
        for (TZrSize paramIndex = 0; paramIndex < functionType->params->count; paramIndex++) {
            SZrAstNode *paramNode = functionType->params->nodes[paramIndex];
            SZrString *parameterName = ZR_NULL;
            SZrString *parameterTypeName = ZR_NULL;
            SZrString *parameterModeName = ZR_NULL;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            if (paramNode->data.parameter.name != ZR_NULL) {
                parameterName = paramNode->data.parameter.name->name;
            }
            parameterTypeName =
                    compile_type_literal_resolve_type_name(cs, paramNode->data.parameter.typeInfo, "any");
            parameterModeName =
                    compile_type_literal_mode_name(cs, paramNode->data.parameter.passingMode);
            ZrCore_Array_Push(cs->state, parameterNames, &parameterName);
            ZrCore_Array_Push(cs->state, parameterTypeNames, &parameterTypeName);
            ZrCore_Array_Push(cs->state, parameterModeNames, &parameterModeName);
        }
    }

    if (functionType->args != ZR_NULL) {
        SZrString *parameterName = ZR_NULL;
        SZrString *parameterTypeName =
                compile_type_literal_resolve_type_name(cs, functionType->args->typeInfo, "any");
        SZrString *parameterModeName =
                compile_type_literal_mode_name(cs, functionType->args->passingMode);

        if (functionType->args->name != ZR_NULL) {
            parameterName = functionType->args->name->name;
        }

        ZrCore_Array_Push(cs->state, parameterNames, &parameterName);
        ZrCore_Array_Push(cs->state, parameterTypeNames, &parameterTypeName);
        ZrCore_Array_Push(cs->state, parameterModeNames, &parameterModeName);
    }

    return ZR_TRUE;
}

void compile_type_literal_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrTypeValue literalValue;
    SZrObject *reflectionObject = ZR_NULL;
    TZrUInt32 slot;
    SZrType *typeInfo;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_TYPE_LITERAL_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected type literal expression", node->location);
        return;
    }

    typeInfo = node->data.typeLiteralExpression.typeInfo;
    if (typeInfo == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Type literal expression requires type info", node->location);
        return;
    }

    if (typeInfo->name != ZR_NULL && typeInfo->name->type == ZR_AST_FUNCTION_TYPE) {
        SZrString *callableName = ZR_NULL;
        SZrString *returnTypeName = ZR_NULL;
        SZrArray parameterNames;
        SZrArray parameterTypeNames;
        SZrArray parameterModeNames;
        SZrArray genericParameterNames;
        TZrBool isVariadic = ZR_FALSE;

        ZrCore_Array_Construct(&parameterNames);
        ZrCore_Array_Construct(&parameterTypeNames);
        ZrCore_Array_Construct(&parameterModeNames);
        ZrCore_Array_Construct(&genericParameterNames);

        if (!compile_type_literal_collect_callable_metadata(cs,
                                                            typeInfo,
                                                            &typeInfo->name->data.functionType,
                                                            &callableName,
                                                            &returnTypeName,
                                                            &parameterNames,
                                                            &parameterTypeNames,
                                                            &parameterModeNames,
                                                            &genericParameterNames,
                                                            &isVariadic)) {
            if (parameterNames.isValid) {
                ZrCore_Array_Free(cs->state, &parameterNames);
            }
            if (parameterTypeNames.isValid) {
                ZrCore_Array_Free(cs->state, &parameterTypeNames);
            }
            if (parameterModeNames.isValid) {
                ZrCore_Array_Free(cs->state, &parameterModeNames);
            }
            if (genericParameterNames.isValid) {
                ZrCore_Array_Free(cs->state, &genericParameterNames);
            }
            ZrParser_Compiler_Error(cs, "Failed to resolve callable type literal metadata", node->location);
            return;
        }

        reflectionObject = ZrCore_Reflection_BuildCallableTypeLiteralObject(
                cs->state,
                callableName,
                returnTypeName,
                (SZrString *const *)parameterNames.head,
                (SZrString *const *)parameterTypeNames.head,
                (SZrString *const *)parameterModeNames.head,
                (TZrUInt32)parameterNames.length,
                (SZrString *const *)genericParameterNames.head,
                (TZrUInt32)genericParameterNames.length,
                isVariadic);

        if (parameterNames.isValid) {
            ZrCore_Array_Free(cs->state, &parameterNames);
        }
        if (parameterTypeNames.isValid) {
            ZrCore_Array_Free(cs->state, &parameterTypeNames);
        }
        if (parameterModeNames.isValid) {
            ZrCore_Array_Free(cs->state, &parameterModeNames);
        }
        if (genericParameterNames.isValid) {
            ZrCore_Array_Free(cs->state, &genericParameterNames);
        }
    } else {
        SZrInferredType inferredType;
        SZrString *typeName = ZR_NULL;

        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_AstTypeToInferredType_Convert(cs, typeInfo, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            ZrParser_Compiler_Error(cs, "Failed to resolve type literal metadata", node->location);
            return;
        }
        typeName = get_type_name_from_inferred_type(cs, &inferredType);
        reflectionObject = ZrCore_Reflection_BuildTypeLiteralObject(cs->state, typeName);
        ZrParser_InferredType_Free(cs->state, &inferredType);
    }

    if (reflectionObject == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to materialize type literal reflection object", node->location);
        return;
    }

    slot = allocate_stack_slot(cs);
    ZrCore_Value_InitAsRawObject(cs->state, &literalValue, ZR_CAST_RAW_OBJECT_AS_SUPER(reflectionObject));
    literalValue.type = ZR_VALUE_TYPE_OBJECT;
    emit_constant_to_slot(cs, slot, &literalValue);
}

// 编译主表达式（属性访问链和函数调用链）
void compile_primary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_PRIMARY_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected primary expression", node->location);
        return;
    }

    if (try_emit_compile_time_function_call(cs, node)) {
        return;
    }

    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    TZrUInt32 currentSlot = ZR_PARSER_SLOT_NONE;
    SZrString *rootTypeName = ZR_NULL;
    TZrBool rootIsTypeReference = ZR_FALSE;
    EZrOwnershipQualifier rootOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    TZrSize memberStartIndex = 0;
    TZrBool rootUsesSuperLookup = ZR_FALSE;
    TZrUInt32 superReceiverSlot = ZR_PARSER_SLOT_NONE;

    // 1. 编译基础属性（标识符或表达式）
    if (primary->property != ZR_NULL) {
        if (compiler_is_super_identifier_node(primary->property)) {
            if (primary->members == ZR_NULL || primary->members->count == 0) {
                ZrParser_Compiler_Error(cs, "super must be followed by a member access", node->location);
                return;
            }
            if (!compiler_resolve_super_member_context(cs,
                                                       primary->property->location,
                                                       &rootTypeName,
                                                       &superReceiverSlot,
                                                       &rootOwnershipQualifier)) {
                return;
            }
            currentSlot = emit_load_global_identifier(cs, rootTypeName);
            if (currentSlot == ZR_PARSER_SLOT_NONE || cs->hasError) {
                ZrParser_Compiler_Error(cs, "Failed to resolve direct base prototype for super.member", node->location);
                return;
            }
            rootIsTypeReference = ZR_FALSE;
            rootUsesSuperLookup = ZR_TRUE;
        } else {
            compile_expression_non_tail(cs, primary->property);
            if (cs->hasError) {
                return;
            }
        }
    } else {
        ZrParser_Compiler_Error(cs, "Primary expression property is null", node->location);
        return;
    }

    if (!rootUsesSuperLookup) {
        currentSlot = ZR_COMPILE_SLOT_U32(cs->stackSlotCount - 1);
        resolve_expression_root_type(cs, primary->property, &rootTypeName, &rootIsTypeReference);
        rootOwnershipQualifier = infer_expression_ownership_qualifier_local(cs, primary->property);
        if (cs->hasError) {
            return;
        }
    }
    compile_primary_member_chain(cs, primary->property, primary->members, memberStartIndex, &currentSlot,
                                 &rootTypeName, &rootIsTypeReference, &rootOwnershipQualifier,
                                 rootUsesSuperLookup, superReceiverSlot);
}

void compile_prototype_reference_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrPrototypeReferenceExpression *prototypeExpr;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected prototype reference expression", node->location);
        return;
    }

    prototypeExpr = &node->data.prototypeReferenceExpression;
    if (prototypeExpr->target == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Prototype reference target is null", node->location);
        return;
    }

    if (resolve_construct_target_type_name(cs, prototypeExpr->target, ZR_NULL) == ZR_NULL) {
        if (cs->hasError) {
            return;
        }
        ZrParser_Compiler_Error(cs,
                                "Prototype reference target must resolve to a registered prototype",
                                node->location);
        return;
    }

    compile_expression_non_tail(cs, prototypeExpr->target);
}

void compile_construct_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrConstructExpression *constructExpr;
    EZrObjectPrototypeType prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    SZrString *typeName;
    const TZrChar *op;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected construct expression", node->location);
        return;
    }

    constructExpr = &node->data.constructExpression;
    if (construct_expression_is_ownership_builtin(constructExpr)) {
        if (!compile_ownership_builtin_expression(cs, constructExpr, node->location) && !cs->hasError) {
            ZrParser_Compiler_Error(cs, "Failed to compile ownership builtin expression", node->location);
        }
        return;
    }

    typeName = resolve_construct_target_type_name(cs, constructExpr->target, &prototypeType);
    if (typeName == ZR_NULL) {
        if (cs->hasError) {
            return;
        }
        ZrParser_Compiler_Error(cs,
                                "Prototype construction target must resolve to a registered prototype",
                                node->location);
        return;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        ZrParser_Compiler_Error(cs, "Module values cannot be constructed directly", node->location);
        return;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
        ZrParser_Compiler_Error(cs, "Interface prototypes cannot be constructed", node->location);
        return;
    }

    op = constructExpr->isNew ? "new" : "$";
    if (emit_shorthand_constructor_instance(cs, op, typeName, constructExpr->args, node->location) == ZR_PARSER_SLOT_NONE &&
        !cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to compile construct expression", node->location);
        return;
    }

    if ((constructExpr->builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_NONE ||
         constructExpr->isUsing ||
         constructExpr->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) &&
        constructExpr->isNew) {
        if (!wrap_constructed_result_with_ownership_builtin(cs, constructExpr, node->location) && !cs->hasError) {
            ZrParser_Compiler_Error(cs, "Failed to wrap ownership-aware construction result", node->location);
        }
    }
}

// 编译数组字面量
