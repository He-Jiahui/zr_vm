#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "execution/execution_internal.h"

/* FAST_SET retains its real profile hook. These providers supply only the
 * optional current-profile seam; no profile runtime or Core library is linked. */
#if defined(_MSC_VER)
static unsigned profile_reads;
SZrProfileRuntime *ZrCore_Profile_Current(void) {
    if (profile_reads < UINT_MAX) ++profile_reads;
    return ZR_NULL;
}
#else
ZR_PROFILE_THREAD_LOCAL SZrProfileRuntime *g_zr_profile_current = ZR_NULL;
static SZrProfileRuntime test_profile;
#endif

void setUp(void) {
#if defined(_MSC_VER)
    profile_reads = 0u;
#else
    memset(&test_profile, 0, sizeof(test_profile));
    test_profile.recordHelpers = ZR_TRUE;
    g_zr_profile_current = &test_profile;
#endif
}

void tearDown(void) {
#if !defined(_MSC_VER)
    g_zr_profile_current = ZR_NULL;
#endif
}

static TZrUInt64 profile_observation(void) {
#if defined(_MSC_VER)
    return (TZrUInt64)profile_reads;
#else
    TEST_ASSERT_TRUE(g_zr_profile_current == &test_profile);
    return test_profile.helperCounts[ZR_PROFILE_HELPER_VALUE_CONSTRUCT];
#endif
}

static TZrBool observed_fast(SZrTypeValue *output, const SZrTypeValue *left,
                             const SZrTypeValue *right, TZrBool *zero,
                             unsigned expected_publications) {
    TZrUInt64 before = profile_observation();
    TZrBool handled;
    /* Each selected test makes fewer than UINT_MAX calls. Bound both the
     * provider and the expected sum before executing production publication. */
    TEST_ASSERT_TRUE(expected_publications <= 1u);
    TEST_ASSERT_TRUE(before < (TZrUInt64)UINT_MAX);
    handled = execution_try_builtin_mod_exact_integer_fast(output, left, right, zero);
    TEST_ASSERT_EQUAL_UINT64(before + expected_publications, profile_observation());
    return handled;
}

static SZrTypeValue signed_value(TZrInt64 value) {
    SZrTypeValue result;
    memset(&result, 0, sizeof(result));
    result.type = ZR_VALUE_TYPE_INT64;
    result.value.nativeObject.nativeInt64 = value;
    return result;
}

static SZrTypeValue unsigned_value(TZrUInt64 value) {
    SZrTypeValue result;
    memset(&result, 0, sizeof(result));
    result.type = ZR_VALUE_TYPE_UINT64;
    result.value.nativeObject.nativeUInt64 = value;
    return result;
}

static void check_signed(TZrInt64 left, TZrInt64 right, TZrInt64 expected) {
    SZrTypeValue a = signed_value(left), b = signed_value(right), result;
    SZrTypeValue before_a, before_b;
    memcpy(&before_a, &a, sizeof(a));
    memcpy(&before_b, &b, sizeof(b));
    TZrBool zero = ZR_TRUE;
    memset(&result, 0xA5, sizeof(result));
    TEST_ASSERT_TRUE(observed_fast(&result, &a, &b, &zero, 1u));
    TEST_ASSERT_FALSE(zero);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(expected, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_MEMORY(&before_a, &a, sizeof(a));
    TEST_ASSERT_EQUAL_MEMORY(&before_b, &b, sizeof(b));
}

static void check_unsigned(TZrUInt64 left, TZrUInt64 right, TZrUInt64 expected) {
    SZrTypeValue a = unsigned_value(left), b = unsigned_value(right), result;
    SZrTypeValue before_a, before_b;
    memcpy(&before_a, &a, sizeof(a));
    memcpy(&before_b, &b, sizeof(b));
    TZrBool zero = ZR_TRUE;
    memset(&result, 0xA5, sizeof(result));
    TEST_ASSERT_TRUE(observed_fast(&result, &a, &b, &zero, 1u));
    TEST_ASSERT_FALSE(zero);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, result.type);
    TEST_ASSERT_EQUAL_UINT64(expected, result.value.nativeObject.nativeUInt64);
    TEST_ASSERT_EQUAL_MEMORY(&before_a, &a, sizeof(a));
    TEST_ASSERT_EQUAL_MEMORY(&before_b, &b, sizeof(b));
}

/* The first call reaches the current production negative-divisor negation.
 * Sanitizer failure here is the intended old-code RED, not a missing API. */
static void test_fast_min_divisor(void) {
    const struct { TZrInt64 left, expected; } cases[] = {
        {1, 1}, {INT64_MIN, 0}, {INT64_MIN + 1, INT64_MIN + 1},
        {-1, -1}, {0, 0}, {INT64_MAX, INT64_MAX}
    };
    size_t i;
    printf("CASE fast_min_divisor begin\n");
    fflush(stdout);
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        check_signed(cases[i].left, INT64_MIN, cases[i].expected);
}

/* Repeated subtraction is an independent oracle only for this small domain.
 * It never negates INT64_MIN or uses the production helper to form expected. */
static TZrInt64 small_remainder(int left, int right) {
    int magnitude = left < 0 ? -left : left;
    const int divisor = right < 0 ? -right : right;
    while (magnitude >= divisor) magnitude -= divisor;
    return left < 0 ? -(TZrInt64)magnitude : (TZrInt64)magnitude;
}

static void test_fast_defined_cases(void) {
    const struct { TZrInt64 left, right, expected; } signed_cases[] = {
        {INT64_MIN, -1, 0}, {INT64_MIN, 1, 0},
        {INT64_MIN, 2, 0}, {INT64_MIN, INT64_MAX, -1},
        {INT64_MAX, -1, 0}, {INT64_MAX, INT64_MIN + 1, 0},
        {-7, 3, -1}, {-7, -3, -1}, {7, 3, 1}, {7, -3, 1},
        {0, -3, 0}, {-1, INT64_MAX, -1}
    };
    const struct { TZrUInt64 left, right, expected; } unsigned_cases[] = {
        {UINT64_MAX, 1, 0}, {UINT64_MAX, UINT64_MAX, 0},
        {UINT64_MAX, UINT64_MAX - 1u, 1}, {UINT64_MAX, 2, 1},
        {UINT64_MAX - 1u, UINT64_MAX, UINT64_MAX - 1u},
        {0, UINT64_MAX, 0}, {1, UINT64_MAX, 1}
    };
    size_t i;
    int left, right;
    for (i = 0; i < sizeof(signed_cases) / sizeof(signed_cases[0]); ++i)
        check_signed(signed_cases[i].left, signed_cases[i].right, signed_cases[i].expected);
    for (i = 0; i < sizeof(unsigned_cases) / sizeof(unsigned_cases[0]); ++i)
        check_unsigned(unsigned_cases[i].left, unsigned_cases[i].right, unsigned_cases[i].expected);
    for (left = -32; left <= 32; ++left)
        for (right = -32; right <= 32; ++right)
            if (right != 0) check_signed(left, right, small_remainder(left, right));
}

static void check_no_write(SZrTypeValue *output, const SZrTypeValue *a,
                           const SZrTypeValue *b, TZrBool handled, TZrBool expected_zero) {
    SZrTypeValue before;
    TZrBool zero = ZR_TRUE;
    if (output != ZR_NULL) memcpy(&before, output, sizeof(before));
    TEST_ASSERT_EQUAL_INT(handled,
        observed_fast(output, a, b, &zero, 0u));
    TEST_ASSERT_EQUAL_INT(expected_zero, zero);
    if (output != ZR_NULL) TEST_ASSERT_EQUAL_MEMORY(&before, output, sizeof(before));
}

static void check_alias(TZrBool is_unsigned, TZrBool output_is_right) {
    SZrTypeValue a = is_unsigned ? unsigned_value(UINT64_MAX) : signed_value(-7);
    SZrTypeValue b = is_unsigned ? unsigned_value(2) : signed_value(-3);
    SZrTypeValue before_a, before_b;
    memcpy(&before_a, &a, sizeof(a));
    memcpy(&before_b, &b, sizeof(b));
    SZrTypeValue *output = output_is_right ? &b : &a;
    TZrBool zero = ZR_TRUE;
    TEST_ASSERT_TRUE(observed_fast(output, &a, &b, &zero, 1u));
    TEST_ASSERT_FALSE(zero);
    if (is_unsigned) {
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, output->type);
        TEST_ASSERT_EQUAL_UINT64(1, output->value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, output->type);
        TEST_ASSERT_EQUAL_INT64(-1, output->value.nativeObject.nativeInt64);
    }
    if (output_is_right) TEST_ASSERT_EQUAL_MEMORY(&before_a, &a, sizeof(a));
    else TEST_ASSERT_EQUAL_MEMORY(&before_b, &b, sizeof(b));
}

static void test_fast_protocol(void) {
    SZrTypeValue a = signed_value(-7), b = signed_value(0), output, before;
    SZrTypeValue ua = unsigned_value(UINT64_MAX), ub = unsigned_value(0);
    SZrTypeValue other;
    TZrBool zero;
    memset(&output, 0xA5, sizeof(output));
    check_no_write(&output, &a, &b, ZR_TRUE, ZR_TRUE);
    check_no_write(&output, &ua, &ub, ZR_TRUE, ZR_TRUE);
    check_no_write(&a, &a, &b, ZR_TRUE, ZR_TRUE);
    check_no_write(&b, &a, &b, ZR_TRUE, ZR_TRUE);
    check_no_write(&ua, &ua, &ub, ZR_TRUE, ZR_TRUE);
    check_no_write(&ub, &ua, &ub, ZR_TRUE, ZR_TRUE);
    check_no_write(ZR_NULL, &a, &b, ZR_FALSE, ZR_FALSE);
    check_no_write(&output, ZR_NULL, &b, ZR_FALSE, ZR_FALSE);
    check_no_write(&output, &a, ZR_NULL, ZR_FALSE, ZR_FALSE);
    check_no_write(&output, &a, &ub, ZR_FALSE, ZR_FALSE);
    check_no_write(&output, &ua, &b, ZR_FALSE, ZR_FALSE);
    memset(&other, 0, sizeof(other));
    other.type = ZR_VALUE_TYPE_DOUBLE;
    other.value.nativeObject.nativeDouble = 3.0;
    check_no_write(&output, &a, &other, ZR_FALSE, ZR_FALSE);
    check_no_write(&output, &other, &other, ZR_FALSE, ZR_FALSE);
    other.type = ZR_VALUE_TYPE_BOOL;
    other.value.nativeObject.nativeBool = ZR_TRUE;
    check_no_write(&output, &other, &other, ZR_FALSE, ZR_FALSE);
    other.type = ZR_VALUE_TYPE_NULL;
    check_no_write(&output, &other, &a, ZR_FALSE, ZR_FALSE);

    /* A NULL zero-flag pointer is optional, including the handled-zero path. */
    memcpy(&before, &output, sizeof(before));
    TEST_ASSERT_TRUE(observed_fast(&output, &a, &b, ZR_NULL, 0u));
    TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
    TEST_ASSERT_FALSE(observed_fast(ZR_NULL, &a, &b, ZR_NULL, 0u));
    TEST_ASSERT_FALSE(observed_fast(&output, &a, &ub, ZR_NULL, 0u));
    TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
    b = signed_value(-3);
    TEST_ASSERT_TRUE(observed_fast(&output, &a, &b, ZR_NULL, 1u));
    TEST_ASSERT_EQUAL_INT64(-1, output.value.nativeObject.nativeInt64);
    check_alias(ZR_FALSE, ZR_FALSE);
    check_alias(ZR_FALSE, ZR_TRUE);
    check_alias(ZR_TRUE, ZR_FALSE);
    check_alias(ZR_TRUE, ZR_TRUE);

    /* Reuse one output across unsupported, zero, then healthy operation. */
    memset(&output, 0xA5, sizeof(output));
    check_no_write(&output, &a, &ub, ZR_FALSE, ZR_FALSE);
    b = signed_value(0);
    check_no_write(&output, &a, &b, ZR_TRUE, ZR_TRUE);
    b = signed_value(3);
    zero = ZR_TRUE;
    TEST_ASSERT_TRUE(observed_fast(&output, &a, &b, &zero, 1u));
    TEST_ASSERT_FALSE(zero);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, output.type);
    TEST_ASSERT_EQUAL_INT64(-1, output.value.nativeObject.nativeInt64);
}

int main(int argc, char **argv) {
    const char *selection = argc == 2 ? argv[1] : ZR_NULL;
    if (argc > 2 || (selection != ZR_NULL &&
        strcmp(selection, "--case=fast_min_divisor") != 0 &&
        strcmp(selection, "--case=fast_defined_cases") != 0 &&
        strcmp(selection, "--case=fast_protocol") != 0)) {
        fprintf(stderr, "expected --case=fast_min_divisor, fast_defined_cases, or fast_protocol\n");
        return 2;
    }
    UNITY_BEGIN();
    if (selection == ZR_NULL || strcmp(selection, "--case=fast_min_divisor") == 0)
        RUN_TEST(test_fast_min_divisor);
    if (selection == ZR_NULL || strcmp(selection, "--case=fast_defined_cases") == 0)
        RUN_TEST(test_fast_defined_cases);
    if (selection == ZR_NULL || strcmp(selection, "--case=fast_protocol") == 0)
        RUN_TEST(test_fast_protocol);
    return UNITY_END();
}
