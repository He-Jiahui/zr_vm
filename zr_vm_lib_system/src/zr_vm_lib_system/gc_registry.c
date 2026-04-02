//
// zr.system.gc descriptor registry.
//

#include "zr_vm_lib_system/gc_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

const ZrLibModuleDescriptor *ZrSystem_GcRegistry_GetModule(void) {
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"start", 0, 0, ZrSystem_Gc_Start, "null", "Resume garbage collection.", ZR_NULL, 0},
            {"stop", 0, 0, ZrSystem_Gc_Stop, "null", "Pause garbage collection.", ZR_NULL, 0},
            {"step", 0, 0, ZrSystem_Gc_Step, "null", "Run one garbage-collection step.", ZR_NULL, 0},
            {"collect", 0, 0, ZrSystem_Gc_Collect, "null", "Run a full garbage collection.", ZR_NULL, 0},
    };
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"start", "function", "start(): null", "Resume garbage collection."},
            {"stop", "function", "stop(): null", "Pause garbage collection."},
            {"step", "function", "step(): null", "Run one garbage-collection step."},
            {"collect", "function", "collect(): null", "Run a full garbage collection."},
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
            ZR_NULL,
            0,
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Garbage-collection controls.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
