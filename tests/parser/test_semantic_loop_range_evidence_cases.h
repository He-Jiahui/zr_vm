#ifndef ZR_TEST_SEMANTIC_LOOP_RANGE_EVIDENCE_CASES_H
#define ZR_TEST_SEMANTIC_LOOP_RANGE_EVIDENCE_CASES_H

#include <stdio.h>

#include "zr_vm_core/gc.h"
#include "zr_vm_parser/token_stream.h"
#include "parser/parser_ownership.h"
#include "parser/parser_range_evidence.h"
#include "syntax/token_stream_internal.h"

enum { ZR_TEST_LOOP_RANGE_TOKEN_LIMIT = 128, ZR_TEST_LOOP_RANGE_CHILD_LIMIT = 64 };

typedef struct SZrLoopRangeTokenSnapshot {
    const SZrTokenTree *tree;
    SZrTokenTree value;
} SZrLoopRangeTokenSnapshot;

typedef struct SZrLoopRangeFixture {
    SZrSyntaxContextTable *table;
    SZrTokenStream *original;
    SZrTokenStream *stream;
    SZrParserState parser;
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrArray symbols;
    SZrGcRootLease nameRoot;
    SZrString *name;
    TZrBool parserInitialized;
    TZrBool compilerInitialized;
    TZrTokenSourceId sourceA;
    TZrTokenSourceId sourceB;
    TZrUInt64 originA;
    TZrUInt64 originB;
    TZrSize bodyStart;
    TZrSize bodyEnd;
    char bodySource[128];
    SZrLoopRangeTokenSnapshot tokens[ZR_TEST_LOOP_RANGE_TOKEN_LIMIT];
    TZrSize tokenCount;
} SZrLoopRangeFixture;

static SZrLoopRangeFixture g_loopRange;

static void semantic_loop_range_evidence_cleanup(void) {
    if (g_loopRange.symbols.isValid) {
        ZrCore_Array_Free(g_state, &g_loopRange.symbols);
    }
    if (g_loopRange.compilerInitialized) {
        symbol_release_compiler_function(&g_loopRange.compiler);
        ZrParser_CompilerState_Free(&g_loopRange.compiler);
    }
    if (g_loopRange.ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_loopRange.ast);
    }
    if (g_loopRange.parserInitialized) {
        ZrParser_State_Free(&g_loopRange.parser);
    }
    if (g_state != ZR_NULL) {
        ZrCore_Gc_RootLeaseRelease(g_state->global, &g_loopRange.nameRoot);
    }
    ZrParser_TokenStream_Release(g_loopRange.stream);
    ZrParser_TokenStream_Release(g_loopRange.original);
    ZrParser_SyntaxContextTable_Release(g_loopRange.table);
    memset(&g_loopRange, 0, sizeof(g_loopRange));
}

static void loop_range_root_name(void) {
    g_loopRange.name = ZrCore_String_CreateFromNative(g_state, "loop_scope_a.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_loopRange.name,
            "Fixture prerequisite: loop source name allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(g_loopRange.name), &g_loopRange.nameRoot),
            "Fixture prerequisite: loop source name rooting failed");
}

static SZrTokenSpan loop_range_map_span(SZrTokenSpan span) {
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenSpan_IsValid(span),
            "Fixture prerequisite: source-backed loop token has no span");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, span.startLine,
            "Fixture prerequisite: loop fixture start is not on its actual source line");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, span.endLine,
            "Fixture prerequisite: loop fixture end is not on its actual source line");
    if (span.startOffset >= g_loopRange.bodyStart && span.endOffset <= g_loopRange.bodyEnd) {
        span.sourceId = g_loopRange.sourceB;
        span.startOffset = 64U + span.startOffset - g_loopRange.bodyStart;
        span.endOffset = 64U + span.endOffset - g_loopRange.bodyStart;
        span.startColumn = (TZrInt32)span.startOffset + 1;
        span.endColumn = (TZrInt32)span.endOffset + 1;
    } else {
        span.sourceId = g_loopRange.sourceA;
    }
    return span;
}

static SZrTokenText loop_range_copy_payload(SZrTokenText text) {
    SZrTokenText copy = {0};

    if (text.sourceId != ZR_TOKEN_SOURCE_INVALID) {
        const char *bytes = ZrParser_TokenStream_TextBytes(g_loopRange.original, text);
        TEST_ASSERT_NOT_NULL_MESSAGE(bytes,
                "Fixture prerequisite: original loop token semantic text is missing");
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_CopyText(
                g_loopRange.stream, bytes, text.length, &copy),
                "Fixture prerequisite: loop token semantic text copy failed");
    }
    return copy;
}

/* Reuse real lexed payloads; only the complete loop-body subtree changes source. */
static const SZrTokenTree *loop_range_copy_tree(const SZrTokenTree *tree) {
    SZrTokenSpan span;
    TZrSize index = ZR_MAX_SIZE;
    SZrSyntaxContext context;

    TEST_ASSERT_NOT_NULL_MESSAGE(tree, "Fixture prerequisite: original loop tree is missing");
    span = loop_range_map_span(ZrParser_TokenTree_Span(tree));
    context = ZrParser_TokenTree_Context(tree);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SyntaxContext_Equal(
            context, ZrParser_SyntaxContext_TableRoot(g_loopRange.table)),
            "Fixture prerequisite: original loop token context changed");
    if (ZrParser_TokenTree_Kind(tree) == ZR_TOKEN_TREE_GROUP) {
        SZrTokenTree *children[ZR_TEST_LOOP_RANGE_CHILD_LIMIT];
        TZrSize count = ZrParser_TokenTree_GroupChildCount(tree);
        TZrSize rootBase = g_loopRange.stream->rootCount;

        TEST_ASSERT_TRUE_MESSAGE(count <= ZR_TEST_LOOP_RANGE_CHILD_LIMIT,
                "Fixture prerequisite: loop group exceeds bounded fixture storage");
        for (TZrSize child = 0U; child < count; child++) {
            children[child] = (SZrTokenTree *)loop_range_copy_tree(ZrParser_TokenTree_GroupChildAt(tree, child));
        }
        /* FromText uses the same root-stack adoption for children in its own builder. */
        g_loopRange.stream->rootCount = rootBase;
        TEST_ASSERT_TRUE_MESSAGE(token_stream_make_group(
                g_loopRange.stream, ZrParser_TokenTree_GroupDelimiter(tree),
                loop_range_map_span(ZrParser_TokenTree_GroupOpenSpan(tree)),
                loop_range_map_span(ZrParser_TokenTree_GroupCloseSpan(tree)),
                children, count, span, context, ZR_NULL, 0U, ZR_NULL, 0U, &index),
                "Fixture prerequisite: mixed loop group construction failed");
    } else {
        SZrTokenNativeSemInfo info;
        SZrTokenText raw = ZrParser_TokenTree_Text(tree);
        const char *originalBytes = ZrParser_TokenStream_TextBytes(g_loopRange.original, raw);
        TZrBool appended = ZR_FALSE;

        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenTree_NativeSemInfo(tree, &info),
                "Fixture prerequisite: real loop token payload is missing");
        TEST_ASSERT_NOT_NULL_MESSAGE(originalBytes,
                "Fixture prerequisite: original loop token raw text is missing");
        raw.sourceId = span.sourceId;
        raw.offset = span.startOffset;
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(originalBytes,
                ZrParser_TokenStream_TextBytes(g_loopRange.stream, raw), raw.length,
                "Fixture prerequisite: mixed loop token does not spell its actual source bytes");
        info.text = loop_range_copy_payload(info.text);
        info.suffix = loop_range_copy_payload(info.suffix);
        switch (ZrParser_TokenTree_Kind(tree)) {
            case ZR_TOKEN_TREE_IDENT:
                appended = ZrParser_TokenStream_AppendIdent(g_loopRange.stream, info, raw,
                        span, context, ZR_NULL, 0U, ZR_NULL, 0U, &index);
                break;
            case ZR_TOKEN_TREE_LITERAL:
                appended = ZrParser_TokenStream_AppendLiteral(g_loopRange.stream, info, raw,
                        span, context, ZR_NULL, 0U, ZR_NULL, 0U, &index);
                break;
            case ZR_TOKEN_TREE_PUNCT:
                appended = ZrParser_TokenStream_AppendPunct(g_loopRange.stream, info, raw,
                        ZrParser_TokenTree_PunctCodepoint(tree), ZrParser_TokenTree_PunctSpacing(tree),
                        span, context, ZR_NULL, 0U, ZR_NULL, 0U, &index);
                break;
            default:
                break;
        }
        TEST_ASSERT_TRUE_MESSAGE(appended, "Fixture prerequisite: mixed loop leaf construction failed");
    }
    return ZrParser_TokenStream_At(g_loopRange.stream, index);
}

static void loop_range_snapshot_tokens(void) {
    TEST_ASSERT_TRUE_MESSAGE(g_loopRange.stream->treeCount <= ZR_TEST_LOOP_RANGE_TOKEN_LIMIT,
            "Fixture prerequisite: loop tokens exceed bounded fixture storage");
    g_loopRange.tokenCount = g_loopRange.stream->treeCount;
    for (TZrSize index = 0U; index < g_loopRange.tokenCount; index++) {
        g_loopRange.tokens[index].tree = g_loopRange.stream->trees[index];
        memcpy(&g_loopRange.tokens[index].value, g_loopRange.stream->trees[index], sizeof(SZrTokenTree));
    }
}

static void loop_range_require_unchanged_tokens(const char *source) {
    TZrSize length;
    const char *bytes;

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(g_loopRange.tokenCount, g_loopRange.stream->treeCount,
            "Fixture prerequisite: parsing or compilation changed native loop tree count");
    for (TZrSize index = 0U; index < g_loopRange.tokenCount; index++) {
        TEST_ASSERT_EQUAL_PTR_MESSAGE(g_loopRange.tokens[index].tree, g_loopRange.stream->trees[index],
                "Fixture prerequisite: parsing or compilation replaced a native loop tree");
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&g_loopRange.tokens[index].value,
                g_loopRange.tokens[index].tree, sizeof(SZrTokenTree),
                "Fixture prerequisite: parsing or compilation changed an original loop token");
    }
    bytes = ZrParser_TokenStream_SourceBytes(g_loopRange.stream, g_loopRange.sourceA, &length);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(strlen(source), length,
            "Fixture prerequisite: original loop source A length changed");
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(source, bytes, length,
            "Fixture prerequisite: original loop source A bytes changed");
    bytes = ZrParser_TokenStream_SourceBytes(g_loopRange.stream, g_loopRange.sourceB, &length);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(strlen(g_loopRange.bodySource), length,
            "Fixture prerequisite: original loop source B length changed");
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(g_loopRange.bodySource, bytes, length,
            "Fixture prerequisite: original loop source B bytes changed");
}

static void loop_range_parse(const char *source, const char *bodyText, TZrBool mixed) {
    loop_range_root_name();
    if (mixed) {
        const char *body = strstr(source, bodyText);

        TEST_ASSERT_NOT_NULL_MESSAGE(body, "Fixture prerequisite: actual loop body text is missing");
        g_loopRange.bodyStart = (TZrSize)(body - source);
        g_loopRange.bodyEnd = g_loopRange.bodyStart + strlen(bodyText);
        TEST_ASSERT_TRUE_MESSAGE(64U + strlen(bodyText) < sizeof(g_loopRange.bodySource),
                "Fixture prerequisite: loop body exceeds bounded source storage");
        memset(g_loopRange.bodySource, ' ', 64U);
        memcpy(g_loopRange.bodySource + 64U, bodyText, strlen(bodyText) + 1U);
        g_loopRange.table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
        TEST_ASSERT_NOT_NULL_MESSAGE(g_loopRange.table,
                "Fixture prerequisite: loop context table allocation failed");
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(g_state, g_loopRange.table,
                "loop_scope_template.zr", strlen("loop_scope_template.zr"), source, strlen(source),
                ZR_NULL, &g_loopRange.original),
                "Fixture prerequisite: real loop source tokenization failed");
        g_loopRange.stream = ZrParser_TokenStream_Create(g_state->global, g_loopRange.table, ZR_NULL);
        TEST_ASSERT_NOT_NULL_MESSAGE(g_loopRange.stream,
                "Fixture prerequisite: mixed loop stream allocation failed");
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_AddSource(g_loopRange.stream,
                "loop_scope_a.zr", strlen("loop_scope_a.zr"), source, strlen(source), &g_loopRange.sourceA),
                "Fixture prerequisite: real loop source A storage failed");
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_AddSource(g_loopRange.stream,
                "loop_scope_b.zr", strlen("loop_scope_b.zr"), g_loopRange.bodySource,
                strlen(g_loopRange.bodySource), &g_loopRange.sourceB),
                "Fixture prerequisite: real loop source B storage failed");
        TEST_ASSERT_TRUE_MESSAGE(token_stream_source_origin(
                g_loopRange.stream, g_loopRange.sourceA, &g_loopRange.originA) &&
                token_stream_source_origin(g_loopRange.stream, g_loopRange.sourceB, &g_loopRange.originB) &&
                g_loopRange.originA != g_loopRange.originB,
                "Fixture prerequisite: mixed loop source origins are not distinct real identities");
        for (TZrSize index = 0U; index < ZrParser_TokenStream_Count(g_loopRange.original); index++) {
            TEST_ASSERT_NOT_NULL_MESSAGE(loop_range_copy_tree(
                    ZrParser_TokenStream_At(g_loopRange.original, index)),
                    "Fixture prerequisite: mixed loop tree was not published");
        }
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Seal(g_loopRange.stream),
                "Fixture prerequisite: mixed loop stream sealing failed");
        loop_range_snapshot_tokens();
        g_loopRange.parserInitialized = ZR_TRUE;
        ZrParser_State_InitWithTokenStream(&g_loopRange.parser, g_state, g_loopRange.stream, g_loopRange.name);
    } else {
        g_loopRange.parserInitialized = ZR_TRUE;
        ZrParser_State_Init(&g_loopRange.parser, g_state, source, strlen(source), g_loopRange.name);
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(g_loopRange.parser.lexer,
            "Fixture prerequisite: loop parser lexer is missing");
    g_loopRange.parser.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE_MESSAGE(g_loopRange.parser.hasError || g_loopRange.parser.hasFatalError ||
            g_loopRange.parser.lexer->t.hasLexError || g_loopRange.parser.lexer->hasSourceEvidenceError,
            "Fixture prerequisite: loop parser initialization failed");
    g_loopRange.ast = ZrParser_ParseWithState(&g_loopRange.parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_loopRange.ast, "Fixture prerequisite: real loop parsing failed");
    TEST_ASSERT_FALSE_MESSAGE(g_loopRange.parser.hasError || g_loopRange.parser.hasFatalError ||
            g_loopRange.parser.lexer->t.hasLexError || g_loopRange.parser.lexer->hasSourceEvidenceError,
            "Fixture prerequisite: real loop parsing reported an error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_TK_EOS, g_loopRange.parser.lexer->t.token,
            "Fixture prerequisite: loop parsing did not consume the complete source");
    if (mixed) {
        SZrTokenSpan expected = ZrParser_TokenStream_EndSpan(g_loopRange.stream);
        SZrTokenSpan actual = g_loopRange.parser.lexer->t.span;
        TEST_ASSERT_TRUE_MESSAGE(actual.sourceId == expected.sourceId &&
                actual.startOffset == expected.startOffset && actual.endOffset == expected.endOffset &&
                actual.startLine == expected.startLine && actual.startColumn == expected.startColumn &&
                actual.endLine == expected.endLine && actual.endColumn == expected.endColumn,
                "Fixture prerequisite: mixed loop EOS lost its actual source span");
    }
}

static void loop_range_compile(void) {
    g_loopRange.compilerInitialized = ZR_TRUE;
    ZrParser_CompilerState_Init(&g_loopRange.compiler, g_state);
    g_loopRange.compiler.suppressErrorOutput = ZR_TRUE;
    g_loopRange.compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_loopRange.compiler.currentFunction,
            "Fixture prerequisite: loop compiler function allocation failed");
    compile_script(&g_loopRange.compiler, g_loopRange.ast);
    TEST_ASSERT_FALSE_MESSAGE(g_loopRange.compiler.hasError,
            "Fixture prerequisite: real loop compilation failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_loopRange.compiler.semanticContext,
            "Fixture prerequisite: compiled loop semantic context is missing");
}

static const SZrSemanticReferenceFact *loop_range_require_declaration(SZrAstNode *node) {
    const SZrSemanticReferenceFact *fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            g_loopRange.compiler.semanticContext, node, ZR_SEMANTIC_REFERENCE_DECLARATION);
    const SZrSemanticSymbolRecord *symbol;

    TEST_ASSERT_NOT_NULL_MESSAGE(fact, "Fixture prerequisite: exact loop declaration fact is missing");
    TEST_ASSERT_TRUE_MESSAGE(fact->isResolved && fact->node == node &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID && fact->typeId != ZR_SEMANTIC_ID_INVALID,
            "Fixture prerequisite: loop declaration identity or type is unresolved");
    symbol = ZrParser_Semantic_FindSymbolById(g_loopRange.compiler.semanticContext, fact->symbolId);
    TEST_ASSERT_NOT_NULL_MESSAGE(symbol, "Fixture prerequisite: loop declaration symbol is missing");
    TEST_ASSERT_TRUE_MESSAGE(symbol->typeId == fact->typeId &&
            ZrParser_CanonicalType_Find(g_loopRange.compiler.semanticContext, fact->typeId) != ZR_NULL,
            "Fixture prerequisite: loop declaration canonical type is invalid");
    return fact;
}

static SZrAstNode *loop_range_require_nodes(TZrBool foreachLoop, SZrAstNode **outBody,
                                           SZrAstNode **outBinding, SZrAstNode **outFollowing) {
    SZrAstNode *function;
    SZrAstNodeArray *statements;
    SZrAstNode *loop;

    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_SCRIPT, g_loopRange.ast->type,
            "Fixture prerequisite: loop source did not produce a SCRIPT");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_loopRange.ast->data.script.statements,
            "Fixture prerequisite: loop script statements are missing");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1U, g_loopRange.ast->data.script.statements->count,
            "Fixture prerequisite: loop script function count changed");
    function = g_loopRange.ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Fixture prerequisite: loop function is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_FUNCTION_DECLARATION, function->type,
            "Fixture prerequisite: loop source did not produce a function declaration");
    TEST_ASSERT_NOT_NULL_MESSAGE(function->data.functionDeclaration.body,
            "Fixture prerequisite: loop function body is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_BLOCK, function->data.functionDeclaration.body->type,
            "Fixture prerequisite: loop function body is not a BLOCK");
    statements = function->data.functionDeclaration.body->data.block.body;
    TEST_ASSERT_NOT_NULL_MESSAGE(statements, "Fixture prerequisite: loop function statements are missing");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(foreachLoop ? 4U : 3U, statements->count,
            "Fixture prerequisite: loop function statement count changed");
    loop = statements->nodes[foreachLoop ? 1U : 0U];
    *outFollowing = statements->nodes[foreachLoop ? 2U : 1U];
    TEST_ASSERT_NOT_NULL_MESSAGE(loop, "Fixture prerequisite: actual loop AST is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(foreachLoop ? ZR_AST_FOREACH_LOOP : ZR_AST_FOR_LOOP, loop->type,
            "Fixture prerequisite: actual loop AST kind changed");
    *outBody = foreachLoop ? loop->data.foreachLoop.block : loop->data.forLoop.block;
    *outBinding = foreachLoop ? loop->data.foreachLoop.pattern : loop->data.forLoop.init;
    TEST_ASSERT_NOT_NULL_MESSAGE(*outBody, "Fixture prerequisite: actual loop body AST is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_BLOCK, (*outBody)->type,
            "Fixture prerequisite: actual loop body is not a BLOCK");
    TEST_ASSERT_NOT_NULL_MESSAGE(*outBinding, "Fixture prerequisite: actual loop binding AST is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(foreachLoop ? ZR_AST_IDENTIFIER_LITERAL : ZR_AST_VARIABLE_DECLARATION,
            (*outBinding)->type, "Fixture prerequisite: actual loop binding AST kind changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(*outFollowing, "Fixture prerequisite: following loop declaration is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_VARIABLE_DECLARATION, (*outFollowing)->type,
            "Fixture prerequisite: following loop statement is not a declaration");
    TEST_ASSERT_NOT_NULL_MESSAGE((*outFollowing)->data.variableDeclaration.pattern,
            "Fixture prerequisite: following declaration pattern is missing");
    TEST_ASSERT_TRUE_MESSAGE(g_loopRange.ast->ownership != ZR_NULL &&
            function->ownership == g_loopRange.ast->ownership && loop->ownership == g_loopRange.ast->ownership &&
            (*outBody)->ownership == g_loopRange.ast->ownership &&
            (*outBinding)->ownership == g_loopRange.ast->ownership &&
            (*outFollowing)->ownership == g_loopRange.ast->ownership,
            "Fixture prerequisite: real loop AST nodes do not share their source owner");
    return loop;
}

static SZrAstNode *loop_range_require_foreach_witness(SZrAstNode *body) {
    SZrAstNodeArray *statements = body->data.block.body;
    SZrAstNode *witness;
    SZrAstNode *read;

    TEST_ASSERT_TRUE_MESSAGE(statements != ZR_NULL && statements->count == 2U,
            "Fixture prerequisite: foreach body witness statement count changed");
    witness = statements->nodes[0];
    read = statements->nodes[1];
    TEST_ASSERT_TRUE_MESSAGE(witness != ZR_NULL && witness->type == ZR_AST_VARIABLE_DECLARATION &&
            witness->data.variableDeclaration.pattern != ZR_NULL &&
            witness->data.variableDeclaration.pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
            witness->data.variableDeclaration.pattern->data.identifier.name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(
                    witness->data.variableDeclaration.pattern->data.identifier.name), "scopeWitness") == 0 &&
            witness->data.variableDeclaration.value != ZR_NULL &&
            witness->data.variableDeclaration.value->type == ZR_AST_IDENTIFIER_LITERAL &&
            witness->data.variableDeclaration.value->data.identifier.name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(
                    witness->data.variableDeclaration.value->data.identifier.name), "item") == 0 &&
            witness->ownership == body->ownership &&
            witness->data.variableDeclaration.pattern->ownership == body->ownership &&
            witness->data.variableDeclaration.value->ownership == body->ownership,
            "Fixture prerequisite: foreach witness must be a real source-owned declaration initialized from item");
    TEST_ASSERT_TRUE_MESSAGE(read != ZR_NULL && read->type == ZR_AST_EXPRESSION_STATEMENT &&
            read->data.expressionStatement.expr != ZR_NULL &&
            read->data.expressionStatement.expr->type == ZR_AST_IDENTIFIER_LITERAL &&
            read->data.expressionStatement.expr->data.identifier.name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(
                    read->data.expressionStatement.expr->data.identifier.name), "scopeWitness") == 0 &&
            read->ownership == body->ownership && read->data.expressionStatement.expr->ownership == body->ownership,
            "Fixture prerequisite: foreach witness read must retain its real source-owned AST");
    return witness;
}

static void loop_range_plain_control(const char *source, const char *bodyText,
                                     const char *bindingName, TZrBool foreachLoop) {
    SZrAstNode *body;
    SZrAstNode *binding;
    SZrAstNode *following;
    const SZrSemanticReferenceFact *loopDeclaration;
    const SZrSemanticReferenceFact *followingDeclaration;
    SZrParserSemanticVisibleSymbolOptions options = {0};

    loop_range_parse(source, bodyText, ZR_FALSE);
    loop_range_require_nodes(foreachLoop, &body, &binding, &following);
    loop_range_compile();
    loopDeclaration = loop_range_require_declaration(
            foreachLoop ? loop_range_require_foreach_witness(body) : binding);
    followingDeclaration = loop_range_require_declaration(following);
    TEST_ASSERT_TRUE_MESSAGE(loopDeclaration->symbolId != followingDeclaration->symbolId,
            foreachLoop ? "Fixture prerequisite: foreach witness and following declaration share an identity" :
                    "Fixture prerequisite: same-name loop and following declarations share an identity");
    ZrCore_Array_Construct(&g_loopRange.symbols);
    for (TZrSize query = 0U; query < 2U; query++) {
        SZrFileRange position = following->data.variableDeclaration.pattern->location;
        const SZrParserSemanticSymbolQuery *visible;

        if (query != 0U) {
            const char *read = strstr(source, "return ");
            SZrFilePosition point;
            TEST_ASSERT_NOT_NULL_MESSAGE(read, "Fixture prerequisite: following binding read is missing");
            point.offset = (TZrSize)(read - source) + strlen("return ");
            point.line = 1;
            point.column = (TZrInt32)point.offset + 1;
            position = ZrParser_FileRange_Create(point, point, g_loopRange.name);
        }
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticQuery_VisibleSymbols(
                g_loopRange.compiler.semanticContext, position, ZR_NULL, &options, &g_loopRange.symbols),
                "Fixture prerequisite: ordinary loop visible-symbol query failed");
        visible = symbol_find_visible_name(&g_loopRange.symbols, bindingName);
        TEST_ASSERT_TRUE_MESSAGE(visible != ZR_NULL && visible->symbolId == followingDeclaration->symbolId &&
                symbol_count_visible_name(&g_loopRange.symbols, bindingName) == 1U,
                "Fixture prerequisite: ordinary query did not select the real following declaration");
        if (foreachLoop) {
            TEST_ASSERT_EQUAL_UINT64_MESSAGE(0U, symbol_count_visible_name(&g_loopRange.symbols, "scopeWitness"),
                    "Fixture prerequisite: ordinary query exposed the foreach body witness after its block");
        }
        for (TZrSize index = 0U; index < g_loopRange.symbols.length; index++) {
            const SZrParserSemanticSymbolQuery *candidate =
                    (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(&g_loopRange.symbols, index);
            TEST_ASSERT_TRUE_MESSAGE(candidate != ZR_NULL && candidate->symbolId != loopDeclaration->symbolId,
                    foreachLoop ? "Fixture prerequisite: ordinary query leaked the foreach body witness after its block" :
                            "Fixture prerequisite: ordinary query leaked the loop binding after its body");
        }
    }
    semantic_loop_range_evidence_cleanup();
}

static TZrBool loop_range_anchor_is(SZrFileRange range, TZrUInt64 origin, TZrSize start, TZrSize end) {
    return range.originKey == origin && range.start.offset == start && range.end.offset == end &&
           range.start.line == 1 && range.end.line == 1 &&
           range.start.column == (TZrInt32)start + 1 && range.end.column == (TZrInt32)end + 1;
}

static void loop_range_run(const char *source, const char *bodyText,
                           const char *bindingName, TZrBool foreachLoop, const char *reason) {
    SZrAstNode *loop;
    SZrAstNode *body;
    SZrAstNode *binding;
    SZrAstNode *following;
    SZrAstOwnership *owner;
    const SZrSemanticReferenceFact *declaration;
    const SZrSemanticReferenceFact *followingDeclaration;
    const SZrSemanticVisibleSymbolFact *visible;
    const SZrSemanticScopeFact *scope;
    const SZrSemanticSymbolRecord *ownerSymbol;
    SZrParserRangeEvidenceDescription loopDescription;
    SZrParserRangeEvidenceDescription bodyDescription;
    SZrParserRangeEvidenceDescription followingDescription;
    SZrParserRangeEvidenceDescription scopeDescription = {0};
    SZrFileRange first;
    SZrFileRange last;
    const char *loopText = strstr(source, "for (");
    TZrSize loopStart;
    TZrSize closeOffset = 64U + strlen(bodyText) - 1U;
    TZrBool passed;

    semantic_loop_range_evidence_cleanup();
    TEST_ASSERT_NOT_NULL_MESSAGE(loopText, "Fixture prerequisite: actual loop head text is missing");
    loopStart = (TZrSize)(loopText - source);
    loop_range_plain_control(source, bodyText, bindingName, foreachLoop);
    loop_range_parse(source, bodyText, ZR_TRUE);
    loop = loop_range_require_nodes(foreachLoop, &body, &binding, &following);
    owner = loop->ownership;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_RangeEvidence_Describe(owner, loop->location, &loopDescription) &&
            loopDescription.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL,
            "Fixture prerequisite: parsed loop has no complete owner-backed token interval");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_RangeEvidence_Describe(owner, body->location, &bodyDescription) &&
            bodyDescription.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL &&
            ZrParser_RangeEvidence_Describe(owner, following->data.variableDeclaration.pattern->location,
                                          &followingDescription) &&
            followingDescription.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL &&
            loopDescription.first < bodyDescription.first && bodyDescription.last < followingDescription.first,
            "Fixture prerequisite: loop/body/following token order is not real and distinct");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_RangeEvidence_AnchorAt(owner, loop->location, 0U, &first) &&
            loop_range_anchor_is(first, g_loopRange.originA, loopStart, loopStart + 3U) &&
            ZrParser_RangeEvidence_AnchorAt(owner, body->location,
                    bodyDescription.last - bodyDescription.first, &last) &&
            loop_range_anchor_is(last, g_loopRange.originB, closeOffset, closeOffset + 1U) &&
            following->data.variableDeclaration.pattern->location.originKey == g_loopRange.originA,
            "Fixture prerequisite: loop head/body close/following declaration lost their actual origins");
    loop_range_compile();
    loop_range_require_unchanged_tokens(source);
    declaration = loop_range_require_declaration(
            foreachLoop ? loop_range_require_foreach_witness(body) : binding);
    followingDeclaration = loop_range_require_declaration(following);
    TEST_ASSERT_TRUE_MESSAGE(declaration->symbolId != followingDeclaration->symbolId,
            foreachLoop ? "Fixture prerequisite: mixed foreach witness and following declaration share an identity" :
                    "Fixture prerequisite: mixed loop and following declarations share an identity");
    visible = symbol_find_visible_fact(g_loopRange.compiler.semanticContext, declaration->symbolId);
    TEST_ASSERT_NOT_NULL_MESSAGE(visible,
            "Fixture prerequisite: compiled loop binding has no published visible-symbol fact");
    scope = ZrParser_Semantic_FindScopeFactById(g_loopRange.compiler.semanticContext, visible->scopeId);
    TEST_ASSERT_NOT_NULL_MESSAGE(scope, "Fixture prerequisite: real loop binding scope is missing");
    if (foreachLoop) {
        const SZrSemanticScopeFact *bodyScope = scope;
        SZrParserRangeEvidenceDescription bodyScopeDescription;

        TEST_ASSERT_TRUE_MESSAGE(bodyScope->id == visible->scopeId &&
                bodyScope->kind == ZR_SEMANTIC_SCOPE_KIND_BLOCK &&
                bodyScope->parentScopeId != ZR_SEMANTIC_ID_INVALID &&
                ZrParser_RangeEvidence_Describe(owner, bodyScope->range, &bodyScopeDescription) &&
                bodyScopeDescription.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL &&
                bodyScopeDescription.first == bodyDescription.first &&
                bodyScopeDescription.last == bodyDescription.last &&
                bodyScope->range.ownerKey == ZrParser_Ownership_Key(owner) &&
                bodyScope->range.source == body->location.source && bodyScope->range.flags == 0U &&
                loop_range_anchor_is(bodyScope->range, g_loopRange.originB,
                        64U, 64U + strlen(bodyText)),
                "Fixture prerequisite: foreach witness must identify the exact published body scope");
        scope = ZrParser_Semantic_FindScopeFactById(
                g_loopRange.compiler.semanticContext, bodyScope->parentScopeId);
        TEST_ASSERT_TRUE_MESSAGE(scope != ZR_NULL && scope->id == bodyScope->parentScopeId &&
                scope->id != bodyScope->id && scope->ownerSymbolId == bodyScope->ownerSymbolId,
                "Fixture prerequisite: foreach body must identify its real parent loop scope");
    }
    ownerSymbol = ZrParser_Semantic_FindSymbolById(g_loopRange.compiler.semanticContext, scope->ownerSymbolId);
    TEST_ASSERT_TRUE_MESSAGE(scope->kind == ZR_SEMANTIC_SCOPE_KIND_BLOCK &&
            ownerSymbol != ZR_NULL && ownerSymbol->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
            ownerSymbol->astNode == g_loopRange.ast->data.script.statements->nodes[0] &&
            ZrParser_CanonicalType_Find(g_loopRange.compiler.semanticContext, ownerSymbol->typeId) != ZR_NULL,
            "Fixture prerequisite: loop scope has no real callable owner");

    /* Mixed-source query selection is deliberately not asserted by this M1 publication test. */
    passed = ZrParser_RangeEvidence_Describe(owner, scope->range, &scopeDescription) &&
            scopeDescription.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL &&
            scopeDescription.first == loopDescription.first && scopeDescription.last == bodyDescription.last &&
            scopeDescription.last < followingDescription.first &&
            scope->range.ownerKey == ZrParser_Ownership_Key(owner) &&
            scope->range.evidenceIndex != 0U && scope->range.flags == ZR_FILE_RANGE_FLAG_COMPOSITE &&
            loop_range_anchor_is(scope->range, g_loopRange.originA, loopStart, loopStart + 3U);
    if (passed) {
        for (TZrSize index = 0U; index <= scopeDescription.last - scopeDescription.first; index++) {
            SZrFileRange anchor;
            if (!ZrParser_RangeEvidence_AnchorAt(owner, scope->range, index, &anchor) ||
                (anchor.originKey != g_loopRange.originA && anchor.originKey != g_loopRange.originB)) {
                passed = ZR_FALSE;
                break;
            }
        }
        passed = passed && ZrParser_RangeEvidence_AnchorAt(owner, scope->range, 0U, &first) &&
                loop_range_anchor_is(first, g_loopRange.originA, loopStart, loopStart + 3U) &&
                ZrParser_RangeEvidence_AnchorAt(owner, scope->range,
                        scopeDescription.last - scopeDescription.first, &last) &&
                loop_range_anchor_is(last, g_loopRange.originB, closeOffset, closeOffset + 1U);
    }
    if (!passed) {
        printf("Loop scope evidence kind=%d interval=%u..%u expected=%u..%u next=%u physical=%llu..%llu flags=%u\n",
                (int)scopeDescription.kind, scopeDescription.first, scopeDescription.last,
                loopDescription.first, bodyDescription.last, followingDescription.first,
                (unsigned long long)scope->range.start.offset,
                (unsigned long long)scope->range.end.offset, (unsigned int)scope->range.flags);
    }
    semantic_loop_range_evidence_cleanup();
    TEST_ASSERT_TRUE_MESSAGE(passed, reason);
}

static void test_for_scope_preserves_complete_mixed_source_body_interval(void) {
    loop_range_run(
            "fn probe(seed: int): int { for (var step: int = 0; step < seed; step = step + 1) { step; } "
            "var step: int = seed; return step; }",
            "{ step; }", "step", ZR_FALSE,
            "For scope evidence must end at its real body close without absorbing the following declaration");
}

static void test_foreach_scope_preserves_complete_mixed_source_body_interval(void) {
    loop_range_run(
            "fn probe(seed: int): int { var values = [1, 2, 3]; for (var item in values) "
            "{ var scopeWitness: int = item; scopeWitness; } "
            "var item: int = seed; return item; }",
            "{ var scopeWitness: int = item; scopeWitness; }", "item", ZR_TRUE,
            "Foreach scope evidence must end at its real body close without absorbing the following declaration");
}

#endif
