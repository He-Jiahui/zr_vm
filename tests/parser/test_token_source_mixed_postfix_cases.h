#ifndef ZR_TEST_TOKEN_SOURCE_MIXED_POSTFIX_CASES_H
#define ZR_TEST_TOKEN_SOURCE_MIXED_POSTFIX_CASES_H

#include "zr_vm_core/gc.h"

typedef struct SZrMixedPostfixFixture {
    SZrSyntaxContextTable *table;
    SZrTokenStream *left;
    SZrTokenStream *right;
    SZrTokenStream *combined;
    SZrParserState parser;
    SZrAstNode *ast;
    SZrGcRootLease expansionNameRoot;
    TZrBool parserInitialized;
} SZrMixedPostfixFixture;

static SZrMixedPostfixFixture g_mixedPostfix;

static void mixed_postfix_cleanup(void) {
    if (g_mixedPostfix.ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_mixedPostfix.ast);
    }
    if (g_mixedPostfix.parserInitialized) {
        ZrParser_State_Free(&g_mixedPostfix.parser);
    }
    if (g_state != ZR_NULL) {
        ZrCore_Gc_RootLeaseRelease(g_state->global, &g_mixedPostfix.expansionNameRoot);
    }
    ZrParser_TokenStream_Release(g_mixedPostfix.combined);
    ZrParser_TokenStream_Release(g_mixedPostfix.right);
    ZrParser_TokenStream_Release(g_mixedPostfix.left);
    ZrParser_SyntaxContextTable_Release(g_mixedPostfix.table);
    memset(&g_mixedPostfix, 0, sizeof(g_mixedPostfix));
}

static void mixed_postfix_require_origin(const SZrTokenTree *tree,
                                         const char *name,
                                         TZrSize start, TZrSize end) {
    SZrTokenSpan span;
    const char *actualName;
    TZrSize nameLength = 0U;

    TEST_ASSERT_NOT_NULL_MESSAGE(tree, "Fixture prerequisite: original token missing");
    span = ZrParser_TokenTree_Span(tree);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenSpan_IsValid(span),
                             "Fixture prerequisite: original token Span invalid");
    actualName = ZrParser_TokenStream_SourceName(
            g_mixedPostfix.combined, span.sourceId, &nameLength);
    TEST_ASSERT_NOT_NULL_MESSAGE(actualName, "Fixture prerequisite: source identity missing");
    TEST_ASSERT_EQUAL_UINT64(strlen(name), nameLength);
    TEST_ASSERT_EQUAL_MEMORY(name, actualName, nameLength);
    TEST_ASSERT_EQUAL_UINT64(start, span.startOffset);
    TEST_ASSERT_EQUAL_UINT64(end, span.endOffset);
}

static TZrBool mixed_postfix_location_is(const SZrAstNode *node,
                                         const char *source,
                                         TZrSize start, TZrSize end) {
    return node != ZR_NULL && node->location.source != ZR_NULL &&
           strcmp(ZrCore_String_GetNativeString(node->location.source), source) == 0 &&
           node->location.start.offset == start && node->location.end.offset == end &&
           node->location.start.line == 1 && node->location.end.line == 1 &&
           node->location.start.column == (TZrInt32)start + 1 &&
           node->location.end.column == (TZrInt32)end + 1;
}

static void test_mixed_source_postfix_preserves_origins_and_diagnostic_anchors(void) {
    char leftSource[71];
    static const char rightSource[] = "(watcher); return 9;";
    SZrString *expansionName;
    SZrAstNode *primary;
    SZrAstNode *target;
    SZrAstNode *call;
    SZrAstNode *watcher;
    SZrAstNode *following;
    const SZrTokenTree *group;
    TZrBool targetMatches;
    TZrBool callMatches;
    TZrBool watcherMatches;
    TZrBool anchorMatches;
    TZrBool followingMatches;

    memset(leftSource, ' ', 64U);
    memcpy(leftSource + 64U, "target", 7U);
    g_mixedPostfix.table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_mixedPostfix.table);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(
            g_state, g_mixedPostfix.table, "left.zr", 7U,
            leftSource, strlen(leftSource), ZR_NULL, &g_mixedPostfix.left),
            "Fixture prerequisite: left source tokenization failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(
            g_state, g_mixedPostfix.table, "right.zr", 8U,
            rightSource, sizeof(rightSource) - 1U, ZR_NULL, &g_mixedPostfix.right),
            "Fixture prerequisite: right source tokenization failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Concat(
            g_mixedPostfix.left, g_mixedPostfix.right, ZR_NULL, &g_mixedPostfix.combined),
            "Fixture prerequisite: mixed source concatenation failed");
    TEST_ASSERT_TRUE(ZrParser_TokenStream_IsSealed(g_mixedPostfix.combined));
    mixed_postfix_require_origin(ZrParser_TokenStream_At(g_mixedPostfix.combined, 0U),
                                 "left.zr", 64U, 70U);
    group = ZrParser_TokenStream_At(g_mixedPostfix.combined, 1U);
    TEST_ASSERT_NOT_NULL(group);
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_TREE_GROUP, ZrParser_TokenTree_Kind(group));
    TEST_ASSERT_EQUAL_UINT64(1U, ZrParser_TokenTree_GroupChildCount(group));
    mixed_postfix_require_origin(group, "right.zr", 0U, 9U);
    mixed_postfix_require_origin(ZrParser_TokenTree_GroupChildAt(group, 0U),
                                 "right.zr", 1U, 8U);

    expansionName = ZrCore_String_CreateFromNative(g_state, "expansion.zr");
    TEST_ASSERT_NOT_NULL(expansionName);
    TEST_ASSERT_TRUE(ZrCore_Gc_RootLeaseAcquire(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(expansionName), &g_mixedPostfix.expansionNameRoot));
    g_mixedPostfix.parserInitialized = ZR_TRUE;
    ZrParser_State_InitWithTokenStream(&g_mixedPostfix.parser, g_state,
                                      g_mixedPostfix.combined, expansionName);
    g_mixedPostfix.parser.suppressErrorOutput = ZR_TRUE;
    g_mixedPostfix.ast = ZrParser_ParseWithState(&g_mixedPostfix.parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_mixedPostfix.ast, "Fixture prerequisite: mixed postfix parse failed");
    TEST_ASSERT_FALSE_MESSAGE(g_mixedPostfix.parser.hasError || g_mixedPostfix.parser.hasFatalError,
                              "Fixture prerequisite: mixed postfix parser error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_TK_EOS, g_mixedPostfix.parser.lexer->t.token,
                                  "Fixture prerequisite: incomplete mixed input consumption");
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_mixedPostfix.ast->type);
    TEST_ASSERT_NOT_NULL(g_mixedPostfix.ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(2U, g_mixedPostfix.ast->data.script.statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT,
            g_mixedPostfix.ast->data.script.statements->nodes[0]->type);
    primary = g_mixedPostfix.ast->data.script.statements->nodes[0]->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(primary);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, primary->type);
    target = primary->data.primaryExpression.property;
    TEST_ASSERT_NOT_NULL(target);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, target->type);
    TEST_ASSERT_EQUAL_STRING("target", ZrCore_String_GetNativeString(target->data.identifier.name));
    TEST_ASSERT_NOT_NULL(primary->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT64(1U, primary->data.primaryExpression.members->count);
    call = primary->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->type);
    TEST_ASSERT_NOT_NULL(call->data.functionCall.args);
    TEST_ASSERT_EQUAL_UINT64(1U, call->data.functionCall.args->count);
    watcher = call->data.functionCall.args->nodes[0];
    TEST_ASSERT_NOT_NULL(watcher);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, watcher->type);
    TEST_ASSERT_EQUAL_STRING("watcher", ZrCore_String_GetNativeString(watcher->data.identifier.name));
    following = g_mixedPostfix.ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(following);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, following->type);
    mixed_postfix_require_origin(ZrParser_TokenStream_At(g_mixedPostfix.combined, 0U),
                                 "left.zr", 64U, 70U);
    mixed_postfix_require_origin(ZrParser_TokenTree_GroupChildAt(group, 0U),
                                 "right.zr", 1U, 8U);

    targetMatches = mixed_postfix_location_is(target, "left.zr", 64U, 70U);
    callMatches = mixed_postfix_location_is(call, "right.zr", 0U, 9U);
    watcherMatches = mixed_postfix_location_is(watcher, "right.zr", 1U, 8U);
    anchorMatches = mixed_postfix_location_is(primary, "left.zr", 64U, 70U);
    followingMatches = mixed_postfix_location_is(following, "right.zr", 11U, 20U);
    if (!(targetMatches && callMatches && watcherMatches && anchorMatches && followingMatches)) {
        printf("Mixed postfix anchors target=%u call=%u argument=%u parent=%u following=%u parentRange=%llu..%llu\n",
               (unsigned)targetMatches, (unsigned)callMatches, (unsigned)watcherMatches,
               (unsigned)anchorMatches, (unsigned)followingMatches,
               (unsigned long long)primary->location.start.offset,
               (unsigned long long)primary->location.end.offset);
    }
    TEST_ASSERT_TRUE_MESSAGE(targetMatches && callMatches && watcherMatches && anchorMatches && followingMatches,
                             "Cross-source AST must retain real source anchors, not a fabricated envelope");
}

#endif
