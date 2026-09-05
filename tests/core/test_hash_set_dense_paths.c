#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/hash_set.h"

typedef struct SZrHashPairTestAllocation {
    TZrPtr pointer;
    TZrSize size;
} SZrHashPairTestAllocation;

static SZrState *g_removalState;
static FZrAllocator g_removalAllocator;
static SZrHashPairTestAllocation g_pairAllocations[8];
static TZrUInt32 g_pairAllocationCount;
static TZrUInt32 g_pairFreeCount;
static TZrUInt32 g_invalidPairFreeCount;

static TZrPtr hash_pair_test_allocator(TZrPtr userData, TZrPtr pointer,
                                      TZrSize originalSize, TZrSize newSize,
                                      TZrInt64 flag) {
    TZrPtr result;
    if (flag == ZR_MEMORY_NATIVE_TYPE_HASH_PAIR && newSize == 0u) {
        for (TZrUInt32 index = 0u; index < g_pairAllocationCount; ++index) {
            if (g_pairAllocations[index].pointer == pointer &&
                g_pairAllocations[index].size == originalSize) {
                g_pairAllocations[index].pointer = ZR_NULL;
                ++g_pairFreeCount;
                return g_removalAllocator(userData, pointer, originalSize, newSize, flag);
            }
        }
        ++g_invalidPairFreeCount;
        return ZR_NULL;
    }
    result = g_removalAllocator(userData, pointer, originalSize, newSize, flag);
    if (flag == ZR_MEMORY_NATIVE_TYPE_HASH_PAIR && result != ZR_NULL) {
        if (g_pairAllocationCount < ZR_ARRAY_COUNT(g_pairAllocations)) {
            g_pairAllocations[g_pairAllocationCount].pointer = result;
            g_pairAllocations[g_pairAllocationCount].size = newSize;
            ++g_pairAllocationCount;
        } else {
            ++g_invalidPairFreeCount;
        }
    }
    return result;
}

void setUp(void) {}

void tearDown(void) {
    if (g_removalState != ZR_NULL) {
        g_removalState->global->allocator = g_removalAllocator;
        ZrTests_Runtime_State_Destroy(g_removalState);
        g_removalState = ZR_NULL;
    }
}

static void assert_hash_set_removal_allocation_contract(TZrUInt32 standaloneCount) {
    SZrHashSet set;
    const TZrInt64 keys[] = {0, 8, 16};
    const TZrUInt32 removalOrder[] = {1u, 0u, 2u};
    TZrUInt32 standaloneFrees = 0u;

    g_removalState = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_removalState);
    g_removalAllocator = g_removalState->global->allocator;
    memset(g_pairAllocations, 0, sizeof(g_pairAllocations));
    g_pairAllocationCount = 0u;
    g_pairFreeCount = 0u;
    g_invalidPairFreeCount = 0u;
    g_removalState->global->allocator = hash_pair_test_allocator;
    ZrCore_HashSet_Construct(&set);
    ZrCore_HashSet_Init(g_removalState, &set, 3u);
    TEST_ASSERT_TRUE(set.isValid);

    for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(keys); ++index) {
        SZrTypeValue key;
        SZrHashKeyValuePair *pair;
        ZrCore_Value_InitAsInt(g_removalState, &key, keys[index]);
        if (index < standaloneCount) {
            pair = ZrCore_HashSet_Add(g_removalState, &set, &key);
        } else {
            TZrSize bucket = ZR_HASH_MOD(ZrCore_Value_GetHash(g_removalState, &key), set.capacity);
            TEST_ASSERT_TRUE(ZrCore_HashSet_EnsurePairPoolForElementCount(
                    g_removalState, &set, set.pairPoolUsed + 1u));
            pair = ZrCore_HashSet_TakeReservedPair(&set);
            TEST_ASSERT_NOT_NULL(pair);
            pair->key = key;
            ZrCore_Value_ResetAsNull(&pair->value);
            pair->next = set.buckets[bucket];
            set.buckets[bucket] = pair;
            ++set.elementCount;
        }
        TEST_ASSERT_NOT_NULL(pair);
    }

    for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(removalOrder); ++index) {
        SZrTypeValue key;
        SZrTypeValue removed;
        ZrCore_Value_InitAsInt(g_removalState, &key, keys[removalOrder[index]]);
        removed = ZrCore_HashSet_Remove(g_removalState, &set, &key);
        TEST_ASSERT_EQUAL_INT64(keys[removalOrder[index]], removed.value.nativeObject.nativeInt64);
        TEST_ASSERT_NULL(ZrCore_HashSet_Find(g_removalState, &set, &key));
        TEST_ASSERT_EQUAL_UINT64(ZR_ARRAY_COUNT(keys) - index - 1u, set.elementCount);
        if (removalOrder[index] < standaloneCount) {
            ++standaloneFrees;
        }
        TEST_ASSERT_EQUAL_UINT32(standaloneFrees, g_pairFreeCount);
        removed = ZrCore_HashSet_Remove(g_removalState, &set, &key);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(removed.type));
    }
    ZrCore_HashSet_Deconstruct(g_removalState, &set);
    TEST_ASSERT_EQUAL_UINT32(0u, g_invalidPairFreeCount);
    TEST_ASSERT_EQUAL_UINT32(g_pairAllocationCount, g_pairFreeCount);
}

static void test_hash_set_remove_keeps_pooled_pairs_until_deconstruction(void) {
    assert_hash_set_removal_allocation_contract(0u);
}

static void test_hash_set_remove_frees_standalone_pairs_once(void) {
    assert_hash_set_removal_allocation_contract(3u);
}

static void test_hash_set_remove_handles_mixed_pool_and_standalone_pairs(void) {
    assert_hash_set_removal_allocation_contract(1u);
}

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
    RUN_TEST(test_hash_set_remove_keeps_pooled_pairs_until_deconstruction);
    RUN_TEST(test_hash_set_remove_frees_standalone_pairs_once);
    RUN_TEST(test_hash_set_remove_handles_mixed_pool_and_standalone_pairs);

    return UNITY_END();
}
