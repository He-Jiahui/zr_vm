#include "zr_vm_lib_iteration/module.h"

#include "zr_vm_core/object.h"

static const ZrLibGenericParameterDescriptor g_iteration_generic_parameter[] = {
        {"T", "Iteration element type.", ZR_NULL, 0},
};

static const TZrChar *g_iterator_implements[] = {
        "zr.iteration.Enumerator<T>",
};

static const ZrLibFieldDescriptor g_enumerator_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("current", "T", "Current element after a successful moveNext.",
                                          ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD),
};

static const ZrLibFieldDescriptor g_async_iterator_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("current", "T", "Current element after a successful asynchronous moveNext.",
                                          ZR_MEMBER_CONTRACT_ROLE_ASYNC_ITERATOR_CURRENT_FIELD),
};

static const ZrLibMethodDescriptor g_iterable_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getEnumerator", 0, 0, ZR_NULL,
                                           "zr.iteration.Enumerator<T>",
                                           "Create a new synchronous enumerator.", ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};

static const ZrLibMethodDescriptor g_enumerator_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("moveNext", 0, 0, ZR_NULL, "bool",
                                           "Advance the enumerator.", ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT),
};

static const ZrLibMethodDescriptor g_async_iterator_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("moveNext", 0, 0, ZR_NULL, "zr.task.Task<bool>",
                                           "Advance the asynchronous iterator.", ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ASYNC_ITERATOR_MOVE_NEXT),
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("close", 0, 0, ZR_NULL, "zr.task.Task<void>",
                                           "Asynchronously release the iterator.", ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ASYNC_ITERATOR_CLOSE),
};

static const ZrLibTypeDescriptor g_iteration_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Iterable", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                                             ZR_NULL, 0, g_iterable_methods, ZR_ARRAY_COUNT(g_iterable_methods),
                                             ZR_NULL, 0, "Repeatable synchronous iteration capability.",
                                             ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE,
                                             ZR_NULL, g_iteration_generic_parameter,
                                             ZR_ARRAY_COUNT(g_iteration_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Enumerator", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                                             g_enumerator_fields, ZR_ARRAY_COUNT(g_enumerator_fields),
                                             g_enumerator_methods, ZR_ARRAY_COUNT(g_enumerator_methods),
                                             ZR_NULL, 0, "Single-pass synchronous cursor capability.",
                                             ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE,
                                             ZR_NULL, g_iteration_generic_parameter,
                                             ZR_ARRAY_COUNT(g_iteration_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERATOR)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Iterator", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                             ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                             "Compiler-backed opaque synchronous iterator carrier.",
                                             ZR_NULL, g_iterator_implements, ZR_ARRAY_COUNT(g_iterator_implements),
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                             g_iteration_generic_parameter,
                                             ZR_ARRAY_COUNT(g_iteration_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERATOR)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("AsyncIterator", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                                             g_async_iterator_fields, ZR_ARRAY_COUNT(g_async_iterator_fields),
                                             g_async_iterator_methods, ZR_ARRAY_COUNT(g_async_iterator_methods),
                                             ZR_NULL, 0, "Single-pass asynchronous cursor capability.",
                                             ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE,
                                             ZR_NULL, g_iteration_generic_parameter,
                                             ZR_ARRAY_COUNT(g_iteration_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ASYNC_ITERATOR)),
};

static const ZrLibModuleDescriptor g_iteration_module_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.iteration",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = ZR_NULL,
        .functionCount = 0,
        .types = g_iteration_types,
        .typeCount = ZR_ARRAY_COUNT(g_iteration_types),
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = ZR_NULL,
        .documentation = "Canonical synchronous and asynchronous iteration capabilities.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = 0,
        .onMaterialize = ZR_NULL,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
        .publicContractHash = "zr.iteration:v1:canonical-iterator-protocols",
};

const ZrLibModuleDescriptor *ZrVmLibIteration_Runtime_GetModuleDescriptor(void) {
    return &g_iteration_module_descriptor;
}
