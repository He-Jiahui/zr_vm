#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/hash_set.h"

void setUp(void) {}

void tearDown(void) {}

static void test_hash_set_dense_growth_uses_full_bucket_capacity_as_append_threshold(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrHashSet set;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_HashSet_Construct(&set);
    ZrCore_HashSet_Init(state, &set, 3);
    TEST_ASSERT_TRUE(set.isValid);
    TEST_ASSERT_EQUAL_UINT64(8u, (unsigned long long)set.capacity);
    TEST_ASSERT_TRUE(ZrCore_HashSet_GrowDenseSequentialIntKeys(state, &set, 16));
    TEST_ASSERT_EQUAL_UINT64(16u, (unsigned long long)set.capacity);
    TEST_ASSERT_EQUAL_UINT64(
            16u,
            (unsigned long long)set.resizeThreshold);

    ZrCore_HashSet_Deconstruct(state, &set);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_hash_set_dense_growth_uses_full_bucket_capacity_as_append_threshold);

    return UNITY_END();
}
