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
TZrBool garbage_collector_object_can_hold_gc_references(const SZrRawObject *object);
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

void ZrGarbageCollectorReallyMarkObject(SZrState *state, SZrRawObject *object);
TZrSize ZrGarbageCollectorPropagateMark(SZrState *state);

#endif // ZR_VM_CORE_GC_INTERNAL_H
