#include "unity.h"

#include <string.h>

#include "runtime_support.h"
#include "compiler/compile_time/compile_time_executor_internal.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrAstNode *parse_source(const TZrChar *source) {
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "comptime_control_flow_edges.zr");
    SZrParserState parser;
    SZrAstNode *ast;
    TZrBool hadError;

    ZrParser_State_Init(&parser, g_state, source, strlen(source), name);
    parser.suppressErrorOutput = ZR_TRUE;
    ast = ZrParser_ParseWithState(&parser);
    hadError = parser.hasError;
    ZrParser_State_Free(&parser);
    TEST_ASSERT_FALSE_MESSAGE(hadError, "Control-flow edge fixture contains a syntax error");
    TEST_ASSERT_NOT_NULL_MESSAGE(ast, "Control-flow edge fixture must parse as ZR");
    return ast;
}

static void assert_checks_succeed(const TZrChar *source) {
    SZrAstNode *ast = parse_source(source);
    SZrCompilerState compiler;
    TZrBool buildFactsPrepared;
    TZrBool succeeded = ZR_FALSE;
    TZrBool hadError;
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    TZrSize rootsAfterCompilerFree;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    buildFactsPrepared = ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &compiler, ast);
    if (buildFactsPrepared) {
        compiler.compilePhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK;
        succeeded = ZrParser_CompileTime_ExecuteLateChecksInCompilerState(
                &compiler, ast);
    }
    hadError = compiler.hasCompileTimeError;
    ZrParser_CompilerState_Free(&compiler);
    rootsAfterCompilerFree = ZrCore_GcDomain_GetRootCount(g_state);
    ZrParser_Ast_Free(g_state, ast);
    TEST_ASSERT_TRUE_MESSAGE(
            buildFactsPrepared,
            "BuildFacts must succeed before the expected LateCheck success");
    TEST_ASSERT_TRUE_MESSAGE(succeeded, "Compile-time edge assertion failed");
    TEST_ASSERT_FALSE(hadError);
    TEST_ASSERT_EQUAL_UINT64(baseline, rootsAfterCompilerFree);
}

static void assert_checks_fail(
        const TZrChar *source,
        const TZrChar *expectedErrorSubstring) {
    SZrAstNode *ast = parse_source(source);
    SZrCompilerState compiler;
    TZrBool buildFactsPrepared;
    TZrBool succeeded = ZR_FALSE;
    TZrBool hadError;
    TZrBool errorMatches;
    TZrSize baseline;
    TZrSize rootsAfterCompilerFree;

    baseline = ZrCore_GcDomain_GetRootCount(g_state);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    buildFactsPrepared = ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &compiler, ast);
    if (buildFactsPrepared) {
        compiler.compilePhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK;
        succeeded = ZrParser_CompileTime_ExecuteLateChecksInCompilerState(
                &compiler, ast);
    }
    hadError = compiler.hasCompileTimeError;
    errorMatches = compiler.errorMessage != ZR_NULL &&
            expectedErrorSubstring != ZR_NULL &&
            strstr(compiler.errorMessage, expectedErrorSubstring) != ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    rootsAfterCompilerFree = ZrCore_GcDomain_GetRootCount(g_state);
    ZrParser_Ast_Free(g_state, ast);

    TEST_ASSERT_TRUE_MESSAGE(
            buildFactsPrepared,
            "BuildFacts must succeed before the expected LateCheck rejection");
    TEST_ASSERT_FALSE_MESSAGE(
            succeeded,
            "Invalid compile-time control flow was accepted");
    TEST_ASSERT_TRUE_MESSAGE(
            hadError,
            "LateCheck rejection must set hasCompileTimeError");
    TEST_ASSERT_TRUE_MESSAGE(errorMatches, expectedErrorSubstring);
    TEST_ASSERT_EQUAL_UINT64(baseline, rootsAfterCompilerFree);
}

static void test_foreach_object_alias_and_shorthand_bindings(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn sum(): int {\n"
            "    var total = 0;\n"
            "    for (let {target: source} in [{source: 2}, {source: 3}]) {\n"
            "        total += target;\n"
            "    }\n"
            "    for (let {same} in [{same: 4}]) { total += same; }\n"
            "    return total;\n"
            "}\n"
            "comptime { compile.assert(sum() == 9, \"object bindings\"); }\n"
            "return 0;\n");
}

static void test_block_shadow_does_not_mutate_outer_binding(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn shadowValue(): int {\n"
            "    var value = 1;\n"
            "    if (true) { var value = 4; value += 1; }\n"
            "    return value;\n"
            "}\n"
            "comptime { compile.assert(shadowValue() == 1, \"block shadow\"); }\n"
            "return 0;\n");
}

static void test_const_shadow_assignment_is_rejected(void) {
    assert_checks_fail(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn immutableValue(): int {\n"
            "    let fixed = 1;\n"
            "    fixed = 2;\n"
            "    return fixed;\n"
            "}\n"
            "comptime { compile.assert(immutableValue() == 2, \"const\"); }\n"
            "return 0;\n",
            "Cannot assign to immutable compile-time binding");
}

static void test_comptime_break_outside_loop_is_rejected(void) {
    assert_checks_fail(
            "comptime fn breakOutsideLoop(): int {\n"
            "    break;\n"
            "    return 1;\n"
            "}\n"
            "comptime { breakOutsideLoop(); }\n"
            "return 0;\n",
            "Compile-time break is outside a loop");
}

static void test_comptime_continue_outside_loop_is_rejected(void) {
    assert_checks_fail(
            "comptime fn continueOutsideLoop(): int {\n"
            "    continue;\n"
            "    return 1;\n"
            "}\n"
            "comptime { continueOutsideLoop(); }\n"
            "return 0;\n",
            "Compile-time continue is outside a loop");
}

static void test_comptime_foreach_rejects_gc_object_iterable(void) {
    assert_checks_fail(
            "comptime fn foreachObject(): int {\n"
            "    for (let value in { value: 1 }) { return value; }\n"
            "    return 0;\n"
            "}\n"
            "comptime { foreachObject(); }\n"
            "return 0;\n",
            "Compile-time foreach requires a built-in array iterable");
}

static void test_comptime_for_binding_is_out_of_scope_after_loop(void) {
    assert_checks_fail(
            "comptime fn forBindingOutsideLoop(): int {\n"
            "    for (var i = 0; i < 1; i += 1) {}\n"
            "    return i;\n"
            "}\n"
            "comptime { forBindingOutsideLoop(); }\n"
            "return 0;\n",
            "Unknown compile-time identifier: i");
}

static void test_comptime_foreach_binding_is_out_of_scope_after_loop(void) {
    assert_checks_fail(
            "comptime fn foreachBindingOutsideLoop(): int {\n"
            "    for (let value in [1]) {}\n"
            "    return value;\n"
            "}\n"
            "comptime { foreachBindingOutsideLoop(); }\n"
            "return 0;\n",
            "Unknown compile-time identifier: value");
}

static void test_comptime_switch_without_default_continues_after_gc_selector(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn switchWithoutDefault(): int {\n"
            "    switch ({ value: 1 }) {\n"
            "        (0) { return 99; }\n"
            "    }\n"
            "    return 17;\n"
            "}\n"
            "comptime { compile.assert(\n"
            "    switchWithoutDefault() == 17,\n"
            "    \"switch without default\"); }\n"
            "return 0;\n");
}

static SZrAstNode *find_comptime_statement_block(SZrAstNode *ast) {
    SZrScript *script;

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        return ZR_NULL;
    }
    script = &ast->data.script;
    for (TZrSize index = 0U;
         script->statements != ZR_NULL && index < script->statements->count;
         index++) {
        SZrAstNode *statement = script->statements->nodes[index];
        if (statement != ZR_NULL &&
            statement->type == ZR_AST_COMPILE_TIME_DECLARATION &&
            statement->data.compileTimeDeclaration.declarationType ==
                    ZR_COMPILE_TIME_STATEMENT) {
            return statement->data.compileTimeDeclaration.declaration;
        }
    }
    return ZR_NULL;
}

static SZrAstNode *find_named_variable(SZrAstNode *block, const TZrChar *name) {
    if (block == ZR_NULL || block->type != ZR_AST_BLOCK || name == ZR_NULL ||
        block->data.block.body == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < block->data.block.body->count; index++) {
        SZrAstNode *statement = block->data.block.body->nodes[index];
        SZrAstNode *pattern;
        TZrNativeString nativeName;
        if (statement == ZR_NULL ||
            statement->type != ZR_AST_VARIABLE_DECLARATION) {
            continue;
        }
        pattern = statement->data.variableDeclaration.pattern;
        if (pattern == ZR_NULL || pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
            pattern->data.identifier.name == ZR_NULL) {
            continue;
        }
        nativeName = ZrCore_String_GetNativeString(pattern->data.identifier.name);
        if (nativeName != ZR_NULL && strcmp(nativeName, name) == 0) {
            return statement;
        }
    }
    return ZR_NULL;
}

static SZrAstNode *find_top_level_variable(
        SZrAstNode *ast,
        const TZrChar *name) {
    SZrScript *script;

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT || name == ZR_NULL) {
        return ZR_NULL;
    }
    script = &ast->data.script;
    for (TZrSize index = 0U;
         script->statements != ZR_NULL && index < script->statements->count;
         index++) {
        SZrAstNode *statement = script->statements->nodes[index];
        SZrAstNode *pattern;
        TZrNativeString nativeName;
        if (statement == ZR_NULL ||
            statement->type != ZR_AST_VARIABLE_DECLARATION) {
            continue;
        }
        pattern = statement->data.variableDeclaration.pattern;
        if (pattern == ZR_NULL || pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
            pattern->data.identifier.name == ZR_NULL) {
            continue;
        }
        nativeName = ZrCore_String_GetNativeString(pattern->data.identifier.name);
        if (nativeName != ZR_NULL && strcmp(nativeName, name) == 0) {
            return statement;
        }
    }
    return ZR_NULL;
}

static void test_nested_call_loop_value_survives_forced_gc(void) {
    SZrAstNode *ast = parse_source(
            "comptime fn nested(): object {\n"
            "    var current = { value: 1 };\n"
            "    for (var i = 0; i < 5; i += 1) {\n"
            "        current = { value: current.value + i };\n"
            "    }\n"
            "    return current;\n"
            "}\n"
            "comptime { var answer = nested(); }\n"
            "return 0;\n");
    SZrCompilerState compiler;
    SZrAstNode *comptimeBlock;
    SZrAstNode *answerDeclaration;
    SZrCompileTimeFrame frame;
    SZrCompileTimeFlowResult flow = {0};
    SZrTypeValue answer;
    SZrTypeValue key;
    const SZrTypeValue *field;
    TZrSize baseline;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &compiler, ast));
    comptimeBlock = find_comptime_statement_block(ast);
    TEST_ASSERT_NOT_NULL(comptimeBlock);
    answerDeclaration = find_named_variable(comptimeBlock, "answer");
    TEST_ASSERT_NOT_NULL(answerDeclaration);

    baseline = ZrCore_GcDomain_GetRootCount(g_state);
    ct_frame_init(&compiler, &frame, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_FLOW_NORMAL,
            ZrParser_CompileTimeFlow_ExecuteStatement(
                    &compiler,
                    answerDeclaration,
                    &frame,
                    ZR_FALSE,
                    &flow));
    TEST_ASSERT_TRUE(
            ZrCore_GcDomain_GetRootCount(g_state) >= baseline + 2U);

    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_TRUE(ct_frame_get(
            &frame,
            answerDeclaration->data.variableDeclaration.pattern->data.identifier.name,
            &answer));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, answer.type);
    ZrCore_Value_InitAsRawObject(
            g_state,
            &key,
            ZR_CAST_RAW_OBJECT_AS_SUPER(
                    ZrCore_String_CreateFromNative(g_state, "value")));
    key.type = ZR_VALUE_TYPE_STRING;
    field = ZrCore_Object_GetValue(
            g_state, ZR_CAST_OBJECT(g_state, answer.value.object), &key);
    TEST_ASSERT_NOT_NULL(field);
    TEST_ASSERT_EQUAL_INT64(11, field->value.nativeObject.nativeInt64);

    ct_frame_free(&compiler, &frame);
    TEST_ASSERT_EQUAL_UINT64(
            baseline, ZrCore_GcDomain_GetRootCount(g_state));
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_cached_compile_time_value_survives_forced_gc(void) {
    SZrAstNode *ast = parse_source(
            "var cached = { value: 7 };\n"
            "return 0;\n");
    SZrCompilerState compiler;
    SZrAstNode *declaration;
    SZrCompileTimeVariable *variable;
    SZrTypeValue value;
    SZrTypeValue key;
    const SZrTypeValue *field;
    TZrSize baseline;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    declaration = find_top_level_variable(ast, "cached");
    TEST_ASSERT_NOT_NULL(declaration);
    baseline = ZrCore_GcDomain_GetRootCount(g_state);
    TEST_ASSERT_TRUE(register_compile_time_variable_declaration(
            &compiler, declaration, declaration->location));
    variable = find_compile_time_variable_with_context(
            &compiler,
            declaration->data.variableDeclaration.pattern->data.identifier.name,
            declaration->data.variableDeclaration.pattern->data.identifier.syntaxContext);
    TEST_ASSERT_NOT_NULL(variable);
    TEST_ASSERT_TRUE(variable->hasEvaluatedValueRoot);

    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_TRUE(ct_try_get_compile_time_value_with_context(
            &compiler,
            variable->name,
            variable->syntaxContext,
            &value));
    ZrCore_Value_InitAsRawObject(
            g_state,
            &key,
            ZR_CAST_RAW_OBJECT_AS_SUPER(
                    ZrCore_String_CreateFromNative(g_state, "value")));
    key.type = ZR_VALUE_TYPE_STRING;
    field = ZrCore_Object_GetValue(
            g_state, ZR_CAST_OBJECT(g_state, value.value.object), &key);
    TEST_ASSERT_NOT_NULL(field);
    TEST_ASSERT_EQUAL_INT64(7, field->value.nativeObject.nativeInt64);

    ZrParser_CompilerState_Free(&compiler);
    TEST_ASSERT_EQUAL_UINT64(
            baseline, ZrCore_GcDomain_GetRootCount(g_state));
    ZrParser_Ast_Free(g_state, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_foreach_object_alias_and_shorthand_bindings);
    RUN_TEST(test_block_shadow_does_not_mutate_outer_binding);
    RUN_TEST(test_const_shadow_assignment_is_rejected);
    RUN_TEST(test_nested_call_loop_value_survives_forced_gc);
    RUN_TEST(test_cached_compile_time_value_survives_forced_gc);
    RUN_TEST(test_comptime_break_outside_loop_is_rejected);
    RUN_TEST(test_comptime_continue_outside_loop_is_rejected);
    RUN_TEST(test_comptime_foreach_rejects_gc_object_iterable);
    RUN_TEST(test_comptime_for_binding_is_out_of_scope_after_loop);
    RUN_TEST(test_comptime_foreach_binding_is_out_of_scope_after_loop);
    RUN_TEST(test_comptime_switch_without_default_continues_after_gc_selector);
    return UNITY_END();
}
