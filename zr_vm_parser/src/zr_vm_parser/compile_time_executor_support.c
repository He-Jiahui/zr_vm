//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "compile_time_executor_internal.h"
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

const TZrChar *ct_name(SZrString *name) {
    if (name == ZR_NULL) {
        return "<null>";
    }
    TZrNativeString nameStr = ZrCore_String_GetNativeString(name);
    return nameStr != ZR_NULL ? nameStr : "<null>";
}

void ct_error_name(SZrCompilerState *cs, SZrString *name, const TZrChar *prefix, SZrFileRange location) {
    TZrChar msg[256];
    snprintf(msg, sizeof(msg), "%s%s", prefix, ct_name(name));
    ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, msg, location);
}

TZrBool ct_truthy(const SZrTypeValue *value) {
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

void ct_init_type_from_value(SZrCompilerState *cs, const SZrTypeValue *value, SZrInferredType *result) {
    ZrParser_InferredType_Init(cs->state, result, value != ZR_NULL ? value->type : ZR_VALUE_TYPE_OBJECT);
}

TZrBool ct_string_equals(SZrString *value, const TZrChar *literal) {
    TZrNativeString nativeValue;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeValue = ZrCore_String_GetNativeString(value);
    return nativeValue != ZR_NULL && strcmp(nativeValue, literal) == 0;
}

TZrBool ct_eval_import_expression(SZrCompilerState *cs,
                                         SZrAstNode *node,
                                         SZrTypeValue *result) {
    SZrAstNode *modulePathNode;
    SZrString *moduleName;
    SZrObjectModule *module;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_IMPORT_EXPRESSION) {
        return ZR_FALSE;
    }

    modulePathNode = node->data.importExpression.modulePath;
    if (modulePathNode == ZR_NULL || modulePathNode->type != ZR_AST_STRING_LITERAL ||
        modulePathNode->data.stringLiteral.value == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time import requires a normalized string module path",
                                   node->location);
        return ZR_FALSE;
    }

    moduleName = modulePathNode->data.stringLiteral.value;
    module = ZrCore_Module_ImportByPath(cs->state, moduleName);
    if (module == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time import failed to load module",
                                   node->location);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

void ct_frame_init(SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrCompileTimeFrame *parent) {
    frame->parent = parent;
    ZrCore_Array_Init(cs->state, &frame->bindings, sizeof(SZrCompileTimeBinding), 4);
}

void ct_frame_free(SZrCompilerState *cs, SZrCompileTimeFrame *frame) {
    if (frame->bindings.isValid && frame->bindings.head != ZR_NULL) {
        ZrCore_Array_Free(cs->state, &frame->bindings);
    }
}

TZrBool ct_frame_get(SZrCompileTimeFrame *frame, SZrString *name, SZrTypeValue *result) {
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

TZrBool ct_frame_set(SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrString *name, const SZrTypeValue *value) {
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

SZrCompileTimeVariable *find_compile_time_variable(SZrCompilerState *cs, SZrString *name) {
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

SZrCompileTimeFunction *find_compile_time_function(SZrCompilerState *cs, SZrString *name) {
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

TZrBool ct_value_from_compile_time_function(SZrCompilerState *cs,
                                                 SZrCompileTimeFunction *func,
                                                 SZrTypeValue *result) {
    if (cs == ZR_NULL || func == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsNativePointer(cs->state, result, func);
    return ZR_TRUE;
}

TZrBool ct_value_try_get_compile_time_function(SZrCompilerState *cs,
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


TZrBool register_compile_time_variable_declaration(SZrCompilerState *cs,
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

TZrBool register_compile_time_function_declaration(SZrCompilerState *cs,
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

