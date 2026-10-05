#include <string.h>

/* Parse CRT declarations before Unity's noreturn macro on Windows. */
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"
#include "../../zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h"

/*
 * Parse-only source range contract. Expected positions come from the actual
 * lexer token ranges in this source, with byte offsets and [start, end) ends.
 * SCRIPT starts at the first token and ends at EOS; RETURN includes its
 * semicolon, but excludes following trivia and the next statement.
 */
enum { RANGE_SCAN_STATE = 0, RANGE_PARSE_STATE = 1, RANGE_STATE_COUNT = 2 };

typedef struct SZrRangeTokenEvidence {
    SZrFileRange first;
    SZrFileRange returnToken;
    SZrFileRange value;
    SZrFileRange reference;
    SZrFileRange semicolon;
    SZrFileRange following;
    SZrFileRange eos;
    TZrBool hasReturn;
    TZrBool hasValue;
    TZrBool hasReference;
    TZrBool hasSemicolon;
    TZrBool hasFollowing;
    EZrToken followingToken;
} SZrRangeTokenEvidence;

static SZrState *g_state;
static SZrString *g_sourceName;
static TZrBool g_sourceNameRooted;
static SZrParserState g_parsers[RANGE_STATE_COUNT];
static TZrBool g_parserInitialized[RANGE_STATE_COUNT];
static SZrAstNode *g_ast;
static SZrAstNode *g_followingAst;
static TZrUInt32 g_diagnosticCount;
static EZrToken g_diagnosticToken;

void setUp(void) {
    g_sourceName = ZR_NULL;
    g_sourceNameRooted = ZR_FALSE;
    g_ast = ZR_NULL;
    g_followingAst = ZR_NULL;
    g_diagnosticCount = 0u;
    g_diagnosticToken = ZR_TK_EOS;
    memset(g_parsers, 0, sizeof(g_parsers));
    memset(g_parserInitialized, 0, sizeof(g_parserInitialized));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    TZrUInt32 index;
    if (g_state == ZR_NULL) return;
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    if (g_followingAst != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_followingAst);
        g_followingAst = ZR_NULL;
    }
    for (index = 0u; index < RANGE_STATE_COUNT; ++index) {
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
    g_diagnosticToken = token;
    ++g_diagnosticCount;
}

/* Literal coordinates keep expected ranges independent of parser helpers. */
static void assert_exact_range(const SZrFileRange *range,
        TZrSize startOffset, TZrInt32 startLine, TZrInt32 startColumn,
        TZrSize endOffset, TZrInt32 endLine, TZrInt32 endColumn) {
    TEST_ASSERT_EQUAL_PTR(g_sourceName, range->source);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)startOffset, (TZrUInt64)range->start.offset);
    TEST_ASSERT_EQUAL_INT(startLine, range->start.line);
    TEST_ASSERT_EQUAL_INT(startColumn, range->start.column);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)endOffset, (TZrUInt64)range->end.offset);
    TEST_ASSERT_EQUAL_INT(endLine, range->end.line);
    TEST_ASSERT_EQUAL_INT(endColumn, range->end.column);
}

static void assert_same_range(const SZrFileRange *expected,
        const SZrFileRange *actual) {
    TEST_ASSERT_EQUAL_PTR(expected->source, actual->source);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)expected->start.offset,
            (TZrUInt64)actual->start.offset);
    TEST_ASSERT_EQUAL_INT(expected->start.line, actual->start.line);
    TEST_ASSERT_EQUAL_INT(expected->start.column, actual->start.column);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)expected->end.offset,
            (TZrUInt64)actual->end.offset);
    TEST_ASSERT_EQUAL_INT(expected->end.line, actual->end.line);
    TEST_ASSERT_EQUAL_INT(expected->end.column, actual->end.column);
}

static void assert_source_span(const char *source, const SZrFileRange *range,
        const char *expected) {
    TZrSize length = strlen(expected);
    TEST_ASSERT_EQUAL_PTR(g_sourceName, range->source);
    TEST_ASSERT_TRUE(range->start.offset <= range->end.offset);
    TEST_ASSERT_TRUE(range->end.offset <= strlen(source));
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)length,
            (TZrUInt64)(range->end.offset - range->start.offset));
    if (length != 0u) {
        TEST_ASSERT_EQUAL_MEMORY(expected, source + range->start.offset, length);
    }
}

static SZrParserState *initialize_parser(const char *source, TZrUInt32 index) {
    SZrParserState *parser = &g_parsers[index];
    g_parserInitialized[index] = ZR_TRUE;
    ZrParser_State_Init(parser, g_state, source, strlen(source), g_sourceName);
    TEST_ASSERT_FALSE_MESSAGE(parser->hasError,
            "PRECONDITION: parser and first token initialize successfully");
    TEST_ASSERT_NOT_NULL(parser->lexer);
    TEST_ASSERT_FALSE(parser->lexer->t.hasLexError);
    parser->errorCallback = capture_parse_error;
    parser->suppressErrorOutput = ZR_TRUE;
    return parser;
}

static void initialize_source_name(void) {
    g_sourceName = ZrCore_String_CreateFromNative(g_state,
            "ssa_source_range_identity.zr");
    TEST_ASSERT_NOT_NULL(g_sourceName);
    g_sourceNameRooted = ZrCore_GarbageCollector_IgnoreObject(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(g_sourceName));
    TEST_ASSERT_TRUE_MESSAGE(g_sourceNameRooted,
            "PRECONDITION: source name remains rooted through both parses");
}

static void parse_with_token_evidence(const char *source,
        SZrRangeTokenEvidence *evidence) {
    SZrParserState *scan;
    SZrParserState *parser;
    memset(evidence, 0, sizeof(*evidence));
    initialize_source_name();

    scan = initialize_parser(source, RANGE_SCAN_STATE);
    evidence->first = get_current_token_location(scan);
    for (;;) {
        EZrToken token = scan->lexer->t.token;
        SZrFileRange range = get_current_token_location(scan);
        TEST_ASSERT_FALSE(scan->lexer->t.hasLexError);
        TEST_ASSERT_EQUAL_PTR(g_sourceName, range.source);
        if (evidence->hasSemicolon && !evidence->hasFollowing) {
            evidence->following = range;
            evidence->followingToken = token;
            evidence->hasFollowing = ZR_TRUE;
        }
        if (token == ZR_TK_RETURN && !evidence->hasReturn) {
            evidence->returnToken = range;
            evidence->hasReturn = ZR_TRUE;
        } else if (evidence->hasReturn && !evidence->hasSemicolon) {
            if (token == ZR_TK_REF) {
                evidence->reference = range;
                evidence->hasReference = ZR_TRUE;
            } else if ((token == ZR_TK_INTEGER || token == ZR_TK_IDENTIFIER) &&
                    !evidence->hasValue) {
                evidence->value = range;
                evidence->hasValue = ZR_TRUE;
            } else if (token == ZR_TK_SEMICOLON) {
                evidence->semicolon = range;
                evidence->hasSemicolon = ZR_TRUE;
            }
        }
        if (token == ZR_TK_EOS) {
            evidence->eos = range;
            break;
        }
        ZrParser_Lexer_Next(scan->lexer);
    }

    parser = initialize_parser(source, RANGE_PARSE_STATE);
    g_ast = ZrParser_ParseWithState(parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_ast,
            "PRECONDITION: legal source produces a real SCRIPT AST");
    TEST_ASSERT_FALSE(parser->hasError);
    TEST_ASSERT_FALSE(parser->hasFatalError);
    TEST_ASSERT_EQUAL_UINT32(0u, g_diagnosticCount);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, parser->lexer->t.token);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    TEST_ASSERT_NULL(g_ast->data.script.moduleName);
}

static void assert_script_range(const SZrRangeTokenEvidence *evidence) {
    SZrFileRange expected = ZrParser_FileRange_Merge(evidence->first, evidence->eos);
    assert_same_range(&expected, &g_ast->location);
}

static SZrAstNode *assert_return_range(const SZrRangeTokenEvidence *evidence,
        TZrUInt32 statementCount) {
    SZrFileRange expected;
    SZrAstNode *statement;
    TEST_ASSERT_TRUE(evidence->hasReturn);
    TEST_ASSERT_TRUE(evidence->hasSemicolon);
    TEST_ASSERT_EQUAL_UINT32(statementCount,
            (TZrUInt32)g_ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements->nodes);
    statement = g_ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statement->type);
    expected = ZrParser_FileRange_Merge(evidence->returnToken, evidence->semicolon);
    assert_same_range(&expected, &statement->location);
    return statement;
}

static void assert_literal_ranges(const char *source, const char *returnText,
        TZrUInt32 statementCount) {
    SZrRangeTokenEvidence evidence;
    SZrAstNode *statement;
    parse_with_token_evidence(source, &evidence);
    statement = assert_return_range(&evidence, statementCount);
    TEST_ASSERT_TRUE(evidence.hasValue);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL,
            statement->data.returnStatement.expr->type);
    TEST_ASSERT_FALSE(statement->data.returnStatement.isReferenceReturn);
    assert_same_range(&evidence.value, &statement->data.returnStatement.expr->location);
    assert_source_span(source, &statement->data.returnStatement.expr->location, "9");
    assert_source_span(source, &statement->location, returnText);
    assert_script_range(&evidence);
}

static void test_script_root_starts_at_first_token(void) {
    SZrRangeTokenEvidence evidence;
    const char *source = "return 9;\n";
    parse_with_token_evidence(source, &evidence);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)g_ast->data.script.statements->count);
    assert_script_range(&evidence);
    assert_source_span(source, &g_ast->location, source);
}

static void test_return_literal_includes_semicolon(void) {
    assert_literal_ranges("return 9;\n", "return 9;", 1u);
}

static void test_literal_at_eof_without_newline(void) {
    assert_literal_ranges("return 9;", "return 9;", 1u);
}

static void test_leading_and_trailing_trivia(void) {
    assert_literal_ranges(" \t// leading\n  return 9; /* trailing */\n\t",
            "return 9;", 1u);
}

static void test_return_expression_on_later_line(void) {
    assert_literal_ranges("return\n  9;\n", "return\n  9;", 1u);
}

static void test_crlf_return_and_root_ranges(void) {
    SZrRangeTokenEvidence evidence;
    SZrAstNode *statement;
    const char *source = "// leading\r\n  return\r\n    9;\r\n";
    parse_with_token_evidence(source, &evidence);
    assert_exact_range(&evidence.returnToken, 14u, 2, 3, 20u, 2, 9);
    assert_exact_range(&evidence.value, 26u, 3, 5, 27u, 3, 6);
    assert_exact_range(&evidence.semicolon, 27u, 3, 6, 28u, 3, 7);
    assert_exact_range(&evidence.eos, 30u, 4, 1, 30u, 4, 1);
    statement = assert_return_range(&evidence, 1u);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL,
            statement->data.returnStatement.expr->type);
    TEST_ASSERT_EQUAL_INT64(9, statement->data.returnStatement.expr->data.integerLiteral.value);
    assert_exact_range(&statement->data.returnStatement.expr->location,
            26u, 3, 5, 27u, 3, 6);
    assert_exact_range(&statement->location, 14u, 2, 3, 28u, 3, 7);
    assert_source_span(source, &statement->location, "return\r\n    9;");
    assert_exact_range(&g_ast->location, 14u, 2, 3, 30u, 4, 1);
    assert_script_range(&evidence);
}

static void test_semicolon_token_before_crlf_excludes_both_newline_bytes(void) {
    const char *source = "return 9;\r\n";
    SZrParserState *scan;
    SZrFileRange range;
    initialize_source_name();
    scan = initialize_parser(source, RANGE_SCAN_STATE);
    TEST_ASSERT_EQUAL_INT(ZR_TK_RETURN, scan->lexer->t.token);
    ZrParser_Lexer_Next(scan->lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_INTEGER, scan->lexer->t.token);
    ZrParser_Lexer_Next(scan->lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_SEMICOLON, scan->lexer->t.token);
    TEST_ASSERT_FALSE(scan->lexer->t.hasLexError);
    /* The scanner has already read the whole CRLF into its newline cursor. */
    TEST_ASSERT_EQUAL_UINT64(8u, (TZrUInt64)scan->lexer->tokenStartOffset);
    TEST_ASSERT_EQUAL_UINT64(0u, (TZrUInt64)scan->lexer->tokenStartLineStart);
    TEST_ASSERT_EQUAL_INT(1, scan->lexer->tokenStartLine);
    TEST_ASSERT_EQUAL_UINT64(11u, (TZrUInt64)scan->lexer->currentPos);
    TEST_ASSERT_EQUAL_INT('\n', scan->lexer->currentChar);
    TEST_ASSERT_EQUAL_INT(2, scan->lexer->lineNumber);
    TEST_ASSERT_EQUAL_UINT64(11u, (TZrUInt64)scan->lexer->currentLineStartOffset);
    range = get_current_token_location(scan);
    assert_exact_range(&range, 8u, 1, 9, 9u, 1, 10);
    assert_source_span(source, &range, ";");
    ZrParser_Lexer_Next(scan->lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, scan->lexer->t.token);
    range = get_current_token_location(scan);
    assert_exact_range(&range, 11u, 2, 1, 11u, 2, 1);
    TEST_ASSERT_EQUAL_UINT32(0u, g_diagnosticCount);
}

static void test_missing_return_semicolon_preserves_following_declaration(void) {
    const char *source = "return 9 var next = 8;";
    SZrParserState *parser;
    SZrFileRange following;
    initialize_source_name();
    parser = initialize_parser(source, RANGE_PARSE_STATE);
    /* Parse each statement directly; SCRIPT's error-state contract is separate. */
    g_ast = parse_return_statement(parser);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, g_ast->type);
    TEST_ASSERT_TRUE(parser->hasError);
    TEST_ASSERT_FALSE(parser->hasFatalError);
    TEST_ASSERT_EQUAL_UINT32(1u, g_diagnosticCount);
    TEST_ASSERT_EQUAL_INT(ZR_TK_VAR, g_diagnosticToken);
    TEST_ASSERT_EQUAL_INT(ZR_TK_VAR, parser->lexer->t.token);
    TEST_ASSERT_FALSE(parser->lexer->t.hasLexError);
    following = get_current_token_location(parser);
    assert_exact_range(&following, 9u, 1, 10, 12u, 1, 13);
    assert_source_span(source, &following, "var");
    assert_exact_range(&g_ast->location, 0u, 1, 1, 8u, 1, 9);
    assert_source_span(source, &g_ast->location, "return 9");
    TEST_ASSERT_FALSE(g_ast->data.returnStatement.isReferenceReturn);
    TEST_ASSERT_NOT_NULL(g_ast->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, g_ast->data.returnStatement.expr->type);
    TEST_ASSERT_EQUAL_INT64(9, g_ast->data.returnStatement.expr->data.integerLiteral.value);
    assert_exact_range(&g_ast->data.returnStatement.expr->location,
            7u, 1, 8, 8u, 1, 9);

    g_followingAst = parse_variable_declaration(parser);
    TEST_ASSERT_NOT_NULL(g_followingAst);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, g_followingAst->type);
    TEST_ASSERT_NOT_NULL(g_followingAst->data.variableDeclaration.pattern);
    assert_source_span(source, &g_followingAst->data.variableDeclaration.pattern->location,
            "next");
    TEST_ASSERT_NOT_NULL(g_followingAst->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL,
            g_followingAst->data.variableDeclaration.value->type);
    TEST_ASSERT_EQUAL_INT64(8,
            g_followingAst->data.variableDeclaration.value->data.integerLiteral.value);
    assert_source_span(source, &g_followingAst->data.variableDeclaration.value->location, "8");
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, parser->lexer->t.token);
    TEST_ASSERT_EQUAL_UINT32(1u, g_diagnosticCount);
    TEST_ASSERT_TRUE(parser->hasError);
    TEST_ASSERT_FALSE(parser->hasFatalError);
}

static void test_return_excludes_following_statement(void) {
    SZrRangeTokenEvidence evidence;
    SZrAstNode *statement;
    const char *source = "return 9; /* gap */ var next = 8;\n";
    parse_with_token_evidence(source, &evidence);
    statement = assert_return_range(&evidence, 2u);
    TEST_ASSERT_TRUE(evidence.hasFollowing);
    TEST_ASSERT_EQUAL_INT(ZR_TK_VAR, evidence.followingToken);
    TEST_ASSERT_TRUE(statement->location.end.offset < evidence.following.start.offset);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION,
            g_ast->data.script.statements->nodes[1]->type);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);
    assert_same_range(&evidence.value, &statement->data.returnStatement.expr->location);
    assert_source_span(source, &statement->location, "return 9;");
    assert_script_range(&evidence);
}

static void assert_empty_script_range(const char *source) {
    SZrRangeTokenEvidence evidence;
    parse_with_token_evidence(source, &evidence);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)g_ast->data.script.statements->count);
    TEST_ASSERT_FALSE(evidence.hasReturn);
    assert_same_range(&evidence.eos, &evidence.first);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)strlen(source),
            (TZrUInt64)evidence.eos.start.offset);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)evidence.eos.start.offset,
            (TZrUInt64)evidence.eos.end.offset);
    assert_script_range(&evidence);
    assert_source_span(source, &g_ast->location, "");
}

static void test_empty_script_is_eos_point(void) {
    assert_empty_script_range("");
}

static void test_trivia_only_script_is_eos_point(void) {
    assert_empty_script_range(" \t// comment\r\n/* closed */\r\n");
}

static void test_operandless_return_includes_semicolon(void) {
    SZrRangeTokenEvidence evidence;
    SZrAstNode *statement;
    const char *source = "return;\n";
    parse_with_token_evidence(source, &evidence);
    statement = assert_return_range(&evidence, 1u);
    TEST_ASSERT_FALSE(evidence.hasValue);
    TEST_ASSERT_NULL(statement->data.returnStatement.expr);
    TEST_ASSERT_FALSE(statement->data.returnStatement.isReferenceReturn);
    assert_source_span(source, &statement->location, "return;");
    assert_script_range(&evidence);
}

static void test_reference_return_preserves_ref_token_range(void) {
    SZrRangeTokenEvidence evidence;
    SZrAstNode *statement;
    const char *source = "return ref value;\n";
    parse_with_token_evidence(source, &evidence);
    statement = assert_return_range(&evidence, 1u);
    TEST_ASSERT_TRUE(evidence.hasReference);
    TEST_ASSERT_TRUE(evidence.hasValue);
    TEST_ASSERT_TRUE(statement->data.returnStatement.isReferenceReturn);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
            statement->data.returnStatement.expr->type);
    assert_same_range(&evidence.reference, &statement->data.returnStatement.referenceLocation);
    assert_same_range(&evidence.value, &statement->data.returnStatement.expr->location);
    assert_source_span(source, &statement->data.returnStatement.referenceLocation, "ref");
    assert_source_span(source, &statement->data.returnStatement.expr->location, "value");
    assert_source_span(source, &statement->location, "return ref value;");
    assert_script_range(&evidence);
}

typedef struct SZrRangeExpectedCoordinates {
    TZrSize startOffset;
    TZrInt32 startLine;
    TZrInt32 startColumn;
    TZrSize endOffset;
    TZrInt32 endLine;
    TZrInt32 endColumn;
} SZrRangeExpectedCoordinates;

static void assert_literal_coordinates(const SZrFileRange *actual,
        const SZrRangeExpectedCoordinates *expected) {
    assert_exact_range(actual, expected->startOffset, expected->startLine,
            expected->startColumn, expected->endOffset, expected->endLine,
            expected->endColumn);
}

static void assert_multiline_template_token_ranges(const char *source,
        const char *templateSource,
        const SZrRangeExpectedCoordinates expected[3]) {
    SZrParserState *parser;
    SZrParserState *scan;
    SZrAstNode *statement;
    SZrFileRange returnBeforeLookahead;
    SZrFileRange returnAfterLookahead;
    SZrFileRange templateBeforeLookahead;
    SZrFileRange templateAfterLookahead;
    SZrFileRange semicolon;
    SZrFileRange eos;

    initialize_source_name();
    /* Establish legal source/owned AST before any coordinate assertion. */
    parser = initialize_parser(source, RANGE_PARSE_STATE);
    g_ast = ZrParser_ParseWithState(parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_ast,
            "PRECONDITION: multiline template source parses successfully");
    TEST_ASSERT_FALSE(parser->hasError);
    TEST_ASSERT_FALSE(parser->hasFatalError);
    TEST_ASSERT_EQUAL_UINT32(0u, g_diagnosticCount);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, parser->lexer->t.token);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)g_ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements->nodes);
    statement = g_ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statement->type);
    TEST_ASSERT_FALSE(statement->data.returnStatement.isReferenceReturn);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TEMPLATE_STRING_LITERAL,
            statement->data.returnStatement.expr->type);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr->data.templateStringLiteral.segments);
    TEST_ASSERT_TRUE(statement->data.returnStatement.expr->data.templateStringLiteral.segments->count > 0);
    /* Template AST coordinates have a separate producer contract. */

    scan = initialize_parser(source, RANGE_SCAN_STATE);
    TEST_ASSERT_EQUAL_INT(ZR_TK_RETURN, scan->lexer->t.token);
    returnBeforeLookahead = get_current_token_location(scan);
    TEST_ASSERT_EQUAL_INT(ZR_TK_TEMPLATE_STRING, ZrParser_Lexer_Lookahead(scan->lexer));
    TEST_ASSERT_FALSE(scan->lexer->lookahead.hasLexError);
    TEST_ASSERT_EQUAL_INT(ZR_TK_RETURN, scan->lexer->t.token);
    returnAfterLookahead = get_current_token_location(scan);
    ZrParser_Lexer_Next(scan->lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_TEMPLATE_STRING, scan->lexer->t.token);
    TEST_ASSERT_FALSE(scan->lexer->t.hasLexError);
    templateBeforeLookahead = get_current_token_location(scan);
    TEST_ASSERT_EQUAL_INT(ZR_TK_SEMICOLON, ZrParser_Lexer_Lookahead(scan->lexer));
    TEST_ASSERT_FALSE(scan->lexer->lookahead.hasLexError);
    TEST_ASSERT_EQUAL_INT(ZR_TK_TEMPLATE_STRING, scan->lexer->t.token);
    templateAfterLookahead = get_current_token_location(scan);
    ZrParser_Lexer_Next(scan->lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_SEMICOLON, scan->lexer->t.token);
    TEST_ASSERT_FALSE(scan->lexer->t.hasLexError);
    semicolon = get_current_token_location(scan);
    ZrParser_Lexer_Next(scan->lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, scan->lexer->t.token);
    TEST_ASSERT_FALSE(scan->lexer->t.hasLexError);
    eos = get_current_token_location(scan);
    TEST_ASSERT_EQUAL_UINT32(0u, g_diagnosticCount);

    assert_same_range(&returnBeforeLookahead, &returnAfterLookahead);
    assert_exact_range(&returnBeforeLookahead, 0u, 1, 1, 6u, 1, 7);
    assert_same_range(&templateBeforeLookahead, &templateAfterLookahead);
    assert_source_span(source, &templateBeforeLookahead, templateSource);
    assert_source_span(source, &semicolon, ";");
    assert_source_span(source, &eos, "");
    assert_literal_coordinates(&semicolon, &expected[1]);
    assert_literal_coordinates(&eos, &expected[2]);
    assert_literal_coordinates(&templateBeforeLookahead, &expected[0]);
}

static void test_multiline_template_token_coordinates_bare_cr(void) {
    const SZrRangeExpectedCoordinates expected[3] = {
        {7u, 1, 8, 14u, 2, 4},
        {14u, 2, 4, 15u, 2, 5},
        {16u, 3, 1, 16u, 3, 1}
    };
    assert_multiline_template_token_ranges("return `ab\rcd`;\r", "`ab\rcd`", expected);
}

static void test_multiline_template_token_coordinates_lf(void) {
    const SZrRangeExpectedCoordinates expected[3] = {
        {7u, 1, 8, 14u, 2, 4},
        {14u, 2, 4, 15u, 2, 5},
        {16u, 3, 1, 16u, 3, 1}
    };
    assert_multiline_template_token_ranges("return `ab\ncd`;\n", "`ab\ncd`", expected);
}

static void test_multiline_template_token_coordinates_crlf(void) {
    const SZrRangeExpectedCoordinates expected[3] = {
        {7u, 1, 8, 15u, 2, 4},
        {15u, 2, 4, 16u, 2, 5},
        {18u, 3, 1, 18u, 3, 1}
    };
    assert_multiline_template_token_ranges("return `ab\r\ncd`;\r\n", "`ab\r\ncd`", expected);
}

static void test_multiline_template_token_coordinates_mixed_newlines(void) {
    const SZrRangeExpectedCoordinates expected[3] = {
        {7u, 1, 8, 17u, 4, 3},
        {19u, 5, 1, 20u, 5, 2},
        {21u, 6, 1, 21u, 6, 1}
    };
    assert_multiline_template_token_ranges("return `a\rb\r\nc\nd`\r\n;\n",
            "`a\rb\r\nc\nd`", expected);
}

int main(int argc, char **argv) {
    TZrBool baselineOnly = ZR_FALSE;
    if (argc == 2 && strcmp(argv[1], "--baseline-only") == 0) {
        baselineOnly = ZR_TRUE;
    } else if (argc != 1) {
        return 2;
    }
    UNITY_BEGIN();
    RUN_TEST(test_script_root_starts_at_first_token);
    RUN_TEST(test_return_literal_includes_semicolon);
    RUN_TEST(test_literal_at_eof_without_newline);
    RUN_TEST(test_leading_and_trailing_trivia);
    RUN_TEST(test_return_expression_on_later_line);
    RUN_TEST(test_crlf_return_and_root_ranges);
    RUN_TEST(test_semicolon_token_before_crlf_excludes_both_newline_bytes);
    RUN_TEST(test_missing_return_semicolon_preserves_following_declaration);
    RUN_TEST(test_return_excludes_following_statement);
    RUN_TEST(test_empty_script_is_eos_point);
    RUN_TEST(test_trivia_only_script_is_eos_point);
    RUN_TEST(test_operandless_return_includes_semicolon);
    RUN_TEST(test_reference_return_preserves_ref_token_range);
    if (!baselineOnly) {
        RUN_TEST(test_multiline_template_token_coordinates_bare_cr);
        RUN_TEST(test_multiline_template_token_coordinates_lf);
        RUN_TEST(test_multiline_template_token_coordinates_crlf);
        RUN_TEST(test_multiline_template_token_coordinates_mixed_newlines);
    }
    return UNITY_END();
}
