//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"

static SZrFileRange semantic_make_identifier_reference_range(SZrString *name, SZrFileRange fallback) {
    const TZrChar *text = semantic_string_native(name);
    TZrSize length;

    if (text == ZR_NULL || *text == '\0') {
        return fallback;
    }

    length = strlen(text);
    if (length == 0) {
        return fallback;
    }

    if (fallback.start.offset > 0) {
        fallback.end.offset = fallback.start.offset + length;
    }
    if (fallback.start.line > 0) {
        fallback.end.line = fallback.start.line;
    }
    if (fallback.start.column > 0) {
        fallback.end.column = fallback.start.column + (TZrInt32)length;
    }

    return fallback;
}

static void semantic_add_type_reference(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrString *name,
                                        SZrFileRange location) {
    SZrSymbol *symbol;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->referenceTracker == ZR_NULL || name == ZR_NULL) {
        return;
    }

    symbol = ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable, name, location);
    if (symbol != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_AddReference(state,
                                                       analyzer->referenceTracker,
                                                       symbol,
                                                       location,
                                                       ZR_REFERENCE_READ);
    }
}

static void semantic_collect_references_from_type_info(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       const SZrType *typeInfo) {
    if (state == ZR_NULL || analyzer == ZR_NULL || typeInfo == ZR_NULL || typeInfo->name == ZR_NULL) {
        return;
    }

    if (typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL &&
        typeInfo->name->data.identifier.name != ZR_NULL) {
        semantic_add_type_reference(state,
                                    analyzer,
                                    typeInfo->name->data.identifier.name,
                                    typeInfo->name->location);
    } else if (typeInfo->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = (SZrGenericType *)&typeInfo->name->data.genericType;

        if (genericType->name != ZR_NULL && genericType->name->name != ZR_NULL) {
            semantic_add_type_reference(state,
                                        analyzer,
                                        genericType->name->name,
                                        semantic_make_identifier_reference_range(genericType->name->name,
                                                                                 typeInfo->name->location));
        }

        if (genericType->params != ZR_NULL && genericType->params->nodes != ZR_NULL) {
            for (TZrSize index = 0; index < genericType->params->count; index++) {
                SZrAstNode *paramNode = genericType->params->nodes[index];
                if (paramNode == ZR_NULL) {
                    continue;
                }

                if (paramNode->type == ZR_AST_TYPE) {
                    semantic_collect_references_from_type_info(state, analyzer, &paramNode->data.type);
                } else {
                    ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, paramNode);
                }
            }
        }
    } else if (typeInfo->name->type == ZR_AST_TUPLE_TYPE) {
        SZrTupleType *tupleType = (SZrTupleType *)&typeInfo->name->data.tupleType;

        if (tupleType->elements != ZR_NULL && tupleType->elements->nodes != ZR_NULL) {
            for (TZrSize index = 0; index < tupleType->elements->count; index++) {
                SZrAstNode *elementNode = tupleType->elements->nodes[index];
                if (elementNode != ZR_NULL && elementNode->type == ZR_AST_TYPE) {
                    semantic_collect_references_from_type_info(state, analyzer, &elementNode->data.type);
                }
            }
        }
    }

    if (typeInfo->subType != ZR_NULL) {
        semantic_collect_references_from_type_info(state, analyzer, typeInfo->subType);
    }

    if (typeInfo->arraySizeExpression != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, typeInfo->arraySizeExpression);
    }
}

void ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    // 检查是否是标识符引用
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *name = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, node);
        if (name != ZR_NULL) {
            SZrSymbol *symbol = ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable,
                                                                              name,
                                                                              node->location);
            if (symbol != ZR_NULL) {
                // 根据上下文判断引用类型
                EZrReferenceType refType = ZR_REFERENCE_READ; // 默认是读引用
                
                // 检查父节点以确定引用类型
                // 注意：这里需要从AST节点中获取父节点信息，但AST节点可能没有父节点指针
                // TODO: 简化实现：根据节点类型推断
                // 如果是赋值表达式的左值，则是写引用
                // 如果是函数调用的callee，则是调用引用
                // 其他情况是读引用
                
                // 由于AST节点没有父节点指针，我们使用简化策略：
                // 在collect_references_from_ast中，我们已经知道当前处理的节点类型
                // 可以通过检查当前处理的节点类型来判断
                // TODO: 这里暂时使用默认的读引用，实际实现需要更复杂的上下文分析
                
                ZrLanguageServer_ReferenceTracker_AddReference(state, analyzer->referenceTracker,
                                               symbol, node->location, refType);
            } else if (ZrLanguageServer_SemanticAnalyzer_IsImplicitRuntimeIdentifier(name)) {
                return;
            }
        }
    }
    
    // 递归处理子节点（根据不同节点类型访问不同的子节点数组）
    // 对于标识符引用，已经处理了引用关系
    // 对于其他节点类型，需要根据具体情况递归处理子节点
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            // 脚本节点：遍历 statements 数组
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    if (script->statements->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            if (script->moduleName != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, script->moduleName);
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            // 块节点：遍历 body 数组
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, block->body->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            // 变量声明：递归处理 value（表达式可能包含引用）
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            if (varDecl->typeInfo != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, varDecl->typeInfo);
            }
            if (varDecl->value != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, varDecl->value);
            }
            break;
        }

        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    if (funcDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, funcDecl->params->nodes[i]);
                    }
                }
            }
            if (funcDecl->returnType != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, funcDecl->returnType);
            }
            if (funcDecl->body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, funcDecl->body);
            }
            break;
        }

        case ZR_AST_TEST_DECLARATION: {
            SZrTestDeclaration *testDecl = &node->data.testDeclaration;
            if (testDecl->body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, testDecl->body);
            }
            break;
        }

        case ZR_AST_COMPILE_TIME_DECLARATION:
            if (node->data.compileTimeDeclaration.declaration != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state,
                                                                           analyzer,
                                                                           node->data.compileTimeDeclaration.declaration);
            }
            break;

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            ZrLanguageServer_SemanticAnalyzer_RecordUsingCleanupStep(analyzer, usingStmt->resource);
            if (usingStmt->resource != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, usingStmt->resource);
            }
            if (usingStmt->body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, usingStmt->body);
            }
            break;
        }

        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStmt = &node->data.returnStatement;
            if (returnStmt->expr != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, returnStmt->expr);
            }
            break;
        }
        
        case ZR_AST_EXPRESSION_STATEMENT: {
            // 表达式语句：递归处理表达式
            SZrExpressionStatement *exprStmt = &node->data.expressionStatement;
            if (exprStmt->expr != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, exprStmt->expr);
            }
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            // 函数调用：递归处理参数
            // 注意：函数调用本身不包含 callee，callee 通过 SZrPrimaryExpression 的 property 和 members 组织
            SZrFunctionCall *funcCall = &node->data.functionCall;
            if (funcCall->args != ZR_NULL && funcCall->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    if (funcCall->args->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, funcCall->args->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            // 主表达式：可能包含 property 和 members（包括函数调用）
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            if (primaryExpr->property != ZR_NULL) {
                // property 可能是标识符（函数名），需要标记为调用引用
                if (primaryExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrString *name = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, primaryExpr->property);
                    if (name != ZR_NULL) {
                        SZrSymbol *symbol = ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable,
                                                                                          name,
                                                                                          primaryExpr->property->location);
                        if (symbol != ZR_NULL && symbol->type == ZR_SYMBOL_FUNCTION) {
                            // 如果后面有函数调用，则是调用引用
                            TZrBool isCall = (primaryExpr->members != ZR_NULL && 
                                           primaryExpr->members->count > 0);
                            EZrReferenceType refType = isCall ? ZR_REFERENCE_CALL : ZR_REFERENCE_READ;
                            ZrLanguageServer_ReferenceTracker_AddReference(state, analyzer->referenceTracker,
                                                           symbol, primaryExpr->property->location, 
                                                           refType);
                        } else {
                            ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, primaryExpr->property);
                        }
                    }
                } else {
                    ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, primaryExpr->property);
                }
            }
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    if (primaryExpr->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, primaryExpr->members->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_MEMBER_EXPRESSION: {
            // 成员表达式：递归处理 property
            SZrMemberExpression *memberExpr = &node->data.memberExpression;
            if (memberExpr->property != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, memberExpr->property);
            }
            break;
        }

        case ZR_AST_CONSTRUCT_EXPRESSION: {
            SZrConstructExpression *constructExpr = &node->data.constructExpression;
            if (constructExpr->target != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, constructExpr->target);
            }
            if (constructExpr->args != ZR_NULL && constructExpr->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < constructExpr->args->count; i++) {
                    if (constructExpr->args->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, constructExpr->args->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_TEMPLATE_STRING_LITERAL: {
            SZrTemplateStringLiteral *templateLiteral = &node->data.templateStringLiteral;
            ZrLanguageServer_SemanticAnalyzer_RecordTemplateStringSegments(analyzer, node);
            if (templateLiteral->segments != ZR_NULL && templateLiteral->segments->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
                    if (templateLiteral->segments->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, templateLiteral->segments->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_INTERPOLATED_SEGMENT: {
            if (node->data.interpolatedSegment.expression != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state,
                                            analyzer,
                                            node->data.interpolatedSegment.expression);
            }
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            // 二元表达式：递归处理左右操作数
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            if (binExpr->left != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, binExpr->left);
            }
            if (binExpr->right != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, binExpr->right);
            }
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            // 一元表达式：递归处理参数
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            if (unaryExpr->argument != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, unaryExpr->argument);
            }
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            // 赋值表达式：递归处理左右操作数
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            if (assignExpr->left != ZR_NULL) {
                // 左值是写引用
                if (assignExpr->left->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrString *name = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, assignExpr->left);
                    if (name != ZR_NULL) {
                        SZrSymbol *symbol = ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable,
                                                                                          name,
                                                                                          assignExpr->left->location);
                        if (symbol != ZR_NULL) {
                            ZrLanguageServer_ReferenceTracker_AddReference(state, analyzer->referenceTracker,
                                                           symbol, assignExpr->left->location, 
                                                           ZR_REFERENCE_WRITE);
                        }
                    }
                } else {
                    ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, assignExpr->left);
                }
            }
            if (assignExpr->right != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, assignExpr->right);
            }
            break;
        }

        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            if (classDecl->inherits != ZR_NULL && classDecl->inherits->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classDecl->inherits->count; i++) {
                    if (classDecl->inherits->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, classDecl->inherits->nodes[i]);
                    }
                }
            }
            if (classDecl->members != ZR_NULL && classDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classDecl->members->count; i++) {
                    if (classDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, classDecl->members->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_CLASS_FIELD: {
            SZrClassField *classField = &node->data.classField;
            if (classField->typeInfo != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, classField->typeInfo);
            }
            if (classField->init != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, classField->init);
            }
            break;
        }

        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *classMethod = &node->data.classMethod;
            if (classMethod->params != ZR_NULL && classMethod->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classMethod->params->count; i++) {
                    if (classMethod->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, classMethod->params->nodes[i]);
                    }
                }
            }
            if (classMethod->returnType != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, classMethod->returnType);
            }
            if (classMethod->body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, classMethod->body);
            }
            break;
        }

        case ZR_AST_CLASS_META_FUNCTION: {
            SZrClassMetaFunction *classMetaFunction = &node->data.classMetaFunction;
            if (classMetaFunction->params != ZR_NULL && classMetaFunction->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classMetaFunction->params->count; i++) {
                    if (classMetaFunction->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state,
                                                                                   analyzer,
                                                                                   classMetaFunction->params->nodes[i]);
                    }
                }
            }
            if (classMetaFunction->superArgs != ZR_NULL && classMetaFunction->superArgs->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classMetaFunction->superArgs->count; i++) {
                    if (classMetaFunction->superArgs->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state,
                                                                                   analyzer,
                                                                                   classMetaFunction->superArgs->nodes[i]);
                    }
                }
            }
            if (classMetaFunction->returnType != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, classMetaFunction->returnType);
            }
            if (classMetaFunction->body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, classMetaFunction->body);
            }
            break;
        }

        case ZR_AST_CLASS_PROPERTY: {
            if (node->data.classProperty.modifier != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, node->data.classProperty.modifier);
            }
            break;
        }

        case ZR_AST_PROPERTY_GET: {
            if (node->data.propertyGet.targetType != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, node->data.propertyGet.targetType);
            }
            if (node->data.propertyGet.body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, node->data.propertyGet.body);
            }
            break;
        }

        case ZR_AST_PROPERTY_SET: {
            if (node->data.propertySet.targetType != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, node->data.propertySet.targetType);
            }
            if (node->data.propertySet.body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, node->data.propertySet.body);
            }
            break;
        }

        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            if (structDecl->inherits != ZR_NULL && structDecl->inherits->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < structDecl->inherits->count; i++) {
                    if (structDecl->inherits->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, structDecl->inherits->nodes[i]);
                    }
                }
            }
            if (structDecl->members != ZR_NULL && structDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < structDecl->members->count; i++) {
                    if (structDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, structDecl->members->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_STRUCT_FIELD: {
            SZrStructField *structField = &node->data.structField;
            if (structField->typeInfo != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, structField->typeInfo);
            }
            if (structField->init != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, structField->init);
            }
            break;
        }

        case ZR_AST_STRUCT_METHOD: {
            SZrStructMethod *structMethod = &node->data.structMethod;
            if (structMethod->params != ZR_NULL && structMethod->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < structMethod->params->count; i++) {
                    if (structMethod->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, structMethod->params->nodes[i]);
                    }
                }
            }
            if (structMethod->returnType != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, structMethod->returnType);
            }
            if (structMethod->body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, structMethod->body);
            }
            break;
        }

        case ZR_AST_STRUCT_META_FUNCTION: {
            SZrStructMetaFunction *structMetaFunction = &node->data.structMetaFunction;
            if (structMetaFunction->params != ZR_NULL && structMetaFunction->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < structMetaFunction->params->count; i++) {
                    if (structMetaFunction->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state,
                                                                                   analyzer,
                                                                                   structMetaFunction->params->nodes[i]);
                    }
                }
            }
            if (structMetaFunction->returnType != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, structMetaFunction->returnType);
            }
            if (structMetaFunction->body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, structMetaFunction->body);
            }
            break;
        }

        case ZR_AST_INTERFACE_DECLARATION: {
            SZrInterfaceDeclaration *interfaceDecl = &node->data.interfaceDeclaration;
            if (interfaceDecl->inherits != ZR_NULL && interfaceDecl->inherits->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < interfaceDecl->inherits->count; i++) {
                    if (interfaceDecl->inherits->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, interfaceDecl->inherits->nodes[i]);
                    }
                }
            }
            if (interfaceDecl->members != ZR_NULL && interfaceDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < interfaceDecl->members->count; i++) {
                    if (interfaceDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, interfaceDecl->members->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_INTERFACE_FIELD_DECLARATION:
            if (node->data.interfaceFieldDeclaration.typeInfo != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, node->data.interfaceFieldDeclaration.typeInfo);
            }
            break;

        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            if (node->data.interfaceMethodSignature.params != ZR_NULL &&
                node->data.interfaceMethodSignature.params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < node->data.interfaceMethodSignature.params->count; i++) {
                    if (node->data.interfaceMethodSignature.params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state,
                                                                                   analyzer,
                                                                                   node->data.interfaceMethodSignature.params->nodes[i]);
                    }
                }
            }
            if (node->data.interfaceMethodSignature.returnType != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, node->data.interfaceMethodSignature.returnType);
            }
            break;

        case ZR_AST_INTERFACE_PROPERTY_SIGNATURE:
            if (node->data.interfacePropertySignature.typeInfo != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, node->data.interfacePropertySignature.typeInfo);
            }
            break;

        case ZR_AST_LAMBDA_EXPRESSION:
            if (node->data.lambdaExpression.params != ZR_NULL && node->data.lambdaExpression.params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < node->data.lambdaExpression.params->count; i++) {
                    if (node->data.lambdaExpression.params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state,
                                                                                   analyzer,
                                                                                   node->data.lambdaExpression.params->nodes[i]);
                    }
                }
            }
            if (node->data.lambdaExpression.block != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state,
                                                                           analyzer,
                                                                           node->data.lambdaExpression.block);
            }
            break;

        case ZR_AST_INTERFACE_META_SIGNATURE:
            if (node->data.interfaceMetaSignature.params != ZR_NULL &&
                node->data.interfaceMetaSignature.params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < node->data.interfaceMetaSignature.params->count; i++) {
                    if (node->data.interfaceMetaSignature.params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state,
                                                                                   analyzer,
                                                                                   node->data.interfaceMetaSignature.params->nodes[i]);
                    }
                }
            }
            if (node->data.interfaceMetaSignature.returnType != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, node->data.interfaceMetaSignature.returnType);
            }
            break;

        case ZR_AST_PARAMETER:
            if (node->data.parameter.typeInfo != ZR_NULL) {
                semantic_collect_references_from_type_info(state, analyzer, node->data.parameter.typeInfo);
            }
            if (node->data.parameter.genericTypeConstraints != ZR_NULL &&
                node->data.parameter.genericTypeConstraints->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < node->data.parameter.genericTypeConstraints->count; i++) {
                    SZrAstNode *constraintNode = node->data.parameter.genericTypeConstraints->nodes[i];
                    if (constraintNode != ZR_NULL && constraintNode->type == ZR_AST_TYPE) {
                        semantic_collect_references_from_type_info(state, analyzer, &constraintNode->data.type);
                    }
                }
            }
            break;

        case ZR_AST_TYPE:
            semantic_collect_references_from_type_info(state, analyzer, &node->data.type);
            break;
        
        default:
            // TODO: 其他节点类型暂时跳过
            break;
    }
}

// 分析 AST
