#include "zr_vm_language_server_stdio_internal.h"
#include "unity.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void expect_size(double value, TZrBool valid) {
    cJSON number = {0};
    TZrSize parsed = 7;

    number.type = cJSON_Number;
    number.valuedouble = value;
    TEST_ASSERT_EQUAL(valid, parse_size_value_strict(&number, &parsed));
    if (valid) {
        TEST_ASSERT_TRUE(value == (double)parsed);
        TEST_ASSERT_TRUE(value == (double)parse_size_value(&number, 19));
    } else {
        TEST_ASSERT_EQUAL_UINT64(7, parsed);
        TEST_ASSERT_EQUAL_UINT64(19, parse_size_value(&number, 19));
    }
}

static void test_size_rejects_exclusive_upper_bound(void) {
    expect_size(ldexp(1.0, (int)(sizeof(TZrSize) * CHAR_BIT)), ZR_FALSE);
}

static void test_size_accepts_representable_integers(void) {
    double upperBound = ldexp(1.0, (int)(sizeof(TZrSize) * CHAR_BIT));

    expect_size(0, ZR_TRUE);
    expect_size(-0.0, ZR_TRUE);
    expect_size(1, ZR_TRUE);
    expect_size(INT32_MAX, ZR_TRUE);
    expect_size(floor(nextafter(upperBound, 0.0)), ZR_TRUE);
}

static void test_size_rejects_invalid_numbers(void) {
    const double invalid[] = {-1, -0.5, 0.5, 1.5, NAN, INFINITY, -INFINITY, DBL_MAX};
    TZrSize index;

    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index++) {
        expect_size(invalid[index], ZR_FALSE);
    }
}

static void test_size_rejects_non_numbers_and_null_output(void) {
    const int types[] = {cJSON_NULL, cJSON_True, cJSON_False, cJSON_String,
                         cJSON_Array, cJSON_Object};
    cJSON json = {0};
    TZrSize index;
    TZrSize parsed = 7;

    TEST_ASSERT_FALSE(parse_size_value_strict(NULL, &parsed));
    TEST_ASSERT_EQUAL_UINT64(19, parse_size_value(NULL, 19));
    for (index = 0; index < sizeof(types) / sizeof(types[0]); index++) {
        json.type = types[index];
        TEST_ASSERT_FALSE(parse_size_value_strict(&json, &parsed));
        TEST_ASSERT_EQUAL_UINT64(7, parsed);
        TEST_ASSERT_EQUAL_UINT64(19, parse_size_value(&json, 19));
    }
    json.type = cJSON_Number;
    TEST_ASSERT_FALSE(parse_size_value_strict(&json, NULL));
}

static void expect_position(double line, double character, TZrBool valid) {
    cJSON *json = cJSON_CreateObject();
    SZrLspPosition position = {0};
    int result;

    TEST_ASSERT_NOT_NULL(json);
    /* cJSON's number constructor casts NaN to its integer cache. */
    cJSON_AddNumberToObject(json, "line", 0)->valuedouble = line;
    cJSON_AddNumberToObject(json, "character", 0)->valuedouble = character;
    result = parse_position(json, &position);
    cJSON_Delete(json);
    TEST_ASSERT_EQUAL(valid, result);
    if (valid) {
        TEST_ASSERT_TRUE(line == (double)position.line);
        TEST_ASSERT_TRUE(character == (double)position.character);
    }
}

static void test_position_accepts_integer_boundaries(void) {
    expect_position(0, 0, ZR_TRUE);
    expect_position(-0.0, -0.0, ZR_TRUE);
    expect_position(INT32_MAX, INT32_MAX, ZR_TRUE);
}

static void test_position_rejects_invalid_components(void) {
    const double invalid[] = {-1, -0.5, 0.5, NAN, INFINITY, -INFINITY,
                              (double)INT32_MAX + 1, DBL_MAX};
    TZrSize index;

    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index++) {
        expect_position(invalid[index], 0, ZR_FALSE);
        expect_position(0, invalid[index], ZR_FALSE);
    }
}

static void test_position_rejects_malformed_objects(void) {
    const char *invalid[] = {"null", "[]", "1", "{}", "{\"line\":0}",
                            "{\"character\":0}",
                            "{\"line\":\"0\",\"character\":0}",
                            "{\"line\":0,\"character\":false}"};
    SZrLspPosition position = {0};
    TZrSize index;

    TEST_ASSERT_FALSE(parse_position(NULL, &position));
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index++) {
        cJSON *json = cJSON_Parse(invalid[index]);
        int result = parse_position(json, &position);
        cJSON_Delete(json);
        TEST_ASSERT_FALSE_MESSAGE(result, invalid[index]);
    }
    TEST_ASSERT_FALSE(parse_position(NULL, NULL));
}

static void expect_range(const char *text, TZrBool valid) {
    cJSON *json = cJSON_Parse(text);
    SZrLspRange range = {0};
    int result;

    TEST_ASSERT_NOT_NULL(json);
    result = parse_range(json, &range);
    cJSON_Delete(json);
    TEST_ASSERT_EQUAL_MESSAGE(valid, result, text);
}

static void test_range_accepts_ordered_endpoints(void) {
    expect_range("{\"start\":{\"line\":0,\"character\":0},"
                 "\"end\":{\"line\":0,\"character\":0}}", ZR_TRUE);
    expect_range("{\"start\":{\"line\":0,\"character\":0},"
                 "\"end\":{\"line\":0,\"character\":1}}", ZR_TRUE);
    expect_range("{\"start\":{\"line\":0,\"character\":2147483647},"
                 "\"end\":{\"line\":1,\"character\":0}}", ZR_TRUE);
}

static void test_range_rejects_reversed_or_invalid_endpoints(void) {
    expect_range("{\"start\":{\"line\":1,\"character\":0},"
                 "\"end\":{\"line\":0,\"character\":2147483647}}", ZR_FALSE);
    expect_range("{\"start\":{\"line\":0,\"character\":1},"
                 "\"end\":{\"line\":0,\"character\":0}}", ZR_FALSE);
    expect_range("{\"start\":{\"line\":-1,\"character\":0},"
                 "\"end\":{\"line\":0,\"character\":0}}", ZR_FALSE);
    expect_range("{\"start\":{\"line\":0,\"character\":0},"
                 "\"end\":{\"line\":0,\"character\":0.5}}", ZR_FALSE);
    expect_range("{\"start\":{\"line\":0,\"character\":0}}", ZR_FALSE);
    expect_range("{\"end\":{\"line\":0,\"character\":0}}", ZR_FALSE);
    expect_range("{}", ZR_FALSE);
    expect_range("[]", ZR_FALSE);
    expect_range("null", ZR_FALSE);
    TEST_ASSERT_FALSE(parse_range(NULL, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_size_rejects_exclusive_upper_bound);
    RUN_TEST(test_size_accepts_representable_integers);
    RUN_TEST(test_size_rejects_invalid_numbers);
    RUN_TEST(test_size_rejects_non_numbers_and_null_output);
    RUN_TEST(test_position_accepts_integer_boundaries);
    RUN_TEST(test_position_rejects_invalid_components);
    RUN_TEST(test_position_rejects_malformed_objects);
    RUN_TEST(test_range_accepts_ordered_endpoints);
    RUN_TEST(test_range_rejects_reversed_or_invalid_endpoints);
    return UNITY_END();
}
