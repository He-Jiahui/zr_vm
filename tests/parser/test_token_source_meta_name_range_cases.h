#ifndef ZR_TEST_TOKEN_SOURCE_META_NAME_RANGE_CASES_H
#define ZR_TEST_TOKEN_SOURCE_META_NAME_RANGE_CASES_H

#include <stddef.h>

#include "parser/parser_ownership.h"
#include "parser/parser_range_evidence.h"

enum { ZR_TEST_META_NAME_RANGE_STREAMS = 3 };

typedef struct SZrMetaNameRangeFixture {
    SZrSyntaxContextTable *table;
    SZrTokenStream *streams[ZR_TEST_META_NAME_RANGE_STREAMS];
    SZrParserState parser;
    TZrBool parserInitialized;
    SZrAstNode *ast;
    SZrGcRootLease entryNameRoot;
} SZrMetaNameRangeFixture;

static SZrMetaNameRangeFixture g_metaNameRange;

static void token_source_meta_name_range_cleanup(void) {
    ZrParser_Ast_Free(g_state, g_metaNameRange.ast);
    if (g_metaNameRange.parserInitialized) {
        ZrParser_State_Free(&g_metaNameRange.parser);
    }
    if (g_state != ZR_NULL) {
        ZrCore_Gc_RootLeaseRelease(g_state->global, &g_metaNameRange.entryNameRoot);
    }
    for (TZrSize index = ZR_TEST_META_NAME_RANGE_STREAMS; index > 0U; --index) {
        ZrParser_TokenStream_Release(g_metaNameRange.streams[index - 1U]);
    }
    ZrParser_SyntaxContextTable_Release(g_metaNameRange.table);
    memset(&g_metaNameRange, 0, sizeof(g_metaNameRange));
}

static SZrTokenSpan meta_name_range_span(TZrTokenSourceId source, TZrSize start, TZrSize end) {
    SZrTokenSpan span = {0};
    span.sourceId = source;
    span.startOffset = start;
    span.endOffset = end;
    span.startLine = span.endLine = 1;
    span.startColumn = (TZrInt32)start + 1;
    span.endColumn = (TZrInt32)end + 1;
    return span;
}

static TZrBool meta_name_range_physical(SZrFileRange range, const char *source,
        TZrSize start, TZrSize end) {
    return range.source != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(range.source), source) == 0 &&
            range.start.offset == start && range.end.offset == end &&
            range.start.line == 1 && range.end.line == 1 &&
            range.start.column == (TZrInt32)start + 1 &&
            range.end.column == (TZrInt32)end + 1;
}

static TZrBool meta_name_range_interval(SZrAstOwnership *owner, SZrFileRange range,
        TZrUInt32 first, TZrUInt32 last) {
    SZrParserRangeEvidenceDescription description;
    return ZrParser_RangeEvidence_Describe(owner, range, &description) &&
            description.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL &&
            description.first == first && description.last == last;
}

static SZrTokenStream *meta_name_range_mixed_input(TZrBool isClass) {
    const char *envelope = isClass ? "class Box {}" : "struct Box {}";
    const char *head = "pub @";
    const char *tail = "constructor() {}";
    const SZrTokenTree *children[5];
    SZrTokenStream *stream;
    TZrTokenSourceId source;
    TZrSize keywordLength = isClass ? 5U : 6U;
    TZrSize brace = keywordLength + 5U;
    TZrSize index;
    TZrSize appended;

    g_metaNameRange.table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_metaNameRange.table);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(g_state, g_metaNameRange.table,
            "meta_at.zr", strlen("meta_at.zr"), head, strlen(head), ZR_NULL,
            &g_metaNameRange.streams[0]), "Fixture prerequisite: meta head must tokenize");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(g_state, g_metaNameRange.table,
            "meta_name.zr", strlen("meta_name.zr"), tail, strlen(tail), ZR_NULL,
            &g_metaNameRange.streams[1]), "Fixture prerequisite: meta tail must tokenize");
    TEST_ASSERT_TRUE(ZrParser_TokenStream_IsSealed(g_metaNameRange.streams[0]));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_IsSealed(g_metaNameRange.streams[1]));
    TEST_ASSERT_EQUAL_UINT64(2U, ZrParser_TokenStream_Count(g_metaNameRange.streams[0]));
    TEST_ASSERT_EQUAL_UINT64(3U, ZrParser_TokenStream_Count(g_metaNameRange.streams[1]));
    children[0] = ZrParser_TokenStream_At(g_metaNameRange.streams[0], 0U);
    children[1] = ZrParser_TokenStream_At(g_metaNameRange.streams[0], 1U);
    for (index = 0U; index < 3U; ++index) {
        children[index + 2U] = ZrParser_TokenStream_At(g_metaNameRange.streams[1], index);
    }
    g_metaNameRange.streams[2] = ZrParser_TokenStream_Create(
            g_state->global, g_metaNameRange.table, ZR_NULL);
    stream = g_metaNameRange.streams[2];
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(stream, "meta_envelope.zr",
            strlen("meta_envelope.zr"), envelope, strlen(envelope), &source));
    for (index = 0U; index < 2U; ++index) {
        SZrTokenNativeSemInfo info = {0};
        SZrTokenText raw;
        raw.sourceId = source;
        raw.offset = index == 0U ? 0U : keywordLength + 1U;
        raw.length = index == 0U ? keywordLength : 3U;
        info.token = index == 0U ? (isClass ? ZR_TK_CLASS : ZR_TK_STRUCT) : ZR_TK_IDENTIFIER;
        info.text = raw;
        TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendIdent(stream, info, raw,
                meta_name_range_span(source, raw.offset, raw.offset + raw.length),
                ZrParser_SyntaxContext_TableRoot(g_metaNameRange.table),
                ZR_NULL, 0U, ZR_NULL, 0U, &appended));
        TEST_ASSERT_EQUAL_UINT64(index, appended);
    }
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendGroup(stream, ZR_TOKEN_DELIMITER_BRACE,
            meta_name_range_span(source, brace, brace + 1U),
            meta_name_range_span(source, brace + 1U, brace + 2U), children, 5U,
            meta_name_range_span(source, brace, brace + 2U),
            ZrParser_SyntaxContext_TableRoot(g_metaNameRange.table),
            ZR_NULL, 0U, ZR_NULL, 0U, &appended));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));
    TEST_ASSERT_EQUAL_UINT64(3U, ZrParser_TokenStream_Count(stream));
    return stream;
}

static SZrAstNode *meta_name_range_parse(TZrBool isClass, TZrBool mixed) {
    const char *source = isClass ? "class Box { pub @constructor() {} }" :
            "struct Box { pub @constructor() {} }";
    SZrTokenStream *stream = mixed ? meta_name_range_mixed_input(isClass) : ZR_NULL;
    SZrString *entryName = ZrCore_String_CreateFromNative(g_state, "meta_plain.zr");
    SZrAstNode *type;
    SZrAstNodeArray *members;

    TEST_ASSERT_NOT_NULL(entryName);
    TEST_ASSERT_TRUE(ZrCore_Gc_RootLeaseAcquire(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(entryName), &g_metaNameRange.entryNameRoot));
    g_metaNameRange.parserInitialized = ZR_TRUE;
    if (mixed) {
        ZrParser_State_InitWithTokenStream(&g_metaNameRange.parser, g_state, stream, entryName);
    } else {
        ZrParser_State_Init(&g_metaNameRange.parser, g_state, source, strlen(source), entryName);
    }
    TEST_ASSERT_NOT_NULL(g_metaNameRange.parser.lexer);
    g_metaNameRange.parser.suppressErrorOutput = ZR_TRUE;
    g_metaNameRange.ast = ZrParser_ParseWithState(&g_metaNameRange.parser);
    TEST_ASSERT_TRUE_MESSAGE(g_metaNameRange.ast != ZR_NULL &&
            !g_metaNameRange.parser.hasError && !g_metaNameRange.parser.hasFatalError &&
            !g_metaNameRange.parser.hasNestingError &&
            !g_metaNameRange.parser.lexer->hasSourceEvidenceError &&
            !g_metaNameRange.parser.lexer->t.hasLexError &&
            g_metaNameRange.parser.lexer->t.token == ZR_TK_EOS,
            "Fixture prerequisite: meta declaration must parse completely without errors");
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_metaNameRange.ast->type);
    TEST_ASSERT_NOT_NULL(g_metaNameRange.ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1U, g_metaNameRange.ast->data.script.statements->count);
    type = g_metaNameRange.ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_EQUAL_INT(isClass ? ZR_AST_CLASS_DECLARATION : ZR_AST_STRUCT_DECLARATION, type->type);
    members = isClass ? type->data.classDeclaration.members : type->data.structDeclaration.members;
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_NOT_NULL(members->nodes);
    TEST_ASSERT_EQUAL_UINT64(1U, members->count);
    TEST_ASSERT_NOT_NULL(members->nodes[0]);
    TEST_ASSERT_EQUAL_INT(isClass ? ZR_AST_CLASS_META_FUNCTION : ZR_AST_STRUCT_META_FUNCTION,
            members->nodes[0]->type);
    return members->nodes[0];
}

static TZrBool meta_name_range_case(TZrBool isClass, TZrBool mixed) {
    SZrAstNode *member = meta_name_range_parse(isClass, mixed);
    SZrIdentifier *name = isClass ? member->data.classMetaFunction.meta :
            member->data.structMetaFunction.meta;
    SZrAstNodeArray *params = isClass ? member->data.classMetaFunction.params :
            member->data.structMetaFunction.params;
    SZrAstNode *body = isClass ? member->data.classMetaFunction.body :
            member->data.structMetaFunction.body;
    SZrFileRange nameLocation = isClass ? member->data.classMetaFunction.nameLocation :
            member->data.structMetaFunction.nameLocation;
    SZrAstNode *nameNode;
    SZrAstOwnership *owner = member->ownership;
    SZrParserRangeEvidenceDescription description = {0};
    SZrFileRange first = {0};
    SZrFileRange last = {0};
    SZrFileRange beyond;
    TZrSize at = isClass ? 16U : 17U;
    TZrBool passed;

    TEST_ASSERT_NOT_NULL(owner);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NOT_NULL(name->name);
    TEST_ASSERT_EQUAL_STRING("constructor", ZrCore_String_GetNativeString(name->name));
    nameNode = (SZrAstNode *)((char *)name - offsetof(SZrAstNode, data.identifier));
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, nameNode->type);
    TEST_ASSERT_TRUE(nameNode->ownership == owner);
    TEST_ASSERT_NOT_NULL(params);
    TEST_ASSERT_EQUAL_UINT64(0U, params->count);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(0U, body->data.block.body->count);
    TEST_ASSERT_TRUE(ZrParser_RangeEvidence_Validate(owner, member->location));
    TEST_ASSERT_TRUE(ZrParser_RangeEvidence_Validate(owner, nameNode->location));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(name->syntaxContext, mixed ?
            ZrParser_SyntaxContext_TableRoot(g_metaNameRange.table) : ZrParser_SyntaxContext_Root()));
    if (mixed) {
        TEST_ASSERT_TRUE_MESSAGE(meta_name_range_interval(owner, member->location, 4U, 11U) &&
                meta_name_range_physical(member->location, "meta_at.zr", 0U, 3U) &&
                meta_name_range_interval(owner, nameNode->location, 6U, 6U) &&
                meta_name_range_physical(nameNode->location, "meta_name.zr", 0U, 11U) &&
                meta_name_range_interval(owner, body->location, 9U, 10U) &&
                g_metaNameRange.parser.lexer->t.firstNativeOrdinal == 12U &&
                g_metaNameRange.parser.lexer->t.lastNativeOrdinal == 12U &&
                g_metaNameRange.parser.lexer->t.span.sourceId ==
                        ZrParser_TokenStream_EndSpan(g_metaNameRange.streams[2]).sourceId,
                "Fixture prerequisite: original member, leaf, body and EOS evidence must remain intact");
        passed = ZrParser_RangeEvidence_Describe(owner, nameLocation, &description) &&
                description.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL &&
                description.first == 5U && description.last == 6U &&
                nameLocation.ownerKey == ZrParser_Ownership_Key(owner) &&
                nameLocation.flags == ZR_FILE_RANGE_FLAG_COMPOSITE &&
                meta_name_range_physical(nameLocation, "meta_at.zr", 4U, 5U) &&
                ZrParser_RangeEvidence_AnchorAt(owner, nameLocation, 0U, &first) &&
                ZrParser_RangeEvidence_AnchorAt(owner, nameLocation, 1U, &last) &&
                meta_name_range_interval(owner, first, 5U, 5U) &&
                meta_name_range_interval(owner, last, 6U, 6U) &&
                meta_name_range_physical(first, "meta_at.zr", 4U, 5U) &&
                meta_name_range_physical(last, "meta_name.zr", 0U, 11U) &&
                first.originKey != last.originKey &&
                !ZrParser_RangeEvidence_AnchorAt(owner, nameLocation, 2U, &beyond);
    } else {
        TEST_ASSERT_TRUE_MESSAGE(meta_name_range_physical(nameNode->location, "meta_plain.zr",
                at + 1U, at + 12U) && member->location.start.offset < at &&
                member->location.end.offset > at + 12U,
                "Fixture prerequisite: the real identifier and full declaration must remain distinct");
        passed = ZrParser_RangeEvidence_Describe(owner, nameLocation, &description) &&
                description.kind == ZR_PARSER_RANGE_EVIDENCE_SOURCE && nameLocation.flags == 0U &&
                meta_name_range_physical(nameLocation, "meta_plain.zr", at, at + 12U);
    }
    printf("Meta name class=%u mixed=%u proof=%u interval=%u..%u flags=%u physical=%llu..%llu\n",
            (unsigned)isClass, (unsigned)mixed, (unsigned)passed,
            (unsigned)description.first, (unsigned)description.last, (unsigned)nameLocation.flags,
            (unsigned long long)nameLocation.start.offset, (unsigned long long)nameLocation.end.offset);
    token_source_meta_name_range_cleanup();
    return passed;
}

static void test_class_meta_name_range_keeps_at_token(void) {
    TEST_ASSERT_TRUE_MESSAGE(meta_name_range_case(ZR_TRUE, ZR_FALSE),
            "Class meta names must include the real @ and exclude the signature and body");
}

static void test_struct_meta_name_range_keeps_at_token(void) {
    TEST_ASSERT_TRUE_MESSAGE(meta_name_range_case(ZR_FALSE, ZR_FALSE),
            "Struct meta names must include the real @ and exclude the signature and body");
}

static void test_class_meta_name_range_preserves_mixed_sources(void) {
    TEST_ASSERT_TRUE_MESSAGE(meta_name_range_case(ZR_TRUE, ZR_TRUE),
            "Class meta names must retain native interval [5,6] and both original sources");
}

static void test_struct_meta_name_range_preserves_mixed_sources(void) {
    TEST_ASSERT_TRUE_MESSAGE(meta_name_range_case(ZR_FALSE, ZR_TRUE),
            "Struct meta names must retain native interval [5,6] and both original sources");
}

#endif
