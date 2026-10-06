#include <stdio.h>
#include <string.h>

/* Parse CRT declarations before Unity's noreturn macro on Windows. */
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"
#include "../../zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h"

/* Parser-only contract: fixed raw byte ranges include both backticks. */
enum { TEMPLATE_PARSE_STATE = 0, TEMPLATE_SCAN_STATE = 1, TEMPLATE_STATE_COUNT = 2 };

typedef struct SZrTemplateExpectedRange {
    TZrSize startOffset;
    TZrInt32 startLine;
    TZrInt32 startColumn;
    TZrSize endOffset;
    TZrInt32 endLine;
    TZrInt32 endColumn;
} SZrTemplateExpectedRange;

static SZrState *g_state;
static SZrString *g_sourceName;
static TZrBool g_sourceNameRooted;
static SZrParserState g_parsers[TEMPLATE_STATE_COUNT];
static TZrBool g_parserInitialized[TEMPLATE_STATE_COUNT];
static SZrAstNode *g_firstStatement;
static SZrAstNode *g_secondStatement;
static SZrAstNode *g_expression;
static TZrUInt32 g_diagnosticCount;
static TZrBool g_prerequisitesOnly;

void setUp(void) {
    g_state = ZR_NULL;
    g_sourceName = ZR_NULL;
    g_sourceNameRooted = ZR_FALSE;
    g_firstStatement = ZR_NULL;
    g_secondStatement = ZR_NULL;
    g_expression = ZR_NULL;
    g_diagnosticCount = 0u;
    memset(g_parsers, 0, sizeof(g_parsers));
    memset(g_parserInitialized, 0, sizeof(g_parserInitialized));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    TZrUInt32 index;
    if (g_state == ZR_NULL) return;
    if (g_firstStatement != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_firstStatement);
        g_firstStatement = ZR_NULL;
    }
    if (g_secondStatement != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_secondStatement);
        g_secondStatement = ZR_NULL;
    }
    if (g_expression != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_expression);
        g_expression = ZR_NULL;
    }
    for (index = 0u; index < TEMPLATE_STATE_COUNT; ++index) {
        if (g_parserInitialized[index]) {
            ZrParser_State_Free(&g_parsers[index]);
            g_parserInitialized[index] = ZR_FALSE;
        }
    }
    if (g_sourceNameRooted) {
        ZrCore_GarbageCollector_UnignoreObject(g_state->global,
                ZR_CAST_RAW_OBJECT_AS_SUPER(g_sourceName));
        g_sourceNameRooted = ZR_FALSE;
    }
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
    g_sourceName = ZR_NULL;
}

static void capture_parse_error(TZrPtr userData, const SZrFileRange *location,
        const TZrChar *message, EZrToken token) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(message);
    ZR_UNUSED_PARAMETER(token);
    ++g_diagnosticCount;
}

static void initialize_source_name(void) {
    g_sourceName = ZrCore_String_CreateFromNative(g_state,
            "template_literal_source_range.zr");
    TEST_ASSERT_NOT_NULL(g_sourceName);
    g_sourceNameRooted = ZrCore_GarbageCollector_IgnoreObject(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(g_sourceName));
    TEST_ASSERT_TRUE_MESSAGE(g_sourceNameRooted,
            "PRECONDITION: source name remains rooted through both parsers");
}

static SZrParserState *initialize_parser(const char *source, TZrUInt32 index) {
    SZrParserState *parser = &g_parsers[index];
    g_parserInitialized[index] = ZR_TRUE;
    ZrParser_State_Init(parser, g_state, source, strlen(source), g_sourceName);
    TEST_ASSERT_FALSE_MESSAGE(parser->hasError,
            "PRECONDITION: legal source initializes the actual parser");
    TEST_ASSERT_NOT_NULL(parser->lexer);
    TEST_ASSERT_FALSE(parser->lexer->t.hasLexError);
    parser->errorCallback = capture_parse_error;
    parser->suppressErrorOutput = ZR_TRUE;
    return parser;
}

static void assert_parser_complete(const SZrParserState *parser) {
    TEST_ASSERT_FALSE(parser->hasError);
    TEST_ASSERT_FALSE(parser->hasFatalError);
    TEST_ASSERT_FALSE(parser->lexer->t.hasLexError);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, parser->lexer->t.token);
    TEST_ASSERT_EQUAL_UINT32(0u, g_diagnosticCount);
}

static void assert_exact_range(const SZrFileRange *actual,
        const SZrTemplateExpectedRange *expected) {
    TEST_ASSERT_EQUAL_PTR(g_sourceName, actual->source);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)expected->startOffset,
            (TZrUInt64)actual->start.offset);
    TEST_ASSERT_EQUAL_INT(expected->startLine, actual->start.line);
    TEST_ASSERT_EQUAL_INT(expected->startColumn, actual->start.column);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)expected->endOffset,
            (TZrUInt64)actual->end.offset);
    TEST_ASSERT_EQUAL_INT(expected->endLine, actual->end.line);
    TEST_ASSERT_EQUAL_INT(expected->endColumn, actual->end.column);
}

static void assert_token_prerequisite(const char *source, const char *rawTemplate,
        TZrBool hasSuffix, const SZrTemplateExpectedRange *expected) {
    SZrParserState *scan = initialize_parser(source, TEMPLATE_SCAN_STATE);
    SZrFileRange tokenRange;
    if (hasSuffix) {
        TEST_ASSERT_EQUAL_INT(ZR_TK_RETURN, scan->lexer->t.token);
        ZrParser_Lexer_Next(scan->lexer);
    }
    TEST_ASSERT_EQUAL_INT(ZR_TK_TEMPLATE_STRING, scan->lexer->t.token);
    TEST_ASSERT_FALSE(scan->lexer->t.hasLexError);
    tokenRange = get_current_token_location(scan);
    assert_exact_range(&tokenRange, expected);
    TEST_ASSERT_TRUE(tokenRange.start.offset <= tokenRange.end.offset);
    TEST_ASSERT_TRUE(tokenRange.end.offset <= strlen(source));
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)strlen(rawTemplate),
            (TZrUInt64)(tokenRange.end.offset - tokenRange.start.offset));
    TEST_ASSERT_EQUAL_MEMORY(rawTemplate, source + tokenRange.start.offset,
            strlen(rawTemplate));
    ZrParser_Lexer_Next(scan->lexer);
    if (hasSuffix) {
        TEST_ASSERT_EQUAL_INT(ZR_TK_SEMICOLON, scan->lexer->t.token);
        ZrParser_Lexer_Next(scan->lexer);
        TEST_ASSERT_EQUAL_INT(ZR_TK_RETURN, scan->lexer->t.token);
        ZrParser_Lexer_Next(scan->lexer);
        TEST_ASSERT_EQUAL_INT(ZR_TK_INTEGER, scan->lexer->t.token);
        ZrParser_Lexer_Next(scan->lexer);
        TEST_ASSERT_EQUAL_INT(ZR_TK_SEMICOLON, scan->lexer->t.token);
        ZrParser_Lexer_Next(scan->lexer);
    }
    assert_parser_complete(scan);
}

static void assert_string_segment(const SZrAstNode *segment, const char *text) {
    TEST_ASSERT_NOT_NULL(segment);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL, segment->type);
    TEST_ASSERT_NOT_NULL(segment->data.stringLiteral.value);
    TEST_ASSERT_EQUAL_STRING(text,
            ZrCore_String_GetNativeString(segment->data.stringLiteral.value));
}

static SZrAstNodeArray *assert_template_segments(const SZrAstNode *node) {
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TEMPLATE_STRING_LITERAL, node->type);
    TEST_ASSERT_NOT_NULL(node->data.templateStringLiteral.segments);
    TEST_ASSERT_TRUE(node->data.templateStringLiteral.segments->count > 0);
    TEST_ASSERT_NOT_NULL(node->data.templateStringLiteral.segments->nodes);
    return node->data.templateStringLiteral.segments;
}

static void assert_suffix_case(const char *source, const char *rawTemplate,
        const SZrTemplateExpectedRange *expected) {
    SZrParserState *parser;
    SZrAstNode *templateNode;
    initialize_source_name();
    parser = initialize_parser(source, TEMPLATE_PARSE_STATE);
    g_firstStatement = ZrParser_ParseTopLevelStatementWithState(parser);
    TEST_ASSERT_NOT_NULL(g_firstStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, g_firstStatement->type);
    TEST_ASSERT_FALSE(g_firstStatement->data.returnStatement.isReferenceReturn);
    templateNode = g_firstStatement->data.returnStatement.expr;
    assert_template_segments(templateNode);
    g_secondStatement = ZrParser_ParseTopLevelStatementWithState(parser);
    TEST_ASSERT_NOT_NULL(g_secondStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, g_secondStatement->type);
    TEST_ASSERT_FALSE(g_secondStatement->data.returnStatement.isReferenceReturn);
    TEST_ASSERT_NOT_NULL(g_secondStatement->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL,
            g_secondStatement->data.returnStatement.expr->type);
    TEST_ASSERT_EQUAL_INT64(9,
            g_secondStatement->data.returnStatement.expr->data.integerLiteral.value);
    assert_parser_complete(parser);
    assert_token_prerequisite(source, rawTemplate, ZR_TRUE, expected);
    if (!g_prerequisitesOnly) assert_exact_range(&templateNode->location, expected);
}

static void test_single_line_template_excludes_following_statement(void) {
    const SZrTemplateExpectedRange expected = {7u, 1, 8, 10u, 1, 11};
    assert_suffix_case("return `x`;return 9;", "`x`", &expected);
}

static void test_lf_template_excludes_following_statement(void) {
    const SZrTemplateExpectedRange expected = {7u, 1, 8, 14u, 2, 4};
    assert_suffix_case("return `ab\ncd`;\nreturn 9;", "`ab\ncd`", &expected);
}

static void test_crlf_template_excludes_following_statement(void) {
    const SZrTemplateExpectedRange expected = {7u, 1, 8, 15u, 2, 4};
    assert_suffix_case("return `ab\r\ncd`;\r\nreturn 9;", "`ab\r\ncd`", &expected);
}

static void test_bare_cr_template_excludes_following_statement(void) {
    const SZrTemplateExpectedRange expected = {7u, 1, 8, 14u, 2, 4};
    assert_suffix_case("return `ab\rcd`;\rreturn 9;", "`ab\rcd`", &expected);
}

static void assert_fragment_case(const char *source, TZrBool interpolated,
        const SZrTemplateExpectedRange *expected) {
    SZrParserState *parser;
    SZrAstNodeArray *segments;
    SZrAstNode *embedded;
    initialize_source_name();
    parser = initialize_parser(source, TEMPLATE_PARSE_STATE);
    g_expression = ZrParser_ParseExpressionWithState(parser);
    segments = assert_template_segments(g_expression);
    if (interpolated) {
        TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)segments->count);
        assert_string_segment(segments->nodes[0], "a");
        TEST_ASSERT_NOT_NULL(segments->nodes[1]);
        TEST_ASSERT_EQUAL_INT(ZR_AST_INTERPOLATED_SEGMENT, segments->nodes[1]->type);
        embedded = segments->nodes[1]->data.interpolatedSegment.expression;
        TEST_ASSERT_NOT_NULL(embedded);
        TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, embedded->type);
        TEST_ASSERT_EQUAL_INT64(1, embedded->data.integerLiteral.value);
        assert_string_segment(segments->nodes[2], "b");
    } else {
        TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)segments->count);
        assert_string_segment(segments->nodes[0], "x");
    }
    assert_parser_complete(parser);
    assert_token_prerequisite(source, source, ZR_FALSE, expected);
    if (!g_prerequisitesOnly) assert_exact_range(&g_expression->location, expected);
}

static void test_template_fragment_at_eof(void) {
    const SZrTemplateExpectedRange expected = {0u, 1, 1, 3u, 1, 4};
    assert_fragment_case("`x`", ZR_FALSE, &expected);
}

static void test_interpolated_template_fragment_at_eof(void) {
    const SZrTemplateExpectedRange expected = {0u, 1, 1, 8u, 1, 9};
    assert_fragment_case("`a${1}b`", ZR_TRUE, &expected);
}

int main(int argc, char **argv) {
    g_prerequisitesOnly = ZR_FALSE;
    if (argc == 2 && strcmp(argv[1], "--prerequisites-only") == 0) {
        g_prerequisitesOnly = ZR_TRUE;
        puts("Prerequisites only: final template AST root range assertions withheld.");
    } else if (argc != 1) {
        fputs("Usage: test_template_literal_source_range [--prerequisites-only]\n", stderr);
        return 2;
    }
    UNITY_BEGIN();
    RUN_TEST(test_single_line_template_excludes_following_statement);
    RUN_TEST(test_lf_template_excludes_following_statement);
    RUN_TEST(test_crlf_template_excludes_following_statement);
    RUN_TEST(test_bare_cr_template_excludes_following_statement);
    RUN_TEST(test_template_fragment_at_eof);
    RUN_TEST(test_interpolated_template_fragment_at_eof);
    return UNITY_END();
}
