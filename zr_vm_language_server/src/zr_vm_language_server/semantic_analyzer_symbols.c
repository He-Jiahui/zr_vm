//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"

SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName);

static SZrInferredType *allocate_object_type_info(SZrState *state) {
    SZrInferredType *typeInfo;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    typeInfo = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
    if (typeInfo != ZR_NULL) {
        ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
    }

    return typeInfo;
}

static void copy_inferred_type_into(SZrState *state,
                                    SZrInferredType *dest,
                                    const SZrInferredType *src) {
    if (state == ZR_NULL || dest == ZR_NULL || src == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Free(state, dest);
    ZrParser_InferredType_Copy(state, dest, src);
}

static TZrBool infer_symbol_expression_type(SZrState *state,
                                            SZrSemanticAnalyzer *analyzer,
                                            SZrAstNode *node,
                                            SZrInferredType *result) {
    SZrString *name;
    SZrSymbol *symbol;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
            ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_BOOL);
            return ZR_TRUE;

        case ZR_AST_INTEGER_LITERAL:
            ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_INT64);
            return ZR_TRUE;

        case ZR_AST_FLOAT_LITERAL:
            ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_DOUBLE);
            return ZR_TRUE;

        case ZR_AST_STRING_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_STRING);
            return ZR_TRUE;

        case ZR_AST_CHAR_LITERAL:
            ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_INT8);
            return ZR_TRUE;

        case ZR_AST_NULL_LITERAL:
            ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_NULL);
            return ZR_TRUE;

        case ZR_AST_IMPORT_EXPRESSION:
            if (analyzer->compilerState != ZR_NULL &&
                ZrParser_ExpressionType_Infer(analyzer->compilerState, node, result)) {
                return ZR_TRUE;
            }

            if (node->data.importExpression.modulePath != ZR_NULL &&
                node->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
                node->data.importExpression.modulePath->data.stringLiteral.value != ZR_NULL) {
                ZrParser_InferredType_InitFull(state,
                                               result,
                                               ZR_VALUE_TYPE_OBJECT,
                                               ZR_FALSE,
                                               node->data.importExpression.modulePath->data.stringLiteral.value);
            } else {
                ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_OBJECT);
            }
            return ZR_TRUE;

        case ZR_AST_LOGICAL_EXPRESSION:
            ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_BOOL);
            return ZR_TRUE;

        case ZR_AST_IDENTIFIER_LITERAL:
            name = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, node);
            symbol = name != ZR_NULL
                     ? ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable, name, node->location)
                     : ZR_NULL;
            if (symbol != ZR_NULL && symbol->typeInfo != ZR_NULL) {
                ZrParser_InferredType_Copy(state, result, symbol->typeInfo);
            } else {
                ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_OBJECT);
            }
            return ZR_TRUE;

        case ZR_AST_BINARY_EXPRESSION: {
            SZrInferredType leftType;
            SZrInferredType rightType;
            TZrBool hasLeftType;
            TZrBool hasRightType;

            ZrParser_InferredType_Init(state, &leftType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_InferredType_Init(state, &rightType, ZR_VALUE_TYPE_OBJECT);
            hasLeftType = infer_symbol_expression_type(state, analyzer, node->data.binaryExpression.left, &leftType);
            hasRightType = infer_symbol_expression_type(state, analyzer, node->data.binaryExpression.right, &rightType);
            if (hasLeftType && hasRightType) {
                if (leftType.baseType == ZR_VALUE_TYPE_STRING || rightType.baseType == ZR_VALUE_TYPE_STRING) {
                    ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_STRING);
                } else if (!ZrParser_InferredType_GetCommonType(state, result, &leftType, &rightType)) {
                    ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_OBJECT);
                }
            } else {
                ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_OBJECT);
            }
            ZrParser_InferredType_Free(state, &rightType);
            ZrParser_InferredType_Free(state, &leftType);
            return ZR_TRUE;
        }

        case ZR_AST_LAMBDA_EXPRESSION:
            ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_CLOSURE);
            return ZR_TRUE;

        default:
            if (analyzer->compilerState != ZR_NULL &&
                ZrParser_ExpressionType_Infer(analyzer->compilerState, node, result)) {
                return ZR_TRUE;
            }
            ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_OBJECT);
            return ZR_TRUE;
    }
}

static SZrInferredType *create_type_info_from_type_node(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        const SZrType *typeNode) {
    SZrInferredType *typeInfo = allocate_object_type_info(state);

    if (typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeNode != ZR_NULL &&
        analyzer != ZR_NULL &&
        analyzer->compilerState != ZR_NULL &&
        ZrParser_AstTypeToInferredType_Convert(analyzer->compilerState, typeNode, typeInfo)) {
        return typeInfo;
    }

    if (typeNode != ZR_NULL && analyzer != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state,
                                                                         analyzer,
                                                                         typeNode->name != ZR_NULL
                                                                                 ? typeNode->name->location
                                                                                 : ZrParser_FileRange_Create(
                                                                                           ZrParser_FilePosition_Create(0, 0, 0),
                                                                                           ZrParser_FilePosition_Create(0, 0, 0),
                                                                                           ZR_NULL));
    }

    if (typeNode != ZR_NULL) {
        typeInfo->ownershipQualifier = typeNode->ownershipQualifier;
    }

    return typeInfo;
}

static SZrInferredType *create_type_info_for_variable(SZrState *state,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrVariableDeclaration *varDecl) {
    SZrInferredType *typeInfo;

    if (state == ZR_NULL || varDecl == ZR_NULL) {
        return ZR_NULL;
    }

    if (varDecl->typeInfo != ZR_NULL) {
        return create_type_info_from_type_node(state, analyzer, varDecl->typeInfo);
    }

    typeInfo = allocate_object_type_info(state);
    if (typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    if (varDecl->value != ZR_NULL) {
        SZrInferredType inferredType;
        ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (infer_symbol_expression_type(state, analyzer, varDecl->value, &inferredType)) {
            copy_inferred_type_into(state, typeInfo, &inferredType);
        }
        ZrParser_InferredType_Free(state, &inferredType);
    }

    return typeInfo;
}

static void register_function_type_binding_in_env(SZrState *state,
                                                  SZrSemanticAnalyzer *analyzer,
                                                  SZrTypeEnvironment *typeEnv,
                                                  SZrFunctionDeclaration *funcDecl) {
    SZrCompilerState *compilerState;
    SZrInferredType returnType;
    SZrArray paramTypes;

    if (state == ZR_NULL || analyzer == ZR_NULL || funcDecl == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    if (compilerState == ZR_NULL ||
        typeEnv == ZR_NULL ||
        funcDecl->name == ZR_NULL ||
        funcDecl->name->name == ZR_NULL) {
        return;
    }

    if (funcDecl->returnType != ZR_NULL &&
        !ZrParser_AstTypeToInferredType_Convert(compilerState, funcDecl->returnType, &returnType)) {
        return;
    }

    if (funcDecl->returnType == ZR_NULL) {
        ZrParser_InferredType_Init(state, &returnType, ZR_VALUE_TYPE_OBJECT);
    }

    ZrCore_Array_Init(state, &paramTypes, sizeof(SZrInferredType), funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
    if (funcDecl->params != ZR_NULL) {
        for (TZrSize index = 0; index < funcDecl->params->count; index++) {
            SZrAstNode *paramNode = funcDecl->params->nodes[index];
            SZrInferredType paramType;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            if (paramNode->data.parameter.typeInfo != ZR_NULL) {
                if (!ZrParser_AstTypeToInferredType_Convert(compilerState,
                                                            paramNode->data.parameter.typeInfo,
                                                            &paramType)) {
                    continue;
                }
            } else {
                ZrParser_InferredType_Init(state, &paramType, ZR_VALUE_TYPE_OBJECT);
            }

            ZrCore_Array_Push(state, &paramTypes, &paramType);
        }
    }

    ZrParser_TypeEnvironment_RegisterFunction(state, typeEnv, funcDecl->name->name, &returnType, &paramTypes);

    ZrParser_InferredType_Free(state, &returnType);
    for (TZrSize index = 0; index < paramTypes.length; index++) {
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, index);
        if (paramType != ZR_NULL) {
            ZrParser_InferredType_Free(state, paramType);
        }
    }
    ZrCore_Array_Free(state, &paramTypes);
}

static void register_extern_function_type_binding_in_env(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrTypeEnvironment *typeEnv,
                                                         SZrAstNode *declarationNode,
                                                         SZrExternFunctionDeclaration *funcDecl) {
    SZrCompilerState *compilerState;
    SZrInferredType returnType;
    SZrArray paramTypes;
    SZrArray parameterPassingModes;

    if (state == ZR_NULL || analyzer == ZR_NULL || declarationNode == ZR_NULL || funcDecl == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    if (compilerState == ZR_NULL ||
        typeEnv == ZR_NULL ||
        funcDecl->name == ZR_NULL ||
        funcDecl->name->name == ZR_NULL) {
        return;
    }

    if (funcDecl->returnType != ZR_NULL &&
        !ZrParser_AstTypeToInferredType_Convert(compilerState, funcDecl->returnType, &returnType)) {
        return;
    }

    if (funcDecl->returnType == ZR_NULL) {
        ZrParser_InferredType_Init(state, &returnType, ZR_VALUE_TYPE_NULL);
    }

    ZrCore_Array_Init(state,
                      &paramTypes,
                      sizeof(SZrInferredType),
                      funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
    ZrCore_Array_Init(state,
                      &parameterPassingModes,
                      sizeof(EZrParameterPassingMode),
                      funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
    if (funcDecl->params != ZR_NULL) {
        for (TZrSize index = 0; index < funcDecl->params->count; index++) {
            SZrAstNode *paramNode = funcDecl->params->nodes[index];
            SZrInferredType paramType;
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            if (paramNode->data.parameter.typeInfo != ZR_NULL) {
                if (!ZrParser_AstTypeToInferredType_Convert(compilerState,
                                                            paramNode->data.parameter.typeInfo,
                                                            &paramType)) {
                    continue;
                }
            } else {
                ZrParser_InferredType_Init(state, &paramType, ZR_VALUE_TYPE_OBJECT);
            }

            passingMode = paramNode->data.parameter.passingMode;
            ZrCore_Array_Push(state, &paramTypes, &paramType);
            ZrCore_Array_Push(state, &parameterPassingModes, &passingMode);
        }
    }

    ZrParser_TypeEnvironment_RegisterFunctionEx(state,
                                                typeEnv,
                                                funcDecl->name->name,
                                                &returnType,
                                                &paramTypes,
                                                ZR_NULL,
                                                &parameterPassingModes,
                                                declarationNode);

    ZrParser_InferredType_Free(state, &returnType);
    for (TZrSize index = 0; index < paramTypes.length; index++) {
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, index);
        if (paramType != ZR_NULL) {
            ZrParser_InferredType_Free(state, paramType);
        }
    }
    ZrCore_Array_Free(state, &paramTypes);
    ZrCore_Array_Free(state, &parameterPassingModes);
}

static void register_function_type_binding(SZrState *state,
                                           SZrSemanticAnalyzer *analyzer,
                                           SZrFunctionDeclaration *funcDecl) {
    register_function_type_binding_in_env(state,
                                          analyzer,
                                          analyzer != ZR_NULL && analyzer->compilerState != ZR_NULL
                                              ? analyzer->compilerState->typeEnv
                                              : ZR_NULL,
                                          funcDecl);
}

static TZrBool file_range_contains_range(SZrFileRange outer, SZrFileRange inner) {
    if (outer.start.offset > 0 && outer.end.offset > 0 &&
        inner.start.offset > 0 && inner.end.offset > 0) {
        return outer.start.offset <= inner.start.offset &&
               inner.end.offset <= outer.end.offset;
    }

    return ((outer.start.line < inner.start.line) ||
            (outer.start.line == inner.start.line && outer.start.column <= inner.start.column)) &&
           ((inner.end.line < outer.end.line) ||
            (inner.end.line == outer.end.line && inner.end.column <= outer.end.column));
}

static EZrAstNodeType get_class_property_accessor_type(SZrAstNode *classMember) {
    if (classMember == ZR_NULL || classMember->type != ZR_AST_CLASS_PROPERTY ||
        classMember->data.classProperty.modifier == ZR_NULL) {
        return ZR_AST_CLASS_PROPERTY;
    }

    return classMember->data.classProperty.modifier->type;
}

static SZrFileRange get_class_property_selection_range(SZrAstNode *classMember) {
    if (classMember != ZR_NULL && classMember->type == ZR_AST_CLASS_PROPERTY &&
        classMember->data.classProperty.modifier != ZR_NULL) {
        if (classMember->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET) {
            return classMember->data.classProperty.modifier->data.propertyGet.nameLocation;
        }
        if (classMember->data.classProperty.modifier->type == ZR_AST_PROPERTY_SET) {
            return classMember->data.classProperty.modifier->data.propertySet.nameLocation;
        }
    }

    return classMember != ZR_NULL
           ? classMember->location
           : ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                       ZrParser_FilePosition_Create(0, 0, 0),
                                       ZR_NULL);
}

static void add_definition_reference_for_range(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               SZrSymbol *symbol,
                                               SZrFileRange range) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->referenceTracker == ZR_NULL ||
        symbol == ZR_NULL) {
        return;
    }

    ZrLanguageServer_ReferenceTracker_AddReference(state,
                                                   analyzer->referenceTracker,
                                                   symbol,
                                                   range,
                                                   ZR_REFERENCE_DEFINITION);
}

static SZrSymbol *find_matching_class_property_symbol(SZrState *state,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrAstNode *classNode,
                                                      SZrAstNode *classMember,
                                                      SZrString *memberName) {
    SZrArray matches;
    SZrSymbolScope *currentScope;
    EZrAstNodeType accessorType;
    SZrClassProperty *property;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
        classNode == ZR_NULL || classMember == ZR_NULL || memberName == ZR_NULL ||
        classNode->type != ZR_AST_CLASS_DECLARATION || classMember->type != ZR_AST_CLASS_PROPERTY) {
        return ZR_NULL;
    }

    accessorType = get_class_property_accessor_type(classMember);
    if (accessorType != ZR_AST_PROPERTY_GET && accessorType != ZR_AST_PROPERTY_SET) {
        return ZR_NULL;
    }

    property = &classMember->data.classProperty;
    currentScope = ZrLanguageServer_SymbolTable_GetCurrentScope(analyzer->symbolTable);
    ZrCore_Array_Construct(&matches);
    if (!ZrLanguageServer_SymbolTable_LookupAll(state,
                                                analyzer->symbolTable,
                                                memberName,
                                                currentScope,
                                                &matches)) {
        ZrCore_Array_Free(state, &matches);
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < matches.length; index++) {
        SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&matches, index);
        SZrSymbol *symbol;
        SZrClassProperty *existingProperty;
        EZrAstNodeType existingAccessorType;

        if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL) {
            continue;
        }

        symbol = *symbolPtr;
        if (symbol->type != ZR_SYMBOL_PROPERTY ||
            symbol->astNode == ZR_NULL ||
            symbol->astNode->type != ZR_AST_CLASS_PROPERTY ||
            !file_range_contains_range(classNode->location, symbol->location)) {
            continue;
        }

        existingProperty = &symbol->astNode->data.classProperty;
        existingAccessorType = get_class_property_accessor_type(symbol->astNode);
        if (existingProperty->isStatic != property->isStatic ||
            existingAccessorType == accessorType) {
            continue;
        }

        ZrCore_Array_Free(state, &matches);
        return symbol;
    }

    ZrCore_Array_Free(state, &matches);
    return ZR_NULL;
}

static void promote_class_property_symbol_to_getter(SZrSymbol *symbol, SZrAstNode *classMember) {
    if (symbol == ZR_NULL || classMember == ZR_NULL || classMember->type != ZR_AST_CLASS_PROPERTY ||
        get_class_property_accessor_type(classMember) != ZR_AST_PROPERTY_GET) {
        return;
    }

    symbol->location = classMember->location;
    symbol->selectionRange = get_class_property_selection_range(classMember);
    symbol->astNode = classMember;
}

static void register_variable_type_binding_in_env(SZrState *state,
                                                  SZrTypeEnvironment *typeEnv,
                                                  SZrString *name,
                                                  SZrInferredType *typeInfo);

static void collect_function_parameters(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrAstNodeArray *params) {
    if (state == ZR_NULL || analyzer == ZR_NULL || params == ZR_NULL || params->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrParameter *param;
        SZrInferredType *typeInfo;
        SZrSymbol *symbol = ZR_NULL;
        SZrString *name;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        param = &paramNode->data.parameter;
        name = (param->name != ZR_NULL) ? param->name->name : ZR_NULL;
        if (name == ZR_NULL) {
            continue;
        }

        typeInfo = create_type_info_from_type_node(state, analyzer, param->typeInfo);
        ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                 analyzer->symbolTable,
                                                 ZR_SYMBOL_PARAMETER,
                                                 name,
                                                 paramNode->location,
                                                 typeInfo,
                                                 ZR_ACCESS_PRIVATE,
                                                 paramNode,
                                                 &symbol);
        ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                  symbol,
                                                                  ZR_SEMANTIC_SYMBOL_KIND_PARAMETER,
                                                                  typeInfo,
                                                                  ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
        ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
        register_variable_type_binding_in_env(state,
                                              analyzer->compilerState != ZR_NULL ? analyzer->compilerState->typeEnv
                                                                                 : ZR_NULL,
                                              name,
                                              typeInfo);
    }
}

static void register_variable_type_binding_in_env(SZrState *state,
                                                  SZrTypeEnvironment *typeEnv,
                                                  SZrString *name,
                                                  SZrInferredType *typeInfo) {
    if (state == ZR_NULL || typeEnv == ZR_NULL || name == ZR_NULL || typeInfo == ZR_NULL) {
        return;
    }

    ZrParser_TypeEnvironment_RegisterVariable(state, typeEnv, name, typeInfo);
}

static SZrTypeMemberInfo *find_type_member_info_by_name(SZrTypePrototypeInfo *prototype,
                                                        SZrString *memberName) {
    if (prototype == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < prototype->members.length; index++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, index);
        if (memberInfo != ZR_NULL &&
            memberInfo->name != ZR_NULL &&
            ZrCore_String_Equal(memberInfo->name, memberName)) {
            return memberInfo;
        }
    }

    return ZR_NULL;
}

static void report_duplicate_type_name_diagnostic(SZrState *state,
                                                  SZrSemanticAnalyzer *analyzer,
                                                  SZrString *name,
                                                  SZrFileRange location) {
    TZrChar message[ZR_LSP_TEXT_BUFFER_LENGTH];
    const TZrChar *nameText;

    if (state == ZR_NULL || analyzer == ZR_NULL || name == ZR_NULL) {
        return;
    }

    nameText = semantic_string_native(name);
    if (nameText != ZR_NULL) {
        snprintf(message, sizeof(message), "Type name '%s' is already declared in this context", nameText);
    } else {
        snprintf(message, sizeof(message), "Type name is already declared in this context");
    }

    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    message,
                                                    "duplicate_type");
}

static TZrBool register_type_name_binding_in_env(SZrState *state,
                                                 SZrSemanticAnalyzer *analyzer,
                                                 SZrString *name,
                                                 SZrFileRange location) {
    SZrTypeEnvironment *typeEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    typeEnv = analyzer->compilerState->typeEnv;
    if (typeEnv == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrParser_TypeEnvironment_LookupType(typeEnv, name)) {
        report_duplicate_type_name_diagnostic(state, analyzer, name, location);
        return ZR_FALSE;
    }

    if (!ZrParser_TypeEnvironment_RegisterType(state, typeEnv, name)) {
        report_duplicate_type_name_diagnostic(state, analyzer, name, location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static void register_imported_destructured_type_aliases(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrVariableDeclaration *varDecl) {
    SZrInferredType importedModuleType;
    SZrString *moduleName;
    SZrDestructuringObject *destructuring;
    SZrTypePrototypeInfo *modulePrototype;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || varDecl == ZR_NULL ||
        varDecl->pattern == ZR_NULL || varDecl->pattern->type != ZR_AST_DESTRUCTURING_OBJECT ||
        varDecl->value == ZR_NULL || varDecl->value->type != ZR_AST_IMPORT_EXPRESSION ||
        varDecl->value->data.importExpression.modulePath == ZR_NULL ||
        varDecl->value->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL ||
        varDecl->value->data.importExpression.modulePath->data.stringLiteral.value == ZR_NULL) {
        return;
    }

    moduleName = varDecl->value->data.importExpression.modulePath->data.stringLiteral.value;
    ZrParser_InferredType_Init(state, &importedModuleType, ZR_VALUE_TYPE_OBJECT);
    if (moduleName == ZR_NULL ||
        !ZrParser_ExpressionType_Infer(analyzer->compilerState, varDecl->value, &importedModuleType)) {
        ZrParser_InferredType_Free(state, &importedModuleType);
        ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state,
                                                                         analyzer,
                                                                         varDecl->pattern != ZR_NULL
                                                                                 ? varDecl->pattern->location
                                                                                 : ZrParser_FileRange_Create(
                                                                                           ZrParser_FilePosition_Create(0, 0, 0),
                                                                                           ZrParser_FilePosition_Create(0, 0, 0),
                                                                                           ZR_NULL));
        return;
    }
    ZrParser_InferredType_Free(state, &importedModuleType);

    modulePrototype = find_compiler_type_prototype_inference(analyzer->compilerState, moduleName);
    if (modulePrototype == ZR_NULL || modulePrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        return;
    }

    destructuring = &varDecl->pattern->data.destructuringObject;
    if (destructuring->keys == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < destructuring->keys->count; index++) {
        SZrAstNode *keyNode = destructuring->keys->nodes[index];
        SZrString *keyName;
        SZrTypeMemberInfo *memberInfo;

        if (keyNode == ZR_NULL || keyNode->type != ZR_AST_IDENTIFIER_LITERAL ||
            keyNode->data.identifier.name == ZR_NULL) {
            continue;
        }

        keyName = keyNode->data.identifier.name;
        memberInfo = find_type_member_info_by_name(modulePrototype, keyName);
        if (memberInfo == ZR_NULL || memberInfo->fieldTypeName == ZR_NULL ||
            !ZrCore_String_Equal(memberInfo->fieldTypeName, keyName) ||
            find_compiler_type_prototype_inference(analyzer->compilerState, keyName) == ZR_NULL) {
            continue;
        }

        register_type_name_binding_in_env(state, analyzer, keyName, keyNode->location);
    }
}

static SZrTypeEnvironment *push_runtime_type_binding_scope(SZrState *state,
                                                           SZrSemanticAnalyzer *analyzer) {
    SZrTypeEnvironment *savedEnv;
    SZrTypeEnvironment *newEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return ZR_NULL;
    }

    savedEnv = analyzer->compilerState->typeEnv;
    newEnv = ZrParser_TypeEnvironment_New(state);
    if (newEnv == ZR_NULL) {
        return savedEnv;
    }

    newEnv->parent = savedEnv;
    newEnv->semanticContext = savedEnv != ZR_NULL ? savedEnv->semanticContext : analyzer->compilerState->semanticContext;
    analyzer->compilerState->typeEnv = newEnv;
    return savedEnv;
}

static void pop_runtime_type_binding_scope(SZrState *state,
                                           SZrSemanticAnalyzer *analyzer,
                                           SZrTypeEnvironment *savedEnv) {
    SZrTypeEnvironment *currentEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return;
    }

    currentEnv = analyzer->compilerState->typeEnv;
    if (currentEnv == savedEnv) {
        return;
    }

    analyzer->compilerState->typeEnv = savedEnv;
    if (currentEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(state, currentEnv);
    }
}

static SZrInferredType *create_named_object_type_info(SZrState *state,
                                                      SZrString *typeName) {
    SZrInferredType *typeInfo;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    typeInfo = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
    if (typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(state, typeInfo, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
    } else {
        ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
    }

    return typeInfo;
}

static void register_enum_member_symbol(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrString *enumTypeName,
                                        SZrAstNode *memberNode) {
    SZrInferredType *typeInfo;
    SZrSymbol *symbol = ZR_NULL;
    SZrString *memberName;

    if (state == ZR_NULL || analyzer == ZR_NULL || enumTypeName == ZR_NULL || memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_ENUM_MEMBER || memberNode->data.enumMember.name == ZR_NULL) {
        return;
    }

    memberName = memberNode->data.enumMember.name->name;
    if (memberName == ZR_NULL) {
        return;
    }

    typeInfo = create_named_object_type_info(state, enumTypeName);
    ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                             analyzer->symbolTable,
                                             ZR_SYMBOL_ENUM_MEMBER,
                                             memberName,
                                             memberNode->location,
                                             typeInfo,
                                             ZR_ACCESS_PUBLIC,
                                             memberNode,
                                             &symbol);
    if (symbol != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                  symbol,
                                                                  ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                                                                  typeInfo,
                                                                  ZR_SEMANTIC_TYPE_KIND_VALUE);
        ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
    }
}

static SZrFileRange compute_extern_callable_name_range(SZrAstNode *node, SZrString *name) {
    const TZrChar *nameText = semantic_string_native(name);
    TZrSize nameLength = nameText != ZR_NULL ? strlen(nameText) : 0;
    SZrFileRange range;

    if (node == ZR_NULL || nameLength == 0) {
        return node != ZR_NULL
                   ? node->location
                   : ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                               ZrParser_FilePosition_Create(0, 0, 0),
                                               ZR_NULL);
    }

    range = node->location;
    range.end = range.start;

    if (node->type == ZR_AST_EXTERN_FUNCTION_DECLARATION) {
        if (range.start.offset > nameLength + 1) {
            range.start.offset -= nameLength + 1;
        }
        if (range.start.column > (TZrInt32)(nameLength + 1)) {
            range.start.column -= (TZrInt32)(nameLength + 1);
        }
    }

    if (range.start.offset > 0) {
        range.end.offset = range.start.offset + nameLength;
    }
    if (range.start.line > 0) {
        range.end.line = range.start.line;
    }
    if (range.start.column > 0) {
        range.end.column = range.start.column + (TZrInt32)nameLength;
    }

    return range;
}

static SZrString *get_inherited_type_name(SZrAstNode *classNode) {
    SZrAstNode *inheritNode;

    if (classNode == ZR_NULL ||
        classNode->type != ZR_AST_CLASS_DECLARATION ||
        classNode->data.classDeclaration.inherits == ZR_NULL ||
        classNode->data.classDeclaration.inherits->count == 0) {
        return ZR_NULL;
    }

    inheritNode = classNode->data.classDeclaration.inherits->nodes[0];
    if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE || inheritNode->data.type.name == ZR_NULL) {
        return ZR_NULL;
    }

    if (inheritNode->data.type.name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return inheritNode->data.type.name->data.identifier.name;
    }

    if (inheritNode->data.type.name->type == ZR_AST_GENERIC_TYPE &&
        inheritNode->data.type.name->data.genericType.name != ZR_NULL) {
        return inheritNode->data.type.name->data.genericType.name->name;
    }

    return ZR_NULL;
}

static void register_implicit_runtime_symbol(SZrState *state,
                                             SZrSemanticAnalyzer *analyzer,
                                             SZrAstNode *scopeNode,
                                             const TZrChar *literalName,
                                             SZrString *typeName) {
    SZrString *name;
    SZrInferredType *typeInfo;
    SZrSymbol *symbol = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || scopeNode == ZR_NULL || literalName == ZR_NULL) {
        return;
    }

    name = ZrCore_String_Create(state, (TZrNativeString)literalName, strlen(literalName));
    if (name == ZR_NULL) {
        return;
    }

    typeInfo = create_named_object_type_info(state, typeName);
    ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                             analyzer->symbolTable,
                                             ZR_SYMBOL_VARIABLE,
                                             name,
                                             scopeNode->location,
                                             typeInfo,
                                             ZR_ACCESS_PRIVATE,
                                             ZR_NULL,
                                             &symbol);
    if (symbol != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                  symbol,
                                                                  ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                                                                  typeInfo,
                                                                  ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
    }
    register_variable_type_binding_in_env(state,
                                          analyzer->compilerState != ZR_NULL ? analyzer->compilerState->typeEnv : ZR_NULL,
                                          name,
                                          typeInfo);
}

static void collect_single_parameter_symbol(SZrState *state,
                                            SZrSemanticAnalyzer *analyzer,
                                            SZrString *name,
                                            SZrType *typeInfoNode,
                                            SZrAstNode *ownerNode) {
    SZrInferredType *typeInfo;
    SZrSymbol *symbol = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || name == ZR_NULL || ownerNode == ZR_NULL) {
        return;
    }

    typeInfo = create_type_info_from_type_node(state, analyzer, typeInfoNode);
    ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                             analyzer->symbolTable,
                                             ZR_SYMBOL_PARAMETER,
                                             name,
                                             ownerNode->location,
                                             typeInfo,
                                             ZR_ACCESS_PRIVATE,
                                             ownerNode,
                                             &symbol);
    if (symbol != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                  symbol,
                                                                  ZR_SEMANTIC_SYMBOL_KIND_PARAMETER,
                                                                  typeInfo,
                                                                  ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
        ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
    }
    register_variable_type_binding_in_env(state,
                                          analyzer->compilerState != ZR_NULL ? analyzer->compilerState->typeEnv : ZR_NULL,
                                          name,
                                          typeInfo);
}

static void collect_function_like_scope(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrAstNode *scopeNode,
                                        SZrAstNodeArray *params,
                                        SZrAstNode *body,
                                        SZrAstNode *ownerTypeNode,
                                        TZrBool isStatic,
                                        TZrBool isClassScope,
                                        TZrBool isStructScope,
                                        SZrString *manualParamName,
                                        SZrType *manualParamType) {
    SZrTypeEnvironment *savedTypeEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || scopeNode == ZR_NULL) {
        return;
    }

    savedTypeEnv = push_runtime_type_binding_scope(state, analyzer);
    ZrLanguageServer_SymbolTable_EnterScope(state,
                                            analyzer->symbolTable,
                                            scopeNode->location,
                                            ZR_TRUE,
                                            isClassScope,
                                            isStructScope);

    if (!isStatic && ownerTypeNode != ZR_NULL) {
        SZrString *typeName = ZR_NULL;

        if (ownerTypeNode->type == ZR_AST_CLASS_DECLARATION &&
            ownerTypeNode->data.classDeclaration.name != ZR_NULL) {
            SZrString *superTypeName;
            typeName = ownerTypeNode->data.classDeclaration.name->name;
            register_implicit_runtime_symbol(state, analyzer, scopeNode, "this", typeName);
            superTypeName = get_inherited_type_name(ownerTypeNode);
            if (superTypeName != ZR_NULL) {
                register_implicit_runtime_symbol(state, analyzer, scopeNode, "super", superTypeName);
            }
        } else if (ownerTypeNode->type == ZR_AST_STRUCT_DECLARATION &&
                   ownerTypeNode->data.structDeclaration.name != ZR_NULL) {
            typeName = ownerTypeNode->data.structDeclaration.name->name;
            register_implicit_runtime_symbol(state, analyzer, scopeNode, "this", typeName);
        }
    }

    collect_function_parameters(state, analyzer, params);
    if (manualParamName != ZR_NULL) {
        collect_single_parameter_symbol(state, analyzer, manualParamName, manualParamType, scopeNode);
    }
    if (body != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, body);
    }

    ZrLanguageServer_SymbolTable_ExitScope(analyzer->symbolTable);
    pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
}

static void collect_symbols_from_node_array(SZrState *state,
                                            SZrSemanticAnalyzer *analyzer,
                                            SZrAstNodeArray *nodes) {
    if (state == ZR_NULL || analyzer == ZR_NULL || nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (nodes->nodes[index] != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, nodes->nodes[index]);
        }
    }
}

void ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            // 脚本节点：遍历 statements 数组
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    if (script->statements->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            // 处理 moduleName（如果有）
            if (script->moduleName != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, script->moduleName);
            }
            return; // 已经递归处理，不需要继续
        }
        
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            SZrTypeEnvironment *savedTypeEnv = push_runtime_type_binding_scope(state, analyzer);
            ZrLanguageServer_SymbolTable_EnterScope(state,
                                                    analyzer->symbolTable,
                                                    node->location,
                                                    ZR_FALSE,
                                                    ZR_FALSE,
                                                    ZR_FALSE);
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, block->body->nodes[i]);
                    }
                }
            }
            ZrLanguageServer_SymbolTable_ExitScope(analyzer->symbolTable);
            pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            return;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            // pattern 可能是 Identifier, DestructuringPattern, 或 DestructuringArrayPattern
            register_imported_destructured_type_aliases(state, analyzer, varDecl);
            SZrString *name = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, varDecl->pattern);
            if (name != ZR_NULL) {
                SZrInferredType *typeInfo = create_type_info_for_variable(state, analyzer, varDecl);
                
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_VARIABLE, name,
                                         node->location, typeInfo,
                                         varDecl->accessModifier, node,
                                         &symbol);
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                                          typeInfo,
                                          ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
                register_variable_type_binding_in_env(state,
                                                      analyzer->compilerState != ZR_NULL
                                                          ? analyzer->compilerState->typeEnv
                                                          : ZR_NULL,
                                                      name,
                                                      typeInfo);
            }
            if (varDecl->value != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, varDecl->value);
            }
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            if (usingStmt->body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, usingStmt->body);
            }
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = funcDecl->name != ZR_NULL ? funcDecl->name->name : ZR_NULL;
            if (name != ZR_NULL) {
                SZrInferredType *returnType = create_type_info_from_type_node(state, analyzer, funcDecl->returnType);
                
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_FUNCTION, name,
                                         node->location, returnType,
                                         ZR_ACCESS_PUBLIC, node,
                                         &symbol);
                register_function_type_binding(state, analyzer, funcDecl);
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                          returnType,
                                          ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);

                collect_function_like_scope(state,
                                            analyzer,
                                            node,
                                            funcDecl->params,
                                            funcDecl->body,
                                            ZR_NULL,
                                            ZR_TRUE,
                                            ZR_FALSE,
                                            ZR_FALSE,
                                            ZR_NULL,
                                            ZR_NULL);
            }
            return;
        }

        case ZR_AST_TEST_DECLARATION: {
            SZrTestDeclaration *testDecl = &node->data.testDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = testDecl->name != ZR_NULL ? testDecl->name->name : ZR_NULL;

            if (name != ZR_NULL) {
                ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                         analyzer->symbolTable,
                                                         ZR_SYMBOL_FUNCTION,
                                                         name,
                                                         node->location,
                                                         ZR_NULL,
                                                         ZR_ACCESS_PRIVATE,
                                                         node,
                                                         &symbol);
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                          symbol,
                                                                          ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                                                          ZR_NULL,
                                                                          ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
            }

            collect_function_like_scope(state,
                                        analyzer,
                                        node,
                                        testDecl->params,
                                        testDecl->body,
                                        ZR_NULL,
                                        ZR_TRUE,
                                        ZR_FALSE,
                                        ZR_FALSE,
                                        ZR_NULL,
                                        ZR_NULL);
            return;
        }

        case ZR_AST_COMPILE_TIME_DECLARATION: {
            SZrCompileTimeDeclaration *compileTimeDecl = &node->data.compileTimeDeclaration;
            SZrAstNode *wrappedNode = compileTimeDecl->declaration;

            if (wrappedNode == ZR_NULL) {
                return;
            }

            if (compileTimeDecl->declarationType == ZR_COMPILE_TIME_FUNCTION &&
                wrappedNode->type == ZR_AST_FUNCTION_DECLARATION) {
                register_function_type_binding_in_env(state,
                                                      analyzer,
                                                      analyzer->compilerState != ZR_NULL
                                                          ? analyzer->compilerState->typeEnv
                                                          : ZR_NULL,
                                                      &wrappedNode->data.functionDeclaration);
                register_function_type_binding_in_env(state,
                                                      analyzer,
                                                      analyzer->compilerState != ZR_NULL
                                                          ? analyzer->compilerState->compileTimeTypeEnv
                                                          : ZR_NULL,
                                                      &wrappedNode->data.functionDeclaration);
            } else if (compileTimeDecl->declarationType == ZR_COMPILE_TIME_VARIABLE &&
                       wrappedNode->type == ZR_AST_VARIABLE_DECLARATION &&
                       wrappedNode->data.variableDeclaration.pattern != ZR_NULL) {
                SZrString *name =
                    ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state,
                                                                            wrappedNode->data.variableDeclaration.pattern);
                SZrInferredType *typeInfo = create_type_info_for_variable(state,
                                                                          analyzer,
                                                                          &wrappedNode->data.variableDeclaration);
                register_variable_type_binding_in_env(state,
                                                      analyzer->compilerState != ZR_NULL
                                                          ? analyzer->compilerState->typeEnv
                                                          : ZR_NULL,
                                                      name,
                                                      typeInfo);
                register_variable_type_binding_in_env(state,
                                                      analyzer->compilerState != ZR_NULL
                                                          ? analyzer->compilerState->compileTimeTypeEnv
                                                          : ZR_NULL,
                                                      name,
                                                      typeInfo);
                if (typeInfo != ZR_NULL) {
                    ZrParser_InferredType_Free(state, typeInfo);
                    ZrCore_Memory_RawFree(state->global, typeInfo, sizeof(SZrInferredType));
                }
            }

            if (compileTimeDecl->declarationType == ZR_COMPILE_TIME_STATEMENT &&
                wrappedNode->type == ZR_AST_BLOCK &&
                wrappedNode->data.block.body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, wrappedNode);
                return;
            }

            ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, wrappedNode);
            return;
        }

        case ZR_AST_EXTERN_BLOCK:
            collect_symbols_from_node_array(state, analyzer, node->data.externBlock.declarations);
            return;

        case ZR_AST_EXTERN_FUNCTION_DECLARATION: {
            SZrExternFunctionDeclaration *funcDecl = &node->data.externFunctionDeclaration;
            SZrString *name = funcDecl->name != ZR_NULL ? funcDecl->name->name : ZR_NULL;
            SZrSymbol *symbol = ZR_NULL;
            SZrInferredType *returnType;

            if (name == ZR_NULL) {
                return;
            }

            returnType = create_type_info_from_type_node(state, analyzer, funcDecl->returnType);
            register_extern_function_type_binding_in_env(state,
                                                         analyzer,
                                                         analyzer->compilerState != ZR_NULL
                                                             ? analyzer->compilerState->typeEnv
                                                             : ZR_NULL,
                                                         node,
                                                         funcDecl);
            register_extern_function_type_binding_in_env(state,
                                                         analyzer,
                                                         analyzer->compilerState != ZR_NULL
                                                             ? analyzer->compilerState->compileTimeTypeEnv
                                                             : ZR_NULL,
                                                         node,
                                                         funcDecl);
            ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                     analyzer->symbolTable,
                                                     ZR_SYMBOL_FUNCTION,
                                                     name,
                                                     node->location,
                                                     returnType,
                                                     ZR_ACCESS_PUBLIC,
                                                     node,
                                                     &symbol);
            if (symbol != ZR_NULL) {
                symbol->selectionRange = compute_extern_callable_name_range(node, name);
            }
            ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                      symbol,
                                                                      ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                                                      returnType,
                                                                      ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
            ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);

            collect_function_like_scope(state,
                                        analyzer,
                                        node,
                                        funcDecl->params,
                                        ZR_NULL,
                                        ZR_NULL,
                                        ZR_TRUE,
                                        ZR_FALSE,
                                        ZR_FALSE,
                                        ZR_NULL,
                                        ZR_NULL);
            return;
        }

        case ZR_AST_EXTERN_DELEGATE_DECLARATION: {
            SZrExternDelegateDeclaration *delegateDecl = &node->data.externDelegateDeclaration;
            SZrString *name = delegateDecl->name != ZR_NULL ? delegateDecl->name->name : ZR_NULL;
            SZrSymbol *symbol = ZR_NULL;
            SZrInferredType *delegateTypeInfo;

            if (name == ZR_NULL) {
                return;
            }

            delegateTypeInfo = create_named_object_type_info(state, name);
            ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                     analyzer->symbolTable,
                                                     ZR_SYMBOL_FUNCTION,
                                                     name,
                                                     node->location,
                                                     delegateTypeInfo,
                                                     ZR_ACCESS_PUBLIC,
                                                     node,
                                                     &symbol);
            if (symbol != ZR_NULL) {
                symbol->selectionRange = compute_extern_callable_name_range(node, name);
            }
            ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                      symbol,
                                                                      ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                                                      delegateTypeInfo,
                                                                      ZR_SEMANTIC_TYPE_KIND_REFERENCE);
            ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);

            collect_function_like_scope(state,
                                        analyzer,
                                        node,
                                        delegateDecl->params,
                                        ZR_NULL,
                                        ZR_NULL,
                                        ZR_TRUE,
                                        ZR_FALSE,
                                        ZR_FALSE,
                                        ZR_NULL,
                                        ZR_NULL);
            return;
        }

        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = classDecl->name != ZR_NULL ? classDecl->name->name : ZR_NULL;
            TZrLifetimeRegionId ownerRegionId = 0;
            if (name != ZR_NULL) {
                if (!register_type_name_binding_in_env(state, analyzer, name, node->location)) {
                    return;
                }
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_CLASS, name,
                                         node->location, ZR_NULL,
                                         classDecl->accessModifier, node,
                                         &symbol);
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                          ZR_NULL,
                                          ZR_SEMANTIC_TYPE_KIND_REFERENCE);
                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
                
                // 检查类实现的接口，验证 const 字段匹配
                if (classDecl->inherits != ZR_NULL && classDecl->inherits->count > 0) {
                    for (TZrSize i = 0; i < classDecl->inherits->count; i++) {
                        SZrAstNode *inheritNode = classDecl->inherits->nodes[i];
                        if (inheritNode != ZR_NULL && inheritNode->type == ZR_AST_TYPE) {
                            // 查找接口定义
                            SZrType *inheritType = &inheritNode->data.type;
                            if (inheritType->name != ZR_NULL && 
                                inheritType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
                                SZrString *interfaceName = inheritType->name->data.identifier.name;
                                if (interfaceName != ZR_NULL) {
                                    SZrSymbol *interfaceSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, interfaceName, ZR_NULL);
                                    if (interfaceSymbol != ZR_NULL && 
                                        interfaceSymbol->type == ZR_SYMBOL_INTERFACE &&
                                        interfaceSymbol->astNode != ZR_NULL &&
                                        interfaceSymbol->astNode->type == ZR_AST_INTERFACE_DECLARATION) {
                                        // 检查接口中的 const 字段是否在类中也标记为 const
                                        SZrInterfaceDeclaration *interfaceDecl = &interfaceSymbol->astNode->data.interfaceDeclaration;
                                        if (interfaceDecl->members != ZR_NULL) {
                                            for (TZrSize j = 0; j < interfaceDecl->members->count; j++) {
                                                SZrAstNode *interfaceMember = interfaceDecl->members->nodes[j];
                                                if (interfaceMember != ZR_NULL && 
                                                    interfaceMember->type == ZR_AST_INTERFACE_FIELD_DECLARATION) {
                                                    SZrInterfaceFieldDeclaration *interfaceField = &interfaceMember->data.interfaceFieldDeclaration;
                                                    if (interfaceField->isConst && interfaceField->name != ZR_NULL) {
                                                        SZrString *fieldName = interfaceField->name->name;
                                                        // 在类中查找对应的字段
                                                        if (classDecl->members != ZR_NULL) {
                                                            for (TZrSize k = 0; k < classDecl->members->count; k++) {
                                                                SZrAstNode *classMember = classDecl->members->nodes[k];
                                                                if (classMember != ZR_NULL && 
                                                                    classMember->type == ZR_AST_CLASS_FIELD) {
                                                                    SZrClassField *classField = &classMember->data.classField;
                                                                    if (classField->name != ZR_NULL && 
                                                                        ZrCore_String_Equal(classField->name->name, fieldName)) {
                                                                        // 检查类字段是否也是 const
                                                                        if (!classField->isConst) {
                                                                            TZrChar errorMsg[ZR_LSP_TEXT_BUFFER_LENGTH];
                                                                            TZrNativeString fieldNameStr = ZrCore_String_GetNativeStringShort(fieldName);
                                                                            if (fieldNameStr != ZR_NULL) {
                                                                                snprintf(errorMsg, sizeof(errorMsg), 
                                                                                        "Interface field '%s' is const, but implementation field is not const", 
                                                                                        fieldNameStr);
                                                                            } else {
                                                                                snprintf(errorMsg, sizeof(errorMsg), 
                                                                                        "Interface field is const, but implementation field is not const");
                                                                            }
                                                                            ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                                                                            ZR_DIAGNOSTIC_ERROR,
                                                                                                            classMember->location,
                                                                                                            errorMsg,
                                            "const_interface_mismatch");
                                                                        }
                                                                        break;
                                                                    }
                                                                }
                                                            }
                                                            // TODO: 如果字段未找到，也应该报告错误（字段缺失）
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (classDecl->members != ZR_NULL) {
                ZrLanguageServer_SymbolTable_EnterScope(state,
                                                        analyzer->symbolTable,
                                                        node->location,
                                                        ZR_FALSE,
                                                        ZR_TRUE,
                                                        ZR_FALSE);
                ownerRegionId = analyzer->semanticContext != ZR_NULL
                                    ? ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext)
                                    : 0;
                for (TZrSize memberIndex = 0; memberIndex < classDecl->members->count; memberIndex++) {
                    SZrAstNode *classMember = classDecl->members->nodes[memberIndex];
                    if (classMember == ZR_NULL) {
                        continue;
                    }

                    if (classMember->type == ZR_AST_CLASS_FIELD) {
                        ZrLanguageServer_SemanticAnalyzer_RegisterFieldSymbolFromAst(state,
                                                       analyzer,
                                                       classMember,
                                                       ownerRegionId,
                                                       ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD,
                                                       (TZrInt32)memberIndex);
                    } else if (classMember->type == ZR_AST_CLASS_METHOD) {
                        SZrClassMethod *method = &classMember->data.classMethod;
                        SZrString *memberName = method->name != ZR_NULL ? method->name->name : ZR_NULL;
                        SZrSymbol *memberSymbol = ZR_NULL;
                        SZrInferredType *returnType = create_type_info_from_type_node(state,
                                                                                      analyzer,
                                                                                      method->returnType);
                        if (memberName != ZR_NULL) {
                            ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                                     analyzer->symbolTable,
                                                                     ZR_SYMBOL_METHOD,
                                                                     memberName,
                                                                     classMember->location,
                                                                     returnType,
                                                                     method->access,
                                                                     classMember,
                                                                     &memberSymbol);
                            ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                      memberSymbol,
                                                      ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                                      returnType,
                                                      ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                            ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, memberSymbol);
                        } else if (returnType != ZR_NULL) {
                            ZrParser_InferredType_Free(state, returnType);
                            ZrCore_Memory_RawFree(state->global, returnType, sizeof(SZrInferredType));
                        }
                    } else if (classMember->type == ZR_AST_CLASS_PROPERTY) {
                        SZrClassProperty *property = &classMember->data.classProperty;
                        SZrString *memberName = ZrLanguageServer_SemanticAnalyzer_GetClassPropertySymbolName(classMember);
                        SZrSymbol *memberSymbol = ZR_NULL;
                        if (memberName != ZR_NULL) {
                            memberSymbol = find_matching_class_property_symbol(state,
                                                                              analyzer,
                                                                              node,
                                                                              classMember,
                                                                              memberName);
                            if (memberSymbol == ZR_NULL) {
                                ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                                         analyzer->symbolTable,
                                                                         ZR_SYMBOL_PROPERTY,
                                                                         memberName,
                                                                         classMember->location,
                                                                         ZR_NULL,
                                                                         property->access,
                                                                         classMember,
                                                                         &memberSymbol);
                                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                          memberSymbol,
                                                          ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                                                          ZR_NULL,
                                                          ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state,
                                                                                                   analyzer,
                                                                                                   memberSymbol);
                            } else {
                                promote_class_property_symbol_to_getter(memberSymbol, classMember);
                                add_definition_reference_for_range(state,
                                                                   analyzer,
                                                                   memberSymbol,
                                                                   get_class_property_selection_range(classMember));
                            }
                        }
                    }
                }

                for (TZrSize memberIndex = 0; memberIndex < classDecl->members->count; memberIndex++) {
                    SZrAstNode *classMember = classDecl->members->nodes[memberIndex];
                    if (classMember == ZR_NULL) {
                        continue;
                    }

                    if (classMember->type == ZR_AST_CLASS_METHOD) {
                        collect_function_like_scope(state,
                                                    analyzer,
                                                    classMember,
                                                    classMember->data.classMethod.params,
                                                    classMember->data.classMethod.body,
                                                    node,
                                                    classMember->data.classMethod.isStatic,
                                                    ZR_TRUE,
                                                    ZR_FALSE,
                                                    ZR_NULL,
                                                    ZR_NULL);
                    } else if (classMember->type == ZR_AST_CLASS_META_FUNCTION) {
                        collect_function_like_scope(state,
                                                    analyzer,
                                                    classMember,
                                                    classMember->data.classMetaFunction.params,
                                                    classMember->data.classMetaFunction.body,
                                                    node,
                                                    classMember->data.classMetaFunction.isStatic,
                                                    ZR_TRUE,
                                                    ZR_FALSE,
                                                    ZR_NULL,
                                                    ZR_NULL);
                    } else if (classMember->type == ZR_AST_CLASS_PROPERTY &&
                               classMember->data.classProperty.modifier != ZR_NULL) {
                        SZrAstNode *modifier = classMember->data.classProperty.modifier;
                        if (modifier->type == ZR_AST_PROPERTY_GET) {
                            collect_function_like_scope(state,
                                                        analyzer,
                                                        modifier,
                                                        ZR_NULL,
                                                        modifier->data.propertyGet.body,
                                                        node,
                                                        classMember->data.classProperty.isStatic,
                                                        ZR_TRUE,
                                                        ZR_FALSE,
                                                        ZR_NULL,
                                                        ZR_NULL);
                        } else if (modifier->type == ZR_AST_PROPERTY_SET) {
                            collect_function_like_scope(state,
                                                        analyzer,
                                                        modifier,
                                                        ZR_NULL,
                                                        modifier->data.propertySet.body,
                                                        node,
                                                        classMember->data.classProperty.isStatic,
                                                        ZR_TRUE,
                                                        ZR_FALSE,
                                                        modifier->data.propertySet.param != ZR_NULL
                                                            ? modifier->data.propertySet.param->name
                                                            : ZR_NULL,
                                                        modifier->data.propertySet.targetType);
                        }
                    }
                }

                ZrLanguageServer_SymbolTable_ExitScope(analyzer->symbolTable);
            }
            break;
        }
        
        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = structDecl->name != ZR_NULL ? structDecl->name->name : ZR_NULL;
            TZrLifetimeRegionId ownerRegionId = 0;
            if (name != ZR_NULL) {
                if (!register_type_name_binding_in_env(state, analyzer, name, node->location)) {
                    return;
                }
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_STRUCT, name,
                                         node->location, ZR_NULL,
                                         structDecl->accessModifier, node,
                                         &symbol);
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                          ZR_NULL,
                                          ZR_SEMANTIC_TYPE_KIND_VALUE);
                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
            }

            if (structDecl->members != ZR_NULL) {
                ZrLanguageServer_SymbolTable_EnterScope(state,
                                                        analyzer->symbolTable,
                                                        node->location,
                                                        ZR_FALSE,
                                                        ZR_FALSE,
                                                        ZR_TRUE);
                ownerRegionId = analyzer->semanticContext != ZR_NULL
                                    ? ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext)
                                    : 0;
                for (TZrSize memberIndex = 0; memberIndex < structDecl->members->count; memberIndex++) {
                    SZrAstNode *structMember = structDecl->members->nodes[memberIndex];
                    if (structMember == ZR_NULL) {
                        continue;
                    }

                    if (structMember->type == ZR_AST_STRUCT_FIELD) {
                        ZrLanguageServer_SemanticAnalyzer_RegisterFieldSymbolFromAst(state,
                                                       analyzer,
                                                       structMember,
                                                       ownerRegionId,
                                                       ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD,
                                                       (TZrInt32)memberIndex);
                    } else if (structMember->type == ZR_AST_STRUCT_METHOD) {
                        SZrStructMethod *method = &structMember->data.structMethod;
                        SZrString *memberName = method->name != ZR_NULL ? method->name->name : ZR_NULL;
                        SZrSymbol *memberSymbol = ZR_NULL;
                        SZrInferredType *returnType = create_type_info_from_type_node(state,
                                                                                      analyzer,
                                                                                      method->returnType);

                        if (memberName != ZR_NULL) {
                            ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                                     analyzer->symbolTable,
                                                                     ZR_SYMBOL_METHOD,
                                                                     memberName,
                                                                     structMember->location,
                                                                     returnType,
                                                                     method->access,
                                                                     structMember,
                                                                     &memberSymbol);
                            ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                                      memberSymbol,
                                                                                      ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                                                                      returnType,
                                                                                      ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                            ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state,
                                                                                               analyzer,
                                                                                               memberSymbol);
                        } else if (returnType != ZR_NULL) {
                            ZrParser_InferredType_Free(state, returnType);
                            ZrCore_Memory_RawFree(state->global, returnType, sizeof(SZrInferredType));
                        }
                    }
                }

                for (TZrSize memberIndex = 0; memberIndex < structDecl->members->count; memberIndex++) {
                    SZrAstNode *structMember = structDecl->members->nodes[memberIndex];
                    if (structMember == ZR_NULL) {
                        continue;
                    }

                    if (structMember->type == ZR_AST_STRUCT_METHOD) {
                        collect_function_like_scope(state,
                                                    analyzer,
                                                    structMember,
                                                    structMember->data.structMethod.params,
                                                    structMember->data.structMethod.body,
                                                    node,
                                                    structMember->data.structMethod.isStatic,
                                                    ZR_FALSE,
                                                    ZR_TRUE,
                                                    ZR_NULL,
                                                    ZR_NULL);
                    } else if (structMember->type == ZR_AST_STRUCT_META_FUNCTION) {
                        collect_function_like_scope(state,
                                                    analyzer,
                                                    structMember,
                                                    structMember->data.structMetaFunction.params,
                                                    structMember->data.structMetaFunction.body,
                                                    node,
                                                    structMember->data.structMetaFunction.isStatic,
                                                    ZR_FALSE,
                                                    ZR_TRUE,
                                                    ZR_NULL,
                                                    ZR_NULL);
                    }
                }

                ZrLanguageServer_SymbolTable_ExitScope(analyzer->symbolTable);
            }
            break;
        }

        case ZR_AST_ENUM_DECLARATION: {
            SZrEnumDeclaration *enumDecl = &node->data.enumDeclaration;
            SZrString *name = enumDecl->name != ZR_NULL ? enumDecl->name->name : ZR_NULL;
            SZrSymbol *symbol = ZR_NULL;

            if (name != ZR_NULL) {
                if (!register_type_name_binding_in_env(state, analyzer, name, node->location)) {
                    return;
                }
                ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                         analyzer->symbolTable,
                                                         ZR_SYMBOL_ENUM,
                                                         name,
                                                         node->location,
                                                         ZR_NULL,
                                                         enumDecl->accessModifier,
                                                         node,
                                                         &symbol);
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                          symbol,
                                                                          ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                                                          ZR_NULL,
                                                                          ZR_SEMANTIC_TYPE_KIND_VALUE);
                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
            }

            if (enumDecl->members != ZR_NULL && name != ZR_NULL) {
                for (TZrSize memberIndex = 0; memberIndex < enumDecl->members->count; memberIndex++) {
                    SZrAstNode *memberNode = enumDecl->members->nodes[memberIndex];
                    if (memberNode == ZR_NULL) {
                        continue;
                    }

                    register_enum_member_symbol(state, analyzer, name, memberNode);
                    if (memberNode->type == ZR_AST_ENUM_MEMBER &&
                        memberNode->data.enumMember.value != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state,
                                                                                analyzer,
                                                                                memberNode->data.enumMember.value);
                    }
                }
            }
            return;
        }

        case ZR_AST_LAMBDA_EXPRESSION:
            collect_function_like_scope(state,
                                        analyzer,
                                        node,
                                        node->data.lambdaExpression.params,
                                        node->data.lambdaExpression.block,
                                        ZR_NULL,
                                        ZR_TRUE,
                                        ZR_FALSE,
                                        ZR_FALSE,
                                        ZR_NULL,
                                        ZR_NULL);
            return;

        case ZR_AST_INTERFACE_DECLARATION: {
            SZrInterfaceDeclaration *interfaceDecl = &node->data.interfaceDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = interfaceDecl->name != ZR_NULL ? interfaceDecl->name->name : ZR_NULL;

            if (name != ZR_NULL) {
                if (!register_type_name_binding_in_env(state, analyzer, name, node->location)) {
                    return;
                }
                ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                         analyzer->symbolTable,
                                                         ZR_SYMBOL_INTERFACE,
                                                         name,
                                                         node->location,
                                                         ZR_NULL,
                                                         interfaceDecl->accessModifier,
                                                         node,
                                                         &symbol);
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                          symbol,
                                                                          ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                                                          ZR_NULL,
                                                                          ZR_SEMANTIC_TYPE_KIND_REFERENCE);
                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
            }
            break;
        }
        
        default:
            // 对于其他节点类型，继续递归处理可能的子节点
            break;
    }
    
    // 递归处理子节点（根据不同节点类型访问不同的子节点数组）
    // 对于已处理的节点类型（如 SCRIPT, BLOCK），已经在 switch 中处理并返回
    // 对于其他节点类型，需要根据具体情况递归处理子节点
    // 例如：函数声明可能有 body（Block），类声明可能有 members 数组等
    // TODO: 由于这些不是顶层声明节点，暂时跳过深度递归，仅处理直接的符号定义
}

// 辅助函数：遍历 AST 收集引用
