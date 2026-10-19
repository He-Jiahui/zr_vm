//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "compiler/compile_time/compile_time_executor_internal.h"
#include "compiler/compile_time/comptime_runtime_contract.h"
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

/* Compiler-only names carry the same context identity as their AST spelling.
 * Keep the evaluator's lookup rule in one place so local bindings and item
 * declarations cannot accidentally compare raw mark ids. */
TZrBool ct_contexts_equal(
        SZrSyntaxContext left,
        SZrSyntaxContext right,
        EZrSyntaxNamespace namespaceKind) {
    SZrSyntaxContext normalizedLeft;
    SZrSyntaxContext normalizedRight;

    if (!ZrParser_SyntaxContext_IsValid(left) ||
        !ZrParser_SyntaxContext_IsValid(right)) {
        return ZR_FALSE;
    }
    normalizedLeft = ZrParser_SyntaxContext_Normalize(left, namespaceKind);
    normalizedRight = ZrParser_SyntaxContext_Normalize(right, namespaceKind);
    return ZrParser_SyntaxContext_IsValid(normalizedLeft) &&
           ZrParser_SyntaxContext_IsValid(normalizedRight) &&
           ZrParser_SyntaxContext_Equal(normalizedLeft, normalizedRight);
}

TZrBool ct_value_needs_root(const SZrTypeValue *value) {
    return value != ZR_NULL && value->isGarbageCollectable &&
           value->value.object != ZR_NULL;
}


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


TZrBool ct_compile_time_variable_set_value(
        SZrCompilerState *cs,
        SZrCompileTimeVariable *variable,
        const SZrTypeValue *value) {
    if (cs == ZR_NULL || variable == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ct_value_needs_root(value)) {
        if (!variable->hasEvaluatedValueRoot) {
            if (!ZrCore_GcRootHandle_Create(
                        cs->state,
                        value->value.object,
                        &variable->evaluatedValueRoot)) {
                return ZR_FALSE;
            }
            variable->hasEvaluatedValueRoot = ZR_TRUE;
        } else if (!ZrCore_GcRootHandle_Update(
                           cs->state,
                           &variable->evaluatedValueRoot,
                           value->value.object)) {
            return ZR_FALSE;
        }
    } else if (variable->hasEvaluatedValueRoot) {
        ZrCore_GcRootHandle_Release(
                cs->state, &variable->evaluatedValueRoot);
        variable->hasEvaluatedValueRoot = ZR_FALSE;
    }
    variable->evaluatedValue = *value;
    return ZR_TRUE;
}

TZrBool ct_compile_time_variable_get_value(
        SZrCompilerState *cs,
        SZrCompileTimeVariable *variable,
        SZrTypeValue *result) {
    SZrRawObject *resolved = ZR_NULL;

    if (cs == ZR_NULL || variable == ZR_NULL || result == ZR_NULL ||
        !variable->hasEvaluatedValue) {
        return ZR_FALSE;
    }
    if (variable->hasEvaluatedValueRoot) {
        if (!ZrCore_GcRootHandle_Resolve(
                    cs->state, &variable->evaluatedValueRoot, &resolved) ||
            resolved == ZR_NULL) {
            return ZR_FALSE;
        }
        variable->evaluatedValue.value.object = resolved;
    }
    *result = variable->evaluatedValue;
    return ZR_TRUE;
}

SZrCompileTimeVariable *find_compile_time_variable_with_context(
        SZrCompilerState *cs,
        SZrString *name,
        SZrSyntaxContext syntaxContext) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize i = 0; i < cs->compileTimeVariables.length; i++) {
        SZrCompileTimeVariable **varPtr = (SZrCompileTimeVariable **)ZrCore_Array_Get(&cs->compileTimeVariables, i);
        if (varPtr != ZR_NULL && *varPtr != ZR_NULL && (*varPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*varPtr)->name, name) &&
            ct_contexts_equal((*varPtr)->syntaxContext,
                              syntaxContext,
                              ZR_SYNTAX_NAMESPACE_ITEM)) {
            return *varPtr;
        }
    }
    return ZR_NULL;
}

SZrCompileTimeVariable *find_compile_time_variable(
        SZrCompilerState *cs,
        SZrString *name) {
    return find_compile_time_variable_with_context(
            cs, name, ZrParser_SyntaxContext_Root());
}

SZrCompileTimeFunction *find_compile_time_function_with_context(
        SZrCompilerState *cs,
        SZrString *name,
        SZrSyntaxContext syntaxContext) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    if (cs->activeImportedCompileTimeModule != ZR_NULL) {
        SZrArray *functions =
                &cs->activeImportedCompileTimeModule->compileTimeFunctions;
        for (TZrSize index = 0U; index < functions->length; index++) {
            SZrCompileTimeFunction **function =
                    (SZrCompileTimeFunction **)ZrCore_Array_Get(
                            functions, index);
            if (function != ZR_NULL && *function != ZR_NULL &&
                (*function)->name != ZR_NULL &&
                ZrCore_String_Equal((*function)->name, name) &&
                ct_contexts_equal((*function)->syntaxContext,
                                  syntaxContext,
                                  ZR_SYNTAX_NAMESPACE_ITEM)) {
                return *function;
            }
        }
        return ZR_NULL;
    }
    for (TZrSize i = 0; i < cs->compileTimeFunctions.length; i++) {
        SZrCompileTimeFunction **funcPtr = (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, i);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (*funcPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*funcPtr)->name, name) &&
            ct_contexts_equal((*funcPtr)->syntaxContext,
                              syntaxContext,
                              ZR_SYNTAX_NAMESPACE_ITEM)) {
            return *funcPtr;
        }
    }
    return ZR_NULL;
}

SZrCompileTimeFunction *find_compile_time_function(
        SZrCompilerState *cs,
        SZrString *name) {
    return find_compile_time_function_with_context(
            cs, name, ZrParser_SyntaxContext_Root());
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
    ZrCore_Array_Init(cs->state, &func->paramHasDefaultValues, sizeof(TZrBool), parameterCapacity);
    ZrCore_Array_Init(cs->state, &func->paramDefaultValues, sizeof(SZrTypeValue), parameterCapacity);
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
    func->paramHasDefaultValues.length = 0;
    func->paramDefaultValues.length = 0;
    ZrParser_InferredType_Free(cs->state, &func->returnType);
    ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);
    func->runtimeProjectionModuleName = ZR_NULL;
    func->runtimeProjectionExportName = ZR_NULL;
    func->isRuntimeProjection = ZR_FALSE;
    func->isDeclarationTransform = ZR_FALSE;
    func->isExported = ZR_FALSE;
    func->ownerModule = ZR_NULL;
}

static TZrBool ct_compile_time_function_append_parameter(SZrCompilerState *cs,
                                                         SZrCompileTimeFunction *func,
                                                         SZrParameter *param) {
    SZrInferredType paramType;
    SZrString *paramName = ZR_NULL;
    TZrBool hasDefaultValue = ZR_FALSE;
    SZrTypeValue defaultValue;

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

    ZrCore_Value_ResetAsNull(&defaultValue);
    ZrCore_Array_Push(cs->state, &func->paramTypes, &paramType);
    ZrCore_Array_Push(cs->state, &func->paramNames, &paramName);
    ZrCore_Array_Push(cs->state, &func->paramHasDefaultValues, &hasDefaultValue);
    ZrCore_Array_Push(cs->state, &func->paramDefaultValues, &defaultValue);
    return ZR_TRUE;
}

TZrBool ct_try_get_compile_time_value_with_context(
        SZrCompilerState *cs,
        SZrString *name,
        SZrSyntaxContext syntaxContext,
        SZrTypeValue *result) {
    if (cs == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrCompileTimeVariable *var = find_compile_time_variable_with_context(
            cs, name, syntaxContext);
    if (var == ZR_NULL) {
        return ZR_FALSE;
    }
    if (var->hasEvaluatedValue) {
        return ct_compile_time_variable_get_value(cs, var, result);
    }
    if (var->isEvaluating) {
        ct_error_name(cs, name, "Circular compile-time variable dependency: ", var->location);
        return ZR_FALSE;
    }

    var->isEvaluating = ZR_TRUE;
    if (var->value == ZR_NULL) {
        SZrTypeValue nullValue;
        ZrCore_Value_ResetAsNull(&nullValue);
        if (!ct_compile_time_variable_set_value(cs, var, &nullValue)) {
            var->isEvaluating = ZR_FALSE;
            return ZR_FALSE;
        }
        var->hasEvaluatedValue = ZR_TRUE;
        var->isEvaluating = ZR_FALSE;
        return ct_compile_time_variable_get_value(cs, var, result);
    }

    SZrTypeValue evaluatedValue;
    SZrCompileTimeFrame scratchFrame;
    TZrBool evaluated = ZR_FALSE;
    ct_frame_init(cs, &scratchFrame, ZR_NULL);
    evaluated = evaluate_compile_time_expression_internal(
            cs, var->value, &scratchFrame, &evaluatedValue);
    if (!evaluated ||
        !ct_compile_time_variable_set_value(cs, var, &evaluatedValue)) {
        ct_frame_free(cs, &scratchFrame);
        var->isEvaluating = ZR_FALSE;
        return ZR_FALSE;
    }
    ct_frame_free(cs, &scratchFrame);

    var->hasEvaluatedValue = ZR_TRUE;
    var->isEvaluating = ZR_FALSE;
    return ct_compile_time_variable_get_value(cs, var, result);
}

ZR_PARSER_API TZrBool ZrParser_Compiler_TryGetCompileTimeValue(
        SZrCompilerState *cs,
        SZrString *name,
        SZrTypeValue *result) {
    return ct_try_get_compile_time_value_with_context(
            cs, name, ZrParser_SyntaxContext_Root(), result);
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
    var = find_compile_time_variable_with_context(
            cs,
            varName,
            varDecl->pattern->data.identifier.syntaxContext);
    if (var == ZR_NULL) {
        var = ZrCore_Memory_RawMallocWithType(cs->state->global,
                                        sizeof(SZrCompileTimeVariable),
                                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (var == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(var, 0, sizeof(*var));
        var->name = varName;
        ZrParser_InferredType_Init(cs->state, &var->type, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Push(cs->state, &cs->compileTimeVariables, &var);
    } else {
        ZrParser_InferredType_Free(cs->state, &var->type);
        ZrParser_InferredType_Init(cs->state, &var->type, ZR_VALUE_TYPE_OBJECT);
    }

    var->value = varDecl->value;
    var->syntaxContext = varDecl->pattern->data.identifier.syntaxContext;
    var->location = location;
    if (var->hasEvaluatedValueRoot) {
        ZrCore_GcRootHandle_Release(
                cs->state, &var->evaluatedValueRoot);
        var->hasEvaluatedValueRoot = ZR_FALSE;
    }
    ZrCore_Value_ResetAsNull(&var->evaluatedValue);
    var->hasEvaluatedValue = ZR_FALSE;
    var->isEvaluating = ZR_FALSE;

    if (varDecl->typeInfo != ZR_NULL &&
        !ZrParser_AstTypeToInferredType_Convert(cs, varDecl->typeInfo, &var->type)) {
        ZrParser_InferredType_Free(cs->state, &var->type);
        ZrParser_InferredType_Init(cs->state, &var->type, ZR_VALUE_TYPE_OBJECT);
    }

    if (varDecl->value != ZR_NULL) {
        if (!ct_try_get_compile_time_value_with_context(
                    cs,
                    varName,
                    varDecl->pattern->data.identifier.syntaxContext,
                    &value)) {
            return ZR_FALSE;
        }
        if (varDecl->typeInfo == ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, &var->type);
            ct_init_type_from_value(cs, &value, &var->type);
        }
    } else {
        SZrTypeValue nullValue;
        ZrCore_Value_ResetAsNull(&nullValue);
        if (!ct_compile_time_variable_set_value(cs, var, &nullValue)) {
            return ZR_FALSE;
        }
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
    func = find_compile_time_function_with_context(
            cs,
            funcDecl->name->name,
            funcDecl->name->syntaxContext);
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
    func->syntaxContext = funcDecl->name->syntaxContext;
    func->declaration = node;
    func->isExported =
            (funcDecl->accessModifier == ZR_ACCESS_PUBLIC ||
             funcDecl->accessModifier == ZR_ACCESS_PROTECTED)
                    ? ZR_TRUE
                    : ZR_FALSE;
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
    func->syntaxContext = ZrParser_SyntaxContext_Root();
    func->declaration = node;
    func->isExported =
            (funcDecl->accessModifier == ZR_ACCESS_PUBLIC ||
             funcDecl->accessModifier == ZR_ACCESS_PROTECTED)
                    ? ZR_TRUE
                    : ZR_FALSE;
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
