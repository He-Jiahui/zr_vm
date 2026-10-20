#include "compiler/compile_time/compile_time_executor_internal.h"
#include "compiler/tool/compile_tool_execution_scope.h"
#include "zr_vm_parser/ast.h"

#include <string.h>

static SZrString *ct_decorator_target_type_name(SZrType *typeInfo) {
    while (typeInfo != ZR_NULL && typeInfo->subType != ZR_NULL) {
        typeInfo = typeInfo->subType;
    }
    if (typeInfo == ZR_NULL || typeInfo->name == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return typeInfo->name->data.identifier.name;
    }

    if (typeInfo->name->type == ZR_AST_GENERIC_TYPE &&
        typeInfo->name->data.genericType.name != ZR_NULL) {
        return typeInfo->name->data.genericType.name->name;
    }

    return ZR_NULL;
}

static TZrBool ct_validate_named_decorator_target_param(
        SZrCompilerState *cs,
        SZrParameter *param,
        const TZrChar *expectedName,
        const TZrChar *decoratorKind,
        SZrFileRange location) {
    SZrString *typeName;

    if (cs == ZR_NULL || param == ZR_NULL) {
        return ZR_FALSE;
    }

    typeName = ct_decorator_target_type_name(param->typeInfo);
    if (typeName != ZR_NULL &&
        ((expectedName != ZR_NULL && ct_string_equals(typeName, expectedName)) ||
         ct_string_equals(typeName, "DeclarationView") ||
         ((expectedName != ZR_NULL &&
           (strcmp(expectedName, "Class") == 0 ||
            strcmp(expectedName, "Struct") == 0 ||
            strcmp(expectedName, "TypeView") == 0)) &&
          ct_string_equals(typeName, "TypeView")))) {
        return ZR_TRUE;
    }

    ZrParser_CompileTime_Error(cs,
                              ZR_COMPILE_TIME_ERROR_ERROR,
                              decoratorKind != ZR_NULL
                                      ? decoratorKind
                                      : "Compile-time decorator target parameter must use a canonical declaration view",
                              location);
    return ZR_FALSE;
}

TZrBool ct_execute_compile_time_decorator_function(
        SZrCompilerState *cs,
        SZrCompileTimeFunction *decoratorFunction,
        SZrFunctionCall *constructorCall,
        const SZrTypeValue *targetSnapshot,
        const TZrChar *expectedTargetName,
        SZrCompileTimeFrame *lifetimeFrame,
        SZrTypeValue *patchResult,
        SZrFileRange location) {
    SZrFunctionDeclaration *decl;
    SZrCompileTimeFrame frame;
    TZrBool success = ZR_FALSE;
    TZrBool didReturn = ZR_FALSE;
    TZrSize expectedArgumentCount = 0;
    EZrParserComptimeContext oldComptimeContext;
    SZrCompileToolExecutionScope executionScope;

    if (cs == ZR_NULL || decoratorFunction == ZR_NULL || targetSnapshot == ZR_NULL ||
        lifetimeFrame == ZR_NULL || patchResult == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!decoratorFunction->isDeclarationTransform ||
        decoratorFunction->isRuntimeProjection) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "decorator.runtime_removed: only compiler-owned declarationTransform functions may execute as decorators",
                location);
        return ZR_FALSE;
    }

    if (decoratorFunction->declaration == ZR_NULL ||
        decoratorFunction->declaration->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }

    decl = &decoratorFunction->declaration->data.functionDeclaration;
    if (decl->params == ZR_NULL || decl->params->count == 0 || decl->params->nodes[0] == ZR_NULL ||
        decl->params->nodes[0]->type != ZR_AST_PARAMETER) {
        ZrParser_CompileTime_Error(cs,
                                  ZR_COMPILE_TIME_ERROR_ERROR,
                                  "Compile-time decorator function must declare a target parameter",
                                  location);
        return ZR_FALSE;
    }

    if (!ct_validate_named_decorator_target_param(
                cs,
                &decl->params->nodes[0]->data.parameter,
                expectedTargetName,
                "Compile-time decorator function target must use a canonical declaration view",
                location)) {
        return ZR_FALSE;
    }

    expectedArgumentCount = decl->params->count > 0 ? decl->params->count - 1 : 0;
    if (constructorCall != ZR_NULL && constructorCall->args != ZR_NULL && decl->args == ZR_NULL &&
        constructorCall->args->count > expectedArgumentCount) {
        ZrParser_CompileTime_Error(cs,
                                  ZR_COMPILE_TIME_ERROR_ERROR,
                                  "Too many arguments for compile-time decorator function",
                                  location);
        return ZR_FALSE;
    }

    if (!ZrParser_CompileToolExecutionScope_EnterFunction(
                cs, decoratorFunction, &executionScope)) {
        return ZR_FALSE;
    }

    ct_frame_init(cs, &frame, ZR_NULL);
    if (!ct_frame_declare_with_context(
                cs,
                &frame,
                decl->params->nodes[0]->data.parameter.name != ZR_NULL
                        ? decl->params->nodes[0]->data.parameter.name->name
                        : ZR_NULL,
                decl->params->nodes[0]->data.parameter.name != ZR_NULL
                        ? decl->params->nodes[0]->data.parameter.name->syntaxContext
                        : ZrParser_SyntaxContext_Root(),
                targetSnapshot,
                decl->params->nodes[0]->data.parameter.isConst)) {
        ct_frame_free(cs, &frame);
        ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
        return ZR_FALSE;
    }

    for (TZrSize paramIndex = 1; paramIndex < decl->params->count; paramIndex++) {
        SZrAstNode *paramNode = decl->params->nodes[paramIndex];
        SZrParameter *param;
        SZrTypeValue argValue;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        param = &paramNode->data.parameter;
        if (param->name == ZR_NULL || param->name->name == ZR_NULL ||
            !ct_eval_call_arg(cs, constructorCall, param, paramIndex - 1, &frame, &argValue) ||
            !ct_frame_declare_with_context(
                    cs,
                    &frame,
                    param->name->name,
                    param->name->syntaxContext,
                    &argValue,
                    param->isConst)) {
            ct_frame_free(cs, &frame);
            ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
            return ZR_FALSE;
        }
    }

    if (decl->body == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                  ZR_COMPILE_TIME_ERROR_ERROR,
                                  "Compile-time decorator function body is null",
                                  location);
        ct_frame_free(cs, &frame);
        ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
        return ZR_FALSE;
    }

    oldComptimeContext = cs->comptimeContext;
    cs->comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM;
    success = decl->body->type == ZR_AST_BLOCK
                      ? execute_compile_time_block(cs, decl->body, &frame, &didReturn, patchResult)
                      : execute_compile_time_statement(cs, decl->body, &frame, &didReturn, patchResult);
    cs->comptimeContext = oldComptimeContext;
    if (success && !didReturn) {
        ZrParser_CompileTime_Error(cs,
                                  ZR_COMPILE_TIME_ERROR_ERROR,
                                  "declaration_transform.return: transform must return declaration.Patch",
                                  location);
        success = ZR_FALSE;
    }
    /* The caller retains patches through later transforms and patch application. */
    if (success && !ct_frame_root_value(cs, lifetimeFrame, patchResult)) {
        success = ZR_FALSE;
    }

    ct_frame_free(cs, &frame);
    ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
    return success;
}
