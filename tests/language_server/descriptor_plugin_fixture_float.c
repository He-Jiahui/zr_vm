#include "zr_vm_library/native_binding.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const ZrLibFunctionDescriptor g_descriptor_plugin_functions[] = {
    {
        .name = "answer",
        .minArgumentCount = 0,
        .maxArgumentCount = 0,
        .callback = ZR_NULL,
        .returnTypeName = "float",
        .documentation = "Descriptor plugin fixture that exposes a float answer().",
        .parameters = ZR_NULL,
        .parameterCount = 0,
    },
};

static const ZrLibModuleDescriptor g_descriptor_plugin_module = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
    .moduleName = "zr.pluginprobe",
    .constants = ZR_NULL,
    .constantCount = 0,
    .functions = g_descriptor_plugin_functions,
    .functionCount = ZR_ARRAY_COUNT(g_descriptor_plugin_functions),
    .types = ZR_NULL,
    .typeCount = 0,
    .typeHints = ZR_NULL,
    .typeHintCount = 0,
    .typeHintsJson = ZR_NULL,
    .documentation = "Descriptor plugin fixture module with answer(): float.",
    .moduleLinks = ZR_NULL,
    .moduleLinkCount = 0,
    .moduleVersion = "1.0.0",
    .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
    .requiredCapabilities = 0,
};

const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);

const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return &g_descriptor_plugin_module;
}
