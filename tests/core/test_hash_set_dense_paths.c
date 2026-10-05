#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/string.h"

/* 记录 allocator 返回的整块基址和请求字节数；池内节点地址不能作为独立释放目标。 */
typedef struct SZrHashPairTestAllocation {
    /* 借用原分配地址；成功观察到释放后清空，记录本身不拥有该块。 */
    TZrPtr pointer;
    /* 必须与释放请求的 originalSize 配对；单节点和带头部池块大小不同。 */
    TZrSize size;
} SZrHashPairTestAllocation;

/* 自定义 allocator 只观察 HASH_PAIR，核对独立 pair 和池内 pair 的
 * 释放责任；其他 flag 仍委托测试运行时原 allocator。 */
static SZrState *g_removalState;
static FZrAllocator g_removalAllocator;
/* 每次 removal helper 重置这组有限记录；三个键最多产生三个 HASH_PAIR 块。 */
static SZrHashPairTestAllocation g_pairAllocations[8];
static TZrUInt32 g_pairAllocationCount;
static TZrUInt32 g_pairFreeCount;
static TZrUInt32 g_invalidPairFreeCount;
/* 静态 set 让 tearDown 在断言跳转后仍可取到资源；标志在 Construct 后、Init 前发布。 */
static SZrHashSet g_ownedKeySet;
/* owner 与 Remove 的返回槽分别持有引用；初始化标志使失败清理只访问已初始化的值。 */
static SZrTypeValue g_ownedKeyOwner;
static SZrTypeValue g_ownedKeyRemoved;
static TZrBool g_ownedKeySetConstructed;
static TZrBool g_ownedKeyValuesInitialized;

/* 挂在 global->allocator 的观察层保留原 userData 与预算 allocator；只拦截 HASH_PAIR。
 * 非法释放记账后不交给底层，避免池内地址或重复释放破坏宿主堆；不模拟分配失败。 */
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

/* HashSet_Deconstruct 只回收原生存储；断言失败留下节点时，先释放仍在桶链上的值引用。
 * 已被 Remove 摘除的 key 引用由返回槽负责，不能在这里再次释放。 */
static void release_hash_set_node_values(SZrState *state, SZrHashSet *set) {
    if (state == ZR_NULL || set == ZR_NULL || set->buckets == ZR_NULL) {
        return;
    }
    for (TZrSize bucketIndex = 0u; bucketIndex < set->capacity; ++bucketIndex) {
        SZrHashKeyValuePair *pair = set->buckets[bucketIndex];
        while (pair != ZR_NULL) {
            ZrCore_Ownership_ReleaseValue(state, &pair->key);
            ZrCore_Ownership_ReleaseValue(state, &pair->value);
            pair = pair->next;
        }
    }
}

/* removal 用例将 allocator 临时替换在 global 上，先恢复原指针再销毁 VM；
 * Unity 断言失败时也通过此入口收回测试状态。 */
void tearDown(void) {
    if (g_removalState != ZR_NULL) {
        g_removalState->global->allocator = g_removalAllocator;
        if (g_ownedKeySetConstructed) {
            release_hash_set_node_values(g_removalState, &g_ownedKeySet);
            ZrCore_HashSet_Deconstruct(g_removalState, &g_ownedKeySet);
            g_ownedKeySetConstructed = ZR_FALSE;
        }
        if (g_ownedKeyValuesInitialized) {
            ZrCore_Ownership_ReleaseValue(g_removalState, &g_ownedKeyRemoved);
            ZrCore_Ownership_ReleaseValue(g_removalState, &g_ownedKeyOwner);
            g_ownedKeyValuesInitialized = ZR_FALSE;
        }
        ZrTests_Runtime_State_Destroy(g_removalState);
        g_removalState = ZR_NULL;
    }
}

/* standaloneCount 只由三个场景传入 0/3/1，表示前几个键经 Add 分配独立节点。
 * 容量参数 3 是 log2，即八桶；0/8/16 同桶，顺序删除中间、尾、首节点。 */
static void assert_hash_set_removal_allocation_contract(TZrUInt32 standaloneCount) {
    SZrHashSet set;
    const TZrInt64 keys[] = {0, 8, 16};
    const TZrUInt32 removalOrder[] = {1u, 0u, 2u};
    TZrUInt32 standaloneFrees = 0u;
    /* TODO: 若此 helper 的断言跳转，局部 set 无法由 tearDown 回收；当前节点最终 OOM
     * 会进入 VM panic，尚未证明它返回 Unity 断言。需沿失败注入/异常入口核实后再定 BUG。 */
    g_removalState = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_removalState);
    g_removalAllocator = g_removalState->global->allocator;
    memset(g_pairAllocations, 0, sizeof(g_pairAllocations));
    g_pairAllocationCount = 0u;
    g_pairFreeCount = 0u;
    g_invalidPairFreeCount = 0u;
    /* 状态创建完成后才安装观察层，排除 VM 初始化的 HASH_PAIR；tearDown 先恢复分配器再销毁 VM。 */
    g_removalState->global->allocator = hash_pair_test_allocator;
    ZrCore_HashSet_Construct(&set);
    ZrCore_HashSet_Init(g_removalState, &set, 3u);
    TEST_ASSERT_TRUE(set.isValid);

    for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(keys); ++index) {
        SZrTypeValue key;
        SZrHashKeyValuePair *pair;
        ZrCore_Value_InitAsInt(g_removalState, &key, keys[index]);
        /* 直接 Add 产生独立 pair，池路径模拟保留容量供增长复用。 */
        if (index < standaloneCount) {
            pair = ZrCore_HashSet_Add(g_removalState, &set, &key);
        } else {
            /* 保留池槽后自行挂链以构造混合来源；这里只复制无 ownership 的整数 key，
             * 不把这种赋值当作 owned key 的 retain。pairPoolUsed 是只增的分配游标。 */
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

/* 创建普通 class 对象供 unique/shared 引用计数场景使用；prototype 属于 VM，
 * 返回对象尚未建立本测试的 owned 值，失败由调用方断言及 fixture 清理处理。 */
static SZrObject *create_owned_hash_set_key_object(SZrState *state) {
    SZrString *typeName = ZrCore_String_CreateFromNative(state, "HashSetOwnedKey");
    SZrObjectPrototype *prototype;
    SZrObject *object;

    if (typeName == ZR_NULL) {
        return ZR_NULL;
    }
    prototype = ZrCore_ObjectPrototype_New(
            state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }
    object = ZrCore_Object_New(state, prototype);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Object_Init(state, object);
    return object;
}

/* Add 复制 unique key 后让节点持有 shared 引用；Remove 转交这份引用而不新增或销毁它。
 * 依次释放原 owner 和返回槽，核对一次 retain、两次分别释放及返回槽清空。 */
static void test_hash_set_remove_transfers_owned_key_reference(void) {
    SZrObject *object;
    SZrRawObject *rawObject;
    SZrHashKeyValuePair *pair;

    TEST_ASSERT_NULL(g_removalState);
    g_removalState = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_removalState);
    g_removalAllocator = g_removalState->global->allocator;
    ZrCore_Value_ResetAsNull(&g_ownedKeyOwner);
    ZrCore_Value_ResetAsNull(&g_ownedKeyRemoved);
    g_ownedKeyValuesInitialized = ZR_TRUE;
    ZrCore_HashSet_Construct(&g_ownedKeySet);
    g_ownedKeySetConstructed = ZR_TRUE;
    ZrCore_HashSet_Init(g_removalState, &g_ownedKeySet, 3u);
    TEST_ASSERT_TRUE(g_ownedKeySet.isValid);

    object = create_owned_hash_set_key_object(g_removalState);
    TEST_ASSERT_NOT_NULL(object);
    rawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            g_removalState, &g_ownedKeyOwner, rawObject));
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_VALUE_KIND_UNIQUE, g_ownedKeyOwner.ownershipKind);
    TEST_ASSERT_EQUAL_UINT32(
            1u, ZrCore_Ownership_GetStrongRefCount(rawObject));

    pair = ZrCore_HashSet_Add(
            g_removalState, &g_ownedKeySet, &g_ownedKeyOwner);
    TEST_ASSERT_NOT_NULL(pair);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_VALUE_KIND_SHARED, pair->key.ownershipKind);
    TEST_ASSERT_EQUAL_PTR(
            g_ownedKeyOwner.ownershipControl, pair->key.ownershipControl);
    TEST_ASSERT_EQUAL_UINT32(
            2u, ZrCore_Ownership_GetStrongRefCount(rawObject));

    g_ownedKeyRemoved = ZrCore_HashSet_Remove(
            g_removalState, &g_ownedKeySet, &g_ownedKeyOwner);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_VALUE_KIND_SHARED, g_ownedKeyRemoved.ownershipKind);
    TEST_ASSERT_EQUAL_PTR(rawObject, g_ownedKeyRemoved.value.object);
    TEST_ASSERT_EQUAL_PTR(
            g_ownedKeyOwner.ownershipControl,
            g_ownedKeyRemoved.ownershipControl);
    TEST_ASSERT_EQUAL_UINT32(
            2u, ZrCore_Ownership_GetStrongRefCount(rawObject));
    TEST_ASSERT_EQUAL_UINT64(0u, g_ownedKeySet.elementCount);
    TEST_ASSERT_NULL(ZrCore_HashSet_Find(
            g_removalState, &g_ownedKeySet, &g_ownedKeyOwner));

    /* 节点已摘链，其 retained 引用现由 g_ownedKeyRemoved 持有；不能再从 set 释放。 */
    /* Removing transfers the pair's retained reference to the result. */
    ZrCore_Ownership_ReleaseValue(g_removalState, &g_ownedKeyOwner);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(g_ownedKeyOwner.type));
    TEST_ASSERT_EQUAL_UINT32(
            1u, ZrCore_Ownership_GetStrongRefCount(rawObject));
    TEST_ASSERT_EQUAL_PTR(rawObject, g_ownedKeyRemoved.value.object);
    ZrCore_Ownership_ReleaseValue(g_removalState, &g_ownedKeyRemoved);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(g_ownedKeyRemoved.type));
    TEST_ASSERT_NULL(g_ownedKeyRemoved.ownershipControl);
}

/* 空集合从八桶扩到十六桶，核对专用直索引路径的阈值等于完整桶容量。
 * 本场景没有插入 dense key，不证明元素保留、重新挂链或实际 append 行为。 */
static void test_hash_set_dense_growth_uses_full_bucket_capacity_as_append_threshold(void) {
    /* BUG: 本用例使用局部 state 而非 g_removalState；一旦断言失败，
     * tearDown 不会销毁该 VM，也不会 Deconstruct 局部 hash set。 */
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

/* CTest 的 hash_set_dense_paths 启动本 runner；五个场景经 Unity 分别执行生命周期钩子。 */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_hash_set_dense_growth_uses_full_bucket_capacity_as_append_threshold);
    RUN_TEST(test_hash_set_remove_keeps_pooled_pairs_until_deconstruction);
    RUN_TEST(test_hash_set_remove_frees_standalone_pairs_once);
    RUN_TEST(test_hash_set_remove_handles_mixed_pool_and_standalone_pairs);
    RUN_TEST(test_hash_set_remove_transfers_owned_key_reference);

    return UNITY_END();
}
