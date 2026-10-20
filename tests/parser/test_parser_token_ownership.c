#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/token_stream.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrTokenStream *make_input(SZrSyntaxContextTable *table) {
    static const char source[] = "let retainedTokenIdentifier = \"retained decoded value\";";
    SZrTokenStream *stream = ZR_NULL;
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            g_state, table, "retained.zr", 11u, source, sizeof(source) - 1u,
            ZR_NULL, &stream));
    return stream;
}

static void assert_declaration_survives_gc(SZrAstNode *declaration) {
    SZrAstNode *name = declaration->data.variableDeclaration.pattern;
    SZrAstNode *value = declaration->data.variableDeclaration.value;
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_STRING("retainedTokenIdentifier", ZrCore_String_GetNativeString(name->data.identifier.name));
    TEST_ASSERT_EQUAL_STRING("retained decoded value", ZrCore_String_GetNativeString(value->data.stringLiteral.value));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(name->data.identifier.syntaxContext));
}

static void test_script_roots_materialized_strings_after_parser_and_stream_free(void) {
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    SZrTokenStream *stream = make_input(table);
    SZrAstNode *ast = ZrParser_ParseTokenStream(g_state, stream, ZR_NULL);

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
    TEST_ASSERT_GREATER_THAN(baseline, ZrCore_GcDomain_GetRootCount(g_state));
    assert_declaration_survives_gc(ast->data.script.statements->nodes[0]);
    ZrParser_Ast_Free(g_state, ast);
    TEST_ASSERT_EQUAL_UINT(baseline, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_detached_statement_retains_strings_and_context_table(void) {
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    SZrTokenStream *stream = make_input(table);
    SZrParserState parser;
    SZrAstNode *statement;

    ZrParser_State_InitWithTokenStream(&parser, g_state, stream, ZR_NULL);
    statement = ZrParser_ParseTopLevelStatementWithState(&parser);
    TEST_ASSERT_NOT_NULL(statement);
    ZrParser_State_Free(&parser);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
    TEST_ASSERT_GREATER_THAN(baseline, ZrCore_GcDomain_GetRootCount(g_state));
    assert_declaration_survives_gc(statement);
    ZrParser_Ast_Free(g_state, statement);
    TEST_ASSERT_EQUAL_UINT(baseline, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_source_asts_can_be_freed_out_of_order(void) {
    static const char source[] = "let retainedTokenIdentifier = \"retained decoded value\";";
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    SZrAstNode *first = ZrParser_Parse(g_state, source, sizeof(source) - 1u, ZR_NULL);
    SZrAstNode *second = ZrParser_Parse(g_state, source, sizeof(source) - 1u, ZR_NULL);

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    ZrParser_Ast_Free(g_state, first);
    TEST_ASSERT_GREATER_THAN(baseline, ZrCore_GcDomain_GetRootCount(g_state));
    assert_declaration_survives_gc(second->data.script.statements->nodes[0]);
    ZrParser_Ast_Free(g_state, second);
    TEST_ASSERT_EQUAL_UINT(baseline, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_token_parser_charges_shared_expansion_budget(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    SZrTokenStream *stream = make_input(table);
    SZrParserState parser;
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetLimits limits = {0};
    SZrAstNode *ast;
    TZrBool initialized;
    TZrBool parsed;
    TZrBool healthy;

    limits.fuel = ZR_PARSER_COMPTIME_DEFAULT_FUEL;
    limits.heapBytes = ZR_PARSER_COMPTIME_DEFAULT_HEAP_BYTES;
    limits.nodeCount = ZR_PARSER_COMPTIME_DEFAULT_NODE_COUNT;
    limits.aggregateCount = ZR_PARSER_COMPTIME_DEFAULT_AGGREGATE_COUNT;
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    ZrParser_State_InitWithTokenStreamBudget(&parser, g_state, stream, ZR_NULL, &budget);
    initialized = parser.lexer != ZR_NULL && !parser.hasError && !parser.hasFatalError &&
            !parser.lexer->t.hasLexError && !parser.lexer->hasSourceEvidenceError &&
            parser.lexer->t.token == ZR_TK_LET &&
            budget.exceededResource == ZR_PARSER_COMPTIME_BUDGET_NONE;
    ast = ZrParser_ParseWithState(&parser);
    parsed = ast != ZR_NULL;
    healthy = parser.lexer != ZR_NULL && !parser.hasError && !parser.hasFatalError &&
            !parser.lexer->hasSourceEvidenceError &&
            budget.exceededResource == ZR_PARSER_COMPTIME_BUDGET_NONE;
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_State_Free(&parser);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);

    TEST_ASSERT_TRUE_MESSAGE(initialized, "Shared-budget parser must initialize before AST parsing");
    TEST_ASSERT_TRUE_MESSAGE(parsed, "Shared-budget token parser must publish an AST");
    TEST_ASSERT_TRUE_MESSAGE(healthy, "Shared-budget token parser must finish without resource failure");
    TEST_ASSERT_GREATER_THAN(0u, budget.usage.heapBytes);
    TEST_ASSERT_GREATER_THAN(0u, budget.usage.nodeCount);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, budget.usage.fuel);
}

static void test_ast_allocation_budget_failure_releases_collected_subtrees(void) {
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    SZrTokenStream *stream = make_input(table);
    SZrParserState parser;
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetLimits limits = {0};
    SZrAstNode *ast;
    TZrBool initialized;
    TZrBool rejected;
    TZrBool hadError;
    TZrBool fatal;
    TZrBool evidenceHealthy;

    limits.fuel = ZR_PARSER_COMPTIME_DEFAULT_FUEL;
    limits.heapBytes = ZR_PARSER_COMPTIME_DEFAULT_HEAP_BYTES;
    limits.nodeCount = 3u;
    limits.aggregateCount = ZR_PARSER_COMPTIME_DEFAULT_AGGREGATE_COUNT;
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    ZrParser_State_InitWithTokenStreamBudget(&parser, g_state, stream, ZR_NULL, &budget);
    initialized = parser.lexer != ZR_NULL && !parser.hasError && !parser.hasFatalError &&
            !parser.lexer->t.hasLexError && !parser.lexer->hasSourceEvidenceError &&
            parser.lexer->t.token == ZR_TK_LET &&
            budget.exceededResource == ZR_PARSER_COMPTIME_BUDGET_NONE;
    parser.suppressErrorOutput = ZR_TRUE;
    ast = ZrParser_ParseWithState(&parser);
    rejected = ast == ZR_NULL;
    hadError = parser.hasError;
    fatal = parser.hasFatalError;
    evidenceHealthy = parser.lexer != ZR_NULL && !parser.lexer->hasSourceEvidenceError;
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_State_Free(&parser);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);

    TEST_ASSERT_TRUE_MESSAGE(initialized, "AST-node rejection fixture must first initialize successfully");
    TEST_ASSERT_TRUE(rejected);
    TEST_ASSERT_TRUE(hadError);
    TEST_ASSERT_TRUE(fatal);
    TEST_ASSERT_TRUE_MESSAGE(evidenceHealthy, "AST-node rejection must not be a source-evidence failure");
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, budget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(3u, budget.usage.nodeCount);
    TEST_ASSERT_EQUAL_UINT64(3u, budget.exceededLimit);
    TEST_ASSERT_EQUAL_UINT64(4u, budget.requestedUsage);
    TEST_ASSERT_EQUAL_UINT(baseline, ZrCore_GcDomain_GetRootCount(g_state));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_script_roots_materialized_strings_after_parser_and_stream_free);
    RUN_TEST(test_detached_statement_retains_strings_and_context_table);
    RUN_TEST(test_source_asts_can_be_freed_out_of_order);
    RUN_TEST(test_token_parser_charges_shared_expansion_budget);
    RUN_TEST(test_ast_allocation_budget_failure_releases_collected_subtrees);
    return UNITY_END();
}
