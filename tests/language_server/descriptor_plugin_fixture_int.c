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
        .returnTypeName = "int",
        .documentation = "Descriptor plugin fixture that exposes an int answer().",
        .parameters = ZR_NULL,
        .parameterCount = 0,
    },
    {
        .name = "makePoint",
        .minArgumentCount = 0,
        .maxArgumentCount = 0,
        .callback = ZR_NULL,
        .returnTypeName = "ProbePoint",
        .documentation = "Descriptor plugin fixture that exposes a ProbePoint factory.",
        .parameters = ZR_NULL,
        .parameterCount = 0,
    },
};

static const ZrLibFieldDescriptor g_descriptor_plugin_probe_point_fields[] = {
    ZR_LIB_FIELD_DESCRIPTOR_INIT("x", "int", "Probe point x coordinate."),
    ZR_LIB_FIELD_DESCRIPTOR_INIT("y", "int", "Probe point y coordinate."),
};

static const ZrLibMethodDescriptor g_descriptor_plugin_probe_point_methods[] = {
    {
        .name = "total",
        .minArgumentCount = 0,
        .maxArgumentCount = 0,
        .callback = ZR_NULL,
        .returnTypeName = "int",
        .documentation = "Returns the total coordinate value.",
        .isStatic = ZR_FALSE,
        .parameters = ZR_NULL,
        .parameterCount = 0,
        .contractRole = 0U,
        .genericParameters = ZR_NULL,
        .genericParameterCount = 0,
    },
};

static const ZrLibTypeDescriptor g_descriptor_plugin_types[] = {
    ZR_LIB_TYPE_DESCRIPTOR_INIT("ProbePoint",
                                ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                g_descriptor_plugin_probe_point_fields,
                                ZR_ARRAY_COUNT(g_descriptor_plugin_probe_point_fields),
                                g_descriptor_plugin_probe_point_methods,
                                ZR_ARRAY_COUNT(g_descriptor_plugin_probe_point_methods),
                                ZR_NULL,
                                0,
                                "Descriptor plugin fixture native struct for member navigation tests.",
                                ZR_NULL,
                                ZR_NULL,
                                0,
                                ZR_NULL,
                                0,
                                ZR_NULL,
                                ZR_FALSE,
                                ZR_TRUE,
                                ZR_NULL,
                                ZR_NULL,
                                0),
};

static const ZrLibModuleLinkDescriptor g_descriptor_plugin_module_links[] = {
    {"console", "zr.system.console", "Linked builtin console submodule for chained metadata resolution tests."},
};

static const ZrLibModuleDescriptor g_descriptor_plugin_module = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
    .moduleName = "zr.pluginprobe",
    .constants = ZR_NULL,
    .constantCount = 0,
    .functions = g_descriptor_plugin_functions,
    .functionCount = ZR_ARRAY_COUNT(g_descriptor_plugin_functions),
    .types = g_descriptor_plugin_types,
    .typeCount = ZR_ARRAY_COUNT(g_descriptor_plugin_types),
    .typeHints = ZR_NULL,
    .typeHintCount = 0,
    .typeHintsJson = ZR_NULL,
    .documentation = "Descriptor plugin fixture module with answer(): int.",
    .moduleLinks = g_descriptor_plugin_module_links,
    .moduleLinkCount = ZR_ARRAY_COUNT(g_descriptor_plugin_module_links),
    .moduleVersion = "1.0.0",
    .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
    .requiredCapabilities = 0,
};

const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);

const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return &g_descriptor_plugin_module;
}
