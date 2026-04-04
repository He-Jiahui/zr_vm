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
    TZrChar msg[ZR_PARSER_ERROR_BUFFER_LENGTH];
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

static TZrBool ct_identifier_matches_literal(SZrAstNode *node, const TZrChar *literal) {
    return node != ZR_NULL &&
           node->type == ZR_AST_IDENTIFIER_LITERAL &&
           node->data.identifier.name != ZR_NULL &&
           ct_string_equals(node->data.identifier.name, literal);
}

static TZrBool ct_function_decl_is_async_wrapper(const SZrFunctionDeclaration *funcDecl) {
    SZrBlock *block;
    SZrAstNode *statement;
    SZrAstNode *expression;
    SZrPrimaryExpression *primary;
    SZrAstNode *modulePathNode;
    SZrAstNode *memberNode;
    SZrAstNode *callNode;

    if (funcDecl == ZR_NULL || funcDecl->body == ZR_NULL || funcDecl->body->type != ZR_AST_BLOCK) {
        return ZR_FALSE;
    }

    block = &funcDecl->body->data.block;
    if (block->body == ZR_NULL || block->body->count != 1) {
        return ZR_FALSE;
    }

    statement = block->body->nodes[0];
    if (statement == ZR_NULL || statement->type != ZR_AST_RETURN_STATEMENT ||
        statement->data.returnStatement.expr == ZR_NULL) {
        return ZR_FALSE;
    }

    expression = statement->data.returnStatement.expr;
    if (expression->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &expression->data.primaryExpression;
    if (primary->property == ZR_NULL || primary->property->type != ZR_AST_IMPORT_EXPRESSION ||
        primary->members == ZR_NULL || primary->members->count != 2) {
        return ZR_FALSE;
    }

    modulePathNode = primary->property->data.importExpression.modulePath;
    if (modulePathNode == ZR_NULL || modulePathNode->type != ZR_AST_STRING_LITERAL ||
        modulePathNode->data.stringLiteral.value == ZR_NULL ||
        !ct_string_equals(modulePathNode->data.stringLiteral.value, "zr.task")) {
        return ZR_FALSE;
    }

    memberNode = primary->members->nodes[0];
    callNode = primary->members->nodes[1];
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        memberNode->data.memberExpression.computed ||
        !ct_identifier_matches_literal(memberNode->data.memberExpression.property, "spawn")) {
        return ZR_FALSE;
    }

    if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL ||
        callNode->data.functionCall.args == ZR_NULL ||
        callNode->data.functionCall.args->count != 1 ||
        callNode->data.functionCall.args->nodes[0] == ZR_NULL ||
        callNode->data.functionCall.args->nodes[0]->type != ZR_AST_LAMBDA_EXPRESSION) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
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
    ZrCore_Array_Init(cs->state,
                      &frame->bindings,
                      sizeof(SZrCompileTimeBinding),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
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

static void ct_compile_time_function_init_record(SZrCompilerState *cs,
                                                 SZrCompileTimeFunction *func,
                                                 TZrSize parameterCapacity) {
    if (cs == ZR_NULL || func == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(func, 0, sizeof(*func));
    ZrCore_Array_Init(cs->state, &func->paramTypes, sizeof(SZrInferredType), parameterCapacity);
    ZrCore_Array_Init(cs->state, &func->paramNames, sizeof(SZrString *), parameterCapacity);
    ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);
}

static void ct_compile_time_function_reset_signature(SZrCompilerState *cs, SZrCompileTimeFunction *func) {
    if (cs == ZR_NULL || func == ZR_NULL) {
        return;
    }

    for (TZrSize i = 0; i < func->paramTypes.length; i++) {
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&func->paramTypes, i);
        if (paramType != ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, paramType);
        }
    }
    func->paramTypes.length = 0;
    func->paramNames.length = 0;
    ZrParser_InferredType_Free(cs->state, &func->returnType);
    ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);
    func->runtimeProjectionModuleName = ZR_NULL;
    func->runtimeProjectionExportName = ZR_NULL;
    func->isRuntimeProjection = ZR_FALSE;
}

static TZrBool ct_compile_time_function_append_parameter(SZrCompilerState *cs,
                                                         SZrCompileTimeFunction *func,
                                                         SZrParameter *param) {
    SZrInferredType paramType;
    SZrString *paramName = ZR_NULL;

    if (cs == ZR_NULL || func == ZR_NULL || param == ZR_NULL) {
        return ZR_FALSE;
    }

    if (param->typeInfo != ZR_NULL &&
        !ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
        ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
    } else if (param->typeInfo == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
    }

    if (param->name != ZR_NULL) {
        paramName = param->name->name;
    }

    ZrCore_Array_Push(cs->state, &func->paramTypes, &paramType);
    ZrCore_Array_Push(cs->state, &func->paramNames, &paramName);
    return ZR_TRUE;
}

static SZrCompileTimeDecoratorClass *find_compile_time_decorator_class_local(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < cs->compileTimeDecoratorClasses.length; i++) {
        SZrCompileTimeDecoratorClass **classPtr =
                (SZrCompileTimeDecoratorClass **)ZrCore_Array_Get(&cs->compileTimeDecoratorClasses, i);
        if (classPtr != ZR_NULL && *classPtr != ZR_NULL && (*classPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*classPtr)->name, name)) {
            return *classPtr;
        }
    }

    return ZR_NULL;
}

static SZrAstNode *find_compile_time_decorator_meta_method_from_members(SZrAstNodeArray *members,
                                                                        const TZrChar *metaName,
                                                                        TZrBool isStructDecorator) {
    if (members == ZR_NULL || metaName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < members->count; i++) {
        SZrAstNode *member = members->nodes[i];
        SZrIdentifier *meta = ZR_NULL;

        if (member == ZR_NULL) {
            continue;
        }

        if (!isStructDecorator && member->type == ZR_AST_CLASS_META_FUNCTION) {
            meta = member->data.classMetaFunction.meta;
        } else if (isStructDecorator && member->type == ZR_AST_STRUCT_META_FUNCTION) {
            meta = member->data.structMetaFunction.meta;
        }

        if (meta != ZR_NULL && meta->name != ZR_NULL && ct_string_equals(meta->name, metaName)) {
            return member;
        }
    }

    return ZR_NULL;
}

TZrBool ct_value_from_compile_time_function(SZrCompilerState *cs,
                                                 SZrCompileTimeFunction *func,
                                                 SZrTypeValue *result) {
    SZrObjectModule *module;
    const SZrTypeValue *projectedValue;

    if (cs == ZR_NULL || func == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (func->isRuntimeProjection &&
        func->runtimeProjectionModuleName != ZR_NULL &&
        func->runtimeProjectionExportName != ZR_NULL) {
        module = ZrCore_Module_ImportByPath(cs->state, func->runtimeProjectionModuleName);
        if (module == ZR_NULL) {
            return ZR_FALSE;
        }

        projectedValue = ZrCore_Module_GetProExport(cs->state, module, func->runtimeProjectionExportName);
        if (projectedValue == ZR_NULL) {
            projectedValue = ZrCore_Module_GetPubExport(cs->state, module, func->runtimeProjectionExportName);
        }
        if (projectedValue == ZR_NULL) {
            return ZR_FALSE;
        }

        *result = *projectedValue;
        return ZR_TRUE;
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
    if (ct_function_decl_is_async_wrapper(funcDecl)) {
        return ZR_TRUE;
    }
    func = find_compile_time_function(cs, funcDecl->name->name);
    if (func == ZR_NULL) {
        func = ZrCore_Memory_RawMallocWithType(cs->state->global,
                                         sizeof(SZrCompileTimeFunction),
                                         ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (func == ZR_NULL) {
            return ZR_FALSE;
        }
        ct_compile_time_function_init_record(cs, func, funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
        ZrCore_Array_Push(cs->state, &cs->compileTimeFunctions, &func);
    } else {
        ct_compile_time_function_reset_signature(cs, func);
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
            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }
            ct_compile_time_function_append_parameter(cs, func, &paramNode->data.parameter);
        }
    }

    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterFunction(cs->state, cs->compileTimeTypeEnv, func->name, &func->returnType, &func->paramTypes);
    }

    return ZR_TRUE;
}

TZrBool register_compile_time_function_alias(SZrCompilerState *cs,
                                             SZrString *aliasName,
                                             SZrAstNode *node,
                                             SZrFileRange location) {
    SZrFunctionDeclaration *funcDecl;
    SZrCompileTimeFunction *func;

    if (cs == ZR_NULL || aliasName == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }

    funcDecl = &node->data.functionDeclaration;
    func = find_compile_time_function(cs, aliasName);
    if (func == ZR_NULL) {
        func = ZrCore_Memory_RawMallocWithType(cs->state->global,
                                               sizeof(SZrCompileTimeFunction),
                                               ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (func == ZR_NULL) {
            return ZR_FALSE;
        }
        ct_compile_time_function_init_record(cs, func, funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
        ZrCore_Array_Push(cs->state, &cs->compileTimeFunctions, &func);
    } else {
        ct_compile_time_function_reset_signature(cs, func);
    }

    func->name = aliasName;
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

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }
            ct_compile_time_function_append_parameter(cs, func, &paramNode->data.parameter);
        }
    }

    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterFunction(cs->state,
                                                  cs->compileTimeTypeEnv,
                                                  func->name,
                                                  &func->returnType,
                                                  &func->paramTypes);
    }

    return ZR_TRUE;
}

TZrBool register_compile_time_decorator_class_alias(SZrCompilerState *cs,
                                                    SZrString *aliasName,
                                                    SZrAstNode *node,
                                                    SZrFileRange location) {
    SZrCompileTimeDecoratorClass *decoratorClass;
    SZrAstNodeArray *members = ZR_NULL;
    TZrBool isStructDecorator = ZR_FALSE;

    if (cs == ZR_NULL || aliasName == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_CLASS_DECLARATION) {
        members = node->data.classDeclaration.members;
    } else if (node->type == ZR_AST_STRUCT_DECLARATION) {
        members = node->data.structDeclaration.members;
        isStructDecorator = ZR_TRUE;
    } else {
        return ZR_FALSE;
    }

    decoratorClass = find_compile_time_decorator_class_local(cs, aliasName);
    if (decoratorClass == ZR_NULL) {
        decoratorClass = (SZrCompileTimeDecoratorClass *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrCompileTimeDecoratorClass),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (decoratorClass == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Array_Push(cs->state, &cs->compileTimeDecoratorClasses, &decoratorClass);
    }

    ZrCore_Memory_RawSet(decoratorClass, 0, sizeof(*decoratorClass));
    decoratorClass->name = aliasName;
    decoratorClass->declaration = node;
    decoratorClass->decorateMethod =
            find_compile_time_decorator_meta_method_from_members(members, "decorate", isStructDecorator);
    decoratorClass->constructorMethod =
            find_compile_time_decorator_meta_method_from_members(members, "constructor", isStructDecorator);
    decoratorClass->isStructDecorator = isStructDecorator;
    decoratorClass->location = location;
    return ZR_TRUE;
}
