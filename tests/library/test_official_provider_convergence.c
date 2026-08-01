#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_lib_testing/module.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/attribute_contract.h"
#include "zr_vm_parser/compile_tool.h"

#include <string.h>

typedef struct SZrExpectedOfficialModule {
    const TZrChar *moduleName;
    EZrLibOfficialModuleTier tier;
    EZrLibrary_ProviderPhase phase;
} SZrExpectedOfficialModule;

static const SZrExpectedOfficialModule k_expected_modules[] = {
        {"zr.builtin", ZR_LIB_OFFICIAL_MODULE_TIER_N0, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.container", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.iteration", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.math", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.task", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.debug", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.ffi", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.network", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.network.tcp", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.network.udp", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.pooling", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.reflection", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.assembly", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.console", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.env", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.exception", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.fs", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.gc", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.process", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.system.vm", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.thread", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME},
        {"zr.compile", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL},
        {"zr.compile.declaration", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL},
        {"zr.testing", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_TEST},
};

void setUp(void) {}

void tearDown(void) {}

static ZrLibModuleDescriptor make_descriptor(const TZrChar *moduleName,
                                             EZrLibrary_ProviderPhase phase,
                                             const TZrChar *publicContractHash) {
    ZrLibModuleDescriptor descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION;
    descriptor.moduleName = moduleName;
    descriptor.minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION;
    descriptor.providerPhase = phase;
    descriptor.publicContractHash = publicContractHash;
    return descriptor;
}

static void test_official_inventory_is_frozen_unique_and_phase_typed(void) {
    TZrSize index;
    TZrSize otherIndex;

    TEST_ASSERT_EQUAL_UINT64(ZR_ARRAY_COUNT(k_expected_modules),
                             ZrLibrary_OfficialModuleInventory_GetCount());
    for (index = 0U; index < ZR_ARRAY_COUNT(k_expected_modules); index++) {
        const ZrLibOfficialModuleInventoryEntry *entry =
                ZrLibrary_OfficialModuleInventory_GetAt(index);
        const ZrLibOfficialModuleInventoryEntry *found;

        TEST_ASSERT_NOT_NULL(entry);
        TEST_ASSERT_EQUAL_STRING(k_expected_modules[index].moduleName, entry->moduleName);
        TEST_ASSERT_EQUAL_INT(k_expected_modules[index].tier, entry->tier);
        TEST_ASSERT_EQUAL_INT(k_expected_modules[index].phase, entry->phase);
        found = ZrLibrary_OfficialModuleInventory_Find(entry->moduleName);
        TEST_ASSERT_EQUAL_PTR(entry, found);

        for (otherIndex = index + 1U;
             otherIndex < ZR_ARRAY_COUNT(k_expected_modules);
             otherIndex++) {
            TEST_ASSERT_NOT_EQUAL(0, strcmp(entry->moduleName,
                                            k_expected_modules[otherIndex].moduleName));
        }
    }

    TEST_ASSERT_NULL(ZrLibrary_OfficialModuleInventory_GetAt(
            ZrLibrary_OfficialModuleInventory_GetCount()));
    TEST_ASSERT_NULL(ZrLibrary_OfficialModuleInventory_Find("debug"));
    TEST_ASSERT_NULL(ZrLibrary_OfficialModuleInventory_Find("zr.unknown"));
    TEST_ASSERT_NULL(ZrLibrary_OfficialModuleInventory_Find(ZR_NULL));
}

static void test_owner_descriptors_converge_on_inventory_phase(void) {
    const SZrParserCompileToolModuleDescriptor *compileModule =
            ZrParser_CompileTool_FindModule("zr.compile");
    const SZrParserCompileToolModuleDescriptor *declarationModule =
            ZrParser_CompileTool_FindModule("zr.compile.declaration");
    const ZrLibModuleDescriptor *testingModule = ZrVmLibTesting_GetModuleDescriptor();
    const ZrLibOfficialModuleInventoryEntry *entry;
    const SZrParserAttributeSchema *schema;

    TEST_ASSERT_NOT_NULL(compileModule);
    TEST_ASSERT_NOT_NULL(declarationModule);
    TEST_ASSERT_NOT_NULL(testingModule);

    entry = ZrLibrary_OfficialModuleInventory_Find(compileModule->moduleName);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, compileModule->providerPhase);
    entry = ZrLibrary_OfficialModuleInventory_Find(declarationModule->moduleName);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, declarationModule->providerPhase);
    entry = ZrLibrary_OfficialModuleInventory_Find(testingModule->moduleName);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, testingModule->providerPhase);

    schema = ZrParser_AttributeContract_FindBuiltinByRole(ZR_PARSER_ATTRIBUTE_ROLE_USAGE);
    TEST_ASSERT_NOT_NULL(schema);
    entry = ZrLibrary_OfficialModuleInventory_Find(schema->ownerModule);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, schema->providerPhase);
    schema = ZrParser_AttributeContract_FindBuiltinByRole(ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM);
    TEST_ASSERT_NOT_NULL(schema);
    entry = ZrLibrary_OfficialModuleInventory_Find(schema->ownerModule);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, schema->providerPhase);
    schema = ZrParser_AttributeContract_FindBuiltinByRole(ZR_PARSER_ATTRIBUTE_ROLE_TEST);
    TEST_ASSERT_NOT_NULL(schema);
    entry = ZrLibrary_OfficialModuleInventory_Find(schema->ownerModule);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, schema->providerPhase);
}

static void test_registry_rejects_legacy_phase_and_duplicate_official_providers(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrLibModuleDescriptor legacyDebug = make_descriptor(
            "debug", ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, "legacy-debug");
    ZrLibModuleDescriptor wrongTestingPhase = make_descriptor(
            "zr.testing", ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, "testing-v1");
    ZrLibModuleDescriptor testing = make_descriptor(
            "zr.testing", ZR_LIBRARY_PROVIDER_PHASE_TEST, "testing-v1");
    ZrLibModuleDescriptor equivalentTesting = make_descriptor(
            "zr.testing", ZR_LIBRARY_PROVIDER_PHASE_TEST, "testing-v1");
    ZrLibModuleDescriptor conflictingTesting = make_descriptor(
            "zr.testing", ZR_LIBRARY_PROVIDER_PHASE_TEST, "testing-v2");

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &legacyDebug));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_RESERVED_OFFICIAL_MODULE,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &wrongTestingPhase));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_PHASE_MISMATCH,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));

    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &testing));
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &testing));
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &equivalentTesting));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_DUPLICATE_OFFICIAL_PROVIDER,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &conflictingTesting));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_DUPLICATE_OFFICIAL_PROVIDER,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_provider_phase_admission_is_host_specific(void) {
    TEST_ASSERT_TRUE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME));
    TEST_ASSERT_FALSE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_LIBRARY_PROVIDER_PHASE_TEST));
    TEST_ASSERT_FALSE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL));
    TEST_ASSERT_TRUE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_TEST, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME));
    TEST_ASSERT_TRUE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_TEST, ZR_LIBRARY_PROVIDER_PHASE_TEST));
    TEST_ASSERT_FALSE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_TEST, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL));
    TEST_ASSERT_TRUE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME));
    TEST_ASSERT_FALSE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, ZR_LIBRARY_PROVIDER_PHASE_TEST));
    TEST_ASSERT_TRUE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_official_inventory_is_frozen_unique_and_phase_typed);
    RUN_TEST(test_owner_descriptors_converge_on_inventory_phase);
    RUN_TEST(test_registry_rejects_legacy_phase_and_duplicate_official_providers);
    RUN_TEST(test_provider_phase_admission_is_host_specific);
    return UNITY_END();
}
