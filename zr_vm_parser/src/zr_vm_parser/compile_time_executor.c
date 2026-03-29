//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

typedef struct SZrCompileTimeBinding {
    SZrString *name;
    SZrTypeValue value;
} SZrCompileTimeBinding;

typedef struct SZrCompileTimeFrame {
    SZrArray bindings;
    struct SZrCompileTimeFrame *parent;
} SZrCompileTimeFrame;

static TZrBool evaluate_compile_time_expression_internal(SZrCompilerState *cs,
                                                       SZrAstNode *node,
                                                       SZrCompileTimeFrame *frame,
                                                       SZrTypeValue *result);
static TZrBool execute_compile_time_statement(SZrCompilerState *cs,
                                            SZrAstNode *node,
                                            SZrCompileTimeFrame *frame,
                                            TZrBool *didReturn,
                                            SZrTypeValue *result);
static TZrBool execute_compile_time_block(SZrCompilerState *cs,
                                        SZrAstNode *node,
                                        SZrCompileTimeFrame *frame,
                                        TZrBool *didReturn,
                                        SZrTypeValue *result);
static TZrBool register_compile_time_variable_declaration(SZrCompilerState *cs,
                                                        SZrAstNode *node,
                                                        SZrFileRange location);
static TZrBool register_compile_time_function_declaration(SZrCompilerState *cs,
                                                        SZrAstNode *node,
                                                        SZrFileRange location);
static TZrBool ct_eval_object_literal(SZrCompilerState *cs,
                                    SZrAstNode *node,
                                    SZrCompileTimeFrame *frame,
                                    SZrTypeValue *result);
static TZrBool ct_eval_member_access(SZrCompilerState *cs,
                                   SZrAstNode *callSite,
                                   const SZrTypeValue *baseValue,
                                   SZrMemberExpression *memberExpr,
                                   SZrCompileTimeFrame *frame,
                                   SZrTypeValue *result);
static TZrBool ct_call_value(SZrCompilerState *cs,
                           SZrAstNode *callSite,
                           const SZrTypeValue *callableValue,
                           SZrFunctionCall *call,
                           SZrCompileTimeFrame *frame,
                           SZrTypeValue *result);

static const TZrChar *ct_name(SZrString *name) {
    if (name == ZR_NULL) {
        return "<null>";
    }
    TZrNativeString nameStr = ZrCore_String_GetNativeString(name);
    return nameStr != ZR_NULL ? nameStr : "<null>";
}

static void ct_error_name(SZrCompilerState *cs, SZrString *name, const TZrChar *prefix, SZrFileRange location) {
    TZrChar msg[256];
    snprintf(msg, sizeof(msg), "%s%s", prefix, ct_name(name));
    ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, msg, location);
}

static TZrBool ct_truthy(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            return ZR_FALSE;
        case ZR_VALUE_TYPE_BOOL:
            return value->value.nativeObject.nativeBool != 0;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return value->value.nativeObject.nativeDouble != 0.0;
        default:
            if (ZR_VALUE_IS_TYPE_INT(value->type)) {
                return value->value.nativeObject.nativeInt64 != 0;
            }
            return ZR_TRUE;
    }
}

static void ct_init_type_from_value(SZrCompilerState *cs, const SZrTypeValue *value, SZrInferredType *result) {
    ZrParser_InferredType_Init(cs->state, result, value != ZR_NULL ? value->type : ZR_VALUE_TYPE_OBJECT);
}

static TZrBool ct_string_equals(SZrString *value, const TZrChar *literal) {
    TZrNativeString nativeValue;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeValue = ZrCore_String_GetNativeString(value);
    return nativeValue != ZR_NULL && strcmp(nativeValue, literal) == 0;
}

static void ct_frame_init(SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrCompileTimeFrame *parent) {
    frame->parent = parent;
    ZrCore_Array_Init(cs->state, &frame->bindings, sizeof(SZrCompileTimeBinding), 4);
}

static void ct_frame_free(SZrCompilerState *cs, SZrCompileTimeFrame *frame) {
    if (frame->bindings.isValid && frame->bindings.head != ZR_NULL) {
        ZrCore_Array_Free(cs->state, &frame->bindings);
    }
}

static TZrBool ct_frame_get(SZrCompileTimeFrame *frame, SZrString *name, SZrTypeValue *result) {
    if (frame == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize i = frame->bindings.length; i > 0; i--) {
        SZrCompileTimeBinding *binding = (SZrCompileTimeBinding *)ZrCore_Array_Get(&frame->bindings, i - 1);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            *result = binding->value;
            return ZR_TRUE;
        }
    }
    return frame->parent != ZR_NULL ? ct_frame_get(frame->parent, name, result) : ZR_FALSE;
}

static TZrBool ct_frame_set(SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrString *name, const SZrTypeValue *value) {
    if (cs == ZR_NULL || frame == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize i = frame->bindings.length; i > 0; i--) {
        SZrCompileTimeBinding *binding = (SZrCompileTimeBinding *)ZrCore_Array_Get(&frame->bindings, i - 1);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            binding->value = *value;
            return ZR_TRUE;
        }
    }
    SZrCompileTimeBinding binding;
    binding.name = name;
    binding.value = *value;
    ZrCore_Array_Push(cs->state, &frame->bindings, &binding);
    return ZR_TRUE;
}

static SZrCompileTimeVariable *find_compile_time_variable(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize i = 0; i < cs->compileTimeVariables.length; i++) {
        SZrCompileTimeVariable **varPtr = (SZrCompileTimeVariable **)ZrCore_Array_Get(&cs->compileTimeVariables, i);
        if (varPtr != ZR_NULL && *varPtr != ZR_NULL && (*varPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*varPtr)->name, name)) {
            return *varPtr;
        }
    }
    return ZR_NULL;
}

static SZrCompileTimeFunction *find_compile_time_function(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize i = 0; i < cs->compileTimeFunctions.length; i++) {
        SZrCompileTimeFunction **funcPtr = (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, i);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (*funcPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*funcPtr)->name, name)) {
            return *funcPtr;
        }
    }
    return ZR_NULL;
}

static TZrBool ct_value_from_compile_time_function(SZrCompilerState *cs,
                                                 SZrCompileTimeFunction *func,
                                                 SZrTypeValue *result) {
    if (cs == ZR_NULL || func == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsNativePointer(cs->state, result, func);
    return ZR_TRUE;
}

static TZrBool ct_value_try_get_compile_time_function(SZrCompilerState *cs,
                                                    const SZrTypeValue *value,
                                                    SZrCompileTimeFunction **result) {
    TZrPtr pointerValue;

    if (cs == ZR_NULL || value == ZR_NULL || result == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return ZR_FALSE;
    }

    pointerValue = value->value.nativeObject.nativePointer;
    if (pointerValue == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < cs->compileTimeFunctions.length; i++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, i);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (TZrPtr)(*funcPtr) == pointerValue) {
            *result = *funcPtr;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

ZR_PARSER_API TZrBool ZrParser_Compiler_TryGetCompileTimeValue(SZrCompilerState *cs,
                                                     SZrString *name,
                                                     SZrTypeValue *result) {
    if (cs == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrCompileTimeVariable *var = find_compile_time_variable(cs, name);
    if (var == ZR_NULL) {
        return ZR_FALSE;
    }
    if (var->hasEvaluatedValue) {
        *result = var->evaluatedValue;
        return ZR_TRUE;
    }
    if (var->isEvaluating) {
        ct_error_name(cs, name, "Circular compile-time variable dependency: ", var->location);
        return ZR_FALSE;
    }

    var->isEvaluating = ZR_TRUE;
    if (var->value == ZR_NULL) {
        ZrCore_Value_ResetAsNull(&var->evaluatedValue);
        var->hasEvaluatedValue = ZR_TRUE;
        var->isEvaluating = ZR_FALSE;
        *result = var->evaluatedValue;
        return ZR_TRUE;
    }

    if (!evaluate_compile_time_expression_internal(cs, var->value, ZR_NULL, &var->evaluatedValue)) {
        var->isEvaluating = ZR_FALSE;
        return ZR_FALSE;
    }

    var->hasEvaluatedValue = ZR_TRUE;
    var->isEvaluating = ZR_FALSE;
    *result = var->evaluatedValue;
    return ZR_TRUE;
}

static TZrBool ct_eval_binary(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrBinaryExpression *expr = &node->data.binaryExpression;
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    const TZrChar *op = expr->op.op;

    if (!evaluate_compile_time_expression_internal(cs, expr->left, frame, &leftValue) ||
        !evaluate_compile_time_expression_internal(cs, expr->right, frame, &rightValue) ||
        op == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
        if (!ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) || !ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time arithmetic requires numeric operands", node->location);
            return ZR_FALSE;
        }
        if (strcmp(op, "/") == 0) {
            TZrBool isZero = ZR_VALUE_IS_TYPE_INT(rightValue.type)
                               ? (rightValue.value.nativeObject.nativeInt64 == 0)
                               : (rightValue.value.nativeObject.nativeDouble == 0.0);
            if (isZero) {
                ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Division by zero in compile-time expression", node->location);
                return ZR_FALSE;
            }
        }
        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
            TZrInt64 left = leftValue.value.nativeObject.nativeInt64;
            TZrInt64 right = rightValue.value.nativeObject.nativeInt64;
            if (strcmp(op, "+") == 0) {
                ZrCore_Value_InitAsInt(cs->state, result, left + right);
            } else if (strcmp(op, "-") == 0) {
                ZrCore_Value_InitAsInt(cs->state, result, left - right);
            } else if (strcmp(op, "*") == 0) {
                ZrCore_Value_InitAsInt(cs->state, result, left * right);
            } else {
                ZrCore_Value_InitAsInt(cs->state, result, left / right);
            }
            return ZR_TRUE;
        }

        TZrDouble left = leftValue.value.nativeObject.nativeDouble;
        TZrDouble right = rightValue.value.nativeObject.nativeDouble;
        if (strcmp(op, "+") == 0) {
            ZrCore_Value_InitAsFloat(cs->state, result, left + right);
        } else if (strcmp(op, "-") == 0) {
            ZrCore_Value_InitAsFloat(cs->state, result, left - right);
        } else if (strcmp(op, "*") == 0) {
            ZrCore_Value_InitAsFloat(cs->state, result, left * right);
        } else {
            ZrCore_Value_InitAsFloat(cs->state, result, left / right);
        }
        return ZR_TRUE;
    }

    if (strcmp(op, "%") == 0) {
        if (!ZR_VALUE_IS_TYPE_INT(leftValue.type) || !ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time modulo requires integer operands", node->location);
            return ZR_FALSE;
        }
        if (rightValue.value.nativeObject.nativeInt64 == 0) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Modulo by zero in compile-time expression", node->location);
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsInt(cs->state, result, leftValue.value.nativeObject.nativeInt64 % rightValue.value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
        strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 ||
        strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) {
        TZrBool value = ZR_FALSE;
        if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
            if (leftValue.type == rightValue.type) {
                if (ZR_VALUE_IS_TYPE_INT(leftValue.type)) {
                    value = leftValue.value.nativeObject.nativeInt64 == rightValue.value.nativeObject.nativeInt64;
                } else if (ZR_VALUE_IS_TYPE_FLOAT(leftValue.type)) {
                    value = leftValue.value.nativeObject.nativeDouble == rightValue.value.nativeObject.nativeDouble;
                } else if (leftValue.type == ZR_VALUE_TYPE_BOOL) {
                    value = leftValue.value.nativeObject.nativeBool == rightValue.value.nativeObject.nativeBool;
                } else if (leftValue.type == ZR_VALUE_TYPE_NULL) {
                    value = ZR_TRUE;
                }
            }
            if (strcmp(op, "!=") == 0) {
                value = !value;
            }
        } else if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
            if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                TZrInt64 left = leftValue.value.nativeObject.nativeInt64;
                TZrInt64 right = rightValue.value.nativeObject.nativeInt64;
                value = strcmp(op, "<") == 0 ? left < right :
                        strcmp(op, "<=") == 0 ? left <= right :
                        strcmp(op, ">") == 0 ? left > right : left >= right;
            } else {
                TZrDouble left = leftValue.value.nativeObject.nativeDouble;
                TZrDouble right = rightValue.value.nativeObject.nativeDouble;
                value = strcmp(op, "<") == 0 ? left < right :
                        strcmp(op, "<=") == 0 ? left <= right :
                        strcmp(op, ">") == 0 ? left > right : left >= right;
            }
        } else {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time comparison requires compatible operands", node->location);
            return ZR_FALSE;
        }

        ZrCore_Value_InitAsUInt(cs->state, result, value ? 1 : 0);
        result->type = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }

    ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time binary expression", node->location);
    return ZR_FALSE;
}

static TZrBool ct_eval_logical(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrLogicalExpression *expr = &node->data.logicalExpression;
    SZrTypeValue leftValue;
    TZrBool value;

    if (!evaluate_compile_time_expression_internal(cs, expr->left, frame, &leftValue)) {
        return ZR_FALSE;
    }

    if (strcmp(expr->op, "&&") == 0) {
        value = ct_truthy(&leftValue);
        if (value) {
            SZrTypeValue rightValue;
            if (!evaluate_compile_time_expression_internal(cs, expr->right, frame, &rightValue)) {
                return ZR_FALSE;
            }
            value = ct_truthy(&rightValue);
        }
    } else if (strcmp(expr->op, "||") == 0) {
        value = ct_truthy(&leftValue);
        if (!value) {
            SZrTypeValue rightValue;
            if (!evaluate_compile_time_expression_internal(cs, expr->right, frame, &rightValue)) {
                return ZR_FALSE;
            }
            value = ct_truthy(&rightValue);
        }
    } else {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time logical expression", node->location);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsUInt(cs->state, result, value ? 1 : 0);
    result->type = ZR_VALUE_TYPE_BOOL;
    return ZR_TRUE;
}

static TZrBool ct_eval_unary(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrUnaryExpression *expr = &node->data.unaryExpression;
    SZrTypeValue argValue;

    if (!evaluate_compile_time_expression_internal(cs, expr->argument, frame, &argValue)) {
        return ZR_FALSE;
    }

    if (strcmp(expr->op.op, "!") == 0) {
        ZrCore_Value_InitAsUInt(cs->state, result, ct_truthy(&argValue) ? 0 : 1);
        result->type = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }
    if (strcmp(expr->op.op, "+") == 0) {
        *result = argValue;
        return ZR_TRUE;
    }
    if (strcmp(expr->op.op, "-") == 0) {
        if (ZR_VALUE_IS_TYPE_INT(argValue.type)) {
            ZrCore_Value_InitAsInt(cs->state, result, -argValue.value.nativeObject.nativeInt64);
            return ZR_TRUE;
        }
        if (ZR_VALUE_IS_TYPE_FLOAT(argValue.type)) {
            ZrCore_Value_InitAsFloat(cs->state, result, -argValue.value.nativeObject.nativeDouble);
            return ZR_TRUE;
        }
    }

    ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time unary expression", node->location);
    return ZR_FALSE;
}

static TZrBool ct_eval_builtin_call(SZrCompilerState *cs,
                                  SZrAstNode *node,
                                  SZrString *funcName,
                                  SZrFunctionCall *call,
                                  SZrCompileTimeFrame *frame,
                                  SZrTypeValue *result) {
    const TZrChar *nameStr = ct_name(funcName);

    if (strcmp(nameStr, "FatalError") == 0) {
        const TZrChar *msg = "FatalError";
        if (call != ZR_NULL && call->args != ZR_NULL && call->args->count > 0) {
            SZrTypeValue msgValue;
            if (evaluate_compile_time_expression_internal(cs, call->args->nodes[0], frame, &msgValue) &&
                msgValue.type == ZR_VALUE_TYPE_STRING) {
                msg = ct_name((SZrString *)ZrCore_Value_GetRawObject(&msgValue));
            }
        }
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_FATAL, msg, node->location);
        return ZR_FALSE;
    }

    if (strcmp(nameStr, "Assert") == 0) {
        if (call == ZR_NULL || call->args == ZR_NULL || call->args->count == 0) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Assert requires at least one argument", node->location);
            return ZR_FALSE;
        }

        SZrTypeValue condValue;
        if (!evaluate_compile_time_expression_internal(cs, call->args->nodes[0], frame, &condValue)) {
            return ZR_FALSE;
        }
        if (!ct_truthy(&condValue)) {
            const TZrChar *msg = "Assertion failed";
            if (call->args->count > 1) {
                SZrTypeValue msgValue;
                if (evaluate_compile_time_expression_internal(cs, call->args->nodes[1], frame, &msgValue) &&
                    msgValue.type == ZR_VALUE_TYPE_STRING) {
                    msg = ct_name((SZrString *)ZrCore_Value_GetRawObject(&msgValue));
                }
            }
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_FATAL, msg, node->location);
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsUInt(cs->state, result, 1);
        result->type = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }

    if (strcmp(nameStr, "import") == 0) {
        SZrTypeValue importCallable;
        SZrClosureNative *importClosure = ZrCore_ClosureNative_New(cs->state, 0);
        if (importClosure == ZR_NULL) {
            return ZR_FALSE;
        }
        importClosure->nativeFunction = ZrCore_ImportNativeFunction;
        ZrCore_Value_InitAsRawObject(cs->state, &importCallable, ZR_CAST_RAW_OBJECT_AS_SUPER(importClosure));
        importCallable.isNative = ZR_TRUE;
        return ct_call_value(cs, node, &importCallable, call, frame, result);
    }

    return ZR_FALSE;
}

static TZrBool ct_eval_call_arg(SZrCompilerState *cs,
                              SZrFunctionCall *call,
                              SZrParameter *param,
                              TZrSize paramIndex,
                              SZrCompileTimeFrame *frame,
                              SZrTypeValue *result) {
    TZrSize positionalCount = 0;

    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL &&
        param != ZR_NULL && param->name != ZR_NULL && param->name->name != ZR_NULL) {
        for (TZrSize i = 0; i < call->argNames->length && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        for (TZrSize i = 0; i < call->argNames->length && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr != ZR_NULL &&
                ZrCore_String_Equal(*argNamePtr, param->name->name)) {
                return evaluate_compile_time_expression_internal(cs, call->args->nodes[i], frame, result);
            }
        }

        if (paramIndex < positionalCount) {
            return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
        }
    } else if (call != ZR_NULL && call->args != ZR_NULL && paramIndex < call->args->count) {
        return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
    }

    if (param != ZR_NULL && param->defaultValue != ZR_NULL) {
        return evaluate_compile_time_expression_internal(cs, param->defaultValue, frame, result);
    }

    ct_error_name(cs,
                  param != ZR_NULL && param->name != ZR_NULL ? param->name->name : ZR_NULL,
                  "Missing compile-time argument for parameter: ",
                  (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
    return ZR_FALSE;
}

static TZrBool ct_call_function(SZrCompilerState *cs,
                              SZrAstNode *callSite,
                              SZrCompileTimeFunction *func,
                              SZrFunctionCall *call,
                              SZrCompileTimeFrame *parentFrame,
                              SZrTypeValue *result) {
    SZrFunctionDeclaration *decl;
    SZrCompileTimeFrame frame;
    TZrBool success = ZR_FALSE;
    TZrBool didReturn = ZR_FALSE;

    if (cs == ZR_NULL || func == ZR_NULL || func->declaration == ZR_NULL ||
        func->declaration->type != ZR_AST_FUNCTION_DECLARATION || result == ZR_NULL) {
        return ZR_FALSE;
    }

    decl = &func->declaration->data.functionDeclaration;
    ct_frame_init(cs, &frame, parentFrame);

    if (decl->params != ZR_NULL) {
        for (TZrSize i = 0; i < decl->params->count; i++) {
            SZrAstNode *paramNode = decl->params->nodes[i];
            SZrParameter *param;
            SZrTypeValue argValue;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            param = &paramNode->data.parameter;
            if (!ct_eval_call_arg(cs, call, param, i, &frame, &argValue) ||
                param->name == ZR_NULL || param->name->name == ZR_NULL ||
                !ct_frame_set(cs, &frame, param->name->name, &argValue)) {
                goto cleanup;
            }
        }
    }

    if (call != ZR_NULL && call->args != ZR_NULL && !call->hasNamedArgs) {
        TZrSize expectedArgs = decl->params != ZR_NULL ? decl->params->count : 0;
        if (call->args->count > expectedArgs) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Too many arguments for compile-time function call", callSite->location);
            goto cleanup;
        }
    }

    if (decl->body == ZR_NULL) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time function body is null", callSite->location);
        goto cleanup;
    }

    success = decl->body->type == ZR_AST_BLOCK
                  ? execute_compile_time_block(cs, decl->body, &frame, &didReturn, result)
                  : execute_compile_time_statement(cs, decl->body, &frame, &didReturn, result);
    if (success && !didReturn) {
        ZrCore_Value_ResetAsNull(result);
    }

cleanup:
    ct_frame_free(cs, &frame);
    return success;
}

static TZrBool ct_invoke_runtime_callable(SZrCompilerState *cs,
                                        SZrAstNode *callSite,
                                        const SZrTypeValue *callableValue,
                                        SZrFunctionCall *call,
                                        SZrCompileTimeFrame *frame,
                                        SZrTypeValue *result) {
    SZrState *state;
    TZrStackValuePointer base;
    TZrSize argCount;
    SZrFunctionStackAnchor baseAnchor;

    if (cs == ZR_NULL || callableValue == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (call != ZR_NULL && call->hasNamedArgs) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                           "Named arguments are not supported for runtime callable projection in compile-time evaluation",
                           callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    state = cs->state;
    argCount = (call != ZR_NULL && call->args != ZR_NULL) ? call->args->count : 0;
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, argCount + 1, base, base, &baseAnchor);
    state->stackTop.valuePointer = base;
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), callableValue);
    state->stackTop.valuePointer = base + 1;

    if (call != ZR_NULL && call->args != ZR_NULL) {
        for (TZrSize i = 0; i < call->args->count; i++) {
            SZrTypeValue argValue;
            if (!evaluate_compile_time_expression_internal(cs, call->args->nodes[i], frame, &argValue)) {
                base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
                state->stackTop.valuePointer = base;
                return ZR_FALSE;
            }
            base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
            ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base + 1 + i), &argValue);
            state->stackTop.valuePointer = base + 2 + i;
        }
    }

    base = ZrCore_Function_CallAndRestoreAnchor(state, &baseAnchor, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                           "Runtime callable failed during compile-time evaluation",
                           callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(base));
    return ZR_TRUE;
}

static TZrBool ct_eval_object_key(SZrCompilerState *cs,
                                SZrAstNode *keyNode,
                                SZrCompileTimeFrame *frame,
                                SZrTypeValue *result) {
    if (cs == ZR_NULL || keyNode == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (keyNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(keyNode->data.identifier.name));
        result->type = ZR_VALUE_TYPE_STRING;
        return ZR_TRUE;
    }

    return evaluate_compile_time_expression_internal(cs, keyNode, frame, result);
}

static TZrBool ct_eval_object_literal(SZrCompilerState *cs,
                                    SZrAstNode *node,
                                    SZrCompileTimeFrame *frame,
                                    SZrTypeValue *result) {
    SZrObjectLiteral *objectLiteral;
    SZrObject *object;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_OBJECT_LITERAL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    objectLiteral = &node->data.objectLiteral;
    object = ZrCore_Object_New(cs->state, ZR_NULL);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(cs->state, object);

    if (objectLiteral->properties != ZR_NULL) {
        for (TZrSize i = 0; i < objectLiteral->properties->count; i++) {
            SZrAstNode *propertyNode = objectLiteral->properties->nodes[i];
            SZrTypeValue keyValue;
            SZrTypeValue propertyValue;

            if (propertyNode == ZR_NULL) {
                continue;
            }

            if (propertyNode->type != ZR_AST_KEY_VALUE_PAIR) {
                ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Unsupported compile-time object literal property",
                                   propertyNode->location);
                return ZR_FALSE;
            }

            if (!ct_eval_object_key(cs, propertyNode->data.keyValuePair.key, frame, &keyValue) ||
                !evaluate_compile_time_expression_internal(cs, propertyNode->data.keyValuePair.value, frame, &propertyValue)) {
                return ZR_FALSE;
            }

            ZrCore_Object_SetValue(cs->state, object, &keyValue, &propertyValue);
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_eval_member_access(SZrCompilerState *cs,
                                   SZrAstNode *callSite,
                                   const SZrTypeValue *baseValue,
                                   SZrMemberExpression *memberExpr,
                                   SZrCompileTimeFrame *frame,
                                   SZrTypeValue *result) {
    SZrTypeValue keyValue;
    const SZrTypeValue *memberValue;

    if (cs == ZR_NULL || baseValue == ZR_NULL || memberExpr == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (baseValue->type != ZR_VALUE_TYPE_OBJECT && baseValue->type != ZR_VALUE_TYPE_ARRAY) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                           "Compile-time member access requires object or array value",
                           callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (!memberExpr->computed && memberExpr->property != ZR_NULL &&
        memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
        ZrCore_Value_InitAsRawObject(cs->state, &keyValue,
                               ZR_CAST_RAW_OBJECT_AS_SUPER(memberExpr->property->data.identifier.name));
        keyValue.type = ZR_VALUE_TYPE_STRING;
    } else if (!evaluate_compile_time_expression_internal(cs, memberExpr->property, frame, &keyValue)) {
        return ZR_FALSE;
    }

    memberValue = ZrCore_Object_GetValue(cs->state, ZR_CAST_OBJECT(cs->state, baseValue->value.object), &keyValue);
    if (memberValue == ZR_NULL) {
        TZrChar message[256];
        snprintf(message, sizeof(message), "Unknown compile-time member: %s",
                 (!memberExpr->computed && memberExpr->property != ZR_NULL &&
                  memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL)
                         ? ct_name(memberExpr->property->data.identifier.name)
                         : "<computed>");
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, message,
                           callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    *result = *memberValue;
    return ZR_TRUE;
}

static TZrBool ct_call_value(SZrCompilerState *cs,
                           SZrAstNode *callSite,
                           const SZrTypeValue *callableValue,
                           SZrFunctionCall *call,
                           SZrCompileTimeFrame *frame,
                           SZrTypeValue *result) {
    SZrCompileTimeFunction *compileTimeFunction = ZR_NULL;

    if (ct_value_try_get_compile_time_function(cs, callableValue, &compileTimeFunction)) {
        return ct_call_function(cs, callSite, compileTimeFunction, call, frame, result);
    }

    return ct_invoke_runtime_callable(cs, callSite, callableValue, call, frame, result);
}

static TZrBool ct_eval_primary(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    SZrTypeValue currentValue;
    TZrSize startIndex = 0;

    if (primary->members == ZR_NULL || primary->members->count == 0) {
        return primary->property != ZR_NULL
                   ? evaluate_compile_time_expression_internal(cs, primary->property, frame, result)
                   : ZR_FALSE;
    }

    if (primary->property != ZR_NULL &&
        primary->property->type == ZR_AST_IDENTIFIER_LITERAL &&
        primary->members->nodes[0] != ZR_NULL &&
        primary->members->nodes[0]->type == ZR_AST_FUNCTION_CALL) {
        SZrString *funcName = primary->property->data.identifier.name;
        SZrFunctionCall *call = &primary->members->nodes[0]->data.functionCall;

        if (ct_string_equals(funcName, "FatalError") ||
            ct_string_equals(funcName, "Assert") ||
            ct_string_equals(funcName, "import")) {
            if (!ct_eval_builtin_call(cs, node, funcName, call, frame, &currentValue)) {
                return ZR_FALSE;
            }
            startIndex = 1;
        } else if (!evaluate_compile_time_expression_internal(cs, primary->property, frame, &currentValue)) {
            return ZR_FALSE;
        }
    } else if (!evaluate_compile_time_expression_internal(cs, primary->property, frame, &currentValue)) {
        return ZR_FALSE;
    }

    for (TZrSize i = startIndex; i < primary->members->count; i++) {
        SZrAstNode *memberNode = primary->members->nodes[i];

        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_MEMBER_EXPRESSION) {
            if (!ct_eval_member_access(cs, memberNode, &currentValue, &memberNode->data.memberExpression, frame,
                                       &currentValue)) {
                return ZR_FALSE;
            }
            continue;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            if (!ct_call_value(cs, memberNode, &currentValue, &memberNode->data.functionCall, frame, &currentValue)) {
                return ZR_FALSE;
            }
            continue;
        }

        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                           "Unsupported compile-time primary expression member",
                           memberNode->location);
        return ZR_FALSE;
    }

    *result = currentValue;
    return ZR_TRUE;
}

static TZrBool evaluate_compile_time_expression_internal(SZrCompilerState *cs,
                                                       SZrAstNode *node,
                                                       SZrCompileTimeFrame *frame,
                                                       SZrTypeValue *result) {
    TZrBool oldContext;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    oldContext = cs->isInCompileTimeContext;
    cs->isInCompileTimeContext = ZR_TRUE;

    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL:
            ZrCore_Value_InitAsInt(cs->state, result, node->data.integerLiteral.value);
            break;
        case ZR_AST_FLOAT_LITERAL:
            ZrCore_Value_InitAsFloat(cs->state, result, node->data.floatLiteral.value);
            break;
        case ZR_AST_BOOLEAN_LITERAL:
            ZrCore_Value_InitAsUInt(cs->state, result, node->data.booleanLiteral.value ? 1 : 0);
            result->type = ZR_VALUE_TYPE_BOOL;
            break;
        case ZR_AST_STRING_LITERAL:
            ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(node->data.stringLiteral.value));
            result->type = ZR_VALUE_TYPE_STRING;
            break;
        case ZR_AST_NULL_LITERAL:
            ZrCore_Value_ResetAsNull(result);
            break;
        case ZR_AST_IDENTIFIER_LITERAL:
            if (!ct_frame_get(frame, node->data.identifier.name, result) &&
                !ZrParser_Compiler_TryGetCompileTimeValue(cs, node->data.identifier.name, result)) {
                SZrCompileTimeFunction *func = find_compile_time_function(cs, node->data.identifier.name);
                if (func != ZR_NULL && ct_value_from_compile_time_function(cs, func, result)) {
                    break;
                }
                ct_error_name(cs, node->data.identifier.name, "Unknown compile-time identifier: ", node->location);
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            break;
        case ZR_AST_OBJECT_LITERAL:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_object_literal(cs, node, frame, result);
        case ZR_AST_BINARY_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_binary(cs, node, frame, result);
        case ZR_AST_LOGICAL_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_logical(cs, node, frame, result);
        case ZR_AST_UNARY_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_unary(cs, node, frame, result);
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            SZrConditionalExpression *expr = &node->data.conditionalExpression;
            SZrTypeValue condValue;
            if (!evaluate_compile_time_expression_internal(cs, expr->test, frame, &condValue)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return evaluate_compile_time_expression_internal(cs, ct_truthy(&condValue) ? expr->consequent : expr->alternate, frame, result);
        }
        case ZR_AST_PRIMARY_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_primary(cs, node, frame, result);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "Prototype references are not supported in compile-time expressions",
                               node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "Prototype construction is not supported in compile-time expressions",
                               node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
        case ZR_AST_EXPRESSION_STATEMENT:
            cs->isInCompileTimeContext = oldContext;
            return evaluate_compile_time_expression_internal(cs, node->data.expressionStatement.expr, frame, result);
        default:
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time expression node", node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
    }

    cs->isInCompileTimeContext = oldContext;
    return ZR_TRUE;
}

ZR_PARSER_API TZrBool ZrParser_Compiler_EvaluateCompileTimeExpression(SZrCompilerState *cs,
                                                            SZrAstNode *node,
                                                            SZrTypeValue *result) {
    return evaluate_compile_time_expression_internal(cs, node, ZR_NULL, result);
}

static TZrBool register_compile_time_variable_declaration(SZrCompilerState *cs,
                                                        SZrAstNode *node,
                                                        SZrFileRange location) {
    SZrVariableDeclaration *varDecl;
    SZrString *varName;
    SZrCompileTimeVariable *var;
    SZrTypeValue value;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION ||
        node->data.variableDeclaration.pattern == ZR_NULL ||
        node->data.variableDeclaration.pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    varDecl = &node->data.variableDeclaration;
    varName = varDecl->pattern->data.identifier.name;
    var = find_compile_time_variable(cs, varName);
    if (var == ZR_NULL) {
        var = ZrCore_Memory_RawMallocWithType(cs->state->global,
                                        sizeof(SZrCompileTimeVariable),
                                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (var == ZR_NULL) {
            return ZR_FALSE;
        }
        var->name = varName;
        ZrParser_InferredType_Init(cs->state, &var->type, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Push(cs->state, &cs->compileTimeVariables, &var);
    } else {
        ZrParser_InferredType_Free(cs->state, &var->type);
        ZrParser_InferredType_Init(cs->state, &var->type, ZR_VALUE_TYPE_OBJECT);
    }

    var->value = varDecl->value;
    var->location = location;
    var->hasEvaluatedValue = ZR_FALSE;
    var->isEvaluating = ZR_FALSE;

    if (varDecl->typeInfo != ZR_NULL &&
        !ZrParser_AstTypeToInferredType_Convert(cs, varDecl->typeInfo, &var->type)) {
        ZrParser_InferredType_Free(cs->state, &var->type);
        ZrParser_InferredType_Init(cs->state, &var->type, ZR_VALUE_TYPE_OBJECT);
    }

    if (varDecl->value != ZR_NULL) {
        if (!ZrParser_Compiler_TryGetCompileTimeValue(cs, varName, &value)) {
            return ZR_FALSE;
        }
        if (varDecl->typeInfo == ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, &var->type);
            ct_init_type_from_value(cs, &value, &var->type);
        }
    } else {
        ZrCore_Value_ResetAsNull(&var->evaluatedValue);
        var->hasEvaluatedValue = ZR_TRUE;
        if (varDecl->typeInfo == ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, &var->type);
            ZrParser_InferredType_Init(cs->state, &var->type, ZR_VALUE_TYPE_NULL);
        }
    }

    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->compileTimeTypeEnv, varName, &var->type);
    }
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, varName, &var->type);
    }

    return ZR_TRUE;
}

static TZrBool register_compile_time_function_declaration(SZrCompilerState *cs,
                                                        SZrAstNode *node,
                                                        SZrFileRange location) {
    SZrFunctionDeclaration *funcDecl;
    SZrCompileTimeFunction *func;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_FUNCTION_DECLARATION ||
        node->data.functionDeclaration.name == ZR_NULL ||
        node->data.functionDeclaration.name->name == ZR_NULL) {
        return ZR_FALSE;
    }

    funcDecl = &node->data.functionDeclaration;
    func = find_compile_time_function(cs, funcDecl->name->name);
    if (func == ZR_NULL) {
        func = ZrCore_Memory_RawMallocWithType(cs->state->global,
                                         sizeof(SZrCompileTimeFunction),
                                         ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (func == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Array_Init(cs->state, &func->paramTypes, sizeof(SZrInferredType),
                    funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
        ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Push(cs->state, &cs->compileTimeFunctions, &func);
    } else {
        for (TZrSize i = 0; i < func->paramTypes.length; i++) {
            SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&func->paramTypes, i);
            if (paramType != ZR_NULL) {
                ZrParser_InferredType_Free(cs->state, paramType);
            }
        }
        func->paramTypes.length = 0;
        ZrParser_InferredType_Free(cs->state, &func->returnType);
        ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);
    }

    func->name = funcDecl->name->name;
    func->declaration = node;
    func->location = location;

    if (funcDecl->returnType != ZR_NULL &&
        !ZrParser_AstTypeToInferredType_Convert(cs, funcDecl->returnType, &func->returnType)) {
        ZrParser_InferredType_Free(cs->state, &func->returnType);
        ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);
    }

    if (funcDecl->params != ZR_NULL) {
        for (TZrSize i = 0; i < funcDecl->params->count; i++) {
            SZrAstNode *paramNode = funcDecl->params->nodes[i];
            SZrInferredType paramType;
            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }
            if (paramNode->data.parameter.typeInfo != ZR_NULL &&
                !ZrParser_AstTypeToInferredType_Convert(cs, paramNode->data.parameter.typeInfo, &paramType)) {
                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            } else if (paramNode->data.parameter.typeInfo == ZR_NULL) {
                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            }
            ZrCore_Array_Push(cs->state, &func->paramTypes, &paramType);
        }
    }

    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterFunction(cs->state, cs->compileTimeTypeEnv, func->name, &func->returnType, &func->paramTypes);
    }

    return ZR_TRUE;
}

static TZrBool execute_compile_time_statement(SZrCompilerState *cs,
                                            SZrAstNode *node,
                                            SZrCompileTimeFrame *frame,
                                            TZrBool *didReturn,
                                            SZrTypeValue *result) {
    if (didReturn != ZR_NULL) {
        *didReturn = ZR_FALSE;
    }
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_BLOCK:
            return execute_compile_time_block(cs, node, frame, didReturn, result);
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *stmt = &node->data.returnStatement;
            if (result == ZR_NULL) {
                return ZR_FALSE;
            }
            if (stmt->expr == ZR_NULL) {
                ZrCore_Value_ResetAsNull(result);
            } else if (!evaluate_compile_time_expression_internal(cs, stmt->expr, frame, result)) {
                return ZR_FALSE;
            }
            if (didReturn != ZR_NULL) {
                *didReturn = ZR_TRUE;
            }
            return ZR_TRUE;
        }
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *decl = &node->data.variableDeclaration;
            SZrTypeValue value;
            if (frame == ZR_NULL) {
                return register_compile_time_variable_declaration(cs, node, node->location);
            }
            if (decl->pattern == ZR_NULL || decl->pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
                return ZR_TRUE;
            }
            if (decl->value != ZR_NULL && !evaluate_compile_time_expression_internal(cs, decl->value, frame, &value)) {
                return ZR_FALSE;
            }
            if (decl->value == ZR_NULL) {
                ZrCore_Value_ResetAsNull(&value);
            }
            return ct_frame_set(cs, frame, decl->pattern->data.identifier.name, &value);
        }
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *expr = &node->data.ifExpression;
            SZrTypeValue condValue;
            SZrAstNode *branch;
            if (!evaluate_compile_time_expression_internal(cs, expr->condition, frame, &condValue)) {
                return ZR_FALSE;
            }
            branch = ct_truthy(&condValue) ? expr->thenExpr : expr->elseExpr;
            if (branch == ZR_NULL) {
                return ZR_TRUE;
            }
            if (branch->type == ZR_AST_BLOCK || branch->type == ZR_AST_RETURN_STATEMENT ||
                branch->type == ZR_AST_IF_EXPRESSION || branch->type == ZR_AST_VARIABLE_DECLARATION ||
                branch->type == ZR_AST_FUNCTION_DECLARATION ||
                branch->type == ZR_AST_EXPRESSION_STATEMENT) {
                return execute_compile_time_statement(cs, branch, frame, didReturn, result);
            }
            return result != ZR_NULL ? evaluate_compile_time_expression_internal(cs, branch, frame, result) : ZR_TRUE;
        }
        case ZR_AST_FUNCTION_DECLARATION:
            if (frame == ZR_NULL) {
                return register_compile_time_function_declaration(cs, node, node->location);
            }
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                               "Nested compile-time function declarations are not supported in local frames",
                               node->location);
            return ZR_FALSE;
        case ZR_AST_COMPILE_TIME_DECLARATION:
            return ZrParser_CompileTimeDeclaration_Execute(cs, node);
        default: {
            SZrTypeValue ignored;
            return evaluate_compile_time_expression_internal(cs, node, frame, result != ZR_NULL ? result : &ignored);
        }
    }
}

static TZrBool execute_compile_time_block(SZrCompilerState *cs,
                                        SZrAstNode *node,
                                        SZrCompileTimeFrame *frame,
                                        TZrBool *didReturn,
                                        SZrTypeValue *result) {
    SZrBlock *block;
    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_BLOCK) {
        return ZR_FALSE;
    }
    block = &node->data.block;
    if (block->body == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize i = 0; i < block->body->count; i++) {
        TZrBool returned = ZR_FALSE;
        if (!execute_compile_time_statement(cs, block->body->nodes[i], frame, &returned, result)) {
            return ZR_FALSE;
        }
        if (returned) {
            if (didReturn != ZR_NULL) {
                *didReturn = ZR_TRUE;
            }
            return ZR_TRUE;
        }
    }
    return ZR_TRUE;
}

ZR_PARSER_API TZrBool ZrParser_CompileTimeDeclaration_Execute(SZrCompilerState *cs, SZrAstNode *node) {
    SZrCompileTimeDeclaration *decl;
    SZrAstNode *body;
    TZrBool oldContext;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_COMPILE_TIME_DECLARATION) {
        return ZR_FALSE;
    }

    decl = &node->data.compileTimeDeclaration;
    body = decl->declaration;
    if (body == ZR_NULL) {
        return ZR_FALSE;
    }

    oldContext = cs->isInCompileTimeContext;
    cs->isInCompileTimeContext = ZR_TRUE;

    switch (decl->declarationType) {
        case ZR_COMPILE_TIME_VARIABLE: {
            if (!register_compile_time_variable_declaration(cs, body, node->location)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return ZR_TRUE;
        }

        case ZR_COMPILE_TIME_FUNCTION: {
            if (!register_compile_time_function_declaration(cs, body, node->location)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return ZR_TRUE;
        }

        case ZR_COMPILE_TIME_STATEMENT: {
            TZrBool didReturn = ZR_FALSE;
            SZrTypeValue ignored;
            TZrBool ok = body->type == ZR_AST_BLOCK
                           ? execute_compile_time_block(cs, body, ZR_NULL, &didReturn, &ignored)
                           : execute_compile_time_statement(cs, body, ZR_NULL, &didReturn, &ignored);
            cs->isInCompileTimeContext = oldContext;
            return ok;
        }

        case ZR_COMPILE_TIME_EXPRESSION: {
            SZrTypeValue ignored;
            TZrBool ok = evaluate_compile_time_expression_internal(cs, body, ZR_NULL, &ignored);
            cs->isInCompileTimeContext = oldContext;
            return ok;
        }
    }

    cs->isInCompileTimeContext = oldContext;
    return ZR_FALSE;
}
