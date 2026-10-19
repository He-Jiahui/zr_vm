#include <string.h>

#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_lib_network/network.h"

static void test_debug_token_generation_returns_distinct_256_bit_hex_tokens(void) {
    TZrChar first[65];
    TZrChar second[65];
    TZrSize index;

    TEST_ASSERT_TRUE(ZrNetwork_GenerateDebugToken(first, sizeof(first)));
    TEST_ASSERT_TRUE(ZrNetwork_GenerateDebugToken(second, sizeof(second)));
    TEST_ASSERT_EQUAL_UINT32(64u, (unsigned)strlen(first));
    TEST_ASSERT_EQUAL_UINT32(64u, (unsigned)strlen(second));
    TEST_ASSERT_TRUE(strcmp(first, second) != 0);
    TEST_ASSERT_TRUE(ZrNetwork_TokenMatches(first, first));
    TEST_ASSERT_FALSE(ZrNetwork_TokenMatches(first, second));
    TEST_ASSERT_FALSE(ZrNetwork_TokenMatches(first, ZR_NULL));
    for (index = 0; index < 64u; index++) {
        TEST_ASSERT_TRUE((first[index] >= '0' && first[index] <= '9') ||
                         (first[index] >= 'a' && first[index] <= 'f'));
        TEST_ASSERT_TRUE((second[index] >= '0' && second[index] <= '9') ||
                         (second[index] >= 'a' && second[index] <= 'f'));
    }
}

static void test_debug_token_generation_rejects_small_buffers(void) {
    TZrChar buffer[64];

    TEST_ASSERT_FALSE(ZrNetwork_GenerateDebugToken(buffer, sizeof(buffer)));
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_token_generation_returns_distinct_256_bit_hex_tokens);
    RUN_TEST(test_debug_token_generation_rejects_small_buffers);
    return UNITY_END();
}
