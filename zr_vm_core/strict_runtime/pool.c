#include "zr_vm_strict/pool.h"

#include <string.h>

/* Only the slot header participates in lookup. Payloads use their layout's
 * alignment and cannot move while any handle or view can refer to them. */
typedef enum PoolSlotState { SLOT_FREE, SLOT_RENTED, SLOT_RETIRED } PoolSlotState;

typedef struct PoolSlot {
    uint64_t rental;
    ZrStrictSite site;
    size_t readers;
    unsigned writer;
    PoolSlotState state;
} PoolSlot;

struct ZrStrictPoolSlab {
    ZrStrictPoolSlab *next;
    uint64_t identity;
    size_t capacity, occupied, payloadOffset;
    PoolSlot slots[];
};

static int pool_open(const ZrStrictPool *pool) {
    return pool && pool->runtime && !pool->busy;
}

static int round_up(size_t value, size_t alignment, size_t *out) {
    if (value > SIZE_MAX - (alignment - 1)) return 0;
    *out = (value + alignment - 1) & ~(alignment - 1);
    return 1;
}

ZrStrictStatus ZrStrict_PoolInit(ZrStrictRuntime *runtime, ZrStrictPool *pool, const ZrStrictLayout *layout,
                                 size_t slabSize, size_t hardCap) {
    size_t stride;
    if (!runtime || runtime->closing || !runtime->allocator.allocate || !runtime->allocator.free ||
        !pool || pool->runtime || !layout || !layout->alignment || layout->alignment > UINTPTR_MAX ||
        (layout->alignment & (layout->alignment - 1)) || !slabSize ||
        !round_up(layout->size ? layout->size : 1, layout->alignment, &stride)) return ZR_STRICT_INVALID_ARGUMENT;
    if (runtime->dropDepth || runtime->poolDropDepth) return ZR_STRICT_BUSY;
    if (runtime->lastPoolIdentity == UINT64_MAX || runtime->openPools == SIZE_MAX) return ZR_STRICT_REFCOUNT_LIMIT;
    *pool = (ZrStrictPool){.runtime = runtime, .layout = *layout, .identity = ++runtime->lastPoolIdentity,
                           .stride = stride, .slabSize = slabSize, .hardCap = hardCap};
    ++runtime->openPools;
    return ZR_STRICT_OK;
}

static unsigned char *payload(const ZrStrictPool *pool, ZrStrictPoolSlab *slab, size_t index) {
    return (unsigned char *)slab + slab->payloadOffset + index * pool->stride;
}

static ZrStrictPoolSlab *find_slab(const ZrStrictPool *pool, ZrStrictPoolHandle handle) {
    if (!pool_open(pool) || handle.runtime != (uintptr_t)pool->runtime || handle.pool != pool->identity ||
        !handle.slab || !handle.rental) return NULL;
    for (ZrStrictPoolSlab *slab = pool->slabs; slab; slab = slab->next) {
        if (slab->identity != handle.slab) continue;
        if (handle.slot >= slab->capacity || slab->slots[handle.slot].rental != handle.rental ||
            slab->slots[handle.slot].state == SLOT_FREE) return NULL;
        return slab;
    }
    return NULL;
}

static ZrStrictStatus grow(ZrStrictPool *pool, ZrStrictPoolSlab **out) {
    size_t count = pool->slabSize, metadata, offset;
    if (pool->hardCap) {
        size_t remaining = pool->hardCap - pool->capacity;
        if (count > remaining) count = remaining;
    }
    if (!count || pool->lastSlab == UINT64_MAX || count > SIZE_MAX - pool->capacity)
        return ZR_STRICT_OUT_OF_MEMORY;
    if (count > (SIZE_MAX - sizeof(ZrStrictPoolSlab)) / sizeof(PoolSlot)) return ZR_STRICT_OUT_OF_MEMORY;
    metadata = sizeof(ZrStrictPoolSlab) + count * sizeof(PoolSlot);
    const size_t padding = pool->layout.alignment - 1;
    if (padding > SIZE_MAX - metadata || count > (SIZE_MAX - metadata - padding) / pool->stride)
        return ZR_STRICT_OUT_OF_MEMORY;
    offset = metadata + padding;
    ZrStrictAllocator *allocator = &pool->runtime->allocator;
    ++pool->busy;
    ZrStrictPoolSlab *slab = allocator->allocate(allocator->context, offset + count * pool->stride);
    --pool->busy;
    if (!slab) return ZR_STRICT_OUT_OF_MEMORY;
    memset(slab, 0, metadata);
    slab->next = pool->slabs;
    slab->identity = ++pool->lastSlab;
    slab->capacity = count;
    const uintptr_t start = (uintptr_t)((unsigned char *)slab + metadata);
    slab->payloadOffset = metadata + ((0 - start) & padding);
    pool->slabs = slab;
    pool->capacity += count;
    *out = slab;
    return ZR_STRICT_OK;
}

ZrStrictStatus ZrStrict_PoolRent(ZrStrictPool *pool, const void *value, ZrStrictSite site, ZrStrictPoolHandle *out) {
    if (!pool_open(pool) || !out || out->rental || (!value && pool->layout.size)) return ZR_STRICT_INVALID_ARGUMENT;
    if (pool->runtime->dropDepth || pool->runtime->poolDropDepth) return ZR_STRICT_BUSY;
    if (pool->lastRental == UINT64_MAX) return ZR_STRICT_REFCOUNT_LIMIT;
    ZrStrictPoolSlab *slab = pool->slabs;
    while (slab && slab->occupied == slab->capacity) slab = slab->next;
    if (!slab) {
        ZrStrictStatus status = grow(pool, &slab);
        if (status != ZR_STRICT_OK) return status;
    }
    size_t index = 0;
    while (slab->slots[index].state != SLOT_FREE) ++index;
    PoolSlot *slot = &slab->slots[index];
    slot->rental = ++pool->lastRental;
    slot->site = site;
    slot->state = SLOT_RENTED;
    ++slab->occupied;
    if (pool->layout.size) memcpy(payload(pool, slab, index), value, pool->layout.size);
    *out = (ZrStrictPoolHandle){(uintptr_t)pool->runtime, pool->identity, slab->identity, slot->rental, index};
    return ZR_STRICT_OK;
}

int ZrStrict_PoolContains(const ZrStrictPool *pool, ZrStrictPoolHandle handle) {
    const ZrStrictPoolSlab *slab = find_slab(pool, handle);
    return slab && slab->slots[handle.slot].state == SLOT_RENTED;
}

static ZrStrictStatus drop_slot(ZrStrictPool *pool, ZrStrictPoolSlab *slab, size_t index) {
    unsigned char *data = payload(pool, slab, index);
    ZrStrictStatus status = ZR_STRICT_OK;
    if (pool->layout.drop) {
        ++pool->busy;
        ++pool->runtime->poolDropDepth;
        status = pool->layout.drop(data, pool->layout.dropContext);
        --pool->runtime->poolDropDepth;
        --pool->busy;
    }
    memset(data, 0, pool->stride);
    memset(&slab->slots[index], 0, sizeof(slab->slots[index]));
    --slab->occupied;
    return status;
}

ZrStrictStatus ZrStrict_PoolRecycle(ZrStrictPool *pool, ZrStrictPoolHandle handle) {
    ZrStrictPoolSlab *slab = find_slab(pool, handle);
    if (!slab || slab->slots[handle.slot].state != SLOT_RENTED) return ZR_STRICT_EXPIRED;
    PoolSlot *slot = &slab->slots[handle.slot];
    slot->state = SLOT_RETIRED;
    return slot->readers || slot->writer ? ZR_STRICT_OK : drop_slot(pool, slab, handle.slot);
}

ZrStrictStatus ZrStrict_PoolOpenView(ZrStrictPool *pool, ZrStrictPoolHandle handle, int writable, ZrStrictPoolView *out) {
    if (!out || out->pool) return ZR_STRICT_INVALID_ARGUMENT;
    ZrStrictPoolSlab *slab = find_slab(pool, handle);
    if (!slab || slab->slots[handle.slot].state != SLOT_RENTED) return ZR_STRICT_EXPIRED;
    PoolSlot *slot = &slab->slots[handle.slot];
    if (slot->writer || (writable && slot->readers)) return ZR_STRICT_BUSY;
    if (pool->openViews == SIZE_MAX || slot->readers == SIZE_MAX) return ZR_STRICT_REFCOUNT_LIMIT;
    if (writable) slot->writer = 1;
    else ++slot->readers;
    ++pool->openViews;
    *out = (ZrStrictPoolView){pool, handle, writable != 0};
    return ZR_STRICT_OK;
}

const void *ZrStrict_PoolViewData(const ZrStrictPoolView *view) {
    const ZrStrictPoolHandle handle = view ? view->handle : (ZrStrictPoolHandle){0};
    ZrStrictPoolSlab *slab = view ? find_slab(view->pool, handle) : NULL;
    if (!slab || !(view->writable ? slab->slots[handle.slot].writer : slab->slots[handle.slot].readers)) return NULL;
    return payload(view->pool, slab, handle.slot);
}

void *ZrStrict_PoolViewDataMut(const ZrStrictPoolView *view) {
    return view && view->writable ? (void *)ZrStrict_PoolViewData(view) : NULL;
}

ZrStrictStatus ZrStrict_PoolCloseView(ZrStrictPoolView *view) {
    const ZrStrictPoolHandle handle = view ? view->handle : (ZrStrictPoolHandle){0};
    ZrStrictPool *pool = view ? view->pool : NULL;
    ZrStrictPoolSlab *slab = find_slab(pool, handle);
    if (!slab) return ZR_STRICT_EXPIRED;
    PoolSlot *slot = &slab->slots[handle.slot];
    if (view->writable) {
        if (!slot->writer) return ZR_STRICT_INVALID_ARGUMENT;
        slot->writer = 0;
    } else {
        if (!slot->readers) return ZR_STRICT_INVALID_ARGUMENT;
        --slot->readers;
    }
    --pool->openViews;
    memset(view, 0, sizeof(*view));
    return slot->state == SLOT_RETIRED && !slot->readers && !slot->writer ? drop_slot(pool, slab, handle.slot) : ZR_STRICT_OK;
}

static void release_slab(ZrStrictPool *pool, ZrStrictPoolSlab *slab) {
    ZrStrictAllocator *allocator = &pool->runtime->allocator;
    pool->capacity -= slab->capacity;
    ++pool->busy;
    allocator->free(allocator->context, slab);
    --pool->busy;
}

size_t ZrStrict_PoolShrink(ZrStrictPool *pool) {
    if (!pool_open(pool)) return 0;
    size_t count = 0;
    ZrStrictPoolSlab **link = &pool->slabs;
    while (*link) {
        ZrStrictPoolSlab *slab = *link;
        if (slab->occupied) link = &slab->next;
        else {
            *link = slab->next;
            release_slab(pool, slab);
            ++count;
        }
    }
    return count;
}

ZrStrictStatus ZrStrict_PoolClose(ZrStrictPool *pool, ZrStrictReportRental report, void *context) {
    if (!pool || !pool->runtime) return ZR_STRICT_INVALID_ARGUMENT;
    if (pool->busy || pool->openViews) return ZR_STRICT_BUSY;
    ZrStrictStatus first = ZR_STRICT_OK;
    ++pool->busy;
    while (pool->slabs) {
        ZrStrictPoolSlab *slab = pool->slabs;
        pool->slabs = slab->next;
        for (size_t index = slab->capacity; index > 0;) {
            const PoolSlot *slot = &slab->slots[--index];
            if (slot->state == SLOT_FREE) continue;
            if (report) report(context, pool->layout.name, slot->site);
            ZrStrictStatus status = drop_slot(pool, slab, index);
            if (first == ZR_STRICT_OK) first = status;
        }
        release_slab(pool, slab);
    }
    --pool->runtime->openPools;
    memset(pool, 0, sizeof(*pool));
    return first;
}
