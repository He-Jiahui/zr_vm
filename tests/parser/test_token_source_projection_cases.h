#ifndef ZR_TEST_TOKEN_SOURCE_PROJECTION_CASES_H
#define ZR_TEST_TOKEN_SOURCE_PROJECTION_CASES_H

#include <stdlib.h>

#include "parser/parser_internal.h"
#include "zr_vm_core/gc.h"

enum {
    ZR_TEST_PROJECTION_NAME_LENGTH = 64 * 1024,
    ZR_TEST_PROJECTION_PARSER_HEAP_BYTES = 8 * 1024
};

typedef struct SZrTokenSourceProjectionFixture {
    SZrSyntaxContextTable *table;
    SZrTokenStream *left;
    SZrTokenStream *right;
    SZrTokenStream *combined;
    SZrParserState parser;
    SZrAstNode *ast;
    SZrGcRootLease entryNameRoot;
    SZrParserComptimeBudget budget;
    TZrChar *longName;
    TZrBool parserInitialized;
} SZrTokenSourceProjectionFixture;

static SZrTokenSourceProjectionFixture g_tokenSourceProjection;

static void token_source_projection_free_parser(void) {
    if (g_tokenSourceProjection.parserInitialized) {
        ZrParser_State_Free(&g_tokenSourceProjection.parser);
        g_tokenSourceProjection.parserInitialized = ZR_FALSE;
        memset(&g_tokenSourceProjection.parser, 0, sizeof(g_tokenSourceProjection.parser));
    }
}

static void token_source_projection_release_inputs(void) {
    ZrParser_TokenStream_Release(g_tokenSourceProjection.combined);
    g_tokenSourceProjection.combined = ZR_NULL;
    ZrParser_TokenStream_Release(g_tokenSourceProjection.right);
    g_tokenSourceProjection.right = ZR_NULL;
    ZrParser_TokenStream_Release(g_tokenSourceProjection.left);
    g_tokenSourceProjection.left = ZR_NULL;
    ZrParser_SyntaxContextTable_Release(g_tokenSourceProjection.table);
    g_tokenSourceProjection.table = ZR_NULL;
}

static void token_source_projection_cleanup(void) {
    if (g_tokenSourceProjection.ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_tokenSourceProjection.ast);
        g_tokenSourceProjection.ast = ZR_NULL;
    }
    token_source_projection_free_parser();
    if (g_state != ZR_NULL) {
        ZrCore_Gc_RootLeaseRelease(g_state->global, &g_tokenSourceProjection.entryNameRoot);
    }
    token_source_projection_release_inputs();
    free(g_tokenSourceProjection.longName);
    memset(&g_tokenSourceProjection, 0, sizeof(g_tokenSourceProjection));
}

static SZrString *token_source_projection_entry_name(void) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "projection_entry.zr");

    TEST_ASSERT_NOT_NULL_MESSAGE(name, "Fixture prerequisite: projection entry name allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(name), &g_tokenSourceProjection.entryNameRoot),
            "Fixture prerequisite: projection entry name rooting failed");
    return name;
}

static void token_source_projection_from_text(
        const TZrChar *name, TZrSize nameLength, const TZrChar *source, SZrTokenStream **outStream) {
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(
            g_state, g_tokenSourceProjection.table, name, nameLength,
            source, strlen(source), ZR_NULL, outStream),
            "Fixture prerequisite: projection source tokenization failed");
    TEST_ASSERT_NOT_NULL(*outStream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_IsSealed(*outStream));
}

static void token_source_projection_make_pair(const TZrChar *left, const TZrChar *right) {
    token_source_projection_cleanup();
    g_tokenSourceProjection.table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_tokenSourceProjection.table,
                                 "Fixture prerequisite: projection context table allocation failed");
    token_source_projection_from_text("left_projection.zr", sizeof("left_projection.zr") - 1U,
                                      left, &g_tokenSourceProjection.left);
    token_source_projection_from_text("right_projection.zr", sizeof("right_projection.zr") - 1U,
                                      right, &g_tokenSourceProjection.right);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Concat(
            g_tokenSourceProjection.left, g_tokenSourceProjection.right,
            ZR_NULL, &g_tokenSourceProjection.combined),
            "Fixture prerequisite: projection stream concatenation failed");
    TEST_ASSERT_NOT_NULL(g_tokenSourceProjection.combined);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_IsSealed(g_tokenSourceProjection.combined));
}

static SZrTokenSpan token_source_projection_origin(
        const SZrTokenTree *tree, const TZrChar *expectedName) {
    SZrTokenSpan span;
    const TZrChar *name;
    TZrSize nameLength = 0U;

    TEST_ASSERT_NOT_NULL_MESSAGE(tree, "Fixture prerequisite: original projection token missing");
    span = ZrParser_TokenTree_Span(tree);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenSpan_IsValid(span),
                             "Fixture prerequisite: original projection span invalid");
    name = ZrParser_TokenStream_SourceName(g_tokenSourceProjection.combined, span.sourceId, &nameLength);
    TEST_ASSERT_NOT_NULL_MESSAGE(name, "Fixture prerequisite: original projection source name missing");
    TEST_ASSERT_EQUAL_UINT64(strlen(expectedName), nameLength);
    TEST_ASSERT_EQUAL_MEMORY(expectedName, name, nameLength);
    return span;
}

static TZrBool token_source_projection_spans_equal(SZrTokenSpan left, SZrTokenSpan right) {
    return left.sourceId == right.sourceId &&
           left.startOffset == right.startOffset && left.endOffset == right.endOffset &&
           left.startLine == right.startLine && left.startColumn == right.startColumn &&
           left.endLine == right.endLine && left.endColumn == right.endColumn;
}

static TZrBool token_source_projection_location_matches(
        SZrFileRange range, const TZrChar *name, SZrTokenSpan span) {
    return range.source != ZR_NULL &&
           strcmp(ZrCore_String_GetNativeString(range.source), name) == 0 &&
           range.start.offset == span.startOffset && range.end.offset == span.endOffset &&
           range.start.line == span.startLine && range.start.column == span.startColumn &&
           range.end.line == span.endLine && range.end.column == span.endColumn;
}

static void test_token_source_projection_is_pure_across_lookahead(void) {
    SZrString *entryName;
    SZrLexState *lexer;
    SZrTokenSpan left;
    SZrTokenSpan right;
    SZrFileRange before;
    SZrFileRange afterLookahead;
    SZrFileRange afterConsume;
    TZrBool pure;

    token_source_projection_make_pair("  left", "\n right");
    TEST_ASSERT_EQUAL_UINT64(2U, ZrParser_TokenStream_Count(g_tokenSourceProjection.combined));
    left = token_source_projection_origin(
            ZrParser_TokenStream_At(g_tokenSourceProjection.combined, 0U), "left_projection.zr");
    right = token_source_projection_origin(
            ZrParser_TokenStream_At(g_tokenSourceProjection.combined, 1U), "right_projection.zr");
    TEST_ASSERT_NOT_EQUAL_UINT32(left.sourceId, right.sourceId);
    TEST_ASSERT_EQUAL_UINT64(2U, left.startOffset);
    TEST_ASSERT_EQUAL_UINT64(6U, left.endOffset);
    TEST_ASSERT_EQUAL_INT(1, left.startLine);
    TEST_ASSERT_EQUAL_UINT64(2U, right.startOffset);
    TEST_ASSERT_EQUAL_UINT64(7U, right.endOffset);
    TEST_ASSERT_EQUAL_INT(2, right.startLine);
    entryName = token_source_projection_entry_name();
    g_tokenSourceProjection.parserInitialized = ZR_TRUE;
    ZrParser_State_InitWithTokenStream(&g_tokenSourceProjection.parser, g_state,
                                      g_tokenSourceProjection.combined, entryName);
    TEST_ASSERT_FALSE_MESSAGE(g_tokenSourceProjection.parser.hasError,
                              "Fixture prerequisite: projection parser initialization failed");
    lexer = g_tokenSourceProjection.parser.lexer;
    TEST_ASSERT_NOT_NULL(lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer->t.token);
    TEST_ASSERT_FALSE(lexer->t.hasLexError);
    before = get_current_token_location(&g_tokenSourceProjection.parser);
    pure = lexer->sourceName == entryName && token_source_projection_spans_equal(lexer->t.span, left);

    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, ZrParser_Lexer_Lookahead(lexer));
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, ZrParser_Lexer_Lookahead(lexer));
    TEST_ASSERT_FALSE(lexer->lookahead.hasLexError);
    afterLookahead = get_current_token_location(&g_tokenSourceProjection.parser);
    pure = pure && lexer->sourceName == entryName &&
           token_source_projection_spans_equal(lexer->t.span, left) &&
           token_source_projection_spans_equal(lexer->lookahead.span, right);

    ZrParser_Lexer_Next(lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer->t.token);
    TEST_ASSERT_FALSE(lexer->t.hasLexError);
    TEST_ASSERT_EQUAL_UINT64(5U, lexer->t.rawLength);
    TEST_ASSERT_NOT_NULL(lexer->t.rawText);
    TEST_ASSERT_EQUAL_MEMORY("right", lexer->t.rawText, 5U);
    afterConsume = get_current_token_location(&g_tokenSourceProjection.parser);
    pure = pure && lexer->sourceName == entryName && token_source_projection_spans_equal(lexer->t.span, right);
    ZrParser_Lexer_Next(lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lexer->t.token);
    TEST_ASSERT_FALSE(lexer->t.hasLexError);

    TEST_ASSERT_TRUE_MESSAGE(pure &&
            token_source_projection_location_matches(before, "left_projection.zr", left) &&
            token_source_projection_location_matches(afterLookahead, "left_projection.zr", left) &&
            token_source_projection_location_matches(afterConsume, "right_projection.zr", right),
            "TokenStream locations must follow Span source identity across pure lookahead without changing the entry name");
}

static void test_token_source_projection_names_survive_ast_owner_and_gc(void) {
    SZrString *entryName;
    SZrAstNode *primary;
    SZrAstNode *target;
    SZrAstNode *call;
    SZrAstNode *watcher;
    SZrAstNode *following;
    SZrAstNode *literal;
    const SZrTokenTree *group;
    SZrTokenSpan targetSpan;
    SZrTokenSpan watcherSpan;
    SZrTokenSpan literalSpan;
    SZrGarbageCollectorStatsSnapshot before = {0};
    SZrGarbageCollectorStatsSnapshot after = {0};

    token_source_projection_make_pair("target", "(watcher); return 9;");
    targetSpan = token_source_projection_origin(
            ZrParser_TokenStream_At(g_tokenSourceProjection.combined, 0U), "left_projection.zr");
    group = ZrParser_TokenStream_At(g_tokenSourceProjection.combined, 1U);
    TEST_ASSERT_NOT_NULL(group);
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_TREE_GROUP, ZrParser_TokenTree_Kind(group));
    TEST_ASSERT_EQUAL_UINT64(1U, ZrParser_TokenTree_GroupChildCount(group));
    watcherSpan = token_source_projection_origin(
            ZrParser_TokenTree_GroupChildAt(group, 0U), "right_projection.zr");
    literalSpan = token_source_projection_origin(
            ZrParser_TokenStream_At(g_tokenSourceProjection.combined, 4U), "right_projection.zr");
    entryName = token_source_projection_entry_name();
    g_tokenSourceProjection.parserInitialized = ZR_TRUE;
    ZrParser_State_InitWithTokenStream(&g_tokenSourceProjection.parser, g_state,
                                      g_tokenSourceProjection.combined, entryName);
    g_tokenSourceProjection.parser.suppressErrorOutput = ZR_TRUE;
    g_tokenSourceProjection.ast = ZrParser_ParseWithState(&g_tokenSourceProjection.parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_tokenSourceProjection.ast,
                                 "Fixture prerequisite: projection lifetime source parsing failed");
    TEST_ASSERT_FALSE(g_tokenSourceProjection.parser.hasError || g_tokenSourceProjection.parser.hasFatalError);
    TEST_ASSERT_NOT_NULL(g_tokenSourceProjection.parser.lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_tokenSourceProjection.parser.lexer->t.token);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_tokenSourceProjection.ast->type);
    TEST_ASSERT_NOT_NULL(g_tokenSourceProjection.ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(2U, g_tokenSourceProjection.ast->data.script.statements->count);
    primary = g_tokenSourceProjection.ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(primary);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, primary->type);
    primary = primary->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(primary);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, primary->type);
    target = primary->data.primaryExpression.property;
    TEST_ASSERT_NOT_NULL(target);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, target->type);
    TEST_ASSERT_NOT_NULL(target->data.identifier.name);
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
    TEST_ASSERT_NOT_NULL(watcher->data.identifier.name);
    TEST_ASSERT_EQUAL_STRING("watcher", ZrCore_String_GetNativeString(watcher->data.identifier.name));
    following = g_tokenSourceProjection.ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(following);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, following->type);
    literal = following->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(literal);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, literal->type);
    TEST_ASSERT_NOT_NULL(target->ownership);
    TEST_ASSERT_NOT_NULL(watcher->ownership);
    TEST_ASSERT_NOT_NULL(literal->ownership);

    token_source_projection_free_parser();
    token_source_projection_release_inputs();
    ZrCore_Gc_RootLeaseRelease(g_state->global, &g_tokenSourceProjection.entryNameRoot);
    entryName = ZR_NULL;
    TEST_ASSERT_NOT_NULL(g_tokenSourceProjection.ast->data.script.tokenStream);
    TEST_ASSERT_NOT_NULL(g_tokenSourceProjection.ast->data.script.syntaxContextTable);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &before);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &after);
    TEST_ASSERT_TRUE_MESSAGE(after.fullCollectionCount > before.fullCollectionCount,
                             "Fixture prerequisite: projection lifetime full GC did not complete");

    TEST_ASSERT_TRUE_MESSAGE(
            token_source_projection_location_matches(target->location, "left_projection.zr", targetSpan) &&
            token_source_projection_location_matches(watcher->location, "right_projection.zr", watcherSpan) &&
            token_source_projection_location_matches(literal->location, "right_projection.zr", literalSpan),
            "AST ownership must keep projected leaf source names alive after parser and external stream release and full GC");
}

static void token_source_projection_reset_budget(void) {
    SZrParserComptimeBudgetLimits limits = {0};

    limits.fuel = ZR_PARSER_COMPTIME_DEFAULT_FUEL;
    limits.callDepth = ZR_PARSER_COMPTIME_DEFAULT_CALL_DEPTH;
    limits.heapBytes = ZR_TEST_PROJECTION_PARSER_HEAP_BYTES;
    limits.aggregateCount = ZR_PARSER_COMPTIME_DEFAULT_AGGREGATE_COUNT;
    limits.generatedDeclarationCount = ZR_PARSER_COMPTIME_DEFAULT_GENERATED_DECLARATION_COUNT;
    limits.diagnosticCount = ZR_PARSER_COMPTIME_DEFAULT_DIAGNOSTIC_COUNT;
    limits.tokenCount = ZR_PARSER_COMPTIME_DEFAULT_TOKEN_COUNT;
    limits.nodeCount = ZR_PARSER_COMPTIME_DEFAULT_NODE_COUNT;
    limits.matchingCount = ZR_PARSER_COMPTIME_DEFAULT_MATCHING_COUNT;
    limits.expansionCount = ZR_PARSER_COMPTIME_DEFAULT_EXPANSION_COUNT;
    ZrParser_ComptimeBudget_Init(&g_tokenSourceProjection.budget, &limits);
}

static void token_source_projection_require_short_name_control(SZrString *entryName) {
    token_source_projection_reset_budget();
    g_tokenSourceProjection.parserInitialized = ZR_TRUE;
    ZrParser_State_InitWithTokenStreamBudget(&g_tokenSourceProjection.parser, g_state,
            g_tokenSourceProjection.left, entryName, &g_tokenSourceProjection.budget);
    g_tokenSourceProjection.parser.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE_MESSAGE(g_tokenSourceProjection.parser.hasError,
                              "Fixture prerequisite: short-name budget initialization failed");
    TEST_ASSERT_NOT_NULL(g_tokenSourceProjection.parser.lexer);
    TEST_ASSERT_FALSE(g_tokenSourceProjection.parser.lexer->t.hasLexError);
    TEST_ASSERT_EQUAL_INT(ZR_TK_RETURN, g_tokenSourceProjection.parser.lexer->t.token);
    TEST_ASSERT_TRUE(g_tokenSourceProjection.budget.usage.heapBytes > 0U);
    g_tokenSourceProjection.ast = ZrParser_ParseWithState(&g_tokenSourceProjection.parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_tokenSourceProjection.ast,
                                 "Fixture prerequisite: short-name budget parse control failed");
    TEST_ASSERT_FALSE(g_tokenSourceProjection.parser.hasError || g_tokenSourceProjection.parser.hasFatalError);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_tokenSourceProjection.parser.lexer->t.token);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_NONE, g_tokenSourceProjection.budget.exceededResource);
    TEST_ASSERT_TRUE(g_tokenSourceProjection.budget.usage.heapBytes <= ZR_TEST_PROJECTION_PARSER_HEAP_BYTES);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_tokenSourceProjection.ast->type);
    TEST_ASSERT_NOT_NULL(g_tokenSourceProjection.ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1U, g_tokenSourceProjection.ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(g_tokenSourceProjection.ast->data.script.statements->nodes[0]);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT,
                          g_tokenSourceProjection.ast->data.script.statements->nodes[0]->type);
    ZrParser_Ast_Free(g_state, g_tokenSourceProjection.ast);
    g_tokenSourceProjection.ast = ZR_NULL;
    token_source_projection_free_parser();
}

static void test_token_source_projection_name_budget_fails_closed_and_retries(void) {
    static const TZrChar source[] = "return 1;";
    SZrString *entryName;
    SZrParserComptimeBudget beforeInitialization;
    SZrParserComptimeBudget initializedBudget;
    const SZrTokenTree *longNameToken;
    const TZrChar *copiedName;
    TZrSize copiedNameLength = 0U;
    TZrBool initializedWithError;
    TZrBool initializedWithLexicalError;
    TZrBool noAst;

    token_source_projection_cleanup();
    g_tokenSourceProjection.table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_tokenSourceProjection.table);
    token_source_projection_from_text("short_projection.zr", sizeof("short_projection.zr") - 1U,
                                      source, &g_tokenSourceProjection.left);
    g_tokenSourceProjection.longName = (TZrChar *)malloc(ZR_TEST_PROJECTION_NAME_LENGTH + 1U);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_tokenSourceProjection.longName,
                                 "Fixture prerequisite: native long source name allocation failed");
    memset(g_tokenSourceProjection.longName, 'n', ZR_TEST_PROJECTION_NAME_LENGTH);
    memcpy(g_tokenSourceProjection.longName + ZR_TEST_PROJECTION_NAME_LENGTH - 3U, ".zr", 3U);
    g_tokenSourceProjection.longName[ZR_TEST_PROJECTION_NAME_LENGTH] = '\0';
    token_source_projection_from_text(g_tokenSourceProjection.longName, ZR_TEST_PROJECTION_NAME_LENGTH,
                                      source, &g_tokenSourceProjection.right);
    free(g_tokenSourceProjection.longName);
    g_tokenSourceProjection.longName = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT64(ZrParser_TokenStream_Count(g_tokenSourceProjection.left),
                              ZrParser_TokenStream_Count(g_tokenSourceProjection.right));
    TEST_ASSERT_EQUAL_UINT64(3U, ZrParser_TokenStream_Count(g_tokenSourceProjection.left));
    longNameToken = ZrParser_TokenStream_At(g_tokenSourceProjection.right, 0U);
    TEST_ASSERT_NOT_NULL(longNameToken);
    copiedName = ZrParser_TokenStream_SourceName(g_tokenSourceProjection.right,
            ZrParser_TokenTree_Span(longNameToken).sourceId, &copiedNameLength);
    TEST_ASSERT_NOT_NULL_MESSAGE(copiedName, "Fixture prerequisite: native long source name was not retained");
    TEST_ASSERT_EQUAL_UINT64(ZR_TEST_PROJECTION_NAME_LENGTH, copiedNameLength);
    TEST_ASSERT_EQUAL_INT('n', copiedName[0]);
    TEST_ASSERT_EQUAL_MEMORY(".zr", copiedName + copiedNameLength - 3U, 3U);
    entryName = token_source_projection_entry_name();
    token_source_projection_require_short_name_control(entryName);

    token_source_projection_reset_budget();
    beforeInitialization = g_tokenSourceProjection.budget;
    g_tokenSourceProjection.parserInitialized = ZR_TRUE;
    ZrParser_State_InitWithTokenStreamBudget(&g_tokenSourceProjection.parser, g_state,
            g_tokenSourceProjection.right, entryName, &g_tokenSourceProjection.budget);
    g_tokenSourceProjection.parser.suppressErrorOutput = ZR_TRUE;
    /* Freeze Init's result before Parse can consume any unrelated AST budget. */
    initializedBudget = g_tokenSourceProjection.budget;
    initializedWithError = g_tokenSourceProjection.parser.hasError;
    initializedWithLexicalError = g_tokenSourceProjection.parser.lexer != ZR_NULL &&
                                  g_tokenSourceProjection.parser.lexer->t.hasLexError;
    g_tokenSourceProjection.ast = ZrParser_ParseWithState(&g_tokenSourceProjection.parser);
    noAst = g_tokenSourceProjection.ast == ZR_NULL;
    if (g_tokenSourceProjection.ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_tokenSourceProjection.ast);
        g_tokenSourceProjection.ast = ZR_NULL;
    }
    token_source_projection_free_parser();
    token_source_projection_require_short_name_control(entryName);

    TEST_ASSERT_TRUE_MESSAGE(beforeInitialization.usage.heapBytes == 0U &&
            beforeInitialization.exceededResource == ZR_PARSER_COMPTIME_BUDGET_NONE &&
            initializedWithError && initializedWithLexicalError && noAst &&
            initializedBudget.exceededResource == ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES &&
            initializedBudget.exceededLimit == ZR_TEST_PROJECTION_PARSER_HEAP_BYTES &&
            initializedBudget.requestedUsage > ZR_TEST_PROJECTION_PARSER_HEAP_BYTES &&
            initializedBudget.usage.heapBytes <= ZR_TEST_PROJECTION_PARSER_HEAP_BYTES,
            "TokenStream source-name projection must fail closed during Init when its name exceeds the parser heap budget");
}

#endif
