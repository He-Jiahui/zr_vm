//
// zr.system.process descriptor registry.
//

#include "zr_vm_lib_system/process_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

const ZrLibModuleDescriptor *ZrSystem_ProcessRegistry_GetModule(void) {
    static const ZrLibConstantDescriptor kConstants[] = {
            {"arguments", ZR_LIB_CONSTANT_KIND_ARRAY, 0, 0.0, ZR_NULL, ZR_FALSE, "Process argument list.", "string[]"},
    };
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"sleepMilliseconds", 1, 1, ZrSystem_Process_SleepMilliseconds, "null", "Sleep for the requested number of milliseconds."},
            {"exit", 1, 1, ZrSystem_Process_Exit, "null", "Terminate the process with the given exit code."},
    };
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"arguments", "constant", "arguments: string[]", "Process argument list."},
            {"sleepMilliseconds", "function", "sleepMilliseconds(milliseconds: int): null", "Sleep for the requested number of milliseconds."},
            {"exit", "function", "exit(code: int): null", "Terminate the process with the given exit code."},
    };
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.process\"\n"
            "}\n";
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.process",
            kConstants,
            ZR_ARRAY_COUNT(kConstants),
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            ZR_NULL,
            0,
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Process helpers.",
            ZR_NULL,
            0,
    };

    return &kModule;
}
