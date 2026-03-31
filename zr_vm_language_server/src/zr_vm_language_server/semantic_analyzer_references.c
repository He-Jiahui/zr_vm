//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"

void ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    // 检查是否是标识符引用
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *name = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, node);
        if (name != ZR_NULL) {
            SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, name, ZR_NULL);
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
            if (varDecl->value != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, varDecl->value);
            }
            break;
        }

        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
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
                        SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, name, ZR_NULL);
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
                        SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, name, ZR_NULL);
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
            if (classField->init != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, classField->init);
            }
            break;
        }

        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *classMethod = &node->data.classMethod;
            if (classMethod->body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, classMethod->body);
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
            if (node->data.propertyGet.body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, node->data.propertyGet.body);
            }
            break;
        }

        case ZR_AST_PROPERTY_SET: {
            if (node->data.propertySet.body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, node->data.propertySet.body);
            }
            break;
        }
        
        default:
            // TODO: 其他节点类型暂时跳过
            break;
    }
}

// 分析 AST
