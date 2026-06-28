#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/string.h"

#include <string.h>

#define TEST_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u)
#define TEST_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_ASSEMBLY_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u)
#define TEST_MODULE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 1u)
#define TEST_RESOLVED_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u)
#define TEST_RESOLVED_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 8u)
#define TEST_REF_SIGNATURE_HASH ((TZrUInt64)0x0102030405060708ULL)
#define TEST_RESOLVED_SIGNATURE_HASH ((TZrUInt64)0x1112131415161718ULL)
#define TEST_MODULE_SIGNATURE_HASH ((TZrUInt64)0x2122232425262728ULL)
#define TEST_LAYOUT_VERSION 3u
#define TEST_LAYOUT_HASH ((TZrUInt64)0x3132333435363738ULL)

void setUp(void) {}
void tearDown(void) {}

static SZrString *test_version(SZrState *state, const char *text) {
    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

static SZrMetadataTokenBinding make_matching_binding(void) {
    SZrMetadataTokenBinding binding = {0};

    binding.refToken = TEST_REF_TOKEN;
    binding.refSignatureToken = TEST_REF_SIGNATURE_TOKEN;
    binding.refSignatureHash = TEST_REF_SIGNATURE_HASH;
    binding.expectedMetadataToken = TEST_RESOLVED_TOKEN;
    binding.expectedSignatureToken = TEST_RESOLVED_SIGNATURE_TOKEN;
    binding.expectedSignatureHash = TEST_RESOLVED_SIGNATURE_HASH;
    binding.expectedModuleSignatureHash = TEST_MODULE_SIGNATURE_HASH;
    binding.expectedLayoutVersion = TEST_LAYOUT_VERSION;
    binding.expectedLayoutHash = TEST_LAYOUT_HASH;
    binding.resolvedMetadataToken = TEST_RESOLVED_TOKEN;
    binding.resolvedSignatureToken = TEST_RESOLVED_SIGNATURE_TOKEN;
    binding.resolvedSignatureHash = TEST_RESOLVED_SIGNATURE_HASH;
    binding.resolvedModuleSignatureHash = TEST_MODULE_SIGNATURE_HASH;
    binding.resolvedLayoutVersion = TEST_LAYOUT_VERSION;
    binding.resolvedLayoutHash = TEST_LAYOUT_HASH;
    return binding;
}

static void test_binding_compatibility_accepts_matching_identity_layout_and_version(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataTokenRecord record = {0};
    SZrMetadataRuntimeBindingCompatibilityReport report;

    TEST_ASSERT_NOT_NULL(state);
    record.minModuleVersionInclusive = test_version(state, "1.2.0");
    record.maxModuleVersionExclusive = test_version(state, "2.0.0");

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding,
                                                                 &record,
                                                                 test_version(state, "1.5.0"),
                                                                 &report));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE, report.status);
    TEST_ASSERT_EQUAL_UINT32(TEST_RESOLVED_TOKEN, report.expectedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_RESOLVED_TOKEN, report.actualMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_RESOLVED_SIGNATURE_TOKEN, report.expectedSignatureToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_RESOLVED_SIGNATURE_TOKEN, report.actualSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_RESOLVED_SIGNATURE_HASH, report.expectedSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_RESOLVED_SIGNATURE_HASH, report.actualSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_MODULE_SIGNATURE_HASH, report.expectedModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_MODULE_SIGNATURE_HASH, report.actualModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(TEST_LAYOUT_VERSION, report.expectedLayoutVersion);
    TEST_ASSERT_EQUAL_UINT32(TEST_LAYOUT_VERSION, report.actualLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(TEST_LAYOUT_HASH, report.expectedLayoutHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_LAYOUT_HASH, report.actualLayoutHash);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_binding_compatibility_reports_module_version_range_mismatch(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataTokenRecord record = {0};
    SZrMetadataRuntimeBindingCompatibilityReport report;
    SZrString *actualVersion;

    TEST_ASSERT_NOT_NULL(state);
    record.minModuleVersionInclusive = test_version(state, "1.0.0");
    record.maxModuleVersionExclusive = test_version(state, "2.0.0");
    actualVersion = test_version(state, "2.0.0");

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_VERSION_MISMATCH,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding,
                                                                 &record,
                                                                 actualVersion,
                                                                 &report));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_VERSION_MISMATCH, report.status);
    TEST_ASSERT_EQUAL_PTR(record.minModuleVersionInclusive, report.expectedMinVersionInclusive);
    TEST_ASSERT_EQUAL_PTR(record.maxModuleVersionExclusive, report.expectedMaxVersionExclusive);
    TEST_ASSERT_EQUAL_PTR(actualVersion, report.actualModuleVersion);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_binding_compatibility_ignores_missing_or_legacy_version_strings(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataTokenRecord record = {0};

    TEST_ASSERT_NOT_NULL(state);
    record.minModuleVersionInclusive = test_version(state, "1.x");
    record.maxModuleVersionExclusive = test_version(state, "2.0.0");

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding,
                                                                 &record,
                                                                 test_version(state, "0.5.0"),
                                                                 ZR_NULL));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_binding_compatibility_reports_module_signature_before_member_signature(void) {
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataRuntimeBindingCompatibilityReport report;

    binding.resolvedModuleSignatureHash = TEST_MODULE_SIGNATURE_HASH + 1u;
    binding.resolvedSignatureHash = TEST_RESOLVED_SIGNATURE_HASH + 1u;

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_SIGNATURE_HASH_MISMATCH,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding, ZR_NULL, ZR_NULL, &report));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_SIGNATURE_HASH_MISMATCH, report.status);
    TEST_ASSERT_EQUAL_UINT64(TEST_MODULE_SIGNATURE_HASH, report.expectedModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_MODULE_SIGNATURE_HASH + 1u, report.actualModuleSignatureHash);
}

static void test_binding_compatibility_accepts_assembly_ref_to_module_token_mapping(void) {
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataRuntimeBindingCompatibilityReport report;

    binding.expectedMetadataToken = TEST_ASSEMBLY_REF_TOKEN;
    binding.expectedSignatureToken = TEST_REF_SIGNATURE_TOKEN;
    binding.resolvedMetadataToken = TEST_MODULE_TOKEN;
    binding.resolvedSignatureToken = TEST_RESOLVED_SIGNATURE_TOKEN;

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding, ZR_NULL, ZR_NULL, &report));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE, report.status);
    TEST_ASSERT_EQUAL_UINT32(TEST_ASSEMBLY_REF_TOKEN, report.expectedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_MODULE_TOKEN, report.actualMetadataToken);
}

static void test_binding_compatibility_reports_metadata_token_mismatch(void) {
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataRuntimeBindingCompatibilityReport report;

    binding.resolvedMetadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 9u);

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_METADATA_TOKEN_MISMATCH,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding, ZR_NULL, ZR_NULL, &report));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_METADATA_TOKEN_MISMATCH, report.status);
    TEST_ASSERT_EQUAL_UINT32(TEST_RESOLVED_TOKEN, report.expectedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 9u),
                             report.actualMetadataToken);
}

static void test_binding_compatibility_reports_signature_token_mismatch(void) {
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataRuntimeBindingCompatibilityReport report;

    binding.resolvedSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 9u);

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_TOKEN_MISMATCH,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding, ZR_NULL, ZR_NULL, &report));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_TOKEN_MISMATCH, report.status);
    TEST_ASSERT_EQUAL_UINT32(TEST_RESOLVED_SIGNATURE_TOKEN, report.expectedSignatureToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 9u),
                             report.actualSignatureToken);
}

static void test_binding_compatibility_reports_member_signature_hash_mismatch(void) {
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataRuntimeBindingCompatibilityReport report;

    binding.resolvedSignatureHash = TEST_RESOLVED_SIGNATURE_HASH + 1u;

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_HASH_MISMATCH,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding, ZR_NULL, ZR_NULL, &report));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_HASH_MISMATCH, report.status);
    TEST_ASSERT_EQUAL_UINT64(TEST_RESOLVED_SIGNATURE_HASH, report.expectedSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_RESOLVED_SIGNATURE_HASH + 1u, report.actualSignatureHash);
}

static void test_binding_compatibility_reports_layout_version_before_layout_hash(void) {
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataRuntimeBindingCompatibilityReport report;

    binding.resolvedLayoutVersion = TEST_LAYOUT_VERSION + 1u;
    binding.resolvedLayoutHash = TEST_LAYOUT_HASH + 1u;

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_VERSION_MISMATCH,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding, ZR_NULL, ZR_NULL, &report));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_VERSION_MISMATCH, report.status);
    TEST_ASSERT_EQUAL_UINT32(TEST_LAYOUT_VERSION, report.expectedLayoutVersion);
    TEST_ASSERT_EQUAL_UINT32(TEST_LAYOUT_VERSION + 1u, report.actualLayoutVersion);
}

static void test_binding_compatibility_reports_layout_hash_mismatch(void) {
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataRuntimeBindingCompatibilityReport report;

    binding.resolvedLayoutHash = TEST_LAYOUT_HASH + 1u;

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_HASH_MISMATCH,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding, ZR_NULL, ZR_NULL, &report));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_HASH_MISMATCH, report.status);
    TEST_ASSERT_EQUAL_UINT64(TEST_LAYOUT_HASH, report.expectedLayoutHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_LAYOUT_HASH + 1u, report.actualLayoutHash);
}

static void test_binding_compatibility_treats_missing_layout_side_as_mismatch(void) {
    SZrMetadataTokenBinding binding = make_matching_binding();

    binding.expectedLayoutVersion = 0u;
    binding.expectedLayoutHash = 0u;

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_VERSION_MISMATCH,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(&binding, ZR_NULL, ZR_NULL, ZR_NULL));
}

static void test_binding_compatibility_rejects_null_binding_without_report(void) {
    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT,
            ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(ZR_NULL, ZR_NULL, ZR_NULL, ZR_NULL));
}

static void test_function_binding_compatibility_returns_first_incompatible_binding(void) {
    SZrFunction function = {0};
    SZrMetadataTokenBinding bindings[2];
    SZrMetadataTokenRecord records[2] = {0};
    SZrMetadataRuntimeBindingCompatibilityReport report;
    const SZrMetadataTokenBinding *failedBinding = ZR_NULL;
    const SZrMetadataTokenRecord *failedRecord = ZR_NULL;

    bindings[0] = make_matching_binding();
    bindings[0].refToken = TEST_REF_TOKEN;
    bindings[1] = make_matching_binding();
    bindings[1].refToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 2u);
    bindings[1].resolvedSignatureHash = TEST_RESOLVED_SIGNATURE_HASH + 1u;
    records[0].token = bindings[0].refToken;
    records[1].token = bindings[1].refToken;
    function.moduleMetadataBindings = bindings;
    function.moduleMetadataBindingLength = 2u;
    function.metadataTokenRecords = records;
    function.metadataTokenRecordLength = 2u;

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_HASH_MISMATCH,
            ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility(&function,
                                                                          ZR_NULL,
                                                                          &failedBinding,
                                                                          &failedRecord,
                                                                          &report));
    TEST_ASSERT_EQUAL_PTR(&bindings[1], failedBinding);
    TEST_ASSERT_EQUAL_PTR(&records[1], failedRecord);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_HASH_MISMATCH, report.status);
}

static void test_function_binding_compatibility_uses_module_ref_record_for_version_range(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataTokenRecord record = {0};
    SZrMetadataRuntimeBindingCompatibilityReport report;
    const SZrMetadataTokenBinding *failedBinding = ZR_NULL;
    const SZrMetadataTokenRecord *failedRecord = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);
    binding.refToken = TEST_ASSEMBLY_REF_TOKEN;
    binding.expectedMetadataToken = TEST_ASSEMBLY_REF_TOKEN;
    binding.resolvedMetadataToken = TEST_MODULE_TOKEN;
    record.token = TEST_ASSEMBLY_REF_TOKEN;
    record.minModuleVersionInclusive = test_version(state, "1.0.0");
    record.maxModuleVersionExclusive = test_version(state, "2.0.0");
    function.moduleMetadataBindings = &binding;
    function.moduleMetadataBindingLength = 1u;
    function.moduleMetadataTokenRecords = &record;
    function.moduleMetadataTokenRecordLength = 1u;

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_VERSION_MISMATCH,
            ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility(&function,
                                                                          test_version(state, "2.0.0"),
                                                                          &failedBinding,
                                                                          &failedRecord,
                                                                          &report));
    TEST_ASSERT_EQUAL_PTR(&binding, failedBinding);
    TEST_ASSERT_EQUAL_PTR(&record, failedRecord);
    TEST_ASSERT_EQUAL_PTR(record.minModuleVersionInclusive, report.expectedMinVersionInclusive);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_binding_compatibility_accepts_empty_bindings_and_clears_outputs(void) {
    SZrFunction function = {0};
    const SZrMetadataTokenBinding *failedBinding = (const SZrMetadataTokenBinding *)1;
    const SZrMetadataTokenRecord *failedRecord = (const SZrMetadataTokenRecord *)1;
    SZrMetadataRuntimeBindingCompatibilityReport report;

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE,
            ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility(&function,
                                                                          ZR_NULL,
                                                                          &failedBinding,
                                                                          &failedRecord,
                                                                          &report));
    TEST_ASSERT_NULL(failedBinding);
    TEST_ASSERT_NULL(failedRecord);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE, report.status);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binding_compatibility_accepts_matching_identity_layout_and_version);
    RUN_TEST(test_binding_compatibility_reports_module_version_range_mismatch);
    RUN_TEST(test_binding_compatibility_ignores_missing_or_legacy_version_strings);
    RUN_TEST(test_binding_compatibility_reports_module_signature_before_member_signature);
    RUN_TEST(test_binding_compatibility_accepts_assembly_ref_to_module_token_mapping);
    RUN_TEST(test_binding_compatibility_reports_metadata_token_mismatch);
    RUN_TEST(test_binding_compatibility_reports_signature_token_mismatch);
    RUN_TEST(test_binding_compatibility_reports_member_signature_hash_mismatch);
    RUN_TEST(test_binding_compatibility_reports_layout_version_before_layout_hash);
    RUN_TEST(test_binding_compatibility_reports_layout_hash_mismatch);
    RUN_TEST(test_binding_compatibility_treats_missing_layout_side_as_mismatch);
    RUN_TEST(test_binding_compatibility_rejects_null_binding_without_report);
    RUN_TEST(test_function_binding_compatibility_returns_first_incompatible_binding);
    RUN_TEST(test_function_binding_compatibility_uses_module_ref_record_for_version_range);
    RUN_TEST(test_function_binding_compatibility_accepts_empty_bindings_and_clears_outputs);
    return UNITY_END();
}
