#include <string.h>

#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

static TZrBool parse_source_has_error(const TZrChar *source, TZrChar *name) {
    SZrString *sourceName = ZrCore_String_Create(g_state, name, strlen(name));
    SZrParserState parser;
    SZrAstNode *script;
    TZrBool hasError;

    if (sourceName == ZR_NULL) {
        return ZR_TRUE;
    }
    ZrParser_State_Init(&parser, g_state, source, strlen(source), sourceName);
    parser.suppressErrorOutput = ZR_TRUE;
    script = ZrParser_ParseWithState(&parser);
    hasError = parser.hasError;
    if (script != ZR_NULL) {
        ZrParser_Ast_Free(g_state, script);
    }
    ZrParser_State_Free(&parser);
    return hasError;
}

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

static void test_yield_is_a_reserved_statement_keyword(void) {
    static const TZrChar source[] = "yield 1;";
    SZrString *sourceName = ZrCore_String_Create(
            g_state, "yield_keyword.zr", strlen("yield_keyword.zr"));
    SZrLexState lexer;

    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_Lexer_Init(
            &lexer, g_state, source, strlen(source), sourceName);
    TEST_ASSERT_EQUAL_INT(ZR_TK_YIELD, lexer.t.token);
    ZrParser_Lexer_Free(&lexer);
}

static void test_yield_parses_as_a_dedicated_statement(void) {
    static const TZrChar source[] =
            "fn numbers(limit: int): Iterator<int> {\n"
            "    yield limit;\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state, "yield_statement.zr", strlen("yield_statement.zr"));
    SZrAstNode *script;
    SZrAstNode *function;
    SZrAstNode *yieldStatement;

    TEST_ASSERT_NOT_NULL(sourceName);
    script = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1u, script->data.script.statements->count);

    function = script->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, function->type);
    TEST_ASSERT_NOT_NULL(function->data.functionDeclaration.body);
    TEST_ASSERT_NOT_NULL(function->data.functionDeclaration.body->data.block.body);
    TEST_ASSERT_EQUAL_UINT32(
            1u, function->data.functionDeclaration.body->data.block.body->count);

    yieldStatement = function->data.functionDeclaration.body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(yieldStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_YIELD_STATEMENT, yieldStatement->type);
    TEST_ASSERT_NOT_NULL(yieldStatement->data.yieldStatement.expr);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_IDENTIFIER_LITERAL, yieldStatement->data.yieldStatement.expr->type);

    ZrParser_Ast_Free(g_state, script);
}

static void test_yield_requires_expression_and_terminating_semicolon(void) {
    static const TZrChar missingExpression[] =
            "fn values(): Iterator<int> { yield; }\n";
    static const TZrChar missingSemicolon[] =
            "fn values(): Iterator<int> { yield 1 }\n";

    TEST_ASSERT_TRUE(parse_source_has_error(missingExpression, "yield_missing_expression.zr"));
    TEST_ASSERT_TRUE(parse_source_has_error(missingSemicolon, "yield_missing_semicolon.zr"));
}

static void test_iterator_function_modifier_remains_rejected(void) {
    static const TZrChar source[] =
            "iterator fn values(): Iterator<int> { yield 1; }\n";

    TEST_ASSERT_TRUE(parse_source_has_error(source, "iterator_function_modifier.zr"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_yield_is_a_reserved_statement_keyword);
    RUN_TEST(test_yield_parses_as_a_dedicated_statement);
    RUN_TEST(test_yield_requires_expression_and_terminating_semicolon);
    RUN_TEST(test_iterator_function_modifier_remains_rejected);
    return UNITY_END();
}
