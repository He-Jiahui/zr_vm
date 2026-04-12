//
// Object lifecycle and raw-object helpers for the GC.
//

#include "gc_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define ZR_GC_IGNORE_REGISTRY_INITIAL_CAPACITY ((TZrSize)8)
#define ZR_GC_IGNORE_REGISTRY_GROWTH_FACTOR ((TZrSize)2)
#define ZR_GC_REMEMBERED_REGISTRY_INITIAL_CAPACITY ((TZrSize)8)
#define ZR_GC_REMEMBERED_REGISTRY_GROWTH_FACTOR ((TZrSize)2)
#define ZR_GC_REGION_REGISTRY_INITIAL_CAPACITY ((TZrSize)8)
#define ZR_GC_REGION_REGISTRY_GROWTH_FACTOR ((TZrSize)2)

static TZrBool raw_object_trace_enabled(void);
static void raw_object_trace(const TZrChar *format, ...);

static TZrBool garbage_collector_region_kind_uses_current_slot(EZrGarbageCollectRegionKind regionKind) {
    return regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_EDEN ||
           regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR ||
           regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_OLD;
}

static TZrUInt32 *garbage_collector_current_region_id_slot(SZrGarbageCollector *collector,
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

static TZrUInt64 *garbage_collector_current_region_used_slot(SZrGarbageCollector *collector,
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

static void garbage_collector_sync_current_region_cache(SZrGarbageCollector *collector,
                                                        EZrGarbageCollectRegionKind regionKind,
                                                        SZrGarbageCollectRegionDescriptor *region) {
    TZrUInt32 *regionIdSlot;
    TZrUInt64 *usedBytesSlot;

    if (!garbage_collector_region_kind_uses_current_slot(regionKind)) {
        return;
    }

    regionIdSlot = garbage_collector_current_region_id_slot(collector, regionKind);
    usedBytesSlot = garbage_collector_current_region_used_slot(collector, regionKind);
    if (regionIdSlot == ZR_NULL || usedBytesSlot == ZR_NULL) {
        return;
    }

    if (region == ZR_NULL) {
        *regionIdSlot = 0u;
        *usedBytesSlot = 0u;
        return;
    }

    *regionIdSlot = region->id;
    *usedBytesSlot = region->usedBytes;
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
        case ZR_GARBAGE_COLLECT_REGION_KIND_OLD: {
            TZrUInt64 multiplier = collector->youngRegionCountTarget > 0 ? collector->youngRegionCountTarget : 1u;
            TZrUInt64 oldCapacity = youngCapacity * multiplier;
            TZrUInt64 resolvedCapacity = oldCapacity > 0 ? oldCapacity : youngCapacity;
            return resolvedCapacity > requestedBytes ? resolvedCapacity : requestedBytes;
        }
        case ZR_GARBAGE_COLLECT_REGION_KIND_PINNED:
        case ZR_GARBAGE_COLLECT_REGION_KIND_LARGE:
        case ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT:
        default:
            return requestedBytes > 0 ? requestedBytes : youngCapacity;
    }
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

TZrUInt32 garbage_collector_allocate_region_id(SZrGlobalState *global,
                                               EZrGarbageCollectRegionKind regionKind,
                                               TZrSize objectSize) {
    SZrGarbageCollector *collector;
    SZrGarbageCollectRegionDescriptor *region = ZR_NULL;
    TZrUInt32 *regionIdSlot;
    TZrUInt64 requestedBytes;
    TZrUInt64 requiredCapacity;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return 0u;
    }

    collector = global->garbageCollector;
    requestedBytes = objectSize > 0 ? (TZrUInt64)objectSize : 1u;
    requiredCapacity = garbage_collector_region_capacity(collector, regionKind, requestedBytes);
    regionIdSlot = garbage_collector_current_region_id_slot(collector, regionKind);

    if (regionIdSlot != ZR_NULL && *regionIdSlot != 0u) {
        region = garbage_collector_find_region_descriptor(collector, *regionIdSlot);
        if (region == ZR_NULL ||
            region->kind != regionKind ||
            region->usedBytes + requestedBytes > region->capacityBytes) {
            garbage_collector_sync_current_region_cache(collector, regionKind, ZR_NULL);
            region = ZR_NULL;
        }
    }

    if (region == ZR_NULL) {
        region = garbage_collector_find_reusable_region_descriptor(collector, regionKind, requiredCapacity);
        if (region == ZR_NULL) {
            region = garbage_collector_create_region_descriptor(global, regionKind, requiredCapacity);
        }
        if (region == ZR_NULL) {
            return 0u;
        }

        region->kind = regionKind;
        region->capacityBytes = region->capacityBytes > requiredCapacity ? region->capacityBytes : requiredCapacity;
        region->usedBytes = 0u;
        region->liveBytes = 0u;
        region->liveObjectCount = 0u;
    }

    region->usedBytes += requestedBytes;
    region->liveBytes += requestedBytes;
    region->liveObjectCount++;
    garbage_collector_sync_current_region_cache(collector, regionKind, region);
    return region->id;
}

void garbage_collector_release_region_allocation(SZrGlobalState *global, TZrUInt32 regionId, TZrSize objectSize) {
    SZrGarbageCollector *collector;
    SZrGarbageCollectRegionDescriptor *region;
    TZrUInt64 releasedBytes;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || regionId == 0u) {
        return;
    }

    collector = global->garbageCollector;
    region = garbage_collector_find_region_descriptor(collector, regionId);
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

            if (currentRegionId != ZR_NULL && *currentRegionId == region->id) {
                garbage_collector_sync_current_region_cache(collector, region->kind, ZR_NULL);
            }
        }
    }
}

TZrUInt32 garbage_collector_reassign_region_id(SZrGlobalState *global,
                                               TZrUInt32 previousRegionId,
                                               EZrGarbageCollectRegionKind newRegionKind,
                                               TZrSize objectSize) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return 0u;
    }

    garbage_collector_release_region_allocation(global, previousRegionId, objectSize);
    return garbage_collector_allocate_region_id(global, newRegionKind, objectSize);
}

TZrBool garbage_collector_ignore_registry_contains(SZrGarbageCollector *collector, SZrRawObject *object) {
    if (collector == ZR_NULL || object == ZR_NULL || collector->ignoredObjects == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < collector->ignoredObjectCount; i++) {
        if (collector->ignoredObjects[i] == object) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
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

TZrBool garbage_collector_remembered_registry_contains(SZrGarbageCollector *collector, SZrRawObject *object) {
    if (collector == ZR_NULL || object == ZR_NULL || collector->rememberedObjects == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < collector->rememberedObjectCount; i++) {
        if (collector->rememberedObjects[i] == object) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool garbage_collector_object_can_hold_gc_references(const SZrRawObject *object) {
    if (object == ZR_NULL || object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return ZR_FALSE;
    }

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_OBJECT:
        case ZR_RAW_OBJECT_TYPE_ARRAY:
        case ZR_RAW_OBJECT_TYPE_CLOSURE:
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE:
        case ZR_RAW_OBJECT_TYPE_FUNCTION:
        case ZR_RAW_OBJECT_TYPE_THREAD:
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void garbage_collector_registry_forget_object(SZrRawObject **items, TZrSize *count, SZrRawObject *object) {
    TZrSize newCount;
    TZrSize writeIndex = 0;

    if (items == ZR_NULL || count == ZR_NULL || *count == 0 || object == ZR_NULL) {
        return;
    }

    for (TZrSize readIndex = 0; readIndex < *count; readIndex++) {
        if (items[readIndex] == object) {
            continue;
        }
        items[writeIndex++] = items[readIndex];
    }

    newCount = writeIndex;
    while (writeIndex < *count) {
        items[writeIndex++] = ZR_NULL;
    }

    *count = newCount;
}

void garbage_collector_forget_object_from_registries(SZrGarbageCollector *collector, SZrRawObject *object) {
    if (collector == ZR_NULL || object == ZR_NULL) {
        return;
    }

    garbage_collector_registry_forget_object(collector->ignoredObjects, &collector->ignoredObjectCount, object);
    garbage_collector_registry_forget_object(collector->rememberedObjects, &collector->rememberedObjectCount, object);
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
    SZrObject *runtimeObject;

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

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_OBJECT:
            runtimeObject = (SZrObject *)object;
            if (runtimeObject == ZR_NULL) {
                return sizeof(SZrObject);
            }

            switch (runtimeObject->internalType) {
                case ZR_OBJECT_INTERNAL_TYPE_MODULE:
                    return sizeof(SZrObjectModule);
                case ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE: {
                    SZrObjectPrototype *prototype = (SZrObjectPrototype *)runtimeObject;

                    if (prototype != ZR_NULL && prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                        return sizeof(SZrStructPrototype);
                    }
                    return sizeof(SZrObjectPrototype);
                }
                case ZR_OBJECT_INTERNAL_TYPE_ARRAY:
                case ZR_OBJECT_INTERNAL_TYPE_STRUCT:
                case ZR_OBJECT_INTERNAL_TYPE_OBJECT:
                case ZR_OBJECT_INTERNAL_TYPE_PROTOTYPE_INFO:
                case ZR_OBJECT_INTERNAL_TYPE_CUSTOM_EXTENSION_START:
                default:
                    return sizeof(SZrObject);
            }
        case ZR_RAW_OBJECT_TYPE_FUNCTION:
            return sizeof(SZrFunction);
        case ZR_RAW_OBJECT_TYPE_STRING: {
            if (state != ZR_NULL) {
                SZrString *str = ZR_CAST_STRING(state, object);
                if (str != ZR_NULL) {
                    if (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                        return sizeof(SZrString) + (TZrSize)str->shortStringLength + 1;
                    }
                    return sizeof(SZrString);
                }
            }
            return sizeof(SZrString);
        }
        case ZR_RAW_OBJECT_TYPE_BUFFER:
            return sizeof(SZrArray);
        case ZR_RAW_OBJECT_TYPE_ARRAY:
            // Runtime arrays are object-backed (SZrObject + nodeMap), not standalone SZrArray headers.
            return sizeof(SZrObject);
        case ZR_RAW_OBJECT_TYPE_CLOSURE: {
            if (state != ZR_NULL) {
                if (object->isNative) {
                    SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);
                    if (closure != ZR_NULL) {
                        TZrSize extraCount = closure->closureValueCount > 1 ? closure->closureValueCount - 1 : 0;
                        return sizeof(SZrClosureNative) + extraCount * sizeof(SZrTypeValue *) +
                               closure->closureValueCount * sizeof(SZrRawObject *);
                    }
                } else {
                    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);
                    if (closure != ZR_NULL) {
                        TZrSize extraCount = closure->closureValueCount > 1 ? closure->closureValueCount - 1 : 0;
                        return sizeof(SZrClosure) + extraCount * sizeof(SZrClosureValue *);
                    }
                }
            }
            return object->isNative ? sizeof(SZrClosureNative) : sizeof(SZrClosure);
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE:
            return sizeof(SZrClosureValue);
        case ZR_RAW_OBJECT_TYPE_THREAD:
            return sizeof(SZrState);
        case ZR_RAW_OBJECT_TYPE_NATIVE_POINTER:
            return sizeof(SZrRawObject);
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA: {
            if (state != ZR_NULL) {
                struct SZrNativeData *nativeData =
                        ZR_CAST_CHECKED(state, struct SZrNativeData *, object, ZR_RAW_OBJECT_TYPE_NATIVE_DATA);
                if (nativeData != ZR_NULL) {
                    TZrSize extraCount = nativeData->valueLength > 1 ? nativeData->valueLength - 1 : 0;
                    return sizeof(struct SZrNativeData) + extraCount * sizeof(SZrTypeValue);
                }
            }
            return sizeof(struct SZrNativeData);
        }
        default:
            return sizeof(SZrRawObject);
    }
}

ZR_CORE_API TZrSize ZrCore_GarbageCollector_GetObjectBaseSize(SZrState *state, SZrRawObject *object) {
    return garbage_collector_get_object_base_size(state, object);
}

void garbage_collector_free_object(SZrState *state, SZrRawObject *object) {
    EZrGarbageCollectIncrementalObjectStatus oldStatus;
    SZrGlobalState *global;
    TZrSize objectSize;

    if (object == ZR_NULL || state == ZR_NULL) {
        return;
    }

    if ((TZrPtr)object < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        return;
    }

    global = state->global;
    if (global == ZR_NULL) {
        return;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return;
    }

    oldStatus = object->garbageCollectMark.status;
    if (oldStatus == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED) {
        return;
    }

    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
    objectSize = garbage_collector_get_object_base_size(state, object);
    garbage_collector_release_region_allocation(global, object->garbageCollectMark.regionId, objectSize);
    garbage_collector_forget_object_from_registries(global->garbageCollector, object);

    ZrCore_Ownership_NotifyObjectReleased(state, object);

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_ARRAY: {
            SZrObject *obj = ZR_CAST_OBJECT(state, object);
            if (obj != ZR_NULL) {
                /* Array payload is currently owned elsewhere. */
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_FUNCTION:
        case ZR_RAW_OBJECT_TYPE_OBJECT:
            break;
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA:
            if (object->scanMarkGcFunction != ZR_NULL) {
                object->scanMarkGcFunction(state, object);
            }
            break;
        default:
            break;
    }

    ZrCore_Memory_RawFreeWithType(global, object, objectSize, ZR_MEMORY_NATIVE_TYPE_OBJECT);
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
    object->garbageCollectMark.regionId =
            garbage_collector_allocate_region_id(global, object->garbageCollectMark.regionKind, size);
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
    object->garbageCollectMark.regionId = garbage_collector_allocate_region_id(global, regionKind, size);
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

    if (garbage_collector_ignore_registry_contains(global->garbageCollector, object)) {
        return ZR_FALSE;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return ZR_TRUE;
    }

    status = object->garbageCollectMark.status;
    generation = object->garbageCollectMark.generation;
    return status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED ||
           status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED ||
           (status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED &&
            generation != global->garbageCollector->gcGeneration);
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
    EZrGarbageCollectIncrementalObjectStatus status;

    if (object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT) {
        return;
    }

    status = object->garbageCollectMark.status;
    ZR_ASSERT(status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED ||
              status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN ||
              status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED);

    objectSize = garbage_collector_get_object_base_size(state, object);
    previousRegionId = object->garbageCollectMark.regionId;
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT;
    ZrCore_RawObject_SetStorageKind(object, ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT);
    ZrCore_RawObject_SetRegionKind(object, ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT);
    object->garbageCollectMark.regionId =
            garbage_collector_reassign_region_id(global, previousRegionId, ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT, objectSize);
    object->garbageCollectMark.pinFlags |= ZR_GARBAGE_COLLECT_PIN_KIND_PERSISTENT_ROOT;
    object->garbageCollectMark.escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT;
    object->garbageCollectMark.promotionReason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_GLOBAL_ROOT;
    raw_object_trace("raw object permanent object=%p status=%d region=%u",
                     (void *)object,
                     (int)status,
                     (unsigned int)object->garbageCollectMark.regionId);
}

void ZrCore_Gc_ValueStaticAssertIsAlive(SZrState *state, SZrTypeValue *value) {
    ZR_ASSERT(!value->isGarbageCollectable ||
              ((value->type == (EZrValueType)value->value.object->type) &&
               ((state == ZR_NULL) || !ZrCore_Gc_RawObjectIsDead(state->global, value->value.object))));
}

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
