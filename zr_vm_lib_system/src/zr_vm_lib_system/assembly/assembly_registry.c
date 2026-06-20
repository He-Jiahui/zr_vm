//
// zr.system.assembly descriptor registry.
//

#include "zr_vm_lib_system/assembly_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const ZrLibParameterDescriptor g_resource_name_parameter[] = {
        {"name", "string", "Logical resource name inside the current .zrm assembly."},
};

const ZrLibModuleDescriptor *ZrSystem_AssemblyRegistry_GetModule(void) {
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"resourceExists", 1, 1, ZrSystem_Assembly_ResourceExists, "bool",
             "Return whether the current project assembly contains the named resource.",
             g_resource_name_parameter, ZR_ARRAY_COUNT(g_resource_name_parameter), ZR_NULL, 0, 0U, 0U},
            {"readResourceText", 1, 1, ZrSystem_Assembly_ReadResourceText, "string",
             "Read a current project assembly resource as UTF-8 text.",
             g_resource_name_parameter, ZR_ARRAY_COUNT(g_resource_name_parameter), ZR_NULL, 0, 0U, 0U},
            {"readResourceBytes", 1, 1, ZrSystem_Assembly_ReadResourceBytes, "array",
             "Read a current project assembly resource as byte integers.",
             g_resource_name_parameter, ZR_ARRAY_COUNT(g_resource_name_parameter), ZR_NULL, 0, 0U, 0U},
    };
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"resourceExists", "function", "resourceExists(name: string): bool",
             "Return whether the current project assembly contains the named resource."},
            {"readResourceText", "function", "readResourceText(name: string): string",
             "Read a current project assembly resource as UTF-8 text."},
            {"readResourceBytes", "function", "readResourceBytes(name: string): array",
             "Read a current project assembly resource as byte integers."},
    };
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.assembly\"\n"
            "}\n";
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.assembly",
            ZR_NULL,
            0,
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            ZR_NULL,
            0,
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Current .zrm assembly resource helpers.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
            ZR_NULL,
    };

    return &kModule;
}
