//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"

void ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    // 根据节点类型进行类型检查
    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            if (binExpr->left != ZR_NULL && binExpr->right != ZR_NULL) {
                SZrInferredType leftType, rightType;
                TZrBool hasLeftType;
                TZrBool hasRightType;
                ZrParser_InferredType_Init(state, &leftType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Init(state, &rightType, ZR_VALUE_TYPE_OBJECT);
                hasLeftType = ZrParser_ExpressionType_Infer(analyzer->compilerState, binExpr->left, &leftType);
                hasRightType = hasLeftType
                               ? ZrParser_ExpressionType_Infer(analyzer->compilerState, binExpr->right, &rightType)
                               : ZR_FALSE;
                if (hasLeftType && hasRightType) {
                    // 检查类型兼容性（使用类型检查函数）
                    if (!ZrParser_TypeCompatibility_Check(analyzer->compilerState, &rightType, &leftType, node->location)) {
                        const TZrChar *op = binExpr->op.op;
                        TZrChar buffer[256];
                        snprintf(buffer, sizeof(buffer), 
                                "Type mismatch in binary expression: incompatible types for operator '%s'", 
                                op != ZR_NULL ? op : "?");
                        ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                        ZR_DIAGNOSTIC_ERROR,
                                                        node->location,
                                                        buffer,
                                                        "type_mismatch");
                    }
                }
                if (hasRightType) {
                    ZrParser_InferredType_Free(state, &rightType);
                }
                if (hasLeftType) {
                    ZrParser_InferredType_Free(state, &leftType);
                }
            }
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            if (assignExpr->left != ZR_NULL && assignExpr->right != ZR_NULL) {
                SZrInferredType leftType, rightType;
                TZrBool hasLeftType;
                TZrBool hasRightType;
                ZrParser_InferredType_Init(state, &leftType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Init(state, &rightType, ZR_VALUE_TYPE_OBJECT);
                hasLeftType = ZrParser_ExpressionType_Infer(analyzer->compilerState, assignExpr->left, &leftType);
                hasRightType = hasLeftType
                               ? ZrParser_ExpressionType_Infer(analyzer->compilerState, assignExpr->right, &rightType)
                               : ZR_FALSE;
                if (hasLeftType && hasRightType) {
                    // 检查赋值类型兼容性
                    if (!ZrParser_AssignmentCompatibility_Check(analyzer->compilerState, &leftType, &rightType, node->location)) {
                        ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                        ZR_DIAGNOSTIC_ERROR,
                                                        node->location,
                                                        "Type mismatch in assignment: incompatible types",
                                                        "type_mismatch");
                    }
                }
                if (hasRightType) {
                    ZrParser_InferredType_Free(state, &rightType);
                }
                if (hasLeftType) {
                    ZrParser_InferredType_Free(state, &leftType);
                }
                
                // 检查 const 变量赋值限制
                if (assignExpr->left != ZR_NULL && assignExpr->left->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrString *varName = assignExpr->left->data.identifier.name;
                    if (varName != ZR_NULL) {
                        // 查找符号
                        SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, varName, ZR_NULL);
                        if (symbol != ZR_NULL && symbol->isConst) {
                            // 检查是否是 const 变量
                            if (symbol->type == ZR_SYMBOL_VARIABLE) {
                                // const 局部变量：只能在声明时赋值
                                // TODO: 需要检查是否在声明语句中（通过 AST 上下文判断）
                                // 暂时报告错误，后续完善
                                ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                                ZR_DIAGNOSTIC_ERROR,
                                                                node->location,
                                                                "Cannot assign to const variable after declaration",
                                                                "const_assignment");
                            } else if (symbol->type == ZR_SYMBOL_PARAMETER) {
                                // const 函数参数：不能在函数体内修改
                                ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                                ZR_DIAGNOSTIC_ERROR,
                                                                node->location,
                                                                "Cannot assign to const parameter",
                                                                "const_assignment");
                            } else if (symbol->type == ZR_SYMBOL_FIELD) {
                                // const 成员字段：只能在构造函数中赋值
                                // TODO: 需要检查是否在构造函数中（通过检查当前函数是否为 @constructor 元方法）
                                // 暂时报告错误，后续完善
                                ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                                ZR_DIAGNOSTIC_ERROR,
                                                                node->location,
                                                                "Cannot assign to const field outside constructor",
                                                                "const_assignment");
                            }
                        }
                    }
                }
            }
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            ZR_UNUSED_PARAMETER(&node->data.functionCall);
            // 检查函数调用的参数类型
            // TODO: 注意：这里需要查找函数定义并检查参数类型，简化实现暂时跳过
            // 完整实现需要使用 ZrParser_FunctionCallCompatibility_Check
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            if (varDecl->typeInfo != ZR_NULL && varDecl->value != ZR_NULL) {
                // 检查变量初始值的类型是否与声明类型匹配
                SZrInferredType valueType;
                ZrParser_InferredType_Init(state, &valueType, ZR_VALUE_TYPE_OBJECT);
                if (ZrParser_ExpressionType_Infer(analyzer->compilerState, varDecl->value, &valueType)) {
                    // 这里需要将 AST 类型转换为推断类型进行比较
                    // TODO: 简化实现：暂时跳过详细检查
                    // 完整实现需要使用 ZrParser_AstTypeToInferredType_Convert 和 ZrParser_AssignmentCompatibility_Check
                    ZrParser_InferredType_Free(state, &valueType);
                } else {
                    ZrParser_InferredType_Free(state, &valueType);
                }
            }
            break;
        }
        
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStmt = &node->data.returnStatement;
            if (returnStmt->expr != ZR_NULL) {
                // 检查返回类型是否与函数声明匹配
                // TODO: 简化实现：暂时跳过
                // 完整实现需要获取当前函数的返回类型并比较
            }
            break;
        }
        
        default:
            break;
    }
    
    // 递归检查子节点
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    if (script->statements->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, block->body->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, binExpr->left);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, binExpr->right);
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, unaryExpr->argument);
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, assignExpr->left);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, assignExpr->right);
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *funcCall = &node->data.functionCall;
            if (funcCall->args != ZR_NULL && funcCall->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    if (funcCall->args->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcCall->args->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, primaryExpr->property);
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    if (primaryExpr->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, primaryExpr->members->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, varDecl->pattern);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, varDecl->value);
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->resource);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->body);
            break;
        }

        case ZR_AST_TEMPLATE_STRING_LITERAL: {
            SZrTemplateStringLiteral *templateLiteral = &node->data.templateStringLiteral;
            if (templateLiteral->segments != ZR_NULL && templateLiteral->segments->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
                    if (templateLiteral->segments->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, templateLiteral->segments->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_INTERPOLATED_SEGMENT: {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, node->data.interpolatedSegment.expression);
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    if (funcDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->params->nodes[i]);
                    }
                }
            }
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->body);
            break;
        }
        
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->condition);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->thenExpr);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->elseExpr);
            break;
        }
        
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, whileLoop->cond);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, whileLoop->block);
            break;
        }
        
        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->init);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->cond);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->step);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->block);
            break;
        }
        
        default:
            break;
    }
}

// 创建语义分析器
