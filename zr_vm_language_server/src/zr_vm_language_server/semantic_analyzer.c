//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/type_system.h"

#include <string.h>
#include <stdio.h>

// 前向声明编译器状态初始化函数
extern void ZrCompilerStateInit(SZrCompilerState *cs, SZrState *state);
extern void ZrCompilerStateFree(SZrCompilerState *cs);

// 前向声明类型推断函数
extern TBool infer_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);
extern TBool check_type_compatibility(SZrCompilerState *cs, const SZrInferredType *fromType, 
                                       const SZrInferredType *toType, SZrFileRange location);
extern TBool check_assignment_compatibility(SZrCompilerState *cs, const SZrInferredType *leftType, 
                                            const SZrInferredType *rightType, SZrFileRange location);

// 辅助函数：执行类型检查
static void perform_type_checking(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    // 根据节点类型进行类型检查
    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            if (binExpr->left != ZR_NULL && binExpr->right != ZR_NULL) {
                SZrInferredType leftType, rightType;
                if (infer_expression_type(analyzer->compilerState, binExpr->left, &leftType) &&
                    infer_expression_type(analyzer->compilerState, binExpr->right, &rightType)) {
                    // 检查类型兼容性（使用类型检查函数）
                    if (!check_type_compatibility(analyzer->compilerState, &rightType, &leftType, node->location)) {
                        const TChar *op = binExpr->op.op;
                        TChar buffer[256];
                        snprintf(buffer, sizeof(buffer), 
                                "Type mismatch in binary expression: incompatible types for operator '%s'", 
                                op != ZR_NULL ? op : "?");
                        ZrSemanticAnalyzerAddDiagnostic(state, analyzer,
                                                        ZR_DIAGNOSTIC_ERROR,
                                                        node->location,
                                                        buffer,
                                                        "type_mismatch");
                    }
                }
            }
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            if (assignExpr->left != ZR_NULL && assignExpr->right != ZR_NULL) {
                SZrInferredType leftType, rightType;
                if (infer_expression_type(analyzer->compilerState, assignExpr->left, &leftType) &&
                    infer_expression_type(analyzer->compilerState, assignExpr->right, &rightType)) {
                    // 检查赋值类型兼容性
                    if (!check_assignment_compatibility(analyzer->compilerState, &leftType, &rightType, node->location)) {
                        ZrSemanticAnalyzerAddDiagnostic(state, analyzer,
                                                        ZR_DIAGNOSTIC_ERROR,
                                                        node->location,
                                                        "Type mismatch in assignment: incompatible types",
                                                        "type_mismatch");
                    }
                }
            }
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *funcCall = &node->data.functionCall;
            // 检查函数调用的参数类型
            // TODO: 注意：这里需要查找函数定义并检查参数类型，简化实现暂时跳过
            // 完整实现需要使用 check_function_call_compatibility
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            if (varDecl->typeInfo != ZR_NULL && varDecl->value != ZR_NULL) {
                // 检查变量初始值的类型是否与声明类型匹配
                SZrInferredType valueType;
                if (infer_expression_type(analyzer->compilerState, varDecl->value, &valueType)) {
                    // 这里需要将 AST 类型转换为推断类型进行比较
                    // TODO: 简化实现：暂时跳过详细检查
                    // 完整实现需要使用 convert_ast_type_to_inferred_type 和 check_assignment_compatibility
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
                        perform_type_checking(state, analyzer, script->statements->nodes[i]);
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
                        perform_type_checking(state, analyzer, block->body->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            perform_type_checking(state, analyzer, binExpr->left);
            perform_type_checking(state, analyzer, binExpr->right);
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            perform_type_checking(state, analyzer, unaryExpr->argument);
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            perform_type_checking(state, analyzer, assignExpr->left);
            perform_type_checking(state, analyzer, assignExpr->right);
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *funcCall = &node->data.functionCall;
            if (funcCall->args != ZR_NULL && funcCall->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    if (funcCall->args->nodes[i] != ZR_NULL) {
                        perform_type_checking(state, analyzer, funcCall->args->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            perform_type_checking(state, analyzer, primaryExpr->property);
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    if (primaryExpr->members->nodes[i] != ZR_NULL) {
                        perform_type_checking(state, analyzer, primaryExpr->members->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            perform_type_checking(state, analyzer, varDecl->pattern);
            perform_type_checking(state, analyzer, varDecl->value);
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    if (funcDecl->params->nodes[i] != ZR_NULL) {
                        perform_type_checking(state, analyzer, funcDecl->params->nodes[i]);
                    }
                }
            }
            perform_type_checking(state, analyzer, funcDecl->body);
            break;
        }
        
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            perform_type_checking(state, analyzer, ifExpr->condition);
            perform_type_checking(state, analyzer, ifExpr->thenExpr);
            perform_type_checking(state, analyzer, ifExpr->elseExpr);
            break;
        }
        
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            perform_type_checking(state, analyzer, whileLoop->cond);
            perform_type_checking(state, analyzer, whileLoop->block);
            break;
        }
        
        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            perform_type_checking(state, analyzer, forLoop->init);
            perform_type_checking(state, analyzer, forLoop->cond);
            perform_type_checking(state, analyzer, forLoop->step);
            perform_type_checking(state, analyzer, forLoop->block);
            break;
        }
        
        default:
            break;
    }
}

// 创建语义分析器
SZrSemanticAnalyzer *ZrSemanticAnalyzerNew(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)ZrMemoryRawMalloc(state->global, sizeof(SZrSemanticAnalyzer));
    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }
    
    analyzer->state = state;
    analyzer->symbolTable = ZrSymbolTableNew(state);
    analyzer->referenceTracker = ZR_NULL;
    analyzer->ast = ZR_NULL;
    analyzer->cache = ZR_NULL;
    analyzer->enableCache = ZR_TRUE; // 默认启用缓存
    analyzer->compilerState = ZR_NULL; // 延迟创建
    
    if (analyzer->symbolTable == ZR_NULL) {
        ZrMemoryRawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    analyzer->referenceTracker = ZrReferenceTrackerNew(state, analyzer->symbolTable);
    if (analyzer->referenceTracker == ZR_NULL) {
        ZrSymbolTableFree(state, analyzer->symbolTable);
        ZrMemoryRawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    ZrArrayInit(state, &analyzer->diagnostics, sizeof(SZrDiagnostic *), 8);
    
    // 创建缓存
    analyzer->cache = (SZrAnalysisCache *)ZrMemoryRawMalloc(state->global, sizeof(SZrAnalysisCache));
    if (analyzer->cache != ZR_NULL) {
        analyzer->cache->isValid = ZR_FALSE;
        analyzer->cache->astHash = 0;
        ZrArrayInit(state, &analyzer->cache->cachedDiagnostics, sizeof(SZrDiagnostic *), 8);
        ZrArrayInit(state, &analyzer->cache->cachedSymbols, sizeof(SZrSymbol *), 8);
    }
    
    return analyzer;
}

// 释放语义分析器
void ZrSemanticAnalyzerFree(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }
    
    // 释放所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrArrayGet(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrDiagnosticFree(state, *diagPtr);
        }
    }
    
    ZrArrayFree(state, &analyzer->diagnostics);
    
    // 释放缓存
    if (analyzer->cache != ZR_NULL) {
        // 释放缓存的诊断信息
        for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrArrayGet(&analyzer->cache->cachedDiagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
            }
        }
        ZrArrayFree(state, &analyzer->cache->cachedDiagnostics);
        ZrArrayFree(state, &analyzer->cache->cachedSymbols);
        ZrMemoryRawFree(state->global, analyzer->cache, sizeof(SZrAnalysisCache));
    }
    
    if (analyzer->referenceTracker != ZR_NULL) {
        ZrReferenceTrackerFree(state, analyzer->referenceTracker);
    }
    
    if (analyzer->symbolTable != ZR_NULL) {
        ZrSymbolTableFree(state, analyzer->symbolTable);
    }
    
    // 释放编译器状态
    if (analyzer->compilerState != ZR_NULL) {
        ZrCompilerStateFree(analyzer->compilerState);
        ZrMemoryRawFree(state->global, analyzer->compilerState, sizeof(SZrCompilerState));
    }
    
    ZrMemoryRawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
}

// 辅助函数：从 AST 节点提取标识符名称
static SZrString *extract_identifier_name(SZrState *state, SZrAstNode *node) {
    if (node == ZR_NULL || state == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrIdentifier *identifier = &node->data.identifier;
        if (identifier != ZR_NULL && identifier->name != ZR_NULL) {
            return identifier->name;
        }
    }
    
    return ZR_NULL;
}

// 辅助函数：递归计算 AST 节点的哈希值
static TUInt64 compute_node_hash_recursive(SZrAstNode *node, TZrSize depth) {
    if (node == ZR_NULL || depth > 32) { // 限制递归深度避免栈溢出
        return 0;
    }
    
    TUInt64 hash = (TUInt64)node->type;
    hash = hash * 31 + (TUInt64)node->location.start.offset;
    hash = hash * 31 + (TUInt64)node->location.end.offset;
    
    // 根据节点类型访问不同的子节点
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(script->statements->nodes[i], depth + 1);
                }
            }
            if (script->moduleName != ZR_NULL) {
                hash = hash * 31 + compute_node_hash_recursive(script->moduleName, depth + 1);
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(block->body->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            hash = hash * 31 + compute_node_hash_recursive(binExpr->left, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(binExpr->right, depth + 1);
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            hash = hash * 31 + compute_node_hash_recursive(unaryExpr->argument, depth + 1);
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            hash = hash * 31 + compute_node_hash_recursive(assignExpr->left, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(assignExpr->right, depth + 1);
            break;
        }
        
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            SZrConditionalExpression *condExpr = &node->data.conditionalExpression;
            hash = hash * 31 + compute_node_hash_recursive(condExpr->test, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(condExpr->consequent, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(condExpr->alternate, depth + 1);
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *funcCall = &node->data.functionCall;
            if (funcCall->args != ZR_NULL && funcCall->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(funcCall->args->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            hash = hash * 31 + compute_node_hash_recursive(primaryExpr->property, depth + 1);
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(primaryExpr->members->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            hash = hash * 31 + compute_node_hash_recursive(varDecl->pattern, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(varDecl->value, depth + 1);
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(funcDecl->params->nodes[i], depth + 1);
                }
            }
            hash = hash * 31 + compute_node_hash_recursive(funcDecl->body, depth + 1);
            break;
        }
        
        case ZR_AST_ARRAY_LITERAL: {
            SZrArrayLiteral *arrayLit = &node->data.arrayLiteral;
            if (arrayLit->elements != ZR_NULL && arrayLit->elements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < arrayLit->elements->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(arrayLit->elements->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_OBJECT_LITERAL: {
            SZrObjectLiteral *objLit = &node->data.objectLiteral;
            if (objLit->properties != ZR_NULL && objLit->properties->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < objLit->properties->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(objLit->properties->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            hash = hash * 31 + compute_node_hash_recursive(ifExpr->condition, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(ifExpr->thenExpr, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(ifExpr->elseExpr, depth + 1);
            break;
        }
        
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            hash = hash * 31 + compute_node_hash_recursive(whileLoop->cond, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(whileLoop->block, depth + 1);
            break;
        }
        
        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            hash = hash * 31 + compute_node_hash_recursive(forLoop->init, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(forLoop->cond, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(forLoop->step, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(forLoop->block, depth + 1);
            break;
        }
        
        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            if (classDecl->members != ZR_NULL && classDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classDecl->members->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(classDecl->members->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            if (structDecl->members != ZR_NULL && structDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < structDecl->members->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(structDecl->members->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        default:
            // 对于其他节点类型，只使用基础哈希值
            break;
    }
    
    return hash;
}

// 辅助函数：计算 AST 哈希（递归实现）
static TZrSize compute_ast_hash(SZrAstNode *ast) {
    if (ast == ZR_NULL) {
        return 0;
    }
    
    return (TZrSize)compute_node_hash_recursive(ast, 0);
}

// 辅助函数：遍历 AST 收集符号定义
static void collect_symbols_from_ast(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
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
                        collect_symbols_from_ast(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            // 处理 moduleName（如果有）
            if (script->moduleName != ZR_NULL) {
                collect_symbols_from_ast(state, analyzer, script->moduleName);
            }
            return; // 已经递归处理，不需要继续
        }
        
        case ZR_AST_BLOCK: {
            // 块节点：遍历 body 数组
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        collect_symbols_from_ast(state, analyzer, block->body->nodes[i]);
                    }
                }
            }
            return; // 已经递归处理，不需要继续
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            // pattern 可能是 Identifier, DestructuringPattern, 或 DestructuringArrayPattern
            SZrString *name = extract_identifier_name(state, varDecl->pattern);
            if (name != ZR_NULL) {
                // 推断类型（集成类型推断系统）
                SZrInferredType *typeInfo = (SZrInferredType *)ZrMemoryRawMalloc(state->global, sizeof(SZrInferredType));
                if (typeInfo != ZR_NULL) {
                    // 创建或获取编译器状态（用于类型推断）
                    if (analyzer->compilerState == ZR_NULL) {
                        analyzer->compilerState = (SZrCompilerState *)ZrMemoryRawMalloc(state->global, sizeof(SZrCompilerState));
                        if (analyzer->compilerState != ZR_NULL) {
                            ZrCompilerStateInit(analyzer->compilerState, state);
                        }
                    }
                    
                    if (varDecl->typeInfo != ZR_NULL) {
                        // 转换 AST 类型到推断类型
                        // TODO: 简化实现：根据类型名称推断基础类型
                        // 完整实现需要使用 convert_ast_type_to_inferred_type
                        ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                    } else if (varDecl->value != ZR_NULL) {
                        // 从值推断类型
                        // 使用类型推断系统
                        if (analyzer->compilerState != ZR_NULL) {
                            SZrInferredType inferredType;
                            if (infer_expression_type(analyzer->compilerState, varDecl->value, &inferredType)) {
                                // 复制推断类型
                                *typeInfo = inferredType;
                            } else {
                                // TODO: 回退到简化实现
                                if (varDecl->value->type == ZR_AST_INTEGER_LITERAL) {
                                    ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_INT64);
                                } else if (varDecl->value->type == ZR_AST_FLOAT_LITERAL) {
                                    ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_DOUBLE);
                                } else if (varDecl->value->type == ZR_AST_STRING_LITERAL) {
                                    ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_STRING);
                                } else if (varDecl->value->type == ZR_AST_BOOLEAN_LITERAL) {
                                    ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_BOOL);
                                } else {
                                    ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                                }
                            }
                        } else {
                            // TODO: 简化实现：根据字面量类型推断
                            if (varDecl->value->type == ZR_AST_INTEGER_LITERAL) {
                                ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_INT64);
                            } else if (varDecl->value->type == ZR_AST_FLOAT_LITERAL) {
                                ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_DOUBLE);
                            } else if (varDecl->value->type == ZR_AST_STRING_LITERAL) {
                                ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_STRING);
                            } else if (varDecl->value->type == ZR_AST_BOOLEAN_LITERAL) {
                                ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_BOOL);
                            } else {
                                ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                            }
                        }
                    } else {
                        // 默认类型
                        ZrInferredTypeInit(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                    }
                }
                
                ZrSymbolTableAddSymbol(state, analyzer->symbolTable,
                                       ZR_SYMBOL_VARIABLE, name,
                                       node->location, typeInfo,
                                       varDecl->accessModifier, node);
            }
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            SZrString *name = extract_identifier_name(state, funcDecl->name);
            if (name != ZR_NULL) {
                // 推断返回类型（集成类型推断系统）
                SZrInferredType *returnType = (SZrInferredType *)ZrMemoryRawMalloc(state->global, sizeof(SZrInferredType));
                if (returnType != ZR_NULL) {
                    // 创建或获取编译器状态（用于类型推断）
                    if (analyzer->compilerState == ZR_NULL) {
                        analyzer->compilerState = (SZrCompilerState *)ZrMemoryRawMalloc(state->global, sizeof(SZrCompilerState));
                        if (analyzer->compilerState != ZR_NULL) {
                            ZrCompilerStateInit(analyzer->compilerState, state);
                        }
                    }
                    
                    if (funcDecl->returnType != ZR_NULL) {
                        // 转换 AST 类型到推断类型
                        // TODO: 简化实现：根据类型名称推断基础类型
                        // 完整实现需要使用 convert_ast_type_to_inferred_type
                        ZrInferredTypeInit(state, returnType, ZR_VALUE_TYPE_OBJECT);
                    } else {
                        // 默认返回类型为 object
                        ZrInferredTypeInit(state, returnType, ZR_VALUE_TYPE_OBJECT);
                    }
                }
                
                // SZrFunctionDeclaration 没有 accessModifier 成员，使用默认值
                ZrSymbolTableAddSymbol(state, analyzer->symbolTable,
                                       ZR_SYMBOL_FUNCTION, name,
                                       node->location, returnType,
                                       ZR_ACCESS_PUBLIC, node);
            }
            break;
        }
        
        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            SZrString *name = extract_identifier_name(state, classDecl->name);
            if (name != ZR_NULL) {
                ZrSymbolTableAddSymbol(state, analyzer->symbolTable,
                                       ZR_SYMBOL_CLASS, name,
                                       node->location, ZR_NULL,
                                       classDecl->accessModifier, node);
            }
            break;
        }
        
        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            SZrString *name = extract_identifier_name(state, structDecl->name);
            if (name != ZR_NULL) {
                ZrSymbolTableAddSymbol(state, analyzer->symbolTable,
                                       ZR_SYMBOL_STRUCT, name,
                                       node->location, ZR_NULL,
                                       structDecl->accessModifier, node);
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
static void collect_references_from_ast(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    // 检查是否是标识符引用
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *name = extract_identifier_name(state, node);
        if (name != ZR_NULL) {
            SZrSymbol *symbol = ZrSymbolTableLookup(analyzer->symbolTable, name, ZR_NULL);
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
                
                ZrReferenceTrackerAddReference(state, analyzer->referenceTracker,
                                               symbol, node->location, refType);
            } else {
                // 未定义的标识符，添加诊断
                ZrSemanticAnalyzerAddDiagnostic(state, analyzer,
                                                ZR_DIAGNOSTIC_ERROR,
                                                node->location,
                                                "Undefined identifier",
                                                "undefined_identifier");
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
                        collect_references_from_ast(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            if (script->moduleName != ZR_NULL) {
                collect_references_from_ast(state, analyzer, script->moduleName);
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            // 块节点：遍历 body 数组
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, block->body->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            // 变量声明：递归处理 value（表达式可能包含引用）
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            if (varDecl->value != ZR_NULL) {
                collect_references_from_ast(state, analyzer, varDecl->value);
            }
            break;
        }
        
        case ZR_AST_EXPRESSION_STATEMENT: {
            // 表达式语句：递归处理表达式
            SZrExpressionStatement *exprStmt = &node->data.expressionStatement;
            if (exprStmt->expr != ZR_NULL) {
                collect_references_from_ast(state, analyzer, exprStmt->expr);
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
                        collect_references_from_ast(state, analyzer, funcCall->args->nodes[i]);
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
                    SZrString *name = extract_identifier_name(state, primaryExpr->property);
                    if (name != ZR_NULL) {
                        SZrSymbol *symbol = ZrSymbolTableLookup(analyzer->symbolTable, name, ZR_NULL);
                        if (symbol != ZR_NULL && symbol->type == ZR_SYMBOL_FUNCTION) {
                            // 如果后面有函数调用，则是调用引用
                            TBool isCall = (primaryExpr->members != ZR_NULL && 
                                           primaryExpr->members->count > 0);
                            EZrReferenceType refType = isCall ? ZR_REFERENCE_CALL : ZR_REFERENCE_READ;
                            ZrReferenceTrackerAddReference(state, analyzer->referenceTracker,
                                                           symbol, primaryExpr->property->location, 
                                                           refType);
                        } else {
                            collect_references_from_ast(state, analyzer, primaryExpr->property);
                        }
                    }
                } else {
                    collect_references_from_ast(state, analyzer, primaryExpr->property);
                }
            }
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    if (primaryExpr->members->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, primaryExpr->members->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_MEMBER_EXPRESSION: {
            // 成员表达式：递归处理 property
            SZrMemberExpression *memberExpr = &node->data.memberExpression;
            if (memberExpr->property != ZR_NULL) {
                collect_references_from_ast(state, analyzer, memberExpr->property);
            }
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            // 二元表达式：递归处理左右操作数
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            if (binExpr->left != ZR_NULL) {
                collect_references_from_ast(state, analyzer, binExpr->left);
            }
            if (binExpr->right != ZR_NULL) {
                collect_references_from_ast(state, analyzer, binExpr->right);
            }
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            // 一元表达式：递归处理参数
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            if (unaryExpr->argument != ZR_NULL) {
                collect_references_from_ast(state, analyzer, unaryExpr->argument);
            }
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            // 赋值表达式：递归处理左右操作数
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            if (assignExpr->left != ZR_NULL) {
                // 左值是写引用
                if (assignExpr->left->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrString *name = extract_identifier_name(state, assignExpr->left);
                    if (name != ZR_NULL) {
                        SZrSymbol *symbol = ZrSymbolTableLookup(analyzer->symbolTable, name, ZR_NULL);
                        if (symbol != ZR_NULL) {
                            ZrReferenceTrackerAddReference(state, analyzer->referenceTracker,
                                                           symbol, assignExpr->left->location, 
                                                           ZR_REFERENCE_WRITE);
                        }
                    }
                } else {
                    collect_references_from_ast(state, analyzer, assignExpr->left);
                }
            }
            if (assignExpr->right != ZR_NULL) {
                collect_references_from_ast(state, analyzer, assignExpr->right);
            }
            break;
        }
        
        default:
            // TODO: 其他节点类型暂时跳过
            break;
    }
}

// 分析 AST
TBool ZrSemanticAnalyzerAnalyze(SZrState *state, 
                                 SZrSemanticAnalyzer *analyzer,
                                 SZrAstNode *ast) {
    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查缓存
    TZrSize astHash = 0;
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        astHash = compute_ast_hash(ast);
        if (analyzer->cache->isValid && analyzer->cache->astHash == astHash) {
            // 缓存有效，使用缓存结果
            // 复制缓存的诊断信息
            for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
                SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrArrayGet(&analyzer->cache->cachedDiagnostics, i);
                if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                    ZrArrayPush(state, &analyzer->diagnostics, diagPtr);
                }
            }
            return ZR_TRUE;
        }
    }
    
    analyzer->ast = ast;
    
    // 清除旧的诊断信息
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrArrayGet(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrDiagnosticFree(state, *diagPtr);
        }
    }
    analyzer->diagnostics.length = 0;
    
    // 创建或获取编译器状态（用于类型推断）
    if (analyzer->compilerState == ZR_NULL) {
        analyzer->compilerState = (SZrCompilerState *)ZrMemoryRawMalloc(state->global, sizeof(SZrCompilerState));
        if (analyzer->compilerState != ZR_NULL) {
            ZrCompilerStateInit(analyzer->compilerState, state);
            analyzer->compilerState->scriptAst = ast; // 设置脚本AST引用
        }
    } else {
        // 更新脚本AST引用
        analyzer->compilerState->scriptAst = ast;
    }
    
    // 第一阶段：收集符号定义
    collect_symbols_from_ast(state, analyzer, ast);
    
    // 第二阶段：收集引用
    collect_references_from_ast(state, analyzer, ast);
    
    // 第三阶段：类型检查（集成类型推断系统）
    // 遍历 AST 进行类型检查
    if (analyzer->compilerState != ZR_NULL) {
        perform_type_checking(state, analyzer, ast);
    }
    
    // 更新缓存
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        analyzer->cache->astHash = astHash;
        analyzer->cache->isValid = ZR_TRUE;
        
        // 复制诊断信息到缓存
        analyzer->cache->cachedDiagnostics.length = 0;
        for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrArrayGet(&analyzer->diagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                ZrArrayPush(state, &analyzer->cache->cachedDiagnostics, diagPtr);
            }
        }
    }
    
    return ZR_TRUE;
}

// 获取诊断信息
TBool ZrSemanticAnalyzerGetDiagnostics(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrArray *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrArrayInit(state, result, sizeof(SZrDiagnostic *), analyzer->diagnostics.length);
    }
    
    // 复制所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrArrayGet(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrArrayPush(state, result, diagPtr);
        }
    }
    
    return ZR_TRUE;
}

// 获取位置的符号
SZrSymbol *ZrSemanticAnalyzerGetSymbolAt(SZrSemanticAnalyzer *analyzer,
                                         SZrFileRange position) {
    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }
    
    return ZrSymbolTableFindDefinition(analyzer->symbolTable, position);
}

// 获取悬停信息
TBool ZrSemanticAnalyzerGetHoverInfo(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     SZrFileRange position,
                                     SZrHoverInfo **result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 查找位置的符号
    SZrSymbol *symbol = ZrSemanticAnalyzerGetSymbolAt(analyzer, position);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 构建悬停信息
    TChar buffer[512];
    TZrSize offset = 0;
    
    // 符号类型
    const TChar *typeName = "unknown";
    switch (symbol->type) {
        case ZR_SYMBOL_VARIABLE: typeName = "variable"; break;
        case ZR_SYMBOL_FUNCTION: typeName = "function"; break;
        case ZR_SYMBOL_CLASS: typeName = "class"; break;
        case ZR_SYMBOL_STRUCT: typeName = "struct"; break;
        default: break;
    }
    
    // 符号名称
    TNativeString nameStr;
    TZrSize nameLen;
    if (symbol->name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nameStr = ZrStringGetNativeStringShort(symbol->name);
        nameLen = symbol->name->shortStringLength;
    } else {
        nameStr = ZrStringGetNativeString(symbol->name);
        nameLen = symbol->name->longStringLength;
    }
    
    snprintf(buffer, sizeof(buffer), "**%s**: %.*s\n\nType: %s",
             typeName, (int)nameLen, nameStr, typeName);
    
    SZrString *contents = ZrStringCreate(state, buffer, strlen(buffer));
    if (contents == ZR_NULL) {
        return ZR_FALSE;
    }
    
    *result = ZrHoverInfoNew(state, buffer, symbol->location, symbol->typeInfo);
    return *result != ZR_NULL;
}

// 获取代码补全
TBool ZrSemanticAnalyzerGetCompletions(SZrState *state,
                                       SZrSemanticAnalyzer *analyzer,
                                       SZrFileRange position,
                                       SZrArray *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrArrayInit(state, result, sizeof(SZrCompletionItem *), 8);
    }
    
    // 获取当前作用域的所有符号
    SZrSymbolScope *scope = ZrSymbolTableGetCurrentScope(analyzer->symbolTable);
    if (scope != ZR_NULL) {
        for (TZrSize i = 0; i < scope->symbols.length; i++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrArrayGet(&scope->symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                SZrSymbol *symbol = *symbolPtr;
                
                // 获取符号名称
                TNativeString nameStr;
                TZrSize nameLen;
                if (symbol->name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                    nameStr = ZrStringGetNativeStringShort(symbol->name);
                    nameLen = symbol->name->shortStringLength;
                } else {
                    nameStr = ZrStringGetNativeString(symbol->name);
                    nameLen = symbol->name->longStringLength;
                }
                
                // 确定补全类型
                const TChar *kind = "variable";
                switch (symbol->type) {
                    case ZR_SYMBOL_FUNCTION: kind = "function"; break;
                    case ZR_SYMBOL_CLASS: kind = "class"; break;
                    case ZR_SYMBOL_STRUCT: kind = "struct"; break;
                    default: break;
                }
                
                TChar label[256];
                snprintf(label, sizeof(label), "%.*s", (int)nameLen, nameStr);
                
                SZrCompletionItem *item = ZrCompletionItemNew(state, label, kind, ZR_NULL, ZR_NULL, symbol->typeInfo);
                if (item != ZR_NULL) {
                    ZrArrayPush(state, result, &item);
                }
            }
        }
    }
    
    return ZR_TRUE;
}

// 添加诊断
TBool ZrSemanticAnalyzerAddDiagnostic(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     EZrDiagnosticSeverity severity,
                                     SZrFileRange location,
                                     const TChar *message,
                                     const TChar *code) {
    if (state == ZR_NULL || analyzer == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrDiagnostic *diagnostic = ZrDiagnosticNew(state, severity, location, message, code);
    if (diagnostic == ZR_NULL) {
        return ZR_FALSE;
    }
    
    ZrArrayPush(state, &analyzer->diagnostics, &diagnostic);
    
    return ZR_TRUE;
}

// 创建诊断
SZrDiagnostic *ZrDiagnosticNew(SZrState *state,
                                EZrDiagnosticSeverity severity,
                                SZrFileRange location,
                                const TChar *message,
                                const TChar *code) {
    if (state == ZR_NULL || message == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrDiagnostic *diagnostic = (SZrDiagnostic *)ZrMemoryRawMalloc(state->global, sizeof(SZrDiagnostic));
    if (diagnostic == ZR_NULL) {
        return ZR_NULL;
    }
    
    diagnostic->severity = severity;
    diagnostic->location = location;
    diagnostic->message = ZrStringCreate(state, message, strlen(message));
    diagnostic->code = code != ZR_NULL ? ZrStringCreate(state, code, strlen(code)) : ZR_NULL;
    
    if (diagnostic->message == ZR_NULL) {
        ZrMemoryRawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
        return ZR_NULL;
    }
    
    return diagnostic;
}

// 释放诊断
void ZrDiagnosticFree(SZrState *state, SZrDiagnostic *diagnostic) {
    if (state == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    
    if (diagnostic->message != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (diagnostic->code != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrMemoryRawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
}

// 创建补全项
SZrCompletionItem *ZrCompletionItemNew(SZrState *state,
                                       const TChar *label,
                                       const TChar *kind,
                                       const TChar *detail,
                                       const TChar *documentation,
                                       SZrInferredType *typeInfo) {
    if (state == ZR_NULL || label == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrCompletionItem *item = (SZrCompletionItem *)ZrMemoryRawMalloc(state->global, sizeof(SZrCompletionItem));
    if (item == ZR_NULL) {
        return ZR_NULL;
    }
    
    item->label = ZrStringCreate(state, label, strlen(label));
    item->kind = kind != ZR_NULL ? ZrStringCreate(state, kind, strlen(kind)) : ZR_NULL;
    item->detail = detail != ZR_NULL ? ZrStringCreate(state, detail, strlen(detail)) : ZR_NULL;
    item->documentation = documentation != ZR_NULL ? ZrStringCreate(state, documentation, strlen(documentation)) : ZR_NULL;
    item->typeInfo = typeInfo; // 不复制，只是引用
    
    if (item->label == ZR_NULL) {
        ZrMemoryRawFree(state->global, item, sizeof(SZrCompletionItem));
        return ZR_NULL;
    }
    
    return item;
}

// 释放补全项
void ZrCompletionItemFree(SZrState *state, SZrCompletionItem *item) {
    if (state == ZR_NULL || item == ZR_NULL) {
        return;
    }
    
    if (item->label != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->kind != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->detail != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->documentation != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrMemoryRawFree(state->global, item, sizeof(SZrCompletionItem));
}

// 创建悬停信息
SZrHoverInfo *ZrHoverInfoNew(SZrState *state,
                              const TChar *contents,
                              SZrFileRange range,
                              SZrInferredType *typeInfo) {
    if (state == ZR_NULL || contents == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrHoverInfo *info = (SZrHoverInfo *)ZrMemoryRawMalloc(state->global, sizeof(SZrHoverInfo));
    if (info == ZR_NULL) {
        return ZR_NULL;
    }
    
    info->contents = ZrStringCreate(state, contents, strlen(contents));
    info->range = range;
    info->typeInfo = typeInfo; // 不复制，只是引用
    
    if (info->contents == ZR_NULL) {
        ZrMemoryRawFree(state->global, info, sizeof(SZrHoverInfo));
        return ZR_NULL;
    }
    
    return info;
}

// 释放悬停信息
void ZrHoverInfoFree(SZrState *state, SZrHoverInfo *info) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }
    
    if (info->contents != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrMemoryRawFree(state->global, info, sizeof(SZrHoverInfo));
}

// 启用/禁用缓存
void ZrSemanticAnalyzerSetCacheEnabled(SZrSemanticAnalyzer *analyzer, TBool enabled) {
    if (analyzer == ZR_NULL) {
        return;
    }
    analyzer->enableCache = enabled;
}

// 清除缓存
void ZrSemanticAnalyzerClearCache(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->cache == ZR_NULL) {
        return;
    }
    
    analyzer->cache->isValid = ZR_FALSE;
    analyzer->cache->astHash = 0;
    
    // 清除缓存的诊断信息
    for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrArrayGet(&analyzer->cache->cachedDiagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
        }
    }
    analyzer->cache->cachedDiagnostics.length = 0;
    analyzer->cache->cachedSymbols.length = 0;
}
