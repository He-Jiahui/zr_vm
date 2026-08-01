#include "runtime_internal.h"

#include "zr_vm_parser/attribute_contract.h"

static const ZrLibParameterDescriptor g_assert_parameters[] = {
        {"condition", "bool", "Condition required to be true.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE},
        {"message", "string", "Optional failure message.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE},
};

static const ZrLibParameterDescriptor g_equal_parameters[] = {
        {"actual", "T", "Actual value.", ZR_LIB_PARAMETER_PASSING_MODE_IN},
        {"expected", "T", "Expected value.", ZR_LIB_PARAMETER_PASSING_MODE_IN},
};

static const ZrLibParameterDescriptor g_throws_parameters[] = {
        {"action", "fn() -> void", "Synchronous action required to throw E.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE},
};

static const ZrLibGenericParameterDescriptor g_single_t[] = {
        {"T", "Compared value type.", ZR_NULL, 0U},
};

static const ZrLibGenericParameterDescriptor g_single_e[] = {
        {"E", "Expected exception type.", ZR_NULL, 0U},
};

static const ZrLibFunctionDescriptor g_testing_functions[] = {
        {"assert", 1U, 2U, ZrVmLibTesting_Assert, "void",
         "Raise AssertionFailure when condition is false.",
         g_assert_parameters, ZR_ARRAY_COUNT(g_assert_parameters), ZR_NULL, 0U, 0U, 0U},
        {"equal", 2U, 2U, ZrVmLibTesting_Equal, "void",
         "Compare values through canonical equality.",
         g_equal_parameters, ZR_ARRAY_COUNT(g_equal_parameters),
         g_single_t, ZR_ARRAY_COUNT(g_single_t), 0U, 0U},
        {"throws", 1U, 1U, ZrVmLibTesting_Throws, "E",
         "Return the exception thrown by a synchronous action.",
         g_throws_parameters, ZR_ARRAY_COUNT(g_throws_parameters),
         g_single_e, ZR_ARRAY_COUNT(g_single_e), 0U, 0U},
};

static const ZrLibFieldDescriptor g_skip_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("reason", "string", "Non-empty compile-time skip reason."),
};

static const ZrLibFieldDescriptor g_assertion_failure_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("assertionKind", "int", "Canonical assertion kind."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("sourceLine", "int", "Call-site source line when available."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("message", "string", "Bounded failure message."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("expected", "string", "Bounded expected snapshot."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("actual", "string", "Bounded actual snapshot."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("exception", "string", "Bounded exception snapshot."),
};

static const ZrLibTypeDescriptor g_testing_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Test", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                    ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL, 0U,
                                    "Metadata schema for an ordinary test function.",
                                    ZR_NULL, ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0U),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Case", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                    ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL, 0U,
                                    "Metadata schema for compile-time bound test arguments.",
                                    ZR_NULL, ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0U),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Skip", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                    g_skip_fields, ZR_ARRAY_COUNT(g_skip_fields),
                                    ZR_NULL, 0U, ZR_NULL, 0U,
                                    "Metadata schema for a retained skipped test.",
                                    ZR_NULL, ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0U),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("AssertionFailure", ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                                    g_assertion_failure_fields,
                                    ZR_ARRAY_COUNT(g_assertion_failure_fields),
                                    ZR_NULL, 0U, ZR_NULL, 0U,
                                    "Structured assertion exception.",
                                    "Error", ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0U),
};

static const ZrLibAttributeRoleDescriptor g_testing_roles[] = {
        {"zr.testing.test", 0xdf51f287U, ZR_PARSER_ATTRIBUTE_ROLE_TEST,
         ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION, ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT,
         ZR_FALSE, "Test"},
        {"zr.testing.case", 0xbe2aca8fU, ZR_PARSER_ATTRIBUTE_ROLE_TEST_CASE,
         ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION, ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT,
         ZR_TRUE, "Case"},
        {"zr.testing.skip", 0x91a67a48U, ZR_PARSER_ATTRIBUTE_ROLE_TEST_SKIP,
         ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION, ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT,
         ZR_FALSE, "Skip"},
};

static const ZrLibModuleLinkDescriptor g_testing_links[] = {
        {"task", "zr.task", "Async tests return the canonical Task<void>."},
};

static const ZrLibModuleDescriptor g_testing_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.testing",
        .functions = g_testing_functions,
        .functionCount = ZR_ARRAY_COUNT(g_testing_functions),
        .types = g_testing_types,
        .typeCount = ZR_ARRAY_COUNT(g_testing_types),
        .documentation = "Test-phase metadata and assertion provider.",
        .moduleLinks = g_testing_links,
        .moduleLinkCount = ZR_ARRAY_COUNT(g_testing_links),
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_TEST,
        .attributeRoles = g_testing_roles,
        .attributeRoleCount = ZR_ARRAY_COUNT(g_testing_roles),
        .publicContractHash = "zr.testing:v1:test-case-skip:assert-equal-throws",
};

const ZrLibModuleDescriptor *ZrVmLibTesting_Runtime_GetModuleDescriptor(void) {
    return &g_testing_descriptor;
}
