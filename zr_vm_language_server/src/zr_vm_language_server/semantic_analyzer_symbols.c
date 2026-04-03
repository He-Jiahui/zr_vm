//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"

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

        default:
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

static void register_function_type_binding(SZrState *state,
                                           SZrSemanticAnalyzer *analyzer,
                                           SZrFunctionDeclaration *funcDecl) {
    SZrCompilerState *compilerState;
    SZrInferredType returnType;
    SZrArray paramTypes;

    if (state == ZR_NULL || analyzer == ZR_NULL || funcDecl == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    if (compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL ||
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

    ZrParser_TypeEnvironment_RegisterFunction(state,
                                              compilerState->typeEnv,
                                              funcDecl->name->name,
                                              &returnType,
                                              &paramTypes);

    ZrParser_InferredType_Free(state, &returnType);
    for (TZrSize index = 0; index < paramTypes.length; index++) {
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, index);
        if (paramType != ZR_NULL) {
            ZrParser_InferredType_Free(state, paramType);
        }
    }
    ZrCore_Array_Free(state, &paramTypes);
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
            return;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            // pattern 可能是 Identifier, DestructuringPattern, 或 DestructuringArrayPattern
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

                ZrLanguageServer_SymbolTable_EnterScope(state,
                                                        analyzer->symbolTable,
                                                        node->location,
                                                        ZR_TRUE,
                                                        ZR_FALSE,
                                                        ZR_FALSE);
                collect_function_parameters(state, analyzer, funcDecl->params);
                ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, funcDecl->body);
                ZrLanguageServer_SymbolTable_ExitScope(analyzer->symbolTable);
            }
            return;
        }
        
        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = classDecl->name != ZR_NULL ? classDecl->name->name : ZR_NULL;
            TZrLifetimeRegionId ownerRegionId = 0;
            if (name != ZR_NULL) {
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_CLASS, name,
                                         node->location, ZR_NULL,
                                         classDecl->accessModifier, node,
                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterType(state, analyzer->compilerState->typeEnv, name);
                }
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
                        if (memberName != ZR_NULL) {
                            ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                                     analyzer->symbolTable,
                                                                     ZR_SYMBOL_METHOD,
                                                                     memberName,
                                                                     classMember->location,
                                                                     ZR_NULL,
                                                                     method->access,
                                                                     classMember,
                                                                     &memberSymbol);
                            ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                      memberSymbol,
                                                      ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                                      ZR_NULL,
                                                      ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                            ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, memberSymbol);
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
            }
            break;
        }
        
        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = structDecl->name != ZR_NULL ? structDecl->name->name : ZR_NULL;
            TZrLifetimeRegionId ownerRegionId = 0;
            if (name != ZR_NULL) {
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_STRUCT, name,
                                         node->location, ZR_NULL,
                                         structDecl->accessModifier, node,
                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterType(state, analyzer->compilerState->typeEnv, name);
                }
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                          ZR_NULL,
                                          ZR_SEMANTIC_TYPE_KIND_VALUE);
            }

            if (structDecl->members != ZR_NULL) {
                ownerRegionId = analyzer->semanticContext != ZR_NULL
                                    ? ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext)
                                    : 0;
                for (TZrSize memberIndex = 0; memberIndex < structDecl->members->count; memberIndex++) {
                    SZrAstNode *structMember = structDecl->members->nodes[memberIndex];
                    if (structMember != ZR_NULL && structMember->type == ZR_AST_STRUCT_FIELD) {
                        ZrLanguageServer_SemanticAnalyzer_RegisterFieldSymbolFromAst(state,
                                                       analyzer,
                                                       structMember,
                                                       ownerRegionId,
                                                       ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD,
                                                       (TZrInt32)memberIndex);
                    }
                }
            }
            break;
        }

        case ZR_AST_INTERFACE_DECLARATION: {
            SZrInterfaceDeclaration *interfaceDecl = &node->data.interfaceDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = interfaceDecl->name != ZR_NULL ? interfaceDecl->name->name : ZR_NULL;

            if (name != ZR_NULL) {
                ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                         analyzer->symbolTable,
                                                         ZR_SYMBOL_INTERFACE,
                                                         name,
                                                         node->location,
                                                         ZR_NULL,
                                                         interfaceDecl->accessModifier,
                                                         node,
                                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterType(state, analyzer->compilerState->typeEnv, name);
                }
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
