//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_string_conf.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

// 辅助函数：获取类型名称字符串（用于错误报告）
static const TChar *get_base_type_name(EZrValueType baseType) {
    switch (baseType) {
        case ZR_VALUE_TYPE_NULL:
            return "null";
        case ZR_VALUE_TYPE_BOOL:
            return "bool";
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            return "int";
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return "uint";
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return "float";
        case ZR_VALUE_TYPE_STRING:
            return "string";
        case ZR_VALUE_TYPE_ARRAY:
            return "array";
        case ZR_VALUE_TYPE_OBJECT:
            return "object";
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            return "function";
        default:
            return "unknown";
    }
}

// 获取类型名称字符串（用于错误报告）
const TChar *get_type_name_string(SZrState *state, const SZrInferredType *type, TChar *buffer, TZrSize bufferSize) {
    if (state == ZR_NULL || type == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return "unknown";
    }
    
    const TChar *baseName = get_base_type_name(type->baseType);
    
    // 如果有类型名（用户定义类型），使用类型名
    if (type->typeName != ZR_NULL) {
        TNativeString typeNameStr;
        TZrSize nameLen;
        if (type->typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            typeNameStr = ZrStringGetNativeStringShort(type->typeName);
            nameLen = type->typeName->shortStringLength;
        } else {
            typeNameStr = ZrStringGetNativeString(type->typeName);
            nameLen = type->typeName->longStringLength;
        }
        
        if (nameLen < bufferSize) {
            memcpy(buffer, typeNameStr, nameLen);
            buffer[nameLen] = '\0';
            return buffer;
        }
    }
    
    // 使用基础类型名
    TZrSize nameLen = strlen(baseName);
    if (nameLen < bufferSize) {
        memcpy(buffer, baseName, nameLen);
        buffer[nameLen] = '\0';
        if (type->isNullable) {
            if (nameLen + 6 < bufferSize) {
                memcpy(buffer + nameLen, "?", 1);
                buffer[nameLen + 1] = '\0';
            }
        }
        return buffer;
    }
    
    return baseName;
}

// 报告类型错误
void report_type_error(SZrCompilerState *cs, const TChar *message, const SZrInferredType *expectedType, const SZrInferredType *actualType, SZrFileRange location) {
    if (cs == ZR_NULL || message == ZR_NULL) {
        return;
    }
    
    static TChar errorMsg[512];
    static TChar expectedTypeStr[128];
    static TChar actualTypeStr[128];
    
    const TChar *expectedName = "unknown";
    const TChar *actualName = "unknown";
    
    if (expectedType != ZR_NULL) {
        expectedName = get_type_name_string(cs->state, expectedType, expectedTypeStr, sizeof(expectedTypeStr));
    }
    if (actualType != ZR_NULL) {
        actualName = get_type_name_string(cs->state, actualType, actualTypeStr, sizeof(actualTypeStr));
    }
    
    // 构建详细的错误消息，包含类型信息
    snprintf(errorMsg, sizeof(errorMsg), 
             "Type Error: %s (expected: %s, actual: %s). "
             "Check variable types, function signatures, and type annotations. "
             "Ensure the actual type is compatible with the expected type. "
             "Consider adding explicit type conversions if needed.",
             message, expectedName, actualName);
    
    ZrCompilerError(cs, errorMsg, location);
}

// 检查类型兼容性（用于赋值等场景）
TBool check_type_compatibility(SZrCompilerState *cs, const SZrInferredType *fromType, const SZrInferredType *toType, SZrFileRange location) {
    if (cs == ZR_NULL || fromType == ZR_NULL || toType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    if (ZrInferredTypeIsCompatible(fromType, toType)) {
        return ZR_TRUE;
    }
    
    // 类型不兼容，报告错误
    report_type_error(cs, "Type mismatch", toType, fromType, location);
    return ZR_FALSE;
}

// 检查赋值兼容性
TBool check_assignment_compatibility(SZrCompilerState *cs, const SZrInferredType *leftType, const SZrInferredType *rightType, SZrFileRange location) {
    if (cs == ZR_NULL || leftType == ZR_NULL || rightType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 首先检查基本类型兼容性
    if (!check_type_compatibility(cs, rightType, leftType, location)) {
        return ZR_FALSE;
    }
    
    // 检查范围约束（如果目标类型有范围约束）
    if (leftType->hasRangeConstraint) {
        // 对于字面量，在 check_literal_range 中已经检查
        // 这里主要检查变量赋值时的范围约束
        // 如果 rightType 是编译期常量，可以在这里检查
        // 注意：编译期常量检查需要在编译时进行，这里只做类型兼容性检查
        // 实际的常量值检查在编译期执行器中完成
    }
    
    // 检查数组大小约束
    if (leftType->hasArraySizeConstraint && rightType->baseType == ZR_VALUE_TYPE_ARRAY) {
        // 如果目标数组有固定大小，检查源数组大小是否匹配
        // 注意：这里只能检查字面量数组，变量数组需要在运行时检查
        // 对于数组字面量，需要在赋值时检查（通过检查右值表达式）
        // 在赋值表达式编译时调用 check_array_literal_size
        // 注意：这里只做类型检查，实际的数组大小检查在编译时进行
        // 如果源数组有固定大小，检查是否匹配
        if (rightType->hasArraySizeConstraint && rightType->arrayFixedSize > 0) {
            if (leftType->arrayFixedSize > 0 && leftType->arrayFixedSize != rightType->arrayFixedSize) {
                // 数组大小不匹配，但这里只做类型检查，不报告错误
                // 错误报告在编译时进行
            }
        }
    }
    
    return ZR_TRUE;
}

// 检查函数调用参数兼容性
TBool check_function_call_compatibility(SZrCompilerState *cs, SZrFunctionTypeInfo *funcType, SZrAstNodeArray *args, SZrFileRange location) {
    ZR_UNUSED_PARAMETER(location);
    if (cs == ZR_NULL || funcType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 实现参数类型检查
    // 1. 检查参数数量
    TZrSize paramCount = funcType->paramTypes.length;
    TZrSize argCount = (args != ZR_NULL) ? args->count : 0;
    
    // 检查参数数量是否匹配（考虑可变参数）
    // TODO: 注意：可变参数检查需要函数类型信息支持，这里简化处理
    if (argCount < paramCount) {
        // 参数数量不足（除非有默认参数）
        // TODO: 这里暂时不报告错误，因为可能有默认参数
    }
    
    // 2. 检查每个参数的类型兼容性
    if (args != ZR_NULL && funcType->paramTypes.length > 0) {
        TZrSize minCount = (paramCount < argCount) ? paramCount : argCount;
        for (TZrSize i = 0; i < minCount; i++) {
            SZrAstNode *argNode = args->nodes[i];
            if (argNode != ZR_NULL) {
                SZrInferredType argType;
                if (infer_expression_type(cs, argNode, &argType)) {
                    SZrInferredType *paramType = (SZrInferredType *)ZrArrayGet(&funcType->paramTypes, i);
                    if (paramType != ZR_NULL) {
                        // 检查类型兼容性
                        if (!ZrInferredTypeIsCompatible(&argType, paramType)) {
                            // 类型不兼容，报告错误
                            report_type_error(cs, "Argument type mismatch", paramType, &argType, argNode->location);
                            ZrInferredTypeFree(cs->state, &argType);
                            return ZR_FALSE;
                        }
                    }
                    ZrInferredTypeFree(cs->state, &argType);
                }
            }
        }
    }
    // 3. 支持默认参数
    
    return ZR_TRUE;
}

// 从字面量推断类型
TBool infer_literal_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_BOOL);
            return ZR_TRUE;
            
        case ZR_AST_INTEGER_LITERAL: {
            // 根据值大小选择类型
            // 可以根据值的大小选择更小的类型
            TInt64 value = node->data.integerLiteral.value;
            EZrValueType intType = ZR_VALUE_TYPE_INT64;
            
            // 根据值的大小选择最小的合适类型
            if (value >= -128 && value <= 127) {
                intType = ZR_VALUE_TYPE_INT8;
            } else if (value >= -32768 && value <= 32767) {
                intType = ZR_VALUE_TYPE_INT16;
            } else if (value >= -2147483648LL && value <= 2147483647LL) {
                intType = ZR_VALUE_TYPE_INT32;
            } else {
                intType = ZR_VALUE_TYPE_INT64;
            }
            
            ZrInferredTypeInit(cs->state, result, intType);
            return ZR_TRUE;
        }
            
        case ZR_AST_FLOAT_LITERAL:
            // 默认使用DOUBLE
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_DOUBLE);
            return ZR_TRUE;
            
        case ZR_AST_STRING_LITERAL:
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_STRING);
            return ZR_TRUE;
            
        case ZR_AST_CHAR_LITERAL:
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_INT8);
            return ZR_TRUE;
            
        case ZR_AST_NULL_LITERAL:
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_NULL);
            result->isNullable = ZR_TRUE;
            return ZR_TRUE;
            
        default:
            return ZR_FALSE;
    }
}

// 从标识符推断类型
TBool infer_identifier_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }
    
    SZrString *name = node->data.identifier.name;
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 在类型环境中查找变量类型
    if (cs->typeEnv != ZR_NULL) {
        if (ZrTypeEnvironmentLookupVariable(cs->state, cs->typeEnv, name, result)) {
            return ZR_TRUE;
        }
    }
    
    // 未找到变量类型，不立即报错
    // 可能是全局对象 zr 的属性访问，或者子函数，或者全局对象的其他属性
    // 返回默认的 OBJECT 类型，让 compile_identifier 继续处理
    // compile_identifier 会尝试作为全局对象属性访问、子函数访问等
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从一元表达式推断类型
TBool infer_unary_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    const TChar *op = node->data.unaryExpression.op.op;
    SZrAstNode *arg = node->data.unaryExpression.argument;
    
    // 推断操作数类型
    SZrInferredType argType;
    if (!infer_expression_type(cs, arg, &argType)) {
        return ZR_FALSE;
    }
    
    if (strcmp(op, "!") == 0) {
        // 逻辑非：结果类型是bool
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_BOOL);
        return ZR_TRUE;
    } else if (strcmp(op, "~") == 0) {
        // 位非：结果类型与操作数类型相同（整数类型）
        ZrInferredTypeCopy(cs->state, result, &argType);
        return ZR_TRUE;
    } else if (strcmp(op, "-") == 0 || strcmp(op, "+") == 0) {
        // 取负/正号：结果类型与操作数类型相同
        ZrInferredTypeCopy(cs->state, result, &argType);
        ZrInferredTypeFree(cs->state, &argType);
        return ZR_TRUE;
    }
    
    ZrInferredTypeFree(cs->state, &argType);
    return ZR_FALSE;
}

// 从二元表达式推断类型
TBool infer_binary_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    const TChar *op = node->data.binaryExpression.op.op;
    SZrAstNode *left = node->data.binaryExpression.left;
    SZrAstNode *right = node->data.binaryExpression.right;
    
    // 推断左右操作数类型
    SZrInferredType leftType, rightType;
    if (!infer_expression_type(cs, left, &leftType) || !infer_expression_type(cs, right, &rightType)) {
        if (infer_expression_type(cs, left, &leftType)) {
            ZrInferredTypeFree(cs->state, &leftType);
        }
        return ZR_FALSE;
    }
    
    // 根据操作符确定结果类型
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || 
        strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || 
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
        // 比较运算符：结果类型是bool
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrInferredTypeFree(cs->state, &leftType);
        ZrInferredTypeFree(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        // 逻辑运算符：结果类型是bool
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrInferredTypeFree(cs->state, &leftType);
        ZrInferredTypeFree(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || 
               strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || 
               strcmp(op, "%") == 0 || strcmp(op, "**") == 0) {
        // 算术运算符：获取公共类型（类型提升）
        if (!ZrInferredTypeGetCommonType(cs->state, result, &leftType, &rightType)) {
            // 类型不兼容，报告错误
            report_type_error(cs, "Incompatible types for arithmetic operation", &leftType, &rightType, node->location);
            ZrInferredTypeFree(cs->state, &leftType);
            ZrInferredTypeFree(cs->state, &rightType);
            return ZR_FALSE;
        }
        ZrInferredTypeFree(cs->state, &leftType);
        ZrInferredTypeFree(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0 ||
               strcmp(op, "&") == 0 || strcmp(op, "|") == 0 || strcmp(op, "^") == 0) {
        // 位运算符：结果类型与左操作数类型相同（整数类型）
        ZrInferredTypeCopy(cs->state, result, &leftType);
        ZrInferredTypeFree(cs->state, &leftType);
        ZrInferredTypeFree(cs->state, &rightType);
        return ZR_TRUE;
    }
    
    ZrInferredTypeFree(cs->state, &leftType);
    ZrInferredTypeFree(cs->state, &rightType);
    return ZR_FALSE;
}

// 从函数调用推断类型
TBool infer_function_call_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_FUNCTION_CALL) {
        return ZR_FALSE;
    }
    
    ZR_UNUSED_PARAMETER(&node->data.functionCall);
    
    // 注意：函数调用在 PRIMARY_EXPRESSION 中处理
    // SZrFunctionCall 只有 args 字段，被调用的表达式在 PRIMARY_EXPRESSION 的 property 中
    // 这个函数应该从 PRIMARY_EXPRESSION 调用，而不是直接从 FUNCTION_CALL 调用
    // 如果直接调用，无法获取被调用的表达式，返回默认对象类型
    
    // 尝试从类型环境查找函数类型（如果函数名可以从上下文中推断）
    // 未来可以从 PRIMARY_EXPRESSION 中获取被调用的表达式进行类型推断
    // 注意：SZrFunctionCall没有callee成员，函数调用在primary expression中处理
    // TODO: 这里暂时跳过，因为函数调用的类型推断在infer_primary_expression_type中处理
    // 默认返回对象类型
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从Lambda表达式推断类型
TBool infer_lambda_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_LAMBDA_EXPRESSION) {
        return ZR_FALSE;
    }
    
    // Lambda表达式返回函数类型
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_CLOSURE);
    return ZR_TRUE;
}

// 检查数组字面量大小是否符合约束
static TBool check_array_literal_size(SZrCompilerState *cs, SZrAstNode *arrayLiteralNode, const SZrInferredType *targetType, SZrFileRange location) {
    if (cs == ZR_NULL || arrayLiteralNode == ZR_NULL || targetType == ZR_NULL || 
        arrayLiteralNode->type != ZR_AST_ARRAY_LITERAL) {
        return ZR_FALSE;
    }
    
    if (!targetType->hasArraySizeConstraint) {
        return ZR_TRUE;  // 没有大小约束，通过
    }
    
    SZrArrayLiteral *arrayLiteral = &arrayLiteralNode->data.arrayLiteral;
    TZrSize arraySize = (arrayLiteral->elements != ZR_NULL) ? arrayLiteral->elements->count : 0;
    
    // 检查固定大小
    if (targetType->arrayFixedSize > 0) {
        if (arraySize != targetType->arrayFixedSize) {
            static TChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array literal size mismatch: expected %zu elements, got %zu",
                    targetType->arrayFixedSize, arraySize);
            report_type_error(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
    }
    
    // 检查范围约束
    if (targetType->arrayMinSize > 0) {
        if (arraySize < targetType->arrayMinSize) {
            static TChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array literal size too small: expected at least %zu elements, got %zu",
                    targetType->arrayMinSize, arraySize);
            report_type_error(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
    }
    
    if (targetType->arrayMaxSize > 0) {
        if (arraySize > targetType->arrayMaxSize) {
            static TChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array literal size too large: expected at most %zu elements, got %zu",
                    targetType->arrayMaxSize, arraySize);
            report_type_error(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
    }
    
    return ZR_TRUE;
}

// 从数组字面量推断类型
TBool infer_array_literal_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_ARRAY_LITERAL) {
        return ZR_FALSE;
    }
    
    // 数组字面量返回数组类型
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_ARRAY);
    
    // 推断元素类型（如果需要）
    // TODO: 注意：元素类型推断需要遍历数组元素，这里暂时跳过
    // 未来可以实现元素类型推断
    // 1. 推断所有元素类型
    // 2. 找到公共类型
    // 3. 设置elementTypes
    
    return ZR_TRUE;
}

// 从对象字面量推断类型
TBool infer_object_literal_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_OBJECT_LITERAL) {
        return ZR_FALSE;
    }
    
    // 对象字面量返回对象类型
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从条件表达式推断类型
TBool infer_conditional_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_CONDITIONAL_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrConditionalExpression *condExpr = &node->data.conditionalExpression;
    
    // 推断then和else分支类型
    SZrInferredType thenType, elseType;
    if (!infer_expression_type(cs, condExpr->consequent, &thenType) || 
        !infer_expression_type(cs, condExpr->alternate, &elseType)) {
        if (infer_expression_type(cs, condExpr->consequent, &thenType)) {
            ZrInferredTypeFree(cs->state, &thenType);
        }
        return ZR_FALSE;
    }
    
    // 获取公共类型
    if (!ZrInferredTypeGetCommonType(cs->state, result, &thenType, &elseType)) {
        // 类型不兼容，报告错误
        report_type_error(cs, "Incompatible types in conditional expression branches", &thenType, &elseType, node->location);
        ZrInferredTypeFree(cs->state, &thenType);
        ZrInferredTypeFree(cs->state, &elseType);
        return ZR_FALSE;
    }
    
    ZrInferredTypeFree(cs->state, &thenType);
    ZrInferredTypeFree(cs->state, &elseType);
    return ZR_TRUE;
}

// 从赋值表达式推断类型
TBool infer_assignment_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
    
    // 推断右值类型
    if (!infer_expression_type(cs, assignExpr->right, result)) {
        return ZR_FALSE;
    }
    
    // 检查与左值类型的兼容性
    // 1. 推断左值类型
    SZrInferredType leftType;
    if (infer_expression_type(cs, assignExpr->left, &leftType)) {
        // 2. 检查类型兼容性
        if (!ZrInferredTypeIsCompatible(result, &leftType)) {
            // 3. 报告错误如果不兼容
            report_type_error(cs, "Assignment type mismatch", &leftType, result, node->location);
            ZrInferredTypeFree(cs->state, &leftType);
            return ZR_FALSE;
        }
        ZrInferredTypeFree(cs->state, &leftType);
    }
    
    return ZR_TRUE;
}

// 从primary expression推断类型（包括函数调用）
TBool infer_primary_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    
    // 如果没有members，直接推断property的类型
    if (primary->members == ZR_NULL || primary->members->count == 0) {
        if (primary->property != ZR_NULL) {
            return infer_expression_type(cs, primary->property, result);
        }
        // 如果没有property，返回对象类型
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }
    
    // 检查是否是成员方法调用：obj.method()
    // 需要：members[0] 是 MemberExpression，members[1] 是 FunctionCall
    if (primary->members->count >= 2) {
        SZrAstNode *firstMember = primary->members->nodes[0];
        SZrAstNode *secondMember = primary->members->nodes[1];
        
        if (firstMember != ZR_NULL && firstMember->type == ZR_AST_MEMBER_EXPRESSION &&
            secondMember != ZR_NULL && secondMember->type == ZR_AST_FUNCTION_CALL) {
            // 成员方法调用：obj.method()
            SZrMemberExpression *memberExpr = &firstMember->data.memberExpression;
            
            // 从 MemberExpression 中提取方法名
            if (memberExpr->property != ZR_NULL && memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                // 推断 property (obj) 的类型
                if (primary->property != ZR_NULL) {
                    SZrInferredType objType;
                    if (infer_expression_type(cs, primary->property, &objType)) {
                        // 对于对象类型，方法调用返回对象类型
                        // 未来可以查找对象类型的方法定义来获取精确的返回类型
                        // 目前需要结构体/类的类型信息来查找方法，这是更高级的功能
                        // 如果objType有typeName，可以尝试从类型环境查找方法定义
                        if (objType.typeName != ZR_NULL && cs->typeEnv != ZR_NULL) {
                            // 可以尝试查找类型的方法定义，但需要更复杂的类型系统支持
                            // TODO: 暂时使用默认的对象类型
                        }
                        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                        ZrInferredTypeFree(cs->state, &objType);
                        return ZR_TRUE;
                    }
                    ZrInferredTypeFree(cs->state, &objType);
                }
            }
        }
    }
    
    // 检查第一个member是否是函数调用：foo()
    SZrAstNode *firstMember = primary->members->nodes[0];
    if (firstMember != ZR_NULL && firstMember->type == ZR_AST_FUNCTION_CALL) {
        // 函数调用：从property中提取函数名
        if (primary->property != ZR_NULL && primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
            SZrString *funcName = primary->property->data.identifier.name;
            if (funcName != ZR_NULL && cs->typeEnv != ZR_NULL) {
                // 在类型环境中查找函数类型
                SZrFunctionTypeInfo *funcTypeInfo = ZR_NULL;
                if (ZrTypeEnvironmentLookupFunction(cs->typeEnv, funcName, &funcTypeInfo)) {
                    if (funcTypeInfo != ZR_NULL) {
                        // 复制返回类型
                        ZrInferredTypeCopy(cs->state, result, &funcTypeInfo->returnType);
                        
                        // 检查参数兼容性（可选）
                        SZrFunctionCall *call = &firstMember->data.functionCall;
                        if (call->args != ZR_NULL) {
                            check_function_call_compatibility(cs, funcTypeInfo, call->args, node->location);
                        }
                        
                        return ZR_TRUE;
                    }
                }
                
                // 函数未找到，检查是否是 struct 构造函数调用
                if (ZrTypeEnvironmentLookupType(cs->typeEnv, funcName)) {
                    // 找到类型名称，推断返回类型为对应的 struct 类型
                    ZrInferredTypeInitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, funcName);
                    return ZR_TRUE;
                }
                
                // 函数和类型都未找到，报告错误
                static TChar errorMsg[256];
                TNativeString nameStr;
                TZrSize nameLen;
                if (funcName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                    nameStr = ZrStringGetNativeStringShort(funcName);
                    nameLen = funcName->shortStringLength;
                } else {
                    nameStr = ZrStringGetNativeString(funcName);
                    nameLen = funcName->longStringLength;
                }
                snprintf(errorMsg, sizeof(errorMsg), "Function '%.*s' not found in type environment", (int)nameLen, nameStr);
                ZrCompilerError(cs, errorMsg, node->location);
                ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE; // 返回对象类型作为fallback
            }
        }
        
        // property不是标识符，或者函数名未找到，返回对象类型
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }
    
    // 不是函数调用，或者是成员访问等其他情况
    // 先推断property的类型，然后根据members推断最终类型
    // 实现完整的成员访问链类型推断（如 obj.prop）
    if (primary->property != ZR_NULL) {
        SZrInferredType baseType;
        if (infer_expression_type(cs, primary->property, &baseType)) {
            // 如果有members，需要根据members推断最终类型
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                // 遍历members链，逐步推断类型
                SZrInferredType currentType = baseType;
                for (TZrSize i = 0; i < primary->members->count; i++) {
                    SZrAstNode *member = primary->members->nodes[i];
                    if (member != ZR_NULL && member->type == ZR_AST_MEMBER_EXPRESSION) {
                        // 成员访问：从当前类型推断成员类型
                        // TODO: 注意：这需要类型系统支持，暂时返回对象类型
                        ZrInferredTypeFree(cs->state, &currentType);
                        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                        return ZR_TRUE;
                    }
                }
                ZrInferredTypeCopy(cs->state, result, &currentType);
                ZrInferredTypeFree(cs->state, &currentType);
                return ZR_TRUE;
            } else {
                // 没有members，直接返回property的类型
                ZrInferredTypeCopy(cs->state, result, &baseType);
                ZrInferredTypeFree(cs->state, &baseType);
                return ZR_TRUE;
            }
        }
    }
    
    // 默认返回对象类型
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从AST节点推断类型（主入口函数）
TBool infer_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    switch (node->type) {
        // 字面量
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            return infer_literal_type(cs, node, result);
        
        case ZR_AST_IDENTIFIER_LITERAL:
            return infer_identifier_type(cs, node, result);
        
        // 表达式
        case ZR_AST_BINARY_EXPRESSION:
            return infer_binary_expression_type(cs, node, result);
        
        case ZR_AST_UNARY_EXPRESSION:
            return infer_unary_expression_type(cs, node, result);
        
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return infer_conditional_type(cs, node, result);
        
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return infer_assignment_type(cs, node, result);
        
        case ZR_AST_FUNCTION_CALL:
            return infer_function_call_type(cs, node, result);
        
        case ZR_AST_LAMBDA_EXPRESSION:
            return infer_lambda_type(cs, node, result);
        
        case ZR_AST_ARRAY_LITERAL:
            return infer_array_literal_type(cs, node, result);
        
        case ZR_AST_OBJECT_LITERAL:
            return infer_object_literal_type(cs, node, result);
        
        // TODO: 处理其他表达式类型
        case ZR_AST_PRIMARY_EXPRESSION:
            return infer_primary_expression_type(cs, node, result);
        
        case ZR_AST_MEMBER_EXPRESSION:
            // 实现member expression的类型推断
            // member expression的类型推断需要知道对象类型和成员名称
            // TODO: 这里简化处理，返回对象类型
            // 完整的实现需要从对象类型查找成员定义
            // TODO: 注意：member expression的类型推断需要知道对象类型，暂时返回对象类型
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            return ZR_TRUE;
        
        case ZR_AST_IF_EXPRESSION:
            // 实现if expression的类型推断
            // if expression的类型是thenExpr和elseExpr的公共类型
            {
                SZrIfExpression *ifExpr = &node->data.ifExpression;
                SZrInferredType thenType, elseType;
                if (ifExpr->thenExpr != ZR_NULL && ifExpr->elseExpr != ZR_NULL) {
                    if (infer_expression_type(cs, ifExpr->thenExpr, &thenType) &&
                        infer_expression_type(cs, ifExpr->elseExpr, &elseType)) {
                        // 获取公共类型
                        if (ZrInferredTypeGetCommonType(cs->state, result, &thenType, &elseType)) {
                            ZrInferredTypeFree(cs->state, &thenType);
                            ZrInferredTypeFree(cs->state, &elseType);
                            return ZR_TRUE;
                        }
                        ZrInferredTypeFree(cs->state, &thenType);
                        ZrInferredTypeFree(cs->state, &elseType);
                    }
                } else if (ifExpr->thenExpr != ZR_NULL) {
                    return infer_expression_type(cs, ifExpr->thenExpr, result);
                } else if (ifExpr->elseExpr != ZR_NULL) {
                    return infer_expression_type(cs, ifExpr->elseExpr, result);
                }
                // 默认返回对象类型
                ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        
        case ZR_AST_SWITCH_EXPRESSION:
            // 实现switch expression的类型推断
            // switch expression的类型是所有case和default的公共类型
            {
                SZrSwitchExpression *switchExpr = &node->data.switchExpression;
                SZrInferredType commonType;
                TBool hasType = ZR_FALSE;
                
                // 遍历所有case，推断类型
                if (switchExpr->cases != ZR_NULL) {
                    for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
                        SZrAstNode *caseNode = switchExpr->cases->nodes[i];
                        if (caseNode != ZR_NULL && caseNode->type == ZR_AST_SWITCH_CASE) {
                            SZrSwitchCase *switchCase = &caseNode->data.switchCase;
                            if (switchCase->block != ZR_NULL) {
                                SZrInferredType caseType;
                                if (infer_expression_type(cs, switchCase->block, &caseType)) {
                                    if (!hasType) {
                                        ZrInferredTypeCopy(cs->state, &commonType, &caseType);
                                        hasType = ZR_TRUE;
                                    } else {
                                        SZrInferredType newCommonType;
                                        if (ZrInferredTypeGetCommonType(cs->state, &newCommonType, &commonType, &caseType)) {
                                            ZrInferredTypeFree(cs->state, &commonType);
                                            commonType = newCommonType;
                                        }
                                    }
                                    ZrInferredTypeFree(cs->state, &caseType);
                                }
                            }
                        }
                    }
                }
                
                // 处理default case
                if (switchExpr->defaultCase != ZR_NULL && switchExpr->defaultCase->type == ZR_AST_SWITCH_DEFAULT) {
                    SZrSwitchDefault *switchDefault = &switchExpr->defaultCase->data.switchDefault;
                    if (switchDefault->block != ZR_NULL) {
                        SZrInferredType defaultType;
                        if (infer_expression_type(cs, switchDefault->block, &defaultType)) {
                            if (!hasType) {
                                ZrInferredTypeCopy(cs->state, &commonType, &defaultType);
                                hasType = ZR_TRUE;
                            } else {
                                SZrInferredType newCommonType;
                                if (ZrInferredTypeGetCommonType(cs->state, &newCommonType, &commonType, &defaultType)) {
                                    ZrInferredTypeFree(cs->state, &commonType);
                                    commonType = newCommonType;
                                }
                            }
                            ZrInferredTypeFree(cs->state, &defaultType);
                        }
                    }
                }
                
                if (hasType) {
                    ZrInferredTypeCopy(cs->state, result, &commonType);
                    ZrInferredTypeFree(cs->state, &commonType);
                    return ZR_TRUE;
                }
                
                // 默认返回对象类型
                ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        
        default:
            return ZR_FALSE;
    }
}

// 将AST类型注解转换为推断类型
TBool convert_ast_type_to_inferred_type(SZrCompilerState *cs, const SZrType *astType, SZrInferredType *result) {
    if (cs == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 如果没有类型注解，返回对象类型
    if (astType == ZR_NULL || astType->name == ZR_NULL) {
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }
    
    EZrValueType baseType = ZR_VALUE_TYPE_OBJECT;
    
    // 根据类型名称节点的类型处理
    if (astType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        // 标识符类型（如 int, float, bool, string, 或用户定义类型）
        SZrString *typeName = astType->name->data.identifier.name;
        if (typeName == ZR_NULL) {
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            return ZR_TRUE;
        }
        
        // 获取类型名称字符串
        TNativeString nameStr;
        TZrSize nameLen;
        if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            nameStr = ZrStringGetNativeStringShort(typeName);
            nameLen = typeName->shortStringLength;
        } else {
            nameStr = ZrStringGetNativeString(typeName);
            nameLen = typeName->longStringLength;
        }
        
        // 匹配基本类型名称
        if (nameLen == 3 && memcmp(nameStr, "int", 3) == 0) {
            baseType = ZR_VALUE_TYPE_INT64;
        } else if (nameLen == 5 && memcmp(nameStr, "float", 5) == 0) {
            baseType = ZR_VALUE_TYPE_DOUBLE;
        } else if (nameLen == 4 && memcmp(nameStr, "bool", 4) == 0) {
            baseType = ZR_VALUE_TYPE_BOOL;
        } else if (nameLen == 6 && memcmp(nameStr, "string", 6) == 0) {
            baseType = ZR_VALUE_TYPE_STRING;
        } else if (nameLen == 4 && memcmp(nameStr, "null", 4) == 0) {
            baseType = ZR_VALUE_TYPE_NULL;
        } else if (nameLen == 4 && memcmp(nameStr, "void", 4) == 0) {
            baseType = ZR_VALUE_TYPE_NULL; // void 视为 null
        } else {
            // 用户定义类型（struct/class等），存储类型名称
            ZrInferredTypeInitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
            return ZR_TRUE;
        }
    } else if (astType->name->type == ZR_AST_GENERIC_TYPE) {
        // 泛型类型（TODO: 处理泛型）
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    } else if (astType->name->type == ZR_AST_TUPLE_TYPE) {
        // 元组类型（TODO: 处理元组）
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    } else {
        // 未知类型节点类型
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }
    
    // 处理数组维度
    if (astType->dimensions > 0) {
        // 数组类型
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_ARRAY);
        // 处理元素类型
        if (astType->subType != ZR_NULL) {
            // 递归转换子类型
            SZrInferredType elementType;
            if (convert_ast_type_to_inferred_type(cs, astType->subType, &elementType)) {
                // 将元素类型添加到elementTypes数组
                ZrArrayInit(cs->state, &result->elementTypes, sizeof(SZrInferredType), 1);
                ZrArrayPush(cs->state, &result->elementTypes, &elementType);
            }
        }
        
        // 复制数组大小约束
        if (astType->hasArraySizeConstraint) {
            result->arrayFixedSize = astType->arrayFixedSize;
            result->arrayMinSize = astType->arrayMinSize;
            result->arrayMaxSize = astType->arrayMaxSize;
            result->hasArraySizeConstraint = ZR_TRUE;
        }
    } else {
        // 非数组类型
        ZrInferredTypeInit(cs->state, result, baseType);
    }
    
    return ZR_TRUE;
}

// 获取类型的范围限制（用于整数类型）
static void get_type_range(EZrValueType baseType, TInt64 *minValue, TInt64 *maxValue) {
    switch (baseType) {
        case ZR_VALUE_TYPE_INT8:
            *minValue = -128;
            *maxValue = 127;
            break;
        case ZR_VALUE_TYPE_INT16:
            *minValue = -32768;
            *maxValue = 32767;
            break;
        case ZR_VALUE_TYPE_INT32:
            *minValue = -2147483648LL;
            *maxValue = 2147483647LL;
            break;
        case ZR_VALUE_TYPE_INT64:
            *minValue = -9223372036854775808LL;
            *maxValue = 9223372036854775807LL;
            break;
        case ZR_VALUE_TYPE_UINT8:
            *minValue = 0;
            *maxValue = 255;
            break;
        case ZR_VALUE_TYPE_UINT16:
            *minValue = 0;
            *maxValue = 65535;
            break;
        case ZR_VALUE_TYPE_UINT32:
            *minValue = 0;
            *maxValue = 4294967295LL;
            break;
        case ZR_VALUE_TYPE_UINT64:
            *minValue = 0;
            *maxValue = 18446744073709551615ULL;
            break;
        default:
            *minValue = 0;
            *maxValue = 0;
            break;
    }
}

// 检查字面量范围
TBool check_literal_range(SZrCompilerState *cs, SZrAstNode *literalNode, const SZrInferredType *targetType, SZrFileRange location) {
    if (cs == ZR_NULL || literalNode == ZR_NULL || targetType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查整数类型字面量
    if (literalNode->type == ZR_AST_INTEGER_LITERAL && ZR_VALUE_IS_TYPE_INT(targetType->baseType)) {
        TInt64 literalValue = literalNode->data.integerLiteral.value;
        TInt64 minValue, maxValue;
        get_type_range(targetType->baseType, &minValue, &maxValue);
        
        if (literalValue < minValue || literalValue > maxValue) {
            static TChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Integer literal %lld is out of range for type (expected range: %lld to %lld)",
                    (long long)literalValue, (long long)minValue, (long long)maxValue);
            report_type_error(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
        
        // 检查用户定义的范围约束
        if (targetType->hasRangeConstraint) {
            if (literalValue < targetType->minValue || literalValue > targetType->maxValue) {
                static TChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg),
                        "Integer literal %lld is out of range constraint (expected range: %lld to %lld)",
                        (long long)literalValue, (long long)targetType->minValue, (long long)targetType->maxValue);
                report_type_error(cs, errorMsg, targetType, ZR_NULL, location);
                return ZR_FALSE;
            }
        }
    }
    
    // 检查浮点数类型字面量（NaN, Infinity）
    if (literalNode->type == ZR_AST_FLOAT_LITERAL) {
        TDouble floatValue = literalNode->data.floatLiteral.value;
        // 检查是否为 NaN 或 Infinity（如果目标类型不允许）
        // 根据目标类型决定是否允许 NaN/Infinity
        if (isnan(floatValue) || isinf(floatValue)) {
            // 检查目标类型是否允许NaN/Infinity
            // 对于整数类型，不允许NaN/Infinity
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(targetType->baseType) || 
                ZR_VALUE_IS_TYPE_UNSIGNED_INT(targetType->baseType)) {
                report_type_error(cs, "NaN/Infinity cannot be assigned to integer type", 
                                 targetType, ZR_NULL, location);
                return ZR_FALSE;
            }
            // 对于浮点类型，允许NaN/Infinity
        }
    }
    
    return ZR_TRUE;
}

// 检查数组索引边界
TBool check_array_index_bounds(SZrCompilerState *cs, SZrAstNode *indexExpr, const SZrInferredType *arrayType, SZrFileRange location) {
    if (cs == ZR_NULL || indexExpr == ZR_NULL || arrayType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 只对字面量索引进行编译期检查
    if (indexExpr->type == ZR_AST_INTEGER_LITERAL) {
        TInt64 indexValue = indexExpr->data.integerLiteral.value;
        
        if (indexValue < 0) {
            static TChar errorMsg[128];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array index %lld is negative", (long long)indexValue);
            report_type_error(cs, errorMsg, arrayType, ZR_NULL, location);
            return ZR_FALSE;
        }
        
        // 如果数组有固定大小，检查索引是否越界
        if (arrayType->hasArraySizeConstraint && arrayType->arrayFixedSize > 0) {
            if ((TZrSize)indexValue >= arrayType->arrayFixedSize) {
                static TChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg),
                        "Array index %lld is out of bounds (array size: %zu)",
                        (long long)indexValue, arrayType->arrayFixedSize);
                report_type_error(cs, errorMsg, arrayType, ZR_NULL, location);
                return ZR_FALSE;
            }
        }
    }
    
    // 对于非字面量索引，编译期无法检查，将在运行时检查（如果启用）
    return ZR_TRUE;
}
