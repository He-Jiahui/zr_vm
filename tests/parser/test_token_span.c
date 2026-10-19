#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "zr_vm_parser/token_span.h"

static SZrTokenSpan span_at(TZrTokenSourceId sourceId,
                            TZrSize start,
                            TZrSize end,
                            TZrInt32 column) {
    SZrTokenSpan span = {0};
    span.sourceId = sourceId;
    span.startOffset = start;
    span.endOffset = end;
    span.startLine = 3;
    span.endLine = 3;
    span.startColumn = column;
    span.endColumn = column + (TZrInt32)(end - start);
    return span;
}

static void test_none_and_reversed_spans_are_invalid(void) {
    SZrTokenSpan span = span_at(1U, 5U, 7U, 2);
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_IsValid(ZrParser_TokenSpan_None()));
    TEST_ASSERT_TRUE(ZrParser_TokenSpan_IsValid(span));
    span.endOffset = 4U;
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_IsValid(span));
    span = span_at(1U, 5U, 7U, 2);
    span.endColumn = 1;
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_IsValid(span));
    span = span_at(1U, 5U, 7U, 2);
    span.endLine = 2;
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_IsValid(span));
}

static void test_subspan_uses_absolute_source_offsets(void) {
    SZrTokenSpan whole = span_at(1U, 42U, 44U, 8);
    SZrTokenSpan first;
    SZrTokenSpan second;
    TEST_ASSERT_TRUE(ZrParser_TokenSpan_Subspan(whole, 42U, 43U, &first));
    TEST_ASSERT_TRUE(ZrParser_TokenSpan_Subspan(whole, 43U, 44U, &second));
    TEST_ASSERT_EQUAL_UINT64(42U, first.startOffset);
    TEST_ASSERT_EQUAL_UINT64(43U, first.endOffset);
    TEST_ASSERT_EQUAL_INT(8, first.startColumn);
    TEST_ASSERT_EQUAL_INT(9, first.endColumn);
    TEST_ASSERT_EQUAL_INT(9, second.startColumn);
    TEST_ASSERT_EQUAL_INT(10, second.endColumn);
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_Subspan(whole, 0U, 1U, &first));
}

static void test_failed_subspan_does_not_change_output(void) {
    SZrTokenSpan whole = span_at(1U, 42U, 44U, 8);
    SZrTokenSpan output = span_at(2U, 2U, 3U, 4);
    SZrTokenSpan before = output;
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_Subspan(whole, 41U, 43U, &output));
    TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_Subspan(whole, 43U, 45U, &output));
    TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_Subspan(whole, 44U, 43U, &output));
    TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
    whole.endLine = 4;
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_Subspan(whole, 42U, 43U, &output));
    TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
}

static void test_subspan_rejects_column_overflow(void) {
    SZrTokenSpan whole = span_at(1U, 42U, 42U, INT32_MAX);
    SZrTokenSpan output = {0};
    whole.endOffset = 43U;
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_Subspan(whole, 42U, 43U, &output));
}

static void test_join_preserves_outer_endpoints(void) {
    SZrTokenSpan left = span_at(1U, 42U, 44U, 8);
    SZrTokenSpan right = span_at(1U, 40U, 48U, 6);
    SZrTokenSpan joined;
    TEST_ASSERT_TRUE(ZrParser_TokenSpan_Join(left, right, &joined));
    TEST_ASSERT_EQUAL_UINT64(40U, joined.startOffset);
    TEST_ASSERT_EQUAL_UINT64(48U, joined.endOffset);
    TEST_ASSERT_EQUAL_INT(6, joined.startColumn);
    TEST_ASSERT_EQUAL_INT(14, joined.endColumn);
}

static void test_join_rejects_unrelated_sources_without_changing_output(void) {
    SZrTokenSpan left = span_at(1U, 42U, 44U, 8);
    SZrTokenSpan right = span_at(2U, 40U, 48U, 6);
    SZrTokenSpan output = span_at(3U, 0U, 1U, 1);
    SZrTokenSpan before = output;
    TEST_ASSERT_FALSE(ZrParser_TokenSpan_Join(left, right, &output));
    TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_none_and_reversed_spans_are_invalid);
    RUN_TEST(test_subspan_uses_absolute_source_offsets);
    RUN_TEST(test_failed_subspan_does_not_change_output);
    RUN_TEST(test_subspan_rejects_column_overflow);
    RUN_TEST(test_join_preserves_outer_endpoints);
    RUN_TEST(test_join_rejects_unrelated_sources_without_changing_output);
    return UNITY_END();
}
