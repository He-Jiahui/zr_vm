#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_query.h"

#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void release_compiler_function(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    if (cs->topLevelFunction != ZR_NULL && cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(g_state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }

    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }
}

static const SZrStructuredDiagnostic *find_query_diagnostic_by_code(SZrSemanticContext *context,
                                                                    const TZrChar *code) {
    if (context == ZR_NULL || code == ZR_NULL || !context->queryDiagnostics.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < context->queryDiagnostics.length; index++) {
        const SZrStructuredDiagnostic *diagnostic =
                (const SZrStructuredDiagnostic *)ZrCore_Array_Get(&context->queryDiagnostics, index);
        if (diagnostic != ZR_NULL &&
            diagnostic->code != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(diagnostic->code), code) == 0) {
            return diagnostic;
        }
    }

    return ZR_NULL;
}

static TZrSize count_query_diagnostics_by_code(SZrSemanticContext *context,
                                               const TZrChar *code) {
    TZrSize count = 0;

    if (context == ZR_NULL || code == ZR_NULL || !context->queryDiagnostics.isValid) {
        return 0;
    }

    for (TZrSize index = 0; index < context->queryDiagnostics.length; index++) {
        const SZrStructuredDiagnostic *diagnostic =
                (const SZrStructuredDiagnostic *)ZrCore_Array_Get(&context->queryDiagnostics, index);
        if (diagnostic != ZR_NULL &&
            diagnostic->code != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(diagnostic->code), code) == 0) {
            count++;
        }
    }

    return count;
}

static TZrBool find_position_for_substring(const TZrChar *content,
                                           SZrString *sourceName,
                                           const TZrChar *needle,
                                           TZrSize occurrence,
                                           TZrSize extraOffset,
                                           SZrFileRange *outRange) {
    const TZrChar *match;
    TZrSize currentOccurrence = 0;
    TZrInt32 line = 1;
    TZrInt32 column = 1;
    const TZrChar *cursor = content;
    TZrSize offset;

    if (content == ZR_NULL || needle == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    while (match != ZR_NULL && currentOccurrence < occurrence) {
        match = strstr(match + 1, needle);
        currentOccurrence++;
    }
    if (match == ZR_NULL) {
        return ZR_FALSE;
    }

    match += extraOffset;
    while (cursor < match) {
        if (*cursor == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
        cursor++;
    }

    offset = (TZrSize)(match - content);
    memset(outRange, 0, sizeof(*outRange));
    outRange->source = sourceName;
    outRange->start.offset = offset;
    outRange->start.line = line;
    outRange->start.column = column;
    outRange->end = outRange->start;
    return ZR_TRUE;
}

static void test_compile_script_publishes_semantic_query_diagnostics_without_error(void) {
    const TZrChar *source = "return true ? 1 : 2;";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(g_state,
                                      "compiler_semantic_query_diagnostics_test.zr",
                                      strlen("compiler_semantic_query_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (TZrUInt32)cs.semanticContext->reachabilityFacts.length);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (TZrUInt32)cs.semanticContext->queryDiagnostics.length);

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "unreachable_code");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostic->severity);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_interval_logical_unreachable_branch_diagnostic(void) {
    const TZrChar *source =
            "choose(seed: u8): int {\n"
            "    return seed < 300 ? 1 : 2;\n"
            "}\n";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_interval_logical_branch_diagnostics_test.zr",
            strlen("compiler_semantic_interval_logical_branch_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (TZrUInt32)cs.semanticContext->queryDiagnostics.length);

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "unreachable_code");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostic->severity);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_numeric_overflow_diagnostic(void) {
    const TZrChar *source =
            "overflow(): int {\n"
            "    return 9223372036854775807 + 1;\n"
            "}\n";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_numeric_overflow_diagnostics_test.zr",
            strlen("compiler_semantic_numeric_overflow_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (TZrUInt32)cs.semanticContext->queryDiagnostics.length);

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "numeric_overflow");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostic->severity);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_array_bounds_diagnostic(void) {
    const TZrChar *source =
            "pick(): int {\n"
            "    var values = [1, 2];\n"
            "    return values[2];\n"
            "}\n";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_array_bounds_diagnostics_test.zr",
            strlen("compiler_semantic_array_bounds_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (TZrUInt32)cs.semanticContext->queryDiagnostics.length);

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "array_index_out_of_bounds");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostic->severity);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_interval_array_bounds_diagnostic(void) {
    const TZrChar *source =
            "pick(index: u8): int {\n"
            "    var values = [1, 2];\n"
            "    var maybe = values[index];\n"
            "    return values[index + 2];\n"
            "}\n";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_interval_array_bounds_diagnostics_test.zr",
            strlen("compiler_semantic_interval_array_bounds_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (TZrUInt32)cs.semanticContext->queryDiagnostics.length);
    TEST_ASSERT_EQUAL_UINT32(
            1,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_out_of_bounds"));

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "array_index_out_of_bounds");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostic->severity);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_possible_interval_array_bounds_warning(void) {
    const TZrChar *source =
            "pick(index: u8): int {\n"
            "    var values = [1, 2];\n"
            "    return values[index];\n"
            "}\n";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_possible_interval_array_bounds_diagnostics_test.zr",
            strlen("compiler_semantic_possible_interval_array_bounds_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_EQUAL_UINT32(
            1,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_may_be_out_of_bounds"));
    TEST_ASSERT_EQUAL_UINT32(
            0,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_out_of_bounds"));

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "array_index_may_be_out_of_bounds");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostic->severity);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_primitive_integer_array_bounds_warning(void) {
    const TZrChar *source =
            "pick(index: int): int {\n"
            "    var values = [1, 2];\n"
            "    return values[index];\n"
            "}\n";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_primitive_integer_array_bounds_diagnostics_test.zr",
            strlen("compiler_semantic_primitive_integer_array_bounds_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_EQUAL_UINT32(
            1,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_may_be_out_of_bounds"));
    TEST_ASSERT_EQUAL_UINT32(
            0,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_out_of_bounds"));

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "array_index_may_be_out_of_bounds");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostic->severity);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_array_min_max_bounds_diagnostics(void) {
    const TZrChar *source =
            "maybe(index: u8): int {\n"
            "    var values: int[1 .. 3] = [1, 2];\n"
            "    return values[index];\n"
            "}\n"
            "definite(): int {\n"
            "    var values: int[1 .. 3] = [1, 2];\n"
            "    return values[3];\n"
            "}\n";
    const SZrStructuredDiagnostic *definiteDiagnostic;
    const SZrStructuredDiagnostic *possibleDiagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_array_min_max_bounds_diagnostics_test.zr",
            strlen("compiler_semantic_array_min_max_bounds_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_EQUAL_UINT32(
            1,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_may_be_out_of_bounds"));
    TEST_ASSERT_EQUAL_UINT32(
            1,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_out_of_bounds"));

    possibleDiagnostic = find_query_diagnostic_by_code(cs.semanticContext, "array_index_may_be_out_of_bounds");
    definiteDiagnostic = find_query_diagnostic_by_code(cs.semanticContext, "array_index_out_of_bounds");
    TEST_ASSERT_NOT_NULL(possibleDiagnostic);
    TEST_ASSERT_NOT_NULL(definiteDiagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, possibleDiagnostic->severity);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, definiteDiagnostic->severity);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_min_only_array_negative_interval_warning(void) {
    const TZrChar *source =
            "maybe(index: int): int {\n"
            "    var values: int[1 ..] = [1, 2];\n"
            "    return values[index];\n"
            "}\n"
            "positive(index: u8): int {\n"
            "    var values: int[1 ..] = [1, 2];\n"
            "    return values[index];\n"
            "}\n";
    const SZrStructuredDiagnostic *diagnostic;
    const TZrChar *message;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_min_only_array_negative_interval_diagnostics_test.zr",
            strlen("compiler_semantic_min_only_array_negative_interval_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_EQUAL_UINT32(
            1,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_may_be_out_of_bounds"));
    TEST_ASSERT_EQUAL_UINT32(
            0,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_out_of_bounds"));

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "array_index_may_be_out_of_bounds");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostic->severity);
    TEST_ASSERT_NOT_NULL(diagnostic->message);
    message = ZrCore_String_GetNativeString(diagnostic->message);
    TEST_ASSERT_NOT_NULL(strstr(message, "may be negative"));
    TEST_ASSERT_NULL(strstr(message, "array max size"));

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_non_integer_array_index_diagnostic(void) {
    const TZrChar *source =
            "pick(): int {\n"
            "    var values = [1, 2];\n"
            "    return values[\"name\"];\n"
            "}\n";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_array_index_type_mismatch_diagnostics_test.zr",
            strlen("compiler_semantic_array_index_type_mismatch_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_EQUAL_UINT32(
            1,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_type_mismatch"));
    TEST_ASSERT_EQUAL_UINT32(
            0,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_out_of_bounds"));
    TEST_ASSERT_EQUAL_UINT32(
            0,
            (TZrUInt32)count_query_diagnostics_by_code(cs.semanticContext, "array_index_may_be_out_of_bounds"));

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "array_index_type_mismatch");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostic->severity);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_branch_join_definite_assignment_diagnostic(void) {
    const TZrChar *source =
            "choose(flag: bool): int {\n"
            "    var seed: int;\n"
            "    if (flag) {\n"
            "        seed = 1;\n"
            "    }\n"
            "    return seed;\n"
            "}\n";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(g_state,
                                      "compiler_semantic_branch_join_diagnostics_test.zr",
                                      strlen("compiler_semantic_branch_join_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (TZrUInt32)cs.semanticContext->queryDiagnostics.length);

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "possibly_uninitialized_read");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostic->severity);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_reaching_definition_for_definition_query(void) {
    const TZrChar *source =
            "choose(): int {\n"
            "    var seed: int = 0;\n"
            "    seed = 3;\n"
            "    return seed;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange writePosition;
    SZrFileRange readPosition;
    const SZrSemanticReferenceFact *definition;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_definition_query_test.zr",
            strlen("compiler_semantic_definition_query_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_TRUE(find_position_for_substring(source,
                                                 sourceName,
                                                 "seed = 3",
                                                 0,
                                                 0,
                                                 &writePosition));
    TEST_ASSERT_TRUE(find_position_for_substring(source,
                                                 sourceName,
                                                 "return seed",
                                                 0,
                                                 strlen("return "),
                                                 &readPosition));
    writePosition.source = ZR_NULL;
    readPosition.source = ZR_NULL;

    definition = ZrParser_SemanticQuery_DefinitionOf(cs.semanticContext, readPosition, ZR_NULL);
    TEST_ASSERT_NOT_NULL(definition);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_WRITE, definition->kind);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)writePosition.start.offset,
                             (TZrUInt32)definition->range.start.offset);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_cfg_reaching_definitions_rejects_divergent_branch_writes(void) {
    const TZrChar *source =
            "choose(flag: bool): int {\n"
            "    var seed: int;\n"
            "    if (flag) {\n"
            "        seed = 1;\n"
            "    } else {\n"
            "        seed = 2;\n"
            "    }\n"
            "    return seed;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange readPosition;
    SZrFileRange elseWritePosition;
    const SZrSemanticReferenceFact *readReference;
    const SZrSemanticReferenceFact *elseWriteReference;

    sourceName = ZrCore_String_Create(g_state,
                                      "compiler_semantic_reaching_definition_join_test.zr",
                                      strlen("compiler_semantic_reaching_definition_join_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_TRUE(find_position_for_substring(source,
                                                 sourceName,
                                                 "return seed",
                                                 0,
                                                 strlen("return "),
                                                 &readPosition));
    readPosition.source = ZR_NULL;
    TEST_ASSERT_TRUE(find_position_for_substring(source,
                                                 sourceName,
                                                 "seed = 2",
                                                 0,
                                                 0,
                                                 &elseWritePosition));
    elseWritePosition.source = ZR_NULL;

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(cs.semanticContext));
    readReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            readPosition,
            ZR_SEMANTIC_REFERENCE_READ);
    elseWriteReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            elseWritePosition,
            ZR_SEMANTIC_REFERENCE_WRITE);
    TEST_ASSERT_NOT_NULL(readReference);
    TEST_ASSERT_NOT_NULL(elseWriteReference);
    TEST_ASSERT_TRUE(readReference->hasDefinitionRange);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)elseWriteReference->range.start.offset,
                             (TZrUInt32)readReference->definitionRange.start.offset);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(cs.semanticContext, ast));

    readReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            readPosition,
            ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(readReference);
    TEST_ASSERT_FALSE(readReference->hasDefinitionRange);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_cfg_reaching_definitions_rejects_loop_carried_write(void) {
    const TZrChar *source =
            "choose(flag: bool): int {\n"
            "    var seed: int = 0;\n"
            "    while (flag) {\n"
            "        seed;\n"
            "        seed = 1;\n"
            "    }\n"
            "    return seed;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange bodyReadPosition;
    SZrFileRange loopWritePosition;
    SZrFileRange finalReadPosition;
    const SZrSemanticReferenceFact *bodyReadReference;
    const SZrSemanticReferenceFact *finalReadReference;
    const SZrSemanticReferenceFact *loopWriteReference;

    sourceName = ZrCore_String_Create(g_state,
                                      "compiler_semantic_reaching_definition_loop_test.zr",
                                      strlen("compiler_semantic_reaching_definition_loop_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_TRUE(find_position_for_substring(source,
                                                 sourceName,
                                                 "        seed;",
                                                 0,
                                                 strlen("        "),
                                                 &bodyReadPosition));
    TEST_ASSERT_TRUE(find_position_for_substring(source,
                                                 sourceName,
                                                 "seed = 1",
                                                 0,
                                                 0,
                                                 &loopWritePosition));
    TEST_ASSERT_TRUE(find_position_for_substring(source,
                                                 sourceName,
                                                 "return seed",
                                                 0,
                                                 strlen("return "),
                                                 &finalReadPosition));
    bodyReadPosition.source = ZR_NULL;
    loopWritePosition.source = ZR_NULL;
    finalReadPosition.source = ZR_NULL;

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(cs.semanticContext));
    finalReadReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            finalReadPosition,
            ZR_SEMANTIC_REFERENCE_READ);
    loopWriteReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            loopWritePosition,
            ZR_SEMANTIC_REFERENCE_WRITE);
    TEST_ASSERT_NOT_NULL(finalReadReference);
    TEST_ASSERT_NOT_NULL(loopWriteReference);
    TEST_ASSERT_TRUE(finalReadReference->hasDefinitionRange);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)loopWriteReference->range.start.offset,
                             (TZrUInt32)finalReadReference->definitionRange.start.offset);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(cs.semanticContext, ast));

    bodyReadReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            bodyReadPosition,
            ZR_SEMANTIC_REFERENCE_READ);
    finalReadReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            finalReadPosition,
            ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(bodyReadReference);
    TEST_ASSERT_NOT_NULL(finalReadReference);
    TEST_ASSERT_FALSE(bodyReadReference->hasDefinitionRange);
    TEST_ASSERT_FALSE(finalReadReference->hasDefinitionRange);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_cfg_reaching_definitions_preserves_true_loop_break_write(void) {
    const TZrChar *source =
            "choose(): int {\n"
            "    var seed: int = 0;\n"
            "    while (true) {\n"
            "        seed = 1;\n"
            "        break;\n"
            "    }\n"
            "    return seed;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange loopWritePosition;
    SZrFileRange finalReadPosition;
    const SZrSemanticReferenceFact *finalReadReference;
    const SZrSemanticReferenceFact *loopWriteReference;

    sourceName = ZrCore_String_Create(g_state,
                                      "compiler_semantic_reaching_definition_true_loop_test.zr",
                                      strlen("compiler_semantic_reaching_definition_true_loop_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_TRUE(find_position_for_substring(source,
                                                 sourceName,
                                                 "seed = 1",
                                                 0,
                                                 0,
                                                 &loopWritePosition));
    TEST_ASSERT_TRUE(find_position_for_substring(source,
                                                 sourceName,
                                                 "return seed",
                                                 0,
                                                 strlen("return "),
                                                 &finalReadPosition));
    loopWritePosition.source = ZR_NULL;
    finalReadPosition.source = ZR_NULL;

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(cs.semanticContext));
    finalReadReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            finalReadPosition,
            ZR_SEMANTIC_REFERENCE_READ);
    loopWriteReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            loopWritePosition,
            ZR_SEMANTIC_REFERENCE_WRITE);
    TEST_ASSERT_NOT_NULL(finalReadReference);
    TEST_ASSERT_NOT_NULL(loopWriteReference);
    TEST_ASSERT_TRUE(finalReadReference->hasDefinitionRange);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)loopWriteReference->range.start.offset,
                             (TZrUInt32)finalReadReference->definitionRange.start.offset);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(cs.semanticContext, ast));

    finalReadReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            finalReadPosition,
            ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(finalReadReference);
    TEST_ASSERT_TRUE(finalReadReference->hasDefinitionRange);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)loopWriteReference->range.start.offset,
                             (TZrUInt32)finalReadReference->definitionRange.start.offset);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_suppresses_true_loop_break_definite_assignment_diagnostic(void) {
    const TZrChar *source =
            "choose(): int {\n"
            "    var seed: int;\n"
            "    while (true) {\n"
            "        seed = 1;\n"
            "        break;\n"
            "    }\n"
            "    return seed;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_semantic_true_loop_definite_assignment_test.zr",
            strlen("compiler_semantic_true_loop_definite_assignment_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_NULL(find_query_diagnostic_by_code(cs.semanticContext, "uninitialized_read"));
    TEST_ASSERT_NULL(find_query_diagnostic_by_code(cs.semanticContext, "possibly_uninitialized_read"));

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_compile_script_publishes_semantic_query_diagnostics_without_error);
    RUN_TEST(test_compile_script_publishes_interval_logical_unreachable_branch_diagnostic);
    RUN_TEST(test_compile_script_publishes_numeric_overflow_diagnostic);
    RUN_TEST(test_compile_script_publishes_array_bounds_diagnostic);
    RUN_TEST(test_compile_script_publishes_interval_array_bounds_diagnostic);
    RUN_TEST(test_compile_script_publishes_possible_interval_array_bounds_warning);
    RUN_TEST(test_compile_script_publishes_primitive_integer_array_bounds_warning);
    RUN_TEST(test_compile_script_publishes_array_min_max_bounds_diagnostics);
    RUN_TEST(test_compile_script_publishes_min_only_array_negative_interval_warning);
    RUN_TEST(test_compile_script_publishes_non_integer_array_index_diagnostic);
    RUN_TEST(test_compile_script_publishes_branch_join_definite_assignment_diagnostic);
    RUN_TEST(test_compile_script_publishes_reaching_definition_for_definition_query);
    RUN_TEST(test_compile_script_cfg_reaching_definitions_rejects_divergent_branch_writes);
    RUN_TEST(test_compile_script_cfg_reaching_definitions_rejects_loop_carried_write);
    RUN_TEST(test_compile_script_cfg_reaching_definitions_preserves_true_loop_break_write);
    RUN_TEST(test_compile_script_suppresses_true_loop_break_definite_assignment_diagnostic);
    return UNITY_END();
}
