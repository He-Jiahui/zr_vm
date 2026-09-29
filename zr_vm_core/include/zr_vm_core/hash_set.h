//
// Created by HeJiahui on 2025/6/24.
//

#ifndef ZR_VM_CORE_HASH_SET_H
#define ZR_VM_CORE_HASH_SET_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrGlobalState;

/* pair 池块在 MSVC 与柔性数组编译器下使用不同尾部布局；分配字节数须与布局一致。 */
#if defined(_MSC_VER)
    #define ZR_HASH_PAIR_POOL_INLINE_COUNT 1
    #define ZR_HASH_PAIR_POOL_FLEX_DECL 1
#else
    #define ZR_HASH_PAIR_POOL_INLINE_COUNT 0
    #define ZR_HASH_PAIR_POOL_FLEX_DECL
#endif

/** @brief 原生 pair 池块；used 只增不减，已发出的 pair 地址在集合析构前保持稳定。 */
struct SZrHashPairPoolBlock {
    struct SZrHashPairPoolBlock *next;
    TZrSize capacity;
    TZrSize used;
    SZrHashKeyValuePair pairs[ZR_HASH_PAIR_POOL_FLEX_DECL];
};

typedef struct SZrHashPairPoolBlock SZrHashPairPoolBlock;

/* 通用集合以 3/4 负载扩容；密集整数索引路径另以整张桶数组为上限。 */
#define ZR_HASH_SET_CAPACITY_GROWTH_FACTOR ((TZrSize)2)
#define ZR_HASH_SET_MAX_LOAD_NUMERATOR ((TZrSize)3)
#define ZR_HASH_SET_MAX_LOAD_DENOMINATOR ((TZrSize)4)

/** @brief 对二的幂桶容量取模，供查找、插入与重排共用。 */
/* MSVC 不支持在数组下标中使用逗号运算符，使用内联函数替代。 */
ZR_FORCE_INLINE TZrSize zr_hash_mod_inline(TZrUInt64 hash, TZrSize capacity) {
    ZR_ASSERT((capacity & (capacity - 1)) == 0);
    return (TZrSize)(hash & (capacity - 1));
}
/** @brief 统一桶索引入口；CAPACITY 必须为非零二的幂。 */
#define ZR_HASH_MOD(HASH, CAPACITY) zr_hash_mod_inline((HASH), (CAPACITY))

/** @brief 桶数组和 pair 节点的原生存储；节点内 key/value 的运行时所有权另由值语义管理。 */
/* isValid 表示桶已初始化；pairPoolCapacity/Used 统计所有池块，已使用槽位不会因删除而回收。 */
struct SZrHashSet {
    SZrHashKeyValuePair **buckets;
    TZrSize bucketSize;
    TZrSize elementCount;
    TZrSize capacity;
    TZrSize resizeThreshold;
    struct SZrHashPairPoolBlock *pairPoolHead;
    struct SZrHashPairPoolBlock *pairPoolActive;
    struct SZrHashPairPoolBlock *pairPoolTail;
    TZrSize pairPoolCapacity;
    TZrSize pairPoolUsed;
    TZrBool isValid;
};

typedef struct SZrHashSet SZrHashSet;

/** @brief 判定节点是否落在某个池块内，供删除与析构选择释放方式。 */
ZR_FORCE_INLINE TZrBool zr_hash_pair_pool_contains(const SZrHashSet *set,
                                                  const SZrHashKeyValuePair *pair) {
    const SZrHashPairPoolBlock *block;
    TZrUInt64 pairAddress;

    if (set == ZR_NULL || pair == ZR_NULL) {
        return ZR_FALSE;
    }

    pairAddress = (TZrUInt64)(TZrPtr)pair;
    for (block = set->pairPoolHead; block != ZR_NULL; block = block->next) {
        TZrUInt64 firstPairAddress;
        TZrUInt64 onePastLastPairAddress;

        if (block->capacity == 0u) {
            continue;
        }

        firstPairAddress = (TZrUInt64)(TZrPtr)&block->pairs[0];
        onePastLastPairAddress = firstPairAddress +
                                 block->capacity * sizeof(SZrHashKeyValuePair);
        if (pairAddress >= firstPairAddress && pairAddress < onePastLastPairAddress) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/** @brief 将未初始化集合置为无桶无池状态，随后可调用 Init。 */
ZR_FORCE_INLINE void ZrCore_HashSet_Construct(SZrHashSet *set) {
    set->buckets = ZR_NULL;
    set->bucketSize = 0;
    set->elementCount = 0;
    set->capacity = 0;
    set->resizeThreshold = 0;
    set->pairPoolHead = ZR_NULL;
    set->pairPoolActive = ZR_NULL;
    set->pairPoolTail = ZR_NULL;
    set->pairPoolCapacity = 0;
    set->pairPoolUsed = 0;
    set->isValid = ZR_FALSE;
}
/** @brief 释放桶、独立 pair 及整块 pair 池，并使集合失效。
 * @pre state、set 有效，state 的全局分配器仍存活。
 * @note 不逐个释放节点内的 key/value；调用者须核对值的所有权与 GC 生命周期。
 */
ZR_CORE_API void ZrCore_HashSet_Deconstruct(struct SZrState *state, SZrHashSet *set);

/** @brief 扩大普通哈希桶并重新链接现有节点。
 * @pre newCapacity 大于当前容量且为非零二的幂；分配器需保持 realloc 失败时原桶有效。
 * @return 分配成功时为真；失败时不修改桶链。
 */
ZR_CORE_API TZrBool ZrCore_HashSet_Rehash(struct SZrState *state, SZrHashSet *set, TZrSize newCapacity);
/** @brief 扩大顺序整数键的直索引桶；只用于桶下标在扩容后仍不变的集合。
 * @pre newCapacity 大于当前容量且为非零二的幂。
 */
ZR_CORE_API TZrBool ZrCore_HashSet_GrowDenseSequentialIntKeys(struct SZrState *state,
                                                              SZrHashSet *set,
                                                              TZrSize newCapacity);
/** @brief 为预期元素数补充原生 pair 池块；已有节点地址不随池扩张迁移。
 * @pre set 已成功 Init；调用者在可能触发 GC 的分配期间须保持 set 地址稳定。
 * @return 分配失败时为假，成功时容量至少覆盖 elementCount。
 */
ZR_CORE_API TZrBool ZrCore_HashSet_EnsurePairPoolForElementCount(struct SZrState *state,
                                                                 SZrHashSet *set,
                                                                 TZrSize elementCount);

/** @brief 从已预留池块取一个未初始化 pair；用尽时返回空指针。 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_TakeReservedPair(SZrHashSet *set) {
    struct SZrHashPairPoolBlock *block;

    if (set == ZR_NULL) {
        return ZR_NULL;
    }

    block = set->pairPoolActive;
    while (block != ZR_NULL && block->used >= block->capacity) {
        block = block->next;
    }
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    set->pairPoolActive = block;
    set->pairPoolUsed++;
    return &block->pairs[block->used++];
}

/** @brief 已由调用方保证存在空槽时取一个 pair，跳过正常容量失败分支。 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_TakeReservedPairAssumeAvailable(SZrHashSet *set) {
    struct SZrHashPairPoolBlock *block;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(set->pairPoolUsed < set->pairPoolCapacity);

    block = set->pairPoolActive;
    if (!(ZR_LIKELY(block != ZR_NULL && block->used < block->capacity))) {
        while (block != ZR_NULL && block->used >= block->capacity) {
            block = block->next;
        }

        ZR_ASSERT(block != ZR_NULL);
        if (block == ZR_NULL) {
            return ZR_NULL;
        }

        set->pairPoolActive = block;
    }

    set->pairPoolUsed++;
    return &block->pairs[block->used++];
}

/** @brief 从当前池块取至多 inOutCount 个连续槽，返回实际取得数量。 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_TakeReservedPairSpanAssumeAvailable(SZrHashSet *set,
                                                                                                TZrSize *inOutCount) {
    struct SZrHashPairPoolBlock *block;
    TZrSize available;
    TZrSize takeCount;
    SZrHashKeyValuePair *result;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(inOutCount != ZR_NULL);
    ZR_ASSERT(*inOutCount > 0);
    ZR_ASSERT(set->pairPoolUsed < set->pairPoolCapacity);

    block = set->pairPoolActive;
    if (!(ZR_LIKELY(block != ZR_NULL && block->used < block->capacity))) {
        while (block != ZR_NULL && block->used >= block->capacity) {
            block = block->next;
        }
        ZR_ASSERT(block != ZR_NULL);
        if (block == ZR_NULL) {
            return ZR_NULL;
        }
        set->pairPoolActive = block;
    }

    available = block->capacity - block->used;
    takeCount = *inOutCount < available ? *inOutCount : available;
    result = &block->pairs[block->used];
    block->used += takeCount;
    set->pairPoolUsed += takeCount;
    *inOutCount = takeCount;
    return result;
}

/** @brief 检查后续某块是否容纳完整的连续 pair 槽段，不消耗预留量。 */
static ZR_FORCE_INLINE TZrBool ZrCore_HashSet_HasReservedPairSpanExactAssumeAvailable(SZrHashSet *set,
                                                                                       TZrSize count) {
    struct SZrHashPairPoolBlock *block;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(count > 0);
    ZR_ASSERT(set->pairPoolUsed < set->pairPoolCapacity);

    block = set->pairPoolActive;
    while (block != ZR_NULL && block->used >= block->capacity) {
        block = block->next;
    }
    if (block == ZR_NULL) {
        return ZR_FALSE;
    }

    while (block != ZR_NULL) {
        if (block->used + count <= block->capacity) {
            return ZR_TRUE;
        }
        block = block->next;
    }

    return ZR_FALSE;
}

/** @brief 从后续某块取完整连续槽段；失败时不改变池计数。 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_TakeReservedPairSpanExactAssumeAvailable(SZrHashSet *set,
                                                                                                      TZrSize count) {
    struct SZrHashPairPoolBlock *block;
    SZrHashKeyValuePair *result;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(count > 0);
    ZR_ASSERT(set->pairPoolUsed < set->pairPoolCapacity);

    block = set->pairPoolActive;
    while (block != ZR_NULL && block->used >= block->capacity) {
        block = block->next;
    }
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    while (block != ZR_NULL) {
        if (block->used + count <= block->capacity) {
            result = &block->pairs[block->used];
            block->used += count;
            set->pairPoolUsed += count;
            if (block == set->pairPoolActive && block->used >= block->capacity) {
                set->pairPoolActive = block->next;
            }
            return result;
        }
        block = block->next;
    }

    return ZR_NULL;
}

/** @brief 优先在尾块查找完整槽段，避免批量数组写入跨块。 */
static ZR_FORCE_INLINE TZrBool ZrCore_HashSet_HasReservedPairSpanExactPreferTailAssumeAvailable(SZrHashSet *set,
                                                                                                 TZrSize count) {
    struct SZrHashPairPoolBlock *tail;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(count > 0);
    ZR_ASSERT(set->pairPoolUsed < set->pairPoolCapacity);

    tail = set->pairPoolTail;
    if (ZR_LIKELY(tail != ZR_NULL && tail->used + count <= tail->capacity)) {
        return ZR_TRUE;
    }

    return ZrCore_HashSet_HasReservedPairSpanExactAssumeAvailable(set, count);
}

/** @brief 优先领取尾块连续槽段；尾块不足时搜索其他未用槽段。 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_TakeReservedPairSpanExactPreferTailAssumeAvailable(
        SZrHashSet *set,
        TZrSize count) {
    struct SZrHashPairPoolBlock *tail;
    SZrHashKeyValuePair *result;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(count > 0);
    ZR_ASSERT(set->pairPoolUsed < set->pairPoolCapacity);

    tail = set->pairPoolTail;
    if (ZR_LIKELY(tail != ZR_NULL && tail->used + count <= tail->capacity)) {
        result = &tail->pairs[tail->used];
        tail->used += count;
        set->pairPoolUsed += count;
        if (tail == set->pairPoolActive && tail->used >= tail->capacity) {
            set->pairPoolActive = tail->next;
        }
        return result;
    }

    return ZrCore_HashSet_TakeReservedPairSpanExactAssumeAvailable(set, count);
}

/** @brief 计算满足普通 3/4 负载阈值的最小二的幂容量。 */
ZR_FORCE_INLINE TZrSize ZrCore_HashSet_MinCapacityForElementCount(TZrSize elementCount) {
    TZrSize capacity = 1;

    /* TODO: 大于可表示负载上限时，capacity 倍增及乘法会回绕，循环可能不终止。
     * EnsureCapacity 接受外部计数；下一步补最大可表示容量边界测试。 */
    while (capacity * ZR_HASH_SET_MAX_LOAD_NUMERATOR / ZR_HASH_SET_MAX_LOAD_DENOMINATOR < elementCount) {
        capacity *= ZR_HASH_SET_CAPACITY_GROWTH_FACTOR;
    }

    return capacity;
}

/** @brief 将所需桶数向上取整为二的幂；调用者须保证结果可表示。 */
ZR_FORCE_INLINE TZrSize ZrCore_HashSet_RoundUpPowerOfTwoCapacity(TZrSize minimumCapacity) {
    TZrUInt32 shift;

    if (minimumCapacity <= 1) {
        return 1;
    }
    if (minimumCapacity != 0 && (minimumCapacity & (minimumCapacity - 1)) == 0) {
        return minimumCapacity;
    }

    /* TODO: 超过最高可表示二的幂时结果会回绕为零；密集桶调用方目前未拒绝该值。
     * 下一步从 EnsureDenseSequentialIntKeyCapacity 用极大 elementCount 核对失败语义。 */
    minimumCapacity--;
    for (shift = 1; shift < (TZrUInt32)(sizeof(TZrSize) * 8u); shift <<= 1u) {
        minimumCapacity |= minimumCapacity >> shift;
    }
    return minimumCapacity + 1;
}

/** @brief 直索引整数键需要每个元素各有一个桶槽。 */
ZR_FORCE_INLINE TZrSize ZrCore_HashSet_MinDenseSequentialIntKeyCapacity(TZrSize elementCount) {
    return ZrCore_HashSet_RoundUpPowerOfTwoCapacity(elementCount);
}

/** @brief 校验位掩码桶索引所需的非零二的幂容量。 */
ZR_FORCE_INLINE TZrBool ZrCore_HashSet_IsPowerOfTwoCapacity(TZrSize capacity) {
    return capacity != 0 && (capacity & (capacity - 1)) == 0;
}

/** @brief 为顺序整数键准备直索引桶；已足够时仍将阈值修正为整桶容量。 */
ZR_FORCE_INLINE TZrBool ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(struct SZrState *state,
                                                                            SZrHashSet *set,
                                                                            TZrSize elementCount) {
    TZrSize requiredCapacity;

    if (state == ZR_NULL || set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0) {
        return ZR_FALSE;
    }

    if (elementCount <= set->capacity) {
        if (set->resizeThreshold < set->capacity) {
            set->resizeThreshold = set->capacity;
        }
        return ZR_TRUE;
    }

    requiredCapacity = ZrCore_HashSet_MinDenseSequentialIntKeyCapacity(elementCount);
    if (requiredCapacity > set->capacity) {
        return ZrCore_HashSet_GrowDenseSequentialIntKeys(state, set, requiredCapacity);
    }

    /*
     * Dense sequential int-key tables use direct bucket indexing, so once a
     * table is on this path the current bucket array is the true append limit.
     * Preserve that invariant even when the table was originally initialized
     * through the generic hash-set constructor with a 0.75 load threshold.
     */
    if (set->resizeThreshold < set->capacity) {
        set->resizeThreshold = set->capacity;
    }
    return ZR_TRUE;
}

/** @brief 按已算好的二的幂容量扩充直索引桶，供批量路径复用。 */
ZR_FORCE_INLINE TZrBool ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacityExact(struct SZrState *state,
                                                                                 SZrHashSet *set,
                                                                                 TZrSize requiredCapacity) {
    if (state == ZR_NULL || set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0 ||
        requiredCapacity == 0) {
        return ZR_FALSE;
    }

    ZR_ASSERT(ZrCore_HashSet_IsPowerOfTwoCapacity(requiredCapacity));
    if (requiredCapacity > set->capacity) {
        return ZrCore_HashSet_GrowDenseSequentialIntKeys(state, set, requiredCapacity);
    }

    if (set->resizeThreshold < set->capacity) {
        set->resizeThreshold = set->capacity;
    }
    return ZR_TRUE;
}

/** @brief 先确保直索引桶，再预留等量 pair 槽；调用者仍须检查返回值。 */
ZR_FORCE_INLINE TZrBool ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacityAndPairPoolExact(struct SZrState *state,
                                                                                             SZrHashSet *set,
                                                                                             TZrSize requiredCapacity) {
    if (!ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacityExact(state, set, requiredCapacity)) {
        return ZR_FALSE;
    }

    if (set->pairPoolCapacity < requiredCapacity) {
        return ZrCore_HashSet_EnsurePairPoolForElementCount(state, set, requiredCapacity);
    }

    return ZR_TRUE;
}

/** @brief 为普通哈希负载准备桶；重排失败时原集合可继续使用。 */
ZR_FORCE_INLINE TZrBool ZrCore_HashSet_EnsureCapacityForElementCount(struct SZrState *state,
                                                                     SZrHashSet *set,
                                                                     TZrSize elementCount) {
    TZrSize requiredCapacity;

    if (state == ZR_NULL || set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0) {
        return ZR_FALSE;
    }

    requiredCapacity = ZrCore_HashSet_MinCapacityForElementCount(elementCount);
    if (requiredCapacity <= set->capacity) {
        return ZR_TRUE;
    }

    return ZrCore_HashSet_Rehash(state, set, requiredCapacity);
}

/** @brief 在没有有效 state 时构造不依赖全局单例的空值返回项。 */
ZR_FORCE_INLINE SZrTypeValue ZrCore_HashSet_MakeNullValueFallback(void) {
    SZrTypeValue value;
    value.type = ZR_VALUE_TYPE_NULL;
    value.value.nativeObject.nativeUInt64 = 0;
    value.isGarbageCollectable = ZR_FALSE;
    value.isNative = ZR_TRUE;
    value.ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value.ownershipControl = ZR_NULL;
    value.ownershipWeakRef = ZR_NULL;
    return value;
}

/** @brief 分配初始桶并设置 isValid；失败时集合保留可析构的空状态。
 * @pre capacityLog2 在 TZrSize 位宽以内且非零，set 未持有旧资源。
 */
ZR_FORCE_INLINE void ZrCore_HashSet_Init(struct SZrState *state, SZrHashSet *set, TZrSize capacityLog2) {
    ZR_ASSERT(set != NULL && capacityLog2 != 0);
    const TZrSize capacity = (TZrSize) 1 << capacityLog2;
    set->buckets = ZR_NULL;
    set->elementCount = 0;
    set->capacity = 0;
    set->resizeThreshold = 0;
    set->bucketSize = 0;
    set->pairPoolHead = ZR_NULL;
    set->pairPoolActive = ZR_NULL;
    set->pairPoolTail = ZR_NULL;
    set->pairPoolCapacity = 0;
    set->pairPoolUsed = 0;
    set->isValid = ZrCore_HashSet_Rehash(state, set, capacity);
}

/** @brief 借用指定哈希桶链首节点；调用者须保证集合已成功初始化且容量为非零二的幂。 */
ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_GetBucket(SZrHashSet *set, TZrUInt64 hash) {
    return set->buckets[ZR_HASH_MOD(hash, set->capacity)];
}
/** @brief 新建独立 pair 并复制 key；不检查重复键。
 * @return 新节点的借用指针；分配失败时返回空指针，已有元素仍可查找。
 * @pre 分配可能触发 GC；set 与 element 所在地址在调用期间须保持稳定。
 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_Add(struct SZrState *state, SZrHashSet *set,
                                                         const SZrTypeValue *element) {
    SZrHashKeyValuePair *object;
    SZrHashKeyValuePair *hashElement;
    TZrUInt64 hash;

    if (state == ZR_NULL || set == ZR_NULL || element == ZR_NULL || !set->isValid || set->buckets == ZR_NULL ||
        set->capacity == 0) {
        return ZR_NULL;
    }
    if (set->elementCount + 1 > set->resizeThreshold &&
        !ZrCore_HashSet_Rehash(state, set, set->capacity * ZR_HASH_SET_CAPACITY_GROWTH_FACTOR)) {
        return ZR_NULL;
    }

    hash = ZrCore_Value_GetHash(state, element);
    object = ZrCore_HashSet_GetBucket(set, hash);
    /* TODO: Module_AddPubExport/ProExport 直接传 module 内 proNodeMap；GcMalloc 可触发 GcFull。
     * Object_SetValue 会 pin 宿主，但模块导出路径的地址稳定性需强制 GC 测试核对。 */
    hashElement = (SZrHashKeyValuePair *)ZrCore_Memory_GcMalloc(state,
                                                                ZR_MEMORY_NATIVE_TYPE_HASH_PAIR,
                                                                sizeof(SZrHashKeyValuePair));
    if (hashElement == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_ResetAsNull(&hashElement->key);
    ZrCore_Value_ResetAsNull(&hashElement->value);
    ZrCore_Value_Copy(state, &hashElement->key, element);
    // object->next = hashElement;
    hashElement->next = object;
    set->buckets[ZR_HASH_MOD(hash, set->capacity)] = hashElement;
    set->elementCount++;
    return hashElement;
}
/** @brief 以原始 GC 对象为 key 新建独立 pair；value 初始为空，按需由调用方填写。
 * @pre 分配可能触发 GC；set 与 element 所在地址在调用期间须保持稳定。
 */
ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_AddRawObject(struct SZrState *state, SZrHashSet *set,
                                                           SZrRawObject *element) {
    SZrHashKeyValuePair *object;
    SZrHashKeyValuePair *hashElement;
    TZrUInt64 hash;

    if (state == ZR_NULL || set == ZR_NULL || element == ZR_NULL || !set->isValid || set->buckets == ZR_NULL ||
        set->capacity == 0) {
        return ZR_NULL;
    }
    if (set->elementCount + 1 > set->resizeThreshold &&
        !ZrCore_HashSet_Rehash(state, set, set->capacity * ZR_HASH_SET_CAPACITY_GROWTH_FACTOR)) {
        return ZR_NULL;
    }

    hash = element->hash;
    object = ZrCore_HashSet_GetBucket(set, hash);
    /* 短串调用方先将 newString 链入根表；GC 的已接受压缩路径保持驻留短串地址稳定。 */
    hashElement = (SZrHashKeyValuePair *)ZrCore_Memory_GcMalloc(state,
                                                                ZR_MEMORY_NATIVE_TYPE_HASH_PAIR,
                                                                sizeof(SZrHashKeyValuePair));
    if (hashElement == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_ResetAsNull(&hashElement->key);
    ZrCore_Value_ResetAsNull(&hashElement->value);
    ZrCore_Value_InitAsRawObject(state, &hashElement->key, element);
    // object->next = hashElement;
    hashElement->next = object;
    set->buckets[ZR_HASH_MOD(hash, set->capacity)] = hashElement;
    set->elementCount++;
    return hashElement;
}

/** @brief 从桶链摘除首个相等键，独立 pair 即时释放，池内槽位留至析构。
 * @return 找到时按值返回节点原 key；未找到或参数无效时返回空值。
 * @note 返回值是节点 key 槽位的浅拷贝；此处不 retain 或 release ownership。
 *       对带引用计数 ownership 的 key，调用方接收该槽位的现有引用，并须在用完后
 *       对返回值调用一次 ZrCore_Ownership_ReleaseValue。
 * @note 调用方不得再使用已删除节点指针；普通值及借用值仍遵循各自已有的 ownership 语义。
 */
ZR_FORCE_INLINE SZrTypeValue ZrCore_HashSet_Remove(struct SZrState *state, SZrHashSet *set, const SZrTypeValue *element) {
    if (state == ZR_NULL || set == ZR_NULL || element == ZR_NULL || !set->isValid || set->buckets == ZR_NULL ||
        set->capacity == 0) {
        return state != ZR_NULL && state->global != ZR_NULL
            ? state->global->nullValue
            : ZrCore_HashSet_MakeNullValueFallback();
    }
    TZrUInt64 hash = ZrCore_Value_GetHash(state, element);
    SZrHashKeyValuePair *object = ZrCore_HashSet_GetBucket(set, hash);
    SZrHashKeyValuePair *prev = ZR_NULL;
    while (object != ZR_NULL) {
        // same address or equal content(customized compare function)
        if (ZrCore_Value_CompareDirectly(state, &object->key, element)) {
            if (prev == ZR_NULL) {
                set->buckets[ZR_HASH_MOD(hash, set->capacity)] = object->next;
            } else {
                prev->next = object->next;
            }
            set->elementCount--;
            /* 按值浅拷贝 key 后释放节点，返回值的 owned 引用语义需另行核验。 */
            SZrTypeValue result = object->key;
            if (!zr_hash_pair_pool_contains(set, object)) {
                ZrCore_Memory_RawFreeWithType(state->global, object, sizeof(SZrHashKeyValuePair),
                                            ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
            }
            return result;
        }
        prev = object;
        object = object->next;
    }
    return state->global->nullValue;
}

/** @brief 查找首个相等键；返回借用节点，删除或集合析构后失效。 */
ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_Find(struct SZrState *state, SZrHashSet *set,
                                                   const SZrTypeValue *element) {
    if (state == ZR_NULL || set == ZR_NULL || element == ZR_NULL || !set->isValid || set->buckets == ZR_NULL ||
        set->capacity == 0) {
        return ZR_NULL;
    }
    TZrUInt64 hash = ZrCore_Value_GetHash(state, element);
    SZrHashKeyValuePair *object = ZrCore_HashSet_GetBucket(set, hash);
    while (object != ZR_NULL) {
        if (ZrCore_Value_CompareDirectly(state, &object->key, element)) {
            return object;
        }
        object = object->next;
    }
    return ZR_NULL;
}

#endif // ZR_VM_CORE_HASH_SET_H
