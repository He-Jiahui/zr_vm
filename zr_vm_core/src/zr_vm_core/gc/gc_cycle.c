//
// GC cycle, phase-transition, and scheduling helpers.
//

#include "gc/gc_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "zr_vm_common/zr_runtime_sentinel_conf.h"

static TZrBool garbage_collector_trace_rewrite_slots_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        enabled = getenv("ZR_VM_TRACE_GC_REWRITE_SLOTS") != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static TZrBool garbage_collector_trace_callsite_sanitize_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        enabled = getenv("ZR_VM_TRACE_GC_CALLSITE_SANITIZE") != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

TZrBool garbage_collector_callsite_sanitize_tracing_enabled(void) {
    return garbage_collector_trace_callsite_sanitize_enabled();
}

static const TZrChar *garbage_collector_trace_function_name(const SZrFunction *function);

static const TZrChar *garbage_collector_callsite_cache_kind_name(TZrUInt32 kind) {
    switch ((EZrFunctionCallSiteCacheKind)kind) {
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET:
            return "META_GET";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET:
            return "META_SET";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC:
            return "META_GET_STATIC";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC:
            return "META_SET_STATIC";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL:
            return "META_CALL";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL:
            return "DYN_CALL";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL:
            return "META_TAIL_CALL";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL:
            return "DYN_TAIL_CALL";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET:
            return "MEMBER_GET";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET:
            return "MEMBER_SET";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_NONE:
        default:
            return "NONE";
    }
}

#define ZR_GC_CALLSITE_TRACE_WRITE_TABLE_CAPACITY ((TZrUInt32)256u)

typedef struct SZrGcCallsitePicWriteTrace {
    const SZrFunction *function;
    TZrUInt32 cacheIndex;
    TZrUInt32 slotIndex;
    TZrUInt32 sequence;
    const TZrChar *writer;
    const TZrChar *action;
    TZrUInt32 picSlotCount;
    TZrUInt32 picNextInsertIndex;
    const void *receiver;
    const void *owner;
    const void *target;
} SZrGcCallsitePicWriteTrace;

static SZrGcCallsitePicWriteTrace g_gc_callsite_pic_write_traces[ZR_GC_CALLSITE_TRACE_WRITE_TABLE_CAPACITY];
static TZrUInt32 g_gc_callsite_pic_write_trace_count = 0;
static TZrUInt32 g_gc_callsite_pic_write_trace_sequence = 0;

static SZrGcCallsitePicWriteTrace *garbage_collector_find_callsite_pic_write_trace(const SZrFunction *function,
                                                                                    TZrUInt32 cacheIndex,
                                                                                    TZrUInt32 slotIndex) {
    for (TZrUInt32 index = 0; index < g_gc_callsite_pic_write_trace_count; index++) {
        SZrGcCallsitePicWriteTrace *trace = &g_gc_callsite_pic_write_traces[index];

        if (trace->function == function && trace->cacheIndex == cacheIndex && trace->slotIndex == slotIndex) {
            return trace;
        }
    }

    return ZR_NULL;
}

static void garbage_collector_trace_last_callsite_cache_write(const SZrFunction *function,
                                                              TZrUInt32 cacheIndex,
                                                              const TZrChar *phase,
                                                              const TZrChar *event,
                                                              TZrUInt32 slotIndex) {
    const SZrGcCallsitePicWriteTrace *trace;

    if (!garbage_collector_trace_callsite_sanitize_enabled()) {
        return;
    }

    trace = garbage_collector_find_callsite_pic_write_trace(function, cacheIndex, slotIndex);
    if (trace == ZR_NULL) {
        fprintf(stderr,
                "[gc-callsite-sanitize] phase=%s event=%s function=%p name=%s cacheIndex=%u slotIndex=%u lastWrite=missing\n",
                phase != ZR_NULL ? phase : "unknown",
                event != ZR_NULL ? event : "unknown",
                (const void *)function,
                garbage_collector_trace_function_name(function),
                (unsigned int)cacheIndex,
                (unsigned int)slotIndex);
        return;
    }

    fprintf(stderr,
            "[gc-callsite-sanitize] phase=%s event=%s function=%p name=%s cacheIndex=%u slotIndex=%u "
            "lastWriteSequence=%u writer=%s action=%s picSlotCount=%u picNextInsertIndex=%u "
            "receiver=%p owner=%p target=%p\n",
            phase != ZR_NULL ? phase : "unknown",
            event != ZR_NULL ? event : "unknown",
            (const void *)function,
            garbage_collector_trace_function_name(function),
            (unsigned int)cacheIndex,
            (unsigned int)slotIndex,
            (unsigned int)trace->sequence,
            trace->writer != ZR_NULL ? trace->writer : "unknown",
            trace->action != ZR_NULL ? trace->action : "unknown",
            (unsigned int)trace->picSlotCount,
            (unsigned int)trace->picNextInsertIndex,
            trace->receiver,
            trace->owner,
            trace->target);
}

static const TZrChar *garbage_collector_trace_function_name(const SZrFunction *function) {
    if (function == ZR_NULL || function->functionName == ZR_NULL) {
        return "<anonymous>";
    }

    return ZrCore_String_GetNativeString(function->functionName);
}

static void garbage_collector_trace_callsite_cache_entry(const SZrFunction *function,
                                                         TZrUInt32 cacheIndex,
                                                         const TZrChar *phase,
                                                         const TZrChar *event,
                                                         const SZrFunctionCallSiteCacheEntry *cacheEntry) {
    if (!garbage_collector_trace_callsite_sanitize_enabled() || cacheEntry == ZR_NULL) {
        return;
    }

    fprintf(stderr,
            "[gc-callsite-sanitize] phase=%s event=%s function=%p name=%s cacheIndex=%u kind=%s instructionIndex=%u "
            "memberEntryIndex=%u argumentCount=%u picSlotCount=%u picNextInsertIndex=%u\n",
            phase != ZR_NULL ? phase : "unknown",
            event != ZR_NULL ? event : "unknown",
            (const void *)function,
            garbage_collector_trace_function_name(function),
            (unsigned int)cacheIndex,
            garbage_collector_callsite_cache_kind_name(cacheEntry->kind),
            (unsigned int)cacheEntry->instructionIndex,
            (unsigned int)cacheEntry->memberEntryIndex,
            (unsigned int)cacheEntry->argumentCount,
            (unsigned int)cacheEntry->picSlotCount,
            (unsigned int)cacheEntry->picNextInsertIndex);
}

static void garbage_collector_trace_callsite_cache_slot(const SZrFunction *function,
                                                        TZrUInt32 cacheIndex,
                                                        const TZrChar *phase,
                                                        const TZrChar *event,
                                                        const SZrFunctionCallSiteCacheEntry *cacheEntry,
                                                        TZrUInt32 slotIndex,
                                                        const SZrFunctionCallSitePicSlot *slot) {
    if (!garbage_collector_trace_callsite_sanitize_enabled() || cacheEntry == ZR_NULL || slot == ZR_NULL) {
        return;
    }

    fprintf(stderr,
            "[gc-callsite-sanitize] phase=%s event=%s function=%p name=%s cacheIndex=%u kind=%s instructionIndex=%u "
            "slotIndex=%u receiver=%p owner=%p target=%p receiverVersion=%u ownerVersion=%u descriptorIndex=%u "
            "isStatic=%u picSlotCount=%u picNextInsertIndex=%u\n",
            phase != ZR_NULL ? phase : "unknown",
            event != ZR_NULL ? event : "unknown",
            (const void *)function,
            garbage_collector_trace_function_name(function),
            (unsigned int)cacheIndex,
            garbage_collector_callsite_cache_kind_name(cacheEntry->kind),
            (unsigned int)cacheEntry->instructionIndex,
            (unsigned int)slotIndex,
            (const void *)slot->cachedReceiverPrototype,
            (const void *)slot->cachedOwnerPrototype,
            (const void *)slot->cachedFunction,
            (unsigned int)slot->cachedReceiverVersion,
            (unsigned int)slot->cachedOwnerVersion,
            (unsigned int)slot->cachedDescriptorIndex,
            (unsigned int)slot->cachedIsStatic,
            (unsigned int)cacheEntry->picSlotCount,
            (unsigned int)cacheEntry->picNextInsertIndex);
}

void garbage_collector_record_callsite_cache_pic_write(const SZrFunction *function,
                                                       TZrUInt32 cacheIndex,
                                                       TZrUInt32 slotIndex,
                                                       const TZrChar *writer,
                                                       const TZrChar *action,
                                                       const SZrFunctionCallSiteCacheEntry *cacheEntry,
                                                       const SZrFunctionCallSitePicSlot *slot) {
    SZrGcCallsitePicWriteTrace *trace;

    if (!garbage_collector_trace_callsite_sanitize_enabled() || cacheEntry == ZR_NULL || slot == ZR_NULL) {
        return;
    }

    trace = garbage_collector_find_callsite_pic_write_trace(function, cacheIndex, slotIndex);
    if (trace == ZR_NULL) {
        if (g_gc_callsite_pic_write_trace_count < ZR_GC_CALLSITE_TRACE_WRITE_TABLE_CAPACITY) {
            trace = &g_gc_callsite_pic_write_traces[g_gc_callsite_pic_write_trace_count++];
        } else {
            trace = &g_gc_callsite_pic_write_traces[g_gc_callsite_pic_write_trace_sequence %
                                                    ZR_GC_CALLSITE_TRACE_WRITE_TABLE_CAPACITY];
        }
    }

    trace->function = function;
    trace->cacheIndex = cacheIndex;
    trace->slotIndex = slotIndex;
    trace->sequence = ++g_gc_callsite_pic_write_trace_sequence;
    trace->writer = writer;
    trace->action = action;
    trace->picSlotCount = cacheEntry->picSlotCount;
    trace->picNextInsertIndex = cacheEntry->picNextInsertIndex;
    trace->receiver = slot->cachedReceiverPrototype;
    trace->owner = slot->cachedOwnerPrototype;
    trace->target = slot->cachedFunction;
}

static void garbage_collector_trace_rewrite_function_slot(const TZrChar *slotKind,
                                                          const SZrFunction *ownerFunction,
                                                          const SZrFunction *targetFunction) {
    if (!garbage_collector_trace_rewrite_slots_enabled()) {
        return;
    }

    fprintf(stderr,
            "[gc-rewrite-slot] kind=%s owner=%p target=%p\n",
            slotKind != ZR_NULL ? slotKind : "unknown",
            (const void *)ownerFunction,
            (const void *)targetFunction);
}

static TZrBool garbage_collector_raw_heap_pointer_plausible_for_gc_slot(TZrPtr pointer) {
    if (pointer < (TZrPtr)(TZrSize)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        return ZR_FALSE;
    }
#if defined(ZR_RUNTIME_LIKELY_USER_POINTER_MAX_INCLUSIVE)
    if (pointer > ZR_RUNTIME_LIKELY_USER_POINTER_MAX_INCLUSIVE) {
        return ZR_FALSE;
    }
#endif
    if ((((TZrNativePtr)pointer) & (TZrNativePtr)(sizeof(TZrPtr) - 1u)) != 0) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool garbage_collector_callsite_pic_slot_triple_plausible(const SZrFunctionCallSitePicSlot *picSlot) {
    const TZrPtr pointers[3] = {(TZrPtr)picSlot->cachedReceiverPrototype, (TZrPtr)picSlot->cachedOwnerPrototype,
                                  (TZrPtr)picSlot->cachedFunction};
    TZrSize index;

    for (index = 0; index < 3u; index++) {
        TZrPtr candidate = pointers[index];

        if (candidate == (TZrPtr)0) {
            continue;
        }
        if (!garbage_collector_raw_heap_pointer_plausible_for_gc_slot(candidate)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

void garbage_collector_sanitize_callsite_cache_pic(const SZrFunction *function,
                                                   TZrUInt32 cacheIndex,
                                                   const TZrChar *phase,
                                                   SZrFunctionCallSiteCacheEntry *cacheEntry) {
    TZrUInt32 slotIndex;
    TZrUInt32 limit;

    if (cacheEntry == ZR_NULL) {
        return;
    }
    if (cacheEntry->picSlotCount > ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY) {
        /*
         * TODO: Root-cause corrupt picSlotCount (observed as garbage with cap still scanning picSlots);
         * clearing avoids mark/rewrite treating uninitialized picSlots as live pointers during full GC.
         */
        garbage_collector_trace_callsite_cache_entry(function, cacheIndex, phase, "corrupt-slot-count", cacheEntry);
        for (slotIndex = 0; slotIndex < ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY; slotIndex++) {
            garbage_collector_trace_callsite_cache_slot(function,
                                                        cacheIndex,
                                                        phase,
                                                        "corrupt-slot-snapshot",
                                                        cacheEntry,
                                                        slotIndex,
                                                        &cacheEntry->picSlots[slotIndex]);
            garbage_collector_trace_last_callsite_cache_write(function,
                                                              cacheIndex,
                                                              phase,
                                                              "corrupt-slot-last-write",
                                                              slotIndex);
        }
        ZrCore_Memory_RawSet(cacheEntry->picSlots, 0, sizeof(cacheEntry->picSlots));
        cacheEntry->picSlotCount = 0;
        cacheEntry->picNextInsertIndex = 0;
        return;
    }
    limit = cacheEntry->picSlotCount;
    for (slotIndex = 0; slotIndex < limit; slotIndex++) {
        if (!garbage_collector_callsite_pic_slot_triple_plausible(&cacheEntry->picSlots[slotIndex])) {
            /*
             * TODO: Root-cause PIC slots holding non-heap patterns while picSlotCount stays within capacity
             * (native_numeric_pipeline full GC).
             */
            garbage_collector_trace_callsite_cache_entry(function, cacheIndex, phase, "bad-slot", cacheEntry);
            garbage_collector_trace_callsite_cache_slot(function,
                                                        cacheIndex,
                                                        phase,
                                                        "bad-slot-detail",
                                                        cacheEntry,
                                                        slotIndex,
                                                        &cacheEntry->picSlots[slotIndex]);
            garbage_collector_trace_last_callsite_cache_write(function,
                                                              cacheIndex,
                                                              phase,
                                                              "bad-slot-last-write",
                                                              slotIndex);
            ZrCore_Memory_RawSet(cacheEntry->picSlots, 0, sizeof(cacheEntry->picSlots));
            cacheEntry->picSlotCount = 0;
            cacheEntry->picNextInsertIndex = 0;
            return;
        }
    }
}

void garbage_collector_run_until_state(SZrState *state, EZrGarbageCollectRunningStatus targetState) {
    SZrGlobalState *global = state->global;
    TZrSize iterationCount = 0;
    const TZrSize maxIterations = ZR_GC_RUN_UNTIL_STATE_ITERATION_LIMIT;

    while (global->garbageCollector->gcRunningStatus != targetState) {
        if (++iterationCount > maxIterations) {
            global->garbageCollector->gcRunningStatus = targetState;
            global->garbageCollector->waitToScanObjectList = ZR_NULL;
            global->garbageCollector->waitToScanAgainObjectList = ZR_NULL;
            break;
        }

        garbage_collector_single_step(state);
        if (global->garbageCollector->stopGcFlag) {
            break;
        }
    }
}

void garbage_collector_check_sizes(SZrState *state, SZrGlobalState *global) {
    TZrMemoryOffset actualMemories = 0;
    SZrRawObject *object = global->garbageCollector->gcObjectList;

    while (object != ZR_NULL) {
        if (!ZrCore_RawObject_IsUnreferenced(state, object)) {
            actualMemories += garbage_collector_get_object_base_size(state, object);
        }
        object = object->next;
    }

    TZrMemoryOffset estimatedMemories = global->garbageCollector->managedMemories;
    TZrMemoryOffset difference = actualMemories - estimatedMemories;
    TZrMemoryOffset driftTolerance = estimatedMemories / ZR_GC_MANAGED_MEMORY_DRIFT_TOLERANCE_DIVISOR;
    if (difference > driftTolerance || difference < -driftTolerance) {
        global->garbageCollector->managedMemories = actualMemories;
    }
}

static TZrBool garbage_collector_object_is_live_in_minor(const SZrRawObject *object) {
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED ||
           object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN ||
           object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT;
}

static TZrBool garbage_collector_object_is_old_or_pinned_for_remembered(const SZrRawObject *object) {
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE ||
           object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED ||
           object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT ||
           object->garbageCollectMark.regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT;
}

static TZrBool garbage_collector_raw_object_is_live_young(const SZrRawObject *object) {
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE &&
           object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED &&
           object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
}

static TZrBool garbage_collector_value_references_live_young(const SZrTypeValue *value) {
    return value != ZR_NULL && ZrCore_Value_IsGarbageCollectable((SZrTypeValue *)value) &&
           garbage_collector_raw_object_is_live_young(value->value.object);
}

static TZrBool garbage_collector_hash_set_references_live_young(const SZrHashSet *set) {
    if (set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize bucketIndex = 0; bucketIndex < set->capacity; bucketIndex++) {
        for (SZrHashKeyValuePair *pair = set->buckets[bucketIndex]; pair != ZR_NULL; pair = pair->next) {
            if (garbage_collector_value_references_live_young(&pair->key) ||
                garbage_collector_value_references_live_young(&pair->value)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool garbage_collector_closure_references_live_young(SZrState *state, SZrRawObject *object) {
    if (state == ZR_NULL || object == ZR_NULL || object->type != ZR_RAW_OBJECT_TYPE_CLOSURE) {
        return ZR_FALSE;
    }

    if (object->isNative) {
        SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);

        if (closure == ZR_NULL) {
            return ZR_FALSE;
        }
        if (garbage_collector_raw_object_is_live_young(ZR_CAST_RAW_OBJECT_AS_SUPER(closure->aotShimFunction))) {
            return ZR_TRUE;
        }
        for (TZrSize index = 0; index < closure->closureValueCount; index++) {
            if (garbage_collector_raw_object_is_live_young(ZrCore_ClosureNative_GetCaptureOwner(closure, index)) ||
                garbage_collector_value_references_live_young(closure->closureValuesExtend[index])) {
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }

    {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);

        if (closure == ZR_NULL) {
            return ZR_FALSE;
        }
        if (garbage_collector_raw_object_is_live_young(ZR_CAST_RAW_OBJECT_AS_SUPER(closure->function))) {
            return ZR_TRUE;
        }
        for (TZrSize index = 0; index < closure->closureValueCount; index++) {
            if (garbage_collector_raw_object_is_live_young(
                        ZR_CAST_RAW_OBJECT_AS_SUPER(closure->closureValuesExtend[index]))) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool garbage_collector_native_data_references_live_young(SZrRawObject *object) {
    struct SZrNativeData *nativeData;

    if (object == ZR_NULL || object->type != ZR_RAW_OBJECT_TYPE_NATIVE_DATA) {
        return ZR_FALSE;
    }

    nativeData = ZR_CAST(struct SZrNativeData *, object);
    if (nativeData == ZR_NULL) {
        return ZR_FALSE;
    }
    if (object->scanMarkGcFunction != ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrUInt32 index = 0; index < nativeData->valueLength; index++) {
        if (garbage_collector_value_references_live_young(&nativeData->valueExtend[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool garbage_collector_object_references_live_young(SZrState *state, SZrRawObject *object) {
    if (state == ZR_NULL || object == ZR_NULL || !garbage_collector_object_can_hold_gc_references(object)) {
        return ZR_FALSE;
    }

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_OBJECT:
        case ZR_RAW_OBJECT_TYPE_ARRAY: {
            SZrObject *runtimeObject = ZR_CAST_OBJECT(state, object);

            if (runtimeObject == ZR_NULL) {
                return ZR_FALSE;
            }
            if (garbage_collector_raw_object_is_live_young(ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeObject->prototype)) ||
                garbage_collector_raw_object_is_live_young(
                        ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeObject->cachedHiddenItemsObject)) ||
                garbage_collector_hash_set_references_live_young(&runtimeObject->nodeMap)) {
                return ZR_TRUE;
            }
            if (runtimeObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                SZrObjectModule *module = (SZrObjectModule *)runtimeObject;

                return garbage_collector_raw_object_is_live_young(ZR_CAST_RAW_OBJECT_AS_SUPER(module->moduleName)) ||
                       garbage_collector_raw_object_is_live_young(ZR_CAST_RAW_OBJECT_AS_SUPER(module->fullPath)) ||
                       garbage_collector_hash_set_references_live_young(&module->proNodeMap);
            }
            return ZR_FALSE;
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE:
            return garbage_collector_closure_references_live_young(state, object);
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE: {
            SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, object);
            SZrTypeValue *value = closureValue != ZR_NULL ? ZrCore_ClosureValue_GetValue(closureValue) : ZR_NULL;

            return garbage_collector_value_references_live_young(value);
        }
        case ZR_RAW_OBJECT_TYPE_FUNCTION:
        case ZR_RAW_OBJECT_TYPE_THREAD:
            return ZR_TRUE;
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA:
            return garbage_collector_native_data_references_live_young(object);
        default:
            return ZR_FALSE;
    }
}

static void garbage_collector_prune_remembered_registry(SZrState *state) {
    SZrGarbageCollector *collector;
    TZrSize newCount = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    collector = state->global->garbageCollector;
    for (TZrSize index = 0; index < collector->rememberedObjectCount; index++) {
        SZrRawObject *object = collector->rememberedObjects[index];

        if (object == ZR_NULL ||
            !garbage_collector_object_is_old_or_pinned_for_remembered(object) ||
            ZrCore_RawObject_IsUnreferenced(state, object) ||
            object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED ||
            !garbage_collector_object_references_live_young(state, object)) {
            continue;
        }

        collector->rememberedObjects[newCount++] = object;
    }

    for (TZrSize index = newCount; index < collector->rememberedObjectCount; index++) {
        collector->rememberedObjects[index] = ZR_NULL;
    }

    collector->rememberedObjectCount = newCount;
    collector->statsSnapshot.rememberedObjectCount = (TZrUInt32)newCount;
}

static void garbage_collector_remember_promoted_minor_object(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global;
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || object == ZR_NULL) {
        return;
    }

    global = state->global;
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    if (object->garbageCollectMark.storageKind != ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE &&
        object->garbageCollectMark.storageKind != ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED) {
        return;
    }
    if (!garbage_collector_object_can_hold_gc_references(object)) {
        return;
    }

    collector = global->garbageCollector;
    if (garbage_collector_remembered_registry_contains(collector, object)) {
        return;
    }

    if (!garbage_collector_ensure_remembered_registry_capacity(global, collector->rememberedObjectCount + 1u)) {
        return;
    }

    collector->rememberedObjects[collector->rememberedObjectCount++] = object;
    collector->statsSnapshot.rememberedObjectCount = (TZrUInt32)collector->rememberedObjectCount;
}

static void garbage_collector_clear_forwarding_metadata(SZrGarbageCollector *collector) {
    SZrRawObject *object;

    if (collector == ZR_NULL) {
        return;
    }

    object = collector->gcObjectList;
    while (object != ZR_NULL) {
        object->garbageCollectMark.forwardingAddress = ZR_NULL;
        object->garbageCollectMark.forwardingRefLocation = ZR_NULL;
        object = object->next;
    }
}

static void garbage_collector_begin_minor_scan_epoch(SZrGarbageCollector *collector) {
    SZrRawObject *object;

    if (collector == ZR_NULL) {
        return;
    }

    collector->minorCollectionEpoch++;
    if (collector->minorCollectionEpoch != 0u) {
        return;
    }

    collector->minorCollectionEpoch = 1u;
    object = collector->gcObjectList;
    while (object != ZR_NULL) {
        object->garbageCollectMark.minorScanEpoch = 0u;
        object = object->next;
    }
}

static TZrSize garbage_collector_prepare_minor_collection(SZrState *state) {
    SZrGarbageCollector *collector;
    SZrRawObject *object;
    TZrSize work = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return 0;
    }

    collector = state->global->garbageCollector;
    garbage_collector_clear_forwarding_metadata(collector);
    object = collector->gcObjectList;
    while (object != ZR_NULL) {
        if (object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE &&
            object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED &&
            object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED &&
            object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT) {
            object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
            object->garbageCollectMark.generation = collector->gcGeneration;
            work++;
        }

        object = object->next;
    }

    return work;
}

static TZrSize garbage_collector_mark_minor_root_object(SZrState *state, SZrRawObject *object) {
    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return 0;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID ||
        ZrCore_RawObject_IsReleased(object)) {
        return 0;
    }

    garbage_collector_mark_object(state, object);
    return 1;
}

static TZrSize garbage_collector_mark_minor_root_value(SZrState *state, SZrTypeValue *value) {
    if (value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value) || value->value.object == ZR_NULL) {
        return 0;
    }

    garbage_collector_mark_value(state, value);
    return 1;
}

static TZrSize garbage_collector_restart_minor_collection(SZrState *state) {
    SZrGlobalState *global;
    SZrGarbageCollector *collector;
    TZrSize work = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return 0;
    }

    global = state->global;
    collector = global->garbageCollector;
    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->waitToReleaseObjectList = ZR_NULL;
    collector->releasedObjectList = ZR_NULL;
    garbage_collector_prune_remembered_registry(state);
    garbage_collector_begin_minor_scan_epoch(collector);

    work += garbage_collector_prepare_minor_collection(state);
    work += garbage_collector_mark_minor_root_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(state));
    work += garbage_collector_mark_minor_root_value(state, &global->loadedModulesRegistry);
    work += garbage_collector_mark_minor_root_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->errorPrototype));
    work += garbage_collector_mark_minor_root_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->stackFramePrototype));
    if (global->hasUnhandledExceptionHandler) {
        work += garbage_collector_mark_minor_root_value(state, &global->unhandledExceptionHandler);
    }

    work += garbage_collector_mark_string_roots(state);
    work += garbage_collector_mark_ignored_roots(state);

    for (TZrSize i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        work += garbage_collector_mark_minor_root_object(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->basicTypeObjectPrototype[i]));
    }

    work += garbage_collector_mark_minor_root_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->mainThreadState));

    for (TZrSize rememberedIndex = 0; rememberedIndex < collector->rememberedObjectCount; rememberedIndex++) {
        work += garbage_collector_mark_minor_root_object(state, collector->rememberedObjects[rememberedIndex]);
    }

    return work;
}

static TZrBool garbage_collector_rewrite_value_if_forwarded(SZrTypeValue *value) {
    SZrRawObject *forwardedObject;

    if (value == ZR_NULL || !value->isGarbageCollectable || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    forwardedObject = (SZrRawObject *)value->value.object->garbageCollectMark.forwardingAddress;
    if (forwardedObject == ZR_NULL) {
        return ZR_FALSE;
    }

    value->value.object = forwardedObject;
    value->type = (EZrValueType)forwardedObject->type;
    value->isNative = forwardedObject->isNative;
    return ZR_TRUE;
}

static void garbage_collector_rewrite_raw_object_slot(SZrRawObject **slot) {
    if (slot == ZR_NULL || *slot == ZR_NULL) {
        return;
    }

    if ((*slot)->garbageCollectMark.forwardingAddress != ZR_NULL) {
        *slot = (SZrRawObject *)(*slot)->garbageCollectMark.forwardingAddress;
    }
}

static TZrSize garbage_collector_rewrite_raw_object_slot_counted(SZrRawObject **slot) {
    SZrRawObject *before;

    if (slot == ZR_NULL || *slot == ZR_NULL) {
        return 0;
    }

    before = *slot;
    garbage_collector_rewrite_raw_object_slot(slot);
    return *slot != before ? 1u : 0u;
}

static TZrSize garbage_collector_rewrite_raw_object_registry(SZrRawObject **items, TZrSize count) {
    TZrSize work = 0;
    TZrSize index;

    if (items == ZR_NULL) {
        return 0;
    }

    for (index = 0; index < count; index++) {
        SZrRawObject *before = items[index];

        garbage_collector_rewrite_raw_object_slot(&items[index]);
        if (items[index] != before) {
            work++;
        }
    }

    return work;
}

static TZrSize garbage_collector_rewrite_hash_set(SZrHashSet *set) {
    TZrSize work = 0;

    if (set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL) {
        return 0;
    }

    for (TZrSize bucketIndex = 0; bucketIndex < set->capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = set->buckets[bucketIndex];

        while (pair != ZR_NULL) {
            if (garbage_collector_rewrite_value_if_forwarded(&pair->key)) {
                work++;
            }
            if (garbage_collector_rewrite_value_if_forwarded(&pair->value)) {
                work++;
            }
            pair = pair->next;
        }
    }

    return work;
}

static TZrSize garbage_collector_rewrite_thread_frame_slots(SZrState *threadState) {
    TZrSize work = 0;
    SZrCallInfo *callInfo;
    TZrStackValuePointer youngerFrameBase = ZR_NULL;

    if (threadState == ZR_NULL) {
        return 0;
    }

    callInfo = threadState->callInfoList;
    while (callInfo != ZR_NULL) {
        if (callInfo->functionBase.valuePointer != ZR_NULL) {
            TZrStackValuePointer funcBase = callInfo->functionBase.valuePointer;
            TZrStackValuePointer funcTop = youngerFrameBase;

            if (funcTop == ZR_NULL) {
                funcTop = ZR_CALL_INFO_IS_VM(callInfo) && callInfo->functionTop.valuePointer != ZR_NULL
                                  ? callInfo->functionTop.valuePointer
                                  : threadState->stackTop.valuePointer;
            }

            if (funcTop != ZR_NULL && funcTop > funcBase) {
                while (funcBase < funcTop) {
                    if (garbage_collector_rewrite_value_if_forwarded(&funcBase->value)) {
                        work++;
                    }
                    funcBase++;
                }
            }

            youngerFrameBase = callInfo->functionBase.valuePointer;
        }

        callInfo = callInfo->previous;
    }

    return work;
}

static TZrSize garbage_collector_rewrite_string_slot(SZrString **slot) {
    SZrString *beforeString;

    if (slot == ZR_NULL || *slot == ZR_NULL) {
        return 0;
    }

    beforeString = *slot;
    garbage_collector_rewrite_raw_object_slot((SZrRawObject **)slot);
    return *slot != beforeString ? 1u : 0u;
}

static TZrSize garbage_collector_rewrite_function_slot(SZrFunction **slot) {
    return garbage_collector_rewrite_raw_object_slot_counted((SZrRawObject **)slot);
}

static TZrSize garbage_collector_rewrite_function_graph(SZrState *state, SZrFunction *function);

static TZrSize garbage_collector_rewrite_function_entry_slot(SZrState *state, SZrFunction **slot) {
    TZrSize work;
    SZrRawObject *callableObject;

    if (state == ZR_NULL || slot == ZR_NULL) {
        return 0;
    }

    work = garbage_collector_rewrite_function_slot(slot);
    callableObject = ZR_CAST_RAW_OBJECT_AS_SUPER(*slot);
    if (callableObject != ZR_NULL && callableObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        work += garbage_collector_rewrite_function_graph(state, *slot);
    }

    return work;
}

static TZrSize garbage_collector_rewrite_object_prototype_slot(SZrObjectPrototype **slot) {
    return garbage_collector_rewrite_raw_object_slot_counted((SZrRawObject **)slot);
}

static TZrSize garbage_collector_rewrite_meta_table(SZrState *state, SZrMetaTable *table) {
    TZrSize work = 0;

    if (state == ZR_NULL || table == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 metaIndex = 0; metaIndex < ZR_META_ENUM_MAX; metaIndex++) {
        SZrMeta *meta = table->metas[metaIndex];

        if (meta != ZR_NULL) {
            work += garbage_collector_rewrite_function_entry_slot(state, &meta->function);
        }
    }

    return work;
}

static TZrSize garbage_collector_rewrite_object_prototype_graph(SZrState *state, SZrObjectPrototype *prototype) {
    TZrSize work = 0;

    if (prototype == ZR_NULL) {
        return 0;
    }

    work += garbage_collector_rewrite_string_slot(&prototype->name);
    work += garbage_collector_rewrite_object_prototype_slot(&prototype->superPrototype);
    work += garbage_collector_rewrite_meta_table(state, &prototype->metaTable);

    for (TZrUInt32 memberIndex = 0; memberIndex < prototype->memberDescriptorCount; memberIndex++) {
        SZrMemberDescriptor *descriptor = &prototype->memberDescriptors[memberIndex];

        work += garbage_collector_rewrite_string_slot(&descriptor->name);
        work += garbage_collector_rewrite_function_entry_slot(state, &descriptor->getterFunction);
        work += garbage_collector_rewrite_function_entry_slot(state, &descriptor->setterFunction);
        work += garbage_collector_rewrite_string_slot(&descriptor->ownerTypeName);
        work += garbage_collector_rewrite_string_slot(&descriptor->baseDefinitionOwnerTypeName);
        work += garbage_collector_rewrite_string_slot(&descriptor->baseDefinitionName);
    }

    work += garbage_collector_rewrite_function_entry_slot(state, &prototype->indexContract.getByIndexFunction);
    work += garbage_collector_rewrite_function_entry_slot(state, &prototype->indexContract.setByIndexFunction);
    work += garbage_collector_rewrite_function_entry_slot(state, &prototype->indexContract.containsKeyFunction);
    work += garbage_collector_rewrite_function_entry_slot(state, &prototype->indexContract.getLengthFunction);
    work += garbage_collector_rewrite_function_entry_slot(state, &prototype->iterableContract.iterInitFunction);
    work += garbage_collector_rewrite_function_entry_slot(state, &prototype->iteratorContract.moveNextFunction);
    work += garbage_collector_rewrite_function_entry_slot(state, &prototype->iteratorContract.currentFunction);
    work += garbage_collector_rewrite_string_slot(&prototype->iteratorContract.currentMemberName);

    for (TZrUInt32 fieldIndex = 0; fieldIndex < prototype->managedFieldCount; fieldIndex++) {
        work += garbage_collector_rewrite_string_slot(&prototype->managedFields[fieldIndex].name);
    }

    if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        SZrStructPrototype *structPrototype = (SZrStructPrototype *)prototype;

        work += garbage_collector_rewrite_hash_set(&structPrototype->keyOffsetMap);
    }

    return work;
}

static TZrSize garbage_collector_rewrite_typed_type_ref(SZrFunctionTypedTypeRef *typeRef) {
    TZrSize work = 0;

    if (typeRef == ZR_NULL) {
        return 0;
    }

    work += garbage_collector_rewrite_string_slot(&typeRef->typeName);
    work += garbage_collector_rewrite_string_slot(&typeRef->elementTypeName);
    return work;
}

static TZrSize garbage_collector_rewrite_metadata_parameters(SZrFunctionMetadataParameter *parameters,
                                                             TZrUInt32 parameterCount) {
    TZrSize work = 0;

    if (parameters == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        SZrFunctionMetadataParameter *parameter = &parameters[index];

        work += garbage_collector_rewrite_string_slot(&parameter->name);
        work += garbage_collector_rewrite_typed_type_ref(&parameter->type);
        if (parameter->hasDefaultValue &&
            garbage_collector_rewrite_value_if_forwarded(&parameter->defaultValue)) {
            work++;
        }
        if (parameter->hasDecoratorMetadata &&
            garbage_collector_rewrite_value_if_forwarded(&parameter->decoratorMetadataValue)) {
            work++;
        }
        if (parameter->decoratorNames != ZR_NULL) {
            for (TZrUInt32 decoratorIndex = 0; decoratorIndex < parameter->decoratorCount; decoratorIndex++) {
                work += garbage_collector_rewrite_string_slot(&parameter->decoratorNames[decoratorIndex]);
            }
        }
    }

    return work;
}

static TZrSize garbage_collector_rewrite_call_info_functions(SZrState *state, SZrState *threadState);

static TZrSize garbage_collector_rewrite_function_graph(SZrState *state, SZrFunction *function) {
    TZrSize work = 0;

    if (state == ZR_NULL || function == ZR_NULL) {
        return 0;
    }

    garbage_collector_trace_rewrite_function_slot("ownerFunction", function, function->ownerFunction);
    work += garbage_collector_rewrite_function_slot(&function->ownerFunction);
    work += garbage_collector_rewrite_string_slot(&function->functionName);
    work += garbage_collector_rewrite_string_slot(&function->sourceCodeList);
    work += garbage_collector_rewrite_string_slot(&function->sourceHash);

    for (TZrUInt32 index = 0; index < function->closureValueLength; index++) {
        work += garbage_collector_rewrite_string_slot(&function->closureValueList[index].name);
    }
    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        if (garbage_collector_rewrite_value_if_forwarded(&function->constantValueList[index])) {
            work++;
        }
    }
    for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
        work += garbage_collector_rewrite_string_slot(&function->localVariableList[index].name);
    }
    for (TZrUInt32 index = 0; index < function->catchClauseCount; index++) {
        work += garbage_collector_rewrite_string_slot(&function->catchClauseList[index].typeName);
    }
    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        work += garbage_collector_rewrite_function_graph(state, &function->childFunctionList[index]);
    }
    for (TZrUInt32 index = 0; index < function->exportedVariableLength; index++) {
        work += garbage_collector_rewrite_string_slot(&function->exportedVariables[index].name);
    }
    for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
        work += garbage_collector_rewrite_string_slot(&function->typedLocalBindings[index].name);
        work += garbage_collector_rewrite_typed_type_ref(&function->typedLocalBindings[index].type);
    }
    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];

        work += garbage_collector_rewrite_string_slot(&symbol->name);
        work += garbage_collector_rewrite_typed_type_ref(&symbol->valueType);
        if (symbol->parameterTypes != ZR_NULL) {
            for (TZrUInt32 parameterIndex = 0; parameterIndex < symbol->parameterCount; parameterIndex++) {
                work += garbage_collector_rewrite_typed_type_ref(&symbol->parameterTypes[parameterIndex]);
            }
        }
    }
    if (function->staticImports != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->staticImportLength; index++) {
            work += garbage_collector_rewrite_string_slot(&function->staticImports[index]);
        }
    }
    if (function->moduleEntryEffects != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
            work += garbage_collector_rewrite_string_slot(&function->moduleEntryEffects[index].moduleName);
            work += garbage_collector_rewrite_string_slot(&function->moduleEntryEffects[index].symbolName);
        }
    }
    if (function->exportedCallableSummaries != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->exportedCallableSummaryLength; index++) {
            SZrFunctionCallableSummary *summary = &function->exportedCallableSummaries[index];

            work += garbage_collector_rewrite_string_slot(&summary->name);
            if (summary->effects != ZR_NULL) {
                for (TZrUInt32 effectIndex = 0; effectIndex < summary->effectCount; effectIndex++) {
                    work += garbage_collector_rewrite_string_slot(&summary->effects[effectIndex].moduleName);
                    work += garbage_collector_rewrite_string_slot(&summary->effects[effectIndex].symbolName);
                }
            }
        }
    }
    if (function->topLevelCallableBindings != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->topLevelCallableBindingLength; index++) {
            work += garbage_collector_rewrite_string_slot(&function->topLevelCallableBindings[index].name);
        }
    }

    work += garbage_collector_rewrite_metadata_parameters(function->parameterMetadata,
                                                          function->parameterMetadataCount);

    if (function->compileTimeVariableInfos != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->compileTimeVariableInfoLength; index++) {
            SZrFunctionCompileTimeVariableInfo *info = &function->compileTimeVariableInfos[index];

            work += garbage_collector_rewrite_string_slot(&info->name);
            work += garbage_collector_rewrite_typed_type_ref(&info->type);
            if (info->pathBindings != ZR_NULL) {
                for (TZrUInt32 bindingIndex = 0; bindingIndex < info->pathBindingCount; bindingIndex++) {
                    work += garbage_collector_rewrite_string_slot(&info->pathBindings[bindingIndex].path);
                    work += garbage_collector_rewrite_string_slot(&info->pathBindings[bindingIndex].targetName);
                }
            }
        }
    }
    if (function->compileTimeFunctionInfos != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->compileTimeFunctionInfoLength; index++) {
            SZrFunctionCompileTimeFunctionInfo *info = &function->compileTimeFunctionInfos[index];

            work += garbage_collector_rewrite_string_slot(&info->name);
            work += garbage_collector_rewrite_typed_type_ref(&info->returnType);
            work += garbage_collector_rewrite_metadata_parameters(info->parameters, info->parameterCount);
        }
    }
    if (function->escapeBindings != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->escapeBindingLength; index++) {
            work += garbage_collector_rewrite_string_slot(&function->escapeBindings[index].name);
        }
    }
    if (function->testInfos != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->testInfoLength; index++) {
            SZrFunctionTestInfo *info = &function->testInfos[index];

            work += garbage_collector_rewrite_string_slot(&info->name);
            work += garbage_collector_rewrite_metadata_parameters(info->parameters, info->parameterCount);
        }
    }
    if (function->hasDecoratorMetadata &&
        garbage_collector_rewrite_value_if_forwarded(&function->decoratorMetadataValue)) {
        work++;
    }
    if (function->decoratorNames != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->decoratorCount; index++) {
            work += garbage_collector_rewrite_string_slot(&function->decoratorNames[index]);
        }
    }
    if (function->memberEntries != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->memberEntryLength; index++) {
            work += garbage_collector_rewrite_string_slot(&function->memberEntries[index].symbol);
        }
    }
    if (function->prototypeInstances != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->prototypeInstancesLength; index++) {
            garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&function->prototypeInstances[index]);
        }
    }
    if (function->semIrTypeTable != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->semIrTypeTableLength; index++) {
            work += garbage_collector_rewrite_typed_type_ref(&function->semIrTypeTable[index]);
        }
    }
    if (function->callSiteCaches != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->callSiteCacheLength; index++) {
            SZrFunctionCallSiteCacheEntry *cacheEntry = &function->callSiteCaches[index];

            garbage_collector_sanitize_callsite_cache_pic(function, index, "rewrite", cacheEntry);
            TZrUInt32 picLimit = cacheEntry->picSlotCount;

            if (picLimit > ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY) {
                picLimit = ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY;
            }
            for (TZrUInt32 picIndex = 0; picIndex < picLimit; picIndex++) {
                garbage_collector_rewrite_raw_object_slot(
                        (SZrRawObject **)&cacheEntry->picSlots[picIndex].cachedReceiverPrototype);
                garbage_collector_rewrite_raw_object_slot(
                        (SZrRawObject **)&cacheEntry->picSlots[picIndex].cachedOwnerPrototype);
                garbage_collector_trace_rewrite_function_slot("callSiteCachedFunction",
                                                              function,
                                                              cacheEntry->picSlots[picIndex].cachedFunction);
                garbage_collector_rewrite_raw_object_slot(
                        (SZrRawObject **)&cacheEntry->picSlots[picIndex].cachedFunction);
            }
        }
    }

    garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&function->runtimeDecoratorMetadata);
    garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&function->runtimeDecoratorDecorators);
    garbage_collector_trace_rewrite_function_slot("cachedStatelessClosure",
                                                  function,
                                                  ZR_CAST(SZrFunction *, function->cachedStatelessClosure));
    garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&function->cachedStatelessClosure);

    return work;
}

static TZrSize garbage_collector_rewrite_call_info_functions(SZrState *state, SZrState *threadState) {
    TZrSize work = 0;
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || threadState == ZR_NULL) {
        return 0;
    }

    callInfo = threadState->callInfoList;
    while (callInfo != ZR_NULL) {
        SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(threadState, callInfo);
        if (function != ZR_NULL) {
            work += garbage_collector_rewrite_function_graph(state, function);
        }
        callInfo = callInfo->previous;
    }

    return work;
}

static TZrSize garbage_collector_rewrite_object_graph(SZrState *state, SZrRawObject *object) {
    TZrSize work = 0;

    if (state == ZR_NULL || object == ZR_NULL) {
        return 0;
    }

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_OBJECT:
        case ZR_RAW_OBJECT_TYPE_ARRAY: {
            SZrObject *runtimeObject = ZR_CAST_OBJECT(state, object);

            garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&runtimeObject->prototype);
            garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&runtimeObject->cachedHiddenItemsObject);
            work += garbage_collector_rewrite_hash_set(&runtimeObject->nodeMap);
            if (runtimeObject->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
                work += garbage_collector_rewrite_object_prototype_graph(state, (SZrObjectPrototype *)runtimeObject);
            }

            if (runtimeObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                SZrObjectModule *module = (SZrObjectModule *)runtimeObject;

                work += garbage_collector_rewrite_string_slot(&module->moduleName);
                work += garbage_collector_rewrite_string_slot(&module->fullPath);
                work += garbage_collector_rewrite_hash_set(&module->proNodeMap);
                for (TZrUInt32 descriptorIndex = 0; descriptorIndex < module->exportDescriptorLength; descriptorIndex++) {
                    work += garbage_collector_rewrite_string_slot(&module->exportDescriptors[descriptorIndex].name);
                }
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE: {
            if (object->isNative) {
                SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);
                SZrRawObject **captureOwners = ZrCore_ClosureNative_GetCaptureOwners(closure);

                work += garbage_collector_rewrite_function_entry_slot(state, &closure->aotShimFunction);
                for (TZrSize captureIndex = 0; captureIndex < closure->closureValueCount; captureIndex++) {
                    if (captureOwners != ZR_NULL && captureOwners[captureIndex] != ZR_NULL) {
                        garbage_collector_rewrite_raw_object_slot(&captureOwners[captureIndex]);
                        work++;
                    } else if (closure->closureValuesExtend[captureIndex] != ZR_NULL &&
                               garbage_collector_rewrite_value_if_forwarded(closure->closureValuesExtend[captureIndex])) {
                        work++;
                    }
                }
            } else {
                SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);

                work += garbage_collector_rewrite_function_entry_slot(state, &closure->function);
                for (TZrSize captureIndex = 0; captureIndex < closure->closureValueCount; captureIndex++) {
                    garbage_collector_rewrite_raw_object_slot(
                            (SZrRawObject **)&closure->closureValuesExtend[captureIndex]);
                }
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE: {
            SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, object);

            if (ZrCore_ClosureValue_IsClosed(closureValue)) {
                if (garbage_collector_rewrite_value_if_forwarded(&closureValue->link.closedValue)) {
                    work++;
                }
                closureValue->value.valuePointer = ZR_CAST_STACK_VALUE(&closureValue->link.closedValue);
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_FUNCTION: {
            work += garbage_collector_rewrite_function_graph(state, ZR_CAST_FUNCTION(state, object));
            break;
        }
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA: {
            struct SZrNativeData *nativeData = (struct SZrNativeData *)object;

            for (TZrUInt32 valueIndex = 0; valueIndex < nativeData->valueLength; valueIndex++) {
                if (garbage_collector_rewrite_value_if_forwarded(&nativeData->valueExtend[valueIndex])) {
                    work++;
                }
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_THREAD: {
            SZrState *threadState = (SZrState *)object;

            work += garbage_collector_rewrite_thread_frame_slots(threadState);
            work += garbage_collector_rewrite_call_info_functions(state, threadState);

            if (threadState->hasCurrentException &&
                garbage_collector_rewrite_value_if_forwarded(&threadState->currentException)) {
                work++;
            }
            if (threadState->pendingControl.hasValue &&
                garbage_collector_rewrite_value_if_forwarded(&threadState->pendingControl.value)) {
                work++;
            }
            break;
        }
        default:
            break;
    }

    return work;
}

static TZrSize garbage_collector_rewrite_forwarded_roots(SZrState *state) {
    SZrGlobalState *global;
    TZrSize work = 0;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return 0;
    }

    global = state->global;

    if (garbage_collector_rewrite_value_if_forwarded(&global->loadedModulesRegistry)) {
        work++;
    }
    if (global->hasUnhandledExceptionHandler &&
        garbage_collector_rewrite_value_if_forwarded(&global->unhandledExceptionHandler)) {
        work++;
    }

    garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&global->memoryErrorMessage);
    garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&global->errorPrototype);
    garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&global->stackFramePrototype);
    garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&global->mainThreadState);

    for (TZrSize bucketIndex = 0; bucketIndex < ZR_GLOBAL_API_STRING_CACHE_BUCKET_COUNT; bucketIndex++) {
        for (TZrSize depthIndex = 0; depthIndex < ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH; depthIndex++) {
            garbage_collector_rewrite_raw_object_slot(
                    (SZrRawObject **)&global->stringHashApiCache[bucketIndex][depthIndex]);
        }
    }

    for (TZrSize metaIndex = 0; metaIndex < ZR_META_ENUM_MAX; metaIndex++) {
        garbage_collector_rewrite_raw_object_slot((SZrRawObject **)&global->metaFunctionName[metaIndex]);
    }

    for (TZrSize prototypeIndex = 0; prototypeIndex < ZR_VALUE_TYPE_ENUM_MAX; prototypeIndex++) {
        garbage_collector_rewrite_raw_object_slot(
                (SZrRawObject **)&global->basicTypeObjectPrototype[prototypeIndex]);
    }

    if (global->stringTable != ZR_NULL) {
        work += garbage_collector_rewrite_hash_set(&global->stringTable->stringHashSet);
    }

    if (global->garbageCollector != ZR_NULL) {
        work += garbage_collector_rewrite_raw_object_registry(global->garbageCollector->ignoredObjects,
                                                              global->garbageCollector->ignoredObjectCount);
        work += garbage_collector_rewrite_raw_object_registry(global->garbageCollector->rememberedObjects,
                                                              global->garbageCollector->rememberedObjectCount);
    }

    work += garbage_collector_rewrite_object_graph(state, ZR_CAST_RAW_OBJECT_AS_SUPER(state));
    if (global->mainThreadState != ZR_NULL && global->mainThreadState != state) {
        work += garbage_collector_rewrite_object_graph(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->mainThreadState));
    }

    return work;
}

static void garbage_collector_minor_target_for_object(SZrGarbageCollector *collector,
                                                      const SZrRawObject *object,
                                                      EZrGarbageCollectRegionKind *outRegionKind,
                                                      EZrGarbageCollectStorageKind *outStorageKind,
                                                      EZrGarbageCollectGenerationalObjectStatus *outGenerationalStatus,
                                                      EZrGarbageCollectPromotionReason *outPromotionReason,
                                                      TZrUInt32 *outSurvivalAge) {
    TZrUInt32 newAge = object->garbageCollectMark.survivalAge + 1u;
    EZrGarbageCollectPromotionReason reason = object->garbageCollectMark.promotionReason;

    if (reason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE) {
        reason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL;
    }

    if (object->garbageCollectMark.pinFlags != ZR_GARBAGE_COLLECT_PIN_KIND_NONE) {
        *outRegionKind = ZR_GARBAGE_COLLECT_REGION_KIND_PINNED;
        *outStorageKind = ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED;
        *outGenerationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_ALIVE;
        *outPromotionReason = object->garbageCollectMark.promotionReason != ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE
                                      ? object->garbageCollectMark.promotionReason
                                      : ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED;
        *outSurvivalAge = newAge;
        return;
    }

    if (object->garbageCollectMark.escapeFlags != ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE ||
        newAge >= collector->survivorAgeThreshold) {
        *outRegionKind = ZR_GARBAGE_COLLECT_REGION_KIND_OLD;
        *outStorageKind = ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE;
        *outGenerationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_ALIVE;
        *outPromotionReason = object->garbageCollectMark.promotionReason != ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE
                                      ? object->garbageCollectMark.promotionReason
                                      : reason;
        *outSurvivalAge = newAge;
        return;
    }

    *outRegionKind = ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR;
    *outStorageKind = ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE;
    *outGenerationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL;
    *outPromotionReason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL;
    *outSurvivalAge = newAge;
}

static TZrBool garbage_collector_object_supports_evacuation(SZrState *state, SZrRawObject *object) {
    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_STRING:
        case ZR_RAW_OBJECT_TYPE_OBJECT:
        case ZR_RAW_OBJECT_TYPE_ARRAY:
        case ZR_RAW_OBJECT_TYPE_BUFFER:
        case ZR_RAW_OBJECT_TYPE_CLOSURE:
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA:
            return ZR_TRUE;
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE: {
            SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, object);
            return closureValue != ZR_NULL && ZrCore_ClosureValue_IsClosed(closureValue);
        }
        default:
            return ZR_FALSE;
    }
}

static void garbage_collector_apply_minor_target(SZrState *state,
                                                 SZrRawObject *object,
                                                 TZrUInt32 regionId,
                                                 EZrGarbageCollectRegionKind regionKind,
                                                 EZrGarbageCollectStorageKind storageKind,
                                                 EZrGarbageCollectGenerationalObjectStatus generationalStatus,
                                                 EZrGarbageCollectPromotionReason promotionReason,
                                                 TZrUInt32 survivalAge) {
    if (state == ZR_NULL || object == ZR_NULL) {
        return;
    }

    ZrCore_RawObject_SetStorageKind(object, storageKind);
    ZrCore_RawObject_SetRegionKind(object, regionKind);
    object->garbageCollectMark.regionId = regionId;
    object->garbageCollectMark.generationalStatus = generationalStatus;
    object->garbageCollectMark.promotionReason = promotionReason;
    object->garbageCollectMark.survivalAge = survivalAge;
    object->garbageCollectMark.generation = state->global->garbageCollector->gcGeneration;
}

static void garbage_collector_reassign_minor_target(SZrState *state,
                                                 SZrRawObject *object,
                                                 EZrGarbageCollectRegionKind regionKind,
                                                 EZrGarbageCollectStorageKind storageKind,
                                                 EZrGarbageCollectGenerationalObjectStatus generationalStatus,
                                                 EZrGarbageCollectPromotionReason promotionReason,
                                                 TZrUInt32 survivalAge,
                                                 TZrSize objectSize) {
    TZrUInt32 previousRegionId = object->garbageCollectMark.regionId;
    TZrUInt32 regionId;

    regionId = garbage_collector_reassign_region_id(state->global, previousRegionId, regionKind, objectSize);
    garbage_collector_apply_minor_target(state,
                                         object,
                                         regionId,
                                         regionKind,
                                         storageKind,
                                         generationalStatus,
                                         promotionReason,
                                         survivalAge);
}

static SZrRawObject *garbage_collector_clone_for_minor_evacuation(
        SZrState *state,
        SZrRawObject *object,
        EZrGarbageCollectRegionKind regionKind,
        EZrGarbageCollectStorageKind storageKind,
        EZrGarbageCollectGenerationalObjectStatus generationalStatus,
        EZrGarbageCollectPromotionReason promotionReason,
        TZrUInt32 survivalAge) {
    TZrSize objectSize;
    SZrRawObject *cloneObject;
    SZrRawObject *insertedNext;
    TZrUInt32 cloneRegionId;
    TZrBool wasClosedClosureValue = ZR_FALSE;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    if (object->type == ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE) {
        wasClosedClosureValue = ZrCore_ClosureValue_IsClosed(ZR_CAST_VM_CLOSURE_VALUE(state, object));
    }

    objectSize = garbage_collector_get_object_base_size(state, object);
    cloneObject = garbage_collector_new_raw_object_in_region(state,
                                                             (EZrValueType)object->type,
                                                             objectSize,
                                                             object->isNative,
                                                             regionKind,
                                                             storageKind);
    if (cloneObject == ZR_NULL) {
        return ZR_NULL;
    }

    cloneRegionId = cloneObject->garbageCollectMark.regionId;
    insertedNext = cloneObject->next;
    ZrCore_Memory_RawCopy(cloneObject, object, objectSize);
    cloneObject->next = insertedNext;
    cloneObject->gcList = ZR_NULL;
    cloneObject->garbageCollectMark.forwardingAddress = ZR_NULL;
    cloneObject->garbageCollectMark.forwardingRefLocation = ZR_NULL;
    garbage_collector_apply_minor_target(state,
                                         cloneObject,
                                         cloneRegionId,
                                         regionKind,
                                         storageKind,
                                         generationalStatus,
                                         promotionReason,
                                         survivalAge);

    if (cloneObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE) {
        SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, cloneObject);

        if (closureValue != ZR_NULL && wasClosedClosureValue) {
            closureValue->value.valuePointer = ZR_CAST_STACK_VALUE(&closureValue->link.closedValue);
        }
    }

    object->garbageCollectMark.forwardingAddress = cloneObject;
    object->garbageCollectMark.forwardingRefLocation = ZR_NULL;
    return cloneObject;
}

static TZrBool garbage_collector_object_is_live_after_major(SZrState *state, SZrRawObject *object) {
    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return ZR_FALSE;
    }

    return !ZrCore_RawObject_IsUnreferenced(state, object) &&
           object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
}

static TZrBool garbage_collector_object_can_old_compact(SZrState *state, SZrRawObject *object) {
    SZrGarbageCollector *collector;

    if (!garbage_collector_object_is_live_after_major(state, object)) {
        return ZR_FALSE;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object->garbageCollectMark.storageKind != ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE ||
        object->garbageCollectMark.regionKind != ZR_GARBAGE_COLLECT_REGION_KIND_OLD) {
        return ZR_FALSE;
    }

    collector = state->global->garbageCollector;
    if (garbage_collector_ignore_registry_contains(collector, object) ||
        object->ownershipControl != ZR_NULL ||
        object->scanMarkGcFunction != ZR_NULL) {
        return ZR_FALSE;
    }

    return garbage_collector_object_supports_evacuation(state, object);
}

static TZrBool garbage_collector_old_compactable_region_seen_before(SZrState *state,
                                                                    SZrRawObject *head,
                                                                    const SZrRawObject *limit,
                                                                    TZrUInt32 regionId) {
    SZrRawObject *object = head;

    while (object != ZR_NULL && object != limit) {
        if (garbage_collector_object_can_old_compact(state, object) &&
            object->garbageCollectMark.regionId == regionId) {
            return ZR_TRUE;
        }
        object = object->next;
    }

    return ZR_FALSE;
}

static TZrUInt64 garbage_collector_old_region_capacity(const SZrGarbageCollector *collector) {
    TZrUInt64 youngCapacity;
    TZrUInt64 multiplier;
    TZrUInt64 oldCapacity;

    if (collector == ZR_NULL) {
        return 1u;
    }

    youngCapacity = collector->youngRegionSize > 0 ? collector->youngRegionSize : 1u;
    multiplier = collector->youngRegionCountTarget > 0 ? collector->youngRegionCountTarget : 1u;
    oldCapacity = youngCapacity * multiplier;
    return oldCapacity > 0 ? oldCapacity : youngCapacity;
}

static TZrBool garbage_collector_should_compact_old_regions(SZrState *state, TZrBool forceCompact) {
    SZrGarbageCollector *collector;
    SZrRawObject *object;
    TZrUInt64 liveBytes = 0u;
    TZrUInt64 capacity;
    TZrUInt64 fragmentationPercent;
    TZrUInt32 liveRegionCount = 0u;

    if (forceCompact) {
        return ZR_TRUE;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = state->global->garbageCollector;
    if (collector->fragmentationCompactThreshold >= 100u) {
        return ZR_FALSE;
    }

    object = collector->gcObjectList;
    while (object != ZR_NULL) {
        if (garbage_collector_object_can_old_compact(state, object)) {
            liveBytes += (TZrUInt64)garbage_collector_get_object_base_size(state, object);
            if (object->garbageCollectMark.regionId != 0u &&
                !garbage_collector_old_compactable_region_seen_before(state,
                                                                      collector->gcObjectList,
                                                                      object,
                                                                      object->garbageCollectMark.regionId)) {
                liveRegionCount++;
            }
        }
        object = object->next;
    }

    if (liveBytes == 0u || liveRegionCount <= 1u) {
        return ZR_FALSE;
    }

    capacity = garbage_collector_old_region_capacity(collector) * (TZrUInt64)liveRegionCount;
    if (capacity <= liveBytes) {
        return ZR_FALSE;
    }

    fragmentationPercent = (capacity - liveBytes) * 100u / capacity;
    return fragmentationPercent >= collector->fragmentationCompactThreshold;
}

static TZrSize garbage_collector_free_old_from_space(SZrState *state) {
    SZrRawObject **current;
    TZrSize work = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return 0;
    }

    current = &state->global->garbageCollector->gcObjectList;
    while (*current != ZR_NULL) {
        SZrRawObject *object = *current;

        if (object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE &&
            object->garbageCollectMark.forwardingAddress != ZR_NULL) {
            *current = object->next;
            garbage_collector_free_object(state, object);
            work++;
            continue;
        }

        current = &object->next;
    }

    return work;
}

static TZrSize garbage_collector_run_old_compaction(SZrState *state) {
    SZrGarbageCollector *collector;
    SZrRawObject *object;
    TZrSize work = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return 0;
    }

    collector = state->global->garbageCollector;
    garbage_collector_clear_forwarding_metadata(collector);
    collector->currentOldRegionId = 0u;
    collector->currentOldRegionUsedBytes = 0u;

    object = collector->gcObjectList;
    while (object != ZR_NULL) {
        SZrRawObject *nextObject = object->next;

        if (garbage_collector_object_can_old_compact(state, object) &&
            garbage_collector_clone_for_minor_evacuation(state,
                                                         object,
                                                         ZR_GARBAGE_COLLECT_REGION_KIND_OLD,
                                                         ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                                         object->garbageCollectMark.generationalStatus,
                                                         object->garbageCollectMark.promotionReason,
                                                         object->garbageCollectMark.survivalAge) != ZR_NULL) {
            work++;
        }

        object = nextObject;
    }

    work += garbage_collector_rewrite_forwarded_roots(state);

    object = collector->gcObjectList;
    while (object != ZR_NULL) {
        /*
         * Keep eligibility aligned with garbage_collector_rewrite_minor_forwarding: rewrite interior slots for any
         * object that is still linked on the GC list unless it is explicitly unreferenced or released. Using only
         * garbage_collector_object_is_live_after_major here can skip objects that still carry forwarding edges (for
         * example INITED objects with a stale generation snapshot) and leave external slots (meta tables, PIC caches)
         * pointing at stale addresses before old-space reclamation.
         */
        if (object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED &&
            object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED) {
            work += garbage_collector_rewrite_object_graph(state, object);
        }
        object = object->next;
    }

    work += garbage_collector_free_old_from_space(state);
    return work;
}

static TZrSize garbage_collector_run_minor_evacuation(SZrState *state) {
    SZrGarbageCollector *collector = state->global->garbageCollector;
    SZrRawObject *object = collector->gcObjectList;
    TZrSize work = 0;

    while (object != ZR_NULL) {
        SZrRawObject *nextObject = object->next;

        if (object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE) {
            if (garbage_collector_object_is_live_in_minor(object)) {
                EZrGarbageCollectRegionKind regionKind;
                EZrGarbageCollectStorageKind storageKind;
                EZrGarbageCollectGenerationalObjectStatus generationalStatus;
                EZrGarbageCollectPromotionReason promotionReason;
                TZrUInt32 survivalAge;
                TZrSize objectSize = garbage_collector_get_object_base_size(state, object);

                garbage_collector_minor_target_for_object(collector,
                                                          object,
                                                          &regionKind,
                                                          &storageKind,
                                                          &generationalStatus,
                                                          &promotionReason,
                                                          &survivalAge);
                if (garbage_collector_object_supports_evacuation(state, object)) {
                    SZrRawObject *promotedObject = garbage_collector_clone_for_minor_evacuation(state,
                                                                                                 object,
                                                                                                 regionKind,
                                                                                                 storageKind,
                                                                                                 generationalStatus,
                                                                                                 promotionReason,
                                                                                                 survivalAge);

                    if (promotedObject != ZR_NULL) {
                        garbage_collector_remember_promoted_minor_object(state, promotedObject);
                        work++;
                    }
                } else {
                    garbage_collector_reassign_minor_target(state,
                                                            object,
                                                            regionKind,
                                                            storageKind,
                                                            generationalStatus,
                                                            promotionReason,
                                                            survivalAge,
                                                            objectSize);
                    garbage_collector_remember_promoted_minor_object(state, object);
                    work++;
                }
            } else {
                object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED;
            }
        }

        object = nextObject;
    }

    return work;
}

static TZrSize garbage_collector_rewrite_minor_forwarding(SZrState *state) {
    SZrRawObject *object;
    TZrSize work = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return 0;
    }

    work += garbage_collector_rewrite_forwarded_roots(state);

    object = state->global->garbageCollector->gcObjectList;
    while (object != ZR_NULL) {
        if (object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED &&
            object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED) {
            work += garbage_collector_rewrite_object_graph(state, object);
        }
        object = object->next;
    }

    return work;
}

static TZrSize garbage_collector_free_minor_from_space(SZrState *state) {
    SZrRawObject **current = &state->global->garbageCollector->gcObjectList;
    TZrSize work = 0;

    while (*current != ZR_NULL) {
        SZrRawObject *object = *current;
        TZrBool shouldFree = ZR_FALSE;

        if (object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE) {
            shouldFree = object->garbageCollectMark.forwardingAddress != ZR_NULL ||
                         object->garbageCollectMark.status ==
                                 ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED;
        }

        if (shouldFree) {
            *current = object->next;
            garbage_collector_free_object(state, object);
            work++;
            continue;
        }

        current = &object->next;
    }

    return work;
}

static TZrSize garbage_collector_run_generational_major_collection(SZrState *state,
                                                                   TZrBool forceCompact,
                                                                   TZrBool *outDidCompact) {
    SZrGlobalState *global = state->global;
    SZrGarbageCollector *collector = global->garbageCollector;
    TZrSize work = 0;
    TZrSize sweepIterationCount = 0;
    const TZrSize maxSweepIterations = ZR_GC_GENERATIONAL_FULL_SWEEP_ITERATION_LIMIT;
    SZrRawObject **previousSweeper = ZR_NULL;
    SZrRawObject *object;
    TZrBool didCompact = ZR_FALSE;

    if (outDidCompact != ZR_NULL) {
        *outDidCompact = ZR_FALSE;
    }

    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    ZrGarbageCollectorRestartCollection(state);
    work += ZrGarbageCollectorPropagateAll(state);

    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_REMARK;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    work += garbage_collector_atomic(state);

    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_SWEEP;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    garbage_collector_enter_sweep(state);

    while (collector->gcRunningStatus != ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END) {
        if (++sweepIterationCount > maxSweepIterations) {
            collector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END;
            collector->gcObjectListSweeper = ZR_NULL;
            break;
        }

        if (collector->gcObjectListSweeper == previousSweeper && previousSweeper != ZR_NULL) {
            collector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END;
            collector->gcObjectListSweeper = ZR_NULL;
            break;
        }
        previousSweeper = collector->gcObjectListSweeper;
        garbage_collector_sweep_step(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END, NULL);
    }

    garbage_collector_check_sizes(state, global);
    while (collector->waitToReleaseObjectList != ZR_NULL) {
        garbage_collector_run_a_few_finalizers(state, ZR_GC_FINALIZER_BATCH_MAX);
    }

    object = collector->gcObjectList;
    while (object != ZR_NULL) {
        if (!ZrCore_RawObject_IsUnreferenced(state, object) &&
            object->garbageCollectMark.generationalStatus == ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW) {
            object->garbageCollectMark.generationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL;
        }
        object = object->next;
    }

    if (garbage_collector_should_compact_old_regions(state, forceCompact)) {
        collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_COMPACT;
        collector->statsSnapshot.collectionPhase = collector->collectionPhase;
        work += garbage_collector_run_old_compaction(state);
        garbage_collector_check_sizes(state, global);
        didCompact = ZR_TRUE;
    }

    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->waitToReleaseObjectList = ZR_NULL;
    collector->releasedObjectList = ZR_NULL;
    collector->gcObjectListSweeper = ZR_NULL;
    collector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;

    if (outDidCompact != ZR_NULL) {
        *outDidCompact = didCompact;
    }

    return work;
}

TZrSize garbage_collector_run_generational_full(SZrState *state) {
    return garbage_collector_run_generational_major_collection(state, ZR_TRUE, ZR_NULL);
}

TZrSize garbage_collector_process_weak_tables(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);

    /*
     * Weak table support is not wired up yet. The previous placeholder walked
     * every object nodeMap and eagerly removed entries based on GC color,
     * which corrupts ordinary tables during shutdown/full collections.
     * Leave the phase as a no-op until dedicated weak-table metadata exists.
     */
    return 0;
}

TZrSize garbage_collector_atomic(SZrState *state) {
    SZrGlobalState *global;
    TZrSize work = 0;
    SZrRawObject *stateObject;

    if (state == ZR_NULL) {
        return 0;
    }

    global = state->global;
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return 0;
    }

    global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_ATOMIC;

    stateObject = ZR_CAST_RAW_OBJECT_AS_SUPER(state);
    if (stateObject != ZR_NULL &&
        stateObject->type < ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX &&
        stateObject->type != ZR_RAW_OBJECT_TYPE_INVALID) {
        ZrGarbageCollectorReallyMarkObject(state, stateObject);
    }

    if (ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry)) {
        garbage_collector_mark_value(state, &global->loadedModulesRegistry);
    }
    if (global->errorPrototype != ZR_NULL) {
        garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->errorPrototype));
    }
    if (global->stackFramePrototype != ZR_NULL) {
        garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->stackFramePrototype));
    }
    if (global->hasUnhandledExceptionHandler &&
        ZrCore_Value_IsGarbageCollectable(&global->unhandledExceptionHandler)) {
        garbage_collector_mark_value(state, &global->unhandledExceptionHandler);
    }

    work += garbage_collector_mark_string_roots(state);
    work += garbage_collector_mark_ignored_roots(state);

    for (TZrSize i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        if (global->basicTypeObjectPrototype[i] != ZR_NULL) {
            garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->basicTypeObjectPrototype[i]));
        }
    }

    work += ZrGarbageCollectorPropagateAll(state);
    work += garbage_collector_process_weak_tables(state);
    global->garbageCollector->gcGeneration = ZR_GC_OTHER_GENERATION(global->garbageCollector);
    return work;
}

int garbage_collector_sweep_step(SZrState *state,
                                 EZrGarbageCollectRunningStatus nextstate,
                                 SZrRawObject **nextlist) {
    SZrGlobalState *global = state->global;
    SZrGarbageCollector *collector = global->garbageCollector;
    int sweepBudget;

    if (collector->gcObjectListSweeper) {
        TZrMemoryOffset olddebt = global->garbageCollector->gcDebtSize;
        TZrUInt64 maxSweepSliceBudget = (TZrUInt64)ZR_GC_SWEEP_SLICE_BUDGET_MAX;
        int count;

        sweepBudget = (collector->gcSweepSliceBudget == 0)
                              ? 1
                              : (collector->gcSweepSliceBudget > maxSweepSliceBudget
                                         ? (int)maxSweepSliceBudget
                                         : (int)collector->gcSweepSliceBudget);
        collector->gcObjectListSweeper =
                garbage_collector_sweep_list(state, collector->gcObjectListSweeper, sweepBudget, &count);
        collector->managedMemories += global->garbageCollector->gcDebtSize - olddebt;
        if (collector->gcObjectListSweeper != ZR_NULL && *collector->gcObjectListSweeper == ZR_NULL) {
            collector->gcRunningStatus = nextstate;
            collector->gcObjectListSweeper = nextlist;
        }
        return count;
    }

    collector->gcRunningStatus = nextstate;
    collector->gcObjectListSweeper = nextlist;
    return 0;
}

TZrSize garbage_collector_single_step(SZrState *state) {
    SZrGlobalState *global = state->global;
    TZrSize work;

    switch (global->garbageCollector->gcRunningStatus) {
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED:
            ZrGarbageCollectorRestartCollection(state);
            global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION;
            work = 1;
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION:
            if (global->garbageCollector->waitToScanObjectList == NULL) {
                global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_BEFORE_ATOMIC;
                work = 0;
            } else {
                work = ZrGarbageCollectorPropagateMark(state);
            }
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_BEFORE_ATOMIC:
            work = garbage_collector_atomic(state);
            garbage_collector_enter_sweep(state);
            global->garbageCollector->managedMemories = global->garbageCollector->managedMemories;
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS:
            work = garbage_collector_sweep_step(
                    state,
                    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_WAIT_TO_RELEASE_OBJECTS,
                    &global->garbageCollector->waitToReleaseObjectList);
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_WAIT_TO_RELEASE_OBJECTS:
            work = garbage_collector_sweep_step(
                    state,
                    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_RELEASED_OBJECTS,
                    &global->garbageCollector->waitToReleaseObjectList);
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_RELEASED_OBJECTS:
            work = garbage_collector_sweep_step(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END, NULL);
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END:
            garbage_collector_check_sizes(state, global);
            global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_END;
            work = 0;
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_END:
            if (global->garbageCollector->waitToReleaseObjectList &&
                !global->garbageCollector->isImmediateGcFlag) {
                global->garbageCollector->stopImmediateGcFlag = 0;
                work = garbage_collector_run_a_few_finalizers(state, ZR_GC_FINALIZER_BATCH_MAX);
            } else {
                global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;
                work = 0;
            }
            break;
        default:
            return 0;
    }

    return work;
}

TZrBool garbage_collector_is_generational_mode(SZrGlobalState *global) {
    return global->garbageCollector->gcMode == ZR_GARBAGE_COLLECT_MODE_GENERATIONAL ||
           global->garbageCollector->atomicMemories != 0;
}

void garbage_collector_full_inc(SZrState *state, SZrGlobalState *global) {
    if (ZrCore_GarbageCollector_IsInvariant(global)) {
        garbage_collector_enter_sweep(state);
    }

    garbage_collector_run_until_state(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED);
    garbage_collector_run_until_state(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION);
    global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_BEFORE_ATOMIC;
    garbage_collector_run_until_state(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_END);
    garbage_collector_run_until_state(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED);
    ZrCore_GarbageCollector_AddDebtSpace(global, -ZR_GC_DEBT_CREDIT_BYTES);
}

TZrBool gcrunning(SZrGlobalState *global) {
    return global->garbageCollector->gcStatus == ZR_GARBAGE_COLLECT_STATUS_RUNNING;
}

void garbage_collector_run_generational_step(SZrState *state) {
    SZrGlobalState *global = state->global;
    SZrGarbageCollector *collector = global->garbageCollector;
    TZrMemoryOffset managedMemories = collector->managedMemories;
    TZrMemoryOffset threshold = collector->gcPauseThresholdPercent * managedMemories / 100;
    TZrSize work = 0;
    TZrBool explicitRequest = (collector->gcFlags & ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST) != 0u;
    TZrBool forceFull;
    TZrBool requestMajor;
    TZrBool didCompact = ZR_FALSE;

    forceFull = collector->scheduledCollectionKind == ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL ||
                (!explicitRequest &&
                 collector->heapLimitBytes > 0 &&
                 managedMemories >= collector->heapLimitBytes);
    requestMajor = collector->scheduledCollectionKind == ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR ||
                   (!explicitRequest && managedMemories > threshold);

    if (forceFull || requestMajor) {
        work += garbage_collector_run_generational_major_collection(state, forceFull, &didCompact);
        collector->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
        collector->gcFlags &= ~ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
        collector->statsSnapshot.lastCollectionKind =
                (forceFull || didCompact) ? ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL
                                          : ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR;
        collector->gcLastStepWork = work > 0 ? work : 1;
        return;
    }

    work += garbage_collector_restart_minor_collection(state);
    work += ZrGarbageCollectorPropagateAll(state);
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_EVACUATE;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    work += garbage_collector_run_minor_evacuation(state);
    work += garbage_collector_rewrite_minor_forwarding(state);
    work += garbage_collector_free_minor_from_space(state);

    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->waitToReleaseObjectList = ZR_NULL;
    collector->releasedObjectList = ZR_NULL;
    collector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;
    collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
    collector->gcFlags &= ~ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    collector->gcLastStepWork = work > 0 ? work : 1;
}
