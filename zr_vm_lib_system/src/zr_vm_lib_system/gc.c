//
// zr.system.gc callbacks.
//

#include "zr_vm_lib_system/gc.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"

#include <string.h>

static TZrBool system_gc_get_collector(const ZrLibCallContext *context, SZrGarbageCollector **outCollector) {
    if (context == ZR_NULL || outCollector == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL ||
        context->state->global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    *outCollector = context->state->global->garbageCollector;
    return ZR_TRUE;
}

static void system_gc_write_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void system_gc_write_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static TZrBool system_gc_read_non_negative_int_argument(const ZrLibCallContext *context,
                                                        TZrSize index,
                                                        TZrUInt64 *outValue) {
    TZrInt64 value = 0;

    if (context == ZR_NULL || outValue == ZR_NULL ||
        !ZrLib_CallContext_ReadInt(context, index, &value) ||
        value < 0) {
        return ZR_FALSE;
    }

    *outValue = (TZrUInt64)value;
    return ZR_TRUE;
}

static TZrBool system_gc_read_collection_kind(const ZrLibCallContext *context,
                                              EZrGarbageCollectCollectionKind *outKind) {
    SZrString *kindString = ZR_NULL;
    const TZrChar *kindText = ZR_NULL;

    if (context == ZR_NULL || outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    if (context->argumentCount == 0) {
        *outKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
        return ZR_TRUE;
    }

    if (!ZrLib_CallContext_ReadString(context, 0, &kindString) || kindString == ZR_NULL) {
        return ZR_FALSE;
    }

    kindText = ZrCore_String_GetNativeString(kindString);
    if (kindText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(kindText, "minor") == 0) {
        *outKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
        return ZR_TRUE;
    }
    if (strcmp(kindText, "major") == 0) {
        *outKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR;
        return ZR_TRUE;
    }
    if (strcmp(kindText, "full") == 0) {
        *outKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void system_gc_run_collection(SZrState *state, EZrGarbageCollectCollectionKind kind) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    if (kind == ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL) {
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        return;
    }

    ZrCore_GarbageCollector_ScheduleCollection(state->global, kind);
    ZrCore_GarbageCollector_GcStep(state);
}

static SZrObject *system_gc_make_stats_object(SZrState *state, SZrGarbageCollector *collector) {
    SZrObject *object;
    SZrGarbageCollectorStatsSnapshot snapshot;

    if (state == ZR_NULL || collector == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    memset(&snapshot, 0, sizeof(snapshot));
    ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &snapshot);

    object = ZrLib_Type_NewInstance(state, "SystemGcStats");
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    system_gc_write_bool_field(state, object, "enabled", !collector->stopGcFlag);
    system_gc_write_int_field(state, object, "heapLimitBytes", (TZrInt64)snapshot.heapLimitBytes);
    system_gc_write_int_field(state, object, "managedMemoryBytes", (TZrInt64)snapshot.managedMemoryBytes);
    system_gc_write_int_field(state, object, "gcDebtBytes", (TZrInt64)snapshot.gcDebtBytes);
    system_gc_write_int_field(state, object, "pauseBudgetUs", (TZrInt64)snapshot.pauseBudgetUs);
    system_gc_write_int_field(state, object, "remarkBudgetUs", (TZrInt64)snapshot.remarkBudgetUs);
    system_gc_write_int_field(state, object, "workerCount", (TZrInt64)snapshot.workerCount);
    system_gc_write_int_field(state, object, "ignoredObjectCount", (TZrInt64)snapshot.ignoredObjectCount);
    system_gc_write_int_field(state, object, "rememberedObjectCount", (TZrInt64)snapshot.rememberedObjectCount);
    system_gc_write_int_field(state, object, "regionCount", (TZrInt64)snapshot.regionCount);
    system_gc_write_int_field(state, object, "edenRegionCount", (TZrInt64)snapshot.edenRegionCount);
    system_gc_write_int_field(state, object, "survivorRegionCount", (TZrInt64)snapshot.survivorRegionCount);
    system_gc_write_int_field(state, object, "oldRegionCount", (TZrInt64)snapshot.oldRegionCount);
    system_gc_write_int_field(state, object, "pinnedRegionCount", (TZrInt64)snapshot.pinnedRegionCount);
    system_gc_write_int_field(state, object, "largeRegionCount", (TZrInt64)snapshot.largeRegionCount);
    system_gc_write_int_field(state, object, "permanentRegionCount", (TZrInt64)snapshot.permanentRegionCount);
    system_gc_write_int_field(state, object, "edenUsedBytes", (TZrInt64)snapshot.edenUsedBytes);
    system_gc_write_int_field(state, object, "survivorUsedBytes", (TZrInt64)snapshot.survivorUsedBytes);
    system_gc_write_int_field(state, object, "oldUsedBytes", (TZrInt64)snapshot.oldUsedBytes);
    system_gc_write_int_field(state, object, "pinnedUsedBytes", (TZrInt64)snapshot.pinnedUsedBytes);
    system_gc_write_int_field(state, object, "largeUsedBytes", (TZrInt64)snapshot.largeUsedBytes);
    system_gc_write_int_field(state, object, "permanentUsedBytes", (TZrInt64)snapshot.permanentUsedBytes);
    system_gc_write_int_field(state, object, "edenLiveBytes", (TZrInt64)snapshot.edenLiveBytes);
    system_gc_write_int_field(state, object, "survivorLiveBytes", (TZrInt64)snapshot.survivorLiveBytes);
    system_gc_write_int_field(state, object, "oldLiveBytes", (TZrInt64)snapshot.oldLiveBytes);
    system_gc_write_int_field(state, object, "pinnedLiveBytes", (TZrInt64)snapshot.pinnedLiveBytes);
    system_gc_write_int_field(state, object, "largeLiveBytes", (TZrInt64)snapshot.largeLiveBytes);
    system_gc_write_int_field(state, object, "permanentLiveBytes", (TZrInt64)snapshot.permanentLiveBytes);
    system_gc_write_int_field(state, object, "lastStepDurationUs", (TZrInt64)snapshot.lastStepDurationUs);
    system_gc_write_int_field(state, object, "lastStepWork", (TZrInt64)snapshot.lastStepWork);
    system_gc_write_int_field(state, object, "lastCollectionKind", (TZrInt64)snapshot.lastCollectionKind);
    system_gc_write_int_field(state,
                              object,
                              "lastRequestedCollectionKind",
                              (TZrInt64)snapshot.lastRequestedCollectionKind);
    system_gc_write_int_field(state, object, "collectionPhase", (TZrInt64)snapshot.collectionPhase);
    system_gc_write_int_field(state, object, "minorCollectionCount", (TZrInt64)snapshot.minorCollectionCount);
    system_gc_write_int_field(state, object, "majorCollectionCount", (TZrInt64)snapshot.majorCollectionCount);
    system_gc_write_int_field(state, object, "fullCollectionCount", (TZrInt64)snapshot.fullCollectionCount);
    system_gc_write_int_field(state,
                              object,
                              "minorCollectionTotalDurationUs",
                              (TZrInt64)snapshot.minorCollectionTotalDurationUs);
    system_gc_write_int_field(state,
                              object,
                              "majorCollectionTotalDurationUs",
                              (TZrInt64)snapshot.majorCollectionTotalDurationUs);
    system_gc_write_int_field(state,
                              object,
                              "fullCollectionTotalDurationUs",
                              (TZrInt64)snapshot.fullCollectionTotalDurationUs);
    system_gc_write_int_field(state,
                              object,
                              "minorCollectionMaxDurationUs",
                              (TZrInt64)snapshot.minorCollectionMaxDurationUs);
    system_gc_write_int_field(state,
                              object,
                              "majorCollectionMaxDurationUs",
                              (TZrInt64)snapshot.majorCollectionMaxDurationUs);
    system_gc_write_int_field(state,
                              object,
                              "fullCollectionMaxDurationUs",
                              (TZrInt64)snapshot.fullCollectionMaxDurationUs);
    return object;
}

ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Enable(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrGarbageCollector *collector = ZR_NULL;

    if (result == ZR_NULL || !system_gc_get_collector(context, &collector)) {
        return ZR_FALSE;
    }

    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    collector->stopGcFlag = ZR_FALSE;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Gc_Disable(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrGarbageCollector *collector = ZR_NULL;

    if (result == ZR_NULL || !system_gc_get_collector(context, &collector)) {
        return ZR_FALSE;
    }

    collector->stopGcFlag = ZR_TRUE;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Collect(ZrLibCallContext *context, SZrTypeValue *result) {
    EZrGarbageCollectCollectionKind kind;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !system_gc_read_collection_kind(context, &kind)) {
        return ZR_FALSE;
    }

    system_gc_run_collection(context->state, kind);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_SetHeapLimit(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrUInt64 heapLimitBytes = 0;
    SZrGarbageCollector *collector = ZR_NULL;

    if (result == ZR_NULL ||
        !system_gc_get_collector(context, &collector) ||
        !system_gc_read_non_negative_int_argument(context, 0, &heapLimitBytes)) {
        return ZR_FALSE;
    }

    ZrCore_GarbageCollector_SetHeapLimitBytes(context->state->global, (TZrMemoryOffset)heapLimitBytes);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_SetBudget(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrUInt64 pauseBudgetUs = 0;
    SZrGarbageCollector *collector = ZR_NULL;

    if (result == ZR_NULL ||
        !system_gc_get_collector(context, &collector) ||
        !system_gc_read_non_negative_int_argument(context, 0, &pauseBudgetUs)) {
        return ZR_FALSE;
    }

    ZrCore_GarbageCollector_SetPauseBudgetUs(context->state->global, pauseBudgetUs, pauseBudgetUs);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_GetStats(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrGarbageCollector *collector = ZR_NULL;
    SZrObject *statsObject;

    if (result == ZR_NULL || !system_gc_get_collector(context, &collector)) {
        return ZR_FALSE;
    }

    statsObject = system_gc_make_stats_object(context->state, collector);
    if (statsObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, statsObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
