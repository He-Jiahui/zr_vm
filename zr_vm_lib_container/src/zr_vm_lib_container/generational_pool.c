#include "zr_vm_lib_container/generational_pool.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#define ZR_POOL_NO_SLOT ((TZrSize)SIZE_MAX)
#define ZR_POOL_DEFAULT_SLAB_CAPACITY ((TZrSize)256u)

typedef struct SZrPoolSlot {
    uint64_t generation;
    TZrSize nextFree;
    TZrSize readerCount;
    EZrPoolSlotState state;
    TZrBool writerActive;
    TZrBool initialized;
    TZrBool reused;
} SZrPoolSlot;

typedef struct SZrPoolSlab {
    void *allocation;
    unsigned char *storage;
    SZrPoolSlot *slots;
    TZrSize baseIndex;
} SZrPoolSlab;

struct SZrPool {
    SZrPoolTypeLayout layout;
    SZrPoolConfig config;
    SZrPoolSlab **slabs;
    TZrSize slabCount;
    TZrSize slabCapacity;
    TZrSize elementStride;
    TZrSize freeHead;
    uint64_t id;
    volatile long lockWord;
    TZrBool destroying;
    SZrPoolStats stats;
};

static uint64_t gNextPoolId = 1u;
static volatile long gPoolIdLockWord = 0L;

static void zr_pool_lock_word(volatile long *word) {
#if defined(_MSC_VER)
    while (_InterlockedExchange(word, 1L) != 0L) {
        while (*word != 0L) {
            _ReadWriteBarrier();
        }
    }
#else
    while (__sync_lock_test_and_set(word, 1L) != 0L) {
        while (*word != 0L) {
            __asm__ __volatile__("" ::: "memory");
        }
    }
#endif
}

static void zr_pool_unlock_word(volatile long *word) {
#if defined(_MSC_VER)
    _InterlockedExchange(word, 0L);
#else
    __sync_lock_release(word);
#endif
}

static void zr_pool_lock(const SZrPool *pool) {
    if (pool != ZR_NULL &&
        pool->config.concurrencyMode == ZR_POOL_CONCURRENCY_CONCURRENT) {
        zr_pool_lock_word((volatile long *)&pool->lockWord);
    }
}

static void zr_pool_unlock(const SZrPool *pool) {
    if (pool != ZR_NULL &&
        pool->config.concurrencyMode == ZR_POOL_CONCURRENCY_CONCURRENT) {
        zr_pool_unlock_word((volatile long *)&pool->lockWord);
    }
}

static TZrBool zr_pool_is_power_of_two(TZrSize value) {
    return (TZrBool)(value != 0u && (value & (value - 1u)) == 0u);
}

static TZrBool zr_pool_align_up(
        TZrSize value,
        TZrSize alignment,
        TZrSize *outValue) {
    TZrSize mask;

    if (outValue == ZR_NULL || !zr_pool_is_power_of_two(alignment)) {
        return ZR_FALSE;
    }
    mask = alignment - 1u;
    if (value > SIZE_MAX - mask) {
        return ZR_FALSE;
    }
    *outValue = (value + mask) & ~mask;
    return ZR_TRUE;
}

static void *zr_pool_aligned_allocate(
        TZrSize size,
        TZrSize alignment,
        void **outAllocation) {
    unsigned char *allocation;
    uintptr_t address;

    if (outAllocation == ZR_NULL || size == 0u ||
        !zr_pool_is_power_of_two(alignment) ||
        size > SIZE_MAX - (alignment - 1u)) {
        return ZR_NULL;
    }
    allocation = (unsigned char *)malloc(size + alignment - 1u);
    if (allocation == ZR_NULL) {
        return ZR_NULL;
    }
    address = ((uintptr_t)allocation + (uintptr_t)(alignment - 1u)) &
              ~((uintptr_t)alignment - 1u);
    *outAllocation = allocation;
    return (void *)address;
}

static SZrPoolSlot *zr_pool_find_slot(
        const SZrPool *pool,
        TZrSize slotIndex,
        SZrPoolSlab **outSlab) {
    TZrSize slabIndex;
    TZrSize localIndex;
    SZrPoolSlab *slab;

    if (outSlab != ZR_NULL) {
        *outSlab = ZR_NULL;
    }
    if (pool == ZR_NULL || pool->config.slabCapacity == 0u) {
        return ZR_NULL;
    }
    slabIndex = slotIndex / pool->config.slabCapacity;
    localIndex = slotIndex % pool->config.slabCapacity;
    if (slabIndex >= pool->slabCount) {
        return ZR_NULL;
    }
    slab = pool->slabs[slabIndex];
    if (slab == ZR_NULL) {
        return ZR_NULL;
    }
    if (outSlab != ZR_NULL) {
        *outSlab = slab;
    }
    return &slab->slots[localIndex];
}

static void *zr_pool_slot_value(
        const SZrPool *pool,
        const SZrPoolSlab *slab,
        TZrSize slotIndex) {
    TZrSize localIndex;

    if (pool == ZR_NULL || slab == ZR_NULL ||
        slotIndex < slab->baseIndex) {
        return ZR_NULL;
    }
    localIndex = slotIndex - slab->baseIndex;
    if (localIndex >= pool->config.slabCapacity ||
        localIndex > SIZE_MAX / pool->elementStride) {
        return ZR_NULL;
    }
    return slab->storage + localIndex * pool->elementStride;
}

static TZrBool zr_pool_grow_slab_directory(SZrPool *pool) {
    TZrSize newCapacity;
    SZrPoolSlab **newSlabs;

    if (pool->slabCount < pool->slabCapacity) {
        return ZR_TRUE;
    }
    newCapacity = pool->slabCapacity == 0u ? 4u : pool->slabCapacity * 2u;
    if (newCapacity < pool->slabCapacity ||
        newCapacity > SIZE_MAX / sizeof(*newSlabs)) {
        return ZR_FALSE;
    }
    newSlabs = (SZrPoolSlab **)realloc(
            pool->slabs, newCapacity * sizeof(*newSlabs));
    if (newSlabs == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(
            newSlabs + pool->slabCapacity,
            0,
            (newCapacity - pool->slabCapacity) * sizeof(*newSlabs));
    pool->slabs = newSlabs;
    pool->slabCapacity = newCapacity;
    return ZR_TRUE;
}

static EZrPoolStatus zr_pool_add_slab(SZrPool *pool) {
    SZrPoolSlab *slab;
    TZrSize storageSize;
    TZrSize baseIndex;

    if (pool == ZR_NULL || !zr_pool_grow_slab_directory(pool) ||
        pool->slabCount > SIZE_MAX / pool->config.slabCapacity ||
        pool->config.slabCapacity > SIZE_MAX / pool->elementStride) {
        return ZR_POOL_STATUS_OUT_OF_MEMORY;
    }
    baseIndex = pool->slabCount * pool->config.slabCapacity;
    storageSize = pool->config.slabCapacity * pool->elementStride;
    slab = (SZrPoolSlab *)calloc(1u, sizeof(*slab));
    if (slab == ZR_NULL) {
        return ZR_POOL_STATUS_OUT_OF_MEMORY;
    }
    slab->storage = (unsigned char *)zr_pool_aligned_allocate(
            storageSize,
            pool->layout.elementAlignment,
            &slab->allocation);
    slab->slots = (SZrPoolSlot *)calloc(
            pool->config.slabCapacity, sizeof(*slab->slots));
    if (slab->storage == ZR_NULL || slab->slots == ZR_NULL) {
        free(slab->slots);
        free(slab->allocation);
        free(slab);
        return ZR_POOL_STATUS_OUT_OF_MEMORY;
    }
    slab->baseIndex = baseIndex;
    for (TZrSize index = pool->config.slabCapacity; index > 0u; index--) {
        SZrPoolSlot *slot = &slab->slots[index - 1u];

        slot->state = ZR_POOL_SLOT_FREE;
        slot->nextFree = pool->freeHead;
        pool->freeHead = baseIndex + index - 1u;
    }
    pool->slabs[pool->slabCount++] = slab;
    pool->stats.slabCount++;
    pool->stats.slotCount += pool->config.slabCapacity;
    pool->stats.freeCount += pool->config.slabCapacity;
    return ZR_POOL_STATUS_OK;
}

static void zr_pool_clear_handle(SZrPoolHandle *handle) {
    if (handle != ZR_NULL) {
        memset(handle, 0, sizeof(*handle));
    }
}

static void zr_pool_clear_guard(SZrPoolGuard *guard) {
    if (guard != ZR_NULL) {
        memset(guard, 0, sizeof(*guard));
    }
}

static EZrPoolStatus zr_pool_validate_handle(
        const SZrPool *pool,
        SZrPoolHandle handle,
        SZrPoolSlot **outSlot,
        SZrPoolSlab **outSlab) {
    SZrPoolSlot *slot;

    if (outSlot != ZR_NULL) {
        *outSlot = ZR_NULL;
    }
    if (outSlab != ZR_NULL) {
        *outSlab = ZR_NULL;
    }
    if (pool == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    if (handle.poolId != pool->id) {
        return ZR_POOL_STATUS_WRONG_POOL;
    }
    slot = zr_pool_find_slot(pool, handle.slotIndex, outSlab);
    if (slot == ZR_NULL || slot->generation != handle.generation ||
        handle.generation == 0u || slot->state == ZR_POOL_SLOT_FREE ||
        slot->state == ZR_POOL_SLOT_EXHAUSTED) {
        return ZR_POOL_STATUS_HANDLE_STALE;
    }
    if (outSlot != ZR_NULL) {
        *outSlot = slot;
    }
    return slot->state == ZR_POOL_SLOT_RETIRED
                   ? ZR_POOL_STATUS_ENTITY_RETIRED
                   : ZR_POOL_STATUS_OK;
}

static void zr_pool_reclaim(
        SZrPool *pool,
        TZrSize slotIndex,
        SZrPoolSlot *slot,
        SZrPoolSlab *slab) {
    void *value;

    if (pool == ZR_NULL || slot == ZR_NULL || slab == ZR_NULL ||
        slot->state != ZR_POOL_SLOT_RETIRED || slot->readerCount != 0u ||
        slot->writerActive) {
        return;
    }
    value = zr_pool_slot_value(pool, slab, slotIndex);
    if (slot->initialized) {
        if (pool->layout.drop != ZR_NULL) {
            pool->layout.drop(value, pool->layout.context);
        }
        memset(value, 0, pool->layout.elementSize);
        slot->initialized = ZR_FALSE;
        pool->stats.dropCount++;
    }
    pool->stats.retiredCount--;
    if (slot->generation >= pool->config.generationLimit) {
        slot->state = ZR_POOL_SLOT_EXHAUSTED;
        slot->nextFree = ZR_POOL_NO_SLOT;
        pool->stats.exhaustedCount++;
        return;
    }
    slot->state = ZR_POOL_SLOT_FREE;
    slot->nextFree = pool->freeHead;
    pool->freeHead = slotIndex;
    pool->stats.freeCount++;
}

EZrPoolStatus ZrPool_Create(
        const SZrPoolTypeLayout *layout,
        const SZrPoolConfig *config,
        SZrPool **outPool) {
    SZrPool *pool;
    TZrSize slabCapacity;
    uint64_t generationLimit;

    if (outPool == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    *outPool = ZR_NULL;
    if (layout == ZR_NULL || layout->elementSize == 0u ||
        !zr_pool_is_power_of_two(layout->elementAlignment) ||
        layout->gcScanKind > ZR_POOL_GC_SCAN_BARRIERED ||
        (layout->gcScanKind != ZR_POOL_GC_SCAN_FREE &&
         layout->scan == ZR_NULL)) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    if (config != ZR_NULL &&
        config->concurrencyMode > ZR_POOL_CONCURRENCY_CONCURRENT) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    slabCapacity = config != ZR_NULL && config->slabCapacity != 0u
                           ? config->slabCapacity
                           : ZR_POOL_DEFAULT_SLAB_CAPACITY;
    generationLimit = config != ZR_NULL && config->generationLimit != 0u
                              ? config->generationLimit
                              : UINT64_MAX;
    if (slabCapacity == 0u) {
        return ZR_POOL_STATUS_GENERATION_EXHAUSTED;
    }
    pool = (SZrPool *)calloc(1u, sizeof(*pool));
    if (pool == ZR_NULL) {
        return ZR_POOL_STATUS_OUT_OF_MEMORY;
    }
    pool->layout = *layout;
    pool->config.slabCapacity = slabCapacity;
    pool->config.generationLimit = generationLimit;
    pool->config.concurrencyMode = config != ZR_NULL
                                           ? config->concurrencyMode
                                           : ZR_POOL_CONCURRENCY_THREAD_LOCAL;
    pool->freeHead = ZR_POOL_NO_SLOT;
    zr_pool_lock_word(&gPoolIdLockWord);
    if (gNextPoolId == 0u) {
        zr_pool_unlock_word(&gPoolIdLockWord);
        free(pool);
        return ZR_POOL_STATUS_GENERATION_EXHAUSTED;
    }
    pool->id = gNextPoolId++;
    zr_pool_unlock_word(&gPoolIdLockWord);
    pool->stats.poolId = pool->id;
    if (!zr_pool_align_up(
                layout->elementSize,
                layout->elementAlignment,
                &pool->elementStride)) {
        free(pool);
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    *outPool = pool;
    return ZR_POOL_STATUS_OK;
}

EZrPoolStatus ZrPool_Destroy(SZrPool **poolPointer) {
    SZrPool *pool;

    if (poolPointer == ZR_NULL || *poolPointer == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    pool = *poolPointer;
    zr_pool_lock(pool);
    pool->destroying = ZR_TRUE;
    for (TZrSize slabIndex = 0u; slabIndex < pool->slabCount; slabIndex++) {
        SZrPoolSlab *slab = pool->slabs[slabIndex];

        for (TZrSize localIndex = 0u;
             localIndex < pool->config.slabCapacity;
             localIndex++) {
            SZrPoolSlot *slot = &slab->slots[localIndex];
            TZrSize slotIndex = slab->baseIndex + localIndex;

            if (slot->state == ZR_POOL_SLOT_LIVE) {
                slot->state = ZR_POOL_SLOT_RETIRED;
                pool->stats.liveCount--;
                pool->stats.retiredCount++;
            }
            zr_pool_reclaim(pool, slotIndex, slot, slab);
        }
    }
    if (pool->stats.activeReadCount != 0u ||
        pool->stats.activeWriteCount != 0u ||
        pool->stats.retiredCount != 0u) {
        zr_pool_unlock(pool);
        return ZR_POOL_STATUS_POOL_BUSY;
    }
    zr_pool_unlock(pool);
    for (TZrSize slabIndex = 0u; slabIndex < pool->slabCount; slabIndex++) {
        SZrPoolSlab *slab = pool->slabs[slabIndex];

        free(slab->slots);
        free(slab->allocation);
        free(slab);
    }
    free(pool->slabs);
    memset(pool, 0, sizeof(*pool));
    free(pool);
    *poolPointer = ZR_NULL;
    return ZR_POOL_STATUS_OK;
}

static EZrPoolStatus zr_pool_deliver_unlocked(
        SZrPool *pool,
        const void *source,
        SZrPoolHandle *outHandle) {
    SZrPoolSlot *slot;
    SZrPoolSlab *slab;
    TZrSize slotIndex;
    void *value;
    EZrPoolStatus status;

    zr_pool_clear_handle(outHandle);
    if (pool == ZR_NULL || source == ZR_NULL || outHandle == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    if (pool->destroying) {
        return ZR_POOL_STATUS_POOL_DESTROYED;
    }
    if (pool->freeHead == ZR_POOL_NO_SLOT) {
        status = zr_pool_add_slab(pool);
        if (status != ZR_POOL_STATUS_OK) {
            return status;
        }
    }
    slotIndex = pool->freeHead;
    slot = zr_pool_find_slot(pool, slotIndex, &slab);
    if (slot == ZR_NULL || slot->state != ZR_POOL_SLOT_FREE ||
        slot->generation >= pool->config.generationLimit) {
        return ZR_POOL_STATUS_GENERATION_EXHAUSTED;
    }
    pool->freeHead = slot->nextFree;
    slot->nextFree = ZR_POOL_NO_SLOT;
    pool->stats.freeCount--;
    value = zr_pool_slot_value(pool, slab, slotIndex);
    memset(value, 0, pool->layout.elementSize);
    if (pool->layout.initialize != ZR_NULL) {
        if (!pool->layout.initialize(value, source, pool->layout.context)) {
            memset(value, 0, pool->layout.elementSize);
            slot->nextFree = pool->freeHead;
            pool->freeHead = slotIndex;
            pool->stats.freeCount++;
            return ZR_POOL_STATUS_CONSTRUCTION_FAILED;
        }
    } else {
        memcpy(value, source, pool->layout.elementSize);
    }
    slot->generation++;
    slot->state = ZR_POOL_SLOT_LIVE;
    slot->initialized = ZR_TRUE;
    if (slot->reused) {
        pool->stats.reuseCount++;
    }
    slot->reused = ZR_TRUE;
    pool->stats.liveCount++;
    pool->stats.deliverCount++;
    outHandle->poolId = pool->id;
    outHandle->slotIndex = slotIndex;
    outHandle->generation = slot->generation;
    return ZR_POOL_STATUS_OK;
}

EZrPoolStatus ZrPool_Deliver(
        SZrPool *pool,
        const void *source,
        SZrPoolHandle *outHandle) {
    EZrPoolStatus status;

    if (pool == ZR_NULL) {
        zr_pool_clear_handle(outHandle);
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    zr_pool_lock(pool);
    status = zr_pool_deliver_unlocked(pool, source, outHandle);
    zr_pool_unlock(pool);
    return status;
}

static EZrPoolStatus zr_pool_recycle_unlocked(
        SZrPool *pool,
        SZrPoolHandle handle) {
    SZrPoolSlot *slot;
    SZrPoolSlab *slab;
    EZrPoolStatus status;

    if (pool == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    if (pool->destroying) {
        return ZR_POOL_STATUS_POOL_DESTROYED;
    }
    status = zr_pool_validate_handle(pool, handle, &slot, &slab);
    if (status != ZR_POOL_STATUS_OK) {
        return status;
    }
    slot->state = ZR_POOL_SLOT_RETIRED;
    pool->stats.liveCount--;
    pool->stats.retiredCount++;
    pool->stats.recycleCount++;
    zr_pool_reclaim(pool, handle.slotIndex, slot, slab);
    return ZR_POOL_STATUS_OK;
}

EZrPoolStatus ZrPool_Recycle(SZrPool *pool, SZrPoolHandle handle) {
    EZrPoolStatus status;

    if (pool == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    zr_pool_lock(pool);
    status = zr_pool_recycle_unlocked(pool, handle);
    zr_pool_unlock(pool);
    return status;
}

static EZrPoolStatus zr_pool_validate_unlocked(
        const SZrPool *pool,
        SZrPoolHandle handle) {
    if (pool != ZR_NULL && pool->destroying) {
        return ZR_POOL_STATUS_POOL_DESTROYED;
    }
    return zr_pool_validate_handle(pool, handle, ZR_NULL, ZR_NULL);
}

EZrPoolStatus ZrPool_Validate(
        const SZrPool *pool,
        SZrPoolHandle handle) {
    EZrPoolStatus status;

    if (pool == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    zr_pool_lock(pool);
    status = zr_pool_validate_unlocked(pool, handle);
    zr_pool_unlock(pool);
    return status;
}

static EZrPoolStatus zr_pool_acquire(
        SZrPool *pool,
        SZrPoolHandle handle,
        EZrPoolBorrowMode mode,
        SZrPoolGuard *outGuard) {
    SZrPoolSlot *slot;
    SZrPoolSlab *slab;
    EZrPoolStatus status;

    zr_pool_clear_guard(outGuard);
    if (pool == ZR_NULL || outGuard == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    if (pool->destroying) {
        return ZR_POOL_STATUS_POOL_DESTROYED;
    }
    status = zr_pool_validate_handle(pool, handle, &slot, &slab);
    if (status != ZR_POOL_STATUS_OK) {
        return status;
    }
    if (mode == ZR_POOL_BORROW_READ) {
        if (slot->writerActive || slot->readerCount == SIZE_MAX) {
            return ZR_POOL_STATUS_BORROW_CONFLICT;
        }
        slot->readerCount++;
        pool->stats.activeReadCount++;
    } else {
        if (slot->writerActive || slot->readerCount != 0u) {
            return ZR_POOL_STATUS_BORROW_CONFLICT;
        }
        slot->writerActive = ZR_TRUE;
        pool->stats.activeWriteCount++;
    }
    outGuard->pool = pool;
    outGuard->value = zr_pool_slot_value(pool, slab, handle.slotIndex);
    outGuard->slotIndex = handle.slotIndex;
    outGuard->generation = handle.generation;
    outGuard->mode = mode;
    outGuard->active = ZR_TRUE;
    return ZR_POOL_STATUS_OK;
}

EZrPoolStatus ZrPool_TryRead(
        SZrPool *pool,
        SZrPoolHandle handle,
        SZrPoolGuard *outGuard) {
    EZrPoolStatus status;

    if (pool == ZR_NULL) {
        zr_pool_clear_guard(outGuard);
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    zr_pool_lock(pool);
    status = zr_pool_acquire(
            pool, handle, ZR_POOL_BORROW_READ, outGuard);
    zr_pool_unlock(pool);
    return status;
}

EZrPoolStatus ZrPool_TryBorrow(
        SZrPool *pool,
        SZrPoolHandle handle,
        SZrPoolGuard *outGuard) {
    EZrPoolStatus status;

    if (pool == ZR_NULL) {
        zr_pool_clear_guard(outGuard);
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    zr_pool_lock(pool);
    status = zr_pool_acquire(
            pool, handle, ZR_POOL_BORROW_WRITE, outGuard);
    zr_pool_unlock(pool);
    return status;
}

const void *ZrPoolGuard_ReadOnlyValue(const SZrPoolGuard *guard) {
    if (guard == ZR_NULL || !guard->active || guard->pool == ZR_NULL ||
        guard->value == ZR_NULL) {
        return ZR_NULL;
    }
    return guard->value;
}

void *ZrPoolGuard_Value(SZrPoolGuard *guard) {
    if (guard == ZR_NULL || guard->mode != ZR_POOL_BORROW_WRITE) {
        return ZR_NULL;
    }
    return (void *)ZrPoolGuard_ReadOnlyValue(guard);
}

static EZrPoolStatus zr_pool_guard_release_unlocked(SZrPoolGuard *guard) {
    SZrPool *pool;
    SZrPoolSlab *slab;
    SZrPoolSlot *slot;
    TZrSize slotIndex;

    if (guard == ZR_NULL || !guard->active || guard->pool == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    pool = guard->pool;
    slotIndex = guard->slotIndex;
    slot = zr_pool_find_slot(pool, slotIndex, &slab);
    if (slot == ZR_NULL || slot->generation != guard->generation ||
        (slot->state != ZR_POOL_SLOT_LIVE &&
         slot->state != ZR_POOL_SLOT_RETIRED)) {
        zr_pool_clear_guard(guard);
        return ZR_POOL_STATUS_HANDLE_STALE;
    }
    if (guard->mode == ZR_POOL_BORROW_READ) {
        if (slot->readerCount == 0u) {
            zr_pool_clear_guard(guard);
            return ZR_POOL_STATUS_INVALID_ARGUMENT;
        }
        slot->readerCount--;
        pool->stats.activeReadCount--;
    } else {
        if (!slot->writerActive) {
            zr_pool_clear_guard(guard);
            return ZR_POOL_STATUS_INVALID_ARGUMENT;
        }
        slot->writerActive = ZR_FALSE;
        pool->stats.activeWriteCount--;
    }
    zr_pool_clear_guard(guard);
    zr_pool_reclaim(pool, slotIndex, slot, slab);
    return ZR_POOL_STATUS_OK;
}

EZrPoolStatus ZrPoolGuard_Release(SZrPoolGuard *guard) {
    EZrPoolStatus status;
    SZrPool *pool;

    if (guard == ZR_NULL || !guard->active || guard->pool == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    pool = guard->pool;
    zr_pool_lock(pool);
    status = zr_pool_guard_release_unlocked(guard);
    zr_pool_unlock(pool);
    return status;
}

static EZrPoolStatus zr_pool_scan_unlocked(
        SZrPool *pool,
        uint64_t *outScannedSlots,
        uint64_t *outScannedBytes) {
    uint64_t scannedSlots = 0u;
    uint64_t scannedBytes = 0u;

    if (outScannedSlots != ZR_NULL) {
        *outScannedSlots = 0u;
    }
    if (outScannedBytes != ZR_NULL) {
        *outScannedBytes = 0u;
    }
    if (pool == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    if (pool->layout.gcScanKind == ZR_POOL_GC_SCAN_FREE) {
        return ZR_POOL_STATUS_OK;
    }
    for (TZrSize slabIndex = 0u; slabIndex < pool->slabCount; slabIndex++) {
        SZrPoolSlab *slab = pool->slabs[slabIndex];

        for (TZrSize localIndex = 0u;
             localIndex < pool->config.slabCapacity;
             localIndex++) {
            SZrPoolSlot *slot = &slab->slots[localIndex];

            if (!slot->initialized ||
                (slot->state != ZR_POOL_SLOT_LIVE &&
                 slot->state != ZR_POOL_SLOT_RETIRED)) {
                continue;
            }
            pool->layout.scan(
                    slab->storage + localIndex * pool->elementStride,
                    pool->layout.context);
            scannedSlots++;
            scannedBytes += pool->layout.elementSize;
        }
    }
    pool->stats.scannedSlotCount += scannedSlots;
    pool->stats.scannedByteCount += scannedBytes;
    if (outScannedSlots != ZR_NULL) {
        *outScannedSlots = scannedSlots;
    }
    if (outScannedBytes != ZR_NULL) {
        *outScannedBytes = scannedBytes;
    }
    return ZR_POOL_STATUS_OK;
}

EZrPoolStatus ZrPool_Scan(
        SZrPool *pool,
        uint64_t *outScannedSlots,
        uint64_t *outScannedBytes) {
    EZrPoolStatus status;

    if (pool == ZR_NULL) {
        if (outScannedSlots != ZR_NULL) {
            *outScannedSlots = 0u;
        }
        if (outScannedBytes != ZR_NULL) {
            *outScannedBytes = 0u;
        }
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    zr_pool_lock(pool);
    status = zr_pool_scan_unlocked(
            pool, outScannedSlots, outScannedBytes);
    zr_pool_unlock(pool);
    return status;
}

static EZrPoolStatus zr_pool_get_stats_unlocked(
        const SZrPool *pool,
        SZrPoolStats *outStats) {
    if (pool == ZR_NULL || outStats == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    *outStats = pool->stats;
    return ZR_POOL_STATUS_OK;
}

EZrPoolStatus ZrPool_GetStats(
        const SZrPool *pool,
        SZrPoolStats *outStats) {
    EZrPoolStatus status;

    if (pool == ZR_NULL) {
        return ZR_POOL_STATUS_INVALID_ARGUMENT;
    }
    zr_pool_lock(pool);
    status = zr_pool_get_stats_unlocked(pool, outStats);
    zr_pool_unlock(pool);
    return status;
}

const TZrChar *ZrPool_StatusName(EZrPoolStatus status) {
    switch (status) {
        case ZR_POOL_STATUS_OK:
            return "ok";
        case ZR_POOL_STATUS_INVALID_ARGUMENT:
            return "invalid_argument";
        case ZR_POOL_STATUS_OUT_OF_MEMORY:
            return "out_of_memory";
        case ZR_POOL_STATUS_WRONG_POOL:
            return "handle_wrong_pool";
        case ZR_POOL_STATUS_HANDLE_STALE:
            return "handle_stale";
        case ZR_POOL_STATUS_ENTITY_RETIRED:
            return "entity_retired";
        case ZR_POOL_STATUS_BORROW_CONFLICT:
            return "borrow_conflict";
        case ZR_POOL_STATUS_CONSTRUCTION_FAILED:
            return "construction_failed";
        case ZR_POOL_STATUS_GENERATION_EXHAUSTED:
            return "generation_exhausted";
        case ZR_POOL_STATUS_POOL_BUSY:
            return "pool_busy";
        case ZR_POOL_STATUS_POOL_DESTROYED:
            return "pool_destroyed";
        default:
            return "unknown";
    }
}
