//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/array.h"
#include "zr_vm_common/zr_common_conf.h"

#include <string.h>
#include <stdio.h>

// 前向声明
static TBool evaluate_compile_time_expression(SZrCompilerState *cs, SZrAstNode *node, SZrTypeValue *result);
static TBool check_compile_time_identifier_access(SZrCompilerState *cs, SZrString *name, TBool isWrite);

// 检查标识符是否为编译期标识符
static TBool is_compile_time_identifier(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查编译期变量表
    if (cs->compileTimeVariables.isValid && cs->compileTimeVariables.length > 0) {
        for (TZrSize i = 0; i < cs->compileTimeVariables.length; i++) {
            SZrCompileTimeVariable **varPtr = (SZrCompileTimeVariable**)ZrArrayGet(&cs->compileTimeVariables, i);
            if (varPtr != ZR_NULL && *varPtr != ZR_NULL) {
                if (ZrStringEqual((*varPtr)->name, name)) {
                    return ZR_TRUE;
                }
            }
        }
    }
    
    // 检查编译期函数表
    if (cs->compileTimeFunctions.isValid && cs->compileTimeFunctions.length > 0) {
        for (TZrSize i = 0; i < cs->compileTimeFunctions.length; i++) {
            SZrCompileTimeFunction **funcPtr = (SZrCompileTimeFunction**)ZrArrayGet(&cs->compileTimeFunctions, i);
            if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL) {
                if (ZrStringEqual((*funcPtr)->name, name)) {
                    return ZR_TRUE;
                }
            }
        }
    }
    
    return ZR_FALSE;
}

// 检查编译期对运行时符号的访问（只读访问）
static TBool check_compile_time_identifier_access(SZrCompilerState *cs, SZrString *name, TBool isWrite) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 如果是编译期标识符，允许访问
    if (is_compile_time_identifier(cs, name)) {
        return ZR_TRUE;
    }
    
    // 如果是运行时标识符
    if (isWrite) {
        // 禁止写入运行时变量
        TNativeString nameStr = ZrStringGetNativeString(name);
        TChar errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), 
                "Compile-time code cannot write to runtime variable: %s", 
                nameStr ? nameStr : "<null>");
        ZrCompileTimeError(cs, ZR_COMPILE_TIME_ERROR_ERROR, errorMsg, 
                          cs->currentAst != ZR_NULL ? cs->currentAst->location : 
                          (SZrFileRange){{0}});
        return ZR_FALSE;
    } else {
        // 允许只读访问运行时变量（用于编译期检查）
        return ZR_TRUE;
    }
}

// 编译期表达式求值
static TBool evaluate_compile_time_expression(SZrCompilerState *cs, SZrAstNode *node, SZrTypeValue *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 保存当前上下文
    TBool oldContext = cs->isInCompileTimeContext;
    cs->isInCompileTimeContext = ZR_TRUE;
    
    TBool success = ZR_FALSE;
    
    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL: {
            ZrValueInitAsInt(cs->state, result, node->data.integerLiteral.value);
            success = ZR_TRUE;
            break;
        }
        
        case ZR_AST_FLOAT_LITERAL: {
            ZrValueInitAsFloat(cs->state, result, node->data.floatLiteral.value);
            success = ZR_TRUE;
            break;
        }
        
        case ZR_AST_BOOLEAN_LITERAL: {
            ZrValueInitAsUInt(cs->state, result, node->data.booleanLiteral.value ? 1 : 0);
            result->type = ZR_VALUE_TYPE_BOOL;
            success = ZR_TRUE;
            break;
        }
        
        case ZR_AST_STRING_LITERAL: {
            if (node->data.stringLiteral.value != ZR_NULL) {
                ZrValueInitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(node->data.stringLiteral.value));
                result->type = ZR_VALUE_TYPE_STRING;
                success = ZR_TRUE;
            }
            break;
        }
        
        case ZR_AST_NULL_LITERAL: {
            ZrValueResetAsNull(result);
            success = ZR_TRUE;
            break;
        }
        
        case ZR_AST_IDENTIFIER_LITERAL: {
            // 查找编译期变量
            SZrString *name = node->data.identifier.name;
            if (name != ZR_NULL && check_compile_time_identifier_access(cs, name, ZR_FALSE)) {
                // 查找编译期变量值
                if (cs->compileTimeVariables.isValid && cs->compileTimeVariables.length > 0) {
                    for (TZrSize i = 0; i < cs->compileTimeVariables.length; i++) {
                        SZrCompileTimeVariable **varPtr = (SZrCompileTimeVariable**)ZrArrayGet(&cs->compileTimeVariables, i);
                        if (varPtr != ZR_NULL && *varPtr != ZR_NULL) {
                            if (ZrStringEqual((*varPtr)->name, name)) {
                                // 递归求值变量值
                                if ((*varPtr)->value != ZR_NULL) {
                                    success = evaluate_compile_time_expression(cs, (*varPtr)->value, result);
                                }
                                break;
                            }
                        }
                    }
                }
            }
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            // 编译期二元表达式求值
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            SZrTypeValue leftValue, rightValue;
            
            if (evaluate_compile_time_expression(cs, binExpr->left, &leftValue) &&
                evaluate_compile_time_expression(cs, binExpr->right, &rightValue)) {
                // 根据操作符计算结果
                const TChar *op = binExpr->op.op;
                if (strcmp(op, "+") == 0) {
                    if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
                        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                            TInt64 left = leftValue.value.nativeObject.nativeInt64;
                            TInt64 right = rightValue.value.nativeObject.nativeInt64;
                            ZrValueInitAsInt(cs->state, result, left + right);
                            success = ZR_TRUE;
                        } else {
                            TDouble left = leftValue.value.nativeObject.nativeDouble;
                            TDouble right = rightValue.value.nativeObject.nativeDouble;
                            ZrValueInitAsFloat(cs->state, result, left + right);
                            success = ZR_TRUE;
                        }
                    }
                } else if (strcmp(op, "-") == 0) {
                    if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
                        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                            TInt64 left = leftValue.value.nativeObject.nativeInt64;
                            TInt64 right = rightValue.value.nativeObject.nativeInt64;
                            ZrValueInitAsInt(cs->state, result, left - right);
                            success = ZR_TRUE;
                        } else {
                            TDouble left = leftValue.value.nativeObject.nativeDouble;
                            TDouble right = rightValue.value.nativeObject.nativeDouble;
                            ZrValueInitAsFloat(cs->state, result, left - right);
                            success = ZR_TRUE;
                        }
                    }
                } else if (strcmp(op, "*") == 0) {
                    if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
                        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                            TInt64 left = leftValue.value.nativeObject.nativeInt64;
                            TInt64 right = rightValue.value.nativeObject.nativeInt64;
                            ZrValueInitAsInt(cs->state, result, left * right);
                            success = ZR_TRUE;
                        } else {
                            TDouble left = leftValue.value.nativeObject.nativeDouble;
                            TDouble right = rightValue.value.nativeObject.nativeDouble;
                            ZrValueInitAsFloat(cs->state, result, left * right);
                            success = ZR_TRUE;
                        }
                    }
                } else if (strcmp(op, "/") == 0) {
                    if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
                        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                            TInt64 left = leftValue.value.nativeObject.nativeInt64;
                            TInt64 right = rightValue.value.nativeObject.nativeInt64;
                            if (right != 0) {
                                ZrValueInitAsInt(cs->state, result, left / right);
                                success = ZR_TRUE;
                            }
                        } else {
                            TDouble left = leftValue.value.nativeObject.nativeDouble;
                            TDouble right = rightValue.value.nativeObject.nativeDouble;
                            if (right != 0.0) {
                                ZrValueInitAsFloat(cs->state, result, left / right);
                                success = ZR_TRUE;
                            }
                        }
                    }
                } else if (strcmp(op, "==") == 0) {
                    TBool equal = ZR_FALSE;
                    if (leftValue.type == rightValue.type) {
                        if (ZR_VALUE_IS_TYPE_INT(leftValue.type)) {
                            equal = (leftValue.value.nativeObject.nativeInt64 == rightValue.value.nativeObject.nativeInt64);
                        } else if (ZR_VALUE_IS_TYPE_FLOAT(leftValue.type)) {
                            equal = (leftValue.value.nativeObject.nativeDouble == rightValue.value.nativeObject.nativeDouble);
                        } else if (leftValue.type == ZR_VALUE_TYPE_BOOL) {
                            equal = (leftValue.value.nativeObject.nativeBool != 0) == (rightValue.value.nativeObject.nativeBool != 0);
                        }
                    }
                    ZrValueInitAsUInt(cs->state, result, equal ? 1 : 0);
                    result->type = ZR_VALUE_TYPE_BOOL;
                    success = ZR_TRUE;
                } else if (strcmp(op, "!=") == 0) {
                    TBool notEqual = ZR_TRUE;
                    if (leftValue.type == rightValue.type) {
                        if (ZR_VALUE_IS_TYPE_INT(leftValue.type)) {
                            notEqual = (leftValue.value.nativeObject.nativeInt64 != rightValue.value.nativeObject.nativeInt64);
                        } else if (ZR_VALUE_IS_TYPE_FLOAT(leftValue.type)) {
                            notEqual = (leftValue.value.nativeObject.nativeDouble != rightValue.value.nativeObject.nativeDouble);
                        } else if (leftValue.type == ZR_VALUE_TYPE_BOOL) {
                            notEqual = (leftValue.value.nativeObject.nativeBool != 0) != (rightValue.value.nativeObject.nativeBool != 0);
                        }
                    }
                    ZrValueInitAsUInt(cs->state, result, notEqual ? 1 : 0);
                    result->type = ZR_VALUE_TYPE_BOOL;
                    success = ZR_TRUE;
                } else if (strcmp(op, "%") == 0) {
                    // 取模运算
                    if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                        TInt64 left = leftValue.value.nativeObject.nativeInt64;
                        TInt64 right = rightValue.value.nativeObject.nativeInt64;
                        if (right != 0) {
                            ZrValueInitAsInt(cs->state, result, left % right);
                            success = ZR_TRUE;
                        }
                    }
                } else if (strcmp(op, "<") == 0) {
                    // 小于比较
                    TBool less = ZR_FALSE;
                    if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
                        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                            less = (leftValue.value.nativeObject.nativeInt64 < rightValue.value.nativeObject.nativeInt64);
                        } else {
                            less = (leftValue.value.nativeObject.nativeDouble < rightValue.value.nativeObject.nativeDouble);
                        }
                    }
                    ZrValueInitAsUInt(cs->state, result, less ? 1 : 0);
                    result->type = ZR_VALUE_TYPE_BOOL;
                    success = ZR_TRUE;
                } else if (strcmp(op, ">") == 0) {
                    // 大于比较
                    TBool greater = ZR_FALSE;
                    if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
                        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                            greater = (leftValue.value.nativeObject.nativeInt64 > rightValue.value.nativeObject.nativeInt64);
                        } else {
                            greater = (leftValue.value.nativeObject.nativeDouble > rightValue.value.nativeObject.nativeDouble);
                        }
                    }
                    ZrValueInitAsUInt(cs->state, result, greater ? 1 : 0);
                    result->type = ZR_VALUE_TYPE_BOOL;
                    success = ZR_TRUE;
                } else if (strcmp(op, "<=") == 0) {
                    // 小于等于比较
                    TBool lessEqual = ZR_FALSE;
                    if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
                        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                            lessEqual = (leftValue.value.nativeObject.nativeInt64 <= rightValue.value.nativeObject.nativeInt64);
                        } else {
                            lessEqual = (leftValue.value.nativeObject.nativeDouble <= rightValue.value.nativeObject.nativeDouble);
                        }
                    }
                    ZrValueInitAsUInt(cs->state, result, lessEqual ? 1 : 0);
                    result->type = ZR_VALUE_TYPE_BOOL;
                    success = ZR_TRUE;
                } else if (strcmp(op, ">=") == 0) {
                    // 大于等于比较
                    TBool greaterEqual = ZR_FALSE;
                    if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
                        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                            greaterEqual = (leftValue.value.nativeObject.nativeInt64 >= rightValue.value.nativeObject.nativeInt64);
                        } else {
                            greaterEqual = (leftValue.value.nativeObject.nativeDouble >= rightValue.value.nativeObject.nativeDouble);
                        }
                    }
                    ZrValueInitAsUInt(cs->state, result, greaterEqual ? 1 : 0);
                    result->type = ZR_VALUE_TYPE_BOOL;
                    success = ZR_TRUE;
                } else if (strcmp(op, "&&") == 0) {
                    // 逻辑与
                    TBool leftBool = (leftValue.type == ZR_VALUE_TYPE_BOOL && leftValue.value.nativeObject.nativeBool != 0);
                    TBool rightBool = (rightValue.type == ZR_VALUE_TYPE_BOOL && rightValue.value.nativeObject.nativeBool != 0);
                    ZrValueInitAsUInt(cs->state, result, (leftBool && rightBool) ? 1 : 0);
                    result->type = ZR_VALUE_TYPE_BOOL;
                    success = ZR_TRUE;
                } else if (strcmp(op, "||") == 0) {
                    // 逻辑或
                    TBool leftBool = (leftValue.type == ZR_VALUE_TYPE_BOOL && leftValue.value.nativeObject.nativeBool != 0);
                    TBool rightBool = (rightValue.type == ZR_VALUE_TYPE_BOOL && rightValue.value.nativeObject.nativeBool != 0);
                    ZrValueInitAsUInt(cs->state, result, (leftBool || rightBool) ? 1 : 0);
                    result->type = ZR_VALUE_TYPE_BOOL;
                    success = ZR_TRUE;
                }
                // 支持更多操作符（已支持基本算术、比较和逻辑运算）
            }
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            // 编译期函数调用
            ZR_UNUSED_PARAMETER(&node->data.functionCall);
            
            // 检查是否是内置编译期函数（FatalError, Assert）
            // 注意：函数调用在 primary expression 中，这里需要从上下文中获取函数名
            // 实现完整的编译期函数调用
            // 注意：编译期函数调用需要：
            // 1. 求值参数
            // 2. 查找编译期函数定义
            // 3. 执行函数体
            // 4. 返回结果
            // TODO: 由于函数调用通常在primary expression中处理，这里暂时跳过
            // 完整的编译期函数调用在primary expression求值时实现
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            // 处理编译期函数调用（如 FatalError, Assert）
            SZrPrimaryExpression *primary = &node->data.primaryExpression;
            if (primary->property != ZR_NULL && primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                SZrString *funcName = primary->property->data.identifier.name;
                if (funcName != ZR_NULL) {
                    TNativeString nameStr = ZrStringGetNativeString(funcName);
                    if (nameStr != ZR_NULL) {
                        // 检查是否是 FatalError 或 Assert
                        if (strcmp(nameStr, "FatalError") == 0) {
                            // FatalError(message)
                            if (primary->members != ZR_NULL && primary->members->count > 0) {
                                SZrAstNode *callNode = primary->members->nodes[0];
                                if (callNode != ZR_NULL && callNode->type == ZR_AST_FUNCTION_CALL) {
                                    SZrFunctionCall *funcCall = &callNode->data.functionCall;
                                    if (funcCall->args != ZR_NULL && funcCall->args->count > 0) {
                                        SZrAstNode *argNode = funcCall->args->nodes[0];
                                        if (argNode != ZR_NULL && argNode->type == ZR_AST_STRING_LITERAL) {
                                            SZrString *msg = argNode->data.stringLiteral.value;
                                            TNativeString msgStr = ZrStringGetNativeString(msg);
                                            if (msgStr != ZR_NULL) {
                                                ZrCompileTimeError(cs, ZR_COMPILE_TIME_ERROR_FATAL, msgStr, node->location);
                                                success = ZR_FALSE;  // 致命错误，返回失败
                                            }
                                        }
                                    }
                                }
                            }
                        } else if (strcmp(nameStr, "Assert") == 0) {
                            // Assert(condition, message?)
                            if (primary->members != ZR_NULL && primary->members->count > 0) {
                                SZrAstNode *callNode = primary->members->nodes[0];
                                if (callNode != ZR_NULL && callNode->type == ZR_AST_FUNCTION_CALL) {
                                    SZrFunctionCall *funcCall = &callNode->data.functionCall;
                                    if (funcCall->args != ZR_NULL && funcCall->args->count > 0) {
                                        // 求值第一个参数（条件）
                                        SZrTypeValue conditionValue;
                                        if (evaluate_compile_time_expression(cs, funcCall->args->nodes[0], &conditionValue)) {
                                            TBool condition = ZR_FALSE;
                                            if (conditionValue.type == ZR_VALUE_TYPE_BOOL) {
                                                condition = (conditionValue.value.nativeObject.nativeBool != 0);
                                            } else if (ZR_VALUE_IS_TYPE_INT(conditionValue.type)) {
                                                condition = (conditionValue.value.nativeObject.nativeInt64 != 0);
                                            }
                                            
                                            if (!condition) {
                                                // 断言失败
                                                const TChar *msg = "Assertion failed";
                                                if (funcCall->args->count > 1) {
                                                    SZrAstNode *msgNode = funcCall->args->nodes[1];
                                                    if (msgNode != ZR_NULL && msgNode->type == ZR_AST_STRING_LITERAL) {
                                                        SZrString *msgStr = msgNode->data.stringLiteral.value;
                                                        TNativeString msgNative = ZrStringGetNativeString(msgStr);
                                                        if (msgNative != ZR_NULL) {
                                                            msg = msgNative;
                                                        }
                                                    }
                                                }
                                                ZrCompileTimeError(cs, ZR_COMPILE_TIME_ERROR_FATAL, msg, node->location);
                                                success = ZR_FALSE;
                                            } else {
                                                success = ZR_TRUE;
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            // 查找编译期函数
                            if (cs->compileTimeFunctions.isValid && cs->compileTimeFunctions.length > 0) {
                                for (TZrSize i = 0; i < cs->compileTimeFunctions.length; i++) {
                                    SZrCompileTimeFunction **funcPtr = (SZrCompileTimeFunction**)ZrArrayGet(&cs->compileTimeFunctions, i);
                                    if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL) {
                                        if (ZrStringEqual((*funcPtr)->name, funcName)) {
                                            // 实现编译期函数调用
                                            // 需要：1. 求值参数 2. 执行函数体 3. 返回结果
                                            // 注意：编译期函数调用需要递归执行函数体
                                            // TODO: 这里暂时跳过，因为需要完整的函数执行环境
                                            // 未来可以实现编译期函数调用的完整支持
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            break;
        }
        
        default:
            // 不支持的类型
            break;
    }
    
    // 恢复上下文
    cs->isInCompileTimeContext = oldContext;
    
    return success;
}

// 执行编译期声明
ZR_PARSER_API TBool execute_compile_time_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_COMPILE_TIME_DECLARATION) {
        return ZR_FALSE;
    }
    
    SZrCompileTimeDeclaration *compileTimeDecl = &node->data.compileTimeDeclaration;
    SZrAstNode *declaration = compileTimeDecl->declaration;
    
    if (declaration == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 保存当前上下文
    TBool oldContext = cs->isInCompileTimeContext;
    cs->isInCompileTimeContext = ZR_TRUE;
    
    TBool success = ZR_FALSE;
    
    switch (compileTimeDecl->declarationType) {
        case ZR_COMPILE_TIME_VARIABLE: {
            // 编译期变量声明
            if (declaration->type == ZR_AST_VARIABLE_DECLARATION) {
                SZrVariableDeclaration *varDecl = &declaration->data.variableDeclaration;
                if (varDecl->pattern != ZR_NULL && varDecl->pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrString *varName = varDecl->pattern->data.identifier.name;
                    if (varName != ZR_NULL) {
                        // 创建编译期变量
                        SZrCompileTimeVariable *var = ZrMemoryRawMallocWithType(
                            cs->state->global, sizeof(SZrCompileTimeVariable), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        if (var != ZR_NULL) {
                            var->name = varName;
                            var->value = varDecl->value;
                            var->location = node->location;
                            
                            // 推断类型
                            if (varDecl->typeInfo != ZR_NULL) {
                                convert_ast_type_to_inferred_type(cs, varDecl->typeInfo, &var->type);
                            } else if (varDecl->value != ZR_NULL) {
                                infer_expression_type(cs, varDecl->value, &var->type);
                            } else {
                                ZrInferredTypeInit(cs->state, &var->type, ZR_VALUE_TYPE_OBJECT);
                            }
                            
                            // 添加到编译期变量表
                            ZrArrayPush(cs->state, &cs->compileTimeVariables, &var);
                            success = ZR_TRUE;
                        }
                    }
                }
            }
            break;
        }
        
        case ZR_COMPILE_TIME_FUNCTION: {
            // 编译期函数声明
            if (declaration->type == ZR_AST_FUNCTION_DECLARATION) {
                SZrFunctionDeclaration *funcDecl = &declaration->data.functionDeclaration;
                if (funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
                    // 创建编译期函数
                    SZrCompileTimeFunction *func = ZrMemoryRawMallocWithType(
                        cs->state->global, sizeof(SZrCompileTimeFunction), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    if (func != ZR_NULL) {
                        func->name = funcDecl->name->name;
                        func->declaration = declaration;
                        func->location = node->location;
                        
                        // 推断返回类型
                        if (funcDecl->returnType != ZR_NULL) {
                            convert_ast_type_to_inferred_type(cs, funcDecl->returnType, &func->returnType);
                        } else {
                            ZrInferredTypeInit(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);
                        }
                        
                        // 初始化参数类型数组
                        ZrArrayInit(cs->state, &func->paramTypes, sizeof(SZrInferredType*), 
                                   funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
                        
                        // 提取参数类型
                        if (funcDecl->params != ZR_NULL) {
                            for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                                SZrAstNode *paramNode = funcDecl->params->nodes[i];
                                if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                                    SZrParameter *param = &paramNode->data.parameter;
                                    SZrInferredType *paramType = ZrMemoryRawMallocWithType(
                                        cs->state->global, sizeof(SZrInferredType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                                    if (paramType != ZR_NULL) {
                                        if (param->typeInfo != ZR_NULL) {
                                            convert_ast_type_to_inferred_type(cs, param->typeInfo, paramType);
                                        } else {
                                            ZrInferredTypeInit(cs->state, paramType, ZR_VALUE_TYPE_OBJECT);
                                        }
                                        ZrArrayPush(cs->state, &func->paramTypes, &paramType);
                                    }
                                }
                            }
                        }
                        
                        // 添加到编译期函数表
                        ZrArrayPush(cs->state, &cs->compileTimeFunctions, &func);
                        success = ZR_TRUE;
                    }
                }
            }
            break;
        }
        
        case ZR_COMPILE_TIME_STATEMENT: {
            // 编译期语句块
            if (declaration->type == ZR_AST_BLOCK) {
                SZrBlock *block = &declaration->data.block;
                if (block->body != ZR_NULL) {
                    // 执行语句块中的每个语句
                    for (TZrSize i = 0; i < block->body->count; i++) {
                        SZrAstNode *stmt = block->body->nodes[i];
                        if (stmt != ZR_NULL) {
                            // 递归执行编译期声明
                            if (stmt->type == ZR_AST_COMPILE_TIME_DECLARATION) {
                                execute_compile_time_declaration(cs, stmt);
                            } else if (stmt->type == ZR_AST_EXPRESSION_STATEMENT) {
                                // 执行表达式语句（可能包含 FatalError 调用）
                                SZrTypeValue result;
                                if (evaluate_compile_time_expression(cs, stmt->data.expressionStatement.expr, &result)) {
                                    // 表达式求值成功
                                }
                            } else if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
                                // 变量声明：执行初始化表达式（如果有）
                                SZrVariableDeclaration *varDecl = &stmt->data.variableDeclaration;
                                if (varDecl->value != ZR_NULL) {
                                    SZrTypeValue initResult;
                                    if (evaluate_compile_time_expression(cs, varDecl->value, &initResult)) {
                                        // 初始化表达式求值成功
                                        // 注意：变量声明在编译期执行时不需要存储变量值
                                    }
                                }
                            } else if (stmt->type == ZR_AST_IF_EXPRESSION || stmt->type == ZR_AST_CONDITIONAL_EXPRESSION) {
                                // if表达式：根据条件执行相应分支
                                SZrIfExpression *ifExpr = &stmt->data.ifExpression;
                                if (ifExpr->condition != ZR_NULL) {
                                    SZrTypeValue condResult;
                                    if (evaluate_compile_time_expression(cs, ifExpr->condition, &condResult)) {
                                        TBool condition = (condResult.type == ZR_VALUE_TYPE_BOOL && 
                                                          condResult.value.nativeObject.nativeBool != 0);
                                        if (condition && ifExpr->thenExpr != ZR_NULL) {
                                            SZrTypeValue result;
                                            evaluate_compile_time_expression(cs, ifExpr->thenExpr, &result);
                                        } else if (!condition && ifExpr->elseExpr != ZR_NULL) {
                                            SZrTypeValue result;
                                            evaluate_compile_time_expression(cs, ifExpr->elseExpr, &result);
                                        }
                                    }
                                }
                            }
                            // 支持更多语句类型（已支持表达式语句、变量声明、if表达式）
                        }
                    }
                    success = ZR_TRUE;
                }
            }
            break;
        }
        
        case ZR_COMPILE_TIME_EXPRESSION: {
            // 编译期表达式
            SZrTypeValue result;
            success = evaluate_compile_time_expression(cs, declaration, &result);
            break;
        }
    }
    
    // 恢复上下文
    cs->isInCompileTimeContext = oldContext;
    
    return success;
}
