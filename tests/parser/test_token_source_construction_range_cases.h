#ifndef ZR_TEST_TOKEN_SOURCE_CONSTRUCTION_RANGE_CASES_H
#define ZR_TEST_TOKEN_SOURCE_CONSTRUCTION_RANGE_CASES_H

#include "parser/parser_ownership.h"
#include "parser/parser_range_evidence.h"

enum {
    ZR_TEST_CONSTRUCTION_RANGE_STREAMS = 3,
    ZR_TEST_CONSTRUCTION_RANGE_PADDING = 64,
    ZR_TEST_CONSTRUCTION_RANGE_ATOMS = 10
};

typedef struct SZrConstructionRangeToken {
    EZrToken token;
    TZrSize start;
    TZrSize end;
    const char *raw;
    TZrInt64 integer;
} SZrConstructionRangeToken;

typedef struct SZrConstructionRangeFixture {
    SZrSyntaxContextTable *table;
    SZrTokenStream *streams[ZR_TEST_CONSTRUCTION_RANGE_STREAMS];
    TZrSize streamCount;
    SZrParserState parser;
    TZrBool parserInitialized;
    SZrAstNode *ast;
    SZrGcRootLease entryNameRoot;
    char paddedSource[96];
} SZrConstructionRangeFixture;

static SZrConstructionRangeFixture g_constructionRange;
static const char g_constructionRangeHead[] = "construction_head.zr";
static const char g_constructionRangeTail[] = "construction_tail.zr";
static const SZrConstructionRangeToken g_constructionRangeCallTokens[] = {
    {ZR_TK_LPAREN, 64U, 65U, "(", 0},
    {ZR_TK_INTEGER, 65U, 66U, "1", 1},
    {ZR_TK_RPAREN, 66U, 67U, ")", 0},
    {ZR_TK_SEMICOLON, 67U, 68U, ";", 0},
    {ZR_TK_RETURN, 69U, 75U, "return", 0},
    {ZR_TK_INTEGER, 76U, 77U, "9", 9},
    {ZR_TK_SEMICOLON, 77U, 78U, ";", 0},
    {ZR_TK_EOS, 78U, 78U, "", 0}
};
static const SZrConstructionRangeToken g_constructionRangeNoArgsTokens[] = {
    {ZR_TK_SEMICOLON, 64U, 65U, ";", 0},
    {ZR_TK_RETURN, 66U, 72U, "return", 0},
    {ZR_TK_INTEGER, 73U, 74U, "9", 9},
    {ZR_TK_SEMICOLON, 74U, 75U, ";", 0},
    {ZR_TK_EOS, 75U, 75U, "", 0}
};

static void token_source_construction_range_cleanup(void) {
    TZrSize index;

    ZrParser_Ast_Free(g_state, g_constructionRange.ast);
    if (g_constructionRange.parserInitialized) {
        ZrParser_State_Free(&g_constructionRange.parser);
    }
    if (g_state != ZR_NULL) {
        ZrCore_Gc_RootLeaseRelease(g_state->global, &g_constructionRange.entryNameRoot);
    }
    for (index = g_constructionRange.streamCount; index > 0U; --index) {
        ZrParser_TokenStream_Release(g_constructionRange.streams[index - 1U]);
    }
    ZrParser_SyntaxContextTable_Release(g_constructionRange.table);
    memset(&g_constructionRange, 0, sizeof(g_constructionRange));
}

static SZrTokenStream **construction_range_stream_slot(void) {
    TZrSize index = g_constructionRange.streamCount;

    TEST_ASSERT_TRUE_MESSAGE(index < ZR_TEST_CONSTRUCTION_RANGE_STREAMS,
            "Fixture prerequisite: construction stream inventory overflowed");
    g_constructionRange.streamCount++;
    return &g_constructionRange.streams[index];
}

static SZrTokenStream *construction_range_from_text(const char *name, const char *source) {
    SZrTokenStream **slot = construction_range_stream_slot();

    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(g_state, g_constructionRange.table,
            name, strlen(name), source, strlen(source), ZR_NULL, slot) && *slot != ZR_NULL &&
            ZrParser_TokenStream_IsSealed(*slot),
            "Fixture prerequisite: construction source tokenization failed");
    return *slot;
}

static SZrTokenStream *construction_range_input(TZrBool isOwn, TZrBool hasArgs) {
    const char *suffix = hasArgs ? "(1); return 9;" : "; return 9;";
    SZrTokenStream *left;
    SZrTokenStream *right;
    SZrTokenStream **slot;

    g_constructionRange.table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_constructionRange.table,
            "Fixture prerequisite: construction context table allocation failed");
    memset(g_constructionRange.paddedSource, ' ', ZR_TEST_CONSTRUCTION_RANGE_PADDING);
    memcpy(g_constructionRange.paddedSource + ZR_TEST_CONSTRUCTION_RANGE_PADDING,
            suffix, strlen(suffix) + 1U);
    left = construction_range_from_text(g_constructionRangeHead, isOwn ? "own Box" : "new Box");
    right = construction_range_from_text(g_constructionRangeTail, g_constructionRange.paddedSource);
    slot = construction_range_stream_slot();
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Count(left) == 2U &&
            ZrParser_TokenStream_Count(right) == (hasArgs ? 5U : 4U) &&
            ZrParser_TokenStream_Concat(left, right, ZR_NULL, slot) && *slot != ZR_NULL &&
            ZrParser_TokenStream_IsSealed(*slot),
            "Fixture prerequisite: construction source concatenation failed");
    return *slot;
}

static void construction_range_require_span(SZrTokenStream *stream, SZrTokenSpan span,
        const char *name, TZrSize start, TZrSize end, const char *raw) {
    TZrSize nameLength = 0U;
    TZrSize sourceLength = 0U;
    const char *sourceName = ZrParser_TokenStream_SourceName(stream, span.sourceId, &nameLength);
    const char *source = ZrParser_TokenStream_SourceBytes(stream, span.sourceId, &sourceLength);

    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenSpan_IsValid(span) && sourceName != ZR_NULL &&
            source != ZR_NULL && nameLength == strlen(name) && memcmp(sourceName, name, nameLength) == 0 &&
            start <= end && end <= sourceLength && span.startOffset == start && span.endOffset == end &&
            span.startLine == 1 && span.endLine == 1 &&
            span.startColumn == (TZrInt32)start + 1 && span.endColumn == (TZrInt32)end + 1 &&
            strlen(raw) == end - start && memcmp(source + start, raw, end - start) == 0,
            "Fixture prerequisite: construction raw source, Span or source identity changed");
}

static void construction_range_require_leaf(SZrTokenStream *stream, const SZrTokenTree *tree,
        const char *name, EZrToken token, TZrSize start, TZrSize end, const char *raw, TZrInt64 integer) {
    SZrTokenText text;
    SZrTokenNativeSemInfo info;
    SZrTokenSpan span;
    const char *bytes;
    EZrTokenTreeKind kind = token == ZR_TK_INTEGER ? ZR_TOKEN_TREE_LITERAL :
            (token == ZR_TK_SEMICOLON ? ZR_TOKEN_TREE_PUNCT : ZR_TOKEN_TREE_IDENT);

    TEST_ASSERT_NOT_NULL_MESSAGE(tree, "Fixture prerequisite: construction native leaf is missing");
    span = ZrParser_TokenTree_Span(tree);
    text = ZrParser_TokenTree_Text(tree);
    bytes = ZrParser_TokenStream_TextBytes(stream, text);
    construction_range_require_span(stream, span, name, start, end, raw);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenTree_Kind(tree) == kind && bytes != ZR_NULL &&
            text.sourceId == span.sourceId && text.offset == start && text.length == end - start &&
            memcmp(bytes, raw, text.length) == 0 &&
            ZrParser_SyntaxContext_Equal(ZrParser_TokenTree_Context(tree),
                    ZrParser_SyntaxContext_TableRoot(g_constructionRange.table)) &&
            ZrParser_TokenTree_NativeSemInfo(tree, &info) && info.token == token &&
            (token != ZR_TK_INTEGER || info.intValue == integer),
            "Fixture prerequisite: construction native payload, raw text or root context changed");
}

static void construction_range_require_input(SZrTokenStream *stream, TZrBool isOwn, TZrBool hasArgs) {
    const SZrConstructionRangeToken *tail = hasArgs ? g_constructionRangeCallTokens :
            g_constructionRangeNoArgsTokens;
    TZrSize tailCount = hasArgs ? 8U : 5U;
    TZrSize rootIndex = hasArgs ? 3U : 2U;
    TZrTokenSourceId headId;
    TZrTokenSourceId tailId;
    TZrSize sourceLength = 0U;
    const char *source;
    TZrSize index;

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(hasArgs ? 7U : 6U, ZrParser_TokenStream_Count(stream),
            "Fixture prerequisite: construction root token count changed");
    construction_range_require_leaf(stream, ZrParser_TokenStream_At(stream, 0U),
            g_constructionRangeHead, isOwn ? ZR_TK_IDENTIFIER : ZR_TK_NEW, 0U, 3U,
            isOwn ? "own" : "new", 0);
    construction_range_require_leaf(stream, ZrParser_TokenStream_At(stream, 1U),
            g_constructionRangeHead, ZR_TK_IDENTIFIER, 4U, 7U, "Box", 0);
    headId = ZrParser_TokenTree_Span(ZrParser_TokenStream_At(stream, 0U)).sourceId;
    tailId = ZrParser_TokenTree_Span(ZrParser_TokenStream_At(stream, 2U)).sourceId;
    TEST_ASSERT_TRUE_MESSAGE(headId != tailId &&
            ZrParser_TokenTree_Span(ZrParser_TokenStream_At(stream, 1U)).sourceId == headId,
            "Fixture prerequisite: construction head and tail must have distinct physical sources");
    source = ZrParser_TokenStream_SourceBytes(stream, headId, &sourceLength);
    TEST_ASSERT_TRUE_MESSAGE(source != ZR_NULL && sourceLength == 7U &&
            memcmp(source, isOwn ? "own Box" : "new Box", 7U) == 0,
            "Fixture prerequisite: construction complete head source changed");
    source = ZrParser_TokenStream_SourceBytes(stream, tailId, &sourceLength);
    TEST_ASSERT_TRUE_MESSAGE(source != ZR_NULL && sourceLength == strlen(g_constructionRange.paddedSource) &&
            memcmp(source, g_constructionRange.paddedSource, sourceLength) == 0,
            "Fixture prerequisite: construction complete padded tail source changed");
    if (hasArgs) {
        const SZrTokenTree *group = ZrParser_TokenStream_At(stream, 2U);
        SZrTokenSpan open = ZrParser_TokenTree_GroupOpenSpan(group);
        SZrTokenSpan close = ZrParser_TokenTree_GroupCloseSpan(group);

        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenTree_Kind(group) == ZR_TOKEN_TREE_GROUP &&
                ZrParser_TokenTree_GroupDelimiter(group) == ZR_TOKEN_DELIMITER_PAREN &&
                ZrParser_TokenTree_GroupChildCount(group) == 1U && open.sourceId == tailId &&
                close.sourceId == tailId &&
                ZrParser_SyntaxContext_Equal(ZrParser_TokenTree_Context(group),
                        ZrParser_SyntaxContext_TableRoot(g_constructionRange.table)),
                "Fixture prerequisite: construction argument group or delimiter context changed");
        construction_range_require_span(stream, ZrParser_TokenTree_Span(group),
                g_constructionRangeTail, 64U, 67U, "(1)");
        construction_range_require_span(stream, open, g_constructionRangeTail, 64U, 65U, "(");
        construction_range_require_span(stream, close, g_constructionRangeTail, 66U, 67U, ")");
        construction_range_require_leaf(stream, ZrParser_TokenTree_GroupChildAt(group, 0U),
                g_constructionRangeTail, ZR_TK_INTEGER, 65U, 66U, "1", 1);
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenTree_Span(
                ZrParser_TokenTree_GroupChildAt(group, 0U)).sourceId == tailId,
                "Fixture prerequisite: construction argument must belong to the tail source");
    }
    for (index = hasArgs ? 3U : 0U; index + 1U < tailCount; ++index) {
        const SZrTokenTree *tree = ZrParser_TokenStream_At(stream, rootIndex++);
        construction_range_require_leaf(stream, tree, g_constructionRangeTail, tail[index].token,
                tail[index].start, tail[index].end, tail[index].raw, tail[index].integer);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(tailId, ZrParser_TokenTree_Span(tree).sourceId,
                "Fixture prerequisite: construction trailing token source changed");
    }
    construction_range_require_span(stream, ZrParser_TokenStream_EndSpan(stream),
            g_constructionRangeTail, tail[tailCount - 1U].start, tail[tailCount - 1U].end, "");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(tailId, ZrParser_TokenStream_EndSpan(stream).sourceId,
            "Fixture prerequisite: construction EOS must belong to the tail source");
}

static TZrBool construction_range_physical_is(SZrFileRange range, const char *name,
        TZrSize start, TZrSize end) {
    return range.source != ZR_NULL && ZrCore_String_GetNativeString(range.source) != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(range.source), name) == 0 &&
            range.start.offset == start && range.end.offset == end &&
            range.start.line == 1 && range.end.line == 1 &&
            range.start.column == (TZrInt32)start + 1 && range.end.column == (TZrInt32)end + 1;
}

static TZrBool construction_range_interval_is(SZrAstOwnership *owner, SZrFileRange range,
        TZrUInt32 first, TZrUInt32 last) {
    SZrParserRangeEvidenceDescription description;
    return ZrParser_RangeEvidence_Describe(owner, range, &description) &&
            description.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL &&
            description.first == first && description.last == last;
}

static TZrBool construction_range_same_atom(SZrAstOwnership *owner, SZrFileRange actual,
        SZrFileRange expected) {
    SZrParserRangeEvidenceDescription description;
    return ZrParser_RangeEvidence_Describe(owner, expected, &description) &&
            description.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL && description.first == description.last &&
            construction_range_interval_is(owner, actual, description.first, description.last) &&
            actual.source == expected.source && actual.originKey == expected.originKey &&
            actual.ownerKey == expected.ownerKey && actual.flags == expected.flags &&
            actual.start.offset == expected.start.offset && actual.end.offset == expected.end.offset &&
            actual.start.line == expected.start.line && actual.end.line == expected.end.line &&
            actual.start.column == expected.start.column && actual.end.column == expected.end.column;
}

static void construction_range_entry_name(const char *name) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)name);

    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName,
            "Fixture prerequisite: construction parser source name allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(sourceName), &g_constructionRange.entryNameRoot),
            "Fixture prerequisite: construction parser source name rooting failed");
    g_constructionRange.parserInitialized = ZR_TRUE;
}

static void construction_range_require_node(SZrAstNode *node, EZrAstNodeType type) {
    TEST_ASSERT_TRUE_MESSAGE(node != ZR_NULL && node->type == type &&
            node->ownership == g_constructionRange.ast->ownership &&
            ZrParser_RangeEvidence_Validate(node->ownership, node->location),
            "Fixture prerequisite: construction AST node shape, ownership or range is invalid");
}

static void construction_range_finish_parse(void) {
    TEST_ASSERT_NOT_NULL_MESSAGE(g_constructionRange.parser.lexer,
            "Fixture prerequisite: construction parser initialization failed");
    g_constructionRange.parser.suppressErrorOutput = ZR_TRUE;
    g_constructionRange.ast = ZrParser_ParseWithState(&g_constructionRange.parser);
    TEST_ASSERT_TRUE_MESSAGE(g_constructionRange.ast != ZR_NULL &&
            g_constructionRange.ast->ownership != ZR_NULL && !g_constructionRange.parser.hasError &&
            !g_constructionRange.parser.hasFatalError && !g_constructionRange.parser.hasNestingError &&
            !g_constructionRange.parser.lexer->hasSourceEvidenceError &&
            !g_constructionRange.parser.lexer->t.hasLexError &&
            g_constructionRange.parser.lexer->t.token == ZR_TK_EOS &&
            g_constructionRange.ast->ownership == g_constructionRange.parser.lexer->ownership,
            "Fixture prerequisite: construction parse must reach EOS with an owned AST and no errors");
    construction_range_require_node(g_constructionRange.ast, ZR_AST_SCRIPT);
    TEST_ASSERT_TRUE_MESSAGE(g_constructionRange.ast->data.script.statements != ZR_NULL &&
            g_constructionRange.ast->data.script.statements->nodes != ZR_NULL &&
            g_constructionRange.ast->data.script.statements->count == 2U,
            "Fixture prerequisite: construction script must retain both statements");
}

static SZrAstNode *construction_range_require_return(void) {
    SZrAstNode *statement = g_constructionRange.ast->data.script.statements->nodes[1];
    construction_range_require_node(statement, ZR_AST_RETURN_STATEMENT);
    construction_range_require_node(statement->data.returnStatement.expr, ZR_AST_INTEGER_LITERAL);
    TEST_ASSERT_TRUE_MESSAGE(statement->data.returnStatement.expr->data.integerLiteral.value == 9 &&
            !statement->data.returnStatement.isReferenceReturn,
            "Fixture prerequisite: construction must leave the following return 9 intact");
    return statement;
}

static void construction_range_text_control(TZrBool hasArgs) {
    const char *source = hasArgs ? "Box(1); return 9;" : "Box(); return 9;";
    SZrAstNode *statement;
    SZrAstNode *primary;
    SZrAstNode *call;
    SZrAstNode *target;
    SZrAstNode *next;
    SZrParserRangeEvidenceDescription description;
    TZrSize callEnd = hasArgs ? 6U : 5U;

    construction_range_entry_name("construction_plain.zr");
    ZrParser_State_Init(&g_constructionRange.parser, g_state, source, strlen(source),
            (SZrString *)g_constructionRange.entryNameRoot.object);
    construction_range_finish_parse();
    statement = g_constructionRange.ast->data.script.statements->nodes[0];
    construction_range_require_node(statement, ZR_AST_EXPRESSION_STATEMENT);
    primary = statement->data.expressionStatement.expr;
    construction_range_require_node(primary, ZR_AST_PRIMARY_EXPRESSION);
    target = primary->data.primaryExpression.property;
    construction_range_require_node(target, ZR_AST_IDENTIFIER_LITERAL);
    TEST_ASSERT_TRUE_MESSAGE(target->data.identifier.name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(target->data.identifier.name), "Box") == 0 &&
            ZrParser_SyntaxContext_Equal(target->data.identifier.syntaxContext, ZrParser_SyntaxContext_Root()) &&
            primary->data.primaryExpression.members != ZR_NULL &&
            primary->data.primaryExpression.members->nodes != ZR_NULL &&
            primary->data.primaryExpression.members->count == 1U,
            "Fixture prerequisite: ordinary construction call target or PRIMARY shape changed");
    call = primary->data.primaryExpression.members->nodes[0];
    construction_range_require_node(call, ZR_AST_FUNCTION_CALL);
    TEST_ASSERT_TRUE_MESSAGE(call->data.functionCall.args != ZR_NULL &&
            call->data.functionCall.args->count == (hasArgs ? 1U : 0U) &&
            !call->data.functionCall.hasNamedArgs && call->data.functionCall.accessMode == ZR_POSTFIX_ACCESS_DIRECT,
            "Fixture prerequisite: ordinary construction call arguments changed");
    if (hasArgs) {
        TEST_ASSERT_NOT_NULL_MESSAGE(call->data.functionCall.args->nodes,
                "Fixture prerequisite: ordinary construction argument storage is missing");
        construction_range_require_node(call->data.functionCall.args->nodes[0], ZR_AST_INTEGER_LITERAL);
        TEST_ASSERT_EQUAL_INT64_MESSAGE(1, call->data.functionCall.args->nodes[0]->data.integerLiteral.value,
                "Fixture prerequisite: ordinary construction argument value changed");
    }
    next = construction_range_require_return();
    TEST_ASSERT_TRUE_MESSAGE(construction_range_physical_is(target->location, "construction_plain.zr", 0U, 3U) &&
            construction_range_physical_is(call->location, "construction_plain.zr", 3U, callEnd) &&
            construction_range_physical_is(primary->location, "construction_plain.zr", 0U, callEnd) &&
            construction_range_physical_is(next->location, "construction_plain.zr", callEnd + 2U, strlen(source)) &&
            ZrParser_RangeEvidence_Describe(primary->ownership, primary->location, &description) &&
            description.kind == ZR_PARSER_RANGE_EVIDENCE_SOURCE && primary->location.flags == 0U,
            "Fixture prerequisite: ordinary call ranges must end before the semicolon and preserve return 9");
    token_source_construction_range_cleanup();
}

static TZrSize construction_range_require_native(TZrBool hasArgs, SZrFileRange *nativeRangeEvidence) {
    const SZrConstructionRangeToken *tail = hasArgs ? g_constructionRangeCallTokens :
            g_constructionRangeNoArgsTokens;
    TZrSize count = hasArgs ? 10U : 7U;
    SZrAstOwnership *owner = g_constructionRange.ast->ownership;
    SZrFileRange eos;
    TZrSize index;

    /* Read the lexer's committed atoms; the fixture never creates evidence or AST nodes. */
    for (index = 0U; index < count; ++index) {
        TZrSize start = index < 2U ? (index == 0U ? 0U : 4U) : tail[index - 2U].start;
        TZrSize end = index < 2U ? (index == 0U ? 3U : 7U) : tail[index - 2U].end;
        const char *name = index < 2U ? g_constructionRangeHead : g_constructionRangeTail;
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_RangeEvidence_Location(owner, (TZrUInt32)index + 1U,
                &nativeRangeEvidence[index]) &&
                construction_range_interval_is(owner, nativeRangeEvidence[index],
                        (TZrUInt32)index + 1U, (TZrUInt32)index + 1U) &&
                construction_range_physical_is(nativeRangeEvidence[index], name, start, end) &&
                nativeRangeEvidence[index].flags == 0U && nativeRangeEvidence[index].originKey != 0U &&
                nativeRangeEvidence[index].ownerKey == ZrParser_Ownership_Key(owner),
                "Fixture prerequisite: construction natural native atom evidence changed");
        if (index != 0U) {
            TEST_ASSERT_TRUE_MESSAGE(index == 1U ?
                    nativeRangeEvidence[index].originKey == nativeRangeEvidence[0].originKey :
                    (nativeRangeEvidence[index].originKey == nativeRangeEvidence[2].originKey &&
                     nativeRangeEvidence[index].originKey != nativeRangeEvidence[0].originKey),
                    "Fixture prerequisite: construction native head and tail origins must remain distinct");
        }
    }
    construction_range_require_span(g_constructionRange.streams[2], g_constructionRange.parser.lexer->t.span,
            g_constructionRangeTail, tail[count - 3U].start, tail[count - 3U].end, "");
    TEST_ASSERT_TRUE_MESSAGE(g_constructionRange.parser.lexer->t.firstNativeOrdinal == count &&
            g_constructionRange.parser.lexer->t.lastNativeOrdinal == count &&
            g_constructionRange.parser.lexer->t.rawLength == 0U &&
            ZrParser_RangeEvidence_TokenLocation(owner, g_constructionRange.parser.lexer->t.evidenceIndex,
                    (TZrUInt32)count, (TZrUInt32)count, &eos) &&
            construction_range_same_atom(owner, eos, nativeRangeEvidence[count - 1U]),
            "Fixture prerequisite: construction lexer EOS ordinal changed");
    return count;
}

static SZrAstNode *construction_range_require_construct(TZrBool isOwn, TZrBool hasArgs,
        SZrFileRange *nativeRangeEvidence, TZrSize count) {
    SZrAstNode *statement = g_constructionRange.ast->data.script.statements->nodes[0];
    SZrAstNode *construct;
    SZrAstNode *target;
    SZrAstNode *next;
    SZrAstOwnership *owner = g_constructionRange.ast->ownership;

    construction_range_require_node(statement, ZR_AST_EXPRESSION_STATEMENT);
    construct = statement->data.expressionStatement.expr;
    construction_range_require_node(construct, ZR_AST_CONSTRUCT_EXPRESSION);
    target = construct->data.constructExpression.target;
    construction_range_require_node(target, ZR_AST_IDENTIFIER_LITERAL);
    TEST_ASSERT_TRUE_MESSAGE(target->data.identifier.name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(target->data.identifier.name), "Box") == 0 &&
            ZrParser_SyntaxContext_Equal(target->data.identifier.syntaxContext,
                    ZrParser_SyntaxContext_TableRoot(g_constructionRange.table)) &&
            construction_range_same_atom(owner, target->location, nativeRangeEvidence[1]) &&
            construct->data.constructExpression.args != ZR_NULL &&
            construct->data.constructExpression.args->count == (hasArgs ? 1U : 0U) &&
            construct->data.constructExpression.isNew && !construct->data.constructExpression.isUsing &&
            construct->data.constructExpression.isResourceSurface == isOwn &&
            construct->data.constructExpression.ownershipQualifier ==
                    (isOwn ? ZR_OWNERSHIP_QUALIFIER_UNIQUE : ZR_OWNERSHIP_QUALIFIER_NONE) &&
            construct->data.constructExpression.builtinKind ==
                    (isOwn ? ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE : ZR_OWNERSHIP_BUILTIN_KIND_NONE),
            "Fixture prerequisite: construction target, positional arguments or ownership flags changed");
    if (hasArgs) {
        SZrAstNode *argument;
        TEST_ASSERT_NOT_NULL_MESSAGE(construct->data.constructExpression.args->nodes,
                "Fixture prerequisite: construction argument storage is missing");
        argument = construct->data.constructExpression.args->nodes[0];
        construction_range_require_node(argument, ZR_AST_INTEGER_LITERAL);
        TEST_ASSERT_TRUE_MESSAGE(argument->data.integerLiteral.value == 1 &&
                construction_range_same_atom(owner, argument->location, nativeRangeEvidence[3]),
                "Fixture prerequisite: construction must preserve the real tail argument");
    }
    next = construction_range_require_return();
    TEST_ASSERT_TRUE_MESSAGE(construction_range_same_atom(owner, next->data.returnStatement.expr->location,
            nativeRangeEvidence[count - 3U]) &&
            construction_range_interval_is(owner, next->location, (TZrUInt32)count - 3U, (TZrUInt32)count - 1U) &&
            construction_range_physical_is(next->location, g_constructionRangeTail,
                    nativeRangeEvidence[count - 4U].start.offset, nativeRangeEvidence[count - 2U].end.offset) &&
            next->location.flags == 0U && next->location.originKey == nativeRangeEvidence[2].originKey,
            "Fixture prerequisite: construction must retain the complete second return and its native evidence");
    return construct;
}

static TZrBool construction_range_case(TZrBool isOwn, TZrBool hasArgs) {
    SZrFileRange nativeRangeEvidence[ZR_TEST_CONSTRUCTION_RANGE_ATOMS];
    SZrFileRange firstAnchor;
    SZrFileRange lastAnchor;
    SZrFileRange beyond;
    SZrTokenStream *stream;
    SZrAstNode *construct;
    SZrAstOwnership *owner;
    SZrParserRangeEvidenceDescription description;
    TZrSize count;
    TZrSize last = hasArgs ? 4U : 1U;
    TZrBool interval;
    TZrBool anchors;
    TZrBool stops;
    TZrBool passed;

    construction_range_text_control(hasArgs);
    stream = construction_range_input(isOwn, hasArgs);
    construction_range_require_input(stream, isOwn, hasArgs);
    construction_range_entry_name("construction_expansion.zr");
    ZrParser_State_InitWithTokenStream(&g_constructionRange.parser, g_state, stream,
            (SZrString *)g_constructionRange.entryNameRoot.object);
    construction_range_finish_parse();
    construction_range_require_input(stream, isOwn, hasArgs);
    count = construction_range_require_native(hasArgs, nativeRangeEvidence);
    construct = construction_range_require_construct(isOwn, hasArgs, nativeRangeEvidence, count);
    owner = construct->ownership;
    interval = ZrParser_RangeEvidence_Describe(owner, construct->location, &description) &&
            description.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL && description.first == 1U &&
            description.last == last + 1U;
    anchors = ZrParser_RangeEvidence_AnchorAt(owner, construct->location, 0U, &firstAnchor) &&
            ZrParser_RangeEvidence_AnchorAt(owner, construct->location, last, &lastAnchor) &&
            construction_range_same_atom(owner, firstAnchor, nativeRangeEvidence[0]) &&
            construction_range_same_atom(owner, lastAnchor, nativeRangeEvidence[last]);
    stops = !ZrParser_RangeEvidence_AnchorAt(owner, construct->location, last + 1U, &beyond);
    passed = interval && anchors && stops &&
            construction_range_physical_is(construct->location, g_constructionRangeHead, 0U, hasArgs ? 3U : 7U) &&
            construct->location.flags == (hasArgs ? ZR_FILE_RANGE_FLAG_COMPOSITE : 0U) &&
            construct->location.originKey == nativeRangeEvidence[0].originKey;
    printf("Construction range own=%u args=%u interval=%u..%u flags=%u anchor=%llu..%llu anchors=%u stop=%u proof=%u\n",
            (unsigned)isOwn, (unsigned)hasArgs, (unsigned)description.first, (unsigned)description.last,
            (unsigned)construct->location.flags, (unsigned long long)construct->location.start.offset,
            (unsigned long long)construct->location.end.offset, (unsigned)anchors, (unsigned)stops, (unsigned)passed);
    token_source_construction_range_cleanup();
    return passed;
}

static void test_new_construct_keeps_mixed_call_close_evidence(void) {
    if (!construction_range_case(ZR_FALSE, ZR_TRUE)) {
        TEST_FAIL_MESSAGE("new construction must retain interval [1,5] through the real foreign closing parenthesis");
    }
}

static void test_own_construct_keeps_mixed_call_close_evidence(void) {
    if (!construction_range_case(ZR_TRUE, ZR_TRUE)) {
        TEST_FAIL_MESSAGE("own construction must retain interval [1,5] through the real foreign closing parenthesis");
    }
}

static void test_new_construct_without_args_stops_at_target_evidence(void) {
    if (!construction_range_case(ZR_FALSE, ZR_FALSE)) {
        TEST_FAIL_MESSAGE("Argument-free new construction must end at interval [1,2] without absorbing a foreign semicolon");
    }
}

static const SZrConstructionRangeToken g_constructionRangeGenericHeadTokens[] = {
    {ZR_TK_NEW, 0U, 3U, "new", 0},
    {ZR_TK_IDENTIFIER, 4U, 7U, "Box", 0},
    {ZR_TK_LESS_THAN, 7U, 8U, "<", 0},
    {ZR_TK_IDENTIFIER, 8U, 11U, "int", 0},
    {ZR_TK_GREATER_THAN, 11U, 12U, ">", 0}
};

static void construction_range_require_generic_angle(SZrTokenStream *stream,
        const SZrTokenTree *tree, const SZrConstructionRangeToken *expected) {
    SZrTokenSpan span;
    SZrTokenText text;
    SZrTokenNativeSemInfo info;
    const char *bytes;

    TEST_ASSERT_NOT_NULL_MESSAGE(tree, "Fixture prerequisite: generic construction angle is missing");
    span = ZrParser_TokenTree_Span(tree);
    text = ZrParser_TokenTree_Text(tree);
    bytes = ZrParser_TokenStream_TextBytes(stream, text);
    construction_range_require_span(stream, span, g_constructionRangeHead,
            expected->start, expected->end, expected->raw);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenTree_Kind(tree) == ZR_TOKEN_TREE_PUNCT &&
            ZrParser_TokenTree_PunctCodepoint(tree) == (TZrUInt32)(unsigned char)expected->raw[0] &&
            bytes != ZR_NULL && text.sourceId == span.sourceId && text.offset == expected->start &&
            text.length == 1U && memcmp(bytes, expected->raw, 1U) == 0 &&
            ZrParser_SyntaxContext_Equal(ZrParser_TokenTree_Context(tree),
                    ZrParser_SyntaxContext_TableRoot(g_constructionRange.table)) &&
            ZrParser_TokenTree_NativeSemInfo(tree, &info) && info.token == expected->token,
            "Fixture prerequisite: generic construction angle payload, raw text or root context changed");
}

static SZrTokenStream *construction_range_generic_input(void) {
    const char *suffix = "; return 9;";
    SZrTokenStream *left;
    SZrTokenStream *right;
    SZrTokenStream **slot;

    g_constructionRange.table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_constructionRange.table,
            "Fixture prerequisite: generic construction context table allocation failed");
    memset(g_constructionRange.paddedSource, ' ', ZR_TEST_CONSTRUCTION_RANGE_PADDING);
    memcpy(g_constructionRange.paddedSource + ZR_TEST_CONSTRUCTION_RANGE_PADDING,
            suffix, strlen(suffix) + 1U);
    left = construction_range_from_text(g_constructionRangeHead, "new Box<int>");
    right = construction_range_from_text(g_constructionRangeTail, g_constructionRange.paddedSource);
    slot = construction_range_stream_slot();
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Count(left) == 5U &&
            ZrParser_TokenStream_Count(right) == 4U &&
            ZrParser_TokenStream_Concat(left, right, ZR_NULL, slot) && *slot != ZR_NULL &&
            ZrParser_TokenStream_IsSealed(*slot),
            "Fixture prerequisite: generic construction source concatenation failed");
    return *slot;
}

static void construction_range_require_generic_input(SZrTokenStream *stream) {
    TZrTokenSourceId headId;
    TZrTokenSourceId tailId;
    TZrSize sourceLength = 0U;
    const char *source;
    TZrSize index;

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(9U, ZrParser_TokenStream_Count(stream),
            "Fixture prerequisite: generic construction must have nine ungrouped root leaves");
    headId = ZrParser_TokenTree_Span(ZrParser_TokenStream_At(stream, 0U)).sourceId;
    tailId = ZrParser_TokenTree_Span(ZrParser_TokenStream_At(stream, 5U)).sourceId;
    TEST_ASSERT_TRUE_MESSAGE(headId != tailId,
            "Fixture prerequisite: generic construction head and tail must have distinct sources");
    for (index = 0U; index < 9U; ++index) {
        const SZrTokenTree *tree = ZrParser_TokenStream_At(stream, index);
        const SZrConstructionRangeToken *expected = index < 5U ?
                &g_constructionRangeGenericHeadTokens[index] : &g_constructionRangeNoArgsTokens[index - 5U];
        if (index == 2U || index == 4U) {
            construction_range_require_generic_angle(stream, tree, expected);
        } else {
            construction_range_require_leaf(stream, tree,
                    index < 5U ? g_constructionRangeHead : g_constructionRangeTail,
                    expected->token, expected->start, expected->end, expected->raw, expected->integer);
        }
        TEST_ASSERT_EQUAL_UINT_MESSAGE(index < 5U ? headId : tailId,
                ZrParser_TokenTree_Span(tree).sourceId,
                "Fixture prerequisite: generic construction leaf source changed");
    }
    source = ZrParser_TokenStream_SourceBytes(stream, headId, &sourceLength);
    TEST_ASSERT_TRUE_MESSAGE(source != ZR_NULL && sourceLength == 12U &&
            memcmp(source, "new Box<int>", 12U) == 0,
            "Fixture prerequisite: generic construction complete head source changed");
    source = ZrParser_TokenStream_SourceBytes(stream, tailId, &sourceLength);
    TEST_ASSERT_TRUE_MESSAGE(source != ZR_NULL && sourceLength == 75U &&
            memcmp(source, g_constructionRange.paddedSource, sourceLength) == 0,
            "Fixture prerequisite: generic construction complete padded tail source changed");
    construction_range_require_span(stream, ZrParser_TokenStream_EndSpan(stream),
            g_constructionRangeTail, 75U, 75U, "");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(tailId, ZrParser_TokenStream_EndSpan(stream).sourceId,
            "Fixture prerequisite: generic construction EOS source changed");
}

static void construction_range_require_generic_native(SZrFileRange *nativeRangeEvidence) {
    SZrAstOwnership *owner = g_constructionRange.ast->ownership;
    SZrFileRange eos;
    TZrSize index;

    for (index = 0U; index < 10U; ++index) {
        const SZrConstructionRangeToken *expected = index < 5U ?
                &g_constructionRangeGenericHeadTokens[index] : &g_constructionRangeNoArgsTokens[index - 5U];
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_RangeEvidence_Location(owner, (TZrUInt32)index + 1U,
                &nativeRangeEvidence[index]) &&
                ZrParser_RangeEvidence_Validate(owner, nativeRangeEvidence[index]) &&
                construction_range_interval_is(owner, nativeRangeEvidence[index],
                        (TZrUInt32)index + 1U, (TZrUInt32)index + 1U) &&
                construction_range_physical_is(nativeRangeEvidence[index],
                        index < 5U ? g_constructionRangeHead : g_constructionRangeTail,
                        expected->start, expected->end) &&
                nativeRangeEvidence[index].flags == 0U && nativeRangeEvidence[index].originKey != 0U &&
                nativeRangeEvidence[index].ownerKey == ZrParser_Ownership_Key(owner),
                "Fixture prerequisite: generic construction natural native atom evidence changed");
        TEST_ASSERT_TRUE_MESSAGE(index < 5U ?
                nativeRangeEvidence[index].originKey == nativeRangeEvidence[0].originKey :
                (nativeRangeEvidence[index].originKey == nativeRangeEvidence[5].originKey &&
                 nativeRangeEvidence[index].originKey != nativeRangeEvidence[0].originKey),
                "Fixture prerequisite: generic construction native origins must follow their real sources");
    }
    construction_range_require_span(g_constructionRange.streams[2], g_constructionRange.parser.lexer->t.span,
            g_constructionRangeTail, 75U, 75U, "");
    TEST_ASSERT_TRUE_MESSAGE(g_constructionRange.parser.lexer->t.firstNativeOrdinal == 10U &&
            g_constructionRange.parser.lexer->t.lastNativeOrdinal == 10U &&
            g_constructionRange.parser.lexer->t.rawLength == 0U &&
            g_constructionRange.parser.lexer->t.span.sourceId ==
                    ZrParser_TokenStream_EndSpan(g_constructionRange.streams[2]).sourceId &&
            ZrParser_RangeEvidence_TokenLocation(owner, g_constructionRange.parser.lexer->t.evidenceIndex,
                    10U, 10U, &eos) && construction_range_same_atom(owner, eos, nativeRangeEvidence[9]),
            "Fixture prerequisite: generic construction lexer EOS ordinal or source changed");
}

static SZrAstNode *construction_range_require_generic_construct(SZrFileRange *nativeRangeEvidence) {
    SZrAstNode *statement = g_constructionRange.ast->data.script.statements->nodes[0];
    SZrAstNode *construct;
    SZrAstNode *target;
    SZrAstNode *generic;
    SZrAstNode *parameter;
    SZrAstNode *identifier;
    SZrAstNode *next;
    SZrFileRange wholeRange;
    SZrFileRange firstAnchor;
    SZrFileRange lastAnchor;
    SZrFileRange beyond;
    SZrAstOwnership *owner = g_constructionRange.ast->ownership;

    construction_range_require_node(statement, ZR_AST_EXPRESSION_STATEMENT);
    construct = statement->data.expressionStatement.expr;
    construction_range_require_node(construct, ZR_AST_CONSTRUCT_EXPRESSION);
    target = construct->data.constructExpression.target;
    construction_range_require_node(target, ZR_AST_TYPE);
    generic = target->data.type.name;
    construction_range_require_node(generic, ZR_AST_GENERIC_TYPE);
    TEST_ASSERT_TRUE_MESSAGE(target->data.type.subType == ZR_NULL &&
            generic->data.genericType.name != ZR_NULL && generic->data.genericType.name->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(generic->data.genericType.name->name), "Box") == 0 &&
            ZrParser_SyntaxContext_Equal(generic->data.genericType.name->syntaxContext,
                    ZrParser_SyntaxContext_TableRoot(g_constructionRange.table)) &&
            generic->data.genericType.params != ZR_NULL && generic->data.genericType.params->nodes != ZR_NULL &&
            generic->data.genericType.params->count == 1U &&
            construct->data.constructExpression.args != ZR_NULL &&
            construct->data.constructExpression.args->count == 0U &&
            construct->data.constructExpression.isNew && !construct->data.constructExpression.isUsing &&
            !construct->data.constructExpression.isResourceSurface &&
            construct->data.constructExpression.ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE &&
            construct->data.constructExpression.builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_NONE,
            "Fixture prerequisite: generic construction target, empty arguments or ownership flags changed");
    parameter = generic->data.genericType.params->nodes[0];
    construction_range_require_node(parameter, ZR_AST_TYPE);
    identifier = parameter->data.type.name;
    construction_range_require_node(identifier, ZR_AST_IDENTIFIER_LITERAL);
    TEST_ASSERT_TRUE_MESSAGE(parameter->data.type.subType == ZR_NULL && identifier->data.identifier.name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(identifier->data.identifier.name), "int") == 0 &&
            ZrParser_SyntaxContext_Equal(identifier->data.identifier.syntaxContext,
                    ZrParser_SyntaxContext_TableRoot(g_constructionRange.table)) &&
            construction_range_same_atom(owner, identifier->location, nativeRangeEvidence[3]),
            "Fixture prerequisite: generic construction must preserve the real int type argument");
    wholeRange = generic->data.genericType.wholeRange;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_RangeEvidence_Validate(owner, wholeRange) &&
            construction_range_interval_is(owner, wholeRange, 2U, 5U) &&
            construction_range_physical_is(wholeRange, g_constructionRangeHead, 4U, 12U) &&
            wholeRange.flags == 0U && wholeRange.originKey == nativeRangeEvidence[1].originKey &&
            wholeRange.ownerKey == ZrParser_Ownership_Key(owner) &&
            ZrParser_RangeEvidence_AnchorAt(owner, wholeRange, 0U, &firstAnchor) &&
            ZrParser_RangeEvidence_AnchorAt(owner, wholeRange, 3U, &lastAnchor) &&
            construction_range_same_atom(owner, firstAnchor, nativeRangeEvidence[1]) &&
            construction_range_same_atom(owner, lastAnchor, nativeRangeEvidence[4]) &&
            !ZrParser_RangeEvidence_AnchorAt(owner, wholeRange, 4U, &beyond),
            "Fixture prerequisite: generic type wholeRange must end at its real closing angle [2,5]");
    next = construction_range_require_return();
    TEST_ASSERT_TRUE_MESSAGE(construction_range_same_atom(owner, next->data.returnStatement.expr->location,
            nativeRangeEvidence[7]) && construction_range_interval_is(owner, next->location, 7U, 9U) &&
            construction_range_physical_is(next->location, g_constructionRangeTail, 66U, 75U) &&
            next->location.flags == 0U && next->location.originKey == nativeRangeEvidence[5].originKey &&
            next->location.ownerKey == ZrParser_Ownership_Key(owner),
            "Fixture prerequisite: generic construction must retain the complete second return and its evidence");
    return construct;
}

static TZrBool construction_range_generic_case(void) {
    SZrFileRange nativeRangeEvidence[ZR_TEST_CONSTRUCTION_RANGE_ATOMS];
    SZrFileRange firstAnchor;
    SZrFileRange lastAnchor;
    SZrFileRange beyond;
    SZrTokenStream *stream;
    SZrAstNode *construct;
    SZrAstNode *target;
    SZrAstOwnership *owner;
    SZrParserRangeEvidenceDescription targetDescription = {0};
    SZrParserRangeEvidenceDescription constructDescription = {0};
    TZrBool targetPassed;
    TZrBool constructPassed;

    construction_range_text_control(ZR_FALSE);
    stream = construction_range_generic_input();
    construction_range_require_generic_input(stream);
    construction_range_entry_name("construction_generic_expansion.zr");
    ZrParser_State_InitWithTokenStream(&g_constructionRange.parser, g_state, stream,
            (SZrString *)g_constructionRange.entryNameRoot.object);
    construction_range_finish_parse();
    construction_range_require_generic_input(stream);
    construction_range_require_generic_native(nativeRangeEvidence);
    construct = construction_range_require_generic_construct(nativeRangeEvidence);
    target = construct->data.constructExpression.target;
    owner = construct->ownership;
    targetPassed = ZrParser_RangeEvidence_Describe(owner, target->location, &targetDescription) &&
            targetDescription.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL && targetDescription.first == 2U &&
            targetDescription.last == 5U &&
            construction_range_physical_is(target->location, g_constructionRangeHead, 4U, 12U) &&
            target->location.flags == 0U && target->location.originKey == nativeRangeEvidence[1].originKey &&
            target->location.ownerKey == ZrParser_Ownership_Key(owner) &&
            ZrParser_RangeEvidence_AnchorAt(owner, target->location, 0U, &firstAnchor) &&
            ZrParser_RangeEvidence_AnchorAt(owner, target->location, 3U, &lastAnchor) &&
            construction_range_same_atom(owner, firstAnchor, nativeRangeEvidence[1]) &&
            construction_range_same_atom(owner, lastAnchor, nativeRangeEvidence[4]) &&
            !ZrParser_RangeEvidence_AnchorAt(owner, target->location, 4U, &beyond);
    constructPassed = ZrParser_RangeEvidence_Describe(owner, construct->location, &constructDescription) &&
            constructDescription.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL && constructDescription.first == 1U &&
            constructDescription.last == 5U &&
            construction_range_physical_is(construct->location, g_constructionRangeHead, 0U, 12U) &&
            construct->location.flags == 0U && construct->location.originKey == nativeRangeEvidence[0].originKey &&
            construct->location.ownerKey == ZrParser_Ownership_Key(owner) &&
            ZrParser_RangeEvidence_AnchorAt(owner, construct->location, 0U, &firstAnchor) &&
            ZrParser_RangeEvidence_AnchorAt(owner, construct->location, 4U, &lastAnchor) &&
            construction_range_same_atom(owner, firstAnchor, nativeRangeEvidence[0]) &&
            construction_range_same_atom(owner, lastAnchor, nativeRangeEvidence[4]) &&
            !ZrParser_RangeEvidence_AnchorAt(owner, construct->location, 5U, &beyond);
    printf("Generic construction target=%u..%u construct=%u..%u flags=%u/%u proof=%u/%u\n",
            (unsigned)targetDescription.first, (unsigned)targetDescription.last,
            (unsigned)constructDescription.first, (unsigned)constructDescription.last,
            (unsigned)target->location.flags, (unsigned)construct->location.flags,
            (unsigned)targetPassed, (unsigned)constructPassed);
    token_source_construction_range_cleanup();
    return targetPassed && constructPassed;
}

static void test_new_generic_construct_without_args_stops_at_closing_angle_evidence(void) {
    if (!construction_range_generic_case()) {
        TEST_FAIL_MESSAGE("Argument-free generic new target [2,5] and construction [1,5] must exclude the foreign semicolon");
    }
}

#endif
