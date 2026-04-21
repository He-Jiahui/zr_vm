//
// Object lifecycle and raw-object helpers for the GC.
//

#include "gc/gc_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define ZR_GC_IGNORE_REGISTRY_INITIAL_CAPACITY ((TZrSize)8)
#define ZR_GC_IGNORE_REGISTRY_GROWTH_FACTOR ((TZrSize)2)
#define ZR_GC_REMEMBERED_REGISTRY_INITIAL_CAPACITY ((TZrSize)8)
#define ZR_GC_REMEMBERED_REGISTRY_GROWTH_FACTOR ((TZrSize)2)
#define ZR_GC_REGION_REGISTRY_INITIAL_CAPACITY ((TZrSize)8)
#define ZR_GC_REGION_REGISTRY_GROWTH_FACTOR ((TZrSize)2)

#if defined(ZR_DEBUG)
static TZrBool raw_object_trace_enabled(void);
static void raw_object_trace(const TZrChar *format, ...);
#else
#define raw_object_trace(...) ((void)0)
#endif

static ZR_FORCE_INLINE TZrBool garbage_collector_region_kind_uses_current_slot(
        EZrGarbageCollectRegionKind regionKind) {
    return regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_EDEN ||
           regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR ||
           regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_OLD;
}

static ZR_FORCE_INLINE TZrUInt32 *garbage_collector_current_region_id_slot(
        SZrGarbageCollector *collector,
        EZrGarbageCollectRegionKind regionKind) {
    if (collector == ZR_NULL) {
        return ZR_NULL;
    }

    switch (regionKind) {
        case ZR_GARBAGE_COLLECT_REGION_KIND_EDEN:
            return &collector->currentEdenRegionId;
        case ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR:
            return &collector->currentSurvivorRegionId;
        case ZR_GARBAGE_COLLECT_REGION_KIND_OLD:
            return &collector->currentOldRegionId;
        default:
            return ZR_NULL;
    }
}

static ZR_FORCE_INLINE TZrUInt64 *garbage_collector_current_region_used_slot(
        SZrGarbageCollector *collector,
        EZrGarbageCollectRegionKind regionKind) {
    if (collector == ZR_NULL) {
        return ZR_NULL;
    }

    switch (regionKind) {
        case ZR_GARBAGE_COLLECT_REGION_KIND_EDEN:
            return &collector->currentEdenRegionUsedBytes;
        case ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR:
            return &collector->currentSurvivorRegionUsedBytes;
        case ZR_GARBAGE_COLLECT_REGION_KIND_OLD:
            return &collector->currentOldRegionUsedBytes;
        default:
            return ZR_NULL;
    }
}

static ZR_FORCE_INLINE TZrSize *garbage_collector_current_region_index_slot(
        SZrGarbageCollector *collector,
        EZrGarbageCollectRegionKind regionKind) {
    if (collector == ZR_NULL) {
        return ZR_NULL;
    }

    switch (regionKind) {
        case ZR_GARBAGE_COLLECT_REGION_KIND_EDEN:
            return &collector->currentEdenRegionIndex;
        case ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR:
            return &collector->currentSurvivorRegionIndex;
        case ZR_GARBAGE_COLLECT_REGION_KIND_OLD:
            return &collector->currentOldRegionIndex;
        default:
            return ZR_NULL;
    }
}

static ZR_FORCE_INLINE void garbage_collector_current_region_cache_slots(
        SZrGarbageCollector *collector,
        EZrGarbageCollectRegionKind regionKind,
        TZrUInt32 **outRegionIdSlot,
        TZrSize **outRegionIndexSlot,
        TZrUInt64 **outUsedBytesSlot) {
    TZrUInt32 *regionIdSlot = ZR_NULL;
    TZrSize *regionIndexSlot = ZR_NULL;
    TZrUInt64 *usedBytesSlot = ZR_NULL;

    if (collector != ZR_NULL) {
        switch (regionKind) {
            case ZR_GARBAGE_COLLECT_REGION_KIND_EDEN:
                regionIdSlot = &collector->currentEdenRegionId;
                regionIndexSlot = &collector->currentEdenRegionIndex;
                usedBytesSlot = &collector->currentEdenRegionUsedBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR:
                regionIdSlot = &collector->currentSurvivorRegionId;
                regionIndexSlot = &collector->currentSurvivorRegionIndex;
                usedBytesSlot = &collector->currentSurvivorRegionUsedBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_OLD:
                regionIdSlot = &collector->currentOldRegionId;
                regionIndexSlot = &collector->currentOldRegionIndex;
                usedBytesSlot = &collector->currentOldRegionUsedBytes;
                break;
            default:
                break;
        }
    }

    if (outRegionIdSlot != ZR_NULL) {
        *outRegionIdSlot = regionIdSlot;
    }
    if (outRegionIndexSlot != ZR_NULL) {
        *outRegionIndexSlot = regionIndexSlot;
    }
    if (outUsedBytesSlot != ZR_NULL) {
        *outUsedBytesSlot = usedBytesSlot;
    }
}

static ZR_FORCE_INLINE TZrSize garbage_collector_region_descriptor_index(
        const SZrGarbageCollector *collector,
        const SZrGarbageCollectRegionDescriptor *region) {
    if (collector == ZR_NULL || collector->regions == ZR_NULL || region == ZR_NULL) {
        return ZR_MAX_SIZE;
    }

    if (region < collector->regions || region >= collector->regions + collector->regionCount) {
        return ZR_MAX_SIZE;
    }

    return (TZrSize)(region - collector->regions);
}

static SZrGarbageCollectRegionDescriptor *garbage_collector_find_region_descriptor(SZrGarbageCollector *collector,
                                                                                    TZrUInt32 regionId) {
    if (collector == ZR_NULL || collector->regions == ZR_NULL || regionId == 0u) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < collector->regionCount; index++) {
        if (collector->regions[index].id == regionId) {
            return &collector->regions[index];
        }
    }

    return ZR_NULL;
}

static SZrGarbageCollectRegionDescriptor *garbage_collector_find_region_descriptor_cached(
        SZrGarbageCollector *collector,
        TZrUInt32 regionId,
        TZrSize regionDescriptorIndex,
        TZrSize *outResolvedIndex) {
    SZrGarbageCollectRegionDescriptor *region;

    if (outResolvedIndex != ZR_NULL) {
        *outResolvedIndex = ZR_MAX_SIZE;
    }
    if (collector == ZR_NULL || collector->regions == ZR_NULL || regionId == 0u) {
        return ZR_NULL;
    }

    if (regionDescriptorIndex < collector->regionCount) {
        region = &collector->regions[regionDescriptorIndex];
        if (region->id == regionId) {
            if (outResolvedIndex != ZR_NULL) {
                *outResolvedIndex = regionDescriptorIndex;
            }
            return region;
        }
    }

    region = garbage_collector_find_region_descriptor(collector, regionId);
    if (outResolvedIndex != ZR_NULL) {
        *outResolvedIndex = garbage_collector_region_descriptor_index(collector, region);
    }
    return region;
}

static ZR_FORCE_INLINE void garbage_collector_clear_current_region_cache_slots(TZrUInt32 *regionIdSlot,
                                                                               TZrSize *regionIndexSlot,
                                                                               TZrUInt64 *usedBytesSlot) {
    if (regionIdSlot != ZR_NULL) {
        *regionIdSlot = 0u;
    }
    if (regionIndexSlot != ZR_NULL) {
        *regionIndexSlot = ZR_MAX_SIZE;
    }
    if (usedBytesSlot != ZR_NULL) {
        *usedBytesSlot = 0u;
    }
}

static ZR_FORCE_INLINE void garbage_collector_write_current_region_cache_slots(TZrUInt32 *regionIdSlot,
                                                                               TZrSize *regionIndexSlot,
                                                                               TZrUInt64 *usedBytesSlot,
                                                                               TZrUInt32 regionId,
                                                                               TZrSize regionIndex,
                                                                               TZrUInt64 usedBytes) {
    if (regionIdSlot != ZR_NULL) {
        *regionIdSlot = regionId;
    }
    if (regionIndexSlot != ZR_NULL) {
        *regionIndexSlot = regionIndex;
    }
    if (usedBytesSlot != ZR_NULL) {
        *usedBytesSlot = usedBytes;
    }
}

static ZR_FORCE_INLINE void garbage_collector_sync_current_region_cache(
        SZrGarbageCollector *collector,
        EZrGarbageCollectRegionKind regionKind,
        SZrGarbageCollectRegionDescriptor *region) {
    TZrUInt32 *regionIdSlot;
    TZrSize *regionIndexSlot;
    TZrUInt64 *usedBytesSlot;

    if (!garbage_collector_region_kind_uses_current_slot(regionKind)) {
        return;
    }

    garbage_collector_current_region_cache_slots(
            collector, regionKind, &regionIdSlot, &regionIndexSlot, &usedBytesSlot);
    if (regionIdSlot == ZR_NULL || regionIndexSlot == ZR_NULL || usedBytesSlot == ZR_NULL) {
        return;
    }

    if (region == ZR_NULL) {
        garbage_collector_clear_current_region_cache_slots(regionIdSlot, regionIndexSlot, usedBytesSlot);
        return;
    }

    *regionIndexSlot = garbage_collector_region_descriptor_index(collector, region);
    if (*regionIndexSlot == ZR_MAX_SIZE) {
        garbage_collector_clear_current_region_cache_slots(regionIdSlot, regionIndexSlot, usedBytesSlot);
        return;
    }

    garbage_collector_write_current_region_cache_slots(
            regionIdSlot, regionIndexSlot, usedBytesSlot, region->id, *regionIndexSlot, region->usedBytes);
}

static TZrBool garbage_collector_ensure_region_registry_capacity(SZrGlobalState *global, TZrSize minCapacity) {
    SZrGarbageCollector *collector;
    SZrGarbageCollectRegionDescriptor *newItems;
    TZrSize newCapacity;
    TZrSize oldBytes;
    TZrSize newBytes;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = global->garbageCollector;
    if (collector->regionCapacity >= minCapacity) {
        return ZR_TRUE;
    }

    newCapacity = collector->regionCapacity > 0
                      ? collector->regionCapacity
                      : ZR_GC_REGION_REGISTRY_INITIAL_CAPACITY;
    while (newCapacity < minCapacity) {
        newCapacity *= ZR_GC_REGION_REGISTRY_GROWTH_FACTOR;
    }

    newBytes = newCapacity * sizeof(SZrGarbageCollectRegionDescriptor);
    newItems = (SZrGarbageCollectRegionDescriptor *)ZrCore_Memory_RawMallocWithType(
            global, newBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (newItems == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(newItems, 0, newBytes);
    if (collector->regions != ZR_NULL && collector->regionCount > 0) {
        memcpy(newItems,
               collector->regions,
               collector->regionCount * sizeof(SZrGarbageCollectRegionDescriptor));
    }

    if (collector->regions != ZR_NULL) {
        oldBytes = collector->regionCapacity * sizeof(SZrGarbageCollectRegionDescriptor);
        ZrCore_Memory_RawFreeWithType(global, collector->regions, oldBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }

    collector->regions = newItems;
    collector->regionCapacity = newCapacity;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrUInt64 garbage_collector_old_region_required_capacity(
        const SZrGarbageCollector *collector,
        TZrUInt64 requestedBytes);

static TZrUInt64 garbage_collector_region_capacity(const SZrGarbageCollector *collector,
                                                   EZrGarbageCollectRegionKind regionKind,
                                                   TZrUInt64 requestedBytes) {
    TZrUInt64 youngCapacity;

    if (collector == ZR_NULL) {
        return 1u;
    }

    youngCapacity = collector->youngRegionSize > 0 ? collector->youngRegionSize : 1u;
    switch (regionKind) {
        case ZR_GARBAGE_COLLECT_REGION_KIND_EDEN:
        case ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR:
            return youngCapacity > requestedBytes ? youngCapacity : requestedBytes;
        case ZR_GARBAGE_COLLECT_REGION_KIND_OLD:
            return garbage_collector_old_region_required_capacity(collector, requestedBytes);
        case ZR_GARBAGE_COLLECT_REGION_KIND_PINNED:
        case ZR_GARBAGE_COLLECT_REGION_KIND_LARGE:
        case ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT:
        default:
            return requestedBytes > 0 ? requestedBytes : youngCapacity;
    }
}

static ZR_FORCE_INLINE TZrUInt64 garbage_collector_old_region_required_capacity(
        const SZrGarbageCollector *collector,
        TZrUInt64 requestedBytes) {
    TZrUInt64 youngCapacity;
    TZrUInt64 multiplier;
    TZrUInt64 oldCapacity;
    TZrUInt64 resolvedCapacity;

    if (collector == ZR_NULL) {
        return requestedBytes > 0 ? requestedBytes : 1u;
    }

    youngCapacity = collector->youngRegionSize > 0 ? collector->youngRegionSize : 1u;
    multiplier = collector->youngRegionCountTarget > 0 ? collector->youngRegionCountTarget : 1u;
    oldCapacity = youngCapacity * multiplier;
    resolvedCapacity = oldCapacity > 0 ? oldCapacity : youngCapacity;
    return resolvedCapacity > requestedBytes ? resolvedCapacity : requestedBytes;
}

static SZrGarbageCollectRegionDescriptor *garbage_collector_find_reusable_region_descriptor(
        SZrGarbageCollector *collector,
        EZrGarbageCollectRegionKind regionKind,
        TZrUInt64 requiredCapacity) {
    if (collector == ZR_NULL || collector->regions == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < collector->regionCount; index++) {
        SZrGarbageCollectRegionDescriptor *region = &collector->regions[index];

        if (region->kind == regionKind &&
            region->liveObjectCount == 0u &&
            region->capacityBytes >= requiredCapacity) {
            return region;
        }
    }

    return ZR_NULL;
}

static SZrGarbageCollectRegionDescriptor *garbage_collector_create_region_descriptor(
        SZrGlobalState *global,
        EZrGarbageCollectRegionKind regionKind,
        TZrUInt64 capacityBytes) {
    SZrGarbageCollector *collector;
    SZrGarbageCollectRegionDescriptor *region;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return ZR_NULL;
    }

    collector = global->garbageCollector;
    if (!garbage_collector_ensure_region_registry_capacity(global, collector->regionCount + 1)) {
        return ZR_NULL;
    }

    region = &collector->regions[collector->regionCount++];
    memset(region, 0, sizeof(*region));
    region->id = collector->nextRegionId++;
    region->kind = regionKind;
    region->capacityBytes = capacityBytes > 0 ? capacityBytes : 1u;
    return region;
}

TZrUInt32 garbage_collector_allocate_region_id_cached(SZrGlobalState *global,
                                                      EZrGarbageCollectRegionKind regionKind,
                                                      TZrSize objectSize,
                                                      TZrSize *outRegionDescriptorIndex) {
    SZrGarbageCollector *collector;
    SZrGarbageCollectRegionDescriptor *region = ZR_NULL;
    TZrUInt32 *regionIdSlot;
    TZrUInt64 *usedBytesSlot;
    TZrSize *regionIndexSlot;
    TZrSize regionDescriptorIndex = ZR_MAX_SIZE;
    TZrUInt64 requestedBytes;
    TZrUInt64 updatedUsedBytes;

    if (outRegionDescriptorIndex != ZR_NULL) {
        *outRegionDescriptorIndex = ZR_MAX_SIZE;
    }
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return 0u;
    }

    collector = global->garbageCollector;
    requestedBytes = objectSize > 0 ? (TZrUInt64)objectSize : 1u;
    garbage_collector_current_region_cache_slots(collector, regionKind, &regionIdSlot, &regionIndexSlot, &usedBytesSlot);

    if (regionIdSlot != ZR_NULL && usedBytesSlot != ZR_NULL && regionIndexSlot != ZR_NULL && *regionIdSlot != 0u &&
        *regionIndexSlot < collector->regionCount) {
        region = &collector->regions[*regionIndexSlot];
        if (region == ZR_NULL ||
            region->id != *regionIdSlot ||
            region->kind != regionKind ||
            *usedBytesSlot + requestedBytes > region->capacityBytes) {
            garbage_collector_clear_current_region_cache_slots(regionIdSlot, regionIndexSlot, usedBytesSlot);
            region = ZR_NULL;
        }
        if (region != ZR_NULL) {
            updatedUsedBytes = region->usedBytes + requestedBytes;
            region->usedBytes = updatedUsedBytes;
            region->liveBytes += requestedBytes;
            region->liveObjectCount++;
            *usedBytesSlot = updatedUsedBytes;
            if (outRegionDescriptorIndex != ZR_NULL) {
                *outRegionDescriptorIndex = *regionIndexSlot;
            }
            return region->id;
        }
    }

    if (region == ZR_NULL && regionIdSlot != ZR_NULL && usedBytesSlot != ZR_NULL && regionIndexSlot != ZR_NULL &&
        *regionIdSlot != 0u) {
        region = garbage_collector_find_region_descriptor_cached(
                collector, *regionIdSlot, *regionIndexSlot, &regionDescriptorIndex);
        if (region == ZR_NULL ||
            region->kind != regionKind ||
            region->usedBytes + requestedBytes > region->capacityBytes) {
            garbage_collector_clear_current_region_cache_slots(regionIdSlot, regionIndexSlot, usedBytesSlot);
            region = ZR_NULL;
            regionDescriptorIndex = ZR_MAX_SIZE;
        } else {
            updatedUsedBytes = region->usedBytes + requestedBytes;
            region->usedBytes = updatedUsedBytes;
            region->liveBytes += requestedBytes;
            region->liveObjectCount++;
            garbage_collector_write_current_region_cache_slots(
                    regionIdSlot, regionIndexSlot, usedBytesSlot, region->id, regionDescriptorIndex, updatedUsedBytes);
            if (outRegionDescriptorIndex != ZR_NULL) {
                *outRegionDescriptorIndex = regionDescriptorIndex;
            }
            return region->id;
        }
    }

    if (region == ZR_NULL) {
        TZrUInt64 requiredCapacity = garbage_collector_region_capacity(collector, regionKind, requestedBytes);

        region = garbage_collector_find_reusable_region_descriptor(collector, regionKind, requiredCapacity);
        if (region == ZR_NULL) {
            region = garbage_collector_create_region_descriptor(global, regionKind, requiredCapacity);
        }
        if (region == ZR_NULL) {
            return 0u;
        }

        regionDescriptorIndex = garbage_collector_region_descriptor_index(collector, region);
        region->kind = regionKind;
        region->capacityBytes = region->capacityBytes > requiredCapacity ? region->capacityBytes : requiredCapacity;
        region->usedBytes = 0u;
        region->liveBytes = 0u;
        region->liveObjectCount = 0u;
    }

    updatedUsedBytes = region->usedBytes + requestedBytes;
    region->usedBytes = updatedUsedBytes;
    region->liveBytes += requestedBytes;
    region->liveObjectCount++;
    if (regionIdSlot != ZR_NULL && usedBytesSlot != ZR_NULL && regionIndexSlot != ZR_NULL &&
        regionDescriptorIndex != ZR_MAX_SIZE) {
        garbage_collector_write_current_region_cache_slots(
                regionIdSlot, regionIndexSlot, usedBytesSlot, region->id, regionDescriptorIndex, updatedUsedBytes);
    } else {
        garbage_collector_sync_current_region_cache(collector, regionKind, region);
    }
    if (regionDescriptorIndex == ZR_MAX_SIZE) {
        regionDescriptorIndex = garbage_collector_region_descriptor_index(collector, region);
    }
    if (outRegionDescriptorIndex != ZR_NULL) {
        *outRegionDescriptorIndex = regionDescriptorIndex;
    }
    return region->id;
}

TZrUInt32 garbage_collector_allocate_old_region_id_cached(SZrGlobalState *global,
                                                          TZrSize objectSize,
                                                          TZrSize *outRegionDescriptorIndex) {
    SZrGarbageCollector *collector;
    SZrGarbageCollectRegionDescriptor *region = ZR_NULL;
    TZrSize regionDescriptorIndex = ZR_MAX_SIZE;
    TZrUInt64 requestedBytes;
    TZrUInt64 updatedUsedBytes;
    TZrUInt64 requiredCapacity;

    if (outRegionDescriptorIndex != ZR_NULL) {
        *outRegionDescriptorIndex = ZR_MAX_SIZE;
    }
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return 0u;
    }

    collector = global->garbageCollector;
    requestedBytes = objectSize > 0 ? (TZrUInt64)objectSize : 1u;

    if (collector->currentOldRegionId != 0u &&
        collector->currentOldRegionIndex < collector->regionCount) {
        region = &collector->regions[collector->currentOldRegionIndex];
        if (region->id != collector->currentOldRegionId ||
            region->kind != ZR_GARBAGE_COLLECT_REGION_KIND_OLD ||
            collector->currentOldRegionUsedBytes + requestedBytes > region->capacityBytes) {
            garbage_collector_clear_current_region_cache_slots(&collector->currentOldRegionId,
                                                               &collector->currentOldRegionIndex,
                                                               &collector->currentOldRegionUsedBytes);
            region = ZR_NULL;
        }
        if (region != ZR_NULL) {
            updatedUsedBytes = region->usedBytes + requestedBytes;
            region->usedBytes = updatedUsedBytes;
            region->liveBytes += requestedBytes;
            region->liveObjectCount++;
            collector->currentOldRegionUsedBytes = updatedUsedBytes;
            if (outRegionDescriptorIndex != ZR_NULL) {
                *outRegionDescriptorIndex = collector->currentOldRegionIndex;
            }
            return region->id;
        }
    }

    if (collector->currentOldRegionId != 0u) {
        region = garbage_collector_find_region_descriptor_cached(
                collector,
                collector->currentOldRegionId,
                collector->currentOldRegionIndex,
                &regionDescriptorIndex);
        if (region == ZR_NULL ||
            region->kind != ZR_GARBAGE_COLLECT_REGION_KIND_OLD ||
            region->usedBytes + requestedBytes > region->capacityBytes) {
            garbage_collector_clear_current_region_cache_slots(&collector->currentOldRegionId,
                                                               &collector->currentOldRegionIndex,
                                                               &collector->currentOldRegionUsedBytes);
            region = ZR_NULL;
            regionDescriptorIndex = ZR_MAX_SIZE;
        } else {
            updatedUsedBytes = region->usedBytes + requestedBytes;
            region->usedBytes = updatedUsedBytes;
            region->liveBytes += requestedBytes;
            region->liveObjectCount++;
            garbage_collector_write_current_region_cache_slots(&collector->currentOldRegionId,
                                                               &collector->currentOldRegionIndex,
                                                               &collector->currentOldRegionUsedBytes,
                                                               region->id,
                                                               regionDescriptorIndex,
                                                               updatedUsedBytes);
            if (outRegionDescriptorIndex != ZR_NULL) {
                *outRegionDescriptorIndex = regionDescriptorIndex;
            }
            return region->id;
        }
    }

    requiredCapacity = garbage_collector_old_region_required_capacity(collector, requestedBytes);
    region = garbage_collector_find_reusable_region_descriptor(
            collector, ZR_GARBAGE_COLLECT_REGION_KIND_OLD, requiredCapacity);
    if (region == ZR_NULL) {
        region = garbage_collector_create_region_descriptor(
                global, ZR_GARBAGE_COLLECT_REGION_KIND_OLD, requiredCapacity);
    }
    if (region == ZR_NULL) {
        return 0u;
    }

    regionDescriptorIndex = garbage_collector_region_descriptor_index(collector, region);
    region->kind = ZR_GARBAGE_COLLECT_REGION_KIND_OLD;
    region->capacityBytes = region->capacityBytes > requiredCapacity ? region->capacityBytes : requiredCapacity;
    region->usedBytes = 0u;
    region->liveBytes = 0u;
    region->liveObjectCount = 0u;

    updatedUsedBytes = region->usedBytes + requestedBytes;
    region->usedBytes = updatedUsedBytes;
    region->liveBytes += requestedBytes;
    region->liveObjectCount++;
    if (regionDescriptorIndex != ZR_MAX_SIZE) {
        garbage_collector_write_current_region_cache_slots(&collector->currentOldRegionId,
                                                           &collector->currentOldRegionIndex,
                                                           &collector->currentOldRegionUsedBytes,
                                                           region->id,
                                                           regionDescriptorIndex,
                                                           updatedUsedBytes);
    } else {
        garbage_collector_sync_current_region_cache(
                collector, ZR_GARBAGE_COLLECT_REGION_KIND_OLD, region);
    }
    if (regionDescriptorIndex == ZR_MAX_SIZE) {
        regionDescriptorIndex = garbage_collector_region_descriptor_index(collector, region);
    }
    if (outRegionDescriptorIndex != ZR_NULL) {
        *outRegionDescriptorIndex = regionDescriptorIndex;
    }
    return region->id;
}

TZrUInt32 garbage_collector_allocate_region_id(SZrGlobalState *global,
                                               EZrGarbageCollectRegionKind regionKind,
                                               TZrSize objectSize) {
    return garbage_collector_allocate_region_id_cached(global, regionKind, objectSize, ZR_NULL);
}

void garbage_collector_release_region_allocation_cached(SZrGlobalState *global,
                                                        TZrUInt32 regionId,
                                                        TZrSize regionDescriptorIndex,
                                                        TZrSize objectSize) {
    SZrGarbageCollector *collector;
    SZrGarbageCollectRegionDescriptor *region;
    TZrUInt64 releasedBytes;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || regionId == 0u) {
        return;
    }

    collector = global->garbageCollector;
    region = garbage_collector_find_region_descriptor_cached(
            collector, regionId, regionDescriptorIndex, ZR_NULL);
    if (region == ZR_NULL) {
        return;
    }

    releasedBytes = objectSize > 0 ? (TZrUInt64)objectSize : 1u;
    if (region->liveBytes <= releasedBytes) {
        region->liveBytes = 0u;
    } else {
        region->liveBytes -= releasedBytes;
    }

    if (region->liveObjectCount > 0u) {
        region->liveObjectCount--;
    }

    if (region->liveObjectCount == 0u) {
        region->usedBytes = 0u;
        region->liveBytes = 0u;
        if (garbage_collector_region_kind_uses_current_slot(region->kind)) {
            TZrUInt32 *currentRegionId = garbage_collector_current_region_id_slot(collector, region->kind);
            TZrUInt64 *currentRegionUsedBytes = garbage_collector_current_region_used_slot(collector, region->kind);
            TZrSize *currentRegionIndex = garbage_collector_current_region_index_slot(collector, region->kind);

            if (currentRegionId != ZR_NULL && *currentRegionId == region->id) {
                garbage_collector_clear_current_region_cache_slots(
                        currentRegionId, currentRegionIndex, currentRegionUsedBytes);
            }
        }
    }
}

void garbage_collector_release_region_allocation(SZrGlobalState *global, TZrUInt32 regionId, TZrSize objectSize) {
    garbage_collector_release_region_allocation_cached(global, regionId, ZR_MAX_SIZE, objectSize);
}

TZrUInt32 garbage_collector_reassign_region_id_cached(SZrGlobalState *global,
                                                      TZrUInt32 previousRegionId,
                                                      TZrSize previousRegionDescriptorIndex,
                                                      EZrGarbageCollectRegionKind newRegionKind,
                                                      TZrSize objectSize,
                                                      TZrSize *outRegionDescriptorIndex) {
    TZrUInt32 regionId;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        if (outRegionDescriptorIndex != ZR_NULL) {
            *outRegionDescriptorIndex = ZR_MAX_SIZE;
        }
        return 0u;
    }

    if (!garbage_collector_try_release_region_allocation_fast(global->garbageCollector,
                                                              previousRegionId,
                                                              previousRegionDescriptorIndex,
                                                              objectSize)) {
        garbage_collector_release_region_allocation_cached(
                global, previousRegionId, previousRegionDescriptorIndex, objectSize);
    }

    regionId = garbage_collector_try_allocate_region_id_current_fast(
            global->garbageCollector, newRegionKind, objectSize, outRegionDescriptorIndex);
    if (regionId != 0u) {
        return regionId;
    }

    return garbage_collector_allocate_region_id_cached(
            global, newRegionKind, objectSize, outRegionDescriptorIndex);
}

TZrUInt32 garbage_collector_reassign_region_id(SZrGlobalState *global,
                                               TZrUInt32 previousRegionId,
                                               EZrGarbageCollectRegionKind newRegionKind,
                                               TZrSize objectSize) {
    return garbage_collector_reassign_region_id_cached(
            global, previousRegionId, ZR_MAX_SIZE, newRegionKind, objectSize, ZR_NULL);
}

TZrBool garbage_collector_ensure_ignore_registry_capacity(SZrGlobalState *global, TZrSize minCapacity) {
    SZrGarbageCollector *collector;
    SZrRawObject **newItems;
    TZrSize newCapacity;
    TZrSize oldBytes;
    TZrSize newBytes;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = global->garbageCollector;
    if (collector->ignoredObjectCapacity >= minCapacity) {
        return ZR_TRUE;
    }

    newCapacity = collector->ignoredObjectCapacity > 0
                      ? collector->ignoredObjectCapacity
                      : ZR_GC_IGNORE_REGISTRY_INITIAL_CAPACITY;
    while (newCapacity < minCapacity) {
        newCapacity *= ZR_GC_IGNORE_REGISTRY_GROWTH_FACTOR;
    }

    newBytes = newCapacity * sizeof(SZrRawObject *);
    newItems = (SZrRawObject **)ZrCore_Memory_RawMallocWithType(global, newBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (newItems == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(newItems, 0, newBytes);
    if (collector->ignoredObjects != ZR_NULL && collector->ignoredObjectCount > 0) {
        memcpy(newItems,
               collector->ignoredObjects,
               collector->ignoredObjectCount * sizeof(SZrRawObject *));
    }

    if (collector->ignoredObjects != ZR_NULL) {
        oldBytes = collector->ignoredObjectCapacity * sizeof(SZrRawObject *);
        ZrCore_Memory_RawFreeWithType(global,
                                      collector->ignoredObjects,
                                      oldBytes,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }

    collector->ignoredObjects = newItems;
    collector->ignoredObjectCapacity = newCapacity;
    return ZR_TRUE;
}

static void garbage_collector_ignore_registry_remove_object(SZrGarbageCollector *collector, SZrRawObject *object) {
    TZrSize index;
    TZrSize lastIndex;
    SZrRawObject *movedObject;

    if (collector == ZR_NULL || object == ZR_NULL || collector->ignoredObjects == ZR_NULL) {
        if (object != ZR_NULL) {
            object->garbageCollectMark.ignoredRegistryIndex = ZR_MAX_SIZE;
        }
        return;
    }

    index = object->garbageCollectMark.ignoredRegistryIndex;
    if (index >= collector->ignoredObjectCount || collector->ignoredObjects[index] != object) {
        object->garbageCollectMark.ignoredRegistryIndex = ZR_MAX_SIZE;
        return;
    }

    lastIndex = collector->ignoredObjectCount - 1u;
    movedObject = collector->ignoredObjects[lastIndex];
    collector->ignoredObjects[lastIndex] = ZR_NULL;
    collector->ignoredObjectCount = lastIndex;

    if (index != lastIndex) {
        collector->ignoredObjects[index] = movedObject;
        if (movedObject != ZR_NULL) {
            movedObject->garbageCollectMark.ignoredRegistryIndex = index;
        }
    }

    object->garbageCollectMark.ignoredRegistryIndex = ZR_MAX_SIZE;
}

static void garbage_collector_remembered_registry_remove_object(SZrGarbageCollector *collector, SZrRawObject *object) {
    TZrSize index;
    TZrSize lastIndex;
    SZrRawObject *movedObject;

    if (collector == ZR_NULL || object == ZR_NULL || collector->rememberedObjects == ZR_NULL) {
        if (object != ZR_NULL) {
            object->garbageCollectMark.rememberedRegistryIndex = ZR_MAX_SIZE;
        }
        return;
    }

    index = object->garbageCollectMark.rememberedRegistryIndex;
    if (index >= collector->rememberedObjectCount || collector->rememberedObjects[index] != object) {
        object->garbageCollectMark.rememberedRegistryIndex = ZR_MAX_SIZE;
        return;
    }

    lastIndex = collector->rememberedObjectCount - 1u;
    movedObject = collector->rememberedObjects[lastIndex];
    collector->rememberedObjects[lastIndex] = ZR_NULL;
    collector->rememberedObjectCount = lastIndex;

    if (index != lastIndex) {
        collector->rememberedObjects[index] = movedObject;
        if (movedObject != ZR_NULL) {
            movedObject->garbageCollectMark.rememberedRegistryIndex = index;
        }
    }

    object->garbageCollectMark.rememberedRegistryIndex = ZR_MAX_SIZE;
}

void garbage_collector_forget_object_from_registries(SZrGarbageCollector *collector, SZrRawObject *object) {
    if (collector == ZR_NULL || object == ZR_NULL) {
        return;
    }
    if (object->garbageCollectMark.ignoredRegistryIndex == ZR_MAX_SIZE &&
        object->garbageCollectMark.rememberedRegistryIndex == ZR_MAX_SIZE) {
        return;
    }

    garbage_collector_ignore_registry_remove_object(collector, object);
    garbage_collector_remembered_registry_remove_object(collector, object);
    collector->statsSnapshot.ignoredObjectCount = (TZrUInt32)collector->ignoredObjectCount;
    collector->statsSnapshot.rememberedObjectCount = (TZrUInt32)collector->rememberedObjectCount;
}

TZrBool garbage_collector_ensure_remembered_registry_capacity(SZrGlobalState *global, TZrSize minCapacity) {
    SZrGarbageCollector *collector;
    SZrRawObject **newItems;
    TZrSize newCapacity;
    TZrSize oldBytes;
    TZrSize newBytes;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = global->garbageCollector;
    if (collector->rememberedObjectCapacity >= minCapacity) {
        return ZR_TRUE;
    }

    newCapacity = collector->rememberedObjectCapacity > 0
                      ? collector->rememberedObjectCapacity
                      : ZR_GC_REMEMBERED_REGISTRY_INITIAL_CAPACITY;
    while (newCapacity < minCapacity) {
        newCapacity *= ZR_GC_REMEMBERED_REGISTRY_GROWTH_FACTOR;
    }

    newBytes = newCapacity * sizeof(SZrRawObject *);
    newItems = (SZrRawObject **)ZrCore_Memory_RawMallocWithType(global, newBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (newItems == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(newItems, 0, newBytes);
    if (collector->rememberedObjects != ZR_NULL && collector->rememberedObjectCount > 0) {
        memcpy(newItems,
               collector->rememberedObjects,
               collector->rememberedObjectCount * sizeof(SZrRawObject *));
    }

    if (collector->rememberedObjects != ZR_NULL) {
        oldBytes = collector->rememberedObjectCapacity * sizeof(SZrRawObject *);
        ZrCore_Memory_RawFreeWithType(global,
                                      collector->rememberedObjects,
                                      oldBytes,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }

    collector->rememberedObjects = newItems;
    collector->rememberedObjectCapacity = newCapacity;
    collector->statsSnapshot.rememberedObjectCount = (TZrUInt32)collector->rememberedObjectCount;
    return ZR_TRUE;
}

TZrSize garbage_collector_get_object_base_size(SZrState *state, SZrRawObject *object) {
    ZR_UNUSED_PARAMETER(state);

    if (object == ZR_NULL) {
        return 0;
    }

    if ((TZrPtr)object < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        return 0;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return 0;
    }

    return garbage_collector_get_object_base_size_fast(object);
}

ZR_CORE_API TZrSize ZrCore_GarbageCollector_GetObjectBaseSize(SZrState *state, SZrRawObject *object) {
    return garbage_collector_get_object_base_size(state, object);
}

static ZR_FORCE_INLINE TZrBool garbage_collector_free_object_validate(
        SZrState *state,
        SZrRawObject *object,
        SZrGlobalState **outGlobal) {
    SZrGlobalState *global;

    if (outGlobal != ZR_NULL) {
        *outGlobal = ZR_NULL;
    }
    if (object == ZR_NULL || state == ZR_NULL) {
        return ZR_FALSE;
    }
    if ((TZrPtr)object < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        return ZR_FALSE;
    }

    global = state->global;
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return ZR_FALSE;
    }

    if (outGlobal != ZR_NULL) {
        *outGlobal = global;
    }
    return ZR_TRUE;
}

static ZR_FORCE_INLINE void garbage_collector_free_object_known_size(
        SZrState *state,
        SZrGlobalState *global,
        SZrRawObject *object,
        TZrSize objectSize) {
    EZrGarbageCollectIncrementalObjectStatus oldStatus;

    oldStatus = object->garbageCollectMark.status;
    if (oldStatus == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED) {
        return;
    }

    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
    if (!garbage_collector_try_release_region_allocation_fast(global->garbageCollector,
                                                              object->garbageCollectMark.regionId,
                                                              object->garbageCollectMark.regionDescriptorIndex,
                                                              objectSize)) {
        garbage_collector_release_region_allocation_cached(global,
                                                           object->garbageCollectMark.regionId,
                                                           object->garbageCollectMark.regionDescriptorIndex,
                                                           objectSize);
    }
    object->garbageCollectMark.regionDescriptorIndex = ZR_MAX_SIZE;
    if (object->garbageCollectMark.ignoredRegistryIndex != ZR_MAX_SIZE ||
        object->garbageCollectMark.rememberedRegistryIndex != ZR_MAX_SIZE) {
        garbage_collector_forget_object_from_registries(global->garbageCollector, object);
    }

    if (object->ownershipControl != ZR_NULL) {
        ZrCore_Ownership_NotifyObjectReleased(state, object);
    }

    if (object->type == ZR_RAW_OBJECT_TYPE_NATIVE_DATA &&
        object->scanMarkGcFunction != ZR_NULL) {
        object->scanMarkGcFunction(state, object);
    }

    ZrCore_Memory_RawFreeWithType(global, object, objectSize, ZR_MEMORY_NATIVE_TYPE_OBJECT);
}

void garbage_collector_free_object_sized(SZrState *state, SZrRawObject *object, TZrSize objectSize) {
    SZrGlobalState *global;

    if (objectSize == 0u ||
        !garbage_collector_free_object_validate(state, object, &global)) {
        return;
    }

    garbage_collector_free_object_known_size(state, global, object, objectSize);
}

void garbage_collector_free_object(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global;
    TZrSize objectSize;

    if (!garbage_collector_free_object_validate(state, object, &global)) {
        return;
    }

    objectSize = garbage_collector_get_object_base_size_fast(object);
    if (objectSize == 0u) {
        return;
    }

    garbage_collector_free_object_known_size(state, global, object, objectSize);
}

SZrRawObject *ZrCore_RawObject_New(SZrState *state, EZrValueType type, TZrSize size, TZrBool isNative) {
    SZrGlobalState *global = state->global;
    TZrPtr memory = ZrCore_Memory_GcMalloc(state, ZR_MEMORY_NATIVE_TYPE_OBJECT, size);
    SZrRawObject *object = ZR_CAST_RAW_OBJECT(memory);
    raw_object_trace("raw object new request state=%p type=%d size=%llu memory=%p",
                     (void *)state,
                     (int)type,
                     (unsigned long long)size,
                     memory);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(object, 0, size);
    ZrCore_RawObject_Construct(object, (EZrRawObjectType)type);
    object->isNative = isNative;
    object->garbageCollectMark.status = global->garbageCollector->gcInitializeObjectStatus;
    object->garbageCollectMark.generation = global->garbageCollector->gcGeneration;
    object->garbageCollectMark.regionId = garbage_collector_try_allocate_region_id_current_fast(
            global->garbageCollector,
            object->garbageCollectMark.regionKind,
            size,
            &object->garbageCollectMark.regionDescriptorIndex);
    if (object->garbageCollectMark.regionId == 0u) {
        object->garbageCollectMark.regionId =
                garbage_collector_allocate_region_id_cached(global,
                                                            object->garbageCollectMark.regionKind,
                                                            size,
                                                            &object->garbageCollectMark.regionDescriptorIndex);
    }
    object->next = global->garbageCollector->gcObjectList;
    global->garbageCollector->gcObjectList = object;
    raw_object_trace("raw object new done object=%p gcListNext=%p gcHead=%p",
                     (void *)object,
                     (void *)object->next,
                     (void *)global->garbageCollector->gcObjectList);
    return object;
}

SZrRawObject *garbage_collector_new_raw_object_in_region(SZrState *state,
                                                         EZrValueType type,
                                                         TZrSize size,
                                                         TZrBool isNative,
                                                         EZrGarbageCollectRegionKind regionKind,
                                                         EZrGarbageCollectStorageKind storageKind) {
    SZrGlobalState *global;
    TZrPtr memory;
    SZrRawObject *object;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    memory = ZrCore_Memory_GcMalloc(state, ZR_MEMORY_NATIVE_TYPE_OBJECT, size);
    object = ZR_CAST_RAW_OBJECT(memory);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(object, 0, size);
    ZrCore_RawObject_Construct(object, (EZrRawObjectType)type);
    object->isNative = isNative;
    object->garbageCollectMark.status = global->garbageCollector->gcInitializeObjectStatus;
    object->garbageCollectMark.generation = global->garbageCollector->gcGeneration;
    ZrCore_RawObject_SetStorageKind(object, storageKind);
    ZrCore_RawObject_SetRegionKind(object, regionKind);
    object->garbageCollectMark.regionId = garbage_collector_try_allocate_region_id_current_fast(
            global->garbageCollector, regionKind, size, &object->garbageCollectMark.regionDescriptorIndex);
    if (object->garbageCollectMark.regionId == 0u) {
        object->garbageCollectMark.regionId = garbage_collector_allocate_region_id_cached(
                global, regionKind, size, &object->garbageCollectMark.regionDescriptorIndex);
    }
    object->next = global->garbageCollector->gcObjectList;
    global->garbageCollector->gcObjectList = object;
    return object;
}

TZrBool ZrCore_RawObject_IsUnreferenced(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global;
    EZrGarbageCollectIncrementalObjectStatus status;
    EZrGarbageCollectGeneration generation;

    if (object == ZR_NULL || state == ZR_NULL) {
        return ZR_TRUE;
    }

    if ((TZrPtr)object < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        return ZR_TRUE;
    }

    global = state->global;
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return ZR_TRUE;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return ZR_TRUE;
    }

    status = object->garbageCollectMark.status;
    generation = object->garbageCollectMark.generation;
    if (status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED &&
        status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED &&
        (status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED ||
         generation == global->garbageCollector->gcGeneration)) {
        return ZR_FALSE;
    }

    if (object->garbageCollectMark.ignoredRegistryIndex == ZR_MAX_SIZE) {
        return ZR_TRUE;
    }

    return !garbage_collector_ignore_registry_contains(global->garbageCollector, object);
}

void ZrCore_RawObject_MarkAsInit(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global = state->global;
    EZrGarbageCollectGeneration generation = global->garbageCollector->gcGeneration;

    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
    object->garbageCollectMark.generation = generation;
}

void ZrCore_RawObject_MarkAsPermanent(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global = state->global;
    TZrSize objectSize;
    TZrUInt32 previousRegionId;
#if defined(ZR_DEBUG)
    EZrGarbageCollectIncrementalObjectStatus previousStatus = object->garbageCollectMark.status;
#endif

    if (object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT) {
        return;
    }

    ZR_ASSERT(object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED ||
              object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN ||
              object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED);

    objectSize = garbage_collector_get_object_base_size_fast(object);
    previousRegionId = object->garbageCollectMark.regionId;
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT;
    ZrCore_RawObject_SetStorageKind(object, ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT);
    ZrCore_RawObject_SetRegionKind(object, ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT);
    object->garbageCollectMark.regionId = garbage_collector_reassign_region_id_cached(
            global,
            previousRegionId,
            object->garbageCollectMark.regionDescriptorIndex,
            ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT,
            objectSize,
            &object->garbageCollectMark.regionDescriptorIndex);
    object->garbageCollectMark.pinFlags |= ZR_GARBAGE_COLLECT_PIN_KIND_PERSISTENT_ROOT;
    object->garbageCollectMark.escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT;
    object->garbageCollectMark.promotionReason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_GLOBAL_ROOT;
    raw_object_trace("raw object permanent object=%p status=%d region=%u",
                     (void *)object,
                     (int)previousStatus,
                     (unsigned int)object->garbageCollectMark.regionId);
}

void ZrCore_Gc_ValueStaticAssertIsAlive(SZrState *state, SZrTypeValue *value) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(value);
    ZR_ASSERT(!value->isGarbageCollectable ||
              ((value->type == (EZrValueType)value->value.object->type) &&
               ((state == ZR_NULL) || !ZrCore_Gc_RawObjectIsDead(state->global, value->value.object))));
}

#if defined(ZR_DEBUG)
static TZrBool raw_object_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_CORE_BOOTSTRAP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void raw_object_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!raw_object_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-raw-object] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}
#endif
