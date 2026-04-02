//
// zr.system.env descriptor registry.
//

#include "zr_vm_lib_system/env_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

const ZrLibModuleDescriptor *ZrSystem_EnvRegistry_GetModule(void) {
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"getVariable", 1, 1, ZrSystem_Env_GetVariable, "string", "Read an environment variable by name.", ZR_NULL, 0},
    };
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"getVariable", "function", "getVariable(name: string): string?", "Read an environment variable by name."},
    };
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.env\"\n"
            "}\n";
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.env",
            ZR_NULL,
            0,
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            ZR_NULL,
            0,
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Environment helpers.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
