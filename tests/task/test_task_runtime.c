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
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
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

static SZrState *create_task_test_state_with_project_flags(TZrBool supportMultithread, TZrBool autoCoroutine) {
    static const char *kProjectTemplate =
            "{\n"
            "  \"name\": \"task_runtime_project\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"supportMultithread\": %s,\n"
            "  \"autoCoroutine\": %s\n"
            "}";
    char json[256];
    SZrState *state = create_task_test_state();
    SZrLibrary_Project *project;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return state;
    }

    snprintf(json,
             sizeof(json),
             kProjectTemplate,
             supportMultithread ? "true" : "false",
             autoCoroutine ? "true" : "false");
    project = ZrLibrary_Project_New(state,
                                    (TZrNativeString)json,
                                    (TZrNativeString)"tests/fixtures/projects/hello_world/hello_world.zrp");
    if (project == ZR_NULL) {
        ZrTests_State_Destroy(state);
        return ZR_NULL;
    }

    state->global->userData = project;
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
    TEST_ASSERT_TRUE(project->autoCoroutine);

    ZrLibrary_Project_Free(state, project);
    ZrTests_State_Destroy(state);
}

static void test_project_config_reads_supportMultithread_and_autoCoroutine_flags(void) {
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
    TEST_ASSERT_FALSE(project->autoCoroutine);

    ZrLibrary_Project_Free(state, project);
    ZrTests_State_Destroy(state);
}

static void test_zr_task_and_zr_coroutine_register_new_public_shapes(void) {
    SZrState *state = create_task_test_state();
    const ZrLibModuleDescriptor *taskDescriptor;
    const ZrLibModuleDescriptor *coroutineDescriptor;

    TEST_ASSERT_NOT_NULL(state);

    taskDescriptor = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
    coroutineDescriptor = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.coroutine");
    TEST_ASSERT_NOT_NULL(taskDescriptor);
    TEST_ASSERT_NOT_NULL(coroutineDescriptor);
    TEST_ASSERT_NOT_NULL(find_type_descriptor(taskDescriptor, "IScheduler"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(taskDescriptor, "TaskRunner"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(taskDescriptor, "Task"));
    TEST_ASSERT_NULL(find_type_descriptor(taskDescriptor, "Async"));
    TEST_ASSERT_NULL(find_type_descriptor(taskDescriptor, "Scheduler"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(coroutineDescriptor, "Scheduler"));

    ZrTests_State_Destroy(state);
}

static void test_percent_async_wraps_declared_return_type_to_task_runner(void) {
    static const char *source =
            "%async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();
    SZrAstNode *ast;
    SZrAstNode *statement;
    SZrType *returnType;

    TEST_ASSERT_NOT_NULL(state);
    ast = parse_task_source_ast(state, source, "task_async_return_wrap_test.zr");
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 1);

    statement = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, statement->type);
    returnType = statement->data.functionDeclaration.returnType;
    TEST_ASSERT_NOT_NULL(returnType);
    TEST_ASSERT_NOT_NULL(returnType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, returnType->name->type);
    TEST_ASSERT_NOT_NULL(returnType->name->data.genericType.name);
    TEST_ASSERT_EQUAL_STRING("zr.task.TaskRunner",
                             ZrCore_String_GetNativeString(returnType->name->data.genericType.name->name));
    TEST_ASSERT_NOT_NULL(returnType->name->data.genericType.params);
    TEST_ASSERT_EQUAL_UINT64(1, returnType->name->data.genericType.params->count);

    ZrParser_Ast_Free(state, ast);
    ZrTests_State_Destroy(state);
}

static void test_percent_async_explicit_return_type_sugar_wraps_to_task_runner(void) {
    static const char *source =
            "%async addOne(value: int): %async int {\n"
            "    return value + 1;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();
    SZrAstNode *ast;
    SZrAstNode *statement;
    SZrType *returnType;
    SZrAstNode *innerTypeNode;

    TEST_ASSERT_NOT_NULL(state);
    ast = parse_task_source_ast(state, source, "task_async_explicit_return_wrap_test.zr");
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 1);

    statement = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, statement->type);
    returnType = statement->data.functionDeclaration.returnType;
    TEST_ASSERT_NOT_NULL(returnType);
    TEST_ASSERT_NOT_NULL(returnType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, returnType->name->type);
    TEST_ASSERT_NOT_NULL(returnType->name->data.genericType.name);
    TEST_ASSERT_EQUAL_STRING("zr.task.TaskRunner",
                             ZrCore_String_GetNativeString(returnType->name->data.genericType.name->name));
    TEST_ASSERT_NOT_NULL(returnType->name->data.genericType.params);
    TEST_ASSERT_EQUAL_UINT64(1, returnType->name->data.genericType.params->count);

    innerTypeNode = returnType->name->data.genericType.params->nodes[0];
    TEST_ASSERT_NOT_NULL(innerTypeNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, innerTypeNode->type);
    TEST_ASSERT_NOT_NULL(innerTypeNode->data.type.name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, innerTypeNode->data.type.name->type);
    TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(innerTypeNode->data.type.name->data.identifier.name));

    ZrParser_Ast_Free(state, ast);
    ZrTests_State_Destroy(state);
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

static void test_percent_await_is_rejected_outside_async_context(void) {
    static const char *source =
            "%async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "func invalid(): int {\n"
            "    var runner = addOne(1);\n"
            "    var task = runner.start();\n"
            "    return %await task;\n"
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
            "%async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "%async invalid(): int {\n"
            "    var runner = addOne(4);\n"
            "    return %await runner;\n"
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
            "%async invalid(value: %borrowed string): string {\n"
            "    %async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    %await task;\n"
            "    return value;\n"
            "}\n"
            "return 0;\n";
    expect_task_compile_failure_contains(source,
                                         "task_borrow_across_await_test.zr",
                                         "Borrowed binding 'value' cannot be used after an await boundary");
}

static void test_borrowed_value_used_before_await_still_compiles(void) {
    static const char *source =
            "%async valid(value: %borrowed string): string {\n"
            "    var before = value;\n"
            "    %async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    %await task;\n"
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
            "%async invalid(): int {\n"
            "    var owner = %unique new Box();\n"
            "    var shared = %shared(owner);\n"
            "    var borrowed = %borrow(shared);\n"
            "    %async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    %await task;\n"
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
            "%async invalid(): int {\n"
            "    var owner = %unique new Box();\n"
            "    var loaned = %loan(owner);\n"
            "    %async pause(): int { return 1; }\n"
            "    var task = pause().start();\n"
            "    %await task;\n"
            "    return loaned == null ? 0 : 1;\n"
            "}\n"
            "return 0;\n";
    expect_task_compile_failure_contains(source,
                                         "task_loan_across_await_test.zr",
                                         "Loaned binding 'loaned' cannot be used after an await boundary");
}

static void test_task_runner_start_and_await_execute_on_default_scheduler(void) {
    static const char *source =
            "%async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "%async run(): int {\n"
            "    var task = addOne(9).start();\n"
            "    return %await task;\n"
            "}\n"
            "return %await run().start();\n";
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
            "%async addOne(value: int): %async int {\n"
            "    return value + 1;\n"
            "}\n"
            "%async run(): %async int {\n"
            "    var task = addOne(9).start();\n"
            "    return %await task;\n"
            "}\n"
            "return %await run().start();\n";
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
            "var coroutine = %import(\"zr.coroutine\");\n"
            "%async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "coroutine.coroutineScheduler.setAutoCoroutine(false);\n"
            "var task = coroutine.start(addOne(6));\n"
            "coroutine.coroutineScheduler.pump();\n"
            "return %await task;\n";
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
            "var task = %import(\"zr.task\");\n"
            "var coroutine = %import(\"zr.coroutine\");\n"
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_project_config_defaults_enable_local_async_manual_threads_disabled);
    RUN_TEST(test_project_config_reads_supportMultithread_and_autoCoroutine_flags);
    RUN_TEST(test_zr_task_and_zr_coroutine_register_new_public_shapes);
    RUN_TEST(test_percent_async_wraps_declared_return_type_to_task_runner);
    RUN_TEST(test_percent_async_explicit_return_type_sugar_wraps_to_task_runner);
    RUN_TEST(test_percent_mutex_and_percent_atomic_are_rejected);
    RUN_TEST(test_percent_await_is_rejected_outside_async_context);
    RUN_TEST(test_percent_await_rejects_task_runner_values);
    RUN_TEST(test_borrowed_value_cannot_cross_await_boundary);
    RUN_TEST(test_borrowed_value_used_before_await_still_compiles);
    RUN_TEST(test_local_borrowed_value_cannot_cross_await_boundary);
    RUN_TEST(test_loaned_value_cannot_cross_await_boundary);
    RUN_TEST(test_task_runner_start_and_await_execute_on_default_scheduler);
    RUN_TEST(test_task_runner_start_and_await_execute_with_explicit_async_return_type);
    RUN_TEST(test_coroutine_scheduler_manual_pump_executes_started_runner);
    RUN_TEST(test_default_scheduler_property_is_readable_and_writable);
    return UNITY_END();
}
