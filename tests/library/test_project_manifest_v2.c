#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/project.h"

void setUp(void) {
}

void tearDown(void) {
}

static const TZrChar *test_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }
    return ZrCore_String_GetNativeString(value);
}

static SZrLibrary_Project *new_project(SZrState *state, const TZrChar *manifest) {
    return ZrLibrary_Project_New(state,
                                 (TZrNativeString)manifest,
                                 "E:/repo/manifest-v2/manifest.zrp");
}

static void test_project_manifest_v2_reads_required_base_envelope(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *project;
    static const TZrChar manifest[] =
            "{"
            "\"manifestVersion\":2,"
            "\"name\":\"physics\","
            "\"version\":\"1.0.0\","
            "\"kind\":\"library\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"index\""
            "}";

    TEST_ASSERT_NOT_NULL(state);
    project = new_project(state, manifest);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(2u, project->manifestVersion);
    TEST_ASSERT_EQUAL_STRING("physics", test_string_text(project->name));
    TEST_ASSERT_EQUAL_STRING("1.0.0", test_string_text(project->version));
    TEST_ASSERT_EQUAL_STRING("src", test_string_text(project->source));
    TEST_ASSERT_EQUAL_STRING("bin", test_string_text(project->binary));
    TEST_ASSERT_EQUAL_STRING("index", test_string_text(project->entry));

    ZrLibrary_Project_Free(state, project);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_project_manifest_v2_rejects_incomplete_or_unsupported_base_envelopes(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    static const TZrChar *missingRequiredFields[] = {
            "{\"manifestVersion\":2,\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"binary\":\"bin\",\"entry\":\"index\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"source\":\"src\",\"entry\":\"index\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"source\":\"src\",\"binary\":\"bin\"}"
    };
    static const TZrChar fractionalVersion[] =
            "{\"manifestVersion\":2.5,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}";
    static const TZrChar futureVersion[] =
            "{\"manifestVersion\":3,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}";

    TEST_ASSERT_NOT_NULL(state);
    for (TZrSize index = 0u; index < ZR_ARRAY_COUNT(missingRequiredFields); index++) {
        TEST_ASSERT_NULL(new_project(state, missingRequiredFields[index]));
    }
    TEST_ASSERT_NULL(new_project(state, fractionalVersion));
    TEST_ASSERT_NULL(new_project(state, futureVersion));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_project_manifest_v1_remains_an_explicit_migration_input(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *project;
    static const TZrChar manifest[] =
            "{\"manifestVersion\":1,\"name\":\"legacy\",\"version\":\"1.0.0\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}";
    static const TZrChar missingVersionManifest[] =
            "{\"name\":\"legacy-default\",\"version\":\"1.0.0\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}";

    TEST_ASSERT_NOT_NULL(state);
    project = new_project(state, manifest);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(1u, project->manifestVersion);
    TEST_ASSERT_EQUAL_STRING("legacy", test_string_text(project->name));
    ZrLibrary_Project_Free(state, project);

    project = new_project(state, missingVersionManifest);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(1u, project->manifestVersion);
    TEST_ASSERT_EQUAL_STRING("legacy-default", test_string_text(project->name));
    ZrLibrary_Project_Free(state, project);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_project_manifest_v2_reads_structured_alias_package_and_dependency_declarations(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *project;
    SZrLibrary_ModuleSpecifier aliasRequest;
    SZrLibrary_ModuleSpecifier packageRequest;
    SZrLibrary_ModuleSpecifier resolvedSpecifier;
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];
    static const TZrChar manifest[] =
            "{"
            "\"manifestVersion\":2,"
            "\"name\":\"physics\","
            "\"version\":\"1.0.0\","
            "\"kind\":\"library\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"index\","
            "\"aliases\":{\"#lib\":\"engine/lib\",\"#math\":\"@math\","
            "\"#sdk\":\"file:///vendor/sdk\",\"#source\":\"file:///vendor/main.zr\","
            "\"#artifact\":\"file:///vendor/default.zrm\",\"#archive\":\"file:///vendor/archive.zrp\"},"
            "\"package\":{\"name\":\"@physics\",\"exports\":{\".\":\"index\","
            "\"./matrix\":\"math/matrix\"}},"
            "\"dependencies\":{"
            "\"@math\":{\"version\":\"^1.2.0\",\"path\":\"../math\"},"
            "\"@render\":{\"version\":\"~2.0.0\",\"registry\":\"https://registry.example/render\"},"
            "\"@engine\":{\"version\":\"3.0.0\",\"git\":\"https://git.example/engine.git\"}"
            "}"
            "}";

    TEST_ASSERT_NOT_NULL(state);
    project = new_project(state, manifest);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(6u, project->manifestAliasCount);
    TEST_ASSERT_EQUAL_STRING("#lib", test_string_text(project->manifestAliases[0].root));
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE, project->manifestAliases[0].target.kind);
    TEST_ASSERT_EQUAL_STRING("engine.lib", project->manifestAliases[0].target.identity.segments);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_DOMAIN_PACKAGE, project->packageIdentity.domain);
    TEST_ASSERT_EQUAL_STRING("physics", project->packageIdentity.packageName);
    TEST_ASSERT_EQUAL_UINT32(2u, project->packageExportCount);
    TEST_ASSERT_EQUAL_STRING(".", test_string_text(project->packageExports[0].key));
    TEST_ASSERT_EQUAL_STRING("index", project->packageExports[0].target.identity.segments);
    TEST_ASSERT_EQUAL_UINT32(3u, project->manifestDependencyCount);
    TEST_ASSERT_EQUAL_STRING("math", project->manifestDependencies[0].packageIdentity.packageName);
    TEST_ASSERT_EQUAL_STRING("^1.2.0", test_string_text(project->manifestDependencies[0].versionRequirement));
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH,
                          project->manifestDependencies[0].sourceKind);
    TEST_ASSERT_EQUAL_STRING("../math", test_string_text(project->manifestDependencies[0].source));
    TEST_ASSERT_EQUAL_STRING("render", project->manifestDependencies[1].packageIdentity.packageName);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY,
                          project->manifestDependencies[1].sourceKind);
    TEST_ASSERT_EQUAL_STRING("https://registry.example/render", test_string_text(project->manifestDependencies[1].source));
    TEST_ASSERT_EQUAL_STRING("engine", project->manifestDependencies[2].packageIdentity.packageName);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT,
                          project->manifestDependencies[2].sourceKind);
    TEST_ASSERT_EQUAL_STRING("https://git.example/engine.git", test_string_text(project->manifestDependencies[2].source));

    memset(&aliasRequest, 0, sizeof(aliasRequest));
    memset(&packageRequest, 0, sizeof(packageRequest));
    memset(&resolvedSpecifier, 0, sizeof(resolvedSpecifier));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE(ZrLibrary_ModuleSpecifier_Parse("#lib/tool", &aliasRequest, error, sizeof(error)));
    TEST_ASSERT_TRUE(ZrLibrary_Project_ResolveManifestAlias(project, &aliasRequest, &resolvedSpecifier));
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE, resolvedSpecifier.kind);
    TEST_ASSERT_EQUAL_STRING("engine.lib.tool", resolvedSpecifier.identity.segments);

    TEST_ASSERT_TRUE(ZrLibrary_ModuleSpecifier_Parse("#sdk/tool", &aliasRequest, error, sizeof(error)));
    TEST_ASSERT_TRUE(ZrLibrary_Project_ResolveManifestAlias(project, &aliasRequest, &resolvedSpecifier));
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_FILE, resolvedSpecifier.kind);
    TEST_ASSERT_EQUAL_STRING("file:///vendor/sdk/tool", resolvedSpecifier.locator);

    TEST_ASSERT_TRUE(ZrLibrary_ModuleSpecifier_Parse("#source/tool", &aliasRequest, error, sizeof(error)));
    TEST_ASSERT_FALSE(ZrLibrary_Project_ResolveManifestAlias(project, &aliasRequest, &resolvedSpecifier));
    TEST_ASSERT_EQUAL_INT(0, resolvedSpecifier.kind);
    TEST_ASSERT_TRUE(ZrLibrary_ModuleSpecifier_Parse("#artifact/tool", &aliasRequest, error, sizeof(error)));
    TEST_ASSERT_FALSE(ZrLibrary_Project_ResolveManifestAlias(project, &aliasRequest, &resolvedSpecifier));
    TEST_ASSERT_EQUAL_INT(0, resolvedSpecifier.kind);
    TEST_ASSERT_TRUE(ZrLibrary_ModuleSpecifier_Parse("#archive/tool", &aliasRequest, error, sizeof(error)));
    TEST_ASSERT_TRUE(ZrLibrary_Project_ResolveManifestAlias(project, &aliasRequest, &resolvedSpecifier));
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_FILE, resolvedSpecifier.kind);
    TEST_ASSERT_EQUAL_STRING("file:///vendor/archive.zrp/tool", resolvedSpecifier.locator);

    TEST_ASSERT_TRUE(ZrLibrary_ModuleSpecifier_Parse("@physics/matrix", &packageRequest, error, sizeof(error)));
    TEST_ASSERT_TRUE(ZrLibrary_Project_ResolvePackageExport(project, &packageRequest, &resolvedSpecifier));
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE, resolvedSpecifier.kind);
    TEST_ASSERT_EQUAL_STRING("math.matrix", resolvedSpecifier.identity.segments);
    TEST_ASSERT_TRUE(ZrLibrary_ModuleSpecifier_Parse("@physics/hidden", &packageRequest, error, sizeof(error)));
    TEST_ASSERT_FALSE(ZrLibrary_Project_ResolvePackageExport(project, &packageRequest, &resolvedSpecifier));

    ZrLibrary_Project_Free(state, project);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_project_manifest_v2_rejects_legacy_or_ambiguous_declarations(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    static const TZrChar *invalidManifests[] = {
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"pathAliases\":{\"@legacy\":\"legacy/module\"}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\",\"dependency\":\"legacy\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\",\"local\":\"legacy\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"aliases\":{\"#loop\":\"#other\"}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"aliases\":{\"#missing\":\"@undeclared\"}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"package\":{\"name\":\"@org/math\",\"exports\":{\".\":\"index\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"$legacy\":{\"version\":\"1.0.0\",\"path\":\"../legacy\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"references\":{\"&legacy\":{\"path\":\"../legacy/legacy.zrp\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.0.0\",\"path\":\"../math\","
            "\"registry\":\"https://registry.example/math\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"path\":\"../math\"}}}"
    };

    TEST_ASSERT_NOT_NULL(state);
    for (TZrSize index = 0u; index < ZR_ARRAY_COUNT(invalidManifests); index++) {
        TEST_ASSERT_NULL(new_project(state, invalidManifests[index]));
    }
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_project_manifest_v2_reads_required_base_envelope);
    RUN_TEST(test_project_manifest_v2_rejects_incomplete_or_unsupported_base_envelopes);
    RUN_TEST(test_project_manifest_v1_remains_an_explicit_migration_input);
    RUN_TEST(test_project_manifest_v2_reads_structured_alias_package_and_dependency_declarations);
    RUN_TEST(test_project_manifest_v2_rejects_legacy_or_ambiguous_declarations);

    return UNITY_END();
}
