#ifndef ZR_TEST_TOKEN_SOURCE_COMPOSITE_RANGE_CASES_H
#define ZR_TEST_TOKEN_SOURCE_COMPOSITE_RANGE_CASES_H

#include <stdio.h>

#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"

typedef struct SZrCompositeRangeFixture {
    SZrSyntaxContextTable *table;
    SZrTokenStream *left;
    SZrTokenStream *right;
    SZrTokenStream *stream;
    SZrParserState parser;
    SZrAstNode *ast;
    SZrGcRootLease entryNameRoot;
    TZrBool parserInitialized;
} SZrCompositeRangeFixture;

static SZrCompositeRangeFixture g_compositeRange;

static void token_source_composite_range_cleanup(void) {
    if (g_compositeRange.ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_compositeRange.ast);
    }
    if (g_compositeRange.parserInitialized) {
        ZrParser_State_Free(&g_compositeRange.parser);
    }
    if (g_state != ZR_NULL) {
        ZrCore_Gc_RootLeaseRelease(g_state->global, &g_compositeRange.entryNameRoot);
    }
    ZrParser_TokenStream_Release(g_compositeRange.stream);
    ZrParser_TokenStream_Release(g_compositeRange.right);
    ZrParser_TokenStream_Release(g_compositeRange.left);
    ZrParser_SyntaxContextTable_Release(g_compositeRange.table);
    memset(&g_compositeRange, 0, sizeof(g_compositeRange));
}

static void composite_range_begin(void) {
    token_source_composite_range_cleanup();
    g_compositeRange.table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_compositeRange.table,
                                 "Fixture prerequisite: composite context table allocation failed");
}

static TZrTokenSourceId composite_range_add_source(const char *name, const char *source) {
    TZrTokenSourceId id = ZR_TOKEN_SOURCE_INVALID;

    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_AddSource(
            g_compositeRange.stream, name, strlen(name), source, strlen(source), &id),
            "Fixture prerequisite: composite source storage failed");
    TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(ZR_TOKEN_SOURCE_INVALID, id,
                                         "Fixture prerequisite: composite source identity is invalid");
    return id;
}

static void composite_range_create_builder(void) {
    g_compositeRange.stream = ZrParser_TokenStream_Create(
            g_state->global, g_compositeRange.table, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_compositeRange.stream,
                                 "Fixture prerequisite: composite stream allocation failed");
}

static void composite_range_append_integer(TZrTokenSourceId source, TZrSize offset,
                                           TZrInt64 value, TZrSize expectedIndex) {
    SZrTokenNativeSemInfo info = {0};
    SZrTokenText raw = {source, offset, 1U};
    TZrSize index = ZR_MAX_SIZE;

    info.token = ZR_TK_INTEGER;
    info.intValue = value;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_AppendLiteral(
            g_compositeRange.stream, info, raw,
            make_span(source, offset, offset + 1U, (TZrInt32)offset + 1, (TZrInt32)offset + 2),
            ZrParser_SyntaxContext_TableRoot(g_compositeRange.table),
            ZR_NULL, 0U, ZR_NULL, 0U, &index),
            "Fixture prerequisite: composite integer append failed");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(expectedIndex, index,
                                    "Fixture prerequisite: composite integer root order changed");
}

static void composite_range_append_punct(TZrTokenSourceId source, TZrSize offset,
                                         EZrToken token, TZrUInt32 codepoint, TZrSize expectedIndex) {
    SZrTokenNativeSemInfo info = {0};
    SZrTokenText raw = {source, offset, 1U};
    TZrSize index = ZR_MAX_SIZE;

    info.token = token;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_AppendPunct(
            g_compositeRange.stream, info, raw, codepoint, ZR_TOKEN_SPACING_ALONE,
            make_span(source, offset, offset + 1U, (TZrInt32)offset + 1, (TZrInt32)offset + 2),
            ZrParser_SyntaxContext_TableRoot(g_compositeRange.table),
            ZR_NULL, 0U, ZR_NULL, 0U, &index),
            "Fixture prerequisite: composite punctuation append failed");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(expectedIndex, index,
                                    "Fixture prerequisite: composite punctuation root order changed");
}

static void composite_range_require_span(SZrTokenSpan span, TZrTokenSourceId source,
                                          const char *name, TZrSize start, TZrSize end) {
    const char *actualName;
    TZrSize nameLength = 0U;

    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenSpan_IsValid(span),
                             "Fixture prerequisite: original composite span is invalid");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(source, span.sourceId,
                                    "Fixture prerequisite: original composite source identity changed");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(start, span.startOffset,
                                    "Fixture prerequisite: original composite span start changed");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(end, span.endOffset,
                                    "Fixture prerequisite: original composite span end changed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, span.startLine,
                                  "Fixture prerequisite: original composite start line changed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, span.endLine,
                                  "Fixture prerequisite: original composite end line changed");
    TEST_ASSERT_EQUAL_INT_MESSAGE((TZrInt32)start + 1, span.startColumn,
                                  "Fixture prerequisite: original composite start column changed");
    TEST_ASSERT_EQUAL_INT_MESSAGE((TZrInt32)end + 1, span.endColumn,
                                  "Fixture prerequisite: original composite end column changed");
    actualName = ZrParser_TokenStream_SourceName(g_compositeRange.stream, source, &nameLength);
    TEST_ASSERT_NOT_NULL_MESSAGE(actualName,
                                 "Fixture prerequisite: original composite source name is missing");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(strlen(name), nameLength,
                                    "Fixture prerequisite: original composite source name length changed");
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(name, actualName, nameLength,
                                     "Fixture prerequisite: original composite source name changed");
}

static void composite_range_require_tree(const SZrTokenTree *tree, TZrTokenSourceId source,
                                          const char *name, TZrSize start, TZrSize end) {
    SZrSyntaxContext context;

    TEST_ASSERT_NOT_NULL_MESSAGE(tree, "Fixture prerequisite: original composite tree is missing");
    composite_range_require_span(ZrParser_TokenTree_Span(tree), source, name, start, end);
    context = ZrParser_TokenTree_Context(tree);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SyntaxContext_IsValid(context) &&
            ZrParser_SyntaxContext_Equal(context, ZrParser_SyntaxContext_TableRoot(g_compositeRange.table)),
            "Fixture prerequisite: original composite context changed");
}

static void composite_range_require_leaf(const SZrTokenTree *tree, TZrTokenSourceId source,
                                          const char *name, TZrSize start, TZrSize end,
                                          EZrTokenTreeKind kind, EZrToken token,
                                          const char *raw, TZrInt64 integer) {
    SZrTokenNativeSemInfo info;
    SZrTokenText text;
    const char *bytes;

    composite_range_require_tree(tree, source, name, start, end);
    TEST_ASSERT_EQUAL_INT_MESSAGE(kind, ZrParser_TokenTree_Kind(tree),
                                  "Fixture prerequisite: original composite tree kind changed");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenTree_NativeSemInfo(tree, &info),
                             "Fixture prerequisite: original composite native payload is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(token, info.token,
                                  "Fixture prerequisite: original composite native token changed");
    if (token == ZR_TK_INTEGER) {
        TEST_ASSERT_EQUAL_INT64_MESSAGE(integer, info.intValue,
                                        "Fixture prerequisite: original composite integer value changed");
    }
    text = ZrParser_TokenTree_Text(tree);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(source, text.sourceId,
                                    "Fixture prerequisite: original composite raw source changed");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(start, text.offset,
                                    "Fixture prerequisite: original composite raw offset changed");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(strlen(raw), text.length,
                                    "Fixture prerequisite: original composite raw length changed");
    bytes = ZrParser_TokenStream_TextBytes(g_compositeRange.stream, text);
    TEST_ASSERT_NOT_NULL_MESSAGE(bytes, "Fixture prerequisite: original composite raw bytes are missing");
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(raw, bytes, text.length,
                                     "Fixture prerequisite: original composite raw spelling changed");
}

static SZrAstNodeArray *composite_range_parse(TZrSize statementCount) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "composite_entry.zr");

    TEST_ASSERT_NOT_NULL_MESSAGE(name, "Fixture prerequisite: composite entry name allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(name), &g_compositeRange.entryNameRoot),
            "Fixture prerequisite: composite entry name rooting failed");
    g_compositeRange.parserInitialized = ZR_TRUE;
    ZrParser_State_InitWithTokenStream(&g_compositeRange.parser, g_state, g_compositeRange.stream, name);
    TEST_ASSERT_FALSE_MESSAGE(g_compositeRange.parser.hasError || g_compositeRange.parser.hasFatalError,
                              "Fixture prerequisite: composite parser initialization failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_compositeRange.parser.lexer,
                                 "Fixture prerequisite: composite lexer is missing");
    g_compositeRange.parser.suppressErrorOutput = ZR_TRUE;
    g_compositeRange.ast = ZrParser_ParseWithState(&g_compositeRange.parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_compositeRange.ast,
                                 "Fixture prerequisite: real composite source parsing failed");
    TEST_ASSERT_FALSE_MESSAGE(g_compositeRange.parser.hasError || g_compositeRange.parser.hasFatalError ||
            g_compositeRange.parser.lexer->t.hasLexError,
            "Fixture prerequisite: real composite source reported a parse error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_TK_EOS, g_compositeRange.parser.lexer->t.token,
                                  "Fixture prerequisite: composite parsing did not reach EOS");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_SCRIPT, g_compositeRange.ast->type,
                                  "Fixture prerequisite: composite source did not produce a SCRIPT");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_compositeRange.ast->data.script.statements,
                                 "Fixture prerequisite: composite statements are missing");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(statementCount, g_compositeRange.ast->data.script.statements->count,
                                    "Fixture prerequisite: composite statement count changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_compositeRange.ast->data.script.statements->nodes,
                                 "Fixture prerequisite: composite statement storage is missing");
    return g_compositeRange.ast->data.script.statements;
}

static TZrBool composite_range_location_is(const SZrAstNode *node, const char *name,
                                           TZrSize start, TZrSize end) {
    return node != ZR_NULL && node->location.source != ZR_NULL &&
           strcmp(ZrCore_String_GetNativeString(node->location.source), name) == 0 &&
           node->location.start.offset == start && node->location.end.offset == end &&
           node->location.start.line == 1 && node->location.end.line == 1 &&
           node->location.start.column == (TZrInt32)start + 1 &&
           node->location.end.column == (TZrInt32)end + 1;
}

static SZrAstNode *composite_range_require_binary(SZrAstNodeArray *statements) {
    SZrAstNode *statement = statements->nodes[0];
    SZrAstNode *binary;

    TEST_ASSERT_NOT_NULL_MESSAGE(statement, "Fixture prerequisite: composite expression statement is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_EXPRESSION_STATEMENT, statement->type,
                                  "Fixture prerequisite: composite first statement is not an expression");
    binary = statement->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL_MESSAGE(binary, "Fixture prerequisite: composite binary expression is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_BINARY_EXPRESSION, binary->type,
                                  "Fixture prerequisite: composite expression is not a BINARY");
    TEST_ASSERT_NOT_NULL_MESSAGE(binary->data.binaryExpression.op.op,
                                 "Fixture prerequisite: composite binary operator is missing");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("+", binary->data.binaryExpression.op.op,
                                     "Fixture prerequisite: composite binary operator changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(binary->data.binaryExpression.left,
                                 "Fixture prerequisite: composite left operand is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(binary->data.binaryExpression.right,
                                 "Fixture prerequisite: composite right operand is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_INTEGER_LITERAL, binary->data.binaryExpression.left->type,
                                  "Fixture prerequisite: composite left operand is not an integer");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_INTEGER_LITERAL, binary->data.binaryExpression.right->type,
                                  "Fixture prerequisite: composite right operand is not an integer");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(1, binary->data.binaryExpression.left->data.integerLiteral.value,
                                   "Fixture prerequisite: composite left operand value changed");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(2, binary->data.binaryExpression.right->data.integerLiteral.value,
                                   "Fixture prerequisite: composite right operand value changed");
    return binary;
}

static void composite_range_require_binary_input(TZrTokenSourceId operandSource,
                                                  TZrTokenSourceId operatorSource,
                                                  const char *operandName, const char *operatorName,
                                                  TZrSize left, TZrSize plus, TZrSize right, TZrSize semi) {
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(4U, ZrParser_TokenStream_Count(g_compositeRange.stream),
                                    "Fixture prerequisite: composite binary root count changed");
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 0U),
            operandSource, operandName, left, left + 1U, ZR_TOKEN_TREE_LITERAL, ZR_TK_INTEGER, "1", 1);
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 1U),
            operatorSource, operatorName, plus, plus + 1U, ZR_TOKEN_TREE_PUNCT, ZR_TK_PLUS, "+", 0);
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 2U),
            operandSource, operandName, right, right + 1U, ZR_TOKEN_TREE_LITERAL, ZR_TK_INTEGER, "2", 2);
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 3U),
            operandSource, operandName, semi, semi + 1U, ZR_TOKEN_TREE_PUNCT, ZR_TK_SEMICOLON, ";", 0);
}

static void test_operator_only_foreign_source_anchors_binary_parent(void) {
    TZrTokenSourceId operandSource;
    TZrTokenSourceId operatorSource;
    SZrAstNode *binary;
    TZrBool passed;

    composite_range_begin();
    composite_range_create_builder();
    /* Only the operator belongs to B; both operand AST leaves belong to A. */
    operandSource = composite_range_add_source("operands.zr", "1;2;");
    operatorSource = composite_range_add_source("operator.zr", "+");
    TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(operandSource, operatorSource,
                                         "Fixture prerequisite: operand and operator sources must be distinct");
    composite_range_append_integer(operandSource, 0U, 1, 0U);
    composite_range_append_punct(operatorSource, 0U, ZR_TK_PLUS, '+', 1U);
    composite_range_append_integer(operandSource, 2U, 2, 2U);
    composite_range_append_punct(operandSource, 3U, ZR_TK_SEMICOLON, ';', 3U);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Seal(g_compositeRange.stream),
                             "Fixture prerequisite: operator-only composite stream sealing failed");
    composite_range_require_binary_input(operandSource, operatorSource, "operands.zr", "operator.zr",
                                         0U, 0U, 2U, 3U);
    binary = composite_range_require_binary(composite_range_parse(1U));
    composite_range_require_binary_input(operandSource, operatorSource, "operands.zr", "operator.zr",
                                         0U, 0U, 2U, 3U);
    TEST_ASSERT_TRUE_MESSAGE(composite_range_location_is(binary->data.binaryExpression.left, "operands.zr", 0U, 1U),
                             "Fixture prerequisite: operator-only left operand anchor changed");
    TEST_ASSERT_TRUE_MESSAGE(composite_range_location_is(binary->data.binaryExpression.right, "operands.zr", 2U, 3U),
                             "Fixture prerequisite: operator-only right operand anchor changed");
    passed = composite_range_location_is(binary, "operands.zr", 0U, 1U);
    if (!passed) {
        printf("Operator-only composite parent=%llu..%llu expected=0..1\n",
               (unsigned long long)binary->location.start.offset,
               (unsigned long long)binary->location.end.offset);
        TEST_FAIL_MESSAGE("An operator-only foreign source must prevent a fabricated parent envelope");
    }
}

static void composite_range_require_named_postfix_input(TZrTokenSourceId left, TZrTokenSourceId right) {
    const SZrTokenTree *group;

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(6U, ZrParser_TokenStream_Count(g_compositeRange.stream),
                                    "Fixture prerequisite: same-name composite root count changed");
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 0U),
            left, "same.zr", 64U, 70U, ZR_TOKEN_TREE_IDENT, ZR_TK_IDENTIFIER, "target", 0);
    group = ZrParser_TokenStream_At(g_compositeRange.stream, 1U);
    composite_range_require_tree(group, right, "same.zr", 0U, 9U);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_TOKEN_TREE_GROUP, ZrParser_TokenTree_Kind(group),
                                  "Fixture prerequisite: same-name call group is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_TOKEN_DELIMITER_PAREN, ZrParser_TokenTree_GroupDelimiter(group),
                                  "Fixture prerequisite: same-name call delimiter changed");
    composite_range_require_span(ZrParser_TokenTree_GroupOpenSpan(group), right, "same.zr", 0U, 1U);
    composite_range_require_span(ZrParser_TokenTree_GroupCloseSpan(group), right, "same.zr", 8U, 9U);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1U, ZrParser_TokenTree_GroupChildCount(group),
                                    "Fixture prerequisite: same-name call child count changed");
    composite_range_require_leaf(ZrParser_TokenTree_GroupChildAt(group, 0U),
            right, "same.zr", 1U, 8U, ZR_TOKEN_TREE_IDENT, ZR_TK_IDENTIFIER, "watcher", 0);
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 2U),
            right, "same.zr", 9U, 10U, ZR_TOKEN_TREE_PUNCT, ZR_TK_SEMICOLON, ";", 0);
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 3U),
            right, "same.zr", 11U, 17U, ZR_TOKEN_TREE_IDENT, ZR_TK_RETURN, "return", 0);
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 4U),
            right, "same.zr", 18U, 19U, ZR_TOKEN_TREE_LITERAL, ZR_TK_INTEGER, "9", 9);
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 5U),
            right, "same.zr", 19U, 20U, ZR_TOKEN_TREE_PUNCT, ZR_TK_SEMICOLON, ";", 0);
}

static void test_same_named_sources_keep_distinct_postfix_parent_anchor(void) {
    char leftSource[71];
    static const char rightSource[] = "(watcher); return 9;";
    TZrTokenSourceId left;
    TZrTokenSourceId right;
    SZrAstNodeArray *statements;
    SZrAstNode *primary;
    SZrAstNode *target;
    SZrAstNode *call;
    SZrAstNode *watcher;
    SZrAstNode *following;
    TZrBool targetMatches, callMatches, watcherMatches, followingMatches, parentMatches;

    composite_range_begin();
    memset(leftSource, ' ', 64U);
    memcpy(leftSource + 64U, "target", 7U);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(
            g_state, g_compositeRange.table, "same.zr", 7U,
            leftSource, strlen(leftSource), ZR_NULL, &g_compositeRange.left),
            "Fixture prerequisite: same-name left tokenization failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(
            g_state, g_compositeRange.table, "same.zr", 7U,
            rightSource, sizeof(rightSource) - 1U, ZR_NULL, &g_compositeRange.right),
            "Fixture prerequisite: same-name right tokenization failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Concat(
            g_compositeRange.left, g_compositeRange.right, ZR_NULL, &g_compositeRange.stream),
            "Fixture prerequisite: same-name concatenation failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_IsSealed(g_compositeRange.stream),
                             "Fixture prerequisite: same-name composite stream is not sealed");
    left = ZrParser_TokenTree_Span(ZrParser_TokenStream_At(g_compositeRange.stream, 0U)).sourceId;
    right = ZrParser_TokenTree_Span(ZrParser_TokenStream_At(g_compositeRange.stream, 1U)).sourceId;
    TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(left, right,
                                         "Fixture prerequisite: equal names must have distinct physical source IDs");
    composite_range_require_named_postfix_input(left, right);
    statements = composite_range_parse(2U);
    TEST_ASSERT_NOT_NULL_MESSAGE(statements->nodes[0],
                                 "Fixture prerequisite: same-name expression statement is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_EXPRESSION_STATEMENT, statements->nodes[0]->type,
                                  "Fixture prerequisite: same-name first statement is not an expression");
    primary = statements->nodes[0]->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL_MESSAGE(primary, "Fixture prerequisite: same-name PRIMARY is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_PRIMARY_EXPRESSION, primary->type,
                                  "Fixture prerequisite: same-name expression is not a PRIMARY");
    target = primary->data.primaryExpression.property;
    TEST_ASSERT_NOT_NULL_MESSAGE(target, "Fixture prerequisite: same-name target is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_IDENTIFIER_LITERAL, target->type,
                                  "Fixture prerequisite: same-name target is not an identifier");
    TEST_ASSERT_NOT_NULL_MESSAGE(target->data.identifier.name,
                                 "Fixture prerequisite: same-name target spelling is missing");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("target", ZrCore_String_GetNativeString(target->data.identifier.name),
                                     "Fixture prerequisite: same-name target spelling changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(primary->data.primaryExpression.members,
                                 "Fixture prerequisite: same-name PRIMARY members are missing");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1U, primary->data.primaryExpression.members->count,
                                    "Fixture prerequisite: same-name PRIMARY member count changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(primary->data.primaryExpression.members->nodes,
                                 "Fixture prerequisite: same-name PRIMARY member storage is missing");
    call = primary->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(call, "Fixture prerequisite: same-name CALL is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_FUNCTION_CALL, call->type,
                                  "Fixture prerequisite: same-name member is not a CALL");
    TEST_ASSERT_NOT_NULL_MESSAGE(call->data.functionCall.args,
                                 "Fixture prerequisite: same-name CALL arguments are missing");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1U, call->data.functionCall.args->count,
                                    "Fixture prerequisite: same-name CALL argument count changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(call->data.functionCall.args->nodes,
                                 "Fixture prerequisite: same-name CALL argument storage is missing");
    watcher = call->data.functionCall.args->nodes[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(watcher, "Fixture prerequisite: same-name watcher is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_IDENTIFIER_LITERAL, watcher->type,
                                  "Fixture prerequisite: same-name watcher is not an identifier");
    TEST_ASSERT_NOT_NULL_MESSAGE(watcher->data.identifier.name,
                                 "Fixture prerequisite: same-name watcher spelling is missing");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("watcher", ZrCore_String_GetNativeString(watcher->data.identifier.name),
                                     "Fixture prerequisite: same-name watcher spelling changed");
    following = statements->nodes[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(following, "Fixture prerequisite: same-name following RETURN is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_RETURN_STATEMENT, following->type,
                                  "Fixture prerequisite: same-name following statement is not a RETURN");
    TEST_ASSERT_NOT_NULL_MESSAGE(following->data.returnStatement.expr,
                                 "Fixture prerequisite: same-name following RETURN value is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_INTEGER_LITERAL, following->data.returnStatement.expr->type,
                                  "Fixture prerequisite: same-name following RETURN value is not an integer");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(9, following->data.returnStatement.expr->data.integerLiteral.value,
                                   "Fixture prerequisite: same-name following RETURN value changed");
    composite_range_require_named_postfix_input(left, right);
    targetMatches = composite_range_location_is(target, "same.zr", 64U, 70U);
    TEST_ASSERT_TRUE_MESSAGE(targetMatches, "Fixture prerequisite: same-name target anchor changed");
    callMatches = composite_range_location_is(call, "same.zr", 0U, 9U);
    TEST_ASSERT_TRUE_MESSAGE(callMatches, "Fixture prerequisite: same-name CALL anchor changed");
    watcherMatches = composite_range_location_is(watcher, "same.zr", 1U, 8U);
    TEST_ASSERT_TRUE_MESSAGE(watcherMatches, "Fixture prerequisite: same-name watcher anchor changed");
    followingMatches = composite_range_location_is(following, "same.zr", 11U, 20U);
    TEST_ASSERT_TRUE_MESSAGE(followingMatches, "Fixture prerequisite: same-name following RETURN anchor changed");
    TEST_ASSERT_TRUE_MESSAGE(composite_range_location_is(following->data.returnStatement.expr, "same.zr", 18U, 19U),
                             "Fixture prerequisite: same-name following RETURN value anchor changed");
    parentMatches = composite_range_location_is(primary, "same.zr", 64U, 70U);
    if (!parentMatches) {
        printf("Same-name composite target=%u call=%u argument=%u following=%u parent=%u parentRange=%llu..%llu\n",
               (unsigned)targetMatches, (unsigned)callMatches, (unsigned)watcherMatches,
               (unsigned)followingMatches, (unsigned)parentMatches,
               (unsigned long long)primary->location.start.offset,
               (unsigned long long)primary->location.end.offset);
        TEST_FAIL_MESSAGE("Equal display names must not merge distinct physical token sources");
    }
}

static void test_reordered_single_source_anchors_first_native_token(void) {
    TZrTokenSourceId source;
    SZrAstNode *binary;
    TZrBool passed;

    composite_range_begin();
    composite_range_create_builder();
    /* One source record owns every reordered token, including both operands. */
    source = composite_range_add_source("reordered.zr", "2; 1+9;");
    composite_range_append_integer(source, 3U, 1, 0U);
    composite_range_append_punct(source, 4U, ZR_TK_PLUS, '+', 1U);
    composite_range_append_integer(source, 0U, 2, 2U);
    composite_range_append_punct(source, 1U, ZR_TK_SEMICOLON, ';', 3U);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Seal(g_compositeRange.stream),
                             "Fixture prerequisite: reordered stream sealing failed");
    composite_range_require_binary_input(source, source, "reordered.zr", "reordered.zr",
                                         3U, 4U, 0U, 1U);
    binary = composite_range_require_binary(composite_range_parse(1U));
    composite_range_require_binary_input(source, source, "reordered.zr", "reordered.zr",
                                         3U, 4U, 0U, 1U);
    TEST_ASSERT_TRUE_MESSAGE(composite_range_location_is(binary->data.binaryExpression.left, "reordered.zr", 3U, 4U),
                             "Fixture prerequisite: reordered left operand anchor changed");
    TEST_ASSERT_TRUE_MESSAGE(composite_range_location_is(binary->data.binaryExpression.right, "reordered.zr", 0U, 1U),
                             "Fixture prerequisite: reordered right operand anchor changed");
    passed = composite_range_location_is(binary, "reordered.zr", 3U, 4U);
    if (!passed) {
        printf("Reordered composite parent=%llu..%llu expected=3..4\n",
               (unsigned long long)binary->location.start.offset,
               (unsigned long long)binary->location.end.offset);
        TEST_FAIL_MESSAGE("Reordered tokens from one source must retain their first native anchor");
    }
}

static void composite_range_require_template_input(TZrTokenSourceId source) {
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(3U, ZrParser_TokenStream_Count(g_compositeRange.stream),
                                    "Fixture prerequisite: embedded template root count changed");
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 0U),
            source, "embedded.zr", 0U, 6U, ZR_TOKEN_TREE_IDENT, ZR_TK_RETURN, "return", 0);
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 1U),
            source, "embedded.zr", 7U, 15U, ZR_TOKEN_TREE_LITERAL, ZR_TK_TEMPLATE_STRING, "`a${1}b`", 0);
    composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, 2U),
            source, "embedded.zr", 15U, 16U, ZR_TOKEN_TREE_PUNCT, ZR_TK_SEMICOLON, ";", 0);
}

static void test_embedded_template_retains_whole_origin_span(void) {
    static const char source[] = "return `a${1}b`;";
    TZrTokenSourceId sourceId;
    SZrAstNodeArray *statements;
    SZrAstNode *templateNode;
    SZrAstNodeArray *segments;
    SZrAstNode *embedded;

    composite_range_begin();
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(
            g_state, g_compositeRange.table, "embedded.zr", sizeof("embedded.zr") - 1U,
            source, sizeof(source) - 1U, ZR_NULL, &g_compositeRange.stream),
            "Fixture prerequisite: embedded template tokenization failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_IsSealed(g_compositeRange.stream),
                             "Fixture prerequisite: embedded template stream is not sealed");
    sourceId = ZrParser_TokenTree_Span(ZrParser_TokenStream_At(g_compositeRange.stream, 1U)).sourceId;
    composite_range_require_template_input(sourceId);
    statements = composite_range_parse(1U);
    TEST_ASSERT_NOT_NULL_MESSAGE(statements->nodes[0],
                                 "Fixture prerequisite: embedded template RETURN is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_RETURN_STATEMENT, statements->nodes[0]->type,
                                  "Fixture prerequisite: embedded template statement is not a RETURN");
    templateNode = statements->nodes[0]->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL_MESSAGE(templateNode, "Fixture prerequisite: embedded template expression is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_TEMPLATE_STRING_LITERAL, templateNode->type,
                                  "Fixture prerequisite: embedded expression is not a template");
    segments = templateNode->data.templateStringLiteral.segments;
    TEST_ASSERT_NOT_NULL_MESSAGE(segments, "Fixture prerequisite: embedded template segments are missing");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(3U, segments->count,
                                    "Fixture prerequisite: embedded template segment count changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(segments->nodes,
                                 "Fixture prerequisite: embedded template segment storage is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(segments->nodes[0], "Fixture prerequisite: first template segment is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(segments->nodes[1], "Fixture prerequisite: interpolation segment is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(segments->nodes[2], "Fixture prerequisite: last template segment is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_STRING_LITERAL, segments->nodes[0]->type,
                                  "Fixture prerequisite: first template segment is not text");
    TEST_ASSERT_NOT_NULL_MESSAGE(segments->nodes[0]->data.stringLiteral.value,
                                 "Fixture prerequisite: first template text is missing");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("a", ZrCore_String_GetNativeString(segments->nodes[0]->data.stringLiteral.value),
                                     "Fixture prerequisite: first template text changed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_INTERPOLATED_SEGMENT, segments->nodes[1]->type,
                                  "Fixture prerequisite: middle template segment is not interpolation");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_STRING_LITERAL, segments->nodes[2]->type,
                                  "Fixture prerequisite: last template segment is not text");
    TEST_ASSERT_NOT_NULL_MESSAGE(segments->nodes[2]->data.stringLiteral.value,
                                 "Fixture prerequisite: last template text is missing");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("b", ZrCore_String_GetNativeString(segments->nodes[2]->data.stringLiteral.value),
                                     "Fixture prerequisite: last template text changed");
    embedded = segments->nodes[1]->data.interpolatedSegment.expression;
    TEST_ASSERT_NOT_NULL_MESSAGE(embedded, "Fixture prerequisite: real embedded expression is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_INTEGER_LITERAL, embedded->type,
                                  "Fixture prerequisite: embedded expression is not an integer");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(1, embedded->data.integerLiteral.value,
                                   "Fixture prerequisite: embedded integer value changed");
    composite_range_require_template_input(sourceId);
    TEST_ASSERT_NOT_NULL_MESSAGE(templateNode->ownership,
                                 "Fixture prerequisite: embedded template AST owner is missing");
    TEST_ASSERT_EQUAL_PTR(templateNode->ownership, embedded->ownership);
    /* The embedded cursor inherits the whole token, not the local digit offset. */
    TEST_ASSERT_TRUE_MESSAGE(composite_range_location_is(templateNode, "embedded.zr", 7U, 15U) &&
            composite_range_location_is(embedded, "embedded.zr", 7U, 15U),
            "Embedded template expressions must retain their whole original token anchor");
}

#endif
