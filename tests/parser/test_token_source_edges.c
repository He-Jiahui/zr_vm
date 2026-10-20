/* RED coverage for direct token cursor edge behavior and ownership. */

#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/token_stream.h"
#include "syntax/token_source.h"

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

static SZrTokenStream *stream_from_text(
        SZrSyntaxContextTable *table,
        const TZrChar *sourceName,
        const TZrChar *source) {
    SZrTokenStream *stream = ZR_NULL;

    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            g_state, table, sourceName, strlen(sourceName),
            source, strlen(source), ZR_NULL, &stream));
    TEST_ASSERT_NOT_NULL(stream);
    return stream;
}

static SZrTokenSpan make_span(
        TZrTokenSourceId sourceId,
        TZrSize start,
        TZrSize end,
        TZrInt32 startColumn,
        TZrInt32 endColumn) {
    SZrTokenSpan span;
    span.sourceId = sourceId;
    span.startOffset = start;
    span.endOffset = end;
    span.startLine = 1;
    span.startColumn = startColumn;
    span.endLine = 1;
    span.endColumn = endColumn;
    return span;
}

static SZrSyntaxContext context_for_expansion(
        SZrSyntaxContextTable *table,
        TZrUInt64 stableIdentity) {
    static const TZrChar moduleName[] = "macro.edge";
    static const TZrChar callSource[] = "caller.zr";
    static const TZrChar definitionSource[] = "definition.zr";
    SZrSyntaxExpansionInfo info;
    TZrSyntaxExpansionId expansionId = 0U;

    memset(&info, 0, sizeof(info));
    info.stableIdentity = stableIdentity;
    info.definitionModule = moduleName;
    info.definitionModuleLength = sizeof(moduleName) - 1U;
    info.callSiteSource = callSource;
    info.callSiteSourceLength = sizeof(callSource) - 1U;
    info.definitionSiteSource = definitionSource;
    info.definitionSiteSourceLength = sizeof(definitionSource) - 1U;
    info.callSiteSpan = ZrParser_TokenSpan_None();
    info.definitionSiteSpan = ZrParser_TokenSpan_None();
    info.callSiteContext = ZrParser_SyntaxContext_TableRoot(table);
    info.definitionSiteContext = ZrParser_SyntaxContext_TableRoot(table);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            table, &info, &expansionId));
    return ZrParser_SyntaxContext_DefSite(table, expansionId);
}

static TZrBool append_ident(
        SZrTokenStream *stream,
        TZrTokenSourceId sourceId,
        TZrSize offset,
        TZrSize length,
        TZrInt32 startColumn,
        SZrSyntaxContext context,
        TZrSize *outIndex) {
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenText text = {sourceId, offset, length};

    semInfo.token = ZR_TK_IDENTIFIER;
    semInfo.text = text;
    return ZrParser_TokenStream_AppendIdent(
            stream, semInfo, text,
            make_span(sourceId, offset, offset + length,
                      startColumn, startColumn + (TZrInt32)length),
            context, ZR_NULL, 0U, ZR_NULL, 0U, outIndex);
}

static TZrBool append_greater(
        SZrTokenStream *stream,
        TZrTokenSourceId sourceId,
        TZrSize offset,
        TZrInt32 startColumn,
        EZrTokenSpacing spacing,
        SZrSyntaxContext context,
        TZrSize *outIndex) {
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenText raw = {sourceId, offset, 1U};

    semInfo.token = ZR_TK_GREATER_THAN;
    return ZrParser_TokenStream_AppendPunct(
            stream, semInfo, raw, '>', spacing,
            make_span(sourceId, offset, offset + 1U,
                      startColumn, startColumn + 1),
            context, ZR_NULL, 0U, ZR_NULL, 0U, outIndex);
}

static TZrBool append_integer(
        SZrTokenStream *stream,
        TZrTokenSourceId sourceId,
        TZrSize offset,
        TZrInt32 startColumn,
        TZrInt64 value,
        SZrSyntaxContext context,
        TZrSize *outIndex) {
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenText raw = {sourceId, offset, 1U};

    semInfo.token = ZR_TK_INTEGER;
    semInfo.intValue = value;
    return ZrParser_TokenStream_AppendLiteral(
            stream, semInfo, raw,
            make_span(sourceId, offset, offset + 1U,
                      startColumn, startColumn + 1),
            context, ZR_NULL, 0U, ZR_NULL, 0U, outIndex);
}

static void test_empty_string_literal_materializes_nonnull_vm_string(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    SZrTokenStream *stream;
    SZrLexState lexer;

    TEST_ASSERT_NOT_NULL(table);
    stream = stream_from_text(table, "empty.zr", "\"\"");
    ZrParser_Lexer_InitTokenStream(&lexer, g_state, stream, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(ZR_TK_STRING, lexer.t.token);
    TEST_ASSERT_NOT_NULL(lexer.t.seminfo.stringValue);
    TEST_ASSERT_EQUAL_UINT(0U, (unsigned)ZrCore_String_GetByteLength(
            lexer.t.seminfo.stringValue));
    ZrParser_Lexer_Free(&lexer);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_lexer_init_clears_poisoned_state(void) {
    static const TZrChar source[] = "ready";
    SZrLexState lexer;

    memset(&lexer, 0xA5, sizeof(lexer));
    ZrParser_Lexer_Init(&lexer, g_state, source, sizeof(source) - 1U, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT(5U, (unsigned)lexer.t.rawLength);
    TEST_ASSERT_EQUAL_MEMORY("ready", lexer.t.rawText, 5U);
    TEST_ASSERT_FALSE(lexer.hasLookahead);
    TEST_ASSERT_FALSE(lexer.hasSplitPending);
    TEST_ASSERT_NULL(lexer.tokenStream);
    TEST_ASSERT_NULL(lexer.tokenSourceState);
    ZrParser_Lexer_Free(&lexer);
}

static void test_cross_source_joint_split_keeps_next_token_and_provenance(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    SZrSyntaxContext firstContext;
    SZrSyntaxContext secondContext;
    SZrTokenStream *stream;
    TZrTokenSourceId firstSource = ZR_TOKEN_SOURCE_INVALID;
    TZrTokenSourceId secondSource = ZR_TOKEN_SOURCE_INVALID;
    TZrSize index = 0U;
    SZrLexState lexer;
    SZrTokenSourceCheckpoint beforeLookahead;
    SZrTokenSourceCheckpoint afterLookahead;
    SZrTokenSourceCheckpoint afterSplit;

    TEST_ASSERT_NOT_NULL(table);
    firstContext = context_for_expansion(table, UINT64_C(0xE001));
    secondContext = context_for_expansion(table, UINT64_C(0xE002));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(firstContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(secondContext));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(firstContext, secondContext));

    stream = ZrParser_TokenStream_Create(g_state->global, table, ZR_NULL);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "prefix.zr", sizeof("prefix.zr") - 1U,
            "prefix>", sizeof("prefix>") - 1U, &firstSource));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "generated.zr", sizeof("generated.zr") - 1U,
            ">next", sizeof(">next") - 1U, &secondSource));
    TEST_ASSERT_TRUE(append_greater(
            stream, firstSource, 6U, 7, ZR_TOKEN_SPACING_JOINT,
            firstContext, &index));
    TEST_ASSERT_TRUE(append_greater(
            stream, secondSource, 0U, 1, ZR_TOKEN_SPACING_ALONE,
            secondContext, &index));
    TEST_ASSERT_TRUE(append_ident(
            stream, secondSource, 1U, 4U, 2, ZrParser_SyntaxContext_TableRoot(table),
            &index));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));

    ZrParser_Lexer_InitTokenStream(&lexer, g_state, stream, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(ZR_TK_RIGHT_SHIFT, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT(2U, (unsigned)lexer.t.rawLength);
    TEST_ASSERT_EQUAL_MEMORY(">>", lexer.t.rawText, 2U);

    ZrParser_TokenSource_Save(&lexer, &beforeLookahead);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, ZrParser_Lexer_Lookahead(&lexer));
    ZrParser_TokenSource_Save(&lexer, &afterLookahead);
    TEST_ASSERT_TRUE(afterLookahead.hasLookahead);

    TEST_ASSERT_TRUE(ZrParser_Lexer_SplitCurrentPunct(&lexer, '>'));
    TEST_ASSERT_EQUAL_INT(ZR_TK_GREATER_THAN, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)firstSource,
                             (uint64_t)lexer.t.span.sourceId);
    TEST_ASSERT_EQUAL_UINT64(6U, (uint64_t)lexer.t.span.startOffset);
    TEST_ASSERT_EQUAL_UINT64(7U, (uint64_t)lexer.t.span.endOffset);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            firstContext, lexer.t.syntaxContext));
    ZrParser_TokenSource_Save(&lexer, &afterSplit);
    TEST_ASSERT_TRUE(afterSplit.hasPending);

    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_GREATER_THAN, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)secondSource,
                             (uint64_t)lexer.t.span.sourceId);
    TEST_ASSERT_EQUAL_UINT64(0U, (uint64_t)lexer.t.span.startOffset);
    TEST_ASSERT_EQUAL_UINT64(1U, (uint64_t)lexer.t.span.endOffset);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            secondContext, lexer.t.syntaxContext));
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_MEMORY("next", lexer.t.rawText, 4U);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)secondSource,
                             (uint64_t)lexer.t.span.sourceId);

    ZrParser_TokenSource_Restore(&lexer, &afterSplit);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_GREATER_THAN, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)secondSource,
                             (uint64_t)lexer.t.span.sourceId);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);

    ZrParser_TokenSource_Restore(&lexer, &afterLookahead);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_MEMORY("next", lexer.t.rawText, 4U);
    ZrParser_TokenSource_Restore(&lexer, &beforeLookahead);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, ZrParser_Lexer_Lookahead(&lexer));
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_MEMORY("next", lexer.t.rawText, 4U);

    ZrParser_Lexer_Free(&lexer);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_same_source_joint_split_after_lookahead_at_nonzero_offset(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    SZrSyntaxContext root;
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    TZrSize index = 0U;
    SZrLexState lexer;
    SZrTokenSourceCheckpoint afterLookahead;
    SZrTokenSourceCheckpoint afterSplit;

    TEST_ASSERT_NOT_NULL(table);
    root = ZrParser_SyntaxContext_TableRoot(table);
    stream = ZrParser_TokenStream_Create(g_state->global, table, ZR_NULL);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "same.zr", sizeof("same.zr") - 1U,
            "head >> next", sizeof("head >> next") - 1U, &sourceId));
    TEST_ASSERT_TRUE(append_greater(
            stream, sourceId, 5U, 6, ZR_TOKEN_SPACING_JOINT, root, &index));
    TEST_ASSERT_TRUE(append_greater(
            stream, sourceId, 6U, 7, ZR_TOKEN_SPACING_ALONE, root, &index));
    TEST_ASSERT_TRUE(append_ident(
            stream, sourceId, 8U, 4U, 9, root, &index));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));

    ZrParser_Lexer_InitTokenStream(&lexer, g_state, stream, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(ZR_TK_RIGHT_SHIFT, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT64(5U, (uint64_t)lexer.t.span.startOffset);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, ZrParser_Lexer_Lookahead(&lexer));
    ZrParser_TokenSource_Save(&lexer, &afterLookahead);
    TEST_ASSERT_TRUE(afterLookahead.hasLookahead);
    TEST_ASSERT_TRUE(ZrParser_Lexer_SplitCurrentPunct(&lexer, '>'));
    TEST_ASSERT_EQUAL_UINT64(5U, (uint64_t)lexer.t.span.startOffset);
    ZrParser_TokenSource_Save(&lexer, &afterSplit);
    TEST_ASSERT_TRUE(afterSplit.hasPending);

    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_GREATER_THAN, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT64(6U, (uint64_t)lexer.t.span.startOffset);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_MEMORY("next", lexer.t.rawText, 4U);

    ZrParser_TokenSource_Restore(&lexer, &afterSplit);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_GREATER_THAN, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT64(6U, (uint64_t)lexer.t.span.startOffset);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_MEMORY("next", lexer.t.rawText, 4U);

    ZrParser_TokenSource_Restore(&lexer, &afterLookahead);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_MEMORY("next", lexer.t.rawText, 4U);

    ZrParser_Lexer_Free(&lexer);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_from_text_joint_split_after_lookahead_at_nonzero_offset(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    SZrTokenStream *stream;
    SZrLexState lexer;

    TEST_ASSERT_NOT_NULL(table);
    stream = stream_from_text(table, "text.zr", "head >> next");
    ZrParser_Lexer_InitTokenStream(&lexer, g_state, stream, ZR_NULL);
    while (lexer.t.token != ZR_TK_RIGHT_SHIFT) {
        TEST_ASSERT_NOT_EQUAL(ZR_TK_EOS, lexer.t.token);
        ZrParser_Lexer_Next(&lexer);
    }
    TEST_ASSERT_EQUAL_UINT64(5U, (uint64_t)lexer.t.span.startOffset);
    TEST_ASSERT_EQUAL_UINT(2U, (unsigned)lexer.t.rawLength);
    TEST_ASSERT_EQUAL_MEMORY(">>", lexer.t.rawText, 2U);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, ZrParser_Lexer_Lookahead(&lexer));
    TEST_ASSERT_TRUE(ZrParser_Lexer_SplitCurrentPunct(&lexer, '>'));
    TEST_ASSERT_EQUAL_INT(ZR_TK_GREATER_THAN, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT64(5U, (uint64_t)lexer.t.span.startOffset);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_GREATER_THAN, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT64(6U, (uint64_t)lexer.t.span.startOffset);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_MEMORY("next", lexer.t.rawText, 4U);

    ZrParser_Lexer_Free(&lexer);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_parser_identifier_keeps_token_context_after_advance(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    SZrSyntaxContext identifierContext;
    SZrSyntaxContext root;
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenText raw;
    TZrSize index = 0U;
    SZrParserState parser;
    SZrAstNode *statement;

    TEST_ASSERT_NOT_NULL(table);
    identifierContext = context_for_expansion(table, UINT64_C(0xE003));
    root = ZrParser_SyntaxContext_TableRoot(table);
    stream = ZrParser_TokenStream_Create(g_state->global, table, ZR_NULL);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "context.zr", sizeof("context.zr") - 1U,
            "name;", sizeof("name;") - 1U, &sourceId));
    TEST_ASSERT_TRUE(append_ident(
            stream, sourceId, 0U, 4U, 1, identifierContext, &index));
    semInfo.token = ZR_TK_SEMICOLON;
    raw = (SZrTokenText){sourceId, 4U, 1U};
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendPunct(
            stream, semInfo, raw, ';', ZR_TOKEN_SPACING_ALONE,
            make_span(sourceId, 4U, 5U, 5, 6), root,
            ZR_NULL, 0U, ZR_NULL, 0U, &index));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));

    ZrParser_State_InitWithTokenStream(&parser, g_state, stream, ZR_NULL);
    parser.suppressErrorOutput = ZR_TRUE;
    statement = ZrParser_ParseTopLevelStatementWithState(&parser);
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    TEST_ASSERT_NOT_NULL(statement->data.expressionStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
            statement->data.expressionStatement.expr->type);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            identifierContext,
            statement->data.expressionStatement.expr->data.identifier.syntaxContext));

    ZrParser_Ast_Free(g_state, statement);
    ZrParser_State_Free(&parser);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_variable_pattern_keeps_token_context_after_advance(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    SZrSyntaxContext markedName;
    SZrSyntaxContext root;
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenText raw;
    TZrSize index = 0U;
    SZrParserState parser;
    SZrAstNode *statement;

    TEST_ASSERT_NOT_NULL(table);
    markedName = context_for_expansion(table, UINT64_C(0xE004));
    root = ZrParser_SyntaxContext_TableRoot(table);
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, markedName.markId);
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(markedName, root));
    stream = ZrParser_TokenStream_Create(g_state->global, table, ZR_NULL);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "declaration.zr", sizeof("declaration.zr") - 1U,
            "let name = 1;", sizeof("let name = 1;") - 1U, &sourceId));

    /* Keep the declaration keyword/root punctuation distinct from the marked
     * pattern so the parser must retain the name token's context after Next. */
    semInfo.token = ZR_TK_LET;
    raw = (SZrTokenText){sourceId, 0U, 3U};
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendIdent(
            stream, semInfo, raw,
            make_span(sourceId, 0U, 3U, 1, 4), root,
            ZR_NULL, 0U, ZR_NULL, 0U, &index));
    TEST_ASSERT_TRUE(append_ident(
            stream, sourceId, 4U, 4U, 5, markedName, &index));
    semInfo.token = ZR_TK_EQUALS;
    raw = (SZrTokenText){sourceId, 9U, 1U};
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendPunct(
            stream, semInfo, raw, '=', ZR_TOKEN_SPACING_ALONE,
            make_span(sourceId, 9U, 10U, 10, 11), root,
            ZR_NULL, 0U, ZR_NULL, 0U, &index));
    TEST_ASSERT_TRUE(append_integer(
            stream, sourceId, 11U, 12, 1, root, &index));
    semInfo.token = ZR_TK_SEMICOLON;
    raw = (SZrTokenText){sourceId, 12U, 1U};
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendPunct(
            stream, semInfo, raw, ';', ZR_TOKEN_SPACING_ALONE,
            make_span(sourceId, 12U, 13U, 13, 14), root,
            ZR_NULL, 0U, ZR_NULL, 0U, &index));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));

    ZrParser_State_InitWithTokenStream(&parser, g_state, stream, ZR_NULL);
    parser.suppressErrorOutput = ZR_TRUE;
    statement = ZrParser_ParseTopLevelStatementWithState(&parser);
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, statement->type);
    TEST_ASSERT_NOT_NULL(statement->data.variableDeclaration.pattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
            statement->data.variableDeclaration.pattern->type);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            markedName,
            statement->data.variableDeclaration.pattern->data.identifier.syntaxContext));

    ZrParser_Ast_Free(g_state, statement);
    ZrParser_State_Free(&parser);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_transparent_none_group_keeps_child_sequence(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    SZrSyntaxContext root;
    SZrTokenStream *childrenStream;
    SZrTokenStream *stream;
    TZrTokenSourceId childSource = ZR_TOKEN_SOURCE_INVALID;
    TZrTokenSourceId groupSource = ZR_TOKEN_SOURCE_INVALID;
    TZrSize firstIndex = 0U;
    TZrSize secondIndex = 0U;
    TZrSize groupIndex = 0U;
    const SZrTokenTree *children[2];
    const SZrTokenTree *group;
    SZrLexState lexer;

    TEST_ASSERT_NOT_NULL(table);
    root = ZrParser_SyntaxContext_TableRoot(table);
    childrenStream = ZrParser_TokenStream_Create(g_state->global, table, ZR_NULL);
    TEST_ASSERT_NOT_NULL(childrenStream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            childrenStream, "children.zr", sizeof("children.zr") - 1U,
            "a b", 3U, &childSource));
    TEST_ASSERT_TRUE(append_ident(
            childrenStream, childSource, 0U, 1U, 1, root, &firstIndex));
    TEST_ASSERT_TRUE(append_ident(
            childrenStream, childSource, 2U, 1U, 3, root, &secondIndex));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(childrenStream));
    children[0] = ZrParser_TokenStream_At(childrenStream, firstIndex);
    children[1] = ZrParser_TokenStream_At(childrenStream, secondIndex);

    stream = ZrParser_TokenStream_Create(g_state->global, table, ZR_NULL);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "group.zr", sizeof("group.zr") - 1U,
            "a b", 3U, &groupSource));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendGroup(
            stream, ZR_TOKEN_DELIMITER_NONE,
            ZrParser_TokenSpan_None(), ZrParser_TokenSpan_None(),
            children, 2U, make_span(groupSource, 0U, 3U, 1, 4), root,
            ZR_NULL, 0U, ZR_NULL, 0U, &groupIndex));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));
    group = ZrParser_TokenStream_At(stream, groupIndex);
    TEST_ASSERT_NOT_NULL(group);
    TEST_ASSERT_EQUAL_INT(ZR_TOKEN_DELIMITER_NONE,
                          ZrParser_TokenTree_GroupDelimiter(group));
    TEST_ASSERT_EQUAL_UINT(2U,
                          (unsigned)ZrParser_TokenTree_GroupChildCount(group));

    ZrParser_Lexer_InitTokenStream(&lexer, g_state, stream, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_MEMORY("a", lexer.t.rawText, 1U);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_MEMORY("b", lexer.t.rawText, 1U);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lexer.t.token);

    ZrParser_Lexer_Free(&lexer);
    ZrParser_TokenStream_Release(stream);
    ZrParser_TokenStream_Release(childrenStream);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_parsed_ast_keeps_stream_owners_after_lexer_free_and_gc(void) {
    SZrSyntaxContextTable *table = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    SZrTokenStream *stream;
    SZrParserState parser;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(table);
    stream = stream_from_text(table, "ast.zr", "return 1 + 2;");
    ZrParser_State_InitWithTokenStream(&parser, g_state, stream, ZR_NULL);
    parser.suppressErrorOutput = ZR_TRUE;
    ast = ZrParser_ParseWithState(&parser);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.tokenStream);
    TEST_ASSERT_NOT_NULL(ast->data.script.syntaxContextTable);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(1U,
            (unsigned)ast->data.script.statements->count);

    ZrParser_State_Free(&parser);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_FALSE);

    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.tokenStream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Count(
            ast->data.script.tokenStream) > 0U);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(
            ZrParser_SyntaxContext_TableRoot(
                    ast->data.script.syntaxContextTable)));
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT,
            ast->data.script.statements->nodes[0]->type);

    ZrParser_Ast_Free(g_state, ast);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_string_literal_materializes_nonnull_vm_string);
    RUN_TEST(test_lexer_init_clears_poisoned_state);
    RUN_TEST(test_cross_source_joint_split_keeps_next_token_and_provenance);
    RUN_TEST(test_same_source_joint_split_after_lookahead_at_nonzero_offset);
    RUN_TEST(test_from_text_joint_split_after_lookahead_at_nonzero_offset);
    RUN_TEST(test_parser_identifier_keeps_token_context_after_advance);
    RUN_TEST(test_variable_pattern_keeps_token_context_after_advance);
    RUN_TEST(test_transparent_none_group_keeps_child_sequence);
    RUN_TEST(test_parsed_ast_keeps_stream_owners_after_lexer_free_and_gc);
    return UNITY_END();
}
