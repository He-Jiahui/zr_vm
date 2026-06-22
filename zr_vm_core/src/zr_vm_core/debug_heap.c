#include "zr_vm_core/debug.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#define ZR_DEBUG_HEAP_PROTOTYPE_SUMMARY_CAPACITY 64u

typedef struct SZrDebugHeapPrototypeSummary {
    const SZrObjectPrototype *prototype;
    TZrUInt64 count;
    TZrUInt64 bytes;
} SZrDebugHeapPrototypeSummary;

static const EZrRawObjectType ZR_DEBUG_HEAP_KNOWN_TYPES[] = {
        ZR_RAW_OBJECT_TYPE_STRING,
        ZR_RAW_OBJECT_TYPE_BUFFER,
        ZR_RAW_OBJECT_TYPE_ARRAY,
        ZR_RAW_OBJECT_TYPE_FUNCTION,
        ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE,
        ZR_RAW_OBJECT_TYPE_CLOSURE,
        ZR_RAW_OBJECT_TYPE_OBJECT,
        ZR_RAW_OBJECT_TYPE_THREAD,
        ZR_RAW_OBJECT_TYPE_NATIVE_POINTER,
        ZR_RAW_OBJECT_TYPE_NATIVE_DATA,
};

static TZrNativeString debug_heap_get_string_native(SZrString *stringValue) {
    return stringValue != ZR_NULL ? ZrCore_String_GetNativeString(stringValue) : ZR_NULL;
}

static const TZrChar *debug_heap_raw_object_type_name(EZrRawObjectType type) {
    switch (type) {
        case ZR_RAW_OBJECT_TYPE_STRING:
            return "string";
        case ZR_RAW_OBJECT_TYPE_BUFFER:
            return "buffer";
        case ZR_RAW_OBJECT_TYPE_ARRAY:
            return "array";
        case ZR_RAW_OBJECT_TYPE_FUNCTION:
            return "function";
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE:
            return "closure_value";
        case ZR_RAW_OBJECT_TYPE_CLOSURE:
            return "closure";
        case ZR_RAW_OBJECT_TYPE_OBJECT:
            return "object";
        case ZR_RAW_OBJECT_TYPE_THREAD:
            return "thread";
        case ZR_RAW_OBJECT_TYPE_NATIVE_POINTER:
            return "native_pointer";
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA:
            return "native_data";
        case ZR_RAW_OBJECT_TYPE_INVALID:
        default:
            return "unknown";
    }
}

static const TZrChar *debug_heap_collection_kind_name(EZrGarbageCollectCollectionKind kind) {
    switch (kind) {
        case ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR:
            return "minor";
        case ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR:
            return "major";
        case ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL:
            return "full";
        default:
            return "unknown";
    }
}

static const TZrChar *debug_heap_collection_phase_name(EZrGarbageCollectCollectionPhase phase) {
    switch (phase) {
        case ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE:
            return "idle";
        case ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_MARK:
            return "minor_mark";
        case ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_EVACUATE:
            return "minor_evacuate";
        case ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT:
            return "major_mark_concurrent";
        case ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_REMARK:
            return "major_remark";
        case ZR_GARBAGE_COLLECT_COLLECTION_PHASE_SWEEP:
            return "sweep";
        case ZR_GARBAGE_COLLECT_COLLECTION_PHASE_COMPACT:
            return "compact";
        default:
            return "unknown";
    }
}

static TZrBool debug_heap_object_is_active(const SZrRawObject *object) {
    if (object == ZR_NULL ||
        object->type <= ZR_RAW_OBJECT_TYPE_INVALID ||
        object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX) {
        return ZR_FALSE;
    }

    if (object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED ||
        object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static void debug_heap_record_prototype(SZrDebugHeapPrototypeSummary *summaries,
                                        TZrSize *summaryCount,
                                        const SZrObjectPrototype *prototype,
                                        TZrUInt64 bytes) {
    TZrSize index;

    if (summaries == ZR_NULL || summaryCount == ZR_NULL || prototype == ZR_NULL) {
        return;
    }

    for (index = 0u; index < *summaryCount; index++) {
        if (summaries[index].prototype == prototype) {
            summaries[index].count++;
            summaries[index].bytes += bytes;
            return;
        }
    }

    if (*summaryCount >= ZR_DEBUG_HEAP_PROTOTYPE_SUMMARY_CAPACITY) {
        return;
    }

    summaries[*summaryCount].prototype = prototype;
    summaries[*summaryCount].count = 1u;
    summaries[*summaryCount].bytes = bytes;
    (*summaryCount)++;
}

static void debug_heap_accumulate_object(SZrState *state,
                                         SZrRawObject *object,
                                         TZrUInt64 *counts,
                                         TZrUInt64 *bytes,
                                         TZrUInt64 *totalObjects,
                                         TZrUInt64 *totalBytes,
                                         SZrDebugHeapPrototypeSummary *prototypeSummaries,
                                         TZrSize *prototypeSummaryCount) {
    TZrSize typeIndex;
    TZrUInt64 objectBytes;

    if (!debug_heap_object_is_active(object) ||
        counts == ZR_NULL ||
        bytes == ZR_NULL ||
        totalObjects == ZR_NULL ||
        totalBytes == ZR_NULL) {
        return;
    }

    typeIndex = (TZrSize)object->type;
    objectBytes = (TZrUInt64)ZrCore_GarbageCollector_GetObjectBaseSize(state, object);
    counts[typeIndex]++;
    bytes[typeIndex] += objectBytes;
    (*totalObjects)++;
    (*totalBytes) += objectBytes;

    if (object->type == ZR_RAW_OBJECT_TYPE_OBJECT || object->type == ZR_RAW_OBJECT_TYPE_ARRAY) {
        const SZrObject *objectValue = (const SZrObject *)object;
        debug_heap_record_prototype(prototypeSummaries,
                                    prototypeSummaryCount,
                                    objectValue->prototype,
                                    objectBytes);
    }
}

static void debug_heap_accumulate_list(SZrState *state,
                                       SZrRawObject *objectList,
                                       TZrUInt64 *counts,
                                       TZrUInt64 *bytes,
                                       TZrUInt64 *totalObjects,
                                       TZrUInt64 *totalBytes,
                                       SZrDebugHeapPrototypeSummary *prototypeSummaries,
                                       TZrSize *prototypeSummaryCount) {
    SZrRawObject *object;

    for (object = objectList; object != ZR_NULL; object = object->next) {
        debug_heap_accumulate_object(state,
                                     object,
                                     counts,
                                     bytes,
                                     totalObjects,
                                     totalBytes,
                                     prototypeSummaries,
                                     prototypeSummaryCount);
    }
}

void ZrCore_Debug_HeapSummary(struct SZrState *state, FILE *output) {
    SZrGarbageCollector *collector;
    SZrGarbageCollectorStatsSnapshot snapshot;
    SZrDebugHeapPrototypeSummary prototypeSummaries[ZR_DEBUG_HEAP_PROTOTYPE_SUMMARY_CAPACITY];
    TZrUInt64 counts[ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX];
    TZrUInt64 bytes[ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX];
    TZrUInt64 totalObjects = 0u;
    TZrUInt64 totalBytes = 0u;
    TZrSize prototypeSummaryCount = 0u;
    TZrSize index;

    if (state == ZR_NULL || state->global == ZR_NULL || output == ZR_NULL) {
        return;
    }

    collector = state->global->garbageCollector;
    if (collector == ZR_NULL) {
        return;
    }

    memset(&snapshot, 0, sizeof(snapshot));
    memset(prototypeSummaries, 0, sizeof(prototypeSummaries));
    memset(counts, 0, sizeof(counts));
    memset(bytes, 0, sizeof(bytes));

    debug_heap_accumulate_list(state,
                               collector->gcObjectList,
                               counts,
                               bytes,
                               &totalObjects,
                               &totalBytes,
                               prototypeSummaries,
                               &prototypeSummaryCount);
    debug_heap_accumulate_list(state,
                               collector->permanentObjectList,
                               counts,
                               bytes,
                               &totalObjects,
                               &totalBytes,
                               prototypeSummaries,
                               &prototypeSummaryCount);
    ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &snapshot);

    fprintf(output,
            "ZR_HEAP_SUMMARY objects total=%llu bytes=%llu managed=%llu debt=%lld\n",
            (unsigned long long)totalObjects,
            (unsigned long long)totalBytes,
            (unsigned long long)snapshot.managedMemoryBytes,
            (long long)snapshot.gcDebtBytes);

    for (index = 0u; index < sizeof(ZR_DEBUG_HEAP_KNOWN_TYPES) / sizeof(ZR_DEBUG_HEAP_KNOWN_TYPES[0]); index++) {
        EZrRawObjectType type = ZR_DEBUG_HEAP_KNOWN_TYPES[index];
        TZrSize typeIndex = (TZrSize)type;
        fprintf(output,
                "type %s count %llu bytes %llu\n",
                debug_heap_raw_object_type_name(type),
                (unsigned long long)counts[typeIndex],
                (unsigned long long)bytes[typeIndex]);
    }

    fprintf(output, "prototypes tracked %llu\n", (unsigned long long)prototypeSummaryCount);
    for (index = 0u; index < prototypeSummaryCount; index++) {
        const SZrObjectPrototype *prototype = prototypeSummaries[index].prototype;
        TZrNativeString prototypeName =
                prototype != ZR_NULL ? debug_heap_get_string_native(prototype->name) : ZR_NULL;
        fprintf(output,
                "prototype %s count %llu bytes %llu\n",
                prototypeName != ZR_NULL ? prototypeName : "<anonymous>",
                (unsigned long long)prototypeSummaries[index].count,
                (unsigned long long)prototypeSummaries[index].bytes);
    }

    fprintf(output,
            "gc regions total=%u eden=%u survivor=%u old=%u pinned=%u large=%u permanent=%u\n",
            (unsigned)snapshot.regionCount,
            (unsigned)snapshot.edenRegionCount,
            (unsigned)snapshot.survivorRegionCount,
            (unsigned)snapshot.oldRegionCount,
            (unsigned)snapshot.pinnedRegionCount,
            (unsigned)snapshot.largeRegionCount,
            (unsigned)snapshot.permanentRegionCount);
    fprintf(output,
            "gc bytes eden_used=%llu survivor_used=%llu old_used=%llu pinned_used=%llu large_used=%llu permanent_used=%llu\n",
            (unsigned long long)snapshot.edenUsedBytes,
            (unsigned long long)snapshot.survivorUsedBytes,
            (unsigned long long)snapshot.oldUsedBytes,
            (unsigned long long)snapshot.pinnedUsedBytes,
            (unsigned long long)snapshot.largeUsedBytes,
            (unsigned long long)snapshot.permanentUsedBytes);
    fprintf(output,
            "gc collections minor=%llu major=%llu full=%llu last=%s requested=%s phase=%s remembered=%u ignored=%u\n",
            (unsigned long long)snapshot.minorCollectionCount,
            (unsigned long long)snapshot.majorCollectionCount,
            (unsigned long long)snapshot.fullCollectionCount,
            debug_heap_collection_kind_name(snapshot.lastCollectionKind),
            debug_heap_collection_kind_name(snapshot.lastRequestedCollectionKind),
            debug_heap_collection_phase_name(snapshot.collectionPhase),
            (unsigned)snapshot.rememberedObjectCount,
            (unsigned)snapshot.ignoredObjectCount);
}
