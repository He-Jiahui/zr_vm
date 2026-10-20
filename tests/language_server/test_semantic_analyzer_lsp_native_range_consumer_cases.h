#ifndef ZR_TEST_SEMANTIC_ANALYZER_LSP_NATIVE_RANGE_CONSUMER_CASES_H
#define ZR_TEST_SEMANTIC_ANALYZER_LSP_NATIVE_RANGE_CONSUMER_CASES_H

#include <stddef.h>

#include "interface/lsp_interface_internal.h"
#include "parser/parser_ownership.h"
#include "parser/parser_range_evidence.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/token_stream.h"
#include "lsp_query_result_cleanup.h"

enum {
    ZR_TEST_LSP_NATIVE_RANGE_SOURCES = 2,
    ZR_TEST_LSP_NATIVE_RANGE_STREAMS = 3,
    ZR_TEST_LSP_NATIVE_RANGE_LABEL_SIZE = 256
};

typedef struct SZrLspNativeRangeFixture {
    SZrLspContext *context;
    SZrSemanticAnalyzer *analyzer;
    TZrBool analyzerRegistered;
    SZrAstNode *ast;
    SZrParserState parser;
    TZrBool parserInitialized;
    SZrSyntaxContextTable *table;
    SZrTokenStream *streams[ZR_TEST_LSP_NATIVE_RANGE_STREAMS];
    SZrTokenStream *input;
    SZrString *uris[ZR_TEST_LSP_NATIVE_RANGE_SOURCES];
    SZrGcRootLease uriRoots[ZR_TEST_LSP_NATIVE_RANGE_SOURCES];
    SZrFileVersionContentSnapshot snapshots[ZR_TEST_LSP_NATIVE_RANGE_SOURCES];
    SZrLspSignatureHelp *help;
    SZrArray references;
} SZrLspNativeRangeFixture;

static void lsp_native_range_fixture_free(SZrState *state, SZrLspNativeRangeFixture *fixture) {
    TZrSize index;

    if (fixture->help != ZR_NULL) {
        ZrLanguageServer_LspSignatureHelp_Free(state, fixture->help);
    }
    free_local_reference_projection_results(state, &fixture->references, ZR_NULL);
    for (index = ZR_TEST_LSP_NATIVE_RANGE_SOURCES; index > 0U; --index) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &fixture->snapshots[index - 1U]);
    }
    /* Analyze borrows this AST; its context must release the analyzer first. */
    if (fixture->analyzer != ZR_NULL && !fixture->analyzerRegistered) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, fixture->analyzer);
    }
    if (fixture->context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, fixture->context);
    }
    ZrParser_Ast_Free(state, fixture->ast);
    if (fixture->parserInitialized) {
        ZrParser_State_Free(&fixture->parser);
    }
    for (index = ZR_TEST_LSP_NATIVE_RANGE_SOURCES; index > 0U; --index) {
        ZrCore_Gc_RootLeaseRelease(state->global, &fixture->uriRoots[index - 1U]);
    }
    for (index = ZR_TEST_LSP_NATIVE_RANGE_STREAMS; index > 0U; --index) {
        ZrParser_TokenStream_Release(fixture->streams[index - 1U]);
    }
    ZrParser_SyntaxContextTable_Release(fixture->table);
    memset(fixture, 0, sizeof(*fixture));
}

static TZrBool lsp_native_range_string_is(SZrString *string, const char *text) {
    return string != ZR_NULL && ZrCore_String_GetNativeString(string) != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(string), text) == 0;
}

static SZrFilePosition lsp_native_range_position(const char *source, TZrSize offset) {
    SZrFilePosition position = ZrParser_FilePosition_Create(offset, 1, 1);
    TZrSize index;

    for (index = 0U; index < offset; ++index) {
        if (source[index] == '\n') {
            ++position.line;
            position.column = 1;
        } else {
            ++position.column;
        }
    }
    return position;
}

static TZrBool lsp_native_range_same_position(SZrFilePosition a, SZrFilePosition b) {
    return a.offset == b.offset && a.line == b.line && a.column == b.column;
}

static TZrBool lsp_native_range_span_is(
        const SZrTokenStream *stream, SZrTokenSpan span,
        const char *uri, const char *source, TZrSize start, TZrSize end) {
    TZrSize nameLength = 0U;
    TZrSize sourceLength = 0U;
    const char *name = ZrParser_TokenStream_SourceName(stream, span.sourceId, &nameLength);
    const char *bytes = ZrParser_TokenStream_SourceBytes(stream, span.sourceId, &sourceLength);
    SZrFilePosition first;
    SZrFilePosition last;

    if (!ZrParser_TokenSpan_IsValid(span) || name == ZR_NULL || bytes == ZR_NULL ||
        nameLength != strlen(uri) || memcmp(name, uri, nameLength) != 0 ||
        sourceLength != strlen(source) || memcmp(bytes, source, sourceLength) != 0 ||
        start > end || end > sourceLength || span.startOffset != start || span.endOffset != end) {
        return ZR_FALSE;
    }
    first = lsp_native_range_position(source, start);
    last = lsp_native_range_position(source, end);
    return span.startLine == first.line && span.startColumn == first.column &&
            span.endLine == last.line && span.endColumn == last.column;
}

static const SZrTokenTree *lsp_native_range_find_tree_in(
        const SZrTokenStream *stream, const SZrTokenTree *tree,
        const char *uri, const char *source, TZrSize start, TZrSize end) {
    TZrSize index;
    const SZrTokenTree *found;

    if (tree == ZR_NULL) {
        return ZR_NULL;
    }
    if (lsp_native_range_span_is(stream, ZrParser_TokenTree_Span(tree), uri, source, start, end)) {
        return tree;
    }
    if (ZrParser_TokenTree_Kind(tree) == ZR_TOKEN_TREE_GROUP) {
        for (index = 0U; index < ZrParser_TokenTree_GroupChildCount(tree); ++index) {
            found = lsp_native_range_find_tree_in(stream, ZrParser_TokenTree_GroupChildAt(tree, index),
                    uri, source, start, end);
            if (found != ZR_NULL) {
                return found;
            }
        }
    }
    return ZR_NULL;
}

static const SZrTokenTree *lsp_native_range_find_tree(
        SZrLspNativeRangeFixture *fixture, const char *uri, const char *source,
        TZrSize start, TZrSize end) {
    TZrSize index;
    const SZrTokenTree *found;

    for (index = 0U; index < ZrParser_TokenStream_Count(fixture->input); ++index) {
        found = lsp_native_range_find_tree_in(fixture->input,
                ZrParser_TokenStream_At(fixture->input, index), uri, source, start, end);
        if (found != ZR_NULL) {
            return found;
        }
    }
    return ZR_NULL;
}

static TZrBool lsp_native_range_leaf_is(
        SZrLspNativeRangeFixture *fixture, const SZrTokenTree *tree,
        EZrToken token, const char *raw, TZrInt64 integer) {
    SZrTokenNativeSemInfo info;
    SZrTokenText text;
    SZrTokenSpan span;
    const char *bytes;

    if (tree == ZR_NULL || ZrParser_TokenTree_Kind(tree) == ZR_TOKEN_TREE_GROUP ||
        !ZrParser_SyntaxContext_Equal(ZrParser_TokenTree_Context(tree),
                ZrParser_SyntaxContext_TableRoot(fixture->table)) ||
        !ZrParser_TokenTree_NativeSemInfo(tree, &info) || info.token != token) {
        return ZR_FALSE;
    }
    span = ZrParser_TokenTree_Span(tree);
    text = ZrParser_TokenTree_Text(tree);
    bytes = ZrParser_TokenStream_TextBytes(fixture->input, text);
    return bytes != ZR_NULL && text.sourceId == span.sourceId &&
            text.offset == span.startOffset && text.length == span.endOffset - span.startOffset &&
            text.length == strlen(raw) && memcmp(bytes, raw, text.length) == 0 &&
            (token != ZR_TK_INTEGER || info.intValue == integer);
}

static TZrBool lsp_native_range_group_is(
        SZrLspNativeRangeFixture *fixture, const SZrTokenTree *tree,
        const char *uri, const char *source, TZrSize start, TZrSize end) {
    return tree != ZR_NULL && end >= start + 2U && source[start] == '(' && source[end - 1U] == ')' &&
            ZrParser_TokenTree_Kind(tree) == ZR_TOKEN_TREE_GROUP &&
            ZrParser_TokenTree_GroupDelimiter(tree) == ZR_TOKEN_DELIMITER_PAREN &&
            ZrParser_SyntaxContext_Equal(ZrParser_TokenTree_Context(tree),
                    ZrParser_SyntaxContext_TableRoot(fixture->table)) &&
            lsp_native_range_span_is(fixture->input, ZrParser_TokenTree_GroupOpenSpan(tree),
                    uri, source, start, start + 1U) &&
            lsp_native_range_span_is(fixture->input, ZrParser_TokenTree_GroupCloseSpan(tree),
                    uri, source, end - 1U, end);
}

static TZrBool lsp_native_range_physical_is(
        SZrFileRange range, const char *uri, const char *source, TZrSize start, TZrSize end) {
    return end <= strlen(source) && start <= end && lsp_native_range_string_is(range.source, uri) &&
            lsp_native_range_same_position(range.start, lsp_native_range_position(source, start)) &&
            lsp_native_range_same_position(range.end, lsp_native_range_position(source, end));
}

static TZrBool lsp_native_range_same_range(SZrFileRange a, SZrFileRange b) {
    return a.source == b.source && a.originKey == b.originKey && a.ownerKey == b.ownerKey &&
            a.evidenceIndex == b.evidenceIndex && a.flags == b.flags &&
            lsp_native_range_same_position(a.start, b.start) && lsp_native_range_same_position(a.end, b.end);
}

static TZrBool lsp_native_range_interval_is(
        SZrLspNativeRangeFixture *fixture, SZrFileRange range,
        const char *uri, const char *source, TZrSize start, TZrSize end,
        SZrTokenSpan firstSpan, SZrTokenSpan lastSpan, TZrSize atomCount) {
    SZrAstOwnership *owner = fixture->ast->ownership;
    SZrParserRangeEvidenceDescription description = {0};
    SZrFileRange first;
    SZrFileRange last;
    SZrFileRange replay;

    /* Only read the parser's committed ledger; never manufacture an interval. */
    return lsp_native_range_physical_is(range, uri, source, start, end) && range.flags == 0U &&
            range.ownerKey == ZrParser_Ownership_Key(owner) && range.originKey != 0U &&
            ZrParser_RangeEvidence_Validate(owner, range) &&
            ZrParser_RangeEvidence_Describe(owner, range, &description) &&
            description.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL && description.first <= description.last &&
            (atomCount == 0U || (TZrSize)(description.last - description.first) + 1U == atomCount) &&
            ZrParser_RangeEvidence_TokenLocation(owner, range.evidenceIndex,
                    description.first, description.last, &replay) &&
            lsp_native_range_same_range(range, replay) &&
            ZrParser_RangeEvidence_AnchorAt(owner, range, 0U, &first) &&
            ZrParser_RangeEvidence_AnchorAt(owner, range, description.last - description.first, &last) &&
            first.source == range.source && last.source == range.source &&
            first.originKey == range.originKey && last.originKey == range.originKey &&
            first.ownerKey == range.ownerKey && last.ownerKey == range.ownerKey &&
            lsp_native_range_physical_is(first, uri, source, firstSpan.startOffset, firstSpan.endOffset) &&
            lsp_native_range_physical_is(last, uri, source, lastSpan.startOffset, lastSpan.endOffset);
}

static TZrBool lsp_native_range_node_is(
        SZrLspNativeRangeFixture *fixture, const SZrAstNode *node, EZrAstNodeType type) {
    return node != ZR_NULL && node->type == type && node->ownership == fixture->ast->ownership &&
            node->ownership != ZR_NULL && ZrParser_RangeEvidence_Validate(node->ownership, node->location);
}

static const char *lsp_native_range_fixture_open(
        SZrState *state, SZrLspNativeRangeFixture *fixture,
        const char *headUri, const char *headSource, const char *tailUri, const char *tailSource) {
    const char *uris[ZR_TEST_LSP_NATIVE_RANGE_SOURCES] = {headUri, tailUri};
    const char *sources[ZR_TEST_LSP_NATIVE_RANGE_SOURCES] = {headSource, tailSource};
    TZrSize count = tailUri != ZR_NULL ? 2U : 1U;
    TZrSize index;

    fixture->context = ZrLanguageServer_LspContext_New(state);
    fixture->table = ZrParser_SyntaxContextTable_Create(state->global, ZR_NULL);
    if (fixture->context == ZR_NULL || fixture->context->parser == ZR_NULL || fixture->table == ZR_NULL) {
        return "Fixture prerequisite: native LSP context and syntax table must initialize";
    }
    for (index = 0U; index < count; ++index) {
        SZrFileVersion *version;
        SZrFileVersionContentSnapshot *snapshot = &fixture->snapshots[index];

        fixture->uris[index] = ZrCore_String_CreateFromNative(state, (TZrNativeString)uris[index]);
        if (fixture->uris[index] == ZR_NULL ||
            !ZrCore_Gc_RootLeaseAcquire(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->uris[index]),
                    &fixture->uriRoots[index]) ||
            !ZrLanguageServer_IncrementalParser_UpdateOpenDocument(state, fixture->context->parser,
                    fixture->uris[index], sources[index], strlen(sources[index]), 1U)) {
            return "Fixture prerequisite: native LSP source URI and real document overlay must initialize";
        }
        version = ZrLanguageServer_Lsp_GetDocumentFileVersion(fixture->context, fixture->uris[index]);
        if (version == ZR_NULL || version->uri == ZR_NULL ||
            !ZrCore_String_Equal(version->uri, fixture->uris[index]) || version->version != 1U ||
            !version->isOpenDocument || version->usesFallbackAst || version->isDesynchronized ||
            !ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, version, snapshot) ||
            snapshot->contentBlock == ZR_NULL || snapshot->content == ZR_NULL ||
            snapshot->uri != version->uri || snapshot->version != 1U || !snapshot->isOpenDocument ||
            snapshot->usesFallbackAst || snapshot->contentLength != strlen(sources[index]) ||
            memcmp(snapshot->content, sources[index], snapshot->contentLength) != 0) {
            return "Fixture prerequisite: native LSP source snapshot, version and URI must match the real text";
        }
        if (!ZrParser_TokenStream_FromText(state, fixture->table, uris[index], strlen(uris[index]),
                    sources[index], strlen(sources[index]), ZR_NULL, &fixture->streams[index]) ||
            fixture->streams[index] == ZR_NULL || !ZrParser_TokenStream_IsSealed(fixture->streams[index]) ||
            ZrParser_TokenStream_ContextTable(fixture->streams[index]) != fixture->table) {
            return "Fixture prerequisite: native LSP source must use a real sealed FromText stream";
        }
    }
    fixture->input = fixture->streams[0];
    if (count == 2U) {
        if (!ZrParser_TokenStream_Concat(fixture->streams[0], fixture->streams[1], ZR_NULL,
                    &fixture->streams[2]) || fixture->streams[2] == ZR_NULL ||
            !ZrParser_TokenStream_IsSealed(fixture->streams[2]) ||
            ZrParser_TokenStream_ContextTable(fixture->streams[2]) == ZR_NULL) {
            return "Fixture prerequisite: mixed LSP source must use a real sealed Concat stream";
        }
        fixture->input = fixture->streams[2];
    }
    ZrParser_State_InitWithTokenStream(&fixture->parser, state, fixture->input, fixture->uris[0]);
    fixture->parserInitialized = ZR_TRUE;
    fixture->parser.suppressErrorOutput = ZR_TRUE;
    if (fixture->parser.lexer == ZR_NULL || fixture->parser.hasError) {
        return "Fixture prerequisite: native LSP parser cursor must initialize";
    }
    fixture->ast = ZrParser_ParseWithState(&fixture->parser);
    if (fixture->ast == ZR_NULL || fixture->ast->type != ZR_AST_SCRIPT || fixture->ast->ownership == ZR_NULL ||
        fixture->ast->ownership != fixture->parser.lexer->ownership || fixture->parser.hasError ||
        fixture->parser.hasFatalError || fixture->parser.hasNestingError ||
        fixture->parser.lexer->hasSourceEvidenceError || fixture->parser.lexer->t.hasLexError ||
        fixture->parser.lexer->t.token != ZR_TK_EOS || fixture->parser.lexer->tokenStream != fixture->input ||
        fixture->ast->data.script.statements == ZR_NULL || fixture->ast->data.script.statements->nodes == ZR_NULL ||
        fixture->ast->data.script.tokenStream != fixture->input ||
        fixture->ast->data.script.syntaxContextTable != ZrParser_TokenStream_ContextTable(fixture->input) ||
        ZrParser_Ownership_Evidence(fixture->ast->ownership) == ZR_NULL) {
        return "Fixture prerequisite: native LSP parser must reach EOS with the real owned AST and ledger";
    }
    fixture->analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, fixture->context, fixture->uris[0]);
    if (fixture->analyzer == ZR_NULL) {
        return "Fixture prerequisite: native LSP analyzer must initialize in the real request context";
    }
    fixture->analyzerRegistered =
            ZrLanguageServer_Lsp_FindAnalyzer(state, fixture->context, fixture->uris[0]) == fixture->analyzer;
    if (!fixture->analyzerRegistered) {
        return "Fixture prerequisite: native LSP analyzer must be registered in the real request context";
    }
    ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(fixture->analyzer, ZR_FALSE);
    if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, fixture->analyzer, fixture->ast) ||
        fixture->analyzer->ast != fixture->ast || fixture->analyzer->ownedAst != ZR_NULL ||
        fixture->analyzer->borrowedAst != ZR_NULL || fixture->analyzer->compilerState == ZR_NULL ||
        fixture->analyzer->compilerState->hasError || fixture->analyzer->semanticContext == ZR_NULL ||
        fixture->analyzer->compilerState->semanticContext != fixture->analyzer->semanticContext ||
        ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, fixture->context, fixture->uris[0]) != fixture->analyzer ||
        fixture->analyzer->ast != fixture->ast) {
        return "Fixture prerequisite: public Analyze must publish the same live AST and semantic snapshot";
    }
    return ZR_NULL;
}

static TZrBool lsp_native_range_canonical_call_is(
        SZrLspNativeRangeFixture *fixture, SZrFilePosition position,
        SZrAstNode *primary, SZrAstNode *declaration, TZrSize argumentCount,
        char *label, TZrSize labelSize, TZrSymbolId *target) {
    SZrParserSemanticCallQuery query = {0};
    const SZrSemanticReferenceFact *targetDeclaration;
    SZrFileRange cursor = ZrParser_FileRange_Create(position, position, fixture->uris[0]);

    if (!ZrParser_SemanticQuery_CallAt(fixture->analyzer->semanticContext, cursor, ZR_NULL, &query) ||
        query.expression == ZR_NULL || query.expression->node != primary ||
        query.expression->exactness != ZR_SEMANTIC_FACT_EXACT || !query.expression->hasCallInfo ||
        !lsp_native_range_same_range(query.expression->range, primary->location) ||
        !query.hasResolvedTarget || query.targetSymbolId == ZR_SEMANTIC_ID_INVALID ||
        query.argumentCount != argumentCount || query.hasNamedArguments || query.isMemberCall ||
        ZrParser_CanonicalType_Find(fixture->analyzer->semanticContext, query.callableTypeId) == ZR_NULL ||
        !ZrParser_SemanticQuery_FormatCall(fixture->analyzer->semanticContext, &query, label, labelSize)) {
        return ZR_FALSE;
    }
    targetDeclaration = ZrParser_SemanticQuery_DeclarationOf(
            fixture->analyzer->semanticContext, query.targetSymbolId, ZR_NULL);
    if (targetDeclaration == ZR_NULL || targetDeclaration->node != declaration || !targetDeclaration->isResolved) {
        return ZR_FALSE;
    }
    *target = query.targetSymbolId;
    return ZR_TRUE;
}

static void test_lsp_signature_help_preserves_native_inner_call_close(SZrState *state) {
    static const char uri[] = "file:///lsp_native_signature_range.zr";
    static const char source[] =
            "fn make<T>(a:T,b:T):T{return b;}\n"
            "fn sink(v:int):int{return v;}\n"
            "fn use():int{return sink(make<int>(1, 2  ));}\n";
    const char *summary = "LSP Signature Help Preserves Native Inner Call Close";
    const char *failure;
    SZrTestTimer timer;
    SZrLspNativeRangeFixture fixture = {0};
    SZrAstNode *make;
    SZrAstNode *sink;
    SZrAstNode *use;
    SZrAstNode *body;
    SZrAstNode *statement;
    SZrAstNode *outer;
    SZrAstNode *outerCall;
    SZrAstNode *inner;
    SZrAstNode *innerCall;
    SZrAstNode *firstArg;
    SZrAstNode *lastArg;
    const SZrTokenTree *innerGroup;
    const SZrTokenTree *outerGroup;
    const SZrTokenTree *makeLeaf;
    const SZrTokenTree *sinkLeaf;
    const SZrTokenTree *oneLeaf;
    const SZrTokenTree *twoLeaf;
    SZrLspSignatureInformation **signature;
    SZrFilePosition cursor;
    SZrLspPosition position = {2, 41};
    TZrSize lineStart = sizeof("fn make<T>(a:T,b:T):T{return b;}\nfn sink(v:int):int{return v;}\n") - 1U;
    TZrSymbolId innerTarget = ZR_SEMANTIC_ID_INVALID;
    TZrSymbolId outerTarget = ZR_SEMANTIC_ID_INVALID;
    char innerLabel[ZR_TEST_LSP_NATIVE_RANGE_LABEL_SIZE];
    char outerLabel[ZR_TEST_LSP_NATIVE_RANGE_LABEL_SIZE];
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    failure = lsp_native_range_fixture_open(state, &fixture, uri, source, ZR_NULL, ZR_NULL);
    if (failure != ZR_NULL) {
        goto cleanup;
    }
    if (fixture.ast->data.script.statements->count != 3U) {
        failure = "Fixture prerequisite: native signature source must retain its three real functions";
        goto cleanup;
    }
    make = fixture.ast->data.script.statements->nodes[0];
    sink = fixture.ast->data.script.statements->nodes[1];
    use = fixture.ast->data.script.statements->nodes[2];
    if (!lsp_native_range_node_is(&fixture, make, ZR_AST_FUNCTION_DECLARATION) ||
        !lsp_native_range_node_is(&fixture, sink, ZR_AST_FUNCTION_DECLARATION) ||
        !lsp_native_range_node_is(&fixture, use, ZR_AST_FUNCTION_DECLARATION) ||
        make->data.functionDeclaration.name == ZR_NULL || sink->data.functionDeclaration.name == ZR_NULL ||
        use->data.functionDeclaration.name == ZR_NULL ||
        !lsp_native_range_string_is(make->data.functionDeclaration.name->name, "make") ||
        !lsp_native_range_string_is(sink->data.functionDeclaration.name->name, "sink") ||
        !lsp_native_range_string_is(use->data.functionDeclaration.name->name, "use")) {
        failure = "Fixture prerequisite: native signature declarations must keep their names, owner and complete ranges";
        goto cleanup;
    }
    body = use->data.functionDeclaration.body;
    if (!lsp_native_range_node_is(&fixture, body, ZR_AST_BLOCK) || body->data.block.body == ZR_NULL ||
        body->data.block.body->nodes == ZR_NULL || body->data.block.body->count != 1U ||
        !lsp_native_range_node_is(&fixture, body->data.block.body->nodes[0], ZR_AST_RETURN_STATEMENT)) {
        failure = "Fixture prerequisite: native signature use body must retain its real return statement";
        goto cleanup;
    }
    statement = body->data.block.body->nodes[0];
    outer = statement->data.returnStatement.expr;
    if (!lsp_native_range_node_is(&fixture, outer, ZR_AST_PRIMARY_EXPRESSION) ||
        !lsp_native_range_node_is(&fixture, outer->data.primaryExpression.property, ZR_AST_IDENTIFIER_LITERAL) ||
        !lsp_native_range_string_is(outer->data.primaryExpression.property->data.identifier.name, "sink") ||
        outer->data.primaryExpression.members == ZR_NULL || outer->data.primaryExpression.members->nodes == ZR_NULL ||
        outer->data.primaryExpression.members->count != 1U ||
        !lsp_native_range_node_is(&fixture, outer->data.primaryExpression.members->nodes[0], ZR_AST_FUNCTION_CALL)) {
        failure = "Fixture prerequisite: native signature outer PRIMARY must contain exactly the sink call";
        goto cleanup;
    }
    outerCall = outer->data.primaryExpression.members->nodes[0];
    if (outerCall->data.functionCall.args == ZR_NULL || outerCall->data.functionCall.args->nodes == ZR_NULL ||
        outerCall->data.functionCall.args->count != 1U || outerCall->data.functionCall.hasNamedArgs ||
        outerCall->data.functionCall.accessMode != ZR_POSTFIX_ACCESS_DIRECT) {
        failure = "Fixture prerequisite: native signature outer call must retain one positional argument";
        goto cleanup;
    }
    inner = outerCall->data.functionCall.args->nodes[0];
    if (!lsp_native_range_node_is(&fixture, inner, ZR_AST_PRIMARY_EXPRESSION) ||
        !lsp_native_range_node_is(&fixture, inner->data.primaryExpression.property, ZR_AST_IDENTIFIER_LITERAL) ||
        !lsp_native_range_string_is(inner->data.primaryExpression.property->data.identifier.name, "make") ||
        inner->data.primaryExpression.members == ZR_NULL || inner->data.primaryExpression.members->nodes == ZR_NULL ||
        inner->data.primaryExpression.members->count != 1U ||
        !lsp_native_range_node_is(&fixture, inner->data.primaryExpression.members->nodes[0], ZR_AST_FUNCTION_CALL)) {
        failure = "Fixture prerequisite: native signature inner PRIMARY must contain exactly the make call";
        goto cleanup;
    }
    innerCall = inner->data.primaryExpression.members->nodes[0];
    if (innerCall->data.functionCall.args == ZR_NULL || innerCall->data.functionCall.args->nodes == ZR_NULL ||
        innerCall->data.functionCall.args->count != 2U || innerCall->data.functionCall.hasNamedArgs ||
        innerCall->data.functionCall.accessMode != ZR_POSTFIX_ACCESS_DIRECT ||
        innerCall->data.functionCall.genericArguments == ZR_NULL ||
        innerCall->data.functionCall.genericArguments->nodes == ZR_NULL ||
        innerCall->data.functionCall.genericArguments->count != 1U) {
        failure = "Fixture prerequisite: native signature inner call must retain two arguments and one explicit generic argument";
        goto cleanup;
    }
    firstArg = innerCall->data.functionCall.args->nodes[0];
    lastArg = innerCall->data.functionCall.args->nodes[1];
    innerGroup = lsp_native_range_find_tree(&fixture, uri, source, lineStart + 34U, lineStart + 42U);
    outerGroup = lsp_native_range_find_tree(&fixture, uri, source, lineStart + 24U, lineStart + 43U);
    makeLeaf = lsp_native_range_find_tree(&fixture, uri, source, lineStart + 25U, lineStart + 29U);
    sinkLeaf = lsp_native_range_find_tree(&fixture, uri, source, lineStart + 20U, lineStart + 24U);
    oneLeaf = lsp_native_range_find_tree(&fixture, uri, source, lineStart + 35U, lineStart + 36U);
    twoLeaf = lsp_native_range_find_tree(&fixture, uri, source, lineStart + 38U, lineStart + 39U);
    if (!lsp_native_range_group_is(&fixture, innerGroup, uri, source, lineStart + 34U, lineStart + 42U) ||
        !lsp_native_range_group_is(&fixture, outerGroup, uri, source, lineStart + 24U, lineStart + 43U) ||
        !lsp_native_range_leaf_is(&fixture, makeLeaf, ZR_TK_IDENTIFIER, "make", 0) ||
        !lsp_native_range_leaf_is(&fixture, sinkLeaf, ZR_TK_IDENTIFIER, "sink", 0) ||
        !lsp_native_range_leaf_is(&fixture, oneLeaf, ZR_TK_INTEGER, "1", 1) ||
        !lsp_native_range_leaf_is(&fixture, twoLeaf, ZR_TK_INTEGER, "2", 2) ||
        !lsp_native_range_node_is(&fixture, firstArg, ZR_AST_INTEGER_LITERAL) ||
        !lsp_native_range_node_is(&fixture, lastArg, ZR_AST_INTEGER_LITERAL) ||
        firstArg->data.integerLiteral.value != 1 || lastArg->data.integerLiteral.value != 2 ||
        !ZrParser_SyntaxContext_Equal(inner->data.primaryExpression.property->data.identifier.syntaxContext,
                ZrParser_SyntaxContext_TableRoot(fixture.table)) ||
        !ZrParser_SyntaxContext_Equal(outer->data.primaryExpression.property->data.identifier.syntaxContext,
                ZrParser_SyntaxContext_TableRoot(fixture.table))) {
        failure = "Fixture prerequisite: native signature payloads, raw spans and syntax contexts must match the real input";
        goto cleanup;
    }
    if (!lsp_native_range_interval_is(&fixture, innerCall->location, uri, source,
                lineStart + 34U, lineStart + 42U, ZrParser_TokenTree_GroupOpenSpan(innerGroup),
                ZrParser_TokenTree_GroupCloseSpan(innerGroup), 5U) ||
        !lsp_native_range_interval_is(&fixture, inner->location, uri, source,
                lineStart + 25U, lineStart + 42U, ZrParser_TokenTree_Span(makeLeaf),
                ZrParser_TokenTree_GroupCloseSpan(innerGroup), 9U) ||
        !lsp_native_range_interval_is(&fixture, outerCall->location, uri, source,
                lineStart + 24U, lineStart + 43U, ZrParser_TokenTree_GroupOpenSpan(outerGroup),
                ZrParser_TokenTree_GroupCloseSpan(outerGroup), 11U) ||
        !lsp_native_range_interval_is(&fixture, firstArg->location, uri, source,
                lineStart + 35U, lineStart + 36U, ZrParser_TokenTree_Span(oneLeaf),
                ZrParser_TokenTree_Span(oneLeaf), 1U) ||
        !lsp_native_range_interval_is(&fixture, lastArg->location, uri, source,
                lineStart + 38U, lineStart + 39U, ZrParser_TokenTree_Span(twoLeaf),
                ZrParser_TokenTree_Span(twoLeaf), 1U)) {
        failure = "Fixture prerequisite: native signature AST ranges must retain their real complete ledger intervals";
        goto cleanup;
    }
    cursor = ZrLanguageServer_Lsp_GetDocumentFilePosition(fixture.context, fixture.uris[0], position);
    if (!lsp_native_range_same_position(cursor, lsp_native_range_position(source, lineStart + 41U)) ||
        source[cursor.offset] != ')' || lastArg->location.end.offset >= cursor.offset ||
        cursor.offset >= inner->location.end.offset ||
        !lsp_native_range_canonical_call_is(&fixture, cursor, inner, make, 2U,
                innerLabel, sizeof(innerLabel), &innerTarget) ||
        !lsp_native_range_canonical_call_is(&fixture, lsp_native_range_position(source, lineStart + 21U),
                outer, sink, 1U, outerLabel, sizeof(outerLabel), &outerTarget) ||
        innerTarget == outerTarget || strcmp(innerLabel, outerLabel) == 0) {
        failure = "Fixture prerequisite: native signature cursor must resolve exact distinct make and sink canonical targets";
        goto cleanup;
    }
    if (!ZrLanguageServer_Lsp_GetSignatureHelp(state, fixture.context, fixture.uris[0], position, &fixture.help) ||
        fixture.help == ZR_NULL || !fixture.help->signatures.isValid || fixture.help->signatures.length != 1U ||
        fixture.help->activeSignature != 0) {
        failure = "Fixture prerequisite: public native signature request must return the single canonical signature";
        goto cleanup;
    }
    signature = (SZrLspSignatureInformation **)ZrCore_Array_Get(&fixture.help->signatures, 0U);
    if (signature == ZR_NULL || *signature == ZR_NULL ||
        !lsp_native_range_string_is((*signature)->label, innerLabel) ||
        !(*signature)->parameters.isValid || (*signature)->parameters.length != 2U ||
        fixture.analyzer->ast != fixture.ast ||
        ZrLanguageServer_Lsp_FindAnalyzer(state, fixture.context, fixture.uris[0]) != fixture.analyzer) {
        failure = "Fixture prerequisite: canonical signature label and two parameters must remain correct despite context selection";
        goto cleanup;
    }
    passed = fixture.help->activeParameter == 1;

cleanup:
    lsp_native_range_fixture_free(state, &fixture);
    if (failure != ZR_NULL) {
        TEST_FAIL(timer, summary, failure);
    } else if (!passed) {
        TEST_FAIL(timer, summary,
                "Signature help at the complete native inner call close must keep the inner active parameter");
    } else {
        TEST_PASS(timer, summary);
    }
}

static TZrBool lsp_native_range_has_class_symbol(
        SZrLspNativeRangeFixture *fixture, SZrAstNode *declaration) {
    TZrSize index;
    TZrSize count = 0U;
    const SZrSemanticSymbolRecord *symbol;

    for (index = 0U; index < fixture->analyzer->semanticContext->symbols.length; ++index) {
        symbol = (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                &fixture->analyzer->semanticContext->symbols, index);
        if (symbol != ZR_NULL && symbol->astNode == declaration && symbol->id != ZR_SEMANTIC_ID_INVALID &&
            ZrParser_CanonicalType_Find(fixture->analyzer->semanticContext, symbol->typeId) != ZR_NULL) {
            ++count;
        }
    }
    return count == 1U;
}

static const char *lsp_native_range_super_identity(
        SZrLspNativeRangeFixture *fixture, const char *baseUri, const char *baseSource,
        const char *derivedUri, const char *derivedSource, TZrSize derivedStart) {
    SZrAstNode *base;
    SZrAstNode *derived;
    SZrAstNode *baseConstructor;
    SZrAstNode *derivedConstructor;
    SZrAstNode *inherit;
    const SZrTokenTree *keyword;
    const SZrTokenTree *arguments;
    SZrTokenSpan baseSpan;
    SZrTokenSpan derivedSpan;

    if (fixture->ast->data.script.statements->count != 2U) {
        return "Fixture prerequisite: native super source must retain exactly its two real classes";
    }
    base = fixture->ast->data.script.statements->nodes[0];
    derived = fixture->ast->data.script.statements->nodes[1];
    if (!lsp_native_range_node_is(fixture, base, ZR_AST_CLASS_DECLARATION) ||
        !lsp_native_range_node_is(fixture, derived, ZR_AST_CLASS_DECLARATION) ||
        base->data.classDeclaration.name == ZR_NULL || derived->data.classDeclaration.name == ZR_NULL ||
        !lsp_native_range_string_is(base->data.classDeclaration.name->name, "Base") ||
        !lsp_native_range_string_is(derived->data.classDeclaration.name->name, "Derived") ||
        base->data.classDeclaration.members == ZR_NULL || base->data.classDeclaration.members->nodes == ZR_NULL ||
        base->data.classDeclaration.members->count != 1U || derived->data.classDeclaration.members == ZR_NULL ||
        derived->data.classDeclaration.members->nodes == ZR_NULL || derived->data.classDeclaration.members->count != 1U ||
        derived->data.classDeclaration.inherits == ZR_NULL || derived->data.classDeclaration.inherits->nodes == ZR_NULL ||
        derived->data.classDeclaration.inherits->count != 1U ||
        !lsp_native_range_has_class_symbol(fixture, base) || !lsp_native_range_has_class_symbol(fixture, derived)) {
        return "Fixture prerequisite: native super classes must keep unique real declarations and canonical type symbols";
    }
    inherit = derived->data.classDeclaration.inherits->nodes[0];
    if (!lsp_native_range_node_is(fixture, inherit, ZR_AST_TYPE) ||
        !lsp_native_range_node_is(fixture, inherit->data.type.name, ZR_AST_IDENTIFIER_LITERAL) ||
        !lsp_native_range_string_is(inherit->data.type.name->data.identifier.name, "Base") ||
        !ZrParser_SyntaxContext_Equal(inherit->data.type.name->data.identifier.syntaxContext,
                ZrParser_SyntaxContext_TableRoot(fixture->table))) {
        return "Fixture prerequisite: native super inheritance must retain the unambiguous Base identifier";
    }
    baseConstructor = base->data.classDeclaration.members->nodes[0];
    derivedConstructor = derived->data.classDeclaration.members->nodes[0];
    if (!lsp_native_range_node_is(fixture, baseConstructor, ZR_AST_CLASS_META_FUNCTION) ||
        !lsp_native_range_node_is(fixture, derivedConstructor, ZR_AST_CLASS_META_FUNCTION) ||
        baseConstructor->data.classMetaFunction.meta == ZR_NULL ||
        derivedConstructor->data.classMetaFunction.meta == ZR_NULL ||
        !lsp_native_range_string_is(baseConstructor->data.classMetaFunction.meta->name, "constructor") ||
        !lsp_native_range_string_is(derivedConstructor->data.classMetaFunction.meta->name, "constructor") ||
        baseConstructor->data.classMetaFunction.hasSuperCall || !derivedConstructor->data.classMetaFunction.hasSuperCall ||
        derivedConstructor->data.classMetaFunction.superArgs == ZR_NULL ||
        derivedConstructor->data.classMetaFunction.superArgs->count != 0U ||
        !lsp_native_range_string_is(baseConstructor->location.source, baseUri) ||
        !lsp_native_range_string_is(derivedConstructor->location.source, derivedUri)) {
        return "Fixture prerequisite: native super constructors must keep their real source identities and empty super call";
    }
    keyword = lsp_native_range_find_tree(fixture, derivedUri, derivedSource, derivedStart + 41U, derivedStart + 46U);
    arguments = lsp_native_range_find_tree(fixture, derivedUri, derivedSource, derivedStart + 46U, derivedStart + 48U);
    if (!lsp_native_range_leaf_is(fixture, keyword, ZR_TK_SUPER, "super", 0) ||
        !lsp_native_range_group_is(fixture, arguments, derivedUri, derivedSource,
                derivedStart + 46U, derivedStart + 48U) ||
        ZrParser_TokenTree_GroupChildCount(arguments) != 0U ||
        !lsp_native_range_interval_is(fixture, derivedConstructor->data.classMetaFunction.superCallRange,
                derivedUri, derivedSource, derivedStart + 41U, derivedStart + 48U,
                ZrParser_TokenTree_Span(keyword), ZrParser_TokenTree_GroupCloseSpan(arguments), 3U)) {
        return "Fixture prerequisite: native super call must retain the real keyword, close, context and complete ledger interval";
    }
    baseSpan = ZrParser_TokenTree_Span(ZrParser_TokenStream_At(fixture->input, 0U));
    derivedSpan = ZrParser_TokenTree_Span(keyword);
    if (!lsp_native_range_span_is(fixture->input, baseSpan, baseUri, baseSource, 0U, 5U) ||
        (strcmp(baseUri, derivedUri) != 0 && (baseSpan.sourceId == derivedSpan.sourceId ||
            ZrCore_String_Equal(baseConstructor->location.source, derivedConstructor->location.source)))) {
        return "Fixture prerequisite: native super physical sources must remain distinct after Concat";
    }
    return ZR_NULL;
}

static TZrBool lsp_native_range_reference_is(
        SZrLspNativeRangeFixture *fixture, const char *uri, TZrInt32 line, TZrInt32 character) {
    SZrLspLocation **location;

    if (!fixture->references.isValid || fixture->references.length != 1U) {
        return ZR_FALSE;
    }
    location = (SZrLspLocation **)ZrCore_Array_Get(&fixture->references, 0U);
    return location != ZR_NULL && *location != ZR_NULL &&
            lsp_native_range_string_is((*location)->uri, uri) &&
            (*location)->range.start.line == line && (*location)->range.start.character == character;
}

static void test_lsp_super_references_keep_foreign_native_anchor(SZrState *state) {
    static const char ordinaryUri[] = "file:///lsp_native_super_ordinary.zr";
    static const char baseUri[] = "file:///lsp_native_super_head.zr";
    static const char derivedUri[] = "file:///lsp_native_super_tail.zr";
    static const char baseSource[] = "class Base { pub @constructor() {} }\n";
    static const char derivedSource[] = "class Derived: Base { pub @constructor() super() {} }\n";
    static const char ordinarySource[] =
            "class Base { pub @constructor() {} }\n"
            "class Derived: Base { pub @constructor() super() {} }\n";
    const char *summary = "LSP Super References Keep Foreign Native Anchor";
    const char *failure;
    SZrTestTimer timer;
    SZrLspNativeRangeFixture fixture = {0};
    SZrLspPosition position = {0, 20};
    SZrFilePosition cursor;
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    failure = lsp_native_range_fixture_open(state, &fixture, ordinaryUri, ordinarySource, ZR_NULL, ZR_NULL);
    if (failure != ZR_NULL) {
        goto cleanup;
    }
    failure = lsp_native_range_super_identity(&fixture, ordinaryUri, ordinarySource,
            ordinaryUri, ordinarySource, sizeof(baseSource) - 1U);
    if (failure != ZR_NULL) {
        goto cleanup;
    }
    if (!ZrLanguageServer_Lsp_FindReferences(state, fixture.context, fixture.uris[0], position,
                ZR_FALSE, &fixture.references) || !lsp_native_range_reference_is(&fixture, ordinaryUri, 1, 41)) {
        failure = "Fixture prerequisite: ordinary public super references must resolve the real single-source keyword anchor";
        goto cleanup;
    }
    lsp_native_range_fixture_free(state, &fixture);

    failure = lsp_native_range_fixture_open(state, &fixture, baseUri, baseSource, derivedUri, derivedSource);
    if (failure != ZR_NULL) {
        goto cleanup;
    }
    failure = lsp_native_range_super_identity(&fixture, baseUri, baseSource, derivedUri, derivedSource, 0U);
    if (failure != ZR_NULL) {
        goto cleanup;
    }
    cursor = ZrLanguageServer_Lsp_GetDocumentFilePosition(fixture.context, fixture.uris[0], position);
    if (!lsp_native_range_same_position(cursor, lsp_native_range_position(baseSource, 20U)) ||
        memcmp(baseSource + 18U, "constructor", sizeof("constructor") - 1U) != 0 ||
        !ZrLanguageServer_Lsp_FindReferences(state, fixture.context, fixture.uris[0], position,
                ZR_FALSE, &fixture.references) || fixture.analyzer->ast != fixture.ast ||
        ZrLanguageServer_Lsp_FindAnalyzer(state, fixture.context, fixture.uris[0]) != fixture.analyzer) {
        failure = "Fixture prerequisite: public mixed super request must query the real A declaration with its original analyzer";
        goto cleanup;
    }
    passed = lsp_native_range_reference_is(&fixture, derivedUri, 0, 41);

cleanup:
    lsp_native_range_fixture_free(state, &fixture);
    if (failure != ZR_NULL) {
        TEST_FAIL(timer, summary, failure);
    } else if (!passed) {
        TEST_FAIL(timer, summary,
                "Super constructor references must retain the real native source URI and first anchor");
    } else {
        TEST_PASS(timer, summary);
    }
}


static void test_extern_delegate_selection_preserves_native_name_leaf(SZrState *state) {
    static const char uri[] = "file:///extern_delegate_name_leaf.zr";
    static const char source[] =
            "native extern(\"fixture\") {\n"
            "    delegate Callback(value: i32): void;\n"
            "}\n";
    const char *summary = "Extern Delegate Selection Preserves Native Name Leaf";
    const char *failure;
    SZrTestTimer timer;
    SZrLspNativeRangeFixture fixture = {0};
    SZrAstNode *block;
    SZrAstNode *declaration;
    SZrAstNode *nameNode;
    SZrSymbol *symbol;
    const SZrSemanticReferenceFact *canonical;
    const SZrTokenTree *token;
    TZrSize start = (TZrSize)(strstr(source, "Callback") - source);
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    failure = lsp_native_range_fixture_open(state, &fixture, uri, source, ZR_NULL, ZR_NULL);
    if (failure != ZR_NULL) goto cleanup;
    if (fixture.ast->data.script.statements->count != 1U ||
        !lsp_native_range_node_is(&fixture, block = fixture.ast->data.script.statements->nodes[0],
                ZR_AST_EXTERN_BLOCK) || block->data.externBlock.declarations == ZR_NULL ||
        block->data.externBlock.declarations->count != 1U ||
        !lsp_native_range_node_is(&fixture, declaration = block->data.externBlock.declarations->nodes[0],
                ZR_AST_EXTERN_DELEGATE_DECLARATION) ||
        declaration->data.externDelegateDeclaration.name == ZR_NULL) {
        failure = "Fixture prerequisite: real extern delegate must be parsed with its owned declaration";
        goto cleanup;
    }
    nameNode = (SZrAstNode *)((char *)declaration->data.externDelegateDeclaration.name -
            offsetof(SZrAstNode, data.identifier));
    token = lsp_native_range_find_tree(&fixture, uri, source, start, start + 8U);
    if (!lsp_native_range_node_is(&fixture, nameNode, ZR_AST_IDENTIFIER_LITERAL) ||
        !lsp_native_range_string_is(nameNode->data.identifier.name, "Callback") ||
        !lsp_native_range_leaf_is(&fixture, token, ZR_TK_IDENTIFIER, "Callback", 0) ||
        !ZrParser_SyntaxContext_Equal(nameNode->data.identifier.syntaxContext,
                ZrParser_TokenTree_Context(token)) ||
        !lsp_native_range_interval_is(&fixture, nameNode->location, uri, source, start, start + 8U,
                ZrParser_TokenTree_Span(token), ZrParser_TokenTree_Span(token), 1U)) {
        failure = "Fixture prerequisite: delegate name must retain its real token, context, source and ledger";
        goto cleanup;
    }
    symbol = ZrLanguageServer_SymbolTable_Lookup(fixture.analyzer->symbolTable,
            nameNode->data.identifier.name, ZR_NULL);
    if (symbol == ZR_NULL || symbol->astNode != declaration ||
        symbol->semanticId == ZR_SEMANTIC_ID_INVALID ||
        symbol->semanticTypeId == ZR_SEMANTIC_ID_INVALID) {
        failure = "Fixture prerequisite: public Analyze must publish the real canonical delegate identity";
        goto cleanup;
    }
    canonical = ZrParser_SemanticQuery_DeclarationOf(
            fixture.analyzer->semanticContext, symbol->semanticId, ZR_NULL);
    passed = lsp_native_range_same_range(symbol->selectionRange, nameNode->location) &&
            canonical != ZR_NULL && canonical->node == declaration && canonical->isResolved &&
            canonical->symbolId == symbol->semanticId && canonical->typeId == symbol->semanticTypeId &&
            canonical->hasDefinitionRange &&
            lsp_native_range_same_range(canonical->range, nameNode->location) &&
            lsp_native_range_same_range(canonical->declarationRange, nameNode->location) &&
            lsp_native_range_same_range(canonical->definitionRange, nameNode->location);

cleanup:
    lsp_native_range_fixture_free(state, &fixture);
    if (failure != ZR_NULL) TEST_FAIL(timer, summary, failure);
    else if (!passed) TEST_FAIL(timer, summary,
            "Delegate selection and canonical declaration must use the owned identifier leaf, not a keyword offset");
    else TEST_PASS(timer, summary);
}

static void test_extern_enum_member_use_publishes_canonical_fact(SZrState *state) {
    static const char uri[] = "file:///extern_enum_member_fact.zr";
    static const char source[] =
            "native extern(\"fixture\") {\n"
            "    #zr.ffi.underlying(\"i32\")# enum Mode { #zr.ffi.value(1)# On }\n"
            "}\nMode.On;\n";
    const char *summary = "Extern Enum Member Use Publishes Canonical Fact";
    const char *failure = ZR_NULL;
    SZrTestTimer timer;
    SZrLspNativeRangeFixture fixture = {0};
    SZrCompilerState cs = {0};
    SZrInferredType inferred = {0};
    SZrAstNode *block;
    SZrAstNode *enumeration;
    SZrAstNode *member;
    SZrAstNode *nameNode;
    SZrAstNode *expression;
    SZrAstNode *use;
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *memberInfo;
    const SZrSemanticReferenceFact *fact = ZR_NULL;
    const SZrSemanticSymbolRecord *symbol;
    TZrSize facts = 0U;
    TZrBool compilerInitialized = ZR_FALSE;
    TZrBool inferredInitialized = ZR_FALSE;
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    fixture.table = ZrParser_SyntaxContextTable_Create(state->global, ZR_NULL);
    fixture.uris[0] = ZrCore_String_CreateFromNative(state, (TZrNativeString)uri);
    if (fixture.table == ZR_NULL || fixture.uris[0] == ZR_NULL ||
        !ZrCore_Gc_RootLeaseAcquire(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture.uris[0]),
                &fixture.uriRoots[0]) ||
        !ZrParser_TokenStream_FromText(state, fixture.table, uri, sizeof(uri) - 1U,
                source, sizeof(source) - 1U, ZR_NULL, &fixture.streams[0])) {
        failure = "Fixture prerequisite: parser-only enum must use a real sealed token source";
        goto cleanup;
    }
    fixture.input = fixture.streams[0];
    ZrParser_State_InitWithTokenStream(&fixture.parser, state, fixture.input, fixture.uris[0]);
    fixture.parserInitialized = ZR_TRUE;
    fixture.ast = ZrParser_ParseWithState(&fixture.parser);
    if (fixture.ast == ZR_NULL || fixture.parser.hasError || fixture.parser.hasFatalError ||
        fixture.ast->type != ZR_AST_SCRIPT || fixture.ast->ownership == ZR_NULL ||
        fixture.parser.lexer == ZR_NULL || fixture.ast->ownership != fixture.parser.lexer->ownership ||
        fixture.parser.lexer->t.token != ZR_TK_EOS || fixture.ast->data.script.statements == ZR_NULL ||
        fixture.ast->data.script.statements->count != 2U ||
        !lsp_native_range_node_is(&fixture, block = fixture.ast->data.script.statements->nodes[0],
                ZR_AST_EXTERN_BLOCK) || block->data.externBlock.declarations == ZR_NULL ||
        block->data.externBlock.declarations->count != 1U ||
        !lsp_native_range_node_is(&fixture, enumeration = block->data.externBlock.declarations->nodes[0],
                ZR_AST_ENUM_DECLARATION) || enumeration->data.enumDeclaration.members == ZR_NULL ||
        enumeration->data.enumDeclaration.members->count != 1U ||
        !lsp_native_range_node_is(&fixture, member = enumeration->data.enumDeclaration.members->nodes[0],
                ZR_AST_ENUM_MEMBER) || member->data.enumMember.name == ZR_NULL) {
        failure = "Fixture prerequisite: parser-only enum must reach EOS with its owned member declaration";
        goto cleanup;
    }
    nameNode = (SZrAstNode *)((char *)member->data.enumMember.name -
            offsetof(SZrAstNode, data.identifier));
    expression = fixture.ast->data.script.statements->nodes[1];
    if (!lsp_native_range_node_is(&fixture, nameNode, ZR_AST_IDENTIFIER_LITERAL) ||
        !lsp_native_range_node_is(&fixture, expression, ZR_AST_EXPRESSION_STATEMENT) ||
        !lsp_native_range_node_is(&fixture, expression = expression->data.expressionStatement.expr,
                ZR_AST_PRIMARY_EXPRESSION) || expression->data.primaryExpression.members == ZR_NULL ||
        expression->data.primaryExpression.members->count != 1U ||
        !lsp_native_range_node_is(&fixture, use = expression->data.primaryExpression.members->nodes[0],
                ZR_AST_MEMBER_EXPRESSION) ||
        !lsp_native_range_node_is(&fixture, use = use->data.memberExpression.property,
                ZR_AST_IDENTIFIER_LITERAL)) {
        failure = "Fixture prerequisite: enum member definition and use must be real owned identifier leaves";
        goto cleanup;
    }
    ZrParser_CompilerState_Init(&cs, state);
    compilerInitialized = ZR_TRUE;
    cs.scriptAst = fixture.ast;
    ZrParser_Compiler_PredeclareExternBindings(&cs, fixture.ast->data.script.statements);
    if (cs.hasError || cs.semanticContext == ZR_NULL || cs.typePrototypes.length != 1U) {
        failure = "Fixture prerequisite: public compiler predeclaration must publish the real enum prototype";
        goto cleanup;
    }
    prototype = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs.typePrototypes, 0U);
    memberInfo = prototype != ZR_NULL
            ? (SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, 0U) : ZR_NULL;
    if (prototype == ZR_NULL || prototype->declarationNode != enumeration ||
        prototype->members.length != 1U || memberInfo == ZR_NULL ||
        memberInfo->memberType != ZR_AST_CLASS_FIELD || memberInfo->declarationNode != member ||
        !lsp_native_range_string_is(memberInfo->name, "On")) {
        failure = "Fixture prerequisite: enum must retain its actual field-backed runtime representation";
        goto cleanup;
    }
    ZrParser_InferredType_Init(state, &inferred, ZR_VALUE_TYPE_OBJECT);
    inferredInitialized = ZR_TRUE;
    if (!ZrParser_ExpressionType_Infer(&cs, expression, &inferred) || cs.hasError) {
        failure = "Fixture prerequisite: normal public expression inference must resolve the real enum access";
        goto cleanup;
    }
    for (TZrSize index = 0U; index < cs.semanticContext->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *candidate = ZrCore_Array_Get(&cs.semanticContext->referenceFacts, index);
        if (candidate != ZR_NULL && candidate->node == use &&
            candidate->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS) {
            fact = candidate;
            ++facts;
        }
    }
    symbol = fact != ZR_NULL ? ZrParser_Semantic_FindSymbolById(cs.semanticContext, fact->symbolId) : ZR_NULL;
    passed = facts == 1U && fact != ZR_NULL && fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID && fact->typeId != ZR_SEMANTIC_ID_INVALID &&
            fact->symbolId == memberInfo->symbolId && symbol != ZR_NULL && symbol->astNode == member &&
            symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_FIELD && symbol->typeId == fact->typeId &&
            fact->hasDefinitionRange && lsp_native_range_same_range(fact->range, use->location) &&
            lsp_native_range_same_range(fact->definitionRange, nameNode->location) &&
            lsp_native_range_same_range(fact->declarationRange, member->location) &&
            ZrParser_SyntaxContext_Equal(symbol->syntaxContext, nameNode->data.identifier.syntaxContext);

cleanup:
    if (inferredInitialized) ZrParser_InferredType_Free(state, &inferred);
    if (compilerInitialized) {
        ZrParser_CompilerState_Free(&cs);
    }
    lsp_native_range_fixture_free(state, &fixture);
    if (failure != ZR_NULL) TEST_FAIL(timer, summary, failure);
    else if (!passed) TEST_FAIL(timer, summary,
            "Enum member inference must publish exactly one resolved canonical fact with the native name-leaf target");
    else TEST_PASS(timer, summary);
}

#endif
