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
extern void ZrParser_CompilerState_Init(SZrCompilerState *cs, SZrState *state);
extern void ZrParser_CompilerState_Free(SZrCompilerState *cs);

// 前向声明类型推断函数
extern TZrBool ZrParser_ExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);
extern TZrBool ZrParser_TypeCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *fromType, 
                                       const SZrInferredType *toType, SZrFileRange location);
extern TZrBool ZrParser_AssignmentCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *leftType, 
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

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            perform_type_checking(state, analyzer, usingStmt->resource);
            perform_type_checking(state, analyzer, usingStmt->body);
            break;
        }

        case ZR_AST_TEMPLATE_STRING_LITERAL: {
            SZrTemplateStringLiteral *templateLiteral = &node->data.templateStringLiteral;
            if (templateLiteral->segments != ZR_NULL && templateLiteral->segments->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
                    if (templateLiteral->segments->nodes[i] != ZR_NULL) {
                        perform_type_checking(state, analyzer, templateLiteral->segments->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_INTERPOLATED_SEGMENT: {
            perform_type_checking(state, analyzer, node->data.interpolatedSegment.expression);
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
SZrSemanticAnalyzer *ZrLanguageServer_SemanticAnalyzer_New(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSemanticAnalyzer));
    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }
    
    analyzer->state = state;
    analyzer->symbolTable = ZrLanguageServer_SymbolTable_New(state);
    analyzer->referenceTracker = ZR_NULL;
    analyzer->ast = ZR_NULL;
    analyzer->cache = ZR_NULL;
    analyzer->enableCache = ZR_TRUE; // 默认启用缓存
    analyzer->compilerState = ZR_NULL; // 延迟创建
    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;
    
    if (analyzer->symbolTable == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    analyzer->referenceTracker = ZrLanguageServer_ReferenceTracker_New(state, analyzer->symbolTable);
    if (analyzer->referenceTracker == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    ZrCore_Array_Init(state, &analyzer->diagnostics, sizeof(SZrDiagnostic *), 8);
    
    // 创建缓存
    analyzer->cache = (SZrAnalysisCache *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrAnalysisCache));
    if (analyzer->cache != ZR_NULL) {
        analyzer->cache->isValid = ZR_FALSE;
        analyzer->cache->astHash = 0;
        ZrCore_Array_Init(state, &analyzer->cache->cachedDiagnostics, sizeof(SZrDiagnostic *), 8);
        ZrCore_Array_Init(state, &analyzer->cache->cachedSymbols, sizeof(SZrSymbol *), 8);
    }
    
    return analyzer;
}

// 释放语义分析器
void ZrLanguageServer_SemanticAnalyzer_Free(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }

    // 释放所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
        }
    }
    
    ZrCore_Array_Free(state, &analyzer->diagnostics);
    
    // 释放缓存
    if (analyzer->cache != ZR_NULL) {
        // 释放缓存的诊断信息
        for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
            }
        }
        ZrCore_Array_Free(state, &analyzer->cache->cachedDiagnostics);
        ZrCore_Array_Free(state, &analyzer->cache->cachedSymbols);
        ZrCore_Memory_RawFree(state->global, analyzer->cache, sizeof(SZrAnalysisCache));
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, analyzer->referenceTracker);
    }

    if (analyzer->symbolTable != ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
    }

    // 释放编译器状态
    if (analyzer->compilerState != ZR_NULL) {
        ZrParser_CompilerState_Free(analyzer->compilerState);
        ZrCore_Memory_RawFree(state->global, analyzer->compilerState, sizeof(SZrCompilerState));
    }

    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;

    ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
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

static TZrBool string_equals_literal(SZrString *value, const TZrChar *literal) {
    TZrNativeString text;
    TZrSize length;
    TZrSize literalLength;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        text = ZrCore_String_GetNativeStringShort(value);
        length = value->shortStringLength;
    } else {
        text = ZrCore_String_GetNativeString(value);
        length = value->longStringLength;
    }

    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    literalLength = strlen(literal);
    return length == literalLength && memcmp(text, literal, literalLength) == 0;
}

static TZrBool is_implicit_runtime_identifier(SZrString *name) {
    return string_equals_literal(name, "this") || string_equals_literal(name, "super");
}

static SZrString *get_class_property_symbol_name(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_CLASS_PROPERTY ||
        node->data.classProperty.modifier == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET &&
        node->data.classProperty.modifier->data.propertyGet.name != ZR_NULL) {
        return node->data.classProperty.modifier->data.propertyGet.name->name;
    }

    if (node->data.classProperty.modifier->type == ZR_AST_PROPERTY_SET &&
        node->data.classProperty.modifier->data.propertySet.name != ZR_NULL) {
        return node->data.classProperty.modifier->data.propertySet.name->name;
    }

    return ZR_NULL;
}

static void add_definition_reference_for_symbol(SZrState *state,
                                                SZrSemanticAnalyzer *analyzer,
                                                SZrSymbol *symbol) {
    SZrFileRange range;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->referenceTracker == ZR_NULL ||
        symbol == ZR_NULL) {
        return;
    }

    range = symbol->selectionRange;
    if (range.start.offset == 0 && range.end.offset == 0) {
        range = symbol->location;
    }

    ZrLanguageServer_ReferenceTracker_AddReference(state,
                                                   analyzer->referenceTracker,
                                                   symbol,
                                                   range,
                                                   ZR_REFERENCE_DEFINITION);
}

// 辅助函数：递归计算 AST 节点的哈希值
static TZrUInt64 compute_node_hash_recursive(SZrAstNode *node, TZrSize depth) {
    if (node == ZR_NULL || depth > 32) { // 限制递归深度避免栈溢出
        return 0;
    }
    
    TZrUInt64 hash = (TZrUInt64)node->type;
    hash = hash * 31 + (TZrUInt64)node->location.start.offset;
    hash = hash * 31 + (TZrUInt64)node->location.end.offset;
    
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

static TZrBool reset_symbol_tracking(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, analyzer->referenceTracker);
        analyzer->referenceTracker = ZR_NULL;
    }
    if (analyzer->symbolTable != ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        analyzer->symbolTable = ZR_NULL;
    }

    analyzer->symbolTable = ZrLanguageServer_SymbolTable_New(state);
    if (analyzer->symbolTable == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer->referenceTracker = ZrLanguageServer_ReferenceTracker_New(state, analyzer->symbolTable);
    if (analyzer->referenceTracker == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        analyzer->symbolTable = ZR_NULL;
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool prepare_semantic_state(SZrState *state,
                                    SZrSemanticAnalyzer *analyzer,
                                    SZrAstNode *ast) {
    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reset_symbol_tracking(state, analyzer)) {
        return ZR_FALSE;
    }

    if (analyzer->compilerState == ZR_NULL) {
        analyzer->compilerState =
            (SZrCompilerState *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrCompilerState));
        if (analyzer->compilerState == ZR_NULL) {
            return ZR_FALSE;
        }
    } else {
        ZrParser_CompilerState_Free(analyzer->compilerState);
    }

    ZrParser_CompilerState_Init(analyzer->compilerState, state);
    analyzer->compilerState->scriptAst = ast;
    if (analyzer->compilerState->typeEnv != ZR_NULL) {
        analyzer->compilerState->typeEnv->semanticContext =
            analyzer->compilerState->semanticContext;
    }
    if (analyzer->compilerState->compileTimeTypeEnv != ZR_NULL) {
        analyzer->compilerState->compileTimeTypeEnv->semanticContext =
            analyzer->compilerState->semanticContext;
    }

    if (analyzer->compilerState->hirModule != ZR_NULL) {
        ZrParser_HirModule_Free(state, analyzer->compilerState->hirModule);
        analyzer->compilerState->hirModule = ZR_NULL;
    }
    analyzer->compilerState->hirModule =
        ZrParser_HirModule_New(state, analyzer->compilerState->semanticContext, ast);
    analyzer->semanticContext = analyzer->compilerState->semanticContext;
    analyzer->hirModule = analyzer->compilerState->hirModule;

    return analyzer->semanticContext != ZR_NULL;
}

static TZrBool register_symbol_semantics(SZrSemanticAnalyzer *analyzer,
                                       SZrSymbol *symbol,
                                       EZrSemanticSymbolKind semanticKind,
                                       const SZrInferredType *typeInfo,
                                       EZrSemanticTypeKind typeKind) {
    TZrTypeId typeId = 0;
    TZrOverloadSetId overloadSetId = 0;
    TZrSymbolId symbolId;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    if (semanticKind == ZR_SEMANTIC_SYMBOL_KIND_TYPE && typeInfo == ZR_NULL) {
        typeId = ZrParser_Semantic_RegisterNamedType(analyzer->semanticContext,
                                             symbol->name,
                                             typeKind,
                                             symbol->astNode);
    } else if (typeInfo != ZR_NULL) {
        typeId = ZrParser_Semantic_RegisterInferredType(analyzer->semanticContext,
                                                typeInfo,
                                                typeKind,
                                                typeInfo->typeName,
                                                symbol->astNode);
    }

    if (semanticKind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
        overloadSetId = ZrParser_Semantic_GetOrCreateOverloadSet(analyzer->semanticContext, symbol->name);
    }

    symbolId = ZrParser_Semantic_RegisterSymbol(analyzer->semanticContext,
                                        symbol->name,
                                        semanticKind,
                                        typeId,
                                        overloadSetId,
                                        symbol->astNode,
                                        symbol->location);
    if (overloadSetId != 0) {
        ZrParser_Semantic_AddOverloadMember(analyzer->semanticContext, overloadSetId, symbolId);
    }

    symbol->semanticId = symbolId;
    symbol->semanticTypeId = typeId;
    symbol->overloadSetId = overloadSetId;
    return symbolId != 0;
}

static TZrSymbolId resolve_semantic_symbol_id_for_node(SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *node) {
    SZrString *name;
    SZrSymbol *symbol;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || node == ZR_NULL) {
        return 0;
    }

    name = extract_identifier_name(analyzer->state, node);
    if (name == ZR_NULL) {
        return 0;
    }

    symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, name, ZR_NULL);
    if (symbol == ZR_NULL) {
        return 0;
    }

    return symbol->semanticId;
}

static void record_template_string_segments(SZrSemanticAnalyzer *analyzer,
                                            SZrAstNode *node) {
    SZrTemplateStringLiteral *templateLiteral;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_TEMPLATE_STRING_LITERAL) {
        return;
    }

    templateLiteral = &node->data.templateStringLiteral;
    if (templateLiteral->segments == ZR_NULL || templateLiteral->segments->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
        SZrAstNode *segmentNode = templateLiteral->segments->nodes[i];
        SZrTemplateSegment segment;

        if (segmentNode == ZR_NULL) {
            continue;
        }

        segment.isInterpolation = ZR_FALSE;
        segment.staticText = ZR_NULL;
        segment.expression = ZR_NULL;

        if (segmentNode->type == ZR_AST_STRING_LITERAL) {
            segment.staticText = segmentNode->data.stringLiteral.value;
        } else if (segmentNode->type == ZR_AST_INTERPOLATED_SEGMENT) {
            segment.isInterpolation = ZR_TRUE;
            segment.expression = segmentNode->data.interpolatedSegment.expression;
        } else {
            continue;
        }

        ZrParser_Semantic_AppendTemplateSegment(analyzer->semanticContext, &segment);
    }
}

static void record_using_cleanup_step(SZrSemanticAnalyzer *analyzer,
                                      SZrAstNode *resource) {
    SZrDeterministicCleanupStep step;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || resource == ZR_NULL) {
        return;
    }

    step.kind = ZR_DETERMINISTIC_CLEANUP_KIND_BLOCK_SCOPE;
    step.regionId = ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext);
    step.ownerRegionId = step.regionId;
    step.symbolId = resolve_semantic_symbol_id_for_node(analyzer, resource);
    step.declarationOrder = (TZrInt32)analyzer->semanticContext->cleanupPlan.length;
    step.callsClose = ZR_TRUE;
    step.callsDestructor = ZR_TRUE;
    ZrParser_Semantic_AppendCleanupStep(analyzer->semanticContext, &step);
}

static SZrInferredType *create_field_symbol_type(SZrState *state,
                                                 SZrSemanticAnalyzer *analyzer,
                                                 const SZrType *fieldType) {
    SZrInferredType *typeInfo;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    typeInfo = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
    if (typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
    if (fieldType == ZR_NULL) {
        return typeInfo;
    }

    if (analyzer != ZR_NULL && analyzer->compilerState != ZR_NULL &&
        ZrParser_AstTypeToInferredType_Convert(analyzer->compilerState, fieldType, typeInfo)) {
        return typeInfo;
    }

    ZrParser_InferredType_Free(state, typeInfo);
    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
    typeInfo->ownershipQualifier = fieldType->ownershipQualifier;
    return typeInfo;
}

static void record_field_cleanup_step(SZrSemanticAnalyzer *analyzer,
                                      SZrSymbol *symbol,
                                      EZrDeterministicCleanupKind kind,
                                      TZrLifetimeRegionId ownerRegionId,
                                      TZrInt32 declarationOrder) {
    SZrDeterministicCleanupStep step;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || symbol == ZR_NULL ||
        symbol->semanticId == 0) {
        return;
    }

    step.kind = kind;
    step.regionId = ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext);
    step.ownerRegionId = ownerRegionId;
    step.symbolId = symbol->semanticId;
    step.declarationOrder = declarationOrder;
    step.callsClose = ZR_TRUE;
    step.callsDestructor = ZR_TRUE;
    ZrParser_Semantic_AppendCleanupStep(analyzer->semanticContext, &step);
}

static void register_field_symbol_from_ast(SZrState *state,
                                           SZrSemanticAnalyzer *analyzer,
                                           SZrAstNode *fieldNode,
                                           TZrLifetimeRegionId ownerRegionId,
                                           EZrDeterministicCleanupKind cleanupKind,
                                           TZrInt32 declarationOrder) {
    SZrSymbol *symbol = ZR_NULL;
    SZrString *name = ZR_NULL;
    SZrInferredType *typeInfo = ZR_NULL;
    EZrAccessModifier accessModifier = ZR_ACCESS_PRIVATE;
    TZrBool isUsingManaged = ZR_FALSE;
    TZrBool isStatic = ZR_FALSE;
    const SZrType *fieldType = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || fieldNode == ZR_NULL) {
        return;
    }

    if (fieldNode->type == ZR_AST_STRUCT_FIELD) {
        SZrStructField *field = &fieldNode->data.structField;
        name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
        accessModifier = field->access;
        isUsingManaged = field->isUsingManaged;
        isStatic = field->isStatic;
        fieldType = field->typeInfo;
    } else if (fieldNode->type == ZR_AST_CLASS_FIELD) {
        SZrClassField *field = &fieldNode->data.classField;
        name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
        accessModifier = field->access;
        isUsingManaged = field->isUsingManaged;
        isStatic = field->isStatic;
        fieldType = field->typeInfo;
    } else {
        return;
    }

    if (name == ZR_NULL) {
        return;
    }

    typeInfo = create_field_symbol_type(state, analyzer, fieldType);
    ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                             analyzer->symbolTable,
                             ZR_SYMBOL_FIELD,
                             name,
                             fieldNode->location,
                             typeInfo,
                             accessModifier,
                             fieldNode,
                             &symbol);

    if (symbol != ZR_NULL) {
        register_symbol_semantics(analyzer,
                                  symbol,
                                  ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                                  typeInfo,
                                  ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
        add_definition_reference_for_symbol(state, analyzer, symbol);
    }

    if (!isUsingManaged) {
        return;
    }

    if (isStatic) {
        ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                        analyzer,
                                        ZR_DIAGNOSTIC_ERROR,
                                        fieldNode->location,
                                        "Field-scoped `using` only supports instance fields",
                                        "static_using_field");
        return;
    }

    record_field_cleanup_step(analyzer,
                              symbol,
                              cleanupKind,
                              ownerRegionId,
                              declarationOrder);
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
            SZrSymbol *symbol = ZR_NULL;
            // pattern 可能是 Identifier, DestructuringPattern, 或 DestructuringArrayPattern
            SZrString *name = extract_identifier_name(state, varDecl->pattern);
            if (name != ZR_NULL) {
                // 推断类型（集成类型推断系统）
                SZrInferredType *typeInfo = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
                if (typeInfo != ZR_NULL) {
                    if (varDecl->typeInfo != ZR_NULL) {
                        // 转换 AST 类型到推断类型
                        // TODO: 简化实现：根据类型名称推断基础类型
                        // 完整实现需要使用 ZrParser_AstTypeToInferredType_Convert
                        ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                    } else if (varDecl->value != ZR_NULL) {
                        // 从值推断类型
                        // 使用类型推断系统
                        if (analyzer->compilerState != ZR_NULL) {
                            SZrInferredType inferredType;
                            ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
                            if (ZrParser_ExpressionType_Infer(analyzer->compilerState, varDecl->value, &inferredType)) {
                                // 复制推断类型
                                *typeInfo = inferredType;
                            } else {
                                ZrParser_InferredType_Free(state, &inferredType);
                                // TODO: 回退到简化实现
                                if (varDecl->value->type == ZR_AST_INTEGER_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_INT64);
                                } else if (varDecl->value->type == ZR_AST_FLOAT_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_DOUBLE);
                                } else if (varDecl->value->type == ZR_AST_STRING_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_STRING);
                                } else if (varDecl->value->type == ZR_AST_BOOLEAN_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_BOOL);
                                } else {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                                }
                            }
                        } else {
                            // TODO: 简化实现：根据字面量类型推断
                            if (varDecl->value->type == ZR_AST_INTEGER_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_INT64);
                            } else if (varDecl->value->type == ZR_AST_FLOAT_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_DOUBLE);
                            } else if (varDecl->value->type == ZR_AST_STRING_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_STRING);
                            } else if (varDecl->value->type == ZR_AST_BOOLEAN_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_BOOL);
                            } else {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                            }
                        }
                    } else {
                        // 默认类型
                        ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                    }
                }
                
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_VARIABLE, name,
                                         node->location, typeInfo,
                                         varDecl->accessModifier, node,
                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL &&
                    typeInfo != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterVariable(state,
                                                     analyzer->compilerState->typeEnv,
                                                     name,
                                                     typeInfo);
                }
                register_symbol_semantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                                          typeInfo,
                                          ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                add_definition_reference_for_symbol(state, analyzer, symbol);
            }
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            if (usingStmt->body != ZR_NULL) {
                collect_symbols_from_ast(state, analyzer, usingStmt->body);
            }
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrArray paramTypes;
            SZrString *name = funcDecl->name != ZR_NULL ? funcDecl->name->name : ZR_NULL;
            if (name != ZR_NULL) {
                // 推断返回类型（集成类型推断系统）
                SZrInferredType *returnType = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
                if (returnType != ZR_NULL) {
                    if (funcDecl->returnType != ZR_NULL) {
                        // 转换 AST 类型到推断类型
                        // TODO: 简化实现：根据类型名称推断基础类型
                        // 完整实现需要使用 ZrParser_AstTypeToInferredType_Convert
                        ZrParser_InferredType_Init(state, returnType, ZR_VALUE_TYPE_OBJECT);
                    } else {
                        // 默认返回类型为 object
                        ZrParser_InferredType_Init(state, returnType, ZR_VALUE_TYPE_OBJECT);
                    }
                }
                
                // SZrFunctionDeclaration 没有 accessModifier 成员，使用默认值
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_FUNCTION, name,
                                         node->location, returnType,
                                         ZR_ACCESS_PUBLIC, node,
                                         &symbol);
                ZrCore_Array_Construct(&paramTypes);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL &&
                    returnType != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterFunction(state,
                                                     analyzer->compilerState->typeEnv,
                                                     name,
                                                     returnType,
                                                     &paramTypes);
                }
                register_symbol_semantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                          returnType,
                                          ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                add_definition_reference_for_symbol(state, analyzer, symbol);
            }
            break;
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
                register_symbol_semantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                          ZR_NULL,
                                          ZR_SEMANTIC_TYPE_KIND_REFERENCE);
                add_definition_reference_for_symbol(state, analyzer, symbol);
                
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
                                                            TZrBool found = ZR_FALSE;
                                                            for (TZrSize k = 0; k < classDecl->members->count; k++) {
                                                                SZrAstNode *classMember = classDecl->members->nodes[k];
                                                                if (classMember != ZR_NULL && 
                                                                    classMember->type == ZR_AST_CLASS_FIELD) {
                                                                    SZrClassField *classField = &classMember->data.classField;
                                                                    if (classField->name != ZR_NULL && 
                                                                        ZrCore_String_Equal(classField->name->name, fieldName)) {
                                                                        found = ZR_TRUE;
                                                                        // 检查类字段是否也是 const
                                                                        if (!classField->isConst) {
                                                                            TZrChar errorMsg[256];
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
                        register_field_symbol_from_ast(state,
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
                            register_symbol_semantics(analyzer,
                                                      memberSymbol,
                                                      ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                                      ZR_NULL,
                                                      ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                            add_definition_reference_for_symbol(state, analyzer, memberSymbol);
                        }
                    } else if (classMember->type == ZR_AST_CLASS_PROPERTY) {
                        SZrClassProperty *property = &classMember->data.classProperty;
                        SZrString *memberName = get_class_property_symbol_name(classMember);
                        SZrSymbol *memberSymbol = ZR_NULL;
                        if (memberName != ZR_NULL) {
                            ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                                     analyzer->symbolTable,
                                                                     ZR_SYMBOL_PROPERTY,
                                                                     memberName,
                                                                     classMember->location,
                                                                     ZR_NULL,
                                                                     property->access,
                                                                     classMember,
                                                                     &memberSymbol);
                            register_symbol_semantics(analyzer,
                                                      memberSymbol,
                                                      ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                                                      ZR_NULL,
                                                      ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                            add_definition_reference_for_symbol(state, analyzer, memberSymbol);
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
                register_symbol_semantics(analyzer,
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
                        register_field_symbol_from_ast(state,
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
            } else if (is_implicit_runtime_identifier(name)) {
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

        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            if (funcDecl->body != ZR_NULL) {
                collect_references_from_ast(state, analyzer, funcDecl->body);
            }
            break;
        }

        case ZR_AST_TEST_DECLARATION: {
            SZrTestDeclaration *testDecl = &node->data.testDeclaration;
            if (testDecl->body != ZR_NULL) {
                collect_references_from_ast(state, analyzer, testDecl->body);
            }
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            record_using_cleanup_step(analyzer, usingStmt->resource);
            if (usingStmt->resource != ZR_NULL) {
                collect_references_from_ast(state, analyzer, usingStmt->resource);
            }
            if (usingStmt->body != ZR_NULL) {
                collect_references_from_ast(state, analyzer, usingStmt->body);
            }
            break;
        }

        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStmt = &node->data.returnStatement;
            if (returnStmt->expr != ZR_NULL) {
                collect_references_from_ast(state, analyzer, returnStmt->expr);
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

        case ZR_AST_CONSTRUCT_EXPRESSION: {
            SZrConstructExpression *constructExpr = &node->data.constructExpression;
            if (constructExpr->target != ZR_NULL) {
                collect_references_from_ast(state, analyzer, constructExpr->target);
            }
            if (constructExpr->args != ZR_NULL && constructExpr->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < constructExpr->args->count; i++) {
                    if (constructExpr->args->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, constructExpr->args->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_TEMPLATE_STRING_LITERAL: {
            SZrTemplateStringLiteral *templateLiteral = &node->data.templateStringLiteral;
            record_template_string_segments(analyzer, node);
            if (templateLiteral->segments != ZR_NULL && templateLiteral->segments->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
                    if (templateLiteral->segments->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, templateLiteral->segments->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_INTERPOLATED_SEGMENT: {
            if (node->data.interpolatedSegment.expression != ZR_NULL) {
                collect_references_from_ast(state,
                                            analyzer,
                                            node->data.interpolatedSegment.expression);
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
                        SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, name, ZR_NULL);
                        if (symbol != ZR_NULL) {
                            ZrLanguageServer_ReferenceTracker_AddReference(state, analyzer->referenceTracker,
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

        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            if (classDecl->inherits != ZR_NULL && classDecl->inherits->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classDecl->inherits->count; i++) {
                    if (classDecl->inherits->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, classDecl->inherits->nodes[i]);
                    }
                }
            }
            if (classDecl->members != ZR_NULL && classDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classDecl->members->count; i++) {
                    if (classDecl->members->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, classDecl->members->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_CLASS_FIELD: {
            SZrClassField *classField = &node->data.classField;
            if (classField->init != ZR_NULL) {
                collect_references_from_ast(state, analyzer, classField->init);
            }
            break;
        }

        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *classMethod = &node->data.classMethod;
            if (classMethod->body != ZR_NULL) {
                collect_references_from_ast(state, analyzer, classMethod->body);
            }
            break;
        }

        case ZR_AST_CLASS_PROPERTY: {
            if (node->data.classProperty.modifier != ZR_NULL) {
                collect_references_from_ast(state, analyzer, node->data.classProperty.modifier);
            }
            break;
        }

        case ZR_AST_PROPERTY_GET: {
            if (node->data.propertyGet.body != ZR_NULL) {
                collect_references_from_ast(state, analyzer, node->data.propertyGet.body);
            }
            break;
        }

        case ZR_AST_PROPERTY_SET: {
            if (node->data.propertySet.body != ZR_NULL) {
                collect_references_from_ast(state, analyzer, node->data.propertySet.body);
            }
            break;
        }
        
        default:
            // TODO: 其他节点类型暂时跳过
            break;
    }
}

// 分析 AST
TZrBool ZrLanguageServer_SemanticAnalyzer_Analyze(SZrState *state, 
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
                SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
                if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                    ZrCore_Array_Push(state, &analyzer->diagnostics, diagPtr);
                }
            }
            return ZR_TRUE;
        }
    }
    
    analyzer->ast = ast;
    
    // 清除旧的诊断信息
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
        }
    }
    analyzer->diagnostics.length = 0;
    
    if (!prepare_semantic_state(state, analyzer, ast)) {
        return ZR_FALSE;
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
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                ZrCore_Array_Push(state, &analyzer->cache->cachedDiagnostics, diagPtr);
            }
        }
    }
    
    return ZR_TRUE;
}

// 获取诊断信息
TZrBool ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrArray *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrDiagnostic *), analyzer->diagnostics.length);
    }
    
    // 复制所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrCore_Array_Push(state, result, diagPtr);
        }
    }
    
    return ZR_TRUE;
}

// 获取位置的符号
SZrSymbol *ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(SZrSemanticAnalyzer *analyzer,
                                         SZrFileRange position) {
    SZrReference *reference;

    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        reference = ZrLanguageServer_ReferenceTracker_FindReferenceAt(analyzer->referenceTracker, position);
        if (reference != ZR_NULL) {
            return reference->symbol;
        }
    }
    
    return ZrLanguageServer_SymbolTable_FindDefinition(analyzer->symbolTable, position);
}

// 获取悬停信息
TZrBool ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     SZrFileRange position,
                                     SZrHoverInfo **result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 查找位置的符号
    SZrSymbol *symbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, position);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 构建悬停信息
    TZrChar buffer[512];
    
    // 符号类型
    const TZrChar *typeName = "unknown";
    switch (symbol->type) {
        case ZR_SYMBOL_VARIABLE: typeName = "variable"; break;
        case ZR_SYMBOL_FUNCTION: typeName = "function"; break;
        case ZR_SYMBOL_CLASS: typeName = "class"; break;
        case ZR_SYMBOL_STRUCT: typeName = "struct"; break;
        default: break;
    }
    
    // 符号名称
    TZrNativeString nameStr;
    TZrSize nameLen;
    if (symbol->name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nameStr = ZrCore_String_GetNativeStringShort(symbol->name);
        nameLen = symbol->name->shortStringLength;
    } else {
        nameStr = ZrCore_String_GetNativeString(symbol->name);
        nameLen = symbol->name->longStringLength;
    }
    
    snprintf(buffer, sizeof(buffer), "**%s**: %.*s\n\nType: %s",
             typeName, (int)nameLen, nameStr, typeName);
    
    SZrString *contents = ZrCore_String_Create(state, buffer, strlen(buffer));
    if (contents == ZR_NULL) {
        return ZR_FALSE;
    }
    
    *result = ZrLanguageServer_HoverInfo_New(state, buffer, symbol->selectionRange, symbol->typeInfo);
    return *result != ZR_NULL;
}

// 获取代码补全
TZrBool ZrLanguageServer_SemanticAnalyzer_GetCompletions(SZrState *state,
                                       SZrSemanticAnalyzer *analyzer,
                                       SZrFileRange position,
                                       SZrArray *result) {
    ZR_UNUSED_PARAMETER(position);
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrCompletionItem *), 8);
    }
    
    // 获取当前作用域的所有符号
    SZrSymbolScope *scope = ZrLanguageServer_SymbolTable_GetCurrentScope(analyzer->symbolTable);
    if (scope != ZR_NULL) {
        for (TZrSize i = 0; i < scope->symbols.length; i++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&scope->symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                SZrSymbol *symbol = *symbolPtr;
                
                // 获取符号名称
                TZrNativeString nameStr;
                TZrSize nameLen;
                if (symbol->name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                    nameStr = ZrCore_String_GetNativeStringShort(symbol->name);
                    nameLen = symbol->name->shortStringLength;
                } else {
                    nameStr = ZrCore_String_GetNativeString(symbol->name);
                    nameLen = symbol->name->longStringLength;
                }
                
                // 确定补全类型
                const TZrChar *kind = "variable";
                switch (symbol->type) {
                    case ZR_SYMBOL_FUNCTION: kind = "function"; break;
                    case ZR_SYMBOL_CLASS: kind = "class"; break;
                    case ZR_SYMBOL_STRUCT: kind = "struct"; break;
                    case ZR_SYMBOL_METHOD: kind = "method"; break;
                    case ZR_SYMBOL_PROPERTY: kind = "property"; break;
                    case ZR_SYMBOL_FIELD: kind = "field"; break;
                    default: break;
                }
                
                TZrChar label[256];
                snprintf(label, sizeof(label), "%.*s", (int)nameLen, nameStr);
                
                SZrCompletionItem *item = ZrLanguageServer_CompletionItem_New(state, label, kind, ZR_NULL, ZR_NULL, symbol->typeInfo);
                if (item != ZR_NULL) {
                    ZrCore_Array_Push(state, result, &item);
                }
            }
        }
    }
    
    return ZR_TRUE;
}

// 添加诊断
TZrBool ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     EZrDiagnosticSeverity severity,
                                     SZrFileRange location,
                                     const TZrChar *message,
                                     const TZrChar *code) {
    if (state == ZR_NULL || analyzer == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrDiagnostic *diagnostic = ZrLanguageServer_Diagnostic_New(state, severity, location, message, code);
    if (diagnostic == ZR_NULL) {
        return ZR_FALSE;
    }
    
    ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
    
    return ZR_TRUE;
}

// 创建诊断
SZrDiagnostic *ZrLanguageServer_Diagnostic_New(SZrState *state,
                                EZrDiagnosticSeverity severity,
                                SZrFileRange location,
                                const TZrChar *message,
                                const TZrChar *code) {
    if (state == ZR_NULL || message == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrDiagnostic *diagnostic = (SZrDiagnostic *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrDiagnostic));
    if (diagnostic == ZR_NULL) {
        return ZR_NULL;
    }
    
    diagnostic->severity = severity;
    diagnostic->location = location;
    diagnostic->message = ZrCore_String_Create(state, (TZrNativeString)message, strlen(message));
    diagnostic->code = code != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)code, strlen(code)) : ZR_NULL;
    
    if (diagnostic->message == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
        return ZR_NULL;
    }
    
    return diagnostic;
}

// 释放诊断
void ZrLanguageServer_Diagnostic_Free(SZrState *state, SZrDiagnostic *diagnostic) {
    if (state == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    
    if (diagnostic->message != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (diagnostic->code != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
}

// 创建补全项
SZrCompletionItem *ZrLanguageServer_CompletionItem_New(SZrState *state,
                                       const TZrChar *label,
                                       const TZrChar *kind,
                                       const TZrChar *detail,
                                       const TZrChar *documentation,
                                       SZrInferredType *typeInfo) {
    if (state == ZR_NULL || label == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrCompletionItem *item = (SZrCompletionItem *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrCompletionItem));
    if (item == ZR_NULL) {
        return ZR_NULL;
    }
    
    item->label = ZrCore_String_Create(state, (TZrNativeString)label, strlen(label));
    item->kind = kind != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)kind, strlen(kind)) : ZR_NULL;
    item->detail = detail != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)detail, strlen(detail)) : ZR_NULL;
    item->documentation = documentation != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)documentation, strlen(documentation)) : ZR_NULL;
    item->typeInfo = typeInfo; // 不复制，只是引用
    
    if (item->label == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, item, sizeof(SZrCompletionItem));
        return ZR_NULL;
    }
    
    return item;
}

// 释放补全项
void ZrLanguageServer_CompletionItem_Free(SZrState *state, SZrCompletionItem *item) {
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
    ZrCore_Memory_RawFree(state->global, item, sizeof(SZrCompletionItem));
}

// 创建悬停信息
SZrHoverInfo *ZrLanguageServer_HoverInfo_New(SZrState *state,
                              const TZrChar *contents,
                              SZrFileRange range,
                              SZrInferredType *typeInfo) {
    if (state == ZR_NULL || contents == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrHoverInfo *info = (SZrHoverInfo *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrHoverInfo));
    if (info == ZR_NULL) {
        return ZR_NULL;
    }
    
    info->contents = ZrCore_String_Create(state, (TZrNativeString)contents, strlen(contents));
    info->range = range;
    info->typeInfo = typeInfo; // 不复制，只是引用
    
    if (info->contents == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, info, sizeof(SZrHoverInfo));
        return ZR_NULL;
    }
    
    return info;
}

// 释放悬停信息
void ZrLanguageServer_HoverInfo_Free(SZrState *state, SZrHoverInfo *info) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }
    
    if (info->contents != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, info, sizeof(SZrHoverInfo));
}

// 启用/禁用缓存
void ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(SZrSemanticAnalyzer *analyzer, TZrBool enabled) {
    if (analyzer == ZR_NULL) {
        return;
    }
    analyzer->enableCache = enabled;
}

// 清除缓存
void ZrLanguageServer_SemanticAnalyzer_ClearCache(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->cache == ZR_NULL) {
        return;
    }
    
    analyzer->cache->isValid = ZR_FALSE;
    analyzer->cache->astHash = 0;
    
    // 清除缓存的诊断信息
    for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
        }
    }
    analyzer->cache->cachedDiagnostics.length = 0;
    analyzer->cache->cachedSymbols.length = 0;
}
