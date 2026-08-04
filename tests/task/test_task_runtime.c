#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/syntax_contract.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

typedef struct {
    TZrBool reported;
    char message[256];
} SZrTaskCapturedParserDiagnostic;

static SZrState *create_task_test_state(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return state;
    }

    ZrParser_ToGlobalState_Register(state);
    if (!ZrCore_TaskRuntime_RegisterBuiltins(state->global)) {
        ZrTests_State_Destroy(state);
        return ZR_NULL;
    }

    return state;
}

static void destroy_task_test_state(SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }

    if (state->global != ZR_NULL && state->global->userData != ZR_NULL) {
        ZrLibrary_Project_Free(state, (SZrLibrary_Project *)state->global->userData);
        state->global->userData = ZR_NULL;
    }

    ZrTests_State_Destroy(state);
}

static SZrCompilerState *create_task_test_compiler_state(SZrState *state) {
    SZrCompilerState *cs;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));
    if (cs == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_CompilerState_Init(cs, state);
    return cs;
}

static void destroy_task_test_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    if (cs->topLevelFunction != ZR_NULL && cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(cs->state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }

    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(cs->state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

static void ensure_task_test_root_scope(SZrCompilerState *cs) {
    SZrScope scope;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->scopeStack.length != 0) {
        return;
    }

    memset(&scope, 0, sizeof(scope));
    scope.startVarIndex = cs->localVarCount;
    scope.parentCompiler = cs->currentFunction != ZR_NULL ? cs : ZR_NULL;
    ZrCore_Array_Push(cs->state, &cs->scopeStack, &scope);
}

static void compile_task_top_level_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL) {
        return;
    }

    if (cs->currentFunction == ZR_NULL) {
        cs->currentFunction = ZrCore_Function_New(cs->state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
    }

    cs->isScriptLevel = ZR_TRUE;
    if (cs->scopeStack.length == 0) {
        ensure_task_test_root_scope(cs);
    }

    switch (node->type) {
        case ZR_AST_INTERFACE_DECLARATION:
            ZrParser_Compiler_CompileInterfaceDeclaration(cs, node);
            break;
        case ZR_AST_CLASS_DECLARATION:
            ZrParser_Compiler_CompileClassDeclaration(cs, node);
            break;
        case ZR_AST_STRUCT_DECLARATION:
            ZrParser_Compiler_CompileStructDeclaration(cs, node);
            break;
        default:
            ZrParser_Statement_Compile(cs, node);
            break;
    }
}

static SZrFunction *compile_task_source(SZrState *state, const char *source, const char *name) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrAstNode *parse_task_source_ast(SZrState *state, const char *source, const char *name) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Parse(state, source, strlen(source), sourceName);
}

static void clear_task_parser_diagnostic(SZrTaskCapturedParserDiagnostic *diagnostic) {
    if (diagnostic == ZR_NULL) {
        return;
    }

    memset(diagnostic, 0, sizeof(*diagnostic));
}

static void capture_task_parser_error(TZrPtr userData,
                                      const SZrFileRange *location,
                                      const TZrChar *message,
                                      EZrToken token) {
    SZrTaskCapturedParserDiagnostic *diagnostic = (SZrTaskCapturedParserDiagnostic *)userData;

    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(token);
    if (diagnostic == ZR_NULL || diagnostic->reported) {
        return;
    }

    diagnostic->reported = ZR_TRUE;
    if (message != ZR_NULL) {
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
    }
}

static TZrBool task_source_reports_parser_error(SZrState *state, const char *source, const char *name) {
    SZrString *sourceName;
    SZrParserState parserState;
    SZrAstNode *ast;
    SZrTaskCapturedParserDiagnostic diagnostic;

    if (state == ZR_NULL || source == ZR_NULL || name == ZR_NULL) {
        return ZR_TRUE;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (sourceName == ZR_NULL) {
        return ZR_TRUE;
    }

    clear_task_parser_diagnostic(&diagnostic);
    ZrParser_State_Init(&parserState, state, source, strlen(source), sourceName);
    parserState.errorCallback = capture_task_parser_error;
    parserState.errorUserData = &diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    ast = ZrParser_ParseWithState(&parserState);
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    ZrParser_State_Free(&parserState);
    return diagnostic.reported;
}

static void expect_task_compile_failure_contains(const char *source,
                                                 const char *name,
                                                 const char *expectedMessage) {
    SZrState *compileState;
    SZrState *inspectState;
    SZrFunction *function;
    SZrCompilerState *cs;
    SZrAstNode *ast;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NOT_NULL(expectedMessage);

    compileState = create_task_test_state();
    TEST_ASSERT_NOT_NULL(compileState);
    TEST_ASSERT_FALSE(task_source_reports_parser_error(compileState, source, name));

    function = compile_task_source(compileState, source, name);
    TEST_ASSERT_NULL(function);
    destroy_task_test_state(compileState);

    inspectState = create_task_test_state();
    TEST_ASSERT_NOT_NULL(inspectState);
    TEST_ASSERT_FALSE(task_source_reports_parser_error(inspectState, source, name));

    cs = create_task_test_compiler_state(inspectState);
    TEST_ASSERT_NOT_NULL(cs);

    ast = parse_task_source_ast(inspectState, source, name);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);

    cs->currentAst = ast;
    cs->scriptAst = ast;
    if (compiler_validate_task_effects(cs, ast)) {
        ZrParser_Compiler_PredeclareExternBindings(cs, ast->data.script.statements);
    }
    if (!cs->hasError) {
        ZrParser_Compiler_PredeclareFunctionBindings(cs, ast->data.script.statements);
    }
    if (!cs->hasError) {
        (void)compiler_validate_task_effects(cs, ast);
    }
    for (index = 0; index < ast->data.script.statements->count; index++) {
        if (cs->hasError) {
            break;
        }
        compile_task_top_level_statement(cs, ast->data.script.statements->nodes[index]);
    }

    if (!cs->hasError && cs->hadRecoverableError) {
        cs->hasError = ZR_TRUE;
    }

    TEST_ASSERT_TRUE(cs->hasError);
    TEST_ASSERT_NOT_NULL(cs->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, expectedMessage));

    ZrParser_Ast_Free(inspectState, ast);
    destroy_task_test_compiler_state(cs);
    destroy_task_test_state(inspectState);
}

static void expect_task_effect_failure_contains(const char *source,
                                                const char *name,
                                                const char *expectedMessage) {
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NOT_NULL(expectedMessage);

    state = create_task_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_FALSE(task_source_reports_parser_error(state, source, name));

    cs = create_task_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);

    ast = parse_task_source_ast(state, source, name);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    cs->currentAst = ast;
    cs->scriptAst = ast;
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(cs, ast));
    TEST_ASSERT_FALSE(compiler_validate_task_effects(cs, ast));
    TEST_ASSERT_TRUE(cs->hasError);
    TEST_ASSERT_NOT_NULL(cs->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, expectedMessage));

    ZrParser_Ast_Free(state, ast);
    destroy_task_test_compiler_state(cs);
    destroy_task_test_state(state);
}

static void expect_task_effect_success_after_predeclare(const char *source, const char *name) {
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(name);

    state = create_task_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_FALSE(task_source_reports_parser_error(state, source, name));
    cs = create_task_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    ast = parse_task_source_ast(state, source, name);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    cs->currentAst = ast;
    cs->scriptAst = ast;
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(cs, ast));
    ZrParser_Compiler_PredeclareExternBindings(cs, ast->data.script.statements);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Compiler_PredeclareFunctionBindings(cs, ast->data.script.statements);
    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiler_validate_task_effects(cs, ast));
    TEST_ASSERT_FALSE(cs->hasError);

    ZrParser_Ast_Free(state, ast);
    destroy_task_test_compiler_state(cs);
    destroy_task_test_state(state);
}

static const ZrLibTypeDescriptor *find_type_descriptor(const ZrLibModuleDescriptor *descriptor, const char *typeName) {
    TZrSize index;

    if (descriptor == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        if (typeDescriptor->name != ZR_NULL && strcmp(typeDescriptor->name, typeName) == 0) {
            return typeDescriptor;
        }
    }

    return ZR_NULL;
}

static const ZrLibMethodDescriptor *find_method_descriptor(const ZrLibTypeDescriptor *descriptor,
                                                           const char *methodName) {
    TZrSize index;

    if (descriptor == ZR_NULL || methodName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < descriptor->methodCount; index++) {
        const ZrLibMethodDescriptor *method = &descriptor->methods[index];
        if (method->name != ZR_NULL && strcmp(method->name, methodName) == 0) {
            return method;
        }
    }

    return ZR_NULL;
}

static const ZrLibFunctionDescriptor *find_function_descriptor(const ZrLibModuleDescriptor *descriptor,
                                                                const char *functionName) {
    TZrSize index;

    if (descriptor == ZR_NULL || functionName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < descriptor->functionCount; index++) {
        const ZrLibFunctionDescriptor *function = &descriptor->functions[index];
        if (function->name != ZR_NULL && strcmp(function->name, functionName) == 0) {
            return function;
        }
    }

    return ZR_NULL;
}

static void test_project_config_defaults_enable_local_async_manual_threads_disabled(void) {
    const char *json =
            "{\n"
            "  \"name\": \"task_default_project\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}";
    SZrState *state;
    SZrLibrary_Project *project;

    state = create_task_test_state();
    TEST_ASSERT_NOT_NULL(state);

    project = ZrLibrary_Project_New(state,
                                    (TZrNativeString)json,
                                    (TZrNativeString)"tests/fixtures/projects/hello_world/hello_world.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_FALSE(project->supportMultithread);

    ZrLibrary_Project_Free(state, project);
    ZrTests_State_Destroy(state);
}

static void test_project_config_ignores_legacy_auto_coroutine_flag(void) {
    const char *json =
            "{\n"
            "  \"name\": \"task_mt_project\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"supportMultithread\": true,\n"
            "  \"autoCoroutine\": false\n"
            "}";
    SZrState *state = create_task_test_state();
    SZrLibrary_Project *project;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state,
                                    (TZrNativeString)json,
                                    (TZrNativeString)"tests/fixtures/projects/hello_world/hello_world.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_TRUE(project->supportMultithread);

    ZrLibrary_Project_Free(state, project);
    ZrTests_State_Destroy(state);
}

static void test_zr_task_registers_only_canonical_public_shapes(void) {
    SZrState *state = create_task_test_state();
    const ZrLibModuleDescriptor *taskDescriptor;
    const ZrLibModuleDescriptor *coroutineDescriptor;
    const ZrLibTypeDescriptor *taskType;
    const ZrLibTypeDescriptor *jobType;
    const ZrLibTypeDescriptor *schedulerType;

    TEST_ASSERT_NOT_NULL(state);

    taskDescriptor = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
    coroutineDescriptor = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.coroutine");
    TEST_ASSERT_NOT_NULL(taskDescriptor);
    TEST_ASSERT_NULL(coroutineDescriptor);
    TEST_ASSERT_NULL(find_type_descriptor(taskDescriptor, "IScheduler"));
    TEST_ASSERT_NULL(find_type_descriptor(taskDescriptor, "TaskRunner"));
    taskType = find_type_descriptor(taskDescriptor, "Task");
    jobType = find_type_descriptor(taskDescriptor, "Job");
    schedulerType = find_type_descriptor(taskDescriptor, "Scheduler");
    TEST_ASSERT_NOT_NULL(taskType);
    TEST_ASSERT_NOT_NULL(jobType);
    TEST_ASSERT_NOT_NULL(schedulerType);
    TEST_ASSERT_EQUAL_INT(
            ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, jobType->prototypeType);
    TEST_ASSERT_EQUAL_UINT64(1U, taskType->genericParameterCount);
    TEST_ASSERT_TRUE((taskType->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_HANDLE)) != 0U);
    TEST_ASSERT_NULL(find_type_descriptor(taskDescriptor, "Async"));
    TEST_ASSERT_NOT_NULL(find_method_descriptor(schedulerType, "schedule"));
    TEST_ASSERT_NULL(find_method_descriptor(schedulerType, "start"));
    TEST_ASSERT_NULL(find_method_descriptor(schedulerType, "step"));
    TEST_ASSERT_NULL(find_method_descriptor(schedulerType, "pump"));
    TEST_ASSERT_NULL(find_method_descriptor(schedulerType, "setAutoCoroutine"));
    TEST_ASSERT_NULL(find_method_descriptor(schedulerType, "getAutoCoroutine"));
    TEST_ASSERT_NULL(find_function_descriptor(taskDescriptor, "__createTaskRunner"));
    TEST_ASSERT_NULL(find_function_descriptor(taskDescriptor, "__awaitTask"));

    ZrTests_State_Destroy(state);
}

static void test_legacy_task_source_surfaces_are_rejected(void) {
    static const char *source =
            "async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(task_source_reports_parser_error(state, source, "legacy_percent_async.zr"));
    TEST_ASSERT_TRUE(task_source_reports_parser_error(state,
                                                       "var handle: %async int = null;\n",
                                                       "legacy_percent_async_type.zr"));
    TEST_ASSERT_TRUE(task_source_reports_parser_error(state,
                                                       "%await pending;\n",
                                                       "legacy_percent_await.zr"));
    TEST_ASSERT_NULL(compile_task_source(state,
                                         "var task = import(\"zr.task\");\n"
                                         "var runner: task.TaskRunner<int> = null;\n"
                                         "return 0;\n",
                                         "legacy_task_runner_type.zr"));
    function = compile_task_source(state,
                                   "var task = import(\"zr.task\");\n"
                                   "task.currentScheduler.pump();\n"
                                   "return 0;\n",
                                   "legacy_task_pump.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Function_Execute(state, function, &result));
    function = compile_task_source(state,
                                   "var task = import(\"zr.task\");\n"
                                   "return task.defaultScheduler;\n",
                                   "legacy_default_scheduler.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Function_Execute(state, function, &result));
    TEST_ASSERT_NULL(compile_task_source(state,
                                         "var coroutine = import(\"zr.coroutine\");\n"
                                         "return 0;\n",
                                         "legacy_coroutine_module.zr"));
    ZrTests_State_Destroy(state);
}

static void test_canonical_job_scheduler_path_executes(void) {
    static const char *source =
            "let task = import(\"zr.task\");\n"
            "var job = init task.Job<int>(fn() => { return 17; });\n"
            "var completion = task.currentScheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "canonical_job_scheduler_runtime.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(17, result);
    ZrTests_State_Destroy(state);
}

static void test_async_function_preserves_explicit_task_return_and_direct_await(void) {
    static const char *source =
            "async fn waitFor(value: Task<int>): Task<int> {\n"
            "    return await value;\n"
            "}\n";
    SZrState *state = create_task_test_state();
    SZrAstNode *ast;
    SZrAstNode *functionNode;
    SZrAstNode *returnNode;
    SZrType *returnType;

    TEST_ASSERT_NOT_NULL(state);
    ast = parse_task_source_ast(state, source, "task_explicit_task_direct_await_test.zr");
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1, ast->data.script.statements->count);

    functionNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, functionNode->type);
    TEST_ASSERT_TRUE(functionNode->data.functionDeclaration.isAsync);
    returnType = functionNode->data.functionDeclaration.returnType;
    TEST_ASSERT_NOT_NULL(returnType);
    TEST_ASSERT_NOT_NULL(returnType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, returnType->name->type);
    TEST_ASSERT_NOT_NULL(returnType->name->data.genericType.name);
    TEST_ASSERT_EQUAL_STRING("Task",
                             ZrCore_String_GetNativeString(returnType->name->data.genericType.name->name));

    TEST_ASSERT_NOT_NULL(functionNode->data.functionDeclaration.body);
    TEST_ASSERT_NOT_NULL(functionNode->data.functionDeclaration.body->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(1, functionNode->data.functionDeclaration.body->data.block.body->count);
    returnNode = functionNode->data.functionDeclaration.body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(returnNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, returnNode->type);
    TEST_ASSERT_NOT_NULL(returnNode->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_AWAIT_EXPRESSION, returnNode->data.returnStatement.expr->type);
    TEST_ASSERT_NOT_NULL(returnNode->data.returnStatement.expr->data.awaitExpression.operand);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
                          returnNode->data.returnStatement.expr->data.awaitExpression.operand->type);

    ZrParser_Ast_Free(state, ast);
    ZrTests_State_Destroy(state);
}

static void test_task_effects_follow_active_comptime_branch(void) {
    static const char *activeSource =
            "comptime if (true) {\n"
            "  fn invalid(task: zr.task.Task<int>): int {\n"
            "    return await task;\n"
            "  }\n"
            "}\n";
    static const char *inactiveSource =
            "comptime if (false) {\n"
            "  fn invalid(task: zr.task.Task<int>): int {\n"
            "    return await task;\n"
            "  }\n"
            "}\n";

    expect_task_effect_failure_contains(
            activeSource,
            "task_active_comptime_branch_effects_test.zr",
            "await is only allowed inside async bodies or scheduler-managed top-level coroutines");
    expect_task_effect_success_after_predeclare(
            inactiveSource,
            "task_inactive_comptime_branch_effects_test.zr");
}

static void test_async_task_alias_signature_uses_native_task_carrier(void) {
    expect_task_effect_success_after_predeclare(
            "var task = import(\"zr.task\");\n"
            "async fn waitFor(value: task.Task<int>): task.Task<int> {\n"
            "    return await value;\n"
            "}\n",
            "task_async_alias_signature_test.zr");
}

static void test_async_lambda_preserves_explicit_task_signature(void) {
    static const char *source =
            "var waitFor = async fn(value: zr.task.Task<int>): zr.task.Task<int> => await value;\n";
    SZrState *state = create_task_test_state();
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrAstNode *lambda;

    TEST_ASSERT_NOT_NULL(state);
    ast = parse_task_source_ast(state, source, "task_async_lambda_signature_test.zr");
    TEST_ASSERT_NOT_NULL(ast);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    lambda = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambda->type);
    TEST_ASSERT_TRUE(lambda->data.lambdaExpression.isAsync);
    TEST_ASSERT_NOT_NULL(lambda->data.lambdaExpression.returnType);
    TEST_ASSERT_NOT_NULL(lambda->data.lambdaExpression.block);
    TEST_ASSERT_EQUAL_INT(ZR_AST_AWAIT_EXPRESSION,
                          lambda->data.lambdaExpression.block->data.block.body->nodes[0]
                                  ->data.returnStatement.expr->type);

    ZrParser_Ast_Free(state, ast);
    destroy_task_test_state(state);

    expect_task_effect_success_after_predeclare(source, "task_async_lambda_effect_test.zr");
}

static void test_async_member_requires_explicit_task_signature(void) {
    expect_task_effect_success_after_predeclare(
            "class Worker {\n"
            "    async fn waitFor(value: zr.task.Task<int>): zr.task.Task<int> {\n"
            "        return await value;\n"
            "    }\n"
            "}\n"
            "struct WorkerView {\n"
            "    async fn waitFor(value: zr.task.Task<int>): zr.task.Task<int> {\n"
            "        return await value;\n"
            "    }\n"
            "}\n",
            "task_async_member_signature_test.zr");
    expect_task_compile_failure_contains(
            "class Worker { async fn invalid(): int { return 1; } }\n",
            "task_async_member_requires_task_return_test.zr",
            "async functions must declare a closed zr.task.Task<T> return type");
}

static void test_async_callable_effect_projection_is_canonical(void) {
    static const char *source =
            "async fn named(value: zr.task.Task<int>): zr.task.Task<int> { return await value; }\n"
            "var lambda = async fn(value: zr.task.Task<int>): zr.task.Task<int> => await value;\n"
            "class Worker { async fn member(): zr.task.Task<int> { return await value; } }\n"
            "struct WorkerView { async fn member(): zr.task.Task<int> { return await value; } }\n";
    SZrState *state = create_task_test_state();
    SZrCompilerState *cs;
    SZrAstNode *ast;
    SZrFunctionTypeInfo *namedInfo = ZR_NULL;
    const SZrCanonicalTypeNode *namedCallable;
    SZrAstNode *lambda;
    SZrAstNode *classMember;
    SZrAstNode *structMember;

    TEST_ASSERT_NOT_NULL(state);
    ast = parse_task_source_ast(state, source, "task_async_canonical_effect_test.zr");
    TEST_ASSERT_NOT_NULL(ast);
    cs = create_task_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    cs->currentAst = ast;
    cs->scriptAst = ast;
    ZrParser_Compiler_PredeclareExternBindings(cs, ast->data.script.statements);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Compiler_PredeclareFunctionBindings(cs, ast->data.script.statements);
    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunction(
            cs->typeEnv,
            ast->data.script.statements->nodes[0]->data.functionDeclaration.name->name,
            &namedInfo));
    TEST_ASSERT_NOT_NULL(namedInfo);
    namedCallable = ZrParser_CanonicalType_Find(cs->semanticContext, namedInfo->typeId);
    TEST_ASSERT_NOT_NULL(namedCallable);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, namedCallable->kind);
    TEST_ASSERT_TRUE((namedCallable->data.function.effectFlags &
                      ZR_CANONICAL_CALLABLE_EFFECT_ASYNC) != 0U);

    lambda = ast->data.script.statements->nodes[1]->data.variableDeclaration.value;
    classMember = ast->data.script.statements->nodes[2]->data.classDeclaration.members->nodes[0];
    structMember = ast->data.script.statements->nodes[3]->data.structDeclaration.members->nodes[0];
    TEST_ASSERT_TRUE((ZrParser_SyntaxCallable_EffectFlagsFromDeclaration(lambda) &
                      ZR_CANONICAL_CALLABLE_EFFECT_ASYNC) != 0U);
    TEST_ASSERT_TRUE((ZrParser_SyntaxCallable_EffectFlagsFromDeclaration(classMember) &
                      ZR_CANONICAL_CALLABLE_EFFECT_ASYNC) != 0U);
    TEST_ASSERT_TRUE((ZrParser_SyntaxCallable_EffectFlagsFromDeclaration(structMember) &
                      ZR_CANONICAL_CALLABLE_EFFECT_ASYNC) != 0U);

    ZrParser_Ast_Free(state, ast);
    destroy_task_test_compiler_state(cs);
    destroy_task_test_state(state);
}

static void test_direct_await_requires_async_effect(void) {
    static const char *source =
            "fn invalid(value: Task<int>): Task<int> {\n"
            "    return await value;\n"
            "}\n";

    expect_task_effect_failure_contains(source,
                                        "task_direct_await_outside_async_effect_test.zr",
                                        "await is only allowed inside async bodies");
}

static void test_direct_await_infers_task_payload_type(void) {
    static const char *source =
            "async fn waitFor(value: zr.task.Task<int>): zr.task.Task<int> {\n"
            "    return await value;\n"
            "}\n";
    SZrState *state = create_task_test_state();
    SZrCompilerState *cs;
    SZrAstNode *ast;
    SZrAstNode *functionNode;
    SZrAstNode *returnNode;
    SZrAstNode *parameterNode;
    SZrInferredType taskType;
    SZrInferredType resultType;

    TEST_ASSERT_NOT_NULL(state);
    ast = parse_task_source_ast(state, source, "task_direct_await_payload_type_test.zr");
    TEST_ASSERT_NOT_NULL(ast);
    functionNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_NOT_NULL(functionNode->data.functionDeclaration.params);
    TEST_ASSERT_EQUAL_UINT64(1, functionNode->data.functionDeclaration.params->count);
    parameterNode = functionNode->data.functionDeclaration.params->nodes[0];
    TEST_ASSERT_NOT_NULL(parameterNode);
    returnNode = functionNode->data.functionDeclaration.body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(returnNode);

    cs = create_task_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    cs->currentAst = ast;
    cs->scriptAst = ast;
    ZrParser_InferredType_Init(state, &taskType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(state, &resultType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            cs, parameterNode->data.parameter.typeInfo, &taskType));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            state,
            cs->typeEnv,
            parameterNode->data.parameter.name->name,
            &taskType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            cs, returnNode->data.returnStatement.expr, &resultType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, resultType.baseType);
    TEST_ASSERT_EQUAL_UINT64(0, resultType.elementTypes.length);

    ZrParser_InferredType_Free(state, &resultType);
    ZrParser_InferredType_Free(state, &taskType);
    ZrParser_Ast_Free(state, ast);
    destroy_task_test_compiler_state(cs);
    destroy_task_test_state(state);
}

static void test_direct_await_rejects_non_task_operand(void) {
    static const char *source =
            "async fn waitFor(value: int): zr.task.Task<int> {\n"
            "    return await value;\n"
            "}\n";
    SZrState *state = create_task_test_state();
    SZrCompilerState *cs;
    SZrAstNode *ast;
    SZrAstNode *functionNode;
    SZrAstNode *returnNode;
    SZrAstNode *parameterNode;
    SZrInferredType valueType;
    SZrInferredType resultType;

    TEST_ASSERT_NOT_NULL(state);
    ast = parse_task_source_ast(state, source, "task_direct_await_non_task_type_test.zr");
    TEST_ASSERT_NOT_NULL(ast);
    functionNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(functionNode);
    parameterNode = functionNode->data.functionDeclaration.params->nodes[0];
    returnNode = functionNode->data.functionDeclaration.body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(parameterNode);
    TEST_ASSERT_NOT_NULL(returnNode);

    cs = create_task_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    cs->currentAst = ast;
    cs->scriptAst = ast;
    ZrParser_InferredType_Init(state, &valueType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(state, &resultType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            cs, parameterNode->data.parameter.typeInfo, &valueType));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            state,
            cs->typeEnv,
            parameterNode->data.parameter.name->name,
            &valueType));
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(
            cs, returnNode->data.returnStatement.expr, &resultType));
    TEST_ASSERT_NOT_NULL(cs->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "await expects a zr.task.Task<T>"));

    ZrParser_InferredType_Free(state, &resultType);
    ZrParser_InferredType_Free(state, &valueType);
    ZrParser_Ast_Free(state, ast);
    destroy_task_test_compiler_state(cs);
    destroy_task_test_state(state);
}

static void test_direct_await_rejects_borrow_crossing_suspension(void) {
    expect_task_effect_failure_contains(
            "async fn invalid(task: zr.task.Task<int>): zr.task.Task<int> {\n"
            "    var owner = \"ok\";\n"
            "    var value: ref readonly string = ref owner;\n"
            "    await task;\n"
            "    return value;\n"
            "}\n",
            "task_direct_await_borrow_escape_test.zr",
            "Borrowed binding 'value' cannot be used after an await boundary");
}

static void test_async_signature_requires_closed_task_return_and_value_parameters(void) {
    expect_task_compile_failure_contains(
            "async fn invalid(): int { return 1; }\n",
            "task_async_requires_task_return_test.zr",
            "async functions must declare a closed zr.task.Task<T> return type");
    expect_task_compile_failure_contains(
            "async fn invalid(value: ref int): zr.task.Task<int> { return 1; }\n",
            "task_async_rejects_ref_parameter_test.zr",
            "async functions cannot declare in, ref, or out parameters");
}

static void test_percent_mutex_and_percent_atomic_are_rejected(void) {
    static const char *source =
            "var guarded: %mutex int;\n"
            "var flag: %atomic bool;\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(task_source_reports_parser_error(state, source, "task_mutex_atomic_removed_test.zr"));

    ZrTests_State_Destroy(state);
}

/*
 * Retired TaskRunner/coroutine compatibility coverage.  The canonical runtime
 * deliberately has no public legacy surface, so these historical positive
 * cases must not participate in the task-runtime test binary.
 */
#if 0
static void test_percent_await_is_rejected_outside_async_context(void) {
    static const char *source =
            "async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "fn invalid(): int {\n"
            "    var runner = addOne(1);\n"
            "    var task = runner.start();\n"
            "    return await task;\n"
            "}\n"
            "return invalid();\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "task_await_outside_async_test.zr");
    TEST_ASSERT_NULL(function);

    ZrTests_State_Destroy(state);
}

static void test_percent_await_rejects_task_runner_values(void) {
    static const char *source =
            "async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "async invalid(): int {\n"
            "    var runner = addOne(4);\n"
            "    return await runner;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "task_await_runner_rejected_test.zr");
    TEST_ASSERT_NULL(function);

    ZrTests_State_Destroy(state);
}

static void test_borrowed_value_cannot_cross_await_boundary(void) {
    static const char *source =
            "async fn invalid(value: ref readonly string): string {\n"
            "    async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    await task;\n"
            "    return value;\n"
            "}\n"
            "return 0;\n";
    expect_task_compile_failure_contains(source,
                                         "task_borrow_across_await_test.zr",
                                         "Borrowed binding 'value' cannot be used after an await boundary");
}

static void test_borrowed_value_used_before_await_still_compiles(void) {
    static const char *source =
            "async fn valid(value: ref readonly string): string {\n"
            "    var before = value;\n"
            "    async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    await task;\n"
            "    return \"done\";\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_FALSE(task_source_reports_parser_error(state, source, "task_borrow_before_await_ok_test.zr"));
    function = compile_task_source(state, source, "task_borrow_before_await_ok_test.zr");
    TEST_ASSERT_NOT_NULL(function);

    ZrTests_State_Destroy(state);
}

static void test_local_borrowed_value_cannot_cross_await_boundary(void) {
    static const char *source =
            "class Box {}\n"
            "async invalid(): int {\n"
            "    var owner = own Box();\n"
            "    var shared = owner.share();\n"
            "    var borrowed: ref readonly Box = ref shared;\n"
            "    async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    await task;\n"
            "    return borrowed == null ? 0 : 1;\n"
            "}\n"
            "return 0;\n";
    expect_task_compile_failure_contains(source,
                                         "task_local_borrow_across_await_test.zr",
                                         "Borrowed binding 'borrowed' cannot be used after an await boundary");
}

static void test_loaned_value_cannot_cross_await_boundary(void) {
    static const char *source =
            "class Box {}\n"
            "async invalid(): int {\n"
            "    var owner = own Box();\n"
            "    var loaned = ref owner;\n"
            "    async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    await task;\n"
            "    return loaned == null ? 0 : 1;\n"
            "}\n"
            "return 0;\n";
    expect_task_compile_failure_contains(source,
                                         "task_loan_across_await_test.zr",
                                         "Loaned binding 'loaned' cannot be used after an await boundary");
}

static void test_generic_borrow_parameter_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(value: Borrow<string>): string {\n"
            "    async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    await task;\n"
            "    return value;\n"
            "}\n"
            "return 0;\n";
    expect_task_compile_failure_contains(source,
                                         "task_generic_borrow_parameter_across_await_test.zr",
                                         "Borrowed binding 'value' cannot be used after an await boundary");
}

static void test_generic_loan_parameter_cannot_cross_await_boundary(void) {
    static const char *source =
            "class Box {}\n"
            "async invalid(value: Loan<Box>): int {\n"
            "    async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    await task;\n"
            "    return value == null ? 0 : 1;\n"
            "}\n"
            "return 0;\n";
    expect_task_compile_failure_contains(source,
                                         "task_generic_loan_parameter_across_await_test.zr",
                                         "Loaned binding 'value' cannot be used after an await boundary");
}

static void test_generic_loan_typed_local_cannot_cross_await_boundary(void) {
    static const char *source =
            "class Box {}\n"
            "async invalid(): int {\n"
            "    var value: Loan<Box> = null;\n"
            "    async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    await task;\n"
            "    return value == null ? 0 : 1;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_generic_loan_typed_local_across_await_test.zr",
                                        "Loaned binding 'value' cannot be used after an await boundary");
}

static void test_nested_generic_borrow_typed_local_cannot_cross_await_boundary(void) {
    static const char *source =
            "class Box {}\n"
            "class Holder<T> {}\n"
            "async invalid(): int {\n"
            "    var value: Holder<Borrow<Box>> = null;\n"
            "    async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    await task;\n"
            "    return value == null ? 0 : 1;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_nested_generic_borrow_typed_local_across_await_test.zr",
                                        "Borrowed binding 'value' cannot be used after an await boundary");
}

static void test_nested_generic_loan_typed_local_cannot_cross_await_boundary(void) {
    static const char *source =
            "class Box {}\n"
            "class Holder<T> {}\n"
            "async invalid(): int {\n"
            "    var value: Holder<Loan<Box>> = null;\n"
            "    async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    await task;\n"
            "    return value == null ? 0 : 1;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_nested_generic_loan_typed_local_across_await_test.zr",
                                        "Loaned binding 'value' cannot be used after an await boundary");
}

static void test_using_else_branch_borrow_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(value: Borrow<string>): string {\n"
            "    using (var plugin = import(\"missing.plugin\")) {\n"
            "        return \"ok\";\n"
            "    } else {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        return value;\n"
            "    }\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_using_else_borrow_across_await_test.zr",
                                        "Borrowed binding 'value' cannot be used after an await boundary");
}

static void test_plugin_guard_binding_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        return plugin;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_binding_across_await_test.zr",
                                        "Plugin guard binding 'plugin' cannot be used after an await boundary");
}

static void test_dynamic_module_payload_binding_cannot_cross_await_boundary(void) {
    static const char *source =
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "async invalid(): int {\n"
            "    using (let [m] = import(\"zr.plugins\")) {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        return m;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_dynamic_module_payload_across_await_test.zr",
                                        "Plugin guard binding 'm' cannot be used after an await boundary");
}

static void test_plugin_guard_alias_assignment_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    var alias = 0;\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        alias = plugin;\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        return alias;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_alias_assignment_across_await_test.zr",
                                        "Plugin guard binding 'alias' cannot be used after an await boundary");
}

static void test_plugin_guard_conditional_alias_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(flag: bool): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        var alias = flag ? plugin : null;\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var seen = alias == null ? 0 : 1;\n"
            "        return seen;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_conditional_alias_across_await_test.zr",
                                        "Plugin guard binding 'alias' cannot be used after an await boundary");
}

static void test_plugin_guard_logical_alias_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        var alias = plugin || null;\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var seen = alias == null ? 0 : 1;\n"
            "        return seen;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_logical_alias_across_await_test.zr",
                                        "Plugin guard binding 'alias' cannot be used after an await boundary");
}

static void test_plugin_guard_cast_alias_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        var alias = <object> plugin;\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var seen = alias == null ? 0 : 1;\n"
            "        return seen;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_cast_alias_across_await_test.zr",
                                        "Plugin guard binding 'alias' cannot be used after an await boundary");
}

static void test_plugin_guard_assignment_expression_alias_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        var temp = null;\n"
            "        var alias = (temp = plugin);\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var seen = alias == null ? 0 : 1;\n"
            "        return seen;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_assignment_expression_alias_across_await_test.zr",
                                        "Plugin guard binding 'alias' cannot be used after an await boundary");
}

static void test_plugin_guard_generator_body_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        var gen = {{\n"
            "            async pause(): int { return 1; }\n"
            "            var task = pause().start();\n"
            "            await task;\n"
            "            out plugin;\n"
            "        }};\n"
            "        return 0;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_generator_body_across_await_test.zr",
                                        "Plugin guard binding 'plugin' cannot be used after an await boundary");
}

static void test_plugin_guard_template_interpolation_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var text = `plugin ${plugin}`;\n"
            "        return 0;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_template_interpolation_across_await_test.zr",
                                        "Plugin guard binding 'plugin' cannot be used after an await boundary");
}

static void test_plugin_guard_type_query_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var reflected = typeof(plugin);\n"
            "        return 0;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_type_query_across_await_test.zr",
                                        "Plugin guard binding 'plugin' cannot be used after an await boundary");
}

static void test_plugin_guard_array_alias_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        var alias = [plugin];\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var seen = alias == null ? 0 : 1;\n"
            "        return seen;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_array_alias_across_await_test.zr",
                                        "Plugin guard binding 'alias' cannot be used after an await boundary");
}

static void test_plugin_guard_object_alias_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        var alias = { handle: plugin };\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var seen = alias == null ? 0 : 1;\n"
            "        return seen;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_object_alias_across_await_test.zr",
                                        "Plugin guard binding 'alias' cannot be used after an await boundary");
}

static void test_plugin_guard_construct_argument_cannot_cross_await_boundary(void) {
    static const char *source =
            "class Sink {\n"
            "    pub @constructor(value) { var observed = value; }\n"
            "}\n"
            "async invalid(): int {\n"
            "    using (let plugin = import(\"zr.plugins\")) {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var box = new Sink(plugin);\n"
            "        return 0;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_construct_argument_across_await_test.zr",
                                        "Plugin guard binding 'plugin' cannot be used after an await boundary");
}

static void test_plugin_guard_decorator_argument_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        #plugin.Decorate(plugin)#\n"
            "        fn nested(): int { return 1; }\n"
            "        return 0;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_decorator_argument_across_await_test.zr",
                                        "Plugin guard binding 'plugin' cannot be used after an await boundary");
}

static void test_plugin_guard_if_branch_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(flag: bool): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        if (flag) {\n"
            "            return plugin == null ? 0 : 1;\n"
            "        } else {\n"
            "            return 0;\n"
            "        }\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_if_branch_across_await_test.zr",
                                        "Plugin guard binding 'plugin' cannot be used after an await boundary");
}

static void test_plugin_guard_block_expression_alias_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        var alias = {\n"
            "            var marker = 0;\n"
            "            plugin;\n"
            "        };\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var seen = alias == null ? 0 : 1;\n"
            "        return seen;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_block_expression_alias_across_await_test.zr",
                                        "Plugin guard binding 'alias' cannot be used after an await boundary");
}

static void test_plugin_guard_generic_call_argument_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var seen = sink<plugin + 0>();\n"
            "        return seen;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_generic_call_argument_across_await_test.zr",
                                        "Plugin guard binding 'plugin' cannot be used after an await boundary");
}

static void test_plugin_guard_generic_call_type_argument_cannot_cross_await_boundary(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        var seen = sink<plugin.Vector>();\n"
            "        return seen;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_generic_call_type_argument_across_await_test.zr",
                                        "Plugin guard binding 'plugin' cannot be used after an await boundary");
}

static void test_plugin_guard_nested_function_after_parent_await_cannot_read_binding(void) {
    static const char *source =
            "async invalid(): int {\n"
            "    using (var plugin = import(\"zr.plugins\")) {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        await task;\n"
            "        fn nested(): int {\n"
            "            return plugin == null ? 0 : 1;\n"
            "        }\n"
            "        return nested();\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(source,
                                        "task_plugin_guard_nested_function_after_parent_await_test.zr",
                                        "Plugin guard binding 'plugin' cannot be used after an await boundary");
}

static void test_nested_function_after_parent_await_allows_fresh_local_binding(void) {
    static const char *source =
            "async valid(): int {\n"
            "    async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    await task;\n"
            "    fn nested(): int {\n"
            "        var local = 1;\n"
            "        return local;\n"
            "    }\n"
            "    return nested();\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_success(source,
                               "task_nested_function_after_parent_await_fresh_local_test.zr");
}

static void test_task_effects_reject_await_inside_class_method(void) {
    static const char *source =
            "class Worker {\n"
            "    run(): int {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        return await task;\n"
            "    }\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(
            source,
            "task_class_method_await_outside_async_context_test.zr",
            "%await is only allowed inside %async bodies or scheduler-managed top-level coroutines");
}

static void test_task_effects_reject_await_inside_struct_method(void) {
    static const char *source =
            "struct Worker {\n"
            "    run(): int {\n"
            "        async pause(): int { return 1; }\n"
            "        var task = pause().start();\n"
            "        return await task;\n"
            "    }\n"
            "}\n"
            "return 0;\n";
    expect_task_effect_failure_contains(
            source,
            "task_struct_method_await_outside_async_context_test.zr",
            "%await is only allowed inside %async bodies or scheduler-managed top-level coroutines");
}

static void test_task_runner_start_and_await_execute_on_default_scheduler(void) {
    static const char *source =
            "async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "async run(): int {\n"
            "    var task = addOne(9).start();\n"
            "    return await task;\n"
            "}\n"
            "return await run().start();\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "task_runner_start_default_scheduler_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(10, result);

    ZrTests_State_Destroy(state);
}

static void test_task_runner_start_and_await_execute_with_explicit_async_return_type(void) {
    static const char *source =
            "async addOne(value: int): async int {\n"
            "    return value + 1;\n"
            "}\n"
            "async run(): async int {\n"
            "    var task = addOne(9).start();\n"
            "    return await task;\n"
            "}\n"
            "return await run().start();\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_FALSE(task_source_reports_parser_error(state, source, "task_runner_start_explicit_async_return_test.zr"));
    function = compile_task_source(state, source, "task_runner_start_explicit_async_return_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(10, result);

    ZrTests_State_Destroy(state);
}

static void test_coroutine_scheduler_manual_pump_executes_started_runner(void) {
    static const char *source =
            "let coroutine = import(\"zr.coroutine\");\n"
            "async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "coroutine.coroutineScheduler.setAutoCoroutine(false);\n"
            "var task = coroutine.start(addOne(6));\n"
            "coroutine.coroutineScheduler.pump();\n"
            "return await task;\n";
    SZrState *state = create_task_test_state_with_project_flags(ZR_FALSE, ZR_FALSE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "task_coroutine_manual_pump_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    destroy_task_test_state(state);
}

static void test_default_scheduler_property_is_readable_and_writable(void) {
    static const char *source =
            "let task = import(\"zr.task\");\n"
            "let coroutine = import(\"zr.coroutine\");\n"
            "task.defaultScheduler = coroutine.coroutineScheduler;\n"
            "if (task.defaultScheduler != coroutine.coroutineScheduler) { return 0; }\n"
            "return 1;\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "task_default_scheduler_property_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrTests_State_Destroy(state);
}

#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_project_config_defaults_enable_local_async_manual_threads_disabled);
    RUN_TEST(test_project_config_ignores_legacy_auto_coroutine_flag);
    RUN_TEST(test_zr_task_registers_only_canonical_public_shapes);
    RUN_TEST(test_legacy_task_source_surfaces_are_rejected);
    RUN_TEST(test_canonical_job_scheduler_path_executes);
    RUN_TEST(test_async_function_preserves_explicit_task_return_and_direct_await);
    RUN_TEST(test_task_effects_follow_active_comptime_branch);
    RUN_TEST(test_async_task_alias_signature_uses_native_task_carrier);
    RUN_TEST(test_async_lambda_preserves_explicit_task_signature);
    RUN_TEST(test_async_member_requires_explicit_task_signature);
    RUN_TEST(test_async_callable_effect_projection_is_canonical);
    RUN_TEST(test_direct_await_requires_async_effect);
    RUN_TEST(test_direct_await_infers_task_payload_type);
    RUN_TEST(test_direct_await_rejects_non_task_operand);
    RUN_TEST(test_direct_await_rejects_borrow_crossing_suspension);
    RUN_TEST(test_async_signature_requires_closed_task_return_and_value_parameters);
    RUN_TEST(test_percent_mutex_and_percent_atomic_are_rejected);
    return UNITY_END();
}
