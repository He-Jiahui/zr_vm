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
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/type_inference.h"

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
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_UNSAFE_EDIT,
            diagnostic->noFixReason);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_interval_logical_unreachable_branch_diagnostic(void) {
    const TZrChar *source =
            "fn choose(seed: u8): int {\n"
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
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_UNSAFE_EDIT,
            diagnostic->noFixReason);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_numeric_overflow_diagnostic(void) {
    const TZrChar *source =
            "fn overflow(): int {\n"
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
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic->noFixReason);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_array_bounds_diagnostic(void) {
    const TZrChar *source =
            "fn pick(): int {\n"
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
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic->noFixReason);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_interval_array_bounds_diagnostic(void) {
    const TZrChar *source =
            "fn pick(index: u8): int {\n"
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
            "fn pick(index: u8): int {\n"
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
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic->noFixReason);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_primitive_integer_array_bounds_warning(void) {
    const TZrChar *source =
            "fn pick(index: int): int {\n"
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
            "fn maybe(index: u8): int {\n"
            "    var values: int[1 .. 3] = [1, 2];\n"
            "    return values[index];\n"
            "}\n"
            "fn definite(): int {\n"
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
            "fn maybe(index: int): int {\n"
            "    var values: int[1 ..] = [1, 2];\n"
            "    return values[index];\n"
            "}\n"
            "fn positive(index: u8): int {\n"
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
            "fn pick(): int {\n"
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
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic->noFixReason);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_script_publishes_branch_join_definite_assignment_diagnostic(void) {
    const TZrChar *source =
            "fn choose(flag: bool): int {\n"
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
            "fn choose(): int {\n"
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
            "fn choose(flag: bool): int {\n"
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
            "fn choose(flag: bool): int {\n"
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
            "fn choose(): int {\n"
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
            "fn choose(): int {\n"
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

static void test_compiler_error_publishes_persistent_semantic_diagnostic_fact(void) {
    const TZrChar *source =
            "fn inspect(value: scoped ref readonly int): int { return 1; }\n"
            "fn use(value: ref readonly int): int { return inspect(value); }\n";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_reference_call_diagnostic_test.zr",
            strlen("compiler_reference_call_diagnostic_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(
            cs.errorMessage, "ref parameter requires the 'ref' argument marker"));
    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    cs.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&cs);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    diagnostic = find_query_diagnostic_by_code(
            cs.semanticContext, "compiler_error");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_NOT_NULL(diagnostic->message);
    TEST_ASSERT_NOT_NULL(strstr(
            ZrCore_String_GetNativeString(diagnostic->message),
            "ref parameter requires the 'ref' argument marker"));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT,
            diagnostic->noFixReason);

    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)count_query_diagnostics_by_code(
                    cs.semanticContext, "compiler_error"));

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compiler_structured_error_publisher_deep_copies_diagnostic(void) {
    SZrCompilerState cs;
    SZrStructuredDiagnostic sourceDiagnostic;
    SZrFileRange location;
    SZrFileRange relatedLocation;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *published;
    const SZrStructuredDiagnosticRelatedInformation *related;
    const SZrStructuredDiagnosticFix *fix;

    memset(&cs, 0, sizeof(cs));
    memset(&location, 0, sizeof(location));
    memset(&relatedLocation, 0, sizeof(relatedLocation));
    location.start.offset = 19U;
    location.end.offset = 27U;
    relatedLocation.start.offset = 4U;
    relatedLocation.end.offset = 11U;

    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_Build(
            g_state,
            &sourceDiagnostic,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "uninitialized_read",
            "Structured compiler diagnostic",
            "A canonical compiler cause",
            "Apply the canonical repair"));
    TEST_ASSERT_TRUE(ZrParser_StructuredDiagnostic_AddRelatedInformation(
            g_state,
            &sourceDiagnostic,
            relatedLocation,
            "Declaration is here"));
    TEST_ASSERT_TRUE(ZrParser_StructuredDiagnostic_AddFix(
            g_state,
            &sourceDiagnostic,
            "Initialize value",
            location,
            "value = 0",
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE));

    ZrParser_Compiler_StructuredError(&cs, &sourceDiagnostic);
    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    ZrParser_Compiler_ClearStructuredError(&cs);
    cs.hasError = ZR_FALSE;

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);
    published = &diagnostics.items[0];
    TEST_ASSERT_EQUAL_UINT32(19U, (TZrUInt32)published->location.start.offset);
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, published->descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "uninitialized_read",
            ZrCore_String_GetNativeString(published->code));
    TEST_ASSERT_EQUAL_STRING(
            "A canonical compiler cause",
            ZrCore_String_GetNativeString(published->cause));
    TEST_ASSERT_EQUAL_STRING(
            "Apply the canonical repair",
            ZrCore_String_GetNativeString(published->suggestion));
    TEST_ASSERT_TRUE(published->relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(
            1U, (TZrUInt32)published->relatedInformation.length);
    TEST_ASSERT_TRUE(published->fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)published->fixes.length);

    related = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&published->relatedInformation, 0U);
    fix = (const SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            (SZrArray *)&published->fixes, 0U);
    TEST_ASSERT_NOT_NULL(related);
    TEST_ASSERT_EQUAL_UINT32(
            4U, (TZrUInt32)related->location.start.offset);
    TEST_ASSERT_EQUAL_STRING(
            "Declaration is here",
            ZrCore_String_GetNativeString(related->message));
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Initialize value",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(
            "value = 0",
            ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);

    ZrParser_CompilerState_Free(&cs);
}

static void test_assignment_compatibility_publishes_detailed_type_mismatch_fact(void) {
    SZrCompilerState cs;
    SZrFileRange actualLocation;
    SZrFileRange expectedLocation;
    SZrInferredType actualType;
    SZrInferredType expectedType;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *published;
    const SZrStructuredDiagnosticRelatedInformation *related;
    const SZrStructuredDiagnosticFix *fix;

    memset(&cs, 0, sizeof(cs));
    memset(&actualLocation, 0, sizeof(actualLocation));
    memset(&expectedLocation, 0, sizeof(expectedLocation));
    actualLocation.start.offset = 22U;
    actualLocation.start.line = 2;
    actualLocation.start.column = 23;
    actualLocation.end.offset = 26U;
    actualLocation.end.line = 2;
    actualLocation.end.column = 27;
    expectedLocation.start.offset = 16U;
    expectedLocation.start.line = 2;
    expectedLocation.start.column = 17;
    expectedLocation.end.offset = 19U;
    expectedLocation.end.line = 2;
    expectedLocation.end.column = 20;

    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    ZrParser_InferredType_Init(g_state, &expectedType, ZR_VALUE_TYPE_INT32);
    ZrParser_InferredType_Init(g_state, &actualType, ZR_VALUE_TYPE_FLOAT);

    TEST_ASSERT_FALSE(ZrParser_AssignmentCompatibility_CheckDetailed(
            &cs,
            &expectedType,
            &actualType,
            actualLocation,
            &expectedLocation));
    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    ZrParser_Compiler_ClearStructuredError(&cs);
    cs.hasError = ZR_FALSE;

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);
    published = find_query_diagnostic_by_code(cs.semanticContext, "type_mismatch");
    TEST_ASSERT_NOT_NULL(published);
    TEST_ASSERT_EQUAL_UINT32(2011U, published->descriptorId);
    TEST_ASSERT_EQUAL_UINT32(22U, (TZrUInt32)published->location.start.offset);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED,
            published->noFixReason);
    TEST_ASSERT_TRUE(published->relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)published->relatedInformation.length);
    TEST_ASSERT_TRUE(published->fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)published->fixes.length);

    related = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&published->relatedInformation,
            0U);
    fix = (const SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            (SZrArray *)&published->fixes,
            0U);
    TEST_ASSERT_NOT_NULL(related);
    TEST_ASSERT_EQUAL_UINT32(
            16U,
            (TZrUInt32)related->location.start.offset);
    TEST_ASSERT_EQUAL_STRING(
            "Expected type is declared here",
            ZrCore_String_GetNativeString(related->message));
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Cast value to 'int'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(
            "<int> <expression>",
            ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_HAS_PLACEHOLDERS,
            fix->applicability);

    ZrParser_InferredType_Free(g_state, &actualType);
    ZrParser_InferredType_Free(g_state, &expectedType);
    ZrParser_CompilerState_Free(&cs);
}

static void test_function_call_compatibility_publishes_detailed_type_mismatch_fact(void) {
    const TZrChar *source =
            "fn pick(value: int): int { return value; }\n"
            "fn main(): int {\n"
            "    pick(2.5);\n"
            "    return 0;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *published;
    const SZrStructuredDiagnosticRelatedInformation *related;
    const SZrStructuredDiagnosticFix *fix;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_function_call_type_mismatch_test.zr",
            strlen("compiler_function_call_type_mismatch_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2011U, cs.structuredError.descriptorId);
    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    cs.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&cs);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);
    published = find_query_diagnostic_by_code(cs.semanticContext, "type_mismatch");
    TEST_ASSERT_NOT_NULL(published);
    TEST_ASSERT_EQUAL_UINT32(2011U, published->descriptorId);
    TEST_ASSERT_EQUAL_INT(3, published->location.start.line);
    TEST_ASSERT_EQUAL_INT(10, published->location.start.column);
    TEST_ASSERT_EQUAL_INT(3, published->location.end.line);
    TEST_ASSERT_EQUAL_INT(13, published->location.end.column);
    TEST_ASSERT_TRUE(published->relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)published->relatedInformation.length);
    TEST_ASSERT_TRUE(published->fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)published->fixes.length);

    related = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&published->relatedInformation,
            0U);
    fix = (const SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            (SZrArray *)&published->fixes,
            0U);
    TEST_ASSERT_NOT_NULL(related);
    TEST_ASSERT_EQUAL_INT(1, related->location.start.line);
    TEST_ASSERT_EQUAL_INT(16, related->location.start.column);
    TEST_ASSERT_EQUAL_INT(1, related->location.end.line);
    TEST_ASSERT_EQUAL_INT(19, related->location.end.column);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "<int> <expression>",
            ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_HAS_PLACEHOLDERS,
            fix->applicability);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void assert_compiler_ownership_diagnostic(
        const TZrChar *source,
        const TZrChar *sourceNameText,
        const TZrChar *locationNeedle,
        TZrSize locationOffset,
        const TZrChar *diagnosticCode,
        TZrUInt32 descriptorId,
        EZrOwnershipQualifier qualifier) {
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange argumentPosition;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *published;
    const SZrSemanticOwnershipFact *ownershipFact;

    sourceName = ZrCore_String_Create(
            g_state, sourceNameText, strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_TRUE(find_position_for_substring(
            source,
            sourceName,
            locationNeedle,
            0U,
            locationOffset,
            &argumentPosition));

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(descriptorId, cs.structuredError.descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            diagnosticCode,
            ZrCore_String_GetNativeString(cs.structuredError.code));
    TEST_ASSERT_EQUAL_UINT64(
            argumentPosition.start.offset,
            cs.structuredError.location.start.offset);

    ownershipFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            cs.semanticContext, argumentPosition);
    TEST_ASSERT_NOT_NULL(ownershipFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_OWNERSHIP_FACT_ERROR, ownershipFact->kind);
    TEST_ASSERT_EQUAL_INT(qualifier, ownershipFact->qualifier);
    TEST_ASSERT_TRUE(ownershipFact->isViolation);

    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    cs.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&cs);
    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);
    published = find_query_diagnostic_by_code(cs.semanticContext, diagnosticCode);
    TEST_ASSERT_NOT_NULL(published);
    TEST_ASSERT_EQUAL_UINT32(descriptorId, published->descriptorId);
    TEST_ASSERT_EQUAL_UINT64(
            argumentPosition.start.offset,
            published->location.start.offset);
    TEST_ASSERT_FALSE(published->fixes.isValid);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            published->noFixReason);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_function_call_compatibility_publishes_ownership_diagnostics(void) {
    const TZrChar *mismatchSource =
            "resource class Resource {}\n"
            "fn consume(resource: Unique<Resource>) {}\n"
            "fn run(resource: Shared<Resource>) {\n"
            "    consume(resource);\n"
            "}\n";
    const TZrChar *weakSource =
            "resource class Resource {}\n"
            "fn observe(resource: ref readonly Resource): int { return 0; }\n"
            "fn run(watcher: Weak<Resource>): int {\n"
            "    observe(ref watcher);\n"
            "    return 0;\n"
            "}\n";

    assert_compiler_ownership_diagnostic(
            mismatchSource,
            "compiler_function_call_ownership_mismatch_test.zr",
            "resource);",
            0U,
            "ownership_mismatch",
            2008U,
            ZR_OWNERSHIP_QUALIFIER_SHARED);
    assert_compiler_ownership_diagnostic(
            weakSource,
            "compiler_function_call_weak_wake_test.zr",
            "watcher);",
            0U,
            "weak_value_requires_wake",
            4004U,
            ZR_OWNERSHIP_QUALIFIER_WEAK);
}

static void test_initializer_compatibility_publishes_ownership_diagnostic(void) {
    const TZrChar *initializerSource =
            "resource class Resource {}\n"
            "fn run(source: Shared<Resource>) {\n"
            "    var owner: Unique<Resource> = source;\n"
            "}\n";

    assert_compiler_ownership_diagnostic(
            initializerSource,
            "compiler_initializer_ownership_mismatch_test.zr",
            "= source;",
            2U,
            "ownership_mismatch",
            2008U,
            ZR_OWNERSHIP_QUALIFIER_SHARED);
}

static void test_return_compatibility_api_publishes_ownership_diagnostic(void) {
    SZrCompilerState cs;
    SZrInferredType expectedType;
    SZrInferredType actualType;
    SZrFileRange actualLocation;
    SZrString *sourceName;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrSemanticOwnershipFact *ownershipFact;
    const SZrStructuredDiagnostic *published;

    memset(&cs, 0, sizeof(cs));
    memset(&actualLocation, 0, sizeof(actualLocation));
    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_return_ownership_mismatch_test.zr",
            strlen("compiler_return_ownership_mismatch_test.zr"));
    actualLocation.source = sourceName;
    actualLocation.start.offset = 91U;
    actualLocation.start.line = 3;
    actualLocation.start.column = 12;
    actualLocation.end.offset = 97U;
    actualLocation.end.line = 3;
    actualLocation.end.column = 18;

    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    ZrParser_InferredType_Init(g_state, &expectedType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &actualType, ZR_VALUE_TYPE_OBJECT);
    expectedType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE;
    actualType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;

    TEST_ASSERT_FALSE(ZrParser_AssignmentCompatibility_CheckDetailed(
            &cs,
            &expectedType,
            &actualType,
            actualLocation,
            ZR_NULL));
    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2008U, cs.structuredError.descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "ownership_mismatch",
            ZrCore_String_GetNativeString(cs.structuredError.code));
    TEST_ASSERT_EQUAL_UINT64(
            actualLocation.start.offset,
            cs.structuredError.location.start.offset);

    ownershipFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            cs.semanticContext, actualLocation);
    TEST_ASSERT_NOT_NULL(ownershipFact);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_OWNERSHIP_FACT_ERROR, ownershipFact->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_QUALIFIER_SHARED, ownershipFact->qualifier);
    TEST_ASSERT_TRUE(ownershipFact->isViolation);

    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    cs.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&cs);
    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);
    published = find_query_diagnostic_by_code(
            cs.semanticContext, "ownership_mismatch");
    TEST_ASSERT_NOT_NULL(published);
    TEST_ASSERT_EQUAL_UINT32(2008U, published->descriptorId);
    TEST_ASSERT_EQUAL_UINT64(
            actualLocation.start.offset,
            published->location.start.offset);
    TEST_ASSERT_FALSE(published->fixes.isValid);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            published->noFixReason);

    ZrParser_InferredType_Free(g_state, &actualType);
    ZrParser_InferredType_Free(g_state, &expectedType);
    ZrParser_CompilerState_Free(&cs);
}

static void test_const_assignment_publishes_structured_semantic_diagnostic(void) {
    const TZrChar *source =
            "let value: int = 1;\n"
            "value = 2;\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *published;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_const_assignment_diagnostics_test.zr",
            strlen("compiler_const_assignment_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2012U, cs.structuredError.descriptorId);
    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    cs.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&cs);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);
    published = find_query_diagnostic_by_code(
            cs.semanticContext, "const_assignment");
    TEST_ASSERT_NOT_NULL(published);
    TEST_ASSERT_EQUAL_UINT32(2012U, published->descriptorId);
    TEST_ASSERT_EQUAL_INT(2, published->location.start.line);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            published->noFixReason);
    TEST_ASSERT_TRUE(published->relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)published->relatedInformation.length);
    TEST_ASSERT_FALSE(published->fixes.isValid);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_const_field_assignment_matches_constructor_context_contract(void) {
    const TZrChar *source =
            "class Meter {\n"
            "    pub const value: int;\n"
            "    pub @constructor(seed: int) {\n"
            "        this.value = seed;\n"
            "    }\n"
            "    pub fn update(next: int) {\n"
            "        this.value = next;\n"
            "    }\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    const SZrStructuredDiagnosticRelatedInformation *related;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_const_field_assignment_context_test.zr",
            strlen("compiler_const_field_assignment_context_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2012U, cs.structuredError.descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "const_assignment",
            ZrCore_String_GetNativeString(cs.structuredError.code));
    TEST_ASSERT_EQUAL_INT(7, cs.structuredError.location.start.line);
    TEST_ASSERT_TRUE(cs.structuredError.relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)cs.structuredError.relatedInformation.length);
    TEST_ASSERT_FALSE(cs.structuredError.fixes.isValid);
    related = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &cs.structuredError.relatedInformation, 0U);
    TEST_ASSERT_NOT_NULL(related);
    TEST_ASSERT_EQUAL_INT(2, related->location.start.line);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_invalid_interface_variance_publishes_structured_semantic_diagnostic(void) {
    const TZrChar *source =
            "interface Producer<out T> {\n"
            "    fn accept(value: T): void;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *published;
    const SZrStructuredDiagnosticRelatedInformation *related;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_invalid_variance_diagnostics_test.zr",
            strlen("compiler_invalid_variance_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2013U, cs.structuredError.descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "invalid_variance",
            ZrCore_String_GetNativeString(cs.structuredError.code));
    TEST_ASSERT_EQUAL_INT(2, cs.structuredError.location.start.line);
    TEST_ASSERT_EQUAL_INT(22, cs.structuredError.location.start.column);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            cs.structuredError.noFixReason);
    TEST_ASSERT_TRUE(cs.structuredError.relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)cs.structuredError.relatedInformation.length);
    TEST_ASSERT_FALSE(cs.structuredError.fixes.isValid);
    related = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &cs.structuredError.relatedInformation, 0U);
    TEST_ASSERT_NOT_NULL(related);
    TEST_ASSERT_EQUAL_INT(1, related->location.start.line);
    TEST_ASSERT_EQUAL_INT(24, related->location.start.column);

    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    cs.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&cs);
    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    published = find_query_diagnostic_by_code(
            cs.semanticContext, "invalid_variance");
    TEST_ASSERT_NOT_NULL(published);
    TEST_ASSERT_EQUAL_UINT32(2013U, published->descriptorId);
    TEST_ASSERT_EQUAL_INT(2, published->location.start.line);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_interface_const_field_mismatch_publishes_structured_semantic_diagnostic(void) {
    const TZrChar *source =
            "interface Versioned {\n"
            "    pub const version: int;\n"
            "}\n"
            "class MutableVersion: Versioned {\n"
            "    pub var version: int;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    const SZrStructuredDiagnosticRelatedInformation *related;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_interface_const_field_diagnostics_test.zr",
            strlen("compiler_interface_const_field_diagnostics_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2014U, cs.structuredError.descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "const_interface_mismatch",
            ZrCore_String_GetNativeString(cs.structuredError.code));
    TEST_ASSERT_EQUAL_STRING(
            "Interface const field 'version' must remain const in implementing class",
            ZrCore_String_GetNativeString(cs.structuredError.message));
    TEST_ASSERT_EQUAL_INT(5, cs.structuredError.location.start.line);
    TEST_ASSERT_EQUAL_INT(13, cs.structuredError.location.start.column);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            cs.structuredError.noFixReason);
    TEST_ASSERT_TRUE(cs.structuredError.relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)cs.structuredError.relatedInformation.length);
    TEST_ASSERT_FALSE(cs.structuredError.fixes.isValid);
    related = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &cs.structuredError.relatedInformation, 0U);
    TEST_ASSERT_NOT_NULL(related);
    TEST_ASSERT_EQUAL_INT(2, related->location.start.line);
    TEST_ASSERT_EQUAL_INT(15, related->location.start.column);
    TEST_ASSERT_EQUAL_INT(2, related->location.end.line);
    TEST_ASSERT_EQUAL_INT(22, related->location.end.column);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_resource_strong_cycle_warning_publishes_user_decision_reason(void) {
    const TZrChar *source =
            "resource class Node {\n"
            "    var next: Shared<Node>;\n"
            "}\n";
    const SZrStructuredDiagnostic *diagnostic;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_resource_strong_cycle_diagnostics_test.zr",
            strlen("compiler_resource_strong_cycle_diagnostics_test.zr"));
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
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    diagnostic = find_query_diagnostic_by_code(
            cs.semanticContext, "resource_shared_strong_cycle");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(
            ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostic->severity);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic->noFixReason);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_duplicate_type_publishes_structured_query_diagnostic(void) {
    const TZrChar *source =
            "class Pair {}\n"
            "class Pair {}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *diagnostic;
    const SZrStructuredDiagnosticRelatedInformation *related;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_duplicate_type_diagnostic_test.zr",
            strlen("compiler_duplicate_type_diagnostic_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);
    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2010U, cs.structuredError.descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "duplicate_type",
            ZrCore_String_GetNativeString(cs.structuredError.code));
    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    cs.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&cs);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);

    diagnostic = find_query_diagnostic_by_code(cs.semanticContext, "duplicate_type");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_UINT32(2010U, diagnostic->descriptorId);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostic->severity);
    TEST_ASSERT_EQUAL_INT(ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
                          diagnostic->noFixReason);
    TEST_ASSERT_FALSE(diagnostic->fixes.isValid);
    TEST_ASSERT_TRUE(diagnostic->relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic->relatedInformation.length);
    related = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&diagnostic->relatedInformation, 0U);
    TEST_ASSERT_NOT_NULL(related);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)related->location.start.line);
    TEST_ASSERT_EQUAL_UINT32(7U, (TZrUInt32)related->location.start.column);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)diagnostic->location.start.line);
    TEST_ASSERT_EQUAL_UINT32(7U, (TZrUInt32)diagnostic->location.start.column);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_untyped_uninitialized_variable_publishes_annotation_diagnostic(void) {
    const TZrChar *source =
            "fn probe() {\n"
            "    var missing;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *diagnostic;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_initializer_requires_annotation_test.zr",
            strlen("compiler_initializer_requires_annotation_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);
    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2017U, cs.structuredError.descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "initializer_requires_annotation",
            ZrCore_String_GetNativeString(cs.structuredError.code));
    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    cs.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&cs);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);

    diagnostic = find_query_diagnostic_by_code(
            cs.semanticContext, "initializer_requires_annotation");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_UINT32(2017U, diagnostic->descriptorId);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostic->severity);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic->noFixReason);
    TEST_ASSERT_FALSE(diagnostic->fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)diagnostic->location.start.line);
    TEST_ASSERT_EQUAL_UINT32(9U, (TZrUInt32)diagnostic->location.start.column);
    TEST_ASSERT_EQUAL_UINT32(16U, (TZrUInt32)diagnostic->location.end.column);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unannotated_incompatible_returns_publish_not_provable_diagnostic(void) {
    const TZrChar *source =
            "fn probe(flag: bool) {\n"
            "    if (flag) {\n"
            "        return 1;\n"
            "    }\n"
            "    return \"text\";\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrStructuredDiagnostic *diagnostic;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_return_type_not_provable_test.zr",
            strlen("compiler_return_type_not_provable_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);
    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_TRUE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(2018U, cs.structuredError.descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "return_type_not_provable",
            ZrCore_String_GetNativeString(cs.structuredError.code));
    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&cs));
    cs.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&cs);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            cs.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            cs.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);

    diagnostic = find_query_diagnostic_by_code(
            cs.semanticContext, "return_type_not_provable");
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_UINT32(2018U, diagnostic->descriptorId);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostic->severity);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic->noFixReason);
    TEST_ASSERT_FALSE(diagnostic->fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic->location.start.line);
    TEST_ASSERT_EQUAL_UINT32(4U, (TZrUInt32)diagnostic->location.start.column);
    TEST_ASSERT_EQUAL_UINT32(9U, (TZrUInt32)diagnostic->location.end.column);

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unannotated_compatible_returns_keep_exact_inference(void) {
    const TZrChar *source =
            "fn probe(flag: bool) {\n"
            "    if (flag) {\n"
            "        return 1;\n"
            "    }\n"
            "    return 2;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_exact_return_type_inference_test.zr",
            strlen("compiler_exact_return_type_inference_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);
    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(
            0U,
            (TZrUInt32)count_query_diagnostics_by_code(
                    cs.semanticContext, "return_type_not_provable"));

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unannotated_unresolved_return_keeps_weak_metadata_without_diagnostic(void) {
    const TZrChar *source = "fn probe(value) { return value; }";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(
            g_state,
            "compiler_weak_return_type_metadata_test.zr",
            strlen("compiler_weak_return_type_metadata_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, ast);
    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_FALSE(cs.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(
            0U,
            (TZrUInt32)count_query_diagnostics_by_code(
                    cs.semanticContext, "return_type_not_provable"));

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_missing_statement_semicolon_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    memset(&location, 0, sizeof(location));
    location.start = ZrParser_FilePosition_Create(24U, 3, 1);
    location.end = ZrParser_FilePosition_Create(25U, 3, 2);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(17U, 2, 10),
            ZrParser_FilePosition_Create(17U, 2, 10),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingStatementSemicolon(
            g_state,
            &diagnostic,
            location,
            fixLocation,
            "break"));
    TEST_ASSERT_EQUAL_UINT64(24U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(25U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing semicolon",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(";", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(17U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(17U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_declaration_body_open_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(9U, 1, 10),
            ZrParser_FilePosition_Create(9U, 1, 10),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(9U, 1, 10),
            ZrParser_FilePosition_Create(9U, 1, 10),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen(
            g_state,
            &diagnostic,
            location,
            fixLocation,
            "class declaration"));
    TEST_ASSERT_EQUAL_UINT64(9U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(9U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing declaration body",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING("{}", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(9U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(9U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_statement_body_open_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(10U, 1, 11),
            ZrParser_FilePosition_Create(10U, 1, 11),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(10U, 1, 11),
            ZrParser_FilePosition_Create(10U, 1, 11),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingStatementBodyOpen(
            g_state,
            &diagnostic,
            location,
            fixLocation,
            "if statement"));
    TEST_ASSERT_EQUAL_UINT64(10U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(10U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing statement body",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING("{}", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(10U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(10U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_block_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(11U, 1, 12),
            ZrParser_FilePosition_Create(12U, 1, 13),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(22U, 1, 23),
            ZrParser_FilePosition_Create(22U, 1, 23),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingBlockClose(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_EQUAL_UINT64(11U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(12U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing '}'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING("}", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(22U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(22U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_catch_pattern_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(30U, 1, 31),
            ZrParser_FilePosition_Create(31U, 1, 32),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(30U, 1, 31),
            ZrParser_FilePosition_Create(30U, 1, 31),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingCatchPatternClose(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_EQUAL_UINT64(30U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(31U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ')'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(")", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(30U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(30U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_using_resource_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(17U, 1, 18),
            ZrParser_FilePosition_Create(18U, 1, 19),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(17U, 1, 18),
            ZrParser_FilePosition_Create(17U, 1, 18),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingUsingResourceClose(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_EQUAL_UINT64(17U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(18U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ')'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(")", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(17U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(17U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_for_header_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(28U, 1, 29),
            ZrParser_FilePosition_Create(29U, 1, 30),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(28U, 1, 29),
            ZrParser_FilePosition_Create(28U, 1, 29),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingForHeaderClose(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ')'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(")", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(28U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(28U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_for_header_separator_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(11U, 1, 12),
            ZrParser_FilePosition_Create(12U, 1, 13),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(11U, 1, 12),
            ZrParser_FilePosition_Create(11U, 1, 12),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingForHeaderSeparator(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ';'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(";", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(11U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(11U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_foreach_header_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(23U, 1, 24),
            ZrParser_FilePosition_Create(24U, 1, 25),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(23U, 1, 24),
            ZrParser_FilePosition_Create(23U, 1, 24),
            ZR_NULL);

    TEST_ASSERT_TRUE(
            ZrParser_DiagnosticBuilder_BuildMissingForeachHeaderClose(
                    g_state,
                    &diagnostic,
                    location,
                    fixLocation));
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ')'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(")", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(23U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(23U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_foreach_in_keyword_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(14U, 1, 15),
            ZrParser_FilePosition_Create(19U, 1, 20),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(14U, 1, 15),
            ZrParser_FilePosition_Create(14U, 1, 15),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingForeachInKeyword(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing 'in'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING("in ", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(14U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(14U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_switch_case_header_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(21U, 1, 22),
            ZrParser_FilePosition_Create(22U, 1, 23),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingSwitchCaseHeaderClose(
            g_state,
            &diagnostic,
            location));
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ')'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(")", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(21U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(21U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_switch_body_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(35U, 1, 36),
            ZrParser_FilePosition_Create(35U, 1, 36),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingSwitchBodyClose(
            g_state,
            &diagnostic,
            location));
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing '}'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING("}", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(35U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(35U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_extern_spec_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(24U, 1, 25),
            ZrParser_FilePosition_Create(25U, 1, 26),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingExternSpecClose(
            g_state,
            &diagnostic,
            location));
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ')'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(")", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(24U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(24U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_declaration_body_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(10U, 1, 11),
            ZrParser_FilePosition_Create(11U, 1, 12),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(28U, 2, 17),
            ZrParser_FilePosition_Create(28U, 2, 17),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyClose(
            g_state,
            &diagnostic,
            location,
            fixLocation,
            "class declaration"));
    TEST_ASSERT_EQUAL_UINT64(10U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(11U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing '}'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING("}", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(28U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(28U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_condition_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(10U, 1, 11),
            ZrParser_FilePosition_Create(11U, 1, 12),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingConditionClose(
            g_state,
            &diagnostic,
            location,
            "if"));
    TEST_ASSERT_EQUAL_UINT64(10U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(11U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ')'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(")", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(10U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(10U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_index_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(12U, 1, 13),
            ZrParser_FilePosition_Create(13U, 1, 14),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(14U, 1, 15),
            ZrParser_FilePosition_Create(15U, 1, 16),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingIndexClose(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_EQUAL_UINT64(12U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(13U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ']'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING("]", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(14U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(14U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_parameter_list_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(20U, 1, 21),
            ZrParser_FilePosition_Create(21U, 1, 22),
            ZR_NULL);

    TEST_ASSERT_TRUE(
            ZrParser_DiagnosticBuilder_BuildMissingParameterListClose(
                    g_state,
                    &diagnostic,
                    location));
    TEST_ASSERT_EQUAL_UINT64(20U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(21U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ')'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(")", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(20U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(20U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_call_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(11U, 2, 12),
            ZrParser_FilePosition_Create(12U, 2, 13),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(17U, 2, 18),
            ZrParser_FilePosition_Create(18U, 2, 19),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingCallClose(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_EQUAL_UINT64(11U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(12U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ')'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(")", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(17U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(17U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_group_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(7U, 1, 8),
            ZrParser_FilePosition_Create(8U, 1, 9),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(13U, 1, 14),
            ZrParser_FilePosition_Create(14U, 1, 15),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingGroupClose(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_EQUAL_UINT64(7U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(8U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ')'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(")", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(13U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(13U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_array_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(7U, 1, 8),
            ZrParser_FilePosition_Create(8U, 1, 9),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(12U, 1, 13),
            ZrParser_FilePosition_Create(13U, 1, 14),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingArrayClose(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_EQUAL_UINT64(7U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(8U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ']'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING("]", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(12U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(12U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_object_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(7U, 1, 8),
            ZrParser_FilePosition_Create(8U, 1, 9),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(12U, 1, 13),
            ZrParser_FilePosition_Create(13U, 1, 14),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingObjectClose(
            g_state,
            &diagnostic,
            location,
            fixLocation));
    TEST_ASSERT_EQUAL_UINT64(7U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(8U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing '}'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING("}", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(12U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(12U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_array_element_separator_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(10U, 1, 11),
            ZrParser_FilePosition_Create(11U, 1, 12),
            ZR_NULL);

    TEST_ASSERT_TRUE(
            ZrParser_DiagnosticBuilder_BuildMissingArrayElementSeparator(
                    g_state,
                    &diagnostic,
                    location));
    TEST_ASSERT_EQUAL_UINT64(10U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(11U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ','",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(",", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(10U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(10U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_array_element_assignment_builder_publishes_no_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange location;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(14U, 1, 15),
            ZrParser_FilePosition_Create(15U, 1, 16),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildArrayElementAssignment(
            g_state,
            &diagnostic,
            location));
    TEST_ASSERT_FALSE(diagnostic.fixes.isValid);
    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_object_computed_key_close_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(8U, 1, 9),
            ZrParser_FilePosition_Create(9U, 1, 10),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(13U, 1, 14),
            ZrParser_FilePosition_Create(14U, 1, 15),
            ZR_NULL);

    TEST_ASSERT_TRUE(
            ZrParser_DiagnosticBuilder_BuildMissingObjectComputedKeyClose(
                    g_state,
                    &diagnostic,
                    location,
                    fixLocation));
    TEST_ASSERT_EQUAL_UINT64(8U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(9U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ']'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING("]", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(13U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(13U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_object_property_colon_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(10U, 1, 11),
            ZrParser_FilePosition_Create(11U, 1, 12),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingObjectPropertyColon(
            g_state,
            &diagnostic,
            location));
    TEST_ASSERT_EQUAL_UINT64(10U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(11U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ':'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(":", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(10U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(10U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_object_property_separator_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(13U, 1, 14),
            ZrParser_FilePosition_Create(14U, 1, 15),
            ZR_NULL);

    TEST_ASSERT_TRUE(
            ZrParser_DiagnosticBuilder_BuildMissingObjectPropertySeparator(
                    g_state,
                    &diagnostic,
                    location));
    TEST_ASSERT_EQUAL_UINT64(13U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(14U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ','",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(",", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(13U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(13U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_conditional_colon_builder_publishes_machine_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrStructuredDiagnosticFix *fix;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(12U, 1, 13),
            ZrParser_FilePosition_Create(13U, 1, 14),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(16U, 1, 17),
            ZrParser_FilePosition_Create(17U, 1, 18),
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingConditionalColon(
            g_state,
            &diagnostic,
            location,
            fixLocation,
            ZR_TRUE));
    TEST_ASSERT_EQUAL_UINT64(12U, diagnostic.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(13U, diagnostic.location.end.offset);
    TEST_ASSERT_TRUE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostic.fixes.length);

    fix = (SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
            &diagnostic.fixes, 0U);
    TEST_ASSERT_NOT_NULL(fix);
    TEST_ASSERT_EQUAL_STRING(
            "Insert missing ':'",
            ZrCore_String_GetNativeString(fix->title));
    TEST_ASSERT_EQUAL_STRING(":", ZrCore_String_GetNativeString(fix->editText));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
    TEST_ASSERT_EQUAL_UINT64(16U, fix->editRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(16U, fix->editRange.end.offset);

    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_missing_conditional_branch_expression_builders_publish_no_fix(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange location;
    SZrFileRange fixLocation;

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(12U, 1, 13),
            ZrParser_FilePosition_Create(13U, 1, 14),
            ZR_NULL);
    fixLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(17U, 1, 18),
            ZrParser_FilePosition_Create(18U, 1, 19),
            ZR_NULL);

    TEST_ASSERT_TRUE(
            ZrParser_DiagnosticBuilder_BuildMissingConditionalConsequent(
                    g_state,
                    &diagnostic,
                    location));
    TEST_ASSERT_FALSE(diagnostic.fixes.isValid);
    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingConditionalAlternate(
            g_state,
            &diagnostic,
            location));
    TEST_ASSERT_FALSE(diagnostic.fixes.isValid);
    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingConditionalColon(
            g_state,
            &diagnostic,
            location,
            fixLocation,
            ZR_FALSE));
    TEST_ASSERT_FALSE(diagnostic.fixes.isValid);
    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
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
    RUN_TEST(test_compiler_error_publishes_persistent_semantic_diagnostic_fact);
    RUN_TEST(test_compiler_structured_error_publisher_deep_copies_diagnostic);
    RUN_TEST(test_assignment_compatibility_publishes_detailed_type_mismatch_fact);
    RUN_TEST(test_function_call_compatibility_publishes_detailed_type_mismatch_fact);
    RUN_TEST(test_function_call_compatibility_publishes_ownership_diagnostics);
    RUN_TEST(test_initializer_compatibility_publishes_ownership_diagnostic);
    RUN_TEST(test_return_compatibility_api_publishes_ownership_diagnostic);
    RUN_TEST(test_const_assignment_publishes_structured_semantic_diagnostic);
    RUN_TEST(test_const_field_assignment_matches_constructor_context_contract);
    RUN_TEST(test_invalid_interface_variance_publishes_structured_semantic_diagnostic);
    RUN_TEST(test_interface_const_field_mismatch_publishes_structured_semantic_diagnostic);
    RUN_TEST(test_resource_strong_cycle_warning_publishes_user_decision_reason);
    RUN_TEST(test_duplicate_type_publishes_structured_query_diagnostic);
    RUN_TEST(test_untyped_uninitialized_variable_publishes_annotation_diagnostic);
    RUN_TEST(test_unannotated_incompatible_returns_publish_not_provable_diagnostic);
    RUN_TEST(test_unannotated_compatible_returns_keep_exact_inference);
    RUN_TEST(test_unannotated_unresolved_return_keeps_weak_metadata_without_diagnostic);
    RUN_TEST(test_missing_statement_semicolon_builder_publishes_machine_fix);
    RUN_TEST(test_missing_declaration_body_open_builder_publishes_machine_fix);
    RUN_TEST(test_missing_statement_body_open_builder_publishes_machine_fix);
    RUN_TEST(test_missing_block_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_catch_pattern_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_using_resource_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_for_header_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_for_header_separator_builder_publishes_machine_fix);
    RUN_TEST(test_missing_foreach_header_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_foreach_in_keyword_builder_publishes_machine_fix);
    RUN_TEST(test_missing_switch_case_header_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_switch_body_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_extern_spec_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_declaration_body_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_condition_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_index_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_parameter_list_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_call_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_group_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_array_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_array_element_separator_builder_publishes_machine_fix);
    RUN_TEST(test_array_element_assignment_builder_publishes_no_fix);
    RUN_TEST(test_missing_object_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_object_computed_key_close_builder_publishes_machine_fix);
    RUN_TEST(test_missing_object_property_colon_builder_publishes_machine_fix);
    RUN_TEST(test_missing_object_property_separator_builder_publishes_machine_fix);
    RUN_TEST(test_missing_conditional_colon_builder_publishes_machine_fix);
    RUN_TEST(test_missing_conditional_branch_expression_builders_publish_no_fix);
    return UNITY_END();
}
