#include "unity.h"

#include <stdint.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/parser.h"
#include "parser/parser_internal.h"

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

static void test_growth_failure_keeps_existing_statements(void) {
    static const TZrChar source[] =
            "let first = 1; let second = 2;";
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetLimits limits = {0};
    SZrAstNode *script;
    SZrAstNodeArray *array;
    TZrUInt64 heapBeforeGrowth;
    TZrSize rootsBefore;

    rootsBefore = ZrCore_GcDomain_GetRootCount(g_state);
    script = ZrParser_Parse(g_state, source, sizeof(source) - 1u, ZR_NULL);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_UINT(2u, script->data.script.statements->count);

    limits.heapBytes = UINT64_MAX;
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    array = ZrParser_AstNodeArray_NewBudget(g_state, 1u, &budget);
    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, budget.usage.heapBytes);

    TEST_ASSERT_TRUE(ZrParser_AstNodeArray_TryAddBudget(
            g_state,
            array,
            script->data.script.statements->nodes[0],
            &budget));
    TEST_ASSERT_EQUAL_UINT(1u, array->count);
    heapBeforeGrowth = budget.usage.heapBytes;
    budget.limits.heapBytes = heapBeforeGrowth +
                              (TZrUInt64)(sizeof(SZrAstNode *) * 2u) - 1u;

    TEST_ASSERT_FALSE(ZrParser_AstNodeArray_TryAddBudget(
            g_state,
            array,
            script->data.script.statements->nodes[1],
            &budget));
    TEST_ASSERT_EQUAL_UINT(1u, array->count);
    TEST_ASSERT_EQUAL_PTR(
            script->data.script.statements->nodes[0], array->nodes[0]);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
            budget.exceededResource);

    ZrParser_AstNodeArray_Free(g_state, array);
    ZrParser_Ast_Free(g_state, script);
    TEST_ASSERT_EQUAL_UINT(rootsBefore, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_cursor_restore_preserves_fatal_resource_failure(void) {
    SZrParserState parser;
    SZrParserCursor cursor;
    SZrParserComptimeBudget *budget;
    SZrAstNodeArray *array;
    TZrBool fatal;
    TZrBool error;

    ZrParser_State_Init(&parser, g_state, "1", 1u, ZR_NULL);
    TEST_ASSERT_FALSE(parser.hasError);
    parser.suppressErrorOutput = ZR_TRUE;
    budget = ZrParser_Ownership_Budget(parser.lexer->ownership);
    budget->limits.heapBytes = budget->usage.heapBytes;
    save_parser_cursor(&parser, &cursor);
    array = parser_ast_array_new(&parser, 1u);
    TEST_ASSERT_NULL(array);
    TEST_ASSERT_TRUE(parser.hasFatalError);
    restore_parser_cursor(&parser, &cursor);
    fatal = parser.hasFatalError;
    error = parser.hasError;
    ZrParser_State_Free(&parser);
    TEST_ASSERT_TRUE_MESSAGE(fatal, "Speculative rollback must preserve fatal resource failure");
    TEST_ASSERT_TRUE(error);
}

static void test_ast_node_limit_is_fatal(void) {
    SZrParserState parser;
    SZrParserComptimeBudget *budget;
    SZrAstNode *node;
    TZrBool fatal;

    ZrParser_State_Init(&parser, g_state, "1", 1u, ZR_NULL);
    TEST_ASSERT_FALSE(parser.hasError);
    parser.suppressErrorOutput = ZR_TRUE;
    budget = ZrParser_Ownership_Budget(parser.lexer->ownership);
    budget->limits.nodeCount = budget->usage.nodeCount;
    node = create_ast_node(&parser, ZR_AST_INTEGER_LITERAL, parser.currentLocation);
    TEST_ASSERT_NULL(node);
    fatal = parser.hasFatalError;
    ZrParser_State_Free(&parser);
    TEST_ASSERT_TRUE_MESSAGE(fatal, "AST allocation failure must stop speculative parsing");
}

static void test_embedded_parser_uses_outer_heap_limit(void) {
    SZrParserState parser;
    SZrParserComptimeBudget *budget;
    SZrAstNode *expression;
    SZrFileRange originLocation;
    TZrBool rejected;
    TZrBool heapExceeded;

    ZrParser_State_Init(&parser, g_state, "1", 1u, ZR_NULL);
    TEST_ASSERT_FALSE(parser.hasError);
    parser.suppressErrorOutput = ZR_TRUE;
    originLocation = ZrParser_SourceProjection_CurrentLocation(&parser);
    budget = ZrParser_Ownership_Budget(parser.lexer->ownership);
    budget->limits.heapBytes = budget->usage.heapBytes;
    expression = parse_embedded_expression(&parser, "1", 1u,
            ZrParser_SyntaxContext_Root(), ZrParser_TokenSpan_None(), &originLocation);
    rejected = expression == ZR_NULL;
    heapExceeded = budget->exceededResource == ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES &&
                   budget->usage.heapBytes <= budget->limits.heapBytes &&
                   !parser.lexer->hasSourceEvidenceError;
    ZrParser_Ast_Free(g_state, expression);
    ZrParser_State_Free(&parser);
    TEST_ASSERT_TRUE_MESSAGE(rejected, "Interpolation must obey the outer heap limit");
    TEST_ASSERT_TRUE_MESSAGE(heapExceeded, "Interpolation rejection must come from the outer heap budget");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_growth_failure_keeps_existing_statements);
    RUN_TEST(test_cursor_restore_preserves_fatal_resource_failure);
    RUN_TEST(test_ast_node_limit_is_fatal);
    RUN_TEST(test_embedded_parser_uses_outer_heap_limit);
    return UNITY_END();
}
