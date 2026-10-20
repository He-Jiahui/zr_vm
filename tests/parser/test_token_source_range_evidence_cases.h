#ifndef ZR_TEST_TOKEN_SOURCE_RANGE_EVIDENCE_CASES_H
#define ZR_TEST_TOKEN_SOURCE_RANGE_EVIDENCE_CASES_H

#include "parser/parser_source_projection.h"

static void range_evidence_require_span(SZrTokenSpan expected, SZrTokenSpan actual) {
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected.sourceId, actual.sourceId,
            "Fixture prerequisite: evidence source identity changed");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(expected.startOffset, actual.startOffset,
            "Fixture prerequisite: evidence span start changed");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(expected.endOffset, actual.endOffset,
            "Fixture prerequisite: evidence span end changed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected.startLine, actual.startLine,
            "Fixture prerequisite: evidence start line changed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected.endLine, actual.endLine,
            "Fixture prerequisite: evidence end line changed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected.startColumn, actual.startColumn,
            "Fixture prerequisite: evidence start column changed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected.endColumn, actual.endColumn,
            "Fixture prerequisite: evidence end column changed");
}

static TZrBool range_evidence_matches_span(SZrFileRange range, SZrTokenSpan span) {
    return range.start.offset == span.startOffset && range.end.offset == span.endOffset &&
           range.start.line == span.startLine && range.end.line == span.endLine &&
           range.start.column == span.startColumn && range.end.column == span.endColumn;
}

static void range_evidence_coordinate_case(
        const char *source, const TZrSize offsets[4], const TZrInt32 lines[4],
        const TZrInt32 columns[4], TZrBool composite, const char *failure) {
    static const EZrToken tokens[4] = {ZR_TK_INTEGER, ZR_TK_PLUS, ZR_TK_INTEGER, ZR_TK_SEMICOLON};
    static const char spelling[4] = {'1', '+', '2', ';'};
    SZrTokenSpan spans[4];
    TZrTokenSourceId sourceId;
    SZrAstNode *binary;
    SZrFileRange expected;
    TZrSize i;

    composite_range_begin();
    composite_range_create_builder();
    sourceId = composite_range_add_source("coordinate.zr", source);
    for (i = 0U; i < 4U; ++i) {
        SZrTokenNativeSemInfo info = {0};
        SZrTokenText raw = {sourceId, offsets[i], 1U};
        TZrSize index = ZR_MAX_SIZE;
        TZrBool appended;

        spans[i] = make_span(sourceId, offsets[i], offsets[i] + 1U, columns[i], columns[i] + 1);
        spans[i].startLine = lines[i];
        spans[i].endLine = lines[i];
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenSpan_IsValid(spans[i]),
                "Fixture prerequisite: coordinate span is independently invalid");
        info.token = tokens[i];
        if (i == 0U || i == 2U) {
            info.intValue = i == 0U ? 1 : 2;
            appended = ZrParser_TokenStream_AppendLiteral(g_compositeRange.stream, info, raw,
                    spans[i], ZrParser_SyntaxContext_TableRoot(g_compositeRange.table),
                    ZR_NULL, 0U, ZR_NULL, 0U, &index);
        } else {
            appended = ZrParser_TokenStream_AppendPunct(g_compositeRange.stream, info, raw,
                    (TZrUInt32)spelling[i], ZR_TOKEN_SPACING_ALONE, spans[i],
                    ZrParser_SyntaxContext_TableRoot(g_compositeRange.table),
                    ZR_NULL, 0U, ZR_NULL, 0U, &index);
        }
        TEST_ASSERT_TRUE_MESSAGE(appended, "Fixture prerequisite: coordinate token append failed");
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(i, index,
                "Fixture prerequisite: coordinate token root order changed");
    }
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Seal(g_compositeRange.stream),
            "Fixture prerequisite: coordinate stream sealing failed");
    binary = composite_range_require_binary(composite_range_parse(1U));
    for (i = 0U; i < 4U; ++i) {
        const SZrTokenTree *tree = ZrParser_TokenStream_At(g_compositeRange.stream, i);
        SZrTokenNativeSemInfo info;
        SZrTokenText raw;
        SZrSyntaxContext context;
        const char *bytes;

        TEST_ASSERT_NOT_NULL_MESSAGE(tree, "Fixture prerequisite: coordinate token disappeared");
        range_evidence_require_span(spans[i], ZrParser_TokenTree_Span(tree));
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenTree_NativeSemInfo(tree, &info),
                "Fixture prerequisite: coordinate token payload disappeared");
        TEST_ASSERT_EQUAL_INT_MESSAGE(tokens[i], info.token,
                "Fixture prerequisite: coordinate token kind changed");
        context = ZrParser_TokenTree_Context(tree);
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_SyntaxContext_Equal(
                context, ZrParser_SyntaxContext_TableRoot(g_compositeRange.table)),
                "Fixture prerequisite: coordinate token context changed");
        raw = ZrParser_TokenTree_Text(tree);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(sourceId, raw.sourceId,
                "Fixture prerequisite: coordinate raw source changed");
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(offsets[i], raw.offset,
                "Fixture prerequisite: coordinate raw offset changed");
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(1U, raw.length,
                "Fixture prerequisite: coordinate raw length changed");
        bytes = ZrParser_TokenStream_TextBytes(g_compositeRange.stream, raw);
        TEST_ASSERT_NOT_NULL_MESSAGE(bytes, "Fixture prerequisite: coordinate raw bytes missing");
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&spelling[i], bytes, 1U,
                "Fixture prerequisite: coordinate raw spelling changed");
    }
    TEST_ASSERT_TRUE_MESSAGE(range_evidence_matches_span(binary->data.binaryExpression.left->location, spans[0]),
            "Fixture prerequisite: coordinate left leaf anchor changed");
    TEST_ASSERT_TRUE_MESSAGE(range_evidence_matches_span(binary->data.binaryExpression.right->location, spans[2]),
            "Fixture prerequisite: coordinate right leaf anchor changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(binary->location.source,
            "Fixture prerequisite: coordinate parent source missing");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("coordinate.zr",
            ZrCore_String_GetNativeString(binary->location.source),
            "Fixture prerequisite: coordinate parent source changed");
    TEST_ASSERT_TRUE_MESSAGE(binary->location.originKey != 0U && binary->location.ownerKey != 0U &&
            binary->location.evidenceIndex != 0U,
            "Fixture prerequisite: coordinate parent evidence missing");
    expected = binary->data.binaryExpression.left->location;
    if (!composite) {
        expected.end = binary->data.binaryExpression.right->location.end;
    }
    TEST_ASSERT_TRUE_MESSAGE(
            binary->location.start.offset == expected.start.offset &&
            binary->location.start.line == expected.start.line &&
            binary->location.start.column == expected.start.column &&
            binary->location.end.offset == expected.end.offset &&
            binary->location.end.line == expected.end.line &&
            binary->location.end.column == expected.end.column &&
            binary->location.flags == (composite ? ZR_FILE_RANGE_FLAG_COMPOSITE : 0U), failure);
}

static void test_increasing_offsets_with_line_backtrack_keep_first_anchor(void) {
    static const TZrSize offsets[4] = {0U, 1U, 2U, 3U};
    static const TZrInt32 lines[4] = {2, 1, 1, 1};
    static const TZrInt32 columns[4] = {1, 1, 2, 3};
    range_evidence_coordinate_case("1+2;", offsets, lines, columns, ZR_TRUE,
            "Line-backtracking evidence must preserve the first anchor as composite");
}

static void test_increasing_offsets_with_column_backtrack_keep_first_anchor(void) {
    static const TZrSize offsets[4] = {0U, 1U, 2U, 3U};
    static const TZrInt32 lines[4] = {1, 1, 1, 1};
    static const TZrInt32 columns[4] = {7, 1, 2, 3};
    range_evidence_coordinate_case("1+2;", offsets, lines, columns, ZR_TRUE,
            "Column-backtracking evidence must preserve the first anchor as composite");
}

static void test_interior_operator_line_backtrack_is_composite(void) {
    static const TZrSize offsets[4] = {0U, 1U, 2U, 3U};
    static const TZrInt32 lines[4] = {1, 5, 2, 2};
    static const TZrInt32 columns[4] = {1, 1, 1, 2};
    range_evidence_coordinate_case("1+2;", offsets, lines, columns, ZR_TRUE,
            "Interior operator coordinate backtracking must remain composite");
}

static void test_multiline_forward_coordinates_keep_contiguous_range(void) {
    static const TZrSize offsets[4] = {0U, 2U, 4U, 5U};
    static const TZrInt32 lines[4] = {1, 2, 3, 3};
    static const TZrInt32 columns[4] = {1, 1, 1, 2};
    range_evidence_coordinate_case("1\n+\n2;", offsets, lines, columns, ZR_FALSE,
            "Forward multiline coordinates must retain the contiguous range");
}

static SZrFileRange range_evidence_empty_parser(void) {
    SZrParserState *ps = &g_compositeRange.parser;
    SZrFileRange canonical;

    composite_range_begin();
    composite_range_create_builder();
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Seal(g_compositeRange.stream),
            "Fixture prerequisite: empty evidence stream sealing failed");
    g_compositeRange.parserInitialized = ZR_TRUE;
    ZrParser_State_InitWithTokenStream(ps, g_state, g_compositeRange.stream, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(ps->lexer, "Fixture prerequisite: empty evidence lexer missing");
    ps->suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE_MESSAGE(ps->hasError || ps->hasFatalError || ps->lexer->hasSourceEvidenceError,
            "Fixture prerequisite: empty evidence parser initialization failed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_TK_EOS, ps->lexer->t.token,
            "Fixture prerequisite: empty evidence stream is not at EOS");
    canonical = ZrParser_SourceProjection_CurrentLocation(ps);
    TEST_ASSERT_TRUE_MESSAGE(canonical.start.offset == 0U && canonical.end.offset == 0U &&
            canonical.start.line == 1 && canonical.end.line == 1 &&
            canonical.start.column == 1 && canonical.end.column == 1 &&
            canonical.source == ZR_NULL && canonical.originKey == 0U &&
            canonical.ownerKey == 0U && canonical.evidenceIndex == 0U && canonical.flags == 0U,
            "Fixture prerequisite: canonical empty evidence anchor changed");
    return canonical;
}

static void test_empty_stream_merge_rejects_noncanonical_coordinates(void) {
    SZrParserState *ps = &g_compositeRange.parser;
    TZrSize variant;

    for (variant = 0U; variant < 8U; ++variant) {
        SZrFileRange canonical = range_evidence_empty_parser();
        SZrFileRange bad = canonical;
        SZrFileRange merged;
        SZrFileRange zero = {0};
        TZrBool accepted;

        TEST_ASSERT_TRUE_MESSAGE(ZrParser_MergeLocations(ps, canonical, canonical, &merged),
                "Fixture prerequisite: canonical empty merge was rejected");
        TEST_ASSERT_TRUE_MESSAGE(merged.start.line == 1 && merged.end.line == 1 &&
                merged.start.column == 1 && merged.end.column == 1 &&
                merged.start.offset == 0U && merged.end.offset == 0U && merged.source == ZR_NULL &&
                merged.originKey == 0U && merged.ownerKey == 0U &&
                merged.evidenceIndex == 0U && merged.flags == 0U,
                "Fixture prerequisite: canonical empty merge fabricated evidence");
        switch (variant % 4U) {
            case 0U: bad.start.line = 999; break;
            case 1U: bad.end.line = 999; break;
            case 2U: bad.start.column = 999; break;
            default: bad.end.column = 999; break;
        }
        memset(&merged, 0xA5, sizeof(merged));
        accepted = variant < 4U ? ZrParser_MergeLocations(ps, bad, canonical, &merged) :
                                 ZrParser_MergeLocations(ps, canonical, bad, &merged);
        TEST_ASSERT_FALSE_MESSAGE(accepted,
                "Noncanonical empty-stream coordinates must fail closed");
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&zero, &merged, sizeof(merged),
                "Rejected empty evidence merge must clear its output");
        TEST_ASSERT_TRUE_MESSAGE(ps->lexer->hasSourceEvidenceError && ps->hasError && ps->hasFatalError,
                "Rejected empty evidence merge must preserve sticky failure");
        g_compositeRange.ast = ZrParser_ParseWithState(ps);
        TEST_ASSERT_NULL_MESSAGE(g_compositeRange.ast,
                "An evidence failure must prevent subsequent parse publication");
        (void)range_evidence_empty_parser();
        g_compositeRange.ast = ZrParser_ParseWithState(ps);
        TEST_ASSERT_NOT_NULL_MESSAGE(g_compositeRange.ast,
                "A fresh canonical empty parser must recover after evidence failure");
        TEST_ASSERT_FALSE_MESSAGE(ps->hasError || ps->hasFatalError || ps->lexer->hasSourceEvidenceError,
                "A fresh canonical empty parser must not inherit evidence failure");
    }
}

#endif
