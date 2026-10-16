/* Native registry attach and record OOM paths must leave owned state intact. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

#include "native_binding/native_binding_internal.h"
#include "zr_vm_core/global.h"

typedef struct RegistryOomAllocator {
    TZrBool armed;
    TZrSize failArrayAllocation;
    TZrSize arrayAllocationAttempts;
    TZrBool failArrayGrowth;
    TZrSize failGlobalAllocation;
    TZrSize globalAllocationAttempts;
    TZrSize liveArrayBlocks;
    TZrSize liveGlobalBlocks;
} RegistryOomAllocator;

static TZrPtr registry_oom_allocator(TZrPtr userData,
                                     TZrPtr pointer,
                                     TZrSize originalSize,
                                     TZrSize newSize,
                                     TZrInt64 type) {
    RegistryOomAllocator *tracker = (RegistryOomAllocator *)userData;
    TZrBool fail = ZR_FALSE;
    TZrPtr result;

    if (tracker->armed && type == ZR_MEMORY_NATIVE_TYPE_GLOBAL &&
        pointer == ZR_NULL && newSize != 0u && tracker->failGlobalAllocation != 0u) {
        fail = ++tracker->globalAllocationAttempts == tracker->failGlobalAllocation;
    }

    if (tracker->armed && type == ZR_MEMORY_NATIVE_TYPE_ARRAY && newSize != 0u &&
        ((tracker->failArrayAllocation != 0u && pointer == ZR_NULL) ||
         tracker->failArrayGrowth)) {
        ++tracker->arrayAllocationAttempts;
        fail = tracker->failArrayAllocation != 0u
                       ? tracker->arrayAllocationAttempts == tracker->failArrayAllocation
                       : (tracker->failArrayGrowth && pointer != ZR_NULL && newSize > originalSize);
    }
    if (fail) {
        return ZR_NULL;
    }

    result = newSize == 0u ? (free(pointer), ZR_NULL) : realloc(pointer, newSize);
    if (type == ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        if (pointer == ZR_NULL && result != ZR_NULL) {
            ++tracker->liveArrayBlocks;
        } else if (pointer != ZR_NULL && newSize == 0u) {
            --tracker->liveArrayBlocks;
        }
    }
    if (type == ZR_MEMORY_NATIVE_TYPE_GLOBAL) {
        if (pointer == ZR_NULL && result != ZR_NULL && newSize != 0u) {
            ++tracker->liveGlobalBlocks;
        } else if (pointer != ZR_NULL && newSize == 0u) {
            --tracker->liveGlobalBlocks;
        }
    }
    return result;
}

static SZrGlobalState *registry_oom_global_new(RegistryOomAllocator *tracker) {
    SZrCallbackGlobal callbacks = {0};
    return ZrCore_GlobalState_New(registry_oom_allocator, tracker, 0x4f4f4d, &callbacks);
}

static void registry_oom_fill_array(SZrArray *array) {
    memset(array->head + array->length * array->elementSize,
           0,
           (array->capacity - array->length) * array->elementSize);
    array->length = array->capacity;
}

static void registry_oom_prepare_descriptor(ZrLibModuleDescriptor *descriptor,
                                            const TZrChar *moduleName,
                                            const ZrLibFunctionDescriptor *functions,
                                            TZrSize functionCount) {
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION;
    descriptor->moduleName = moduleName;
    descriptor->functions = functions;
    descriptor->functionCount = functionCount;
    descriptor->providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME;
}

static TZrBool registry_oom_native_callback(ZrLibCallContext *context,
                                            SZrTypeValue *result) {
    ZR_UNUSED_PARAMETER(context);
    ZR_UNUSED_PARAMETER(result);
    return ZR_TRUE;
}

static int test_attach_initial_array_oom(TZrSize failureOrdinal) {
    RegistryOomAllocator tracker = {0};
    SZrGlobalState *global = registry_oom_global_new(&tracker);
    ZrLibrary_NativeRegistryState *registry;
    TZrSize liveArrays;
    TZrSize liveGlobals;

    if (global == ZR_NULL) return 1;
    liveArrays = tracker.liveArrayBlocks;
    liveGlobals = tracker.liveGlobalBlocks;
    tracker.armed = ZR_TRUE;
    tracker.failArrayAllocation = failureOrdinal;
    if (ZrLibrary_NativeRegistry_Attach(global) ||
        global->nativeRegistryState != ZR_NULL ||
        global->nativeRegistryStateCleanup != ZR_NULL ||
        tracker.liveArrayBlocks != liveArrays || tracker.liveGlobalBlocks != liveGlobals) {
        ZrCore_GlobalState_Free(global);
        return 2;
    }

    tracker.armed = ZR_FALSE;
    tracker.failArrayAllocation = 0u;
    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        ZrCore_GlobalState_Free(global);
        return 3;
    }
    registry = native_registry_get(global);
    if (registry == ZR_NULL || !registry->moduleRecords.isValid ||
        !registry->bindingEntries.isValid || !registry->pluginHandles.isValid) {
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 4;
    }
    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
    return 0;
}

static int test_module_record_growth_oom(void) {
    RegistryOomAllocator tracker = {0};
    SZrGlobalState *global = registry_oom_global_new(&tracker);
    ZrLibrary_NativeRegistryState *registry;
    ZrLibModuleDescriptor descriptor;
    TZrSize liveGlobals;

    if (global == ZR_NULL || !ZrLibrary_NativeRegistry_Attach(global)) {
        if (global != ZR_NULL) ZrCore_GlobalState_Free(global);
        return 10;
    }
    registry = native_registry_get(global);
    registry_oom_fill_array(&registry->moduleRecords);
    registry_oom_prepare_descriptor(&descriptor, "zr.test.oom-record", ZR_NULL, 0u);
    tracker.armed = ZR_TRUE;
    tracker.failArrayGrowth = ZR_TRUE;
    liveGlobals = tracker.liveGlobalBlocks;

    if (ZrLibrary_NativeRegistry_RegisterModule(global, &descriptor) ||
        ZrLibrary_NativeRegistry_FindModule(global, descriptor.moduleName) != ZR_NULL ||
        tracker.liveGlobalBlocks != liveGlobals) {
        tracker.armed = ZR_FALSE;
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 11;
    }
    tracker.armed = ZR_FALSE;
    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
    return 0;
}

static int test_binding_entry_growth_oom(void) {
    RegistryOomAllocator tracker = {0};
    SZrGlobalState *global = registry_oom_global_new(&tracker);
    ZrLibrary_NativeRegistryState *registry;
    ZrLibModuleDescriptor descriptor;
    ZrLibFunctionDescriptor functionDescriptor = {0};
    SZrTypeValue value;
    TZrSize bindingCount;

    if (global == ZR_NULL || !ZrLibrary_NativeRegistry_Attach(global)) {
        if (global != ZR_NULL) ZrCore_GlobalState_Free(global);
        return 20;
    }
    registry = native_registry_get(global);
    registry_oom_fill_array(&registry->bindingEntries);
    registry_oom_prepare_descriptor(&descriptor, "zr.test.oom-binding", &functionDescriptor, 1u);
    functionDescriptor.name = "invoke";
    functionDescriptor.callback = registry_oom_native_callback;
    bindingCount = registry->bindingEntries.length;
    tracker.armed = ZR_TRUE;
    tracker.failArrayGrowth = ZR_TRUE;

    if (native_binding_make_callable_value(global->mainThreadState,
                                           registry,
                                           ZR_LIB_RESOLVED_BINDING_FUNCTION,
                                           &descriptor,
                                           ZR_NULL,
                                           ZR_NULL,
                                           &functionDescriptor,
                                           &value) ||
        registry->bindingEntries.length != bindingCount) {
        tracker.armed = ZR_FALSE;
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 21;
    }
    tracker.armed = ZR_FALSE;
    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
    return 0;
}

/* Each mode executes in its own CTest process; never dereference a failed-load descriptor. */
static int test_plugin_transaction_oom(int mode, const char *fixturePath) {
    RegistryOomAllocator tracker = {0};
    SZrGlobalState *global = registry_oom_global_new(&tracker);
    ZrLibrary_NativeRegistryState *registry;
    const ZrLibModuleDescriptor *previous = ZR_NULL;
    const ZrLibModuleDescriptor *loaded;
    TZrSize moduleCount, handleCount, liveGlobals;
    int result = 0;

    if (global == ZR_NULL || !ZrLibrary_NativeRegistry_Attach(global)) {
        if (global != ZR_NULL) ZrCore_GlobalState_Free(global);
        return 30;
    }
    registry = native_registry_get(global);
    if (mode == 4) {
        previous = native_registry_load_plugin_descriptor(
                global->mainThreadState, registry, fixturePath, "zr.pluginprobe");
        if (previous == ZR_NULL) {
            ZrCore_GlobalState_Free(global);
            return 31;
        }
    }
    if (mode == 1) registry_oom_fill_array(&registry->pluginHandles);
    if (mode == 3) registry_oom_fill_array(&registry->moduleRecords);
    moduleCount = registry->moduleRecords.length;
    handleCount = registry->pluginHandles.length;
    liveGlobals = tracker.liveGlobalBlocks;
    tracker.failArrayGrowth = mode == 1 || mode == 3;
    tracker.failGlobalAllocation = mode == 2 || mode == 4 ? 3u : 0u;
    tracker.armed = ZR_TRUE;
    loaded = native_registry_load_plugin_descriptor(
            global->mainThreadState, registry, fixturePath, "zr.pluginprobe");
    tracker.armed = ZR_FALSE;
    if (loaded != ZR_NULL ||
        ZrLibrary_NativeRegistry_FindModule(global, "zr.pluginprobe") != previous ||
        registry->moduleRecords.length != moduleCount ||
        registry->pluginHandles.length != handleCount ||
        tracker.liveGlobalBlocks != liveGlobals) {
        result = 32;
    }
    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
    return result;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc == 3) {
        if (strcmp(argv[1], "plugin_handle_growth") == 0) return test_plugin_transaction_oom(1, argv[2]);
        if (strcmp(argv[1], "plugin_string_allocation") == 0) return test_plugin_transaction_oom(2, argv[2]);
        if (strcmp(argv[1], "plugin_record_growth") == 0) return test_plugin_transaction_oom(3, argv[2]);
        if (strcmp(argv[1], "plugin_reload_allocation") == 0) return test_plugin_transaction_oom(4, argv[2]);
    }
    if (argc != 2) return 64;
    if (strcmp(argv[1], "attach_array_1") == 0) return test_attach_initial_array_oom(1u);
    if (strcmp(argv[1], "attach_array_2") == 0) return test_attach_initial_array_oom(2u);
    if (strcmp(argv[1], "attach_array_3") == 0) return test_attach_initial_array_oom(3u);
    if (strcmp(argv[1], "module_record_growth") == 0) return test_module_record_growth_oom();
    if (strcmp(argv[1], "binding_entry_growth") == 0) return test_binding_entry_growth_oom();
    return 65;
}
