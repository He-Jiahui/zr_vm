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
#include "zr_vm_common/zr_type_conf.h"

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

static TZrBool type_inference_named_struct_is_move_only(SZrCompilerState *cs, SZrString *typeName, TZrUInt32 depth) {
    SZrTypePrototypeInfo *prototype;

    if (cs == ZR_NULL || typeName == ZR_NULL || depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_FALSE;
    }

    prototype = find_compiler_type_prototype_inference(cs, typeName);
    if (prototype == ZR_NULL || prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        return ZR_FALSE;
    }

    for (TZrSize memberIndex = 0; memberIndex < prototype->members.length; memberIndex++) {
        SZrTypeMemberInfo *memberInfo =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, memberIndex);

        if (memberInfo == ZR_NULL || memberInfo->memberType != ZR_AST_STRUCT_FIELD) {
            continue;
        }

        if (memberInfo->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE) {
            return ZR_TRUE;
        }

        if (memberInfo->fieldTypeName != ZR_NULL &&
            !ZrCore_String_Equal(memberInfo->fieldTypeName, typeName) &&
            type_inference_named_struct_is_move_only(cs, memberInfo->fieldTypeName, depth + 1)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool type_inference_inferred_type_is_move_only_struct(SZrCompilerState *cs, const SZrInferredType *type) {
    if (cs == ZR_NULL || type == ZR_NULL || type->baseType != ZR_VALUE_TYPE_OBJECT || type->typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    return type_inference_named_struct_is_move_only(cs, type->typeName, 0);
}

static TZrBool type_inference_report_move_only_struct_copy(SZrCompilerState *cs,
                                                           const SZrInferredType *type,
                                                           const TZrChar *context,
                                                           SZrFileRange location) {
    TZrChar errorMsg[ZR_PARSER_TEXT_BUFFER_LENGTH];
    const TZrChar *typeName = "struct";

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }

    if (type != ZR_NULL && type->typeName != ZR_NULL) {
        const TZrChar *candidateName = ZrCore_String_GetNativeString(type->typeName);
        if (candidateName != ZR_NULL) {
            typeName = candidateName;
        }
    }

    snprintf(errorMsg,
             sizeof(errorMsg),
             "Type Error: move-only struct '%s' cannot be implicitly copied during %s",
             typeName,
             context != ZR_NULL ? context : "this operation");
    ZrParser_Compiler_Error(cs, errorMsg, location);
    return ZR_FALSE;
}

// 检查类型兼容性（用于赋值等场景）
TZrBool ZrParser_TypeCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *fromType, const SZrInferredType *toType, SZrFileRange location) {
    if (cs == ZR_NULL || fromType == ZR_NULL || toType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    if (ZrParser_InferredType_IsCompatible(fromType, toType)) {
        return ZR_TRUE;
    }

    if (inferred_type_can_use_named_constraint_fallback(cs, fromType, toType)) {
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

    if (type_inference_inferred_type_is_move_only_struct(cs, rightType)) {
        return type_inference_report_move_only_struct_copy(cs, rightType, "assignment", location);
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

        if (!ZrParser_InferredType_IsCompatible(argType, paramType) &&
            !inferred_type_can_use_named_constraint_fallback(cs, argType, paramType) &&
            !ffi_function_call_argument_is_native_boundary_compatible(cs, funcType, i, argType, paramType)) {
            ZrParser_TypeError_Report(cs, "Argument type mismatch", paramType, argType, location);
            free_inferred_type_array(cs->state, &argTypes);
            return ZR_FALSE;
        }

        if (type_inference_inferred_type_is_move_only_struct(cs, argType)) {
            TZrBool reported =
                    type_inference_report_move_only_struct_copy(cs, argType, "argument passing", location);
            free_inferred_type_array(cs->state, &argTypes);
            return reported;
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
static SZrString *build_callable_type_name_from_signature(SZrState *state,
                                                          const SZrInferredType *returnType,
                                                          const SZrArray *paramTypes,
                                                          const SZrArray *parameterPassingModes);
static TZrBool infer_lambda_callable_signature(SZrCompilerState *cs,
                                               const SZrLambdaExpression *lambda,
                                               SZrArray *paramTypes,
                                               SZrArray *parameterPassingModes,
                                               SZrInferredType *returnType);
TZrBool ZrParser_LambdaType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    SZrArray paramTypes;
    SZrArray parameterPassingModes;
    SZrInferredType returnType;
    SZrString *callableName;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_LAMBDA_EXPRESSION) {
        return ZR_FALSE;
    }

    if (!infer_lambda_callable_signature(cs, &node->data.lambdaExpression, &paramTypes, &parameterPassingModes, &returnType)) {
        return ZR_FALSE;
    }

    callableName = build_callable_type_name_from_signature(cs->state, &returnType, &paramTypes, &parameterPassingModes);
    ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_CLOSURE, ZR_FALSE, callableName);

    free_inferred_type_array(cs->state, &paramTypes);
    if (parameterPassingModes.isValid) {
        ZrCore_Array_Free(cs->state, &parameterPassingModes);
    }
    ZrParser_InferredType_Free(cs->state, &returnType);
    return ZR_TRUE;
}

static const TZrChar *function_type_passing_mode_prefix(EZrParameterPassingMode passingMode) {
    switch (passingMode) {
        case ZR_PARAMETER_PASSING_MODE_IN: return "%in ";
        case ZR_PARAMETER_PASSING_MODE_OUT: return "%out ";
        case ZR_PARAMETER_PASSING_MODE_REF: return "%ref ";
        case ZR_PARAMETER_PASSING_MODE_VALUE:
        default:
            return "";
    }
}

static TZrBool function_type_name_append(TZrChar *buffer,
                                         TZrSize bufferSize,
                                         TZrSize *writeIndex,
                                         const TZrChar *text) {
    TZrSize length;

    if (buffer == ZR_NULL || writeIndex == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(text);
    if (*writeIndex + length >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer + *writeIndex, text, length);
    *writeIndex += length;
    buffer[*writeIndex] = '\0';
    return ZR_TRUE;
}

static SZrString *build_callable_type_name_from_signature(SZrState *state,
                                                          const SZrInferredType *returnType,
                                                          const SZrArray *paramTypes,
                                                          const SZrArray *parameterPassingModes) {
    TZrChar buffer[ZR_PARSER_DETAIL_BUFFER_LENGTH];
    TZrSize writeIndex = 0;
    TZrChar nestedBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];

    if (state == ZR_NULL || returnType == ZR_NULL) {
        return ZR_NULL;
    }

    buffer[0] = '\0';
    if (!function_type_name_append(buffer, sizeof(buffer), &writeIndex, "%func(")) {
        return ZR_NULL;
    }

    if (paramTypes != ZR_NULL) {
        for (TZrSize index = 0; index < paramTypes->length; index++) {
            SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)paramTypes, index);
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
            EZrParameterPassingMode *modePtr = parameterPassingModes != ZR_NULL
                                                       ? (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)parameterPassingModes,
                                                                                                     index)
                                                       : ZR_NULL;
            const TZrChar *paramText;

            if (index > 0 && !function_type_name_append(buffer, sizeof(buffer), &writeIndex, ", ")) {
                return ZR_NULL;
            }

            if (modePtr != ZR_NULL) {
                passingMode = *modePtr;
            }

            if (!function_type_name_append(buffer,
                                           sizeof(buffer),
                                           &writeIndex,
                                           function_type_passing_mode_prefix(passingMode))) {
                return ZR_NULL;
            }

            paramText = ZrParser_TypeNameString_Get(state,
                                                    paramType,
                                                    nestedBuffer,
                                                    sizeof(nestedBuffer));
            if (!function_type_name_append(buffer,
                                           sizeof(buffer),
                                           &writeIndex,
                                           paramText != ZR_NULL ? paramText : "unknown")) {
                return ZR_NULL;
            }
        }
    }

    if (!function_type_name_append(buffer, sizeof(buffer), &writeIndex, ")->")) {
        return ZR_NULL;
    }

    {
        const TZrChar *returnText =
                ZrParser_TypeNameString_Get(state, returnType, nestedBuffer, sizeof(nestedBuffer));
        if (!function_type_name_append(buffer,
                                       sizeof(buffer),
                                       &writeIndex,
                                       returnText != ZR_NULL ? returnText : "unknown")) {
            return ZR_NULL;
        }
    }

    return ZrCore_String_Create(state, buffer, writeIndex);
}

static TZrBool infer_function_type_node_signature(SZrCompilerState *cs,
                                                  const SZrFunctionType *funcType,
                                                  SZrArray *paramTypes,
                                                  SZrArray *parameterPassingModes,
                                                  SZrInferredType *returnType) {
    if (cs == ZR_NULL || funcType == ZR_NULL || paramTypes == ZR_NULL ||
        parameterPassingModes == ZR_NULL || returnType == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(paramTypes);
    ZrCore_Array_Construct(parameterPassingModes);

    ZrCore_Array_Init(cs->state,
                      paramTypes,
                      sizeof(SZrInferredType),
                      funcType->params != ZR_NULL ? funcType->params->count : 0);
    ZrCore_Array_Init(cs->state,
                      parameterPassingModes,
                      sizeof(EZrParameterPassingMode),
                      funcType->params != ZR_NULL ? funcType->params->count : 0);

    if (funcType->params != ZR_NULL) {
        for (TZrSize index = 0; index < funcType->params->count; index++) {
            SZrAstNode *paramNode = funcType->params->nodes[index];
            SZrInferredType paramType;
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                passingMode = param->passingMode;
                if (param->typeInfo != ZR_NULL &&
                    !ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                    ZrParser_InferredType_Free(cs->state, &paramType);
                    free_inferred_type_array(cs->state, paramTypes);
                    if (parameterPassingModes->isValid) {
                        ZrCore_Array_Free(cs->state, parameterPassingModes);
                    }
                    ZrCore_Array_Construct(paramTypes);
                    ZrCore_Array_Construct(parameterPassingModes);
                    return ZR_FALSE;
                }
            }

            ZrCore_Array_Push(cs->state, paramTypes, &paramType);
            ZrCore_Array_Push(cs->state, parameterPassingModes, &passingMode);
        }
    }

    ZrParser_InferredType_Init(cs->state, returnType, ZR_VALUE_TYPE_OBJECT);
    if (funcType->returnType != ZR_NULL) {
        return ZrParser_AstTypeToInferredType_Convert(cs, funcType->returnType, returnType);
    }
    return ZR_TRUE;
}

static void infer_lambda_return_type_register_variable_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    SZrVariableDeclaration *declaration;
    SZrInferredType bindingType;
    TZrBool hasBindingType = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_VARIABLE_DECLARATION) {
        return;
    }

    declaration = &node->data.variableDeclaration;
    if (declaration->pattern == ZR_NULL || declaration->pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        declaration->pattern->data.identifier.name == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    if (declaration->typeInfo != ZR_NULL) {
        hasBindingType = ZrParser_AstTypeToInferredType_Convert(cs, declaration->typeInfo, &bindingType);
    } else if (declaration->value != ZR_NULL) {
        hasBindingType = ZrParser_ExpressionType_Infer(cs, declaration->value, &bindingType);
    }

    if (cs->hasError) {
        ZrParser_InferredType_Free(cs->state, &bindingType);
        return;
    }

    if (!hasBindingType) {
        ZrParser_InferredType_Free(cs->state, &bindingType);
        ZrParser_InferredType_Init(cs->state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    }

    ZrParser_TypeEnvironment_RegisterVariable(cs->state,
                                              cs->typeEnv,
                                              declaration->pattern->data.identifier.name,
                                              &bindingType);
    ZrParser_InferredType_Free(cs->state, &bindingType);
}

static void infer_lambda_return_type_predeclare_block_functions(SZrCompilerState *cs, SZrAstNodeArray *statements) {
    if (cs == ZR_NULL || statements == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < statements->count; index++) {
        SZrAstNode *statement = statements->nodes[index];
        SZrAstNode *previousFunctionNode;

        if (statement == ZR_NULL || statement->type != ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }

        previousFunctionNode = cs->currentFunctionNode;
        cs->currentFunctionNode = statement;
        compiler_register_function_type_binding(cs, &statement->data.functionDeclaration);
        cs->currentFunctionNode = previousFunctionNode;
        if (cs->hasError) {
            return;
        }
    }
}

static void infer_lambda_return_type_from_node(SZrCompilerState *cs,
                                               SZrAstNode *node,
                                               TZrBool *foundReturn,
                                               SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || foundReturn == ZR_NULL || result == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            SZrTypeEnvironment *savedEnv;
            SZrTypeEnvironment *blockEnv;
            if (block->body == ZR_NULL) {
                return;
            }

            savedEnv = cs->typeEnv;
            blockEnv = ZrParser_TypeEnvironment_New(cs->state);
            if (blockEnv != ZR_NULL) {
                blockEnv->parent = savedEnv;
                cs->typeEnv = blockEnv;
            }

            infer_lambda_return_type_predeclare_block_functions(cs, block->body);
            for (TZrSize index = 0; index < block->body->count; index++) {
                infer_lambda_return_type_from_node(cs, block->body->nodes[index], foundReturn, result);
                if (cs->hasError || *foundReturn) {
                    break;
                }
            }

            cs->typeEnv = savedEnv;
            if (blockEnv != ZR_NULL) {
                ZrParser_TypeEnvironment_Free(cs->state, blockEnv);
            }
            return;
        }
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStmt = &node->data.returnStatement;
            if (returnStmt->expr != ZR_NULL) {
                ZrParser_ExpressionType_Infer(cs, returnStmt->expr, result);
            } else {
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_NULL);
            }
            *foundReturn = ZR_TRUE;
            return;
        }
        case ZR_AST_VARIABLE_DECLARATION:
            infer_lambda_return_type_register_variable_declaration(cs, node);
            return;
        case ZR_AST_IF_EXPRESSION: {
            infer_lambda_return_type_from_node(cs, node->data.ifExpression.thenExpr, foundReturn, result);
            if (*foundReturn) {
                return;
            }
            infer_lambda_return_type_from_node(cs, node->data.ifExpression.elseExpr, foundReturn, result);
            return;
        }
        default:
            return;
    }
}

static TZrBool infer_lambda_callable_signature(SZrCompilerState *cs,
                                               const SZrLambdaExpression *lambda,
                                               SZrArray *paramTypes,
                                               SZrArray *parameterPassingModes,
                                               SZrInferredType *returnType) {
    SZrTypeEnvironment *savedEnv;
    SZrTypeEnvironment *lambdaEnv;

    if (cs == ZR_NULL || lambda == ZR_NULL || paramTypes == ZR_NULL ||
        parameterPassingModes == ZR_NULL || returnType == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(paramTypes);
    ZrCore_Array_Construct(parameterPassingModes);
    ZrCore_Array_Init(cs->state,
                      paramTypes,
                      sizeof(SZrInferredType),
                      lambda->params != ZR_NULL ? lambda->params->count : 0);
    ZrCore_Array_Init(cs->state,
                      parameterPassingModes,
                      sizeof(EZrParameterPassingMode),
                      lambda->params != ZR_NULL ? lambda->params->count : 0);

    if (lambda->params != ZR_NULL) {
        for (TZrSize index = 0; index < lambda->params->count; index++) {
            SZrAstNode *paramNode = lambda->params->nodes[index];
            SZrInferredType paramType;
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                passingMode = param->passingMode;
                if (param->typeInfo != ZR_NULL &&
                    !ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                    ZrParser_InferredType_Free(cs->state, &paramType);
                    free_inferred_type_array(cs->state, paramTypes);
                    if (parameterPassingModes->isValid) {
                        ZrCore_Array_Free(cs->state, parameterPassingModes);
                    }
                    ZrCore_Array_Construct(paramTypes);
                    ZrCore_Array_Construct(parameterPassingModes);
                    return ZR_FALSE;
                }
            }

            ZrCore_Array_Push(cs->state, paramTypes, &paramType);
            ZrCore_Array_Push(cs->state, parameterPassingModes, &passingMode);
        }
    }

    ZrParser_InferredType_Init(cs->state, returnType, ZR_VALUE_TYPE_OBJECT);
    {
        TZrBool foundReturn = ZR_FALSE;
        savedEnv = cs->typeEnv;
        lambdaEnv = ZrParser_TypeEnvironment_New(cs->state);
        if (lambdaEnv != ZR_NULL) {
            lambdaEnv->parent = savedEnv;
            cs->typeEnv = lambdaEnv;
            if (lambda->params != ZR_NULL) {
                for (TZrSize index = 0; index < lambda->params->count; index++) {
                    SZrAstNode *paramNode = lambda->params->nodes[index];
                    SZrInferredType *paramType =
                            (SZrInferredType *)ZrCore_Array_Get(paramTypes, index);
                    if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER &&
                        paramNode->data.parameter.name != ZR_NULL &&
                        paramNode->data.parameter.name->name != ZR_NULL &&
                        paramType != ZR_NULL) {
                        ZrParser_TypeEnvironment_RegisterVariable(cs->state,
                                                                  lambdaEnv,
                                                                  paramNode->data.parameter.name->name,
                                                                  paramType);
                    }
                }
            }
        }
        infer_lambda_return_type_from_node(cs, lambda->block, &foundReturn, returnType);
        cs->typeEnv = savedEnv;
        if (lambdaEnv != ZR_NULL) {
            ZrParser_TypeEnvironment_Free(cs->state, lambdaEnv);
        }
        if (!foundReturn) {
            ZrParser_InferredType_Init(cs->state, returnType, ZR_VALUE_TYPE_NULL);
        }
    }

    return ZR_TRUE;
}

static TZrBool infer_type_literal_expression_type(SZrCompilerState *cs,
                                                  SZrAstNode *node,
                                                  SZrInferredType *result) {
    SZrString *typeName;
    static const TZrChar *kBuiltinTypeInfoName = "zr.builtin.TypeInfo";

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL ||
        node->type != ZR_AST_TYPE_LITERAL_EXPRESSION ||
        node->data.typeLiteralExpression.typeInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    typeName = ZrCore_String_Create(cs->state, kBuiltinTypeInfoName, strlen(kBuiltinTypeInfoName));
    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    ensure_builtin_reflection_compile_type(cs, typeName);
    ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
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
                    if (!ZrParser_FunctionCallCompatibility_Check(cs,
                                                                  cs->typeEnv,
                                                                  funcName,
                                                                  call,
                                                                  funcTypeInfo,
                                                                  &resolvedSignature,
                                                                  node->location)) {
                        ZrParser_InferredType_Free(cs->state, &baseType);
                        free_resolved_call_signature(cs->state, &resolvedSignature);
                        return ZR_FALSE;
                    }
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
                    if (!ZrParser_FunctionCallCompatibility_Check(cs,
                                                                  cs->compileTimeTypeEnv,
                                                                  funcName,
                                                                  call,
                                                                  funcTypeInfo,
                                                                  &resolvedSignature,
                                                                  node->location)) {
                        ZrParser_InferredType_Free(cs->state, &baseType);
                        free_resolved_call_signature(cs->state, &resolvedSignature);
                        return ZR_FALSE;
                    }
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

                if (primary->property != ZR_NULL) {
                    SZrTypePrototypeInfo *basePrototype = ZR_NULL;
                    SZrString *basePrototypeTypeName = ZR_NULL;
                    TZrBool baseIsPrototypeReference = ZR_FALSE;

                    if (ZrParser_ExpressionType_Infer(cs, primary->property, &baseType)) {
                        baseIsPrototypeReference =
                                resolve_prototype_target_inference(cs,
                                                                   primary->property,
                                                                   &basePrototype,
                                                                   &basePrototypeTypeName);
                        ZR_UNUSED_PARAMETER(basePrototype);
                        ZR_UNUSED_PARAMETER(basePrototypeTypeName);
                        if (infer_primary_member_chain_type(cs,
                                                            &baseType,
                                                            primary->members,
                                                            0,
                                                            baseIsPrototypeReference,
                                                            result)) {
                            ZrParser_InferredType_Free(cs->state, &baseType);
                            free_resolved_call_signature(cs->state, &resolvedSignature);
                            return ZR_TRUE;
                        }
                    }
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

        case ZR_AST_TYPE_LITERAL_EXPRESSION:
            return infer_type_literal_expression_type(cs, node, result);
        
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

static TZrBool ast_type_should_preserve_primitive_alias(const TZrNativeString nameStr,
                                                        TZrSize nameLen);

static const TZrChar *ast_type_builtin_canonical_name(const TZrChar *typeNameText) {
    if (typeNameText == ZR_NULL) {
        return ZR_NULL;
    }

    if (strcmp(typeNameText, "Iterable") == 0) {
        return "zr.builtin.IEnumerable";
    }
    if (strcmp(typeNameText, "Iterator") == 0) {
        return "zr.builtin.IEnumerator";
    }
    if (strcmp(typeNameText, "ArrayLike") == 0) {
        return "zr.builtin.IArrayLike";
    }
    if (strcmp(typeNameText, "Equatable") == 0) {
        return "zr.builtin.IEquatable";
    }
    if (strcmp(typeNameText, "Hashable") == 0) {
        return "zr.builtin.IHashable";
    }
    if (strcmp(typeNameText, "Comparable") == 0) {
        return "zr.builtin.IComparable";
    }
    if (strcmp(typeNameText, "zr.system.reflect.Type") == 0 ||
        strcmp(typeNameText, "zr.system.reflect.CallableType") == 0 ||
        strcmp(typeNameText, "TypeInfo") == 0) {
        return "zr.builtin.TypeInfo";
    }
    if (strcmp(typeNameText, "IEnumerable") == 0 ||
        strcmp(typeNameText, "IEnumerator") == 0 ||
        strcmp(typeNameText, "IArrayLike") == 0 ||
        strcmp(typeNameText, "IEquatable") == 0 ||
        strcmp(typeNameText, "IHashable") == 0 ||
        strcmp(typeNameText, "IComparable") == 0 ||
        strcmp(typeNameText, "IComparer") == 0 ||
        strcmp(typeNameText, "Object") == 0 ||
        strcmp(typeNameText, "Module") == 0 ||
        strcmp(typeNameText, "Integer") == 0 ||
        strcmp(typeNameText, "Float") == 0 ||
        strcmp(typeNameText, "Double") == 0 ||
        strcmp(typeNameText, "String") == 0 ||
        strcmp(typeNameText, "Bool") == 0 ||
        strcmp(typeNameText, "Byte") == 0 ||
        strcmp(typeNameText, "Char") == 0 ||
        strcmp(typeNameText, "UInt64") == 0) {
        static TZrChar buffer[64];
        snprintf(buffer, sizeof(buffer), "zr.builtin.%s", typeNameText);
        return buffer;
    }

    return ZR_NULL;
}

static TZrBool ast_type_is_reserved_decorator_pseudo_type_name(const TZrNativeString nameStr,
                                                               TZrSize nameLen) {
    if (nameStr == ZR_NULL) {
        return ZR_FALSE;
    }

    return (nameLen == 5 && strncmp(nameStr, "Class", 5) == 0) ||
           (nameLen == 6 && strncmp(nameStr, "Struct", 6) == 0) ||
           (nameLen == 8 && strncmp(nameStr, "Function", 8) == 0) ||
           (nameLen == 5 && strncmp(nameStr, "Field", 5) == 0) ||
           (nameLen == 6 && strncmp(nameStr, "Method", 6) == 0) ||
           (nameLen == 8 && strncmp(nameStr, "Property", 8) == 0) ||
           (nameLen == 9 && strncmp(nameStr, "Parameter", 9) == 0) ||
           (nameLen == 6 && strncmp(nameStr, "Object", 6) == 0);
}

static TZrBool compiler_lookup_type_value_alias(SZrCompilerState *cs,
                                                SZrString *name,
                                                SZrInferredType *result) {
    if (cs == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->typeValueAliases.length; index++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeValueAliases, index);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            ZrParser_InferredType_Copy(cs->state, result, &binding->type);
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool ast_type_report_missing_explicit_binding(SZrCompilerState *cs,
                                                        SZrString *typeName,
                                                        SZrFileRange location) {
    TZrNativeString typeNameText = ZR_NULL;
    TZrChar errorBuffer[ZR_PARSER_ERROR_BUFFER_LENGTH];
    const TZrChar *canonicalBuiltinName;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        typeNameText = ZrCore_String_GetNativeStringShort(typeName);
    } else {
        typeNameText = ZrCore_String_GetNativeString(typeName);
    }

    canonicalBuiltinName = ast_type_builtin_canonical_name(typeNameText);
    if (typeNameText != ZR_NULL && canonicalBuiltinName != ZR_NULL) {
        snprintf(errorBuffer,
                 sizeof(errorBuffer),
                 "Type name '%s' is not implicitly visible; use '%s' via explicit import or module qualifier",
                 typeNameText,
                 canonicalBuiltinName);
    } else if (typeNameText != ZR_NULL) {
        snprintf(errorBuffer,
                 sizeof(errorBuffer),
                 "Unqualified type name '%s' requires an explicit module qualifier or destructuring import",
                 typeNameText);
    } else {
        snprintf(errorBuffer,
                 sizeof(errorBuffer),
                 "Unqualified type name requires an explicit module qualifier or destructuring import");
    }

    ZrParser_Compiler_Error(cs, errorBuffer, location);
    return ZR_FALSE;
}

static TZrBool ast_type_resolve_unqualified_inferred_type(SZrCompilerState *cs,
                                                          const SZrType *astType,
                                                          SZrInferredType *result) {
    if (cs == ZR_NULL || astType == ZR_NULL || result == ZR_NULL || astType->name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (astType->name->type == ZR_AST_FUNCTION_TYPE) {
        SZrArray paramTypes;
        SZrArray parameterPassingModes;
        SZrInferredType returnType;
        SZrString *callableName;

        if (!infer_function_type_node_signature(cs,
                                                &astType->name->data.functionType,
                                                &paramTypes,
                                                &parameterPassingModes,
                                                &returnType)) {
            return ZR_FALSE;
        }

        callableName = build_callable_type_name_from_signature(cs->state,
                                                               &returnType,
                                                               &paramTypes,
                                                               &parameterPassingModes);
        ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_CLOSURE, ZR_FALSE, callableName);
        result->ownershipQualifier = astType->ownershipQualifier;

        free_inferred_type_array(cs->state, &paramTypes);
        if (parameterPassingModes.isValid) {
            ZrCore_Array_Free(cs->state, &parameterPassingModes);
        }
        ZrParser_InferredType_Free(cs->state, &returnType);
        return ZR_TRUE;
    }

    if (astType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *typeName = astType->name->data.identifier.name;
        EZrValueType baseType = ZR_VALUE_TYPE_OBJECT;
        TZrNativeString nameStr = ZR_NULL;
        TZrSize nameLen = 0;
        TZrBool isPrimitiveAlias = ZR_FALSE;

        if (typeName == ZR_NULL) {
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            return ZR_TRUE;
        }

        if (compiler_lookup_type_value_alias(cs, typeName, result)) {
            result->ownershipQualifier = astType->ownershipQualifier;
            return ZR_TRUE;
        }

        if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            nameStr = ZrCore_String_GetNativeStringShort(typeName);
            nameLen = typeName->shortStringLength;
        } else {
            nameStr = ZrCore_String_GetNativeString(typeName);
            nameLen = typeName->longStringLength;
        }

        isPrimitiveAlias = inferred_type_try_map_primitive_name(nameStr, nameLen, &baseType);
        if (isPrimitiveAlias) {
            if (ast_type_should_preserve_primitive_alias(nameStr, nameLen)) {
                ZrParser_InferredType_InitFull(cs->state, result, baseType, ZR_FALSE, typeName);
            } else {
                ZrParser_InferredType_Init(cs->state, result, baseType);
            }
        } else {
            TZrBool allowReservedDecoratorPseudoType =
                    astType->isDecoratorPseudoType &&
                    ast_type_is_reserved_decorator_pseudo_type_name(nameStr, nameLen);
            TZrBool allowImplicitBuiltinType = astType->isImplicitBuiltinType;

            if (!allowReservedDecoratorPseudoType &&
                !allowImplicitBuiltinType &&
                !type_name_is_explicitly_available_in_context_inference(cs, typeName)) {
                return ast_type_report_missing_explicit_binding(cs, typeName, astType->name->location);
            }
            ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
        }

        result->ownershipQualifier = astType->ownershipQualifier;
        if (!isPrimitiveAlias && cs->semanticContext != ZR_NULL) {
            ZrParser_Semantic_RegisterNamedType(cs->semanticContext,
                                                typeName,
                                                ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                                astType->name);
            ZrParser_Semantic_RegisterInferredType(cs->semanticContext,
                                                   result,
                                                   ZR_SEMANTIC_TYPE_KIND_REFERENCE,
                                                   result->typeName,
                                                   astType->name);
        }

        return ZR_TRUE;
    }

    if (astType->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &astType->name->data.genericType;
        SZrString *canonicalName;

        if (genericType->name == ZR_NULL || genericType->name->name == ZR_NULL) {
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            return ZR_TRUE;
        }

        if (!astType->isImplicitBuiltinType &&
            !type_name_is_explicitly_available_in_context_inference(cs, genericType->name->name)) {
            return ast_type_report_missing_explicit_binding(cs,
                                                            genericType->name->name,
                                                            astType->name->location);
        }

        ZrParser_InferredType_InitFull(cs->state,
                                       result,
                                       ZR_VALUE_TYPE_OBJECT,
                                       ZR_FALSE,
                                       genericType->name->name);

        if (genericType->params != ZR_NULL && genericType->params->count > 0) {
            ZrCore_Array_Init(cs->state,
                              &result->elementTypes,
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
                        ZrParser_InferredType_Free(cs->state, result);
                        return ZR_FALSE;
                    }
                } else {
                    argumentName = extract_generic_argument_name_string(cs, paramNode);
                    if (argumentName == ZR_NULL) {
                        ZrParser_InferredType_Free(cs->state, &paramType);
                        ZrParser_InferredType_Free(cs->state, result);
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

                ZrCore_Array_Push(cs->state, &result->elementTypes, &paramType);
            }
        }

        canonicalName = build_generic_instance_name(cs->state,
                                                    genericType->name->name,
                                                    &result->elementTypes);
        if (canonicalName != ZR_NULL) {
            result->typeName = canonicalName;
        }

        result->ownershipQualifier = astType->ownershipQualifier;
        if (cs->semanticContext != ZR_NULL) {
            ZrParser_Semantic_RegisterNamedType(cs->semanticContext,
                                                genericType->name->name,
                                                ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                                astType->name);
            ZrParser_Semantic_RegisterInferredType(cs->semanticContext,
                                                   result,
                                                   ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE,
                                                   result->typeName,
                                                   astType->name);
        }
        return ZR_TRUE;
    }

    if (astType->name->type == ZR_AST_TUPLE_TYPE) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

static TZrBool ast_type_should_preserve_primitive_alias(const TZrNativeString nameStr,
                                                        TZrSize nameLen) {
    if (nameStr == ZR_NULL) {
        return ZR_FALSE;
    }

    return (nameLen == 2 &&
            (memcmp(nameStr, "i8", 2) == 0 || memcmp(nameStr, "u8", 2) == 0)) ||
           (nameLen == 3 &&
            (memcmp(nameStr, "i16", 3) == 0 || memcmp(nameStr, "u16", 3) == 0 ||
             memcmp(nameStr, "i32", 3) == 0 || memcmp(nameStr, "u32", 3) == 0 ||
             memcmp(nameStr, "i64", 3) == 0 || memcmp(nameStr, "u64", 3) == 0 ||
             memcmp(nameStr, "f32", 3) == 0 || memcmp(nameStr, "f64", 3) == 0));
}

static TZrBool ast_type_resolve_segment_type_names(SZrCompilerState *cs,
                                                   const SZrType *segmentType,
                                                   SZrString **outLookupName,
                                                   SZrString **outResolvedTypeName) {
    if (outLookupName != ZR_NULL) {
        *outLookupName = ZR_NULL;
    }
    if (outResolvedTypeName != ZR_NULL) {
        *outResolvedTypeName = ZR_NULL;
    }
    if (cs == ZR_NULL || segmentType == ZR_NULL || segmentType->name == ZR_NULL ||
        outLookupName == ZR_NULL || outResolvedTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (segmentType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        *outLookupName = segmentType->name->data.identifier.name;
        *outResolvedTypeName = segmentType->name->data.identifier.name;
        return *outLookupName != ZR_NULL;
    }

    if (segmentType->name->type == ZR_AST_GENERIC_TYPE) {
        if (segmentType->name->data.genericType.name == ZR_NULL) {
            return ZR_FALSE;
        }

        *outLookupName = segmentType->name->data.genericType.name->name;
        if (*outLookupName == ZR_NULL) {
            return ZR_FALSE;
        }

        *outResolvedTypeName = extract_type_name_string(cs, (SZrType *)segmentType);
        return *outResolvedTypeName != ZR_NULL;
    }

    return ZR_FALSE;
}

static SZrString *ast_type_build_qualified_module_member_type_name(SZrCompilerState *cs,
                                                                   SZrString *moduleTypeName,
                                                                   SZrString *memberResolvedTypeName) {
    TZrNativeString moduleTypeText;
    TZrNativeString memberTypeText;
    const TZrChar *memberGenericStart;
    const TZrChar *memberRootDot;
    TZrChar buffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 written;

    if (cs == ZR_NULL || moduleTypeName == ZR_NULL || memberResolvedTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    moduleTypeText = ZrCore_String_GetNativeString(moduleTypeName);
    memberTypeText = ZrCore_String_GetNativeString(memberResolvedTypeName);
    memberGenericStart = memberTypeText != ZR_NULL ? strchr(memberTypeText, '<') : ZR_NULL;
    memberRootDot = memberTypeText != ZR_NULL
                            ? memchr(memberTypeText,
                                     '.',
                                     memberGenericStart != ZR_NULL ? (size_t)(memberGenericStart - memberTypeText)
                                                                   : strlen(memberTypeText))
                            : ZR_NULL;
    if (moduleTypeText == ZR_NULL || memberTypeText == ZR_NULL || memberRootDot != ZR_NULL) {
        return ZR_NULL;
    }

    written = snprintf(buffer, sizeof(buffer), "%s.%s", moduleTypeText, memberTypeText);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)written);
}

static SZrString *ast_type_build_canonical_module_member_type_name(SZrCompilerState *cs,
                                                                   SZrString *canonicalMemberTypeName,
                                                                   SZrString *requestedMemberTypeName) {
    const TZrChar *canonicalText;
    const TZrChar *requestedText;
    const TZrChar *requestedGenericStart;
    const TZrChar *canonicalGenericStart;
    TZrSize canonicalBaseLength;
    TZrChar buffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 written;

    if (cs == ZR_NULL || canonicalMemberTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    canonicalText = ZrCore_String_GetNativeString(canonicalMemberTypeName);
    if (canonicalText == ZR_NULL) {
        return ZR_NULL;
    }

    requestedText = requestedMemberTypeName != ZR_NULL ? ZrCore_String_GetNativeString(requestedMemberTypeName) : ZR_NULL;
    requestedGenericStart = requestedText != ZR_NULL ? strchr(requestedText, '<') : ZR_NULL;
    if (requestedGenericStart == ZR_NULL) {
        return canonicalMemberTypeName;
    }

    canonicalGenericStart = strchr(canonicalText, '<');
    canonicalBaseLength = canonicalGenericStart != ZR_NULL ? (TZrSize)(canonicalGenericStart - canonicalText)
                                                           : strlen(canonicalText);
    written = snprintf(buffer,
                       sizeof(buffer),
                       "%.*s%s",
                       (int)canonicalBaseLength,
                       canonicalText,
                       requestedGenericStart);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)written);
}

static TZrBool ast_type_try_resolve_qualified_inferred_type(SZrCompilerState *cs,
                                                            const SZrType *astType,
                                                            SZrInferredType *result) {
    const SZrType *segmentType;
    SZrInferredType currentType;
    TZrBool resolvedRootFromModuleAlias = ZR_FALSE;

    if (cs == ZR_NULL || astType == ZR_NULL || astType->name == ZR_NULL || astType->subType == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);

    if (astType->name->type == ZR_AST_IDENTIFIER_LITERAL &&
        cs->typeEnv != ZR_NULL &&
        ZrParser_TypeEnvironment_LookupVariable(cs->state,
                                                cs->typeEnv,
                                                astType->name->data.identifier.name,
                                                &currentType) &&
        currentType.typeName != ZR_NULL) {
        ensure_import_module_compile_info(cs, currentType.typeName);
        resolvedRootFromModuleAlias = type_name_is_module_prototype_inference(cs, currentType.typeName);
    }

    if (!resolvedRootFromModuleAlias) {
        SZrString *qualifiedTypeName = extract_type_name_string(cs, (SZrType *)astType);
        if (qualifiedTypeName != ZR_NULL) {
            ensure_generic_instance_type_prototype(cs, qualifiedTypeName);
            if (find_compiler_type_prototype_inference(cs, qualifiedTypeName) != ZR_NULL) {
                ZrParser_InferredType_Free(cs->state, &currentType);
                return inferred_type_from_type_name(cs, qualifiedTypeName, result);
            }
            if (ast_type_builtin_canonical_name(ZrCore_String_GetNativeString(qualifiedTypeName)) != ZR_NULL) {
                ZrParser_InferredType_Free(cs->state, &currentType);
                return ast_type_report_missing_explicit_binding(cs, qualifiedTypeName, astType->name->location);
            }
        }

        ZrParser_InferredType_Free(cs->state, &currentType);
        if (!ast_type_resolve_unqualified_inferred_type(cs, astType, &currentType)) {
            return ZR_FALSE;
        }
    }

    segmentType = astType->subType;
    while (segmentType != ZR_NULL) {
        SZrString *memberLookupName = ZR_NULL;
        SZrString *memberResolvedTypeName = ZR_NULL;
        SZrTypeMemberInfo *memberInfo;
        SZrInferredType nextType;
        TZrBool resolvedFromModuleMember = ZR_FALSE;

        if (currentType.typeName == ZR_NULL ||
            !ast_type_resolve_segment_type_names(cs,
                                                 segmentType,
                                                 &memberLookupName,
                                                 &memberResolvedTypeName)) {
            ZrParser_InferredType_Free(cs->state, &currentType);
            return ZR_FALSE;
        }

        memberInfo = find_compiler_type_member_inference(cs, currentType.typeName, memberLookupName);
        if (memberInfo == ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, &currentType);
            return ZR_FALSE;
        }

        ZrParser_InferredType_Init(cs->state, &nextType, ZR_VALUE_TYPE_OBJECT);
        if (type_name_is_module_prototype_inference(cs, currentType.typeName) &&
            memberResolvedTypeName != ZR_NULL) {
            SZrString *qualifiedMemberTypeName =
                    ast_type_build_canonical_module_member_type_name(cs, memberInfo->fieldTypeName, memberResolvedTypeName);
            SZrString *candidateTypeName =
                    qualifiedMemberTypeName != ZR_NULL ? qualifiedMemberTypeName
                                                       : (memberInfo->fieldTypeName != ZR_NULL
                                                                  ? memberInfo->fieldTypeName
                                                                  : memberResolvedTypeName);

            ensure_generic_instance_type_prototype(cs, candidateTypeName);
            if (find_compiler_type_prototype_inference(cs, candidateTypeName) != ZR_NULL &&
                inferred_type_from_type_name(cs, candidateTypeName, &nextType)) {
                resolvedFromModuleMember = ZR_TRUE;
            }
        }

        if (!resolvedFromModuleMember) {
            SZrString *memberTypeName = memberInfo->fieldTypeName != ZR_NULL
                                                ? memberInfo->fieldTypeName
                                                : memberInfo->returnTypeName;
            if (memberTypeName == ZR_NULL) {
                ZrParser_InferredType_Free(cs->state, &nextType);
                ZrParser_InferredType_Free(cs->state, &currentType);
                return ZR_FALSE;
            }

            ensure_generic_instance_type_prototype(cs, memberTypeName);
            if (!inferred_type_from_type_name(cs, memberTypeName, &nextType)) {
                ZrParser_InferredType_Free(cs->state, &nextType);
                ZrParser_InferredType_Free(cs->state, &currentType);
                return ZR_FALSE;
            }
        }

        ZrParser_InferredType_Free(cs->state, &currentType);
        ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(cs->state, &currentType, &nextType);
        ZrParser_InferredType_Free(cs->state, &nextType);
        segmentType = segmentType->subType;
    }

    ZrParser_InferredType_Copy(cs->state, result, &currentType);
    ZrParser_InferredType_Free(cs->state, &currentType);
    return ZR_TRUE;
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

    if (astType->subType != ZR_NULL) {
        if (ast_type_try_resolve_qualified_inferred_type(cs, astType, &namedType)) {
            if (cs->semanticContext != ZR_NULL && namedType.typeName != ZR_NULL) {
                ZrParser_Semantic_RegisterInferredType(cs->semanticContext,
                                                       &namedType,
                                                       ZR_SEMANTIC_TYPE_KIND_REFERENCE,
                                                       namedType.typeName,
                                                       astType->name);
            }
        } else if (cs->hasError) {
            ZrParser_InferredType_Free(cs->state, &namedType);
            return ZR_FALSE;
        } else if (!ast_type_resolve_unqualified_inferred_type(cs, astType, &namedType)) {
            ZrParser_InferredType_Free(cs->state, &namedType);
            return ZR_FALSE;
        }
    } else if (!ast_type_resolve_unqualified_inferred_type(cs, astType, &namedType)) {
        ZrParser_InferredType_Free(cs->state, &namedType);
        return ZR_FALSE;
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
            *minValue = ZR_TYPE_RANGE_INT8_MIN;
            *maxValue = ZR_TYPE_RANGE_INT8_MAX;
            break;
        case ZR_VALUE_TYPE_INT16:
            *minValue = ZR_TYPE_RANGE_INT16_MIN;
            *maxValue = ZR_TYPE_RANGE_INT16_MAX;
            break;
        case ZR_VALUE_TYPE_INT32:
            *minValue = ZR_TYPE_RANGE_INT32_MIN;
            *maxValue = ZR_TYPE_RANGE_INT32_MAX;
            break;
        case ZR_VALUE_TYPE_INT64:
            *minValue = ZR_TYPE_RANGE_INT64_MIN;
            *maxValue = ZR_TYPE_RANGE_INT64_MAX;
            break;
        case ZR_VALUE_TYPE_UINT8:
            *minValue = 0;
            *maxValue = ZR_TYPE_RANGE_UINT8_MAX;
            break;
        case ZR_VALUE_TYPE_UINT16:
            *minValue = 0;
            *maxValue = ZR_TYPE_RANGE_UINT16_MAX;
            break;
        case ZR_VALUE_TYPE_UINT32:
            *minValue = 0;
            *maxValue = ZR_TYPE_RANGE_UINT32_MAX;
            break;
        case ZR_VALUE_TYPE_UINT64:
            *minValue = 0;
            *maxValue = ZR_TYPE_RANGE_UINT64_MAX;
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
