//
// zr.system.gc descriptor registry.
//

#include "zr_vm_lib_system/gc_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const ZrLibParameterDescriptor g_collect_parameters[] = {
        {"kind", "string", "Collection kind: minor, major, or full. Defaults to full."},
};

static const ZrLibParameterDescriptor g_heap_limit_parameters[] = {
        {"bytes", "int", "Heap limit in bytes. Use 0 to clear the limit."},
};

static const ZrLibParameterDescriptor g_budget_parameters[] = {
        {"microseconds", "int", "Pause budget in microseconds used for both pause and remark slices."},
};

static const ZrLibFieldDescriptor g_gc_stats_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("enabled", "bool", "Whether background and incremental GC steps are enabled."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("heapLimitBytes", "int", "Configured heap limit in bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("managedMemoryBytes", "int", "Current managed-memory estimate in bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("gcDebtBytes", "int", "Current GC debt in bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("pauseBudgetUs", "int", "Configured pause budget in microseconds."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("remarkBudgetUs", "int", "Configured remark budget in microseconds."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("workerCount", "int", "Current GC worker count setting."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("ignoredObjectCount", "int", "Current ignored-object root count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("rememberedObjectCount", "int", "Current remembered-set object count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("regionCount", "int", "Active GC region count with live objects."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("edenRegionCount", "int", "Active eden-region count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("survivorRegionCount", "int", "Active survivor-region count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("oldRegionCount", "int", "Active old-region count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("pinnedRegionCount", "int", "Active pinned-region count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("largeRegionCount", "int", "Active large-region count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("permanentRegionCount", "int", "Active permanent-region count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("edenUsedBytes", "int", "Current eden used bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("survivorUsedBytes", "int", "Current survivor used bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("oldUsedBytes", "int", "Current old-region used bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("pinnedUsedBytes", "int", "Current pinned-region used bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("largeUsedBytes", "int", "Current large-region used bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("permanentUsedBytes", "int", "Current permanent-region used bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("edenLiveBytes", "int", "Current eden live bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("survivorLiveBytes", "int", "Current survivor live bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("oldLiveBytes", "int", "Current old-region live bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("pinnedLiveBytes", "int", "Current pinned-region live bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("largeLiveBytes", "int", "Current large-region live bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("permanentLiveBytes", "int", "Current permanent-region live bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("lastStepDurationUs", "int", "Wall-clock duration in microseconds for the last GC step."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("lastStepWork", "int", "Work units reported by the last GC step."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("lastCollectionKind", "int", "Last collection kind that actually ran."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("lastRequestedCollectionKind", "int", "Last collection kind requested by the caller."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("collectionPhase", "int", "Current or last observed GC phase enum value."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("minorCollectionCount", "int", "Cumulative minor collection count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("majorCollectionCount", "int", "Cumulative major collection count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("fullCollectionCount", "int", "Cumulative full collection count."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("minorCollectionTotalDurationUs", "int", "Cumulative wall-clock minor collection time in microseconds."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("majorCollectionTotalDurationUs", "int", "Cumulative wall-clock major collection time in microseconds."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("fullCollectionTotalDurationUs", "int", "Cumulative wall-clock full collection time in microseconds."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("minorCollectionMaxDurationUs", "int", "Maximum observed minor collection duration in microseconds."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("majorCollectionMaxDurationUs", "int", "Maximum observed major collection duration in microseconds."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("fullCollectionMaxDurationUs", "int", "Maximum observed full collection duration in microseconds."),
};

const ZrLibModuleDescriptor *ZrSystem_GcRegistry_GetModule(void) {
    static const ZrLibTypeDescriptor kTypes[] = {
            ZR_LIB_TYPE_DESCRIPTOR_INIT("SystemGcStats", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, g_gc_stats_fields,
                                        ZR_ARRAY_COUNT(g_gc_stats_fields), ZR_NULL, 0, ZR_NULL, 0,
                                        "Snapshot of safe GC control and telemetry state.", ZR_NULL, ZR_NULL, 0,
                                        ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "SystemGcStats()", ZR_NULL, 0),
    };
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"enable", 0, 0, ZrSystem_Gc_Enable, "null", "Enable incremental and scheduled garbage collection.",
             ZR_NULL, 0},
            {"disable", 0, 0, ZrSystem_Gc_Disable, "null", "Disable incremental and scheduled garbage collection.",
             ZR_NULL, 0},
            {"collect", 0, 1, ZrSystem_Gc_Collect, "null", "Request a minor, major, or full garbage collection.",
             g_collect_parameters, ZR_ARRAY_COUNT(g_collect_parameters)},
            {"set_heap_limit", 1, 1, ZrSystem_Gc_SetHeapLimit, "null", "Set the heap limit in bytes.",
             g_heap_limit_parameters, ZR_ARRAY_COUNT(g_heap_limit_parameters)},
            {"set_budget", 1, 1, ZrSystem_Gc_SetBudget, "null",
             "Set the pause budget in microseconds for safe GC slices.",
             g_budget_parameters, ZR_ARRAY_COUNT(g_budget_parameters)},
            {"get_stats", 0, 0, ZrSystem_Gc_GetStats, "SystemGcStats",
             "Return a safe snapshot of GC control and telemetry state.", ZR_NULL, 0},
    };
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"enable", "function", "enable(): null", "Enable incremental and scheduled garbage collection."},
            {"disable", "function", "disable(): null", "Disable incremental and scheduled garbage collection."},
            {"collect", "function", "collect(kind: string = \"full\"): null",
             "Request a minor, major, or full garbage collection."},
            {"set_heap_limit", "function", "set_heap_limit(bytes: int): null", "Set the heap limit in bytes."},
            {"set_budget", "function", "set_budget(microseconds: int): null",
             "Set the pause budget in microseconds for safe GC slices."},
            {"get_stats", "function", "get_stats(): SystemGcStats",
             "Return a safe snapshot of GC control and telemetry state."},
            {"SystemGcStats", "type",
             "struct SystemGcStats { enabled, heapLimitBytes, managedMemoryBytes, gcDebtBytes, pauseBudgetUs, "
             "remarkBudgetUs, workerCount, ignoredObjectCount, rememberedObjectCount, regionCount, "
             "edenRegionCount, survivorRegionCount, oldRegionCount, pinnedRegionCount, largeRegionCount, "
             "permanentRegionCount, edenUsedBytes, survivorUsedBytes, oldUsedBytes, pinnedUsedBytes, "
             "largeUsedBytes, permanentUsedBytes, edenLiveBytes, survivorLiveBytes, oldLiveBytes, "
             "pinnedLiveBytes, largeLiveBytes, permanentLiveBytes, lastStepDurationUs, lastStepWork, lastCollectionKind, "
             "lastRequestedCollectionKind, collectionPhase, minorCollectionCount, majorCollectionCount, "
             "fullCollectionCount, minorCollectionTotalDurationUs, majorCollectionTotalDurationUs, "
             "fullCollectionTotalDurationUs, minorCollectionMaxDurationUs, majorCollectionMaxDurationUs, "
             "fullCollectionMaxDurationUs }",
             "Snapshot of safe GC control and telemetry state."},
    };
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.gc\"\n"
            "}\n";
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.gc",
            ZR_NULL,
            0,
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            kTypes,
            ZR_ARRAY_COUNT(kTypes),
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Safe garbage-collection controls and telemetry.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
