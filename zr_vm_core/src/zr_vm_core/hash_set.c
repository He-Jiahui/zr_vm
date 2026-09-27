//
// Created by HeJiahui on 2025/6/24.
//
#include "zr_vm_core/hash_set.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(ZR_DEBUG)
static TZrBool hash_set_trace_enabled(void);
static void hash_set_trace(const TZrChar *format, ...);
#else
/* 非调试构建移除跟踪调用及其参数求值。 */
#define hash_set_trace(...) ((void)0)
#endif

/* 池块按编译器的尾数组布局分配，释放时必须复算相同大小。 */
static TZrSize zr_hash_pair_pool_block_bytes(TZrSize capacity) {
    /* TODO: extraPairs 乘法与头部加法未检查溢出；EnsurePairPool 接受任意 elementCount。
     * 下一步用边界计数及拒绝超大分配的分配器核对返回容量和实际分配字节数。 */
    TZrSize extraPairs = capacity > ZR_HASH_PAIR_POOL_INLINE_COUNT ? capacity - ZR_HASH_PAIR_POOL_INLINE_COUNT : 0;
    return sizeof(SZrHashPairPoolBlock) + extraPairs * sizeof(SZrHashKeyValuePair);
}

/* 扩容后只清零新桶；原桶链随后由 Rehash 重排，或沿直索引路径保留。 */
static ZR_FORCE_INLINE void zr_hash_set_zero_bucket_slots(SZrHashKeyValuePair **bucketSlots, TZrSize slotCount) {
    ZR_ASSERT(bucketSlots != ZR_NULL || slotCount == 0);
    if (slotCount > 0) {
        ZrCore_Memory_RawSet(bucketSlots, 0, slotCount * sizeof(*bucketSlots));
    }
}

/* 整块释放已预留 pair，池内删除过的槽位不单独释放。 */
static void zr_hash_pair_pool_release(SZrGlobalState *global, SZrHashSet *set) {
    SZrHashPairPoolBlock *block;

    if (global == ZR_NULL || set == ZR_NULL) {
        return;
    }

    block = set->pairPoolHead;
    while (block != ZR_NULL) {
        SZrHashPairPoolBlock *next = block->next;
        ZrCore_Memory_RawFreeWithType(global,
                                      block,
                                      zr_hash_pair_pool_block_bytes(block->capacity),
                                      ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
        block = next;
    }

    set->pairPoolHead = ZR_NULL;
    set->pairPoolActive = ZR_NULL;
    set->pairPoolTail = ZR_NULL;
    set->pairPoolCapacity = 0;
    set->pairPoolUsed = 0;
}

/* 先沿桶链释放独立节点，再整体释放池和桶；set 可在成功 Init 后重复使用。 */
void ZrCore_HashSet_Deconstruct(struct SZrState *state, SZrHashSet *set) {
    SZrGlobalState *global = state->global;
    const TZrSize elementSize = sizeof(TZrPtr);
    TZrSize oldCapacity = set->capacity;
    TZrSize oldBucketCount = oldCapacity * elementSize;
    SZrHashKeyValuePair **oldBuckets = set->buckets;

    /* TODO: 节点 key/value 未逐个执行 Value 的所有权释放；Object_SetValue 会复制 value，
     * Object_Deconstruct 会走此处。需用含 owned value 的节点验证 GC/ownership 清理职责。 */
    if (oldBuckets != ZR_NULL) {
        for (TZrSize bucketIndex = 0; bucketIndex < oldCapacity; bucketIndex++) {
            SZrHashKeyValuePair *pair = oldBuckets[bucketIndex];

            while (pair != ZR_NULL) {
                SZrHashKeyValuePair *next = pair->next;
                if (!zr_hash_pair_pool_contains(set, pair)) {
                    ZrCore_Memory_RawFreeWithType(global,
                                                  pair,
                                                  sizeof(SZrHashKeyValuePair),
                                                  ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                }
                pair = next;
            }
        }
    }
    zr_hash_pair_pool_release(global, set);
    if (oldBuckets != ZR_NULL) {
        ZrCore_Memory_Allocate(global, oldBuckets, oldBucketCount, 0, ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET);
        set->buckets = ZR_NULL;
    }
    set->bucketSize = 0;
    set->elementCount = 0;
    set->capacity = 0;
    set->resizeThreshold = 0;
    set->isValid = ZR_FALSE;
}

/* 增加新的原生池块，不搬迁旧 pair；调用方须保证 GC 分配期间 set 地址稳定。 */
TZrBool ZrCore_HashSet_EnsurePairPoolForElementCount(SZrState *state, SZrHashSet *set, TZrSize elementCount) {
    TZrSize additionalCapacity;
    TZrSize blockBytes;
    SZrHashPairPoolBlock *block;

    /* TODO: Construct 后直接预留一个槽会返回真，但 TakeReservedPair 仍为空；
     * 该调用违反公开的已 Init 前置条件，当前生产调用方会先校验有效性。
     * 需确认接口是否应显式拒绝无效集合，再补对应契约测试。 */
    if (state == ZR_NULL || set == ZR_NULL || !set->isValid || elementCount <= set->pairPoolCapacity) {
        return state != ZR_NULL && set != ZR_NULL;
    }

    additionalCapacity = elementCount - set->pairPoolCapacity;
    blockBytes = zr_hash_pair_pool_block_bytes(additionalCapacity);
    /* TODO: GcMalloc 的失败重试会执行 GcFull；IR 物化因此将 set 暂存于原生内存，
     * 但数组路径传入对象内 nodeMap 地址。需用强制 GC 搬迁测试核对该调用方是否持有稳定地址。 */
    block = ZR_CAST(SZrHashPairPoolBlock *,
                    ZrCore_Memory_GcMalloc(state, ZR_MEMORY_NATIVE_TYPE_HASH_PAIR, blockBytes));
    if (block == ZR_NULL) {
        return ZR_FALSE;
    }

    block->next = ZR_NULL;
    block->capacity = additionalCapacity;
    block->used = 0;

    if (set->pairPoolHead == ZR_NULL) {
        set->pairPoolHead = block;
        set->pairPoolActive = block;
        set->pairPoolTail = block;
    } else {
        ZR_ASSERT(set->pairPoolTail != ZR_NULL);
        set->pairPoolTail->next = block;
        set->pairPoolTail = block;
        if (set->pairPoolActive == ZR_NULL || set->pairPoolActive->used >= set->pairPoolActive->capacity) {
            set->pairPoolActive = block;
        }
    }
    set->pairPoolCapacity += additionalCapacity;
    return ZR_TRUE;
}

/* realloc 成功后才替换桶指针，再逐链用新容量重排；失败依赖分配器保留旧桶。 */
TZrBool ZrCore_HashSet_Rehash(SZrState *state, SZrHashSet *set, TZrSize newCapacity) {
    ZR_ASSERT(set != NULL && newCapacity > set->capacity);
    const TZrSize elementSize = sizeof(TZrPtr);
    SZrGlobalState *global;
    TZrSize oldCapacity = set->capacity;
    TZrSize oldBucketCount = oldCapacity * elementSize;
    TZrSize newBucketCount = newCapacity * elementSize;
    SZrHashKeyValuePair **oldBuckets = set->buckets;
    SZrHashKeyValuePair **newBuckets;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        hash_set_trace("rehash reject state=%p global=%p set=%p newCapacity=%llu",
                       (void *)state,
                       state != ZR_NULL ? (void *)state->global : ZR_NULL,
                       (void *)set,
                       (unsigned long long)newCapacity);
        return ZR_FALSE;
    }

    global = state->global;
    hash_set_trace("rehash enter set=%p oldCapacity=%llu oldBuckets=%p newCapacity=%llu",
                   (void *)set,
                   (unsigned long long)oldCapacity,
                   (void *)oldBuckets,
                   (unsigned long long)newCapacity);
    newBuckets = ZR_CAST_HASH_KEY_VALUE_PAIR_PTR(
            ZrCore_Memory_Allocate(global,
                                   oldBuckets,
                                   oldBucketCount,
                                   newBucketCount,
                                   ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET));
    if (newBuckets == ZR_NULL) {
        hash_set_trace("rehash allocate failed set=%p oldBucketBytes=%llu newBucketBytes=%llu",
                       (void *)set,
                       (unsigned long long)oldBucketCount,
                       (unsigned long long)newBucketCount);
        return ZR_FALSE;
    }
    oldBuckets = ZR_NULL;
    set->buckets = newBuckets;
    set->capacity = newCapacity;
    set->bucketSize = newBucketCount;
    set->resizeThreshold = newCapacity * ZR_HASH_SET_MAX_LOAD_NUMERATOR / ZR_HASH_SET_MAX_LOAD_DENOMINATOR;
    zr_hash_set_zero_bucket_slots(set->buckets + oldCapacity, newCapacity - oldCapacity);
    for (TZrSize i = 0; i < oldCapacity; i++) {
        SZrHashKeyValuePair *objectPtr = newBuckets[i];
        newBuckets[i] = ZR_NULL;
        while (objectPtr != ZR_NULL) {
            SZrHashKeyValuePair *next = objectPtr->next;
            TZrUInt64 hash;
            TZrSize index;

            if (ZR_VALUE_IS_TYPE_SIGNED_INT(objectPtr->key.type) ||
                ZR_VALUE_IS_TYPE_UNSIGNED_INT(objectPtr->key.type) ||
                ZR_VALUE_IS_TYPE_FLOAT(objectPtr->key.type) ||
                ZR_VALUE_IS_TYPE_BOOL(objectPtr->key.type) ||
                ZR_VALUE_IS_TYPE_NULL(objectPtr->key.type)) {
                hash = objectPtr->key.value.nativeObject.nativeUInt64;
            } else if (objectPtr->key.isGarbageCollectable && objectPtr->key.value.object != ZR_NULL) {
                hash = objectPtr->key.value.object->hash;
            } else {
                hash = ZrCore_Value_GetHash(state, &objectPtr->key);
            }

            index = ZR_HASH_MOD(hash, newCapacity);
            objectPtr->next = newBuckets[index];
            newBuckets[index] = objectPtr;
            objectPtr = next;
        }
    }
    hash_set_trace("rehash exit set=%p capacity=%llu bucketSize=%llu threshold=%llu buckets=%p",
                   (void *)set,
                   (unsigned long long)set->capacity,
                   (unsigned long long)set->bucketSize,
                   (unsigned long long)set->resizeThreshold,
                   (void *)set->buckets);
    return ZR_TRUE;
}

/* 顺序整数键按桶下标直接寻址，扩容时原桶索引不变，因此只清空新增尾槽。 */
TZrBool ZrCore_HashSet_GrowDenseSequentialIntKeys(SZrState *state, SZrHashSet *set, TZrSize newCapacity) {
    const TZrSize elementSize = sizeof(TZrPtr);
    SZrGlobalState *global;
    TZrSize oldCapacity;
    TZrSize oldBucketCount;
    TZrSize newBucketCount;
    SZrHashKeyValuePair **oldBuckets;
    SZrHashKeyValuePair **newBuckets;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(newCapacity > (set != ZR_NULL ? set->capacity : 0));

    if (state == ZR_NULL || state->global == ZR_NULL || set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    oldCapacity = set->capacity;
    oldBucketCount = oldCapacity * elementSize;
    newBucketCount = newCapacity * elementSize;
    oldBuckets = set->buckets;

    newBuckets = ZR_CAST_HASH_KEY_VALUE_PAIR_PTR(
            ZrCore_Memory_Allocate(global,
                                   oldBuckets,
                                   oldBucketCount,
                                   newBucketCount,
                                   ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET));
    if (newBuckets == ZR_NULL) {
        return ZR_FALSE;
    }

    set->buckets = newBuckets;
    set->capacity = newCapacity;
    set->bucketSize = newBucketCount;
    /*
     * Dense sequential int-key tables index buckets directly by element index, so
     * the bucket array itself is the real append ceiling. Keeping the threshold at
     * full capacity avoids premature resize checks on this specialized path.
     */
    set->resizeThreshold = newCapacity;
    zr_hash_set_zero_bucket_slots(set->buckets + oldCapacity, newCapacity - oldCapacity);
    return ZR_TRUE;
}

#if defined(ZR_DEBUG)
/* 调试跟踪按环境变量延迟启用，生产构建由空宏完全剔除。 */
static TZrBool hash_set_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_CORE_BOOTSTRAP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

/* 只在显式启用时向 stderr 写启动阶段桶变化。 */
static void hash_set_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!hash_set_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-hash-set] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}
#endif
