//
// Internal GC helpers shared across split translation units.
//

#ifndef ZR_VM_CORE_GC_INTERNAL_H
#define ZR_VM_CORE_GC_INTERNAL_H

#include "zr_vm_core/gc.h"

#include <stddef.h>
#include <string.h>

#include "zr_vm_common/zr_array_conf.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/native.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#define ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST ((TZrUInt32)1u)

static ZR_FORCE_INLINE TZrBool garbage_collector_ignore_registry_contains(SZrGarbageCollector *collector,
                                                                          SZrRawObject *object) {
    TZrSize index;

    if (collector == ZR_NULL || object == ZR_NULL || collector->ignoredObjects == ZR_NULL) {
        return ZR_FALSE;
    }

    index = object->garbageCollectMark.ignoredRegistryIndex;
    return index < collector->ignoredObjectCount && collector->ignoredObjects[index] == object;
}

static ZR_FORCE_INLINE TZrBool garbage_collector_object_is_unreferenced_fast(
        const SZrGarbageCollector *collector,
        const SZrRawObject *object) {
    EZrGarbageCollectIncrementalObjectStatus status;

    if (collector == ZR_NULL || object == ZR_NULL) {
        return ZR_TRUE;
    }

    status = object->garbageCollectMark.status;
    if (status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED &&
        status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED &&
        (status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED ||
         object->garbageCollectMark.generation == collector->gcGeneration)) {
        return ZR_FALSE;
    }

    if (object->garbageCollectMark.ignoredRegistryIndex == ZR_MAX_SIZE) {
        return ZR_TRUE;
    }

    return !garbage_collector_ignore_registry_contains((SZrGarbageCollector *)collector, (SZrRawObject *)object);
}

static ZR_FORCE_INLINE TZrSize garbage_collector_get_object_base_size_fast(const SZrRawObject *object) {
    const SZrObject *runtimeObject;

    if (object == ZR_NULL) {
        return 0;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return 0;
    }

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_OBJECT:
            runtimeObject = (const SZrObject *)object;
            switch (runtimeObject->internalType) {
                case ZR_OBJECT_INTERNAL_TYPE_MODULE:
                    return sizeof(SZrObjectModule);
                case ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE: {
                    const SZrObjectPrototype *prototype = (const SZrObjectPrototype *)runtimeObject;

                    return prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                                   ? sizeof(SZrStructPrototype)
                                   : sizeof(SZrObjectPrototype);
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
            const SZrString *stringValue = (const SZrString *)object;

            return stringValue->shortStringLength < ZR_VM_LONG_STRING_FLAG
                           ? sizeof(SZrString) + (TZrSize)stringValue->shortStringLength + 1u
                           : sizeof(SZrString);
        }
        case ZR_RAW_OBJECT_TYPE_BUFFER:
            return sizeof(SZrArray);
        case ZR_RAW_OBJECT_TYPE_ARRAY:
            return sizeof(SZrObject);
        case ZR_RAW_OBJECT_TYPE_CLOSURE:
            if (object->isNative) {
                const SZrClosureNative *closure = (const SZrClosureNative *)object;
                TZrSize extraCount = closure->closureValueCount > 1 ? closure->closureValueCount - 1 : 0;

                return sizeof(SZrClosureNative) + extraCount * sizeof(SZrTypeValue *) +
                       closure->closureValueCount * sizeof(SZrRawObject *);
            } else {
                const SZrClosure *closure = (const SZrClosure *)object;
                TZrSize extraCount = closure->closureValueCount > 1 ? closure->closureValueCount - 1 : 0;

                return sizeof(SZrClosure) + extraCount * sizeof(SZrClosureValue *);
            }
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE:
            return sizeof(SZrClosureValue);
        case ZR_RAW_OBJECT_TYPE_THREAD:
            return sizeof(SZrState);
        case ZR_RAW_OBJECT_TYPE_NATIVE_POINTER:
            return sizeof(SZrRawObject);
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA: {
            const struct SZrNativeData *nativeData = (const struct SZrNativeData *)object;
            TZrSize extraCount = nativeData->valueLength > 1 ? nativeData->valueLength - 1 : 0;

            return sizeof(struct SZrNativeData) + extraCount * sizeof(SZrTypeValue);
        }
        default:
            return sizeof(SZrRawObject);
    }
}

static ZR_FORCE_INLINE void garbage_collector_current_region_cache_slots_fast(
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

static ZR_FORCE_INLINE void garbage_collector_clear_current_region_cache_slots_fast(
        TZrUInt32 *regionIdSlot,
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

static ZR_FORCE_INLINE TZrUInt32 garbage_collector_try_allocate_region_id_current_fast(
        SZrGarbageCollector *collector,
        EZrGarbageCollectRegionKind regionKind,
        TZrSize objectSize,
        TZrSize *outRegionDescriptorIndex) {
    SZrGarbageCollectRegionDescriptor *region;
    TZrUInt32 *regionIdSlot;
    TZrUInt64 *usedBytesSlot;
    TZrSize *regionIndexSlot;
    TZrUInt64 requestedBytes;
    TZrUInt64 updatedUsedBytes;

    if (outRegionDescriptorIndex != ZR_NULL) {
        *outRegionDescriptorIndex = ZR_MAX_SIZE;
    }
    if (collector == ZR_NULL || collector->regions == ZR_NULL) {
        return 0u;
    }

    requestedBytes = objectSize > 0 ? (TZrUInt64)objectSize : 1u;
    garbage_collector_current_region_cache_slots_fast(
            collector, regionKind, &regionIdSlot, &regionIndexSlot, &usedBytesSlot);
    if (regionIdSlot == ZR_NULL || regionIndexSlot == ZR_NULL || usedBytesSlot == ZR_NULL || *regionIdSlot == 0u ||
        *regionIndexSlot >= collector->regionCount) {
        return 0u;
    }

    region = &collector->regions[*regionIndexSlot];
    updatedUsedBytes = *usedBytesSlot + requestedBytes;
    if (region->id != *regionIdSlot || region->kind != regionKind || updatedUsedBytes > region->capacityBytes) {
        garbage_collector_clear_current_region_cache_slots_fast(regionIdSlot, regionIndexSlot, usedBytesSlot);
        return 0u;
    }

    region->usedBytes = updatedUsedBytes;
    region->liveBytes += requestedBytes;
    region->liveObjectCount++;
    *usedBytesSlot = updatedUsedBytes;
    if (outRegionDescriptorIndex != ZR_NULL) {
        *outRegionDescriptorIndex = *regionIndexSlot;
    }
    return region->id;
}

static ZR_FORCE_INLINE TZrUInt32 garbage_collector_try_allocate_old_region_id_current_fast(
        SZrGarbageCollector *collector,
        TZrSize objectSize,
        TZrSize *outRegionDescriptorIndex) {
    SZrGarbageCollectRegionDescriptor *region;
    TZrUInt64 requestedBytes;
    TZrUInt64 updatedUsedBytes;

    if (outRegionDescriptorIndex != ZR_NULL) {
        *outRegionDescriptorIndex = ZR_MAX_SIZE;
    }
    if (collector == ZR_NULL || collector->regions == ZR_NULL || collector->currentOldRegionId == 0u ||
        collector->currentOldRegionIndex >= collector->regionCount) {
        return 0u;
    }

    requestedBytes = objectSize > 0 ? (TZrUInt64)objectSize : 1u;
    region = &collector->regions[collector->currentOldRegionIndex];
    updatedUsedBytes = collector->currentOldRegionUsedBytes + requestedBytes;
    if (region->id != collector->currentOldRegionId ||
        region->kind != ZR_GARBAGE_COLLECT_REGION_KIND_OLD ||
        updatedUsedBytes > region->capacityBytes) {
        garbage_collector_clear_current_region_cache_slots_fast(&collector->currentOldRegionId,
                                                                &collector->currentOldRegionIndex,
                                                                &collector->currentOldRegionUsedBytes);
        return 0u;
    }

    region->usedBytes = updatedUsedBytes;
    region->liveBytes += requestedBytes;
    region->liveObjectCount++;
    collector->currentOldRegionUsedBytes = updatedUsedBytes;
    if (outRegionDescriptorIndex != ZR_NULL) {
        *outRegionDescriptorIndex = collector->currentOldRegionIndex;
    }
    return region->id;
}

static ZR_FORCE_INLINE TZrBool garbage_collector_try_release_region_allocation_fast(
        SZrGarbageCollector *collector,
        TZrUInt32 regionId,
        TZrSize regionDescriptorIndex,
        TZrSize objectSize) {
    SZrGarbageCollectRegionDescriptor *region;
    TZrUInt32 *currentRegionId;
    TZrUInt64 *currentRegionUsedBytes;
    TZrSize *currentRegionIndex;
    TZrUInt64 releasedBytes;

    if (collector == ZR_NULL || collector->regions == ZR_NULL || regionId == 0u ||
        regionDescriptorIndex >= collector->regionCount) {
        return ZR_FALSE;
    }

    region = &collector->regions[regionDescriptorIndex];
    if (region->id != regionId) {
        return ZR_FALSE;
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

    if (region->liveObjectCount != 0u) {
        return ZR_TRUE;
    }

    region->usedBytes = 0u;
    region->liveBytes = 0u;
    garbage_collector_current_region_cache_slots_fast(
            collector, region->kind, &currentRegionId, &currentRegionIndex, &currentRegionUsedBytes);
    if (currentRegionId != ZR_NULL && *currentRegionId == regionId) {
        garbage_collector_clear_current_region_cache_slots_fast(
                currentRegionId, currentRegionIndex, currentRegionUsedBytes);
    }
    return ZR_TRUE;
}

TZrBool garbage_collector_ensure_ignore_registry_capacity(SZrGlobalState *global, TZrSize minCapacity);
static ZR_FORCE_INLINE TZrBool garbage_collector_remembered_registry_contains(SZrGarbageCollector *collector,
                                                                              SZrRawObject *object) {
    TZrSize index;

    if (collector == ZR_NULL || object == ZR_NULL || collector->rememberedObjects == ZR_NULL) {
        return ZR_FALSE;
    }

    index = object->garbageCollectMark.rememberedRegistryIndex;
    return index < collector->rememberedObjectCount && collector->rememberedObjects[index] == object;
}

TZrBool garbage_collector_ensure_remembered_registry_capacity(SZrGlobalState *global, TZrSize minCapacity);
static ZR_FORCE_INLINE TZrBool garbage_collector_object_can_hold_gc_references(const SZrRawObject *object) {
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

static ZR_FORCE_INLINE TZrBool garbage_collector_minor_collection_is_active_fast(
        const SZrGarbageCollector *collector) {
    return collector != ZR_NULL &&
           collector->gcMode == ZR_GARBAGE_COLLECT_MODE_GENERATIONAL &&
           (collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_MARK ||
            collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_EVACUATE);
}

static ZR_FORCE_INLINE TZrBool garbage_collector_collection_is_marking_roots_fast(
        const SZrGarbageCollector *collector) {
    EZrGarbageCollectCollectionPhase phase;

    if (collector == ZR_NULL) {
        return ZR_FALSE;
    }

    phase = collector->collectionPhase;
    if (phase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE ||
        phase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_SWEEP ||
        phase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_COMPACT) {
        return ZR_FALSE;
    }

    return collector->gcRunningStatus <= ZR_GARBAGE_COLLECT_RUNNING_STATUS_ATOMIC ||
           phase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_MARK ||
           phase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_EVACUATE ||
           phase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT ||
           phase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_REMARK;
}

static ZR_FORCE_INLINE TZrBool garbage_collector_object_is_markable_root_fast(const SZrRawObject *object) {
    return object != ZR_NULL &&
           object->type < ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX &&
           object->type != ZR_RAW_OBJECT_TYPE_INVALID &&
           !ZrCore_RawObject_IsReleased((SZrRawObject *)object);
}
TZrSize garbage_collector_get_object_base_size(SZrState *state, SZrRawObject *object);
TZrBool garbage_collector_callsite_sanitize_tracing_enabled(void);
void garbage_collector_sanitize_callsite_cache_pic(const SZrFunction *function,
                                                   TZrUInt32 cacheIndex,
                                                   const TZrChar *phase,
                                                   SZrFunctionCallSiteCacheEntry *cacheEntry);
void garbage_collector_record_callsite_cache_pic_write(
        const SZrFunction *function,
        TZrUInt32 cacheIndex,
        TZrUInt32 slotIndex,
        const TZrChar *writer,
        const TZrChar *action,
        const SZrFunctionCallSiteCacheEntry *cacheEntry,
        const SZrFunctionCallSitePicSlot *slot);
SZrRawObject *garbage_collector_new_raw_object_in_region(SZrState *state,
                                                         EZrValueType type,
                                                         TZrSize size,
                                                         TZrBool isNative,
                                                         EZrGarbageCollectRegionKind regionKind,
                                                         EZrGarbageCollectStorageKind storageKind);
TZrUInt32 garbage_collector_allocate_region_id_cached(SZrGlobalState *global,
                                                      EZrGarbageCollectRegionKind regionKind,
                                                      TZrSize objectSize,
                                                      TZrSize *outRegionDescriptorIndex);
TZrUInt32 garbage_collector_allocate_old_region_id_cached(SZrGlobalState *global,
                                                          TZrSize objectSize,
                                                          TZrSize *outRegionDescriptorIndex);
TZrUInt32 garbage_collector_allocate_region_id(SZrGlobalState *global,
                                               EZrGarbageCollectRegionKind regionKind,
                                               TZrSize objectSize);
void garbage_collector_release_region_allocation_cached(SZrGlobalState *global,
                                                        TZrUInt32 regionId,
                                                        TZrSize regionDescriptorIndex,
                                                        TZrSize objectSize);
void garbage_collector_release_region_allocation(SZrGlobalState *global,
                                                 TZrUInt32 regionId,
                                                 TZrSize objectSize);
TZrUInt32 garbage_collector_reassign_region_id_cached(SZrGlobalState *global,
                                                      TZrUInt32 previousRegionId,
                                                      TZrSize previousRegionDescriptorIndex,
                                                      EZrGarbageCollectRegionKind newRegionKind,
                                                      TZrSize objectSize,
                                                      TZrSize *outRegionDescriptorIndex);
TZrUInt32 garbage_collector_reassign_region_id(SZrGlobalState *global,
                                               TZrUInt32 previousRegionId,
                                               EZrGarbageCollectRegionKind newRegionKind,
                                               TZrSize objectSize);
void garbage_collector_forget_object_from_registries(SZrGarbageCollector *collector, SZrRawObject *object);
void garbage_collector_free_object_sized(SZrState *state, SZrRawObject *object, TZrSize objectSize);
void garbage_collector_free_object(SZrState *state, SZrRawObject *object);

SZrRawObject **garbage_collector_sweep_list(SZrState *state, SZrRawObject **list, int maxCount, int *count);
void garbage_collector_enter_sweep(SZrState *state);
TZrSize garbage_collector_run_a_few_finalizers(SZrState *state, int maxCount);

void garbage_collector_run_until_state(SZrState *state, EZrGarbageCollectRunningStatus targetState);
void garbage_collector_check_sizes(SZrState *state, SZrGlobalState *global);
TZrSize garbage_collector_run_generational_full(SZrState *state);
TZrSize garbage_collector_process_weak_tables(SZrState *state);
TZrSize garbage_collector_atomic(SZrState *state);
int garbage_collector_sweep_step(SZrState *state, EZrGarbageCollectRunningStatus nextstate, SZrRawObject **nextlist);
TZrSize garbage_collector_single_step(SZrState *state);
TZrBool garbage_collector_is_generational_mode(SZrGlobalState *global);
void garbage_collector_full_inc(SZrState *state, SZrGlobalState *global);
TZrBool gcrunning(SZrGlobalState *global);
void garbage_collector_run_generational_step(SZrState *state);

void garbage_collector_mark_object(SZrState *state, SZrRawObject *object);
void garbage_collector_mark_value(SZrState *state, SZrTypeValue *value);
TZrSize garbage_collector_mark_string_roots(SZrState *state);
TZrSize garbage_collector_mark_ignored_roots(SZrState *state);
void garbage_collector_link_to_gray_list(SZrRawObject *object, SZrRawObject **list);
void garbage_collector_to_gc_list_and_mark_wait_to_scan(SZrRawObject *object, SZrRawObject **list);

static ZR_FORCE_INLINE void garbage_collector_mark_ignored_root_if_needed_fast(
        SZrState *state,
        SZrRawObject *object) {
    SZrGlobalState *global;
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || object == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    global = state->global;
    collector = global->garbageCollector;
    if (!garbage_collector_collection_is_marking_roots_fast(collector) ||
        !garbage_collector_object_is_markable_root_fast(object)) {
        return;
    }

    garbage_collector_mark_object(state, object);
}

void ZrGarbageCollectorReallyMarkObject(SZrState *state, SZrRawObject *object);
TZrSize ZrGarbageCollectorPropagateMark(SZrState *state);

static ZR_FORCE_INLINE TZrBool garbage_collector_try_mark_embedded_child_function_fast(
        SZrState *state,
        SZrGarbageCollector *collector,
        TZrBool minorActive,
        SZrRawObject *object) {
    SZrFunction *function;

    if (state == ZR_NULL || collector == ZR_NULL || object == ZR_NULL || object->type != ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_FALSE;
    }

    function = ZR_CAST_FUNCTION(state, object);
    if (function == ZR_NULL || function->ownerFunction == ZR_NULL || ZrCore_RawObject_IsReleased(object)) {
        return ZR_FALSE;
    }

    if (minorActive) {
        if (collector->minorCollectionEpoch != 0u &&
            object->garbageCollectMark.minorScanEpoch == collector->minorCollectionEpoch &&
            (ZR_GC_IS_REFERENCED(object) || ZR_GC_IS_WAIT_TO_SCAN(object))) {
            return ZR_TRUE;
        }
        object->garbageCollectMark.minorScanEpoch = collector->minorCollectionEpoch;
    } else {
        if (object->garbageCollectMark.generation == collector->gcGeneration &&
            (ZR_GC_IS_REFERENCED(object) || ZR_GC_IS_WAIT_TO_SCAN(object))) {
            return ZR_TRUE;
        }
        object->garbageCollectMark.generation = collector->gcGeneration;
    }

    object->gcList = ZR_NULL;
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
    ZrGarbageCollectorReallyMarkObject(state, object);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool garbage_collector_try_mark_object_during_minor_fast(
        SZrGarbageCollector *collector,
        TZrBool minorActive,
        SZrRawObject *object) {
    if (!minorActive || object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID ||
        ZrCore_RawObject_IsReleased(object)) {
        return ZR_TRUE;
    }

    if (object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE) {
        return ZR_FALSE;
    }

    if (collector == ZR_NULL ||
        collector->minorCollectionEpoch == 0u ||
        object->garbageCollectMark.minorScanEpoch == collector->minorCollectionEpoch) {
        return ZR_TRUE;
    }

    object->garbageCollectMark.minorScanEpoch = collector->minorCollectionEpoch;
    if (object->type == ZR_RAW_OBJECT_TYPE_STRING) {
        return ZR_TRUE;
    }

    object->gcList = collector->waitToScanObjectList;
    collector->waitToScanObjectList = object;
    if (object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT) {
        ZrCore_RawObject_MarkAsWaitToScan(object);
    }
    return ZR_TRUE;
}

static ZR_FORCE_INLINE void garbage_collector_mark_raw_object_internal(
        SZrState *state,
        SZrRawObject *object,
        TZrBool stampYoungMinorEpoch) {
    SZrGarbageCollector *collector;
    TZrBool minorActive;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    collector = state->global->garbageCollector;
    minorActive = garbage_collector_minor_collection_is_active_fast(collector);
    if (garbage_collector_try_mark_embedded_child_function_fast(state, collector, minorActive, object)) {
        return;
    }
    if (garbage_collector_try_mark_object_during_minor_fast(collector, minorActive, object)) {
        return;
    }
    if (stampYoungMinorEpoch &&
        minorActive &&
        object != ZR_NULL &&
        object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE) {
        /*
         * Young objects reached by the current minor mark either keep their address
         * (in-place reassign) or are memcpy-cloned during evacuation. Stamping the
         * from-space object here lets both destination shapes carry the same rewrite
         * epoch without a second full-list discovery pass.
         */
        object->garbageCollectMark.minorScanEpoch = collector->minorCollectionEpoch;
    }
    if (ZR_GC_IS_REFERENCED(object) || ZR_GC_IS_WAIT_TO_SCAN(object)) {
        return;
    }
    if (ZrCore_RawObject_IsMarkInited(object)) {
        if (object->type == ZR_RAW_OBJECT_TYPE_STRING) {
            ZR_GC_SET_REFERENCED(object);
            return;
        }
        ZrGarbageCollectorReallyMarkObject(state, object);
    }
}

#endif // ZR_VM_CORE_GC_INTERNAL_H
