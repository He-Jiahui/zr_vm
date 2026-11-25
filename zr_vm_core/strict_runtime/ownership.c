#include "zr_vm_strict/ownership.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum ZrStrictLifetime {
    ZR_STRICT_BUILDING,
    ZR_STRICT_UNIQUE,
    ZR_STRICT_SHARED,
    ZR_STRICT_DROPPING,
    ZR_STRICT_DEAD
} ZrStrictLifetime;

struct ZrStrictAllocation {
    ZrStrictRuntime *runtime;
    ZrStrictAllocation *next;
    ZrStrictAllocation *dropNext;
    ZrStrictAllocation **previousLink;
    void *payload;
    ZrStrictLayout layout;
    ZrStrictSite site;
    size_t strong;
    size_t weak;
    ZrStrictLifetime lifetime;
};

static void *strict_malloc(void *context, size_t size) {
    (void)context;
    return malloc(size);
}

static void strict_free(void *context, void *address) {
    (void)context;
    free(address);
}

static int runtime_open(const ZrStrictRuntime *runtime) {
    return runtime && runtime->allocator.allocate && !runtime->closing;
}

static int has_lifetime(const ZrStrictAllocation *record, ZrStrictLifetime lifetime) {
    return record && record->lifetime == lifetime && runtime_open(record->runtime);
}

static void free_record(ZrStrictAllocation *record) {
    *record->previousLink = record->next;
    if (record->next) {
        record->next->previousLink = record->previousLink;
    }
    record->runtime->allocator.free(record->runtime->allocator.context, record);
}

ZrStrictStatus ZrStrictRuntime_Init(ZrStrictRuntime *runtime,
                                    const ZrStrictAllocator *allocator) {
    ZrStrictAllocator defaultAllocator = {strict_malloc, strict_free, NULL};
    if (!runtime || (allocator && (!allocator->allocate || !allocator->free))) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->allocator = allocator ? *allocator : defaultAllocator;
    return ZR_STRICT_OK;
}

ZrStrictStatus ZrStrictRuntime_Close(ZrStrictRuntime *runtime,
                                     ZrStrictReportSurvivor report, void *context) {
    ZrStrictAllocation *record;
    if (!runtime_open(runtime)) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    if (runtime->dropDepth || runtime->openPools) {
        return ZR_STRICT_BUSY;
    }
    runtime->closing = 1;
    if (report) {
        for (record = runtime->allocations; record; record = record->next) {
            if (record->strong) {
                report(context, record->layout.name, record->site, record->strong);
            }
        }
    }
    while (runtime->allocations) {
        free_record(runtime->allocations);
    }
    return ZR_STRICT_OK;
}

ZrStrictStatus ZrStrict_OwnBegin(ZrStrictRuntime *runtime, const ZrStrictLayout *layout,
                                ZrStrictSite site, ZrStrictBuilder *out, void **payload) {
    ZrStrictAllocation *record;
    unsigned char *start;
    size_t alignmentMask;
    size_t storageSize;
    size_t padding;
    if (!runtime_open(runtime) || !layout || !layout->alignment ||
        (layout->alignment & (layout->alignment - 1)) != 0 || !out ||
        out->record || !payload) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    *payload = NULL;
    if (runtime->dropDepth || runtime->poolDropDepth) {
        return ZR_STRICT_BUSY;
    }
    alignmentMask = layout->alignment - 1;
    storageSize = layout->size ? layout->size : 1;
    if (storageSize > SIZE_MAX - sizeof(*record) ||
        alignmentMask > SIZE_MAX - sizeof(*record) - storageSize) {
        return ZR_STRICT_OUT_OF_MEMORY;
    }
    record = runtime->allocator.allocate(runtime->allocator.context,
                                         sizeof(*record) + storageSize + alignmentMask);
    if (!record) {
        return ZR_STRICT_OUT_OF_MEMORY;
    }
    start = (unsigned char *)(record + 1);
    padding = (size_t)((-(uintptr_t)start) & alignmentMask);
    *record = (ZrStrictAllocation){
        .runtime = runtime,
        .next = runtime->allocations,
        .previousLink = &runtime->allocations,
        .payload = start + padding,
        .layout = *layout,
        .site = site,
        .lifetime = ZR_STRICT_BUILDING
    };
    if (record->next) {
        record->next->previousLink = &record->next;
    }
    runtime->allocations = record;
    out->record = record;
    *payload = record->payload;
    return ZR_STRICT_OK;
}

ZrStrictStatus ZrStrict_OwnCommit(ZrStrictBuilder *builder, ZrStrictUnique *out) {
    if (!builder || !has_lifetime(builder->record, ZR_STRICT_BUILDING) || !out || out->record) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    out->record = builder->record;
    out->record->strong = 1;
    out->record->lifetime = ZR_STRICT_UNIQUE;
    builder->record = NULL;
    return ZR_STRICT_OK;
}

ZrStrictStatus ZrStrict_OwnAbort(ZrStrictBuilder *builder) {
    ZrStrictAllocation *record;
    if (!builder || !has_lifetime(builder->record, ZR_STRICT_BUILDING)) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    record = builder->record;
    builder->record = NULL;
    free_record(record);
    return ZR_STRICT_OK;
}

void *ZrStrict_UniqueData(const ZrStrictUnique *owner) {
    return owner && has_lifetime(owner->record, ZR_STRICT_UNIQUE) ? owner->record->payload : NULL;
}

static ZrStrictStatus release_strong(ZrStrictAllocation *record) {
    if (--record->strong != 0) return ZR_STRICT_OK;
    ZrStrictRuntime *runtime = record->runtime;
    /* Disable wake immediately but retain the header until its callback runs.
     * Nested releases enqueue their existing headers, so a deep chain can be
     * destroyed without allocating a worklist or growing the C call stack. */
    record->lifetime = ZR_STRICT_DROPPING;
    record->dropNext = NULL;
    if (runtime->dropDepth) {
        *runtime->dropAppend = record;
        runtime->dropAppend = &record->dropNext;
        return ZR_STRICT_OK;
    }
    ZrStrictStatus first = ZR_STRICT_OK;
    ZrStrictAllocation *ready = record;
    ++runtime->dropDepth;
    while (ready) {
        ZrStrictAllocation *next = ready->dropNext;
        ZrStrictAllocation *children = NULL;
        runtime->dropAppend = &children;
        if (ready->layout.drop) {
            const ZrStrictStatus status = ready->layout.drop(ready->payload, ready->layout.dropContext);
            if (first == ZR_STRICT_OK) first = status;
        }
        *runtime->dropAppend = next; /* descendants before siblings, in release order */
        ready->lifetime = ZR_STRICT_DEAD;
        if (!ready->weak) free_record(ready);
        ready = children;
    }
    runtime->dropAppend = NULL;
    --runtime->dropDepth;
    return first;
}

ZrStrictStatus ZrStrict_UniqueDrop(ZrStrictUnique *owner) {
    ZrStrictAllocation *record;
    if (!owner || !has_lifetime(owner->record, ZR_STRICT_UNIQUE)) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    record = owner->record;
    owner->record = NULL;
    return release_strong(record);
}

ZrStrictStatus ZrStrict_Share(ZrStrictUnique *owner, ZrStrictShared *out) {
    if (!owner || !has_lifetime(owner->record, ZR_STRICT_UNIQUE) || !out || out->record) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    out->record = owner->record;
    out->record->lifetime = ZR_STRICT_SHARED;
    owner->record = NULL;
    return ZR_STRICT_OK;
}

static ZrStrictStatus retain_strong(ZrStrictAllocation *record, ZrStrictShared *out) {
    if (record->strong == SIZE_MAX) {
        return ZR_STRICT_REFCOUNT_LIMIT;
    }
    ++record->strong;
    out->record = record;
    return ZR_STRICT_OK;
}

ZrStrictStatus ZrStrict_SharedClone(const ZrStrictShared *owner, ZrStrictShared *out) {
    if (!owner || !has_lifetime(owner->record, ZR_STRICT_SHARED) || !out || out->record) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    return retain_strong(owner->record, out);
}

const void *ZrStrict_SharedData(const ZrStrictShared *owner) {
    return owner && has_lifetime(owner->record, ZR_STRICT_SHARED) ? owner->record->payload : NULL;
}

ZrStrictStatus ZrStrict_SharedDrop(ZrStrictShared *owner) {
    ZrStrictAllocation *record;
    if (!owner || !has_lifetime(owner->record, ZR_STRICT_SHARED)) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    record = owner->record;
    owner->record = NULL;
    return release_strong(record);
}

static ZrStrictStatus retain_weak(ZrStrictAllocation *record, ZrStrictWeak *out) {
    if (record->weak == SIZE_MAX) {
        return ZR_STRICT_REFCOUNT_LIMIT;
    }
    ++record->weak;
    out->record = record;
    return ZR_STRICT_OK;
}

ZrStrictStatus ZrStrict_Degrade(const ZrStrictShared *owner, ZrStrictWeak *out) {
    if (!owner || !has_lifetime(owner->record, ZR_STRICT_SHARED) || !out || out->record) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    return retain_weak(owner->record, out);
}

static int weak_valid(const ZrStrictWeak *observer) {
    return observer && observer->record && observer->record->weak &&
           runtime_open(observer->record->runtime);
}

ZrStrictStatus ZrStrict_WeakClone(const ZrStrictWeak *observer, ZrStrictWeak *out) {
    if (!weak_valid(observer) || !out || out->record) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    return retain_weak(observer->record, out);
}

ZrStrictStatus ZrStrict_Wake(const ZrStrictWeak *observer, ZrStrictShared *out) {
    if (!out || out->record || !observer) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    if (!observer->record) {
        return ZR_STRICT_EXPIRED;
    }
    if (!weak_valid(observer)) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    if (observer->record->lifetime != ZR_STRICT_SHARED) {
        return ZR_STRICT_EXPIRED;
    }
    return retain_strong(observer->record, out);
}

ZrStrictStatus ZrStrict_WeakDrop(ZrStrictWeak *observer) {
    ZrStrictAllocation *record;
    if (!weak_valid(observer)) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    record = observer->record;
    observer->record = NULL;
    if (--record->weak == 0 && record->lifetime == ZR_STRICT_DEAD) {
        free_record(record);
    }
    return ZR_STRICT_OK;
}

ZrStrictStatus ZrStrict_ManualAlloc(ZrStrictRuntime *runtime, size_t size,
                                    ZrStrictSite site, ZrStrictManualBlock *out) {
    const ZrStrictLayout layout = {size, 1, "ManualBlock", NULL, NULL};
    ZrStrictBuilder builder = {0};
    ZrStrictStatus status;
    void *payload;
    if (!out || out->owner.record) {
        return ZR_STRICT_INVALID_ARGUMENT;
    }
    status = ZrStrict_OwnBegin(runtime, &layout, site, &builder, &payload);
    if (status == ZR_STRICT_OK) {
        status = ZrStrict_OwnCommit(&builder, &out->owner);
        if (status != ZR_STRICT_OK) (void)ZrStrict_OwnAbort(&builder);
    }
    return status;
}

void *ZrStrict_ManualData(const ZrStrictManualBlock *block, size_t *size) {
    void *data = block ? ZrStrict_UniqueData(&block->owner) : NULL;
    if (size) {
        *size = data ? block->owner.record->layout.size : 0;
    }
    return data;
}

ZrStrictStatus ZrStrict_ManualFree(ZrStrictManualBlock *block) {
    return block ? ZrStrict_UniqueDrop(&block->owner) : ZR_STRICT_INVALID_ARGUMENT;
}
