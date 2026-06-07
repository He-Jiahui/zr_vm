#include "repl/repl.h"
#include "repl/repl_input_scan.h"
#include "repl/repl_semantic_facts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_cli/conf.h"
#include "project/project.h"
#include "runtime/runtime.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

static void zr_cli_repl_prepare_stdio(void) {
    static TZrBool prepared = ZR_FALSE;

    if (prepared) {
        return;
    }

    (void)setvbuf(stdout, ZR_NULL, _IONBF, 0);
    (void)setvbuf(stderr, ZR_NULL, _IONBF, 0);
    prepared = ZR_TRUE;
}

static void zr_cli_repl_write_help(void) {
    ZrCore_Log_Helpf(ZR_NULL,
                     "Available commands:\n"
                     "  :help   Show this help text.\n"
                     "  :reset  Clear the pending input buffer.\n"
                     "  :type   Show inferred type and local semantic facts for an expression.\n"
                     "  :quit   Exit the REPL.\n");
}

typedef struct ZrCliReplExecuteRequest {
    TZrStackValuePointer callBase;
    TZrStackValuePointer resultBase;
} ZrCliReplExecuteRequest;

typedef struct ZrCliReplSessionContext {
    TZrChar *source;
    TZrSize length;
    TZrSize capacity;
} ZrCliReplSessionContext;

static void zr_cli_repl_execute_body(SZrState *state, TZrPtr arguments) {
    ZrCliReplExecuteRequest *request = (ZrCliReplExecuteRequest *) arguments;

    if (state == ZR_NULL || request == ZR_NULL || request->callBase == ZR_NULL) {
        return;
    }

    request->resultBase = ZrCore_Function_CallAndRestore(state, request->callBase, 1);
}

static TZrBool zr_cli_repl_handle_failure(SZrState *state, EZrThreadStatus status) {
    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!state->hasCurrentException &&
        !ZrCore_Exception_NormalizeStatus(state,
                                          state->threadStatus != ZR_THREAD_STATUS_FINE ? state->threadStatus : status)) {
        ZrCore_Log_Error(state, "repl execution failed with status %d\n", (int)status);
        ZrCore_State_ResetThread(state, status);
        return ZR_FALSE;
    }

    if (state->hasCurrentException) {
        ZrCore_Exception_LogUnhandled(state, &state->currentException);
        ZrCore_State_ResetThread(state, state->currentExceptionStatus);
        return ZR_FALSE;
    }

    ZrCore_Log_Error(state, "repl execution failed with status %d\n", (int)status);
    ZrCore_State_ResetThread(state, status);
    return ZR_FALSE;
}

static TZrBool zr_cli_repl_execute(SZrState *state, TZrStackValuePointer callBase, SZrTypeValue *result) {
    ZrCliReplExecuteRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || callBase == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&request, 0, sizeof(request));
    request.callBase = callBase;
    ZrCore_Value_ResetAsNull(result);
    state->threadStatus = ZR_THREAD_STATUS_FINE;

    status = ZrCore_Exception_TryRun(state, zr_cli_repl_execute_body, &request);
    if (status != ZR_THREAD_STATUS_FINE) {
        return zr_cli_repl_handle_failure(state, status);
    }

    if (state->threadStatus != ZR_THREAD_STATUS_FINE || request.resultBase == ZR_NULL) {
        return state->threadStatus == ZR_THREAD_STATUS_FINE ? ZR_FALSE :
               zr_cli_repl_handle_failure(state, state->threadStatus);
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
    return ZR_TRUE;
}

static void zr_cli_repl_free_global(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return;
    }

    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
}

static void zr_cli_repl_session_free(ZrCliReplSessionContext *session) {
    if (session == ZR_NULL) {
        return;
    }

    free(session->source);
    session->source = ZR_NULL;
    session->length = 0;
    session->capacity = 0;
}

static TZrChar *zr_cli_repl_build_return_wrapper(const TZrChar *code) {
    static const TZrChar prefix[] = "return ";
    static const TZrChar suffix[] = ";";
    const TZrChar *begin;
    const TZrChar *end;
    TZrSize expressionLength;
    TZrSize prefixLength;
    TZrSize suffixLength;
    TZrSize wrapperLength;
    TZrChar *wrapper;

    begin = ZrCli_ReplInput_SkipSpace(code);
    if (begin == ZR_NULL || *begin == '\0') {
        return ZR_NULL;
    }

    end = begin + strlen(begin);
    while (end > begin && ZrCli_ReplInput_IsSpace(end[-1])) {
        --end;
    }

    expressionLength = (TZrSize)(end - begin);
    prefixLength = strlen(prefix);
    suffixLength = strlen(suffix);
    wrapperLength = prefixLength + expressionLength + suffixLength;

    wrapper = (TZrChar *) malloc(wrapperLength + 1);
    if (wrapper == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(wrapper, prefix, prefixLength);
    memcpy(wrapper + prefixLength, begin, expressionLength);
    memcpy(wrapper + prefixLength + expressionLength, suffix, suffixLength);
    wrapper[wrapperLength] = '\0';
    return wrapper;
}

static TZrChar *zr_cli_repl_build_expression_statement_source(const TZrChar *code) {
    const TZrChar *begin;
    const TZrChar *end;
    TZrSize expressionLength;
    TZrBool hasTrailingSemicolon;
    TZrChar *source;

    begin = ZrCli_ReplInput_SkipSpace(code);
    if (begin == ZR_NULL || *begin == '\0') {
        return ZR_NULL;
    }

    end = begin + strlen(begin);
    while (end > begin && ZrCli_ReplInput_IsSpace(end[-1])) {
        --end;
    }
    if (end == begin) {
        return ZR_NULL;
    }

    hasTrailingSemicolon = (TZrBool)(end[-1] == ';');
    expressionLength = (TZrSize)(end - begin);
    source = (TZrChar *)malloc(expressionLength + (hasTrailingSemicolon ? 1u : 2u));
    if (source == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(source, begin, expressionLength);
    if (!hasTrailingSemicolon) {
        source[expressionLength++] = ';';
    }
    source[expressionLength] = '\0';
    return source;
}

static TZrChar *zr_cli_repl_build_prefixed_source(const TZrChar *prefix, const TZrChar *code) {
    TZrSize prefixLength;
    TZrSize codeLength;
    TZrSize separatorLength;
    TZrChar *source;

    if (code == ZR_NULL) {
        return ZR_NULL;
    }

    prefixLength = prefix != ZR_NULL ? strlen(prefix) : 0;
    codeLength = strlen(code);
    separatorLength = (prefixLength > 0 && prefix[prefixLength - 1] != '\n') ? 1u : 0u;

    source = (TZrChar *)malloc(prefixLength + separatorLength + codeLength + 1u);
    if (source == ZR_NULL) {
        return ZR_NULL;
    }

    if (prefixLength > 0) {
        memcpy(source, prefix, prefixLength);
    }
    if (separatorLength > 0) {
        source[prefixLength] = '\n';
    }
    if (codeLength > 0) {
        memcpy(source + prefixLength + separatorLength, code, codeLength);
    }
    source[prefixLength + separatorLength + codeLength] = '\0';
    return source;
}

static SZrAstNode *zr_cli_repl_last_expression_statement_expression(SZrAstNode *ast) {
    SZrAstNode *statement;

    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count == 0) {
        return ZR_NULL;
    }

    statement = ast->data.script.statements->nodes[ast->data.script.statements->count - 1u];
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

static TZrBool zr_cli_repl_session_append(ZrCliReplSessionContext *session, const TZrChar *code) {
    const TZrChar *begin;
    TZrSize codeLength;
    TZrSize separatorLength;
    TZrSize requiredCapacity;
    TZrSize newCapacity;
    TZrChar *newSource;

    if (session == ZR_NULL || code == ZR_NULL) {
        return ZR_FALSE;
    }

    begin = ZrCli_ReplInput_SkipSpace(code);
    if (begin == ZR_NULL || *begin == '\0') {
        return ZR_TRUE;
    }

    codeLength = strlen(begin);
    separatorLength = (session->length > 0 && session->source[session->length - 1] != '\n') ? 1u : 0u;
    requiredCapacity = session->length + separatorLength + codeLength + 2u;
    if (requiredCapacity > session->capacity) {
        newCapacity = session->capacity > 0 ? session->capacity : ZR_CLI_REPL_BUFFER_INITIAL_CAPACITY;
        while (newCapacity < requiredCapacity) {
            newCapacity *= ZR_CLI_COLLECTION_GROWTH_FACTOR;
        }
        newSource = (TZrChar *)realloc(session->source, newCapacity);
        if (newSource == ZR_NULL) {
            return ZR_FALSE;
        }
        session->source = newSource;
        session->capacity = newCapacity;
    }

    if (separatorLength > 0) {
        session->source[session->length++] = '\n';
    }
    memcpy(session->source + session->length, begin, codeLength);
    session->length += codeLength;
    if (session->length == 0 || session->source[session->length - 1] != '\n') {
        session->source[session->length++] = '\n';
    }
    session->source[session->length] = '\0';
    return ZR_TRUE;
}

static TZrBool zr_cli_repl_should_persist_submission(const TZrChar *code) {
    const TZrChar *trimmed = ZrCli_ReplInput_SkipSpace(code);

    if (trimmed == ZR_NULL || *trimmed == '\0') {
        return ZR_FALSE;
    }

    if (ZrCli_ReplInput_ShouldWrapExpression(trimmed)) {
        return ZR_FALSE;
    }

    if (ZrCli_ReplInput_IsSimpleAssignmentStatement(trimmed)) {
        return ZR_TRUE;
    }

    return (TZrBool)(ZrCli_ReplInput_StartsWithKeyword(trimmed, "class") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "const") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "enum") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "extern") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "func") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "interface") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "module") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "pri") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "pro") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "pub") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "struct") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "using") ||
                     ZrCli_ReplInput_StartsWithKeyword(trimmed, "var"));
}

static TZrBool zr_cli_repl_register_variable_type_for_query(SZrCompilerState *compilerState,
                                                            SZrAstNode *statement) {
    SZrVariableDeclaration *declaration;
    SZrAstNode *pattern;
    SZrString *name;
    SZrInferredType inferredType;
    TZrBool inferredTypeInitialized = ZR_FALSE;
    TZrBool registered = ZR_FALSE;

    if (compilerState == ZR_NULL || statement == ZR_NULL ||
        statement->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_TRUE;
    }

    declaration = &statement->data.variableDeclaration;
    pattern = declaration->pattern;
    if (pattern == ZR_NULL || pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        pattern->data.identifier.name == ZR_NULL) {
        return ZR_TRUE;
    }

    name = pattern->data.identifier.name;
    ZrParser_InferredType_Init(compilerState->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    inferredTypeInitialized = ZR_TRUE;

    if (declaration->typeInfo != ZR_NULL) {
        if (!ZrParser_AstTypeToInferredType_Convert(compilerState, declaration->typeInfo, &inferredType)) {
            goto cleanup;
        }
    } else if (declaration->value != ZR_NULL) {
        if (!ZrParser_ExpressionType_Infer(compilerState, declaration->value, &inferredType)) {
            goto cleanup;
        }
    }

    registered = ZrParser_TypeEnvironment_RegisterVariableEx(compilerState->state,
                                                             compilerState->typeEnv,
                                                             name,
                                                             &inferredType,
                                                             pattern,
                                                             pattern->location);

cleanup:
    if (inferredTypeInitialized) {
        ZrParser_InferredType_Free(compilerState->state, &inferredType);
    }
    return registered;
}

static TZrBool zr_cli_repl_register_assignment_type_for_query(SZrCompilerState *compilerState,
                                                              SZrAstNode *statement) {
    SZrAstNode *expr;
    SZrAssignmentExpression *assignment;
    SZrAstNode *target;
    SZrString *name;
    SZrInferredType inferredType;
    SZrFileRange emptyRange = {0};
    TZrBool inferredTypeInitialized = ZR_FALSE;
    TZrBool registered = ZR_FALSE;

    if (compilerState == ZR_NULL ||
        statement == ZR_NULL ||
        statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_TRUE;
    }

    expr = statement->data.expressionStatement.expr;
    if (expr == ZR_NULL || expr->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        return ZR_TRUE;
    }

    assignment = &expr->data.assignmentExpression;
    target = assignment->left;
    if (target == ZR_NULL ||
        target->type != ZR_AST_IDENTIFIER_LITERAL ||
        target->data.identifier.name == ZR_NULL ||
        ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv,
                                                     target->data.identifier.name) == ZR_NULL) {
        return ZR_TRUE;
    }

    name = target->data.identifier.name;
    ZrParser_InferredType_Init(compilerState->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    inferredTypeInitialized = ZR_TRUE;
    if (!ZrParser_ExpressionType_Infer(compilerState, expr, &inferredType)) {
        goto cleanup;
    }

    registered = ZrParser_TypeEnvironment_RegisterVariableEx(compilerState->state,
                                                             compilerState->typeEnv,
                                                             name,
                                                             &inferredType,
                                                             ZR_NULL,
                                                             emptyRange);

cleanup:
    if (inferredTypeInitialized) {
        ZrParser_InferredType_Free(compilerState->state, &inferredType);
    }
    return registered;
}

static void zr_cli_repl_free_parameter_types_for_query(SZrCompilerState *compilerState,
                                                       SZrArray *paramTypes) {
    if (compilerState == ZR_NULL || paramTypes == ZR_NULL ||
        !paramTypes->isValid || paramTypes->head == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < paramTypes->length; ++index) {
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(paramTypes, index);
        if (paramType != ZR_NULL) {
            ZrParser_InferredType_Free(compilerState->state, paramType);
        }
    }
    ZrCore_Array_Free(compilerState->state, paramTypes);
}

static TZrBool zr_cli_repl_collect_function_parameters_for_query(SZrCompilerState *compilerState,
                                                                 SZrAstNodeArray *params,
                                                                 SZrArray *paramTypes,
                                                                 SZrArray *parameterPassingModes) {
    TZrSize capacity = params != ZR_NULL && params->count > 0
                               ? params->count
                               : ZR_PARSER_INITIAL_CAPACITY_SMALL;

    if (compilerState == ZR_NULL || paramTypes == ZR_NULL || parameterPassingModes == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(compilerState->state, paramTypes, sizeof(SZrInferredType), capacity);
    ZrCore_Array_Construct(parameterPassingModes);

    if (params == ZR_NULL || params->count == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(compilerState->state,
                      parameterPassingModes,
                      sizeof(EZrParameterPassingMode),
                      params->count);

    for (TZrSize index = 0; index < params->count; ++index) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrInferredType paramType;
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        if (paramNode->data.parameter.typeInfo != ZR_NULL) {
            if (!ZrParser_AstTypeToInferredType_Convert(compilerState,
                                                        paramNode->data.parameter.typeInfo,
                                                        &paramType)) {
                zr_cli_repl_free_parameter_types_for_query(compilerState, paramTypes);
                if (parameterPassingModes->isValid && parameterPassingModes->head != ZR_NULL) {
                    ZrCore_Array_Free(compilerState->state, parameterPassingModes);
                }
                return ZR_FALSE;
            }
        } else {
            ZrParser_InferredType_Init(compilerState->state, &paramType, ZR_VALUE_TYPE_OBJECT);
        }

        passingMode = paramNode->data.parameter.passingMode;
        ZrCore_Array_Push(compilerState->state, paramTypes, &paramType);
        ZrCore_Array_Push(compilerState->state, parameterPassingModes, &passingMode);
    }

    return ZR_TRUE;
}

static TZrBool zr_cli_repl_register_function_type_for_query(SZrCompilerState *compilerState,
                                                            SZrAstNode *statement) {
    SZrFunctionDeclaration *declaration;
    SZrInferredType returnType;
    SZrArray paramTypes;
    SZrArray parameterPassingModes;
    TZrBool returnTypeInitialized = ZR_FALSE;
    TZrBool paramTypesInitialized = ZR_FALSE;

    if (compilerState == ZR_NULL || statement == ZR_NULL ||
        statement->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_TRUE;
    }

    declaration = &statement->data.functionDeclaration;
    if (declaration->name == ZR_NULL || declaration->name->name == ZR_NULL) {
        return ZR_TRUE;
    }

    if (declaration->returnType != ZR_NULL) {
        if (!ZrParser_AstTypeToInferredType_Convert(compilerState,
                                                    declaration->returnType,
                                                    &returnType)) {
            return ZR_FALSE;
        }
    } else {
        ZrParser_InferredType_Init(compilerState->state, &returnType, ZR_VALUE_TYPE_OBJECT);
    }
    returnTypeInitialized = ZR_TRUE;

    if (!zr_cli_repl_collect_function_parameters_for_query(compilerState,
                                                           declaration->params,
                                                           &paramTypes,
                                                           &parameterPassingModes)) {
        goto cleanup;
    }
    paramTypesInitialized = ZR_TRUE;

    (void)ZrParser_TypeEnvironment_RegisterFunctionEx(compilerState->state,
                                                       compilerState->typeEnv,
                                                       declaration->name->name,
                                                       &returnType,
                                                       &paramTypes,
                                                       ZR_NULL,
                                                       &parameterPassingModes,
                                                       statement);

cleanup:
    if (returnTypeInitialized) {
        ZrParser_InferredType_Free(compilerState->state, &returnType);
    }
    if (paramTypesInitialized) {
        zr_cli_repl_free_parameter_types_for_query(compilerState, &paramTypes);
        if (parameterPassingModes.isValid && parameterPassingModes.head != ZR_NULL) {
            ZrCore_Array_Free(compilerState->state, &parameterPassingModes);
        }
    }
    return (TZrBool)(paramTypesInitialized && !compilerState->hasError);
}

static TZrBool zr_cli_repl_register_prior_types_for_query(SZrCompilerState *compilerState,
                                                          SZrAstNode *ast) {
    SZrAstNodeArray *statements;

    if (compilerState == ZR_NULL || ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL) {
        return ZR_TRUE;
    }

    statements = ast->data.script.statements;
    if (statements->count == 0) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index + 1u < statements->count; ++index) {
        SZrAstNode *statement = statements->nodes[index];
        if (!zr_cli_repl_register_function_type_for_query(compilerState, statement)) {
            return ZR_FALSE;
        }
    }

    for (TZrSize index = 0; index + 1u < statements->count; ++index) {
        SZrAstNode *statement = statements->nodes[index];
        if (!zr_cli_repl_register_variable_type_for_query(compilerState, statement) ||
            !zr_cli_repl_register_assignment_type_for_query(compilerState, statement)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static int zr_cli_repl_type_query(const TZrChar *sessionSource, const TZrChar *expression) {
    SZrGlobalState *global;
    SZrState *state = ZR_NULL;
    SZrString *sourceName;
    TZrChar *source;
    TZrChar *analysisSource = ZR_NULL;
    SZrParserState parserState;
    SZrAstNode *ast = ZR_NULL;
    SZrAstNode *expr;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    TZrChar typeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    const TZrChar *typeText;
    TZrBool compilerStateInitialized = ZR_FALSE;
    TZrBool inferredTypeInitialized = ZR_FALSE;
    int exitCode = 1;

    if (expression == ZR_NULL || *ZrCli_ReplInput_SkipSpace(expression) == '\0') {
        ZrCore_Log_Error(ZR_NULL, "usage: :type <expression>\n");
        return 1;
    }

    source = zr_cli_repl_build_expression_statement_source(expression);
    if (source == ZR_NULL) {
        ZrCore_Log_Error(ZR_NULL, "failed to prepare REPL type query\n");
        return 1;
    }

    global = ZrCli_Project_CreateBareGlobal();
    if (global == ZR_NULL) {
        free(source);
        ZrCore_Log_Error(ZR_NULL, "failed to initialize REPL type query runtime\n");
        return 1;
    }

    if (!ZrCli_Project_RegisterStandardModules(global)) {
        ZrCore_Log_Error(global->mainThreadState, "failed to register REPL standard modules\n");
        goto cleanup;
    }

    state = global->mainThreadState;
    if (!ZrCli_Runtime_InjectProcessArguments(state, "<repl>", ZR_NULL, 0)) {
        ZrCore_Log_Error(state, "failed to initialize REPL process arguments\n");
        goto cleanup;
    }

    sourceName = ZrCore_String_CreateFromNative(state, "<repl:type>");
    analysisSource = zr_cli_repl_build_prefixed_source(sessionSource, source);
    if (analysisSource == ZR_NULL) {
        ZrCore_Log_Error(state, "failed to prepare REPL type query context\n");
        goto cleanup;
    }
    ZrParser_State_Init(&parserState, state, analysisSource, strlen(analysisSource), sourceName);
    ast = ZrParser_ParseWithState(&parserState);
    if (parserState.hasError || ast == ZR_NULL) {
        ZrParser_State_Free(&parserState);
        goto cleanup;
    }
    ZrParser_State_Free(&parserState);

    expr = zr_cli_repl_last_expression_statement_expression(ast);
    if (expr == ZR_NULL) {
        ZrCore_Log_Error(state, ":type expects an expression, not a statement or declaration\n");
        goto cleanup;
    }

    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, state);
    compilerStateInitialized = ZR_TRUE;
    compilerState.currentAst = ast;
    compilerState.scriptAst = ast;
    compilerState.suppressErrorOutput = ZR_FALSE;

    if (!zr_cli_repl_register_prior_types_for_query(&compilerState, ast)) {
        ZrCore_Log_Error(state, "failed to infer prior REPL session declarations\n");
        goto cleanup;
    }

    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    inferredTypeInitialized = ZR_TRUE;
    if (!ZrParser_ExpressionType_Infer(&compilerState, expr, &inferredType)) {
        ZrCore_Log_Error(state, "failed to infer expression type\n");
        goto cleanup;
    }

    typeText = ZrParser_TypeNameString_Get(state, &inferredType, typeBuffer, sizeof(typeBuffer));
    ZrCore_Log_Resultf(state, "Type: %s\n", typeText != ZR_NULL ? typeText : "unknown");

    ZrCli_ReplSemanticFacts_WriteNumericForExpression(state, compilerState.semanticContext, expr);
    ZrCli_ReplSemanticFacts_WriteLogicalForExpression(state, compilerState.semanticContext, expr);
    ZrCli_ReplSemanticFacts_WriteOwnershipForExpression(state, compilerState.semanticContext, expr);
    ZrCli_ReplSemanticFacts_WriteExpressionForExpression(state, compilerState.semanticContext, expr);
    ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, compilerState.semanticContext, expr);
    ZrCli_ReplSemanticFacts_WriteReachabilityForExpression(state, compilerState.semanticContext, expr);

    exitCode = 0;

cleanup:
    if (inferredTypeInitialized) {
        ZrParser_InferredType_Free(state, &inferredType);
    }
    if (compilerStateInitialized) {
        ZrParser_CompilerState_Free(&compilerState);
    }
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    free(analysisSource);
    free(source);
    zr_cli_repl_free_global(global);
    return exitCode;
}

static int zr_cli_repl_submit(const TZrChar *sessionSource, const TZrChar *code) {
    SZrGlobalState *global;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;
    SZrClosure *closure;
    TZrStackValuePointer callBase;
    SZrTypeValue *closureValue;
    SZrTypeValue result;
    SZrString *resultString;
    TZrChar *wrappedExpressionCode = ZR_NULL;
    TZrChar *sessionCompileCode = ZR_NULL;
    const TZrChar *compileCode = code;
    TZrBool ignoredFunction = ZR_FALSE;
    TZrBool ignoredClosure = ZR_FALSE;
    int exitCode = 1;

    if (code == ZR_NULL || code[0] == '\0') {
        return 0;
    }

    global = ZrCli_Project_CreateBareGlobal();
    if (global == ZR_NULL) {
        ZrCore_Log_Error(ZR_NULL, "failed to initialize REPL runtime\n");
        return 1;
    }

    if (!ZrCli_Project_RegisterStandardModules(global)) {
        ZrCore_Log_Error(global->mainThreadState, "failed to register REPL standard modules\n");
        zr_cli_repl_free_global(global);
        return 1;
    }

    state = global->mainThreadState;
    if (!ZrCli_Runtime_InjectProcessArguments(state, "<repl>", ZR_NULL, 0)) {
        ZrCore_Log_Error(state, "failed to initialize REPL process arguments\n");
        zr_cli_repl_free_global(global);
        return 1;
    }

    sourceName = ZrCore_String_CreateFromNative(state, "<repl>");
    if (ZrCli_ReplInput_ShouldWrapExpression(code)) {
        wrappedExpressionCode = zr_cli_repl_build_return_wrapper(code);
        if (wrappedExpressionCode != ZR_NULL) {
            compileCode = wrappedExpressionCode;
        }
    }
    sessionCompileCode = zr_cli_repl_build_prefixed_source(sessionSource, compileCode);
    if (sessionCompileCode == ZR_NULL) {
        free(wrappedExpressionCode);
        zr_cli_repl_free_global(global);
        return 1;
    }
    compileCode = sessionCompileCode;

    function = ZrParser_Source_Compile(state, compileCode, strlen(compileCode), sourceName);
    if (function == ZR_NULL) {
        free(sessionCompileCode);
        free(wrappedExpressionCode);
        zr_cli_repl_free_global(global);
        return 1;
    }

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        free(sessionCompileCode);
        free(wrappedExpressionCode);
        zr_cli_repl_free_global(global);
        return 1;
    }

    ignoredFunction = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    ignoredClosure = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, function->stackSize + 1, callBase);
    closureValue = ZrCore_Stack_GetValue(callBase);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer = callBase + 1;
    if (ignoredClosure) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
        ignoredClosure = ZR_FALSE;
    }
    if (!zr_cli_repl_execute(state, callBase, &result)) {
        goto zr_cli_repl_cleanup;
    }

    resultString = ZrCore_Value_ConvertToString(state, &result);
    if (resultString != ZR_NULL) {
        ZrCore_Log_Resultf(state, "%s\n", ZrCore_String_GetNativeString(resultString));
    } else {
        ZrCore_Log_Error(state, "failed to stringify REPL result\n");
    }

    exitCode = 0;

zr_cli_repl_cleanup:
    if (ignoredClosure) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    }
    if (ignoredFunction) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    }
    free(wrappedExpressionCode);
    free(sessionCompileCode);
    zr_cli_repl_free_global(global);
    return exitCode;
}

int ZrCli_Repl_Run(void) {
    TZrChar line[ZR_CLI_REPL_LINE_BUFFER_LENGTH];
    TZrChar *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    TZrSize bufferCapacity = 0;
    ZrCliReplSessionContext session;

    memset(&session, 0, sizeof(session));
    zr_cli_repl_prepare_stdio();
    ZrCore_Log_Helpf(ZR_NULL,
                     "ZR VM REPL\n"
                     "Enter code, then submit with an empty line. Type :help for commands.\n");
    ZrCore_Log_FlushDefaultSinks();

    for (;;) {
        TZrSize lineLength;

        ZrCore_Log_FlushDefaultSinks();
        if (fgets(line, sizeof(line), stdin) == ZR_NULL) {
            break;
        }

        lineLength = strlen(line);
        while (lineLength > 0 && (line[lineLength - 1] == '\n' || line[lineLength - 1] == '\r')) {
            line[--lineLength] = '\0';
        }

        if (bufferLength == 0 && line[0] == ':') {
            if (strcmp(line, ":help") == 0) {
                zr_cli_repl_write_help();
            } else if (strcmp(line, ":quit") == 0) {
                ZrCore_Log_FlushDefaultSinks();
                free(buffer);
                zr_cli_repl_session_free(&session);
                return 0;
            } else if (strcmp(line, ":reset") == 0) {
                bufferLength = 0;
                if (buffer != ZR_NULL) {
                    buffer[0] = '\0';
                }
                zr_cli_repl_session_free(&session);
            } else if (ZrCli_ReplInput_StartsWithKeyword(line, ":type")) {
                (void)zr_cli_repl_type_query(session.source, line + 5);
            } else {
                ZrCore_Log_Error(ZR_NULL, "unknown REPL command: %s\n", line);
            }
            ZrCore_Log_FlushDefaultSinks();
            continue;
        }

        if (lineLength == 0) {
            if (bufferLength > 0) {
                TZrBool shouldPersist = zr_cli_repl_should_persist_submission(buffer);
                if (zr_cli_repl_submit(session.source, buffer) == 0 && shouldPersist &&
                    !zr_cli_repl_session_append(&session, buffer)) {
                    ZrCore_Log_Error(ZR_NULL, "failed to persist REPL session source\n");
                }
                bufferLength = 0;
                if (buffer != ZR_NULL) {
                    buffer[0] = '\0';
                }
                ZrCore_Log_FlushDefaultSinks();
            }
            continue;
        }

        if (bufferLength + lineLength + 2 > bufferCapacity) {
            TZrSize newCapacity = bufferCapacity == 0 ? ZR_CLI_REPL_BUFFER_INITIAL_CAPACITY
                                                      : bufferCapacity * ZR_CLI_COLLECTION_GROWTH_FACTOR;
            TZrChar *newBuffer;

            while (newCapacity < bufferLength + lineLength + 2) {
                newCapacity *= 2;
            }

            newBuffer = (TZrChar *) realloc(buffer, newCapacity);
            if (newBuffer == ZR_NULL) {
                free(buffer);
                zr_cli_repl_session_free(&session);
                ZrCore_Log_Error(ZR_NULL, "out of memory\n");
                return 1;
            }

            buffer = newBuffer;
            bufferCapacity = newCapacity;
        }

        memcpy(buffer + bufferLength, line, lineLength);
        bufferLength += lineLength;
        buffer[bufferLength++] = '\n';
        buffer[bufferLength] = '\0';
    }

    ZrCore_Log_FlushDefaultSinks();
    free(buffer);
    zr_cli_repl_session_free(&session);
    return 0;
}
