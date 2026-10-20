/* RED coverage for the direct text/TokenStream parser boundary. */
#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/token_stream.h"

static SZrState *g_state;
static void mixed_postfix_cleanup(void);
static void token_source_projection_cleanup(void);
static void token_source_composite_range_cleanup(void);
static void token_source_diagnostic_evidence_cleanup(void);
static void token_source_semantic_range_cleanup(void);
static void token_source_construction_range_cleanup(void);
static void token_source_meta_name_range_cleanup(void);

typedef struct STokenProbe {
    EZrToken kind;
    TZrSize rawLength;
    TZrChar raw[32];
    SZrTokenSpan span;
    SZrSyntaxContext context;
    TZrBool hasLexError;
} STokenProbe;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    token_source_meta_name_range_cleanup();
    token_source_construction_range_cleanup();
    token_source_semantic_range_cleanup();
    token_source_diagnostic_evidence_cleanup();
    token_source_composite_range_cleanup();
    token_source_projection_cleanup();
    mixed_postfix_cleanup();
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static TZrSize collect_lexer_tokens(SZrLexState *lexer,
                                    STokenProbe *out,
                                    TZrSize capacity) {
    TZrSize count = 0;

    TEST_ASSERT_NOT_NULL(lexer);
    TEST_ASSERT_NOT_NULL(out);
    while (count < capacity) {
        const SZrToken *token = &lexer->t;
        TZrSize copyLength = token->rawLength;

        out[count].kind = token->token;
        out[count].rawLength = token->rawLength;
        out[count].span = token->span;
        out[count].context = token->syntaxContext;
        out[count].hasLexError = token->hasLexError;
        if (copyLength >= sizeof(out[count].raw)) {
            copyLength = sizeof(out[count].raw) - 1U;
        }
        if (copyLength > 0U) {
            TEST_ASSERT_NOT_NULL(token->rawText);
            memcpy(out[count].raw, token->rawText, copyLength);
        }
        out[count].raw[copyLength] = '\0';
        count++;
        if (token->token == ZR_TK_EOS) {
            break;
        }
        ZrParser_Lexer_Next(lexer);
    }
    return count;
}

static void assert_probe_equal(const STokenProbe *left,
                               const STokenProbe *right) {
    TEST_ASSERT_EQUAL_INT((int)left->kind, (int)right->kind);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)left->rawLength,
                             (uint64_t)right->rawLength);
    if (left->rawLength != 0u) {
        TEST_ASSERT_EQUAL_MEMORY(left->raw, right->raw, left->rawLength);
    }
    TEST_ASSERT_EQUAL_UINT64((uint64_t)left->span.startOffset,
                             (uint64_t)right->span.startOffset);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)left->span.endOffset,
                             (uint64_t)right->span.endOffset);
    TEST_ASSERT_EQUAL_INT(left->span.startLine, right->span.startLine);
    TEST_ASSERT_EQUAL_INT(left->span.startColumn, right->span.startColumn);
    TEST_ASSERT_EQUAL_INT(left->span.endLine, right->span.endLine);
    TEST_ASSERT_EQUAL_INT(left->span.endColumn, right->span.endColumn);
    TEST_ASSERT_EQUAL_INT(left->hasLexError, right->hasLexError);
}

static SZrTokenStream *stream_from_text(const TZrChar *source,
                                        SZrSyntaxContextTable *table) {
    SZrTokenStream *stream = ZR_NULL;

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            g_state,
            table,
            "token_source.zr",
            sizeof("token_source.zr") - 1U,
            source,
            strlen(source),
            ZR_NULL,
            &stream));
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_IsSealed(stream));
    return stream;
}

static SZrTokenSpan make_span(TZrTokenSourceId sourceId,
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

static SZrTokenStream *stream_with_joint_right_shift(SZrSyntaxContextTable *table) {
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    SZrTokenText raw;
    SZrTokenNativeSemInfo semInfo;
    SZrSyntaxContext context;
    TZrSize index;

    stream = ZrParser_TokenStream_Create(g_state->global, table, ZR_NULL);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, "shift.zr", sizeof("shift.zr") - 1U,
            ">>", 2U, &sourceId));
    context = ZrParser_SyntaxContext_TableRoot(table);
    memset(&semInfo, 0, sizeof(semInfo));
    semInfo.token = ZR_TK_GREATER_THAN;

    raw.sourceId = sourceId;
    raw.offset = 0U;
    raw.length = 1U;
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendPunct(
            stream, semInfo, raw, '>', ZR_TOKEN_SPACING_JOINT,
            make_span(sourceId, 0U, 1U, 1, 2), context,
            ZR_NULL, 0U, ZR_NULL, 0U, &index));

    raw.offset = 1U;
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendPunct(
            stream, semInfo, raw, '>', ZR_TOKEN_SPACING_ALONE,
            make_span(sourceId, 1U, 2U, 2, 3), context,
            ZR_NULL, 0U, ZR_NULL, 0U, &index));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));
    return stream;
}

/* Source and owned stream input must produce identical parser-facing tokens. */
static void test_text_and_owned_stream_have_equal_tokens(void) {
    static const TZrChar source[] = "foo >> 17\nbar";
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    SZrLexState textLexer;
    SZrLexState streamLexer;
    STokenProbe textTokens[8];
    STokenProbe streamTokens[8];
    TZrSize textCount;
    TZrSize streamCount;
    TZrSize i;

    table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(table);
    stream = stream_from_text(source, table);

    ZrParser_Lexer_Init(&textLexer, g_state, source, strlen(source), ZR_NULL);
    ZrParser_Lexer_InitTokenStream(&streamLexer, g_state, stream, ZR_NULL);
    textCount = collect_lexer_tokens(&textLexer, textTokens, 8U);
    streamCount = collect_lexer_tokens(&streamLexer, streamTokens, 8U);
    TEST_ASSERT_EQUAL_UINT((unsigned)textCount, (unsigned)streamCount);
    for (i = 0U; i < textCount; i++) {
        assert_probe_equal(&textTokens[i], &streamTokens[i]);
    }

    ZrParser_Lexer_Free(&textLexer);
    ZrParser_Lexer_Free(&streamLexer);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

/* Lookahead must be a pure operation, including repeated lookahead at EOS. */
static void test_lookahead_is_stable_at_eos(void) {
    static const TZrChar source[] = "value";
    SZrLexState lexer;
    TZrSize currentOffset;
    EZrToken lookaheadOne;
    EZrToken lookaheadTwo;

    ZrParser_Lexer_Init(&lexer, g_state, source, strlen(source), ZR_NULL);
    currentOffset = lexer.t.span.startOffset;
    lookaheadOne = ZrParser_Lexer_Lookahead(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lookaheadOne);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)currentOffset,
                             (uint64_t)lexer.t.span.startOffset);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lexer.t.token);
    lookaheadOne = ZrParser_Lexer_Lookahead(&lexer);
    lookaheadTwo = ZrParser_Lexer_Lookahead(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lookaheadOne);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lookaheadTwo);
    ZrParser_Lexer_Free(&lexer);
}

/* Joint punctuation is assembled once and can be split without source rewinds. */
static void test_joint_punctuation_split_preserves_subspans(void) {
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    SZrLexState lexer;

    table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(table);
    stream = stream_with_joint_right_shift(table);
    ZrParser_Lexer_InitTokenStream(&lexer, g_state, stream, ZR_NULL);

    TEST_ASSERT_EQUAL_INT(ZR_TK_RIGHT_SHIFT, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT(2U, (unsigned)lexer.t.rawLength);
    TEST_ASSERT_EQUAL_MEMORY(">>", lexer.t.rawText, 2U);
    TEST_ASSERT_TRUE(ZrParser_Lexer_SplitCurrentPunct(&lexer, '>'));
    TEST_ASSERT_EQUAL_INT(ZR_TK_GREATER_THAN, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT(1U, (unsigned)lexer.t.rawLength);
    TEST_ASSERT_EQUAL_UINT64(0U, (uint64_t)lexer.t.span.startOffset);
    TEST_ASSERT_EQUAL_UINT64(1U, (uint64_t)lexer.t.span.endOffset);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_GREATER_THAN, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT64(1U, (uint64_t)lexer.t.span.startOffset);
    TEST_ASSERT_EQUAL_UINT64(2U, (uint64_t)lexer.t.span.endOffset);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lexer.t.token);

    ZrParser_Lexer_Free(&lexer);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

/* Stream-owned source bytes and context owners outlive caller buffers. */
static void test_stream_owns_raw_bytes_and_context(void) {
    TZrChar source[] = "owned";
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    SZrLexState lexer;

    table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(table);
    stream = stream_from_text(source, table);
    source[0] = 'X';
    ZrParser_SyntaxContextTable_Release(table);

    ZrParser_Lexer_InitTokenStream(&lexer, g_state, stream, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, lexer.t.token);
    TEST_ASSERT_EQUAL_UINT(5U, (unsigned)lexer.t.rawLength);
    TEST_ASSERT_EQUAL_MEMORY("owned", lexer.t.rawText, 5U);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(lexer.t.syntaxContext));
    ZrParser_Lexer_Free(&lexer);
    ZrParser_TokenStream_Release(stream);
}

/* Parser state consumes stream leaves directly; there is no text round trip. */
static void test_parse_token_stream_entrypoint(void) {
    static const TZrChar source[] = "return 1 + 2;";
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    SZrAstNode *ast;
    SZrString *sourceName;

    table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(table);
    stream = stream_from_text(source, table);
    sourceName = ZrCore_String_CreateFromNative(g_state, "stream.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_ParseTokenStream(g_state, stream, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

/* Malformed source construction must fail with no partially published stream. */
static void test_invalid_text_stream_is_rejected(void) {
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream = ZR_NULL;

    table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_FALSE(ZrParser_TokenStream_FromText(
            g_state, table, "bad.zr", sizeof("bad.zr") - 1U,
            "\"unterminated", sizeof("\"unterminated") - 1U,
            ZR_NULL, &stream));
    TEST_ASSERT_NULL(stream);
    ZrParser_SyntaxContextTable_Release(table);
}

#include "test_token_source_mixed_postfix_cases.h"
#include "test_token_source_projection_cases.h"
#include "test_token_source_composite_range_cases.h"
#include "test_token_source_range_evidence_cases.h"
#include "test_token_source_diagnostic_evidence_cases.h"
#include "test_token_source_semantic_range_cases.h"
#include "test_token_source_construction_range_cases.h"
#include "test_token_source_meta_name_range_cases.h"

/* Unity runner; CMake registration remains root-owned. */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_text_and_owned_stream_have_equal_tokens);
    RUN_TEST(test_lookahead_is_stable_at_eos);
    RUN_TEST(test_joint_punctuation_split_preserves_subspans);
    RUN_TEST(test_stream_owns_raw_bytes_and_context);
    RUN_TEST(test_parse_token_stream_entrypoint);
    RUN_TEST(test_invalid_text_stream_is_rejected);
    RUN_TEST(test_mixed_source_postfix_preserves_origins_and_diagnostic_anchors);
    RUN_TEST(test_token_source_projection_is_pure_across_lookahead);
    RUN_TEST(test_token_source_projection_names_survive_ast_owner_and_gc);
    RUN_TEST(test_token_source_projection_name_budget_fails_closed_and_retries);
    RUN_TEST(test_operator_only_foreign_source_anchors_binary_parent);
    RUN_TEST(test_same_named_sources_keep_distinct_postfix_parent_anchor);
    RUN_TEST(test_reordered_single_source_anchors_first_native_token);
    RUN_TEST(test_embedded_template_retains_whole_origin_span);
    RUN_TEST(test_increasing_offsets_with_line_backtrack_keep_first_anchor);
    RUN_TEST(test_increasing_offsets_with_column_backtrack_keep_first_anchor);
    RUN_TEST(test_interior_operator_line_backtrack_is_composite);
    RUN_TEST(test_multiline_forward_coordinates_keep_contiguous_range);
    RUN_TEST(test_empty_stream_merge_rejects_noncanonical_coordinates);
    RUN_TEST(test_composite_diagnostic_preserves_all_physical_fragments);
    RUN_TEST(test_diagnostic_dedup_uses_native_interval_identity);
    RUN_TEST(test_diagnostic_evidence_budget_failure_is_atomic);
    RUN_TEST(test_diagnostic_evidence_rejects_foreign_context_domain);
    RUN_TEST(test_diagnostic_evidence_duplicate_success_charges_fuel);
    RUN_TEST(test_qualified_alias_preserves_mixed_type_use_evidence);
    RUN_TEST(test_generic_alias_preserves_long_source_evidence);
    RUN_TEST(test_primary_facts_preserve_mixed_call_evidence);
    RUN_TEST(test_new_construct_keeps_mixed_call_close_evidence);
    RUN_TEST(test_own_construct_keeps_mixed_call_close_evidence);
    RUN_TEST(test_new_construct_without_args_stops_at_target_evidence);
    RUN_TEST(test_new_generic_construct_without_args_stops_at_closing_angle_evidence);
    RUN_TEST(test_class_meta_name_range_keeps_at_token);
    RUN_TEST(test_struct_meta_name_range_keeps_at_token);
    RUN_TEST(test_class_meta_name_range_preserves_mixed_sources);
    RUN_TEST(test_struct_meta_name_range_preserves_mixed_sources);
    return UNITY_END();
}
