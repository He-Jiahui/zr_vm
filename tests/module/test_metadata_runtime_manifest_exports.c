#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/string.h"

#include <string.h>

#define TEST_TYPE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 3u)
#define TEST_METHOD_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u)
#define TEST_FIELD_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 9u)
#define TEST_METHOD_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 11u)
#define TEST_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 13u)
#define TEST_SIGNATURE_HASH ((TZrUInt64)0x0102030405060708ULL)
#define TEST_MODULE_SIGNATURE_HASH ((TZrUInt64)0x1112131415161718ULL)
#define TEST_LAYOUT_VERSION 2u
#define TEST_LAYOUT_HASH ((TZrUInt64)0x2122232425262728ULL)

void setUp(void) {}
void tearDown(void) {}

static SZrString *test_version(SZrState *state, const char *text) {
    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

static SZrMetadataTokenBinding make_manifest_export_binding(TZrMetadataToken refToken,
                                                            TZrMetadataToken resolvedToken) {
    SZrMetadataTokenBinding binding = {0};

    binding.refToken = refToken;
    binding.refSignatureToken = TEST_SIGNATURE_TOKEN;
    binding.refSignatureHash = TEST_SIGNATURE_HASH;
    binding.expectedMetadataToken = resolvedToken;
    binding.expectedSignatureToken = TEST_SIGNATURE_TOKEN;
    binding.expectedSignatureHash = TEST_SIGNATURE_HASH;
    binding.expectedModuleSignatureHash = TEST_MODULE_SIGNATURE_HASH;
    binding.expectedLayoutVersion = TEST_LAYOUT_VERSION;
    binding.expectedLayoutHash = TEST_LAYOUT_HASH;
    binding.resolvedMetadataToken = resolvedToken;
    binding.resolvedSignatureToken = TEST_SIGNATURE_TOKEN;
    binding.resolvedSignatureHash = TEST_SIGNATURE_HASH;
    binding.resolvedModuleSignatureHash = TEST_MODULE_SIGNATURE_HASH;
    binding.resolvedLayoutVersion = TEST_LAYOUT_VERSION;
    binding.resolvedLayoutHash = TEST_LAYOUT_HASH;
    return binding;
}

static void test_metadata_runtime_mirrors_manifest_export_table(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    const SZrAotManifestExportEntry manifestExports[2] = {
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN,
                    "Widget",
                    TEST_TYPE_TOKEN,
                    0u,
            },
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                    "Widget.make",
                    0u,
                    TEST_METHOD_TOKEN,
            },
    };
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;

    registration.manifestExports = manifestExports;
    registration.manifestExportCount = 2u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_EQUAL_UINT32(2u, runtime->manifestExportCount);
    TEST_ASSERT_EQUAL_PTR(manifestExports, runtime->manifestExports);
    TEST_ASSERT_EQUAL_STRING("Widget", runtime->manifestExports[0].target);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE, runtime->manifestExports[0].kind);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_TOKEN, runtime->manifestExports[0].typeToken);
    TEST_ASSERT_EQUAL_STRING("Widget.make", runtime->manifestExports[1].target);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD, runtime->manifestExports[1].kind);
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_TOKEN, runtime->manifestExports[1].memberToken);
}

static void test_metadata_runtime_reads_manifest_export_view_by_kind_and_target(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    const SZrAotManifestExportEntry manifestExports[3] = {
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN,
                    "Widget",
                    TEST_TYPE_TOKEN,
                    0u,
            },
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                    "Widget.make",
                    0u,
                    TEST_METHOD_TOKEN,
            },
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                    "Widget.value",
                    0u,
                    TEST_FIELD_TOKEN,
            },
    };
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntimeManifestExportView view;
    SZrMetadataRuntime *runtime;

    registration.manifestExports = manifestExports;
    registration.manifestExportCount = 3u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadManifestExportView(
            runtime,
            ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE,
            "Widget",
            &view));
    TEST_ASSERT_EQUAL_PTR(&manifestExports[0], view.entry);
    TEST_ASSERT_EQUAL_UINT32(0u, view.index);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE, view.kind);
    TEST_ASSERT_EQUAL_STRING("Widget", view.target);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_TOKEN, view.typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, view.memberToken);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadManifestExportView(
            runtime,
            ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
            "Widget.make",
            &view));
    TEST_ASSERT_EQUAL_PTR(&manifestExports[1], view.entry);
    TEST_ASSERT_EQUAL_UINT32(1u, view.index);
    TEST_ASSERT_EQUAL_UINT32(0u, view.typeToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_TOKEN, view.memberToken);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadManifestExportView(
            runtime,
            ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD,
            "Widget.value",
            &view));
    TEST_ASSERT_EQUAL_PTR(&manifestExports[2], view.entry);
    TEST_ASSERT_EQUAL_UINT32(2u, view.index);
    TEST_ASSERT_EQUAL_UINT32(0u, view.typeToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_FIELD_TOKEN, view.memberToken);

    view.entry = &manifestExports[0];
    view.target = "stale";
    view.typeToken = TEST_TYPE_TOKEN + 99u;
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadManifestExportView(
            runtime,
            ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE,
            "Missing",
            &view));
    TEST_ASSERT_NULL(view.entry);
    TEST_ASSERT_NULL(view.target);
    TEST_ASSERT_EQUAL_UINT32(0u, view.typeToken);
}

static void test_metadata_runtime_rejects_ambiguous_manifest_export_target(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    const SZrAotManifestExportEntry manifestExports[2] = {
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                    "Widget.make",
                    0u,
                    TEST_METHOD_TOKEN,
            },
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                    "Widget.make",
                    0u,
                    TEST_METHOD_TOKEN + 1u,
            },
    };
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntimeManifestExportView view;
    SZrMetadataRuntime *runtime;

    registration.manifestExports = manifestExports;
    registration.manifestExportCount = 2u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadManifestExportView(
            runtime,
            ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
            "Widget.make",
            &view));
    TEST_ASSERT_NULL(view.entry);
}

static void test_metadata_runtime_rejects_manifest_export_without_required_token(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    const SZrAotManifestExportEntry manifestExports[2] = {
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE,
                    0u,
                    "Widget",
                    TEST_TYPE_TOKEN,
                    0u,
            },
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN,
                    "Widget.make",
                    TEST_TYPE_TOKEN,
                    TEST_METHOD_TOKEN,
            },
    };
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntimeManifestExportView view;
    SZrMetadataRuntime *runtime;

    registration.manifestExports = manifestExports;
    registration.manifestExportCount = 2u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadManifestExportView(
            runtime,
            ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE,
            "Widget",
            &view));
    TEST_ASSERT_NULL(view.entry);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadManifestExportView(
            runtime,
            ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
            "Widget.make",
            &view));
    TEST_ASSERT_NULL(view.entry);
}

static void test_metadata_runtime_manifest_export_binding_gate_accepts_matching_binding(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    const SZrAotManifestExportEntry manifestExports[2] = {
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN,
                    "Widget",
                    TEST_TYPE_TOKEN,
                    0u,
            },
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                    "Widget.make",
                    0u,
                    TEST_METHOD_TOKEN,
            },
    };
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenBinding typeBinding =
            make_manifest_export_binding(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 2u),
                                         TEST_TYPE_TOKEN);
    SZrMetadataTokenBinding methodBinding = make_manifest_export_binding(TEST_METHOD_REF_TOKEN,
                                                                        TEST_METHOD_TOKEN);
    SZrMetadataRuntimeManifestExportView view;
    SZrMetadataRuntimeBindingCompatibilityReport report;
    SZrMetadataRuntime *runtime;

    registration.manifestExports = manifestExports;
    registration.manifestExportCount = 2u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE,
            ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility(
                    runtime,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE,
                    "Widget",
                    &typeBinding,
                    ZR_NULL,
                    ZR_NULL,
                    &view,
                    &report));
    TEST_ASSERT_EQUAL_PTR(&manifestExports[0], view.entry);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_TOKEN, view.typeToken);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE, report.status);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_TOKEN, report.actualMetadataToken);

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE,
            ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility(
                    runtime,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    "Widget.make",
                    &methodBinding,
                    ZR_NULL,
                    ZR_NULL,
                    &view,
                    &report));
    TEST_ASSERT_EQUAL_PTR(&manifestExports[1], view.entry);
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_TOKEN, view.memberToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_TOKEN, report.actualMetadataToken);
}

static void test_metadata_runtime_manifest_export_binding_gate_reports_export_token_mismatch(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    const SZrAotManifestExportEntry manifestExports[1] = {
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                    "Widget.make",
                    0u,
                    TEST_METHOD_TOKEN,
            },
    };
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenBinding binding =
            make_manifest_export_binding(TEST_METHOD_REF_TOKEN, TEST_METHOD_TOKEN + 1u);
    SZrMetadataRuntimeManifestExportView view;
    SZrMetadataRuntimeBindingCompatibilityReport report;
    SZrMetadataRuntime *runtime;

    registration.manifestExports = manifestExports;
    registration.manifestExportCount = 1u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_MANIFEST_EXPORT_TOKEN_MISMATCH,
            ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility(
                    runtime,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    "Widget.make",
                    &binding,
                    ZR_NULL,
                    ZR_NULL,
                    &view,
                    &report));
    TEST_ASSERT_EQUAL_PTR(&manifestExports[0], view.entry);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_MANIFEST_EXPORT_TOKEN_MISMATCH,
                          report.status);
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_TOKEN, report.expectedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_TOKEN + 1u, report.actualMetadataToken);
}

static void test_metadata_runtime_manifest_export_binding_gate_reports_missing_export_and_version_drift(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    const SZrAotManifestExportEntry manifestExports[1] = {
            {
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                    "Widget.make",
                    0u,
                    TEST_METHOD_TOKEN,
            },
    };
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenBinding binding = make_manifest_export_binding(TEST_METHOD_REF_TOKEN,
                                                                  TEST_METHOD_TOKEN);
    SZrMetadataTokenRecord refRecord = {0};
    SZrString *actualVersion;
    SZrMetadataRuntimeManifestExportView view;
    SZrMetadataRuntimeBindingCompatibilityReport report;
    SZrMetadataRuntime *runtime;

    TEST_ASSERT_NOT_NULL(state);
    registration.manifestExports = manifestExports;
    registration.manifestExportCount = 1u;
    refRecord.token = TEST_METHOD_REF_TOKEN;
    refRecord.minModuleVersionInclusive = test_version(state, "1.0.0");
    refRecord.maxModuleVersionExclusive = test_version(state, "2.0.0");
    actualVersion = test_version(state, "2.0.0");

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_MANIFEST_EXPORT_NOT_FOUND,
            ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility(
                    runtime,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    "Widget.missing",
                    &binding,
                    &refRecord,
                    actualVersion,
                    &view,
                    &report));
    TEST_ASSERT_NULL(view.entry);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_MANIFEST_EXPORT_NOT_FOUND,
                          report.status);
    TEST_ASSERT_EQUAL_PTR(actualVersion, report.actualModuleVersion);

    TEST_ASSERT_EQUAL_INT(
            ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_VERSION_MISMATCH,
            ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility(
                    runtime,
                    ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                    "Widget.make",
                    &binding,
                    &refRecord,
                    actualVersion,
                    &view,
                    &report));
    TEST_ASSERT_EQUAL_PTR(&manifestExports[0], view.entry);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_VERSION_MISMATCH,
                          report.status);
    TEST_ASSERT_EQUAL_PTR(refRecord.minModuleVersionInclusive, report.expectedMinVersionInclusive);
    TEST_ASSERT_EQUAL_PTR(refRecord.maxModuleVersionExclusive, report.expectedMaxVersionExclusive);
    TEST_ASSERT_EQUAL_PTR(actualVersion, report.actualModuleVersion);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_metadata_runtime_mirrors_manifest_export_table);
    RUN_TEST(test_metadata_runtime_reads_manifest_export_view_by_kind_and_target);
    RUN_TEST(test_metadata_runtime_rejects_ambiguous_manifest_export_target);
    RUN_TEST(test_metadata_runtime_rejects_manifest_export_without_required_token);
    RUN_TEST(test_metadata_runtime_manifest_export_binding_gate_accepts_matching_binding);
    RUN_TEST(test_metadata_runtime_manifest_export_binding_gate_reports_export_token_mismatch);
    RUN_TEST(test_metadata_runtime_manifest_export_binding_gate_reports_missing_export_and_version_drift);
    return UNITY_END();
}
