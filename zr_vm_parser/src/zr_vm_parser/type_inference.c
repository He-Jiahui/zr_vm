//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "compiler_internal.h"
#include "type_inference_internal.h"
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_string_conf.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

void ZrParser_TypeError_Report(SZrCompilerState *cs, const TZrChar *message, const SZrInferredType *expectedType, const SZrInferredType *actualType, SZrFileRange location) {
    if (cs == ZR_NULL || message == ZR_NULL) {
        return;
    }
    
    static TZrChar errorMsg[ZR_PARSER_TEXT_BUFFER_LENGTH];
    static TZrChar expectedTypeStr[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    static TZrChar actualTypeStr[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    
    const TZrChar *expectedName = "unknown";
    const TZrChar *actualName = "unknown";
    
    if (expectedType != ZR_NULL) {
        expectedName = ZrParser_TypeNameString_Get(cs->state, expectedType, expectedTypeStr, sizeof(expectedTypeStr));
    }
    if (actualType != ZR_NULL) {
        actualName = ZrParser_TypeNameString_Get(cs->state, actualType, actualTypeStr, sizeof(actualTypeStr));
    }
    
    // 构建详细的错误消息，包含类型信息
    snprintf(errorMsg, sizeof(errorMsg), 
             "Type Error: %s (expected: %s, actual: %s). "
             "Check variable types, function signatures, and type annotations. "
             "Ensure the actual type is compatible with the expected type. "
             "Consider adding explicit type conversions if needed.",
             message, expectedName, actualName);
    
    ZrParser_Compiler_Error(cs, errorMsg, location);
}

// 检查类型兼容性（用于赋值等场景）
TZrBool ZrParser_TypeCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *fromType, const SZrInferredType *toType, SZrFileRange location) {
    if (cs == ZR_NULL || fromType == ZR_NULL || toType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    if (ZrParser_InferredType_IsCompatible(fromType, toType)) {
        return ZR_TRUE;
    }
    
    // 类型不兼容，报告错误
    ZrParser_TypeError_Report(cs, "Type mismatch", toType, fromType, location);
    return ZR_FALSE;
}

// 检查赋值兼容性
TZrBool ZrParser_AssignmentCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *leftType, const SZrInferredType *rightType, SZrFileRange location) {
    if (cs == ZR_NULL || leftType == ZR_NULL || rightType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 首先检查基本类型兼容性
    if (!ZrParser_TypeCompatibility_Check(cs, rightType, leftType, location)) {
        return ZR_FALSE;
    }
    
    // 检查范围约束（如果目标类型有范围约束）
    if (leftType->hasRangeConstraint) {
        // 对于字面量，在 ZrParser_LiteralRange_Check 中已经检查
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
TZrBool ZrParser_FunctionCallCompatibility_Check(SZrCompilerState *cs,
                                        SZrTypeEnvironment *env,
                                        SZrString *funcName,
                                        SZrFunctionCall *call,
                                        SZrFunctionTypeInfo *funcType,
                                        const SZrResolvedCallSignature *resolvedSignature,
                                        SZrFileRange location) {
    SZrArray argTypes;
    TZrBool mismatch = ZR_FALSE;
    const SZrArray *parameterTypes;
    const SZrArray *parameterPassingModes;

    if (cs == ZR_NULL || funcType == ZR_NULL) {
        return ZR_FALSE;
    }

    parameterTypes = resolvedSignature != ZR_NULL ? &resolvedSignature->parameterTypes : &funcType->paramTypes;
    parameterPassingModes =
            resolvedSignature != ZR_NULL ? &resolvedSignature->parameterPassingModes : &funcType->parameterPassingModes;

    if (!infer_function_call_argument_types_for_candidate(cs,
                                                          env,
                                                          funcName,
                                                          call,
                                                          funcType,
                                                          &argTypes,
                                                          &mismatch)) {
        return ZR_FALSE;
    }

    if (mismatch) {
        free_inferred_type_array(cs->state, &argTypes);
        return ZR_FALSE;
    }

    if (argTypes.length != parameterTypes->length) {
        free_inferred_type_array(cs->state, &argTypes);
        return ZR_FALSE;
    }

    if (!validate_call_argument_passing_modes(cs, parameterPassingModes, parameterTypes, call, &argTypes)) {
        free_inferred_type_array(cs->state, &argTypes);
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < argTypes.length; i++) {
        SZrInferredType *argType = (SZrInferredType *) ZrCore_Array_Get(&argTypes, i);
        SZrInferredType *paramType = (SZrInferredType *) ZrCore_Array_Get((SZrArray *)parameterTypes, i);
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
        EZrParameterPassingMode *modePtr = ZR_NULL;

        if (argType == ZR_NULL || paramType == ZR_NULL) {
            free_inferred_type_array(cs->state, &argTypes);
            return ZR_FALSE;
        }

        if (parameterPassingModes != ZR_NULL && i < parameterPassingModes->length) {
            modePtr = (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)parameterPassingModes, i);
            if (modePtr != ZR_NULL) {
                passingMode = *modePtr;
            }
        }

        if (passingMode == ZR_PARAMETER_PASSING_MODE_OUT || passingMode == ZR_PARAMETER_PASSING_MODE_REF) {
            continue;
        }

        if (!ZrParser_InferredType_IsCompatible(argType, paramType)) {
            ZrParser_TypeError_Report(cs, "Argument type mismatch", paramType, argType, location);
            free_inferred_type_array(cs->state, &argTypes);
            return ZR_FALSE;
        }
    }

    free_inferred_type_array(cs->state, &argTypes);
    return ZR_TRUE;
}

// 从字面量推断类型
TZrBool ZrParser_LiteralType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_BOOL);
            return ZR_TRUE;
            
        case ZR_AST_INTEGER_LITERAL: {
            // 未加后缀的整数字面量统一按 int64 推断。
            // 后续若需要字面量收窄，应由语义层在约束上下文中完成，而不是在基础推断阶段直接缩小。
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_INT64);
            return ZR_TRUE;
        }
            
        case ZR_AST_FLOAT_LITERAL:
            // 默认使用DOUBLE
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_DOUBLE);
            return ZR_TRUE;
            
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_STRING);
            return ZR_TRUE;
            
        case ZR_AST_CHAR_LITERAL:
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_INT8);
            return ZR_TRUE;
            
        case ZR_AST_NULL_LITERAL:
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_NULL);
            result->isNullable = ZR_TRUE;
            return ZR_TRUE;
            
        default:
            return ZR_FALSE;
    }
}

// 从标识符推断类型
TZrBool ZrParser_IdentifierType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }
    
    SZrString *name = node->data.identifier.name;
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 在类型环境中查找变量类型
    if (cs->typeEnv != ZR_NULL) {
        if (ZrParser_TypeEnvironment_LookupVariable(cs->state, cs->typeEnv, name, result)) {
            SZrInferredType normalizedType;

            ZrParser_InferredType_Init(cs->state, &normalizedType, ZR_VALUE_TYPE_OBJECT);
            if (result->typeName != ZR_NULL &&
                result->elementTypes.length == 0 &&
                inferred_type_from_type_name(cs, result->typeName, &normalizedType) &&
                normalizedType.elementTypes.length > 0) {
                normalizedType.isNullable = result->isNullable;
                normalizedType.ownershipQualifier = result->ownershipQualifier;
                ZrParser_InferredType_Free(cs->state, result);
                ZrParser_InferredType_Copy(cs->state, result, &normalizedType);
            }
            ZrParser_InferredType_Free(cs->state, &normalizedType);
            return ZR_TRUE;
        }
    }

    if (find_compiler_type_prototype_inference(cs, name) != ZR_NULL) {
        return inferred_type_from_type_name(cs, name, result);
    }
    
    // 未找到变量类型，不立即报错
    // 可能是全局对象 zr 的属性访问，或者子函数，或者全局对象的其他属性
    // 返回默认的 OBJECT 类型，让 compile_identifier 继续处理
    // compile_identifier 会尝试作为全局对象属性访问、子函数访问等
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从一元表达式推断类型
TZrBool ZrParser_UnaryExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    const TZrChar *op = node->data.unaryExpression.op.op;
    SZrAstNode *arg = node->data.unaryExpression.argument;
    if (strcmp(op, "new") == 0 || strcmp(op, "$") == 0) {
        ZrParser_Compiler_Error(cs,
                        "Legacy unary constructor syntax is no longer supported; use $target(...) or new target(...)",
                        node->location);
        return ZR_FALSE;
    }
    
    // 推断操作数类型
    SZrInferredType argType;
    ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, arg, &argType)) {
        ZrParser_InferredType_Free(cs->state, &argType);
        return ZR_FALSE;
    }
    
    if (strcmp(op, "!") == 0) {
        // 逻辑非：结果类型是bool
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrParser_InferredType_Free(cs->state, &argType);
        return ZR_TRUE;
    } else if (strcmp(op, "~") == 0) {
        // 位非：结果类型与操作数类型相同（整数类型）
        ZrParser_InferredType_Copy(cs->state, result, &argType);
        ZrParser_InferredType_Free(cs->state, &argType);
        return ZR_TRUE;
    } else if (strcmp(op, "-") == 0 || strcmp(op, "+") == 0) {
        // 取负/正号：结果类型与操作数类型相同
        ZrParser_InferredType_Copy(cs->state, result, &argType);
        ZrParser_InferredType_Free(cs->state, &argType);
        return ZR_TRUE;
    }
    
    ZrParser_InferredType_Free(cs->state, &argType);
    return ZR_FALSE;
}

// 从二元表达式推断类型
TZrBool ZrParser_BinaryExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    const TZrChar *op = node->data.binaryExpression.op.op;
    SZrAstNode *left = node->data.binaryExpression.left;
    SZrAstNode *right = node->data.binaryExpression.right;
    
    // 推断左右操作数类型
    SZrInferredType leftType, rightType;
    TZrBool hasLeftType = ZR_FALSE;
    TZrBool hasRightType = ZR_FALSE;
    ZrParser_InferredType_Init(cs->state, &leftType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &rightType, ZR_VALUE_TYPE_OBJECT);
    hasLeftType = ZrParser_ExpressionType_Infer(cs, left, &leftType);
    hasRightType = hasLeftType ? ZrParser_ExpressionType_Infer(cs, right, &rightType) : ZR_FALSE;
    if (!hasLeftType || !hasRightType) {
        if (hasLeftType) {
            ZrParser_InferredType_Free(cs->state, &leftType);
        }
        if (hasRightType) {
            ZrParser_InferredType_Free(cs->state, &rightType);
        }
        return ZR_FALSE;
    }
    
    // 根据操作符确定结果类型
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || 
        strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || 
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
        // 比较运算符：结果类型是bool
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrParser_InferredType_Free(cs->state, &leftType);
        ZrParser_InferredType_Free(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        // 逻辑运算符：结果类型是bool
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrParser_InferredType_Free(cs->state, &leftType);
        ZrParser_InferredType_Free(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || 
               strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || 
               strcmp(op, "%") == 0 || strcmp(op, "**") == 0) {
        // 算术运算符：获取公共类型（类型提升）
        if (!ZrParser_InferredType_GetCommonType(cs->state, result, &leftType, &rightType)) {
            // 对类成员访问、动态调用等暂未精确建模的表达式，降级为 object，
            // 避免在 M3 运行时闭环阶段被 M6 的类型系统债务阻塞。
            if (leftType.baseType == ZR_VALUE_TYPE_OBJECT || rightType.baseType == ZR_VALUE_TYPE_OBJECT) {
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Free(cs->state, &leftType);
                ZrParser_InferredType_Free(cs->state, &rightType);
                return ZR_TRUE;
            }

            ZrParser_TypeError_Report(cs, "Incompatible types for arithmetic operation", &leftType, &rightType, node->location);
            ZrParser_InferredType_Free(cs->state, &leftType);
            ZrParser_InferredType_Free(cs->state, &rightType);
            return ZR_FALSE;
        }
        ZrParser_InferredType_Free(cs->state, &leftType);
        ZrParser_InferredType_Free(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0 ||
               strcmp(op, "&") == 0 || strcmp(op, "|") == 0 || strcmp(op, "^") == 0) {
        // 位运算符：结果类型与左操作数类型相同（整数类型）
        ZrParser_InferredType_Copy(cs->state, result, &leftType);
        ZrParser_InferredType_Free(cs->state, &leftType);
        ZrParser_InferredType_Free(cs->state, &rightType);
        return ZR_TRUE;
    }
    
    ZrParser_InferredType_Free(cs->state, &leftType);
    ZrParser_InferredType_Free(cs->state, &rightType);
    return ZR_FALSE;
}

// 从函数调用推断类型
TZrBool ZrParser_FunctionCallType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
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
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从Lambda表达式推断类型
TZrBool ZrParser_LambdaType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_LAMBDA_EXPRESSION) {
        return ZR_FALSE;
    }
    
    // Lambda表达式返回函数类型
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_CLOSURE);
    return ZR_TRUE;
}

static SZrString *build_array_type_name_string(SZrState *state,
                                               const SZrInferredType *elementType,
                                               TZrBool hasFixedSize,
                                               TZrSize fixedSize) {
    TZrChar elementBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrChar nameBuffer[ZR_PARSER_DETAIL_BUFFER_LENGTH];
    const TZrChar *elementName;
    TZrInt32 written;

    if (state == ZR_NULL || elementType == ZR_NULL) {
        return ZR_NULL;
    }

    elementName = ZrParser_TypeNameString_Get(state, elementType, elementBuffer, sizeof(elementBuffer));
    if (elementName == ZR_NULL) {
        return ZR_NULL;
    }

    if (hasFixedSize) {
        written = snprintf(nameBuffer, sizeof(nameBuffer), "%s[%zu]", elementName, fixedSize);
    } else {
        written = snprintf(nameBuffer, sizeof(nameBuffer), "%s[]", elementName);
    }

    if (written <= 0 || (TZrSize)written >= sizeof(nameBuffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, nameBuffer, (TZrSize)written);
}

static TZrBool init_wrapped_array_inferred_type(SZrCompilerState *cs,
                                                const SZrInferredType *elementType,
                                                TZrBool hasArraySizeConstraint,
                                                TZrSize arrayFixedSize,
                                                TZrSize arrayMinSize,
                                                TZrSize arrayMaxSize,
                                                SZrInferredType *result) {
    SZrInferredType copiedElement;

    if (cs == ZR_NULL || elementType == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_ARRAY);
    ZrCore_Array_Init(cs->state, &result->elementTypes, sizeof(SZrInferredType), 1);

    ZrParser_InferredType_Init(cs->state, &copiedElement, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(cs->state, &copiedElement, elementType);
    ZrCore_Array_Push(cs->state, &result->elementTypes, &copiedElement);

    result->hasArraySizeConstraint = hasArraySizeConstraint;
    result->arrayFixedSize = arrayFixedSize;
    result->arrayMinSize = arrayMinSize;
    result->arrayMaxSize = arrayMaxSize;
    result->typeName = build_array_type_name_string(cs->state,
                                                    elementType,
                                                    hasArraySizeConstraint && arrayFixedSize > 0,
                                                    arrayFixedSize);
    return ZR_TRUE;
}

// 从数组字面量推断类型
TZrBool ZrParser_ArrayLiteralType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    SZrArrayLiteral *arrayLiteral;
    SZrInferredType commonElementType;
    TZrBool hasCommonElementType = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_ARRAY_LITERAL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_ARRAY);
    arrayLiteral = &node->data.arrayLiteral;
    if (arrayLiteral->elements == ZR_NULL || arrayLiteral->elements->count == 0) {
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(cs->state, &commonElementType, ZR_VALUE_TYPE_OBJECT);
    for (TZrSize index = 0; index < arrayLiteral->elements->count; index++) {
        SZrAstNode *elementNode = arrayLiteral->elements->nodes[index];
        SZrInferredType elementType;

        if (elementNode == ZR_NULL) {
            continue;
        }

        ZrParser_InferredType_Init(cs->state, &elementType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(cs, elementNode, &elementType)) {
            ZrParser_InferredType_Free(cs->state, &elementType);
            if (hasCommonElementType) {
                ZrParser_InferredType_Free(cs->state, &commonElementType);
            }
            return ZR_FALSE;
        }

        if (!hasCommonElementType) {
            ZrParser_InferredType_Copy(cs->state, &commonElementType, &elementType);
            hasCommonElementType = ZR_TRUE;
            ZrParser_InferredType_Free(cs->state, &elementType);
            continue;
        }

        {
            SZrInferredType mergedType;

            ZrParser_InferredType_Init(cs->state, &mergedType, ZR_VALUE_TYPE_OBJECT);
            if (ZrParser_InferredType_GetCommonType(cs->state, &mergedType, &commonElementType, &elementType)) {
                ZrParser_InferredType_Free(cs->state, &commonElementType);
                ZrParser_InferredType_Init(cs->state, &commonElementType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Copy(cs->state, &commonElementType, &mergedType);
                ZrParser_InferredType_Free(cs->state, &mergedType);
            } else {
                ZrParser_InferredType_Free(cs->state, &commonElementType);
                ZrParser_InferredType_Init(cs->state, &commonElementType, ZR_VALUE_TYPE_OBJECT);
            }
        }

        ZrParser_InferredType_Free(cs->state, &elementType);
    }

    if (!hasCommonElementType) {
        ZrParser_InferredType_Free(cs->state, &commonElementType);
        return ZR_TRUE;
    }

    ZrCore_Array_Init(cs->state, &result->elementTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(cs->state, &result->elementTypes, &commonElementType);
    result->hasArraySizeConstraint = ZR_TRUE;
    result->arrayFixedSize = arrayLiteral->elements->count;
    result->arrayMinSize = arrayLiteral->elements->count;
    result->arrayMaxSize = arrayLiteral->elements->count;
    result->typeName = build_array_type_name_string(cs->state,
                                                    &commonElementType,
                                                    ZR_TRUE,
                                                    arrayLiteral->elements->count);
    return ZR_TRUE;
}

// 从对象字面量推断类型
TZrBool ZrParser_ObjectLiteralType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_OBJECT_LITERAL) {
        return ZR_FALSE;
    }
    
    // 对象字面量返回对象类型
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从条件表达式推断类型
TZrBool ZrParser_ConditionalType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_CONDITIONAL_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrConditionalExpression *condExpr = &node->data.conditionalExpression;
    
    // 推断then和else分支类型
    SZrInferredType thenType, elseType;
    TZrBool hasThenType = ZR_FALSE;
    TZrBool hasElseType = ZR_FALSE;
    ZrParser_InferredType_Init(cs->state, &thenType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &elseType, ZR_VALUE_TYPE_OBJECT);
    hasThenType = ZrParser_ExpressionType_Infer(cs, condExpr->consequent, &thenType);
    hasElseType = hasThenType ? ZrParser_ExpressionType_Infer(cs, condExpr->alternate, &elseType) : ZR_FALSE;
    if (!hasThenType || !hasElseType) {
        if (hasThenType) {
            ZrParser_InferredType_Free(cs->state, &thenType);
        }
        if (hasElseType) {
            ZrParser_InferredType_Free(cs->state, &elseType);
        }
        return ZR_FALSE;
    }
    
    // 获取公共类型
    if (!ZrParser_InferredType_GetCommonType(cs->state, result, &thenType, &elseType)) {
        // 类型不兼容，报告错误
        ZrParser_TypeError_Report(cs, "Incompatible types in conditional expression branches", &thenType, &elseType, node->location);
        ZrParser_InferredType_Free(cs->state, &thenType);
        ZrParser_InferredType_Free(cs->state, &elseType);
        return ZR_FALSE;
    }
    
    ZrParser_InferredType_Free(cs->state, &thenType);
    ZrParser_InferredType_Free(cs->state, &elseType);
    return ZR_TRUE;
}

// 从赋值表达式推断类型
TZrBool ZrParser_AssignmentType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
    
    // 推断右值类型
    if (!ZrParser_ExpressionType_Infer(cs, assignExpr->right, result)) {
        return ZR_FALSE;
    }
    
    // 检查与左值类型的兼容性
    // 1. 推断左值类型
    SZrInferredType leftType;
    ZrParser_InferredType_Init(cs->state, &leftType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(cs, assignExpr->left, &leftType)) {
        // 2. 检查类型兼容性
        if (leftType.baseType != ZR_VALUE_TYPE_OBJECT && result->baseType != ZR_VALUE_TYPE_OBJECT &&
            !ZrParser_InferredType_IsCompatible(result, &leftType)) {
            // 3. 报告错误如果不兼容
            ZrParser_TypeError_Report(cs, "Assignment type mismatch", &leftType, result, node->location);
            ZrParser_InferredType_Free(cs->state, &leftType);
            return ZR_FALSE;
        }
        ZrParser_InferredType_Free(cs->state, &leftType);
    }
    
    return ZR_TRUE;
}

// 从primary expression推断类型（包括函数调用）
TZrBool ZrParser_PrimaryExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    
    // 如果没有members，直接推断property的类型
    if (primary->members == ZR_NULL || primary->members->count == 0) {
        if (primary->property != ZR_NULL) {
            return ZrParser_ExpressionType_Infer(cs, primary->property, result);
        }
        // 如果没有property，返回对象类型
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }
    
    // 检查第一个member是否是函数调用：foo()
    SZrAstNode *firstMember = primary->members->nodes[0];
    if (firstMember != ZR_NULL && firstMember->type == ZR_AST_FUNCTION_CALL) {
        // 函数调用：从property中提取函数名
        if (primary->property != ZR_NULL && primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
            SZrString *funcName = primary->property->data.identifier.name;
            if (funcName != ZR_NULL) {
                SZrFunctionTypeInfo *funcTypeInfo = ZR_NULL;
                SZrFunctionCall *call = &firstMember->data.functionCall;
                SZrInferredType baseType;
                SZrResolvedCallSignature resolvedSignature;
                TZrBool hasRuntimeFunction = ZR_FALSE;
                TZrBool hasCompileTimeFunction = ZR_FALSE;

                ZrParser_InferredType_Init(cs->state, &baseType, ZR_VALUE_TYPE_OBJECT);
                memset(&resolvedSignature, 0, sizeof(resolvedSignature));
                ZrParser_InferredType_Init(cs->state, &resolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
                ZrCore_Array_Construct(&resolvedSignature.parameterTypes);
                ZrCore_Array_Construct(&resolvedSignature.parameterPassingModes);

                if (find_compiler_type_prototype_inference(cs, funcName) != ZR_NULL) {
                    ZrParser_Compiler_Error(cs,
                                            "Prototype references are not callable; use $target(...) or new target(...)",
                                            firstMember->location);
                    ZrParser_InferredType_Free(cs->state, &baseType);
                    return ZR_FALSE;
                }

                if (cs->typeEnv != ZR_NULL) {
                    hasRuntimeFunction = ZrParser_TypeEnvironment_LookupFunction(cs->typeEnv, funcName, &funcTypeInfo);
                    funcTypeInfo = ZR_NULL;
                }

                if (hasRuntimeFunction &&
                    resolve_best_function_overload(cs,
                                                   cs->typeEnv,
                                                   funcName,
                                                   call,
                                                   node->location,
                                                   &funcTypeInfo,
                                                   &resolvedSignature)) {
                    ZrParser_InferredType_Copy(cs->state, &baseType, &resolvedSignature.returnType);
                    ZrParser_FunctionCallCompatibility_Check(cs,
                                                      cs->typeEnv,
                                                      funcName,
                                                      call,
                                                      funcTypeInfo,
                                                      &resolvedSignature,
                                                      node->location);
                    if (primary->members->count > 1) {
                        TZrBool success =
                                infer_primary_member_chain_type(cs, &baseType, primary->members, 1, ZR_FALSE, result);
                        ZrParser_InferredType_Free(cs->state, &baseType);
                        free_resolved_call_signature(cs->state, &resolvedSignature);
                        return success;
                    }
                    ZrParser_InferredType_Copy(cs->state, result, &baseType);
                    ZrParser_InferredType_Free(cs->state, &baseType);
                    free_resolved_call_signature(cs->state, &resolvedSignature);
                    return ZR_TRUE;
                }
                if (hasRuntimeFunction) {
                    ZrParser_InferredType_Free(cs->state, &baseType);
                    free_resolved_call_signature(cs->state, &resolvedSignature);
                    return ZR_FALSE;
                }

                if (cs->compileTimeTypeEnv != ZR_NULL) {
                    hasCompileTimeFunction =
                        ZrParser_TypeEnvironment_LookupFunction(cs->compileTimeTypeEnv, funcName, &funcTypeInfo);
                    funcTypeInfo = ZR_NULL;
                }

                if (hasCompileTimeFunction &&
                    resolve_best_function_overload(cs,
                                                   cs->compileTimeTypeEnv,
                                                   funcName,
                                                   call,
                                                   node->location,
                                                   &funcTypeInfo,
                                                   &resolvedSignature)) {
                    ZrParser_InferredType_Copy(cs->state, &baseType, &resolvedSignature.returnType);
                    ZrParser_FunctionCallCompatibility_Check(cs,
                                                      cs->compileTimeTypeEnv,
                                                      funcName,
                                                      call,
                                                      funcTypeInfo,
                                                      &resolvedSignature,
                                                      node->location);
                    if (primary->members->count > 1) {
                        TZrBool success =
                                infer_primary_member_chain_type(cs, &baseType, primary->members, 1, ZR_FALSE, result);
                        ZrParser_InferredType_Free(cs->state, &baseType);
                        free_resolved_call_signature(cs->state, &resolvedSignature);
                        return success;
                    }
                    ZrParser_InferredType_Copy(cs->state, result, &baseType);
                    ZrParser_InferredType_Free(cs->state, &baseType);
                    free_resolved_call_signature(cs->state, &resolvedSignature);
                    return ZR_TRUE;
                }
                if (hasCompileTimeFunction) {
                    ZrParser_InferredType_Free(cs->state, &baseType);
                    free_resolved_call_signature(cs->state, &resolvedSignature);
                    return ZR_FALSE;
                }
                // 函数和类型都未找到时，保持动态 object fallback。
                ZrParser_InferredType_Free(cs->state, &baseType);
                free_resolved_call_signature(cs->state, &resolvedSignature);
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        }
        
        // property不是标识符，或者函数名未找到，返回对象类型
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }
    
    // 不是函数调用，或者是成员访问等其他情况
    // 先推断property的类型，然后根据members推断最终类型
    // 实现完整的成员访问链类型推断（如 obj.prop）
    if (primary->property != ZR_NULL) {
        SZrInferredType baseType;
        SZrTypePrototypeInfo *basePrototype = ZR_NULL;
        SZrString *basePrototypeTypeName = ZR_NULL;
        TZrBool baseIsPrototypeReference = ZR_FALSE;
        ZrParser_InferredType_Init(cs->state, &baseType, ZR_VALUE_TYPE_OBJECT);
        if (ZrParser_ExpressionType_Infer(cs, primary->property, &baseType)) {
            baseIsPrototypeReference =
                    resolve_prototype_target_inference(cs, primary->property, &basePrototype, &basePrototypeTypeName);
            ZR_UNUSED_PARAMETER(basePrototype);
            ZR_UNUSED_PARAMETER(basePrototypeTypeName);
            // 如果有members，需要根据members推断最终类型
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                TZrBool success = infer_primary_member_chain_type(cs,
                                                                  &baseType,
                                                                  primary->members,
                                                                  0,
                                                                  baseIsPrototypeReference,
                                                                  result);
                ZrParser_InferredType_Free(cs->state, &baseType);
                return success;
            } else {
                // 没有members，直接返回property的类型
                ZrParser_InferredType_Copy(cs->state, result, &baseType);
                ZrParser_InferredType_Free(cs->state, &baseType);
                return ZR_TRUE;
            }
        }
    }
    
    // 默认返回对象类型
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从AST节点推断类型（主入口函数）
TZrBool ZrParser_ExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!cs->externBindingsPredeclared &&
        cs->scriptAst != ZR_NULL &&
        cs->scriptAst->type == ZR_AST_SCRIPT &&
        cs->scriptAst->data.script.statements != ZR_NULL) {
        ZrParser_Compiler_PredeclareExternBindings(cs, cs->scriptAst->data.script.statements);
        if (cs->hasError) {
            return ZR_FALSE;
        }
    }
    
    switch (node->type) {
        // 字面量
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            return ZrParser_LiteralType_Infer(cs, node, result);
        
        case ZR_AST_IDENTIFIER_LITERAL:
            return ZrParser_IdentifierType_Infer(cs, node, result);
        
        // 表达式
        case ZR_AST_BINARY_EXPRESSION:
            return ZrParser_BinaryExpressionType_Infer(cs, node, result);
        
        case ZR_AST_UNARY_EXPRESSION:
            return ZrParser_UnaryExpressionType_Infer(cs, node, result);
        
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ZrParser_ConditionalType_Infer(cs, node, result);
        
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return ZrParser_AssignmentType_Infer(cs, node, result);

        case ZR_AST_TYPE_CAST_EXPRESSION:
            if (node->data.typeCastExpression.targetType == ZR_NULL) {
                return ZR_FALSE;
            }
            return ZrParser_AstTypeToInferredType_Convert(cs, node->data.typeCastExpression.targetType, result);
        
        case ZR_AST_FUNCTION_CALL:
            return ZrParser_FunctionCallType_Infer(cs, node, result);

        case ZR_AST_IMPORT_EXPRESSION:
            return infer_import_expression_type(cs, node, result);

        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return infer_type_query_expression_type(cs, node, result);
        
        case ZR_AST_LAMBDA_EXPRESSION:
            return ZrParser_LambdaType_Infer(cs, node, result);
        
        case ZR_AST_ARRAY_LITERAL:
            return ZrParser_ArrayLiteralType_Infer(cs, node, result);
        
        case ZR_AST_OBJECT_LITERAL:
            return ZrParser_ObjectLiteralType_Infer(cs, node, result);

        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return infer_prototype_reference_type(cs, node, result);

        case ZR_AST_CONSTRUCT_EXPRESSION:
            return infer_construct_expression_type(cs, node, result);
        
        // TODO: 处理其他表达式类型
        case ZR_AST_PRIMARY_EXPRESSION:
            return ZrParser_PrimaryExpressionType_Infer(cs, node, result);
        
        case ZR_AST_MEMBER_EXPRESSION:
            // 实现member expression的类型推断
            // member expression的类型推断需要知道对象类型和成员名称
            // TODO: 这里简化处理，返回对象类型
            // 完整的实现需要从对象类型查找成员定义
            // TODO: 注意：member expression的类型推断需要知道对象类型，暂时返回对象类型
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            return ZR_TRUE;
        
        case ZR_AST_IF_EXPRESSION:
            // 实现if expression的类型推断
            // if expression的类型是thenExpr和elseExpr的公共类型
            {
                SZrIfExpression *ifExpr = &node->data.ifExpression;
                SZrInferredType thenType, elseType;
                ZrParser_InferredType_Init(cs->state, &thenType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Init(cs->state, &elseType, ZR_VALUE_TYPE_OBJECT);
                if (ifExpr->thenExpr != ZR_NULL && ifExpr->elseExpr != ZR_NULL) {
                    if (ZrParser_ExpressionType_Infer(cs, ifExpr->thenExpr, &thenType) &&
                        ZrParser_ExpressionType_Infer(cs, ifExpr->elseExpr, &elseType)) {
                        // 获取公共类型
                        if (ZrParser_InferredType_GetCommonType(cs->state, result, &thenType, &elseType)) {
                            ZrParser_InferredType_Free(cs->state, &thenType);
                            ZrParser_InferredType_Free(cs->state, &elseType);
                            return ZR_TRUE;
                        }
                        ZrParser_InferredType_Free(cs->state, &thenType);
                        ZrParser_InferredType_Free(cs->state, &elseType);
                    }
                } else if (ifExpr->thenExpr != ZR_NULL) {
                    return ZrParser_ExpressionType_Infer(cs, ifExpr->thenExpr, result);
                } else if (ifExpr->elseExpr != ZR_NULL) {
                    return ZrParser_ExpressionType_Infer(cs, ifExpr->elseExpr, result);
                }
                // 默认返回对象类型
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        
        case ZR_AST_SWITCH_EXPRESSION:
            // 实现switch expression的类型推断
            // switch expression的类型是所有case和default的公共类型
            {
                SZrSwitchExpression *switchExpr = &node->data.switchExpression;
                SZrInferredType commonType;
                ZrParser_InferredType_Init(cs->state, &commonType, ZR_VALUE_TYPE_OBJECT);
                TZrBool hasType = ZR_FALSE;
                
                // 遍历所有case，推断类型
                if (switchExpr->cases != ZR_NULL) {
                    for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
                        SZrAstNode *caseNode = switchExpr->cases->nodes[i];
                        if (caseNode != ZR_NULL && caseNode->type == ZR_AST_SWITCH_CASE) {
                            SZrSwitchCase *switchCase = &caseNode->data.switchCase;
                            if (switchCase->block != ZR_NULL) {
                                SZrInferredType caseType;
                                ZrParser_InferredType_Init(cs->state, &caseType, ZR_VALUE_TYPE_OBJECT);
                                if (ZrParser_ExpressionType_Infer(cs, switchCase->block, &caseType)) {
                                    if (!hasType) {
                                        ZrParser_InferredType_Copy(cs->state, &commonType, &caseType);
                                        hasType = ZR_TRUE;
                                    } else {
                                        SZrInferredType newCommonType;
                                        ZrParser_InferredType_Init(cs->state, &newCommonType, ZR_VALUE_TYPE_OBJECT);
                                        if (ZrParser_InferredType_GetCommonType(cs->state, &newCommonType, &commonType, &caseType)) {
                                            ZrParser_InferredType_Free(cs->state, &commonType);
                                            commonType = newCommonType;
                                        }
                                    }
                                    ZrParser_InferredType_Free(cs->state, &caseType);
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
                        ZrParser_InferredType_Init(cs->state, &defaultType, ZR_VALUE_TYPE_OBJECT);
                        if (ZrParser_ExpressionType_Infer(cs, switchDefault->block, &defaultType)) {
                            if (!hasType) {
                                ZrParser_InferredType_Copy(cs->state, &commonType, &defaultType);
                                hasType = ZR_TRUE;
                            } else {
                                SZrInferredType newCommonType;
                                ZrParser_InferredType_Init(cs->state, &newCommonType, ZR_VALUE_TYPE_OBJECT);
                                if (ZrParser_InferredType_GetCommonType(cs->state, &newCommonType, &commonType, &defaultType)) {
                                    ZrParser_InferredType_Free(cs->state, &commonType);
                                    commonType = newCommonType;
                                }
                            }
                            ZrParser_InferredType_Free(cs->state, &defaultType);
                        }
                    }
                }
                
                if (hasType) {
                    ZrParser_InferredType_Copy(cs->state, result, &commonType);
                    ZrParser_InferredType_Free(cs->state, &commonType);
                    return ZR_TRUE;
                }
                
                // 默认返回对象类型
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        
        default:
            return ZR_FALSE;
    }
}

// 将AST类型注解转换为推断类型
TZrBool ZrParser_AstTypeToInferredType_Convert(SZrCompilerState *cs, const SZrType *astType, SZrInferredType *result) {
    SZrInferredType namedType;
    TZrBool namedTypeInitialized = ZR_FALSE;

    if (cs == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 如果没有类型注解，返回对象类型
    if (astType == ZR_NULL || astType->name == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(cs->state, &namedType, ZR_VALUE_TYPE_OBJECT);
    namedTypeInitialized = ZR_TRUE;

    if (astType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *typeName = astType->name->data.identifier.name;
        EZrValueType baseType = ZR_VALUE_TYPE_OBJECT;

        if (typeName == ZR_NULL) {
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            result->ownershipQualifier = astType->ownershipQualifier;
            return ZR_TRUE;
        }

        {
            TZrNativeString nameStr;
            TZrSize nameLen;
            if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                nameStr = ZrCore_String_GetNativeStringShort(typeName);
                nameLen = typeName->shortStringLength;
            } else {
                nameStr = ZrCore_String_GetNativeString(typeName);
                nameLen = typeName->longStringLength;
            }

            if (inferred_type_try_map_primitive_name(nameStr, nameLen, &baseType)) {
                ZrParser_InferredType_Free(cs->state, &namedType);
                ZrParser_InferredType_Init(cs->state, &namedType, baseType);
            } else {
                ZrParser_InferredType_Free(cs->state, &namedType);
                ZrParser_InferredType_InitFull(cs->state, &namedType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
                namedType.ownershipQualifier = astType->ownershipQualifier;
                if (cs->semanticContext != ZR_NULL) {
                    ZrParser_Semantic_RegisterNamedType(cs->semanticContext,
                                                        typeName,
                                                        ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                                        astType->name);
                    ZrParser_Semantic_RegisterInferredType(cs->semanticContext,
                                                           &namedType,
                                                           ZR_SEMANTIC_TYPE_KIND_REFERENCE,
                                                           namedType.typeName,
                                                           astType->name);
                }
            }
        }
    } else if (astType->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &astType->name->data.genericType;
        SZrString *canonicalName;

        if (genericType->name == ZR_NULL || genericType->name->name == ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, &namedType);
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            result->ownershipQualifier = astType->ownershipQualifier;
            return ZR_TRUE;
        }

        ZrParser_InferredType_Free(cs->state, &namedType);
        ZrParser_InferredType_InitFull(cs->state,
                                       &namedType,
                                       ZR_VALUE_TYPE_OBJECT,
                                       ZR_FALSE,
                                       genericType->name->name);

        if (genericType->params != ZR_NULL && genericType->params->count > 0) {
            ZrCore_Array_Init(cs->state,
                              &namedType.elementTypes,
                              sizeof(SZrInferredType),
                              genericType->params->count);
            for (TZrSize i = 0; i < genericType->params->count; i++) {
                SZrAstNode *paramNode = genericType->params->nodes[i];
                SZrInferredType paramType;
                SZrString *argumentName;

                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                if (paramNode != ZR_NULL && paramNode->type == ZR_AST_TYPE) {
                    if (!ZrParser_AstTypeToInferredType_Convert(cs, &paramNode->data.type, &paramType)) {
                        ZrParser_InferredType_Free(cs->state, &paramType);
                        ZrParser_InferredType_Free(cs->state, &namedType);
                        return ZR_FALSE;
                    }
                } else {
                    argumentName = extract_generic_argument_name_string(cs, paramNode);
                    if (argumentName == ZR_NULL) {
                        ZrParser_InferredType_Free(cs->state, &paramType);
                        ZrParser_InferredType_Free(cs->state, &namedType);
                        ZrParser_Compiler_Error(cs,
                                                "Generic type parameter must be a type annotation or const expression",
                                                astType->name->location);
                        return ZR_FALSE;
                    }
                    ZrParser_InferredType_Free(cs->state, &paramType);
                    ZrParser_InferredType_InitFull(cs->state,
                                                   &paramType,
                                                   ZR_VALUE_TYPE_OBJECT,
                                                   ZR_FALSE,
                                                   argumentName);
                }

                ZrCore_Array_Push(cs->state, &namedType.elementTypes, &paramType);
            }
        }

        canonicalName = build_generic_instance_name(cs->state,
                                                    genericType->name->name,
                                                    &namedType.elementTypes);
        if (canonicalName != ZR_NULL) {
            namedType.typeName = canonicalName;
        }

        namedType.ownershipQualifier = astType->ownershipQualifier;
        if (cs->semanticContext != ZR_NULL) {
            ZrParser_Semantic_RegisterNamedType(cs->semanticContext,
                                                genericType->name->name,
                                                ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                                astType->name);
            ZrParser_Semantic_RegisterInferredType(cs->semanticContext,
                                                   &namedType,
                                                   ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE,
                                                   namedType.typeName,
                                                   astType->name);
        }
    } else if (astType->name->type == ZR_AST_TUPLE_TYPE) {
        ZrParser_InferredType_Free(cs->state, &namedType);
        ZrParser_InferredType_Init(cs->state, &namedType, ZR_VALUE_TYPE_OBJECT);
    } else {
        ZrParser_InferredType_Free(cs->state, &namedType);
        ZrParser_InferredType_Init(cs->state, &namedType, ZR_VALUE_TYPE_OBJECT);
    }

    namedType.ownershipQualifier = astType->ownershipQualifier;

    if (astType->dimensions > 0) {
        SZrInferredType currentType;
        TZrSize fixedSize = 0;
        TZrSize minSize = 0;
        TZrSize maxSize = 0;
        TZrBool hasFixedSize = ZR_FALSE;

        ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(cs->state, &currentType, &namedType);
        ZrParser_InferredType_Free(cs->state, &namedType);
        namedTypeInitialized = ZR_FALSE;

        if (astType->hasArraySizeConstraint) {
            if (astType->arraySizeExpression != ZR_NULL) {
                if (!resolve_compile_time_array_size(cs, astType, &fixedSize)) {
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    return ZR_FALSE;
                }
                minSize = fixedSize;
                maxSize = fixedSize;
                hasFixedSize = ZR_TRUE;
            } else {
                fixedSize = astType->arrayFixedSize;
                minSize = astType->arrayMinSize;
                maxSize = astType->arrayMaxSize;
                hasFixedSize = fixedSize > 0 || (minSize > 0 && minSize == maxSize);
                if (!hasFixedSize && minSize > 0 && minSize == maxSize) {
                    fixedSize = minSize;
                    hasFixedSize = ZR_TRUE;
                }
            }
        }

        TZrSize dimensionCount = (TZrSize)astType->dimensions;
        for (TZrSize dim = 0; dim < dimensionCount; dim++) {
            SZrInferredType wrappedType;
            TZrBool isOutermost = (TZrBool)(dim + 1 == dimensionCount);

            if (!init_wrapped_array_inferred_type(cs,
                                                  &currentType,
                                                  isOutermost && astType->hasArraySizeConstraint,
                                                  isOutermost && hasFixedSize ? fixedSize : 0,
                                                  isOutermost ? minSize : 0,
                                                  isOutermost ? maxSize : 0,
                                                  &wrappedType)) {
                ZrParser_InferredType_Free(cs->state, &currentType);
                return ZR_FALSE;
            }

            ZrParser_InferredType_Free(cs->state, &currentType);
            ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_InferredType_Copy(cs->state, &currentType, &wrappedType);
            ZrParser_InferredType_Free(cs->state, &wrappedType);
        }

        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(cs->state, result, &currentType);
        result->ownershipQualifier = astType->ownershipQualifier;
        ZrParser_InferredType_Free(cs->state, &currentType);
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(cs->state, result, &namedType);
    result->ownershipQualifier = astType->ownershipQualifier;
    if (namedTypeInitialized) {
        ZrParser_InferredType_Free(cs->state, &namedType);
    }
    return ZR_TRUE;
}

// 获取类型的范围限制（用于整数类型）
static void get_type_range(EZrValueType baseType, TZrInt64 *minValue, TZrInt64 *maxValue) {
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
            *minValue = INT64_MIN;
            *maxValue = INT64_MAX;
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
TZrBool ZrParser_LiteralRange_Check(SZrCompilerState *cs, SZrAstNode *literalNode, const SZrInferredType *targetType, SZrFileRange location) {
    if (cs == ZR_NULL || literalNode == ZR_NULL || targetType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查整数类型字面量
    if (literalNode->type == ZR_AST_INTEGER_LITERAL && ZR_VALUE_IS_TYPE_INT(targetType->baseType)) {
        TZrInt64 literalValue = literalNode->data.integerLiteral.value;
        TZrInt64 minValue, maxValue;
        get_type_range(targetType->baseType, &minValue, &maxValue);
        
        if (literalValue < minValue || literalValue > maxValue) {
            static TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Integer literal %lld is out of range for type (expected range: %lld to %lld)",
                    (long long)literalValue, (long long)minValue, (long long)maxValue);
            ZrParser_TypeError_Report(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
        
        // 检查用户定义的范围约束
        if (targetType->hasRangeConstraint) {
            if (literalValue < targetType->minValue || literalValue > targetType->maxValue) {
                static TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
                snprintf(errorMsg, sizeof(errorMsg),
                        "Integer literal %lld is out of range constraint (expected range: %lld to %lld)",
                        (long long)literalValue, (long long)targetType->minValue, (long long)targetType->maxValue);
                ZrParser_TypeError_Report(cs, errorMsg, targetType, ZR_NULL, location);
                return ZR_FALSE;
            }
        }
    }
    
    // 检查浮点数类型字面量（NaN, Infinity）
    if (literalNode->type == ZR_AST_FLOAT_LITERAL) {
        TZrDouble floatValue = literalNode->data.floatLiteral.value;
        // 检查是否为 NaN 或 Infinity（如果目标类型不允许）
        // 根据目标类型决定是否允许 NaN/Infinity
        if (isnan(floatValue) || isinf(floatValue)) {
            // 检查目标类型是否允许NaN/Infinity
            // 对于整数类型，不允许NaN/Infinity
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(targetType->baseType) || 
                ZR_VALUE_IS_TYPE_UNSIGNED_INT(targetType->baseType)) {
                ZrParser_TypeError_Report(cs, "NaN/Infinity cannot be assigned to integer type", 
                                 targetType, ZR_NULL, location);
                return ZR_FALSE;
            }
            // 对于浮点类型，允许NaN/Infinity
        }
    }
    
    return ZR_TRUE;
}

// 检查数组索引边界
TZrBool ZrParser_ArrayIndexBounds_Check(SZrCompilerState *cs, SZrAstNode *indexExpr, const SZrInferredType *arrayType, SZrFileRange location) {
    if (cs == ZR_NULL || indexExpr == ZR_NULL || arrayType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 只对字面量索引进行编译期检查
    if (indexExpr->type == ZR_AST_INTEGER_LITERAL) {
        TZrInt64 indexValue = indexExpr->data.integerLiteral.value;
        
        if (indexValue < 0) {
            static TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array index %lld is negative", (long long)indexValue);
            ZrParser_TypeError_Report(cs, errorMsg, arrayType, ZR_NULL, location);
            return ZR_FALSE;
        }
        
        // 如果数组有固定大小，检查索引是否越界
        if (arrayType->hasArraySizeConstraint && arrayType->arrayFixedSize > 0) {
            if ((TZrSize)indexValue >= arrayType->arrayFixedSize) {
                static TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
                snprintf(errorMsg, sizeof(errorMsg),
                        "Array index %lld is out of bounds (array size: %zu)",
                        (long long)indexValue, arrayType->arrayFixedSize);
                ZrParser_TypeError_Report(cs, errorMsg, arrayType, ZR_NULL, location);
                return ZR_FALSE;
            }
        }
    }
    
    // 对于非字面量索引，编译期无法检查，将在运行时检查（如果启用）
    return ZR_TRUE;
}
