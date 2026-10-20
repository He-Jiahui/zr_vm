#include "unity.h"
#include "harness/runtime_support.h"
#include "parser/parser_internal.h"
#include "zr_vm_parser/token_stream.h"

#include <string.h>

typedef SZrAstNode *(*TZrCollectionParser)(SZrParserState *parser);

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static void init_budget(SZrParserComptimeBudget *budget) {
    SZrParserComptimeBudgetLimits limits;
    memset(&limits, 0, sizeof(limits));
    limits.fuel = ZR_PARSER_COMPTIME_DEFAULT_FUEL;
    limits.callDepth = ZR_PARSER_COMPTIME_DEFAULT_CALL_DEPTH;
    limits.heapBytes = ZR_PARSER_COMPTIME_DEFAULT_HEAP_BYTES;
    limits.aggregateCount = ZR_PARSER_COMPTIME_DEFAULT_AGGREGATE_COUNT;
    limits.generatedDeclarationCount =
            ZR_PARSER_COMPTIME_DEFAULT_GENERATED_DECLARATION_COUNT;
    limits.diagnosticCount = ZR_PARSER_COMPTIME_DEFAULT_DIAGNOSTIC_COUNT;
    limits.tokenCount = ZR_PARSER_COMPTIME_DEFAULT_TOKEN_COUNT;
    limits.nodeCount = ZR_PARSER_COMPTIME_DEFAULT_NODE_COUNT;
    limits.matchingCount = ZR_PARSER_COMPTIME_DEFAULT_MATCHING_COUNT;
    limits.expansionCount = ZR_PARSER_COMPTIME_DEFAULT_EXPANSION_COUNT;
    ZrParser_ComptimeBudget_Init(budget, &limits);
}

static void expect_collection_budget_failure(
        const char *source, TZrCollectionParser parse,
        EZrAstNodeType expectedKind, TZrSize shellCount, TZrBool named) {
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream = ZR_NULL;
    SZrParserState parser;
    SZrParserComptimeBudget budget;
    SZrAstNode *node;
    TZrBool rejected;
    EZrParserComptimeBudgetResource exceeded;

    table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            g_state, table, "collection.zr", sizeof("collection.zr") - 1u,
            source, strlen(source), ZR_NULL, &stream));

    init_budget(&budget);
    ZrParser_State_InitWithTokenStreamBudget(
            &parser, g_state, stream, ZR_NULL, &budget);
    parser.suppressErrorOutput = ZR_TRUE;
    node = parse(&parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(node, "Collection fixture must parse without a limit");
    TEST_ASSERT_FALSE_MESSAGE(parser.hasError, "Collection fixture must parse without an error");
    TEST_ASSERT_EQUAL_INT(expectedKind, node->type);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, parser.lexer->t.token);
    ZrParser_Ast_Free(g_state, node);
    ZrParser_State_Free(&parser);

    init_budget(&budget);
    ZrParser_State_InitWithTokenStreamBudget(
            &parser, g_state, stream, ZR_NULL, &budget);
    parser.suppressErrorOutput = ZR_TRUE;
    if (named) {
        /* Decode the name before setting the shell-only allocation allowance. */
        ZrParser_Lexer_Lookahead(parser.lexer);
    }
    budget.limits.heapBytes = budget.usage.heapBytes +
            (TZrUInt64)shellCount * (TZrUInt64)sizeof(SZrAstNode);
    if (expectedKind == ZR_AST_STRUCT_DECLARATION ||
        expectedKind == ZR_AST_CLASS_DECLARATION) {
        /* parse_leading_decorators already meters its capacity of two. */
        budget.limits.heapBytes += (TZrUInt64)sizeof(SZrAstNodeArray) +
                2u * (TZrUInt64)sizeof(SZrAstNode *);
    }
    node = parse(&parser);
    rejected = node == ZR_NULL;
    exceeded = budget.exceededResource;
    ZrParser_Ast_Free(g_state, node);
    ZrParser_State_Free(&parser);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);

    TEST_ASSERT_TRUE_MESSAGE(rejected, "Collection storage must spend the shared heap budget");
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, exceeded);
}

static void test_empty_array_accounts_for_collection_storage(void) {
    expect_collection_budget_failure("[]", parse_array_literal,
            ZR_AST_ARRAY_LITERAL, 1u, ZR_FALSE);
}

static void test_empty_object_accounts_for_collection_storage(void) {
    expect_collection_budget_failure("{}", parse_object_literal,
            ZR_AST_OBJECT_LITERAL, 1u, ZR_FALSE);
}

static void test_empty_struct_accounts_for_collection_storage(void) {
    expect_collection_budget_failure("struct Probe {}", parse_struct_declaration,
            ZR_AST_STRUCT_DECLARATION, 2u, ZR_TRUE);
}

static void test_empty_class_accounts_for_collection_storage(void) {
    expect_collection_budget_failure("class Probe {}", parse_class_declaration,
            ZR_AST_CLASS_DECLARATION, 2u, ZR_TRUE);
}

static void test_empty_interface_accounts_for_collection_storage(void) {
    expect_collection_budget_failure("interface Probe {}", parse_interface_declaration,
            ZR_AST_INTERFACE_DECLARATION, 2u, ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_array_accounts_for_collection_storage);
    RUN_TEST(test_empty_object_accounts_for_collection_storage);
    RUN_TEST(test_empty_struct_accounts_for_collection_storage);
    RUN_TEST(test_empty_class_accounts_for_collection_storage);
    RUN_TEST(test_empty_interface_accounts_for_collection_storage);
    return UNITY_END();
}
