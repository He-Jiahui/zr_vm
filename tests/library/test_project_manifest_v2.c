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

static void test_project_manifest_v2_keeps_build_dependencies_phase_separated(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *project;
    SZrLibrary_Project *roundTrippedProject;
    SZrLibrary_ProjectManifestDependencyLockEntry lockEntries[3];
    TZrChar output[2048];
    static const TZrChar manifest[] =
            "{"
            "\"manifestVersion\":2,\"name\":\"app\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\","
            "\"dependencies\":{"
            "\"@shared\":{\"version\":\"^1.0.0\",\"path\":\"../shared-runtime\"}},"
            "\"buildDependencies\":{"
            "\"@shared\":{\"version\":\"^2.0.0\",\"path\":\"../shared-compile\"},"
            "\"@derive\":{\"version\":\"1.4.0\",\"registry\":\"central\"}}"
            "}";
    static const TZrChar expected[] =
            "{\"manifestVersion\":2,\"name\":\"app\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\","
            "\"dependencies\":{"
            "\"@shared\":{\"version\":\"^1.0.0\",\"path\":\"../shared-runtime\"}},"
            "\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"1.4.0\",\"registry\":\"central\"},"
            "\"@shared\":{\"version\":\"^2.0.0\",\"path\":\"../shared-compile\"}}}";
    static const TZrChar expectedLock[] =
            "{\"lockVersion\":1,\"dependencies\":{"
            "\"@shared\":{\"version\":\"1.2.0\",\"contentHash\":\"sha256-runtime\","
            "\"transitiveIdentity\":\"shared-runtime@1.2.0\",\"provider\":\"path\"}},"
            "\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"1.4.0\",\"contentHash\":\"sha256-derive\","
            "\"transitiveIdentity\":\"derive@1.4.0\",\"provider\":\"registry\"},"
            "\"@shared\":{\"version\":\"2.1.0\",\"contentHash\":\"sha256-compile\","
            "\"transitiveIdentity\":\"shared-compile@2.1.0\",\"provider\":\"path\"}}}";

    TEST_ASSERT_NOT_NULL(state);
    project = new_project(state, manifest);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(1u, project->manifestDependencyCount);
    TEST_ASSERT_EQUAL_UINT32(2u, project->manifestBuildDependencyCount);
    TEST_ASSERT_EQUAL_STRING("shared", project->manifestDependencies[0].packageIdentity.packageName);
    TEST_ASSERT_EQUAL_STRING("../shared-runtime", test_string_text(project->manifestDependencies[0].source));
    TEST_ASSERT_EQUAL_STRING("shared", project->manifestBuildDependencies[0].packageIdentity.packageName);
    TEST_ASSERT_EQUAL_STRING("../shared-compile", test_string_text(project->manifestBuildDependencies[0].source));
    TEST_ASSERT_EQUAL_STRING("derive", project->manifestBuildDependencies[1].packageIdentity.packageName);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY,
                          project->manifestBuildDependencies[1].sourceKind);

    memset(output, 0, sizeof(output));
    TEST_ASSERT_TRUE(ZrLibrary_ProjectManifestV2_Write(project, output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING(expected, output);
    roundTrippedProject = new_project(state, output);
    TEST_ASSERT_NOT_NULL(roundTrippedProject);
    TEST_ASSERT_EQUAL_UINT32(1u, roundTrippedProject->manifestDependencyCount);
    TEST_ASSERT_EQUAL_UINT32(2u, roundTrippedProject->manifestBuildDependencyCount);

    memset(lockEntries, 0, sizeof(lockEntries));
    lockEntries[0].packageIdentity = project->manifestDependencies[0].packageIdentity;
    lockEntries[0].resolvedVersion = "1.2.0";
    lockEntries[0].contentHash = "sha256-runtime";
    lockEntries[0].transitiveIdentity = "shared-runtime@1.2.0";
    lockEntries[0].providerSourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH;
    lockEntries[0].providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME;
    lockEntries[1].packageIdentity = project->manifestBuildDependencies[0].packageIdentity;
    lockEntries[1].resolvedVersion = "2.1.0";
    lockEntries[1].contentHash = "sha256-compile";
    lockEntries[1].transitiveIdentity = "shared-compile@2.1.0";
    lockEntries[1].providerSourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH;
    lockEntries[1].providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;
    lockEntries[2].packageIdentity = project->manifestBuildDependencies[1].packageIdentity;
    lockEntries[2].resolvedVersion = "1.4.0";
    lockEntries[2].contentHash = "sha256-derive";
    lockEntries[2].transitiveIdentity = "derive@1.4.0";
    lockEntries[2].providerSourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY;
    lockEntries[2].providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;
    memset(output, 0, sizeof(output));
    lockEntries[2].providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME;
    TEST_ASSERT_FALSE(ZrLibrary_ProjectManifestV2_WriteDependencyLock(
            project, lockEntries, ZR_ARRAY_COUNT(lockEntries), output, sizeof(output)));
    TEST_ASSERT_EQUAL_CHAR('\0', output[0]);
    lockEntries[2].providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;
    lockEntries[2].providerSourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH;
    TEST_ASSERT_FALSE(ZrLibrary_ProjectManifestV2_WriteDependencyLock(
            project, lockEntries, ZR_ARRAY_COUNT(lockEntries), output, sizeof(output)));
    TEST_ASSERT_EQUAL_CHAR('\0', output[0]);
    lockEntries[2].providerSourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY;
    lockEntries[2].packageIdentity = lockEntries[1].packageIdentity;
    TEST_ASSERT_FALSE(ZrLibrary_ProjectManifestV2_WriteDependencyLock(
            project, lockEntries, ZR_ARRAY_COUNT(lockEntries), output, sizeof(output)));
    TEST_ASSERT_EQUAL_CHAR('\0', output[0]);
    lockEntries[2].packageIdentity = project->manifestBuildDependencies[1].packageIdentity;
    lockEntries[2].packageIdentity.packageName[0] = 'x';
    TEST_ASSERT_FALSE(ZrLibrary_ProjectManifestV2_WriteDependencyLock(
            project, lockEntries, ZR_ARRAY_COUNT(lockEntries), output, sizeof(output)));
    TEST_ASSERT_EQUAL_CHAR('\0', output[0]);
    lockEntries[2].packageIdentity = project->manifestBuildDependencies[1].packageIdentity;
    TEST_ASSERT_TRUE(ZrLibrary_ProjectManifestV2_WriteDependencyLock(
            project, lockEntries, ZR_ARRAY_COUNT(lockEntries), output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING(expectedLock, output);

    ZrLibrary_Project_Free(state, roundTrippedProject);
    ZrLibrary_Project_Free(state, project);
    ZrTests_Runtime_State_Destroy(state);
}

static const SZrLibrary_ProjectManifestDependencyLockEntry *find_project_lock_entry(
        const SZrLibrary_Project *project,
        const TZrChar *packageName,
        EZrLibrary_ProviderPhase providerPhase) {
    for (TZrSize index = 0u; index < project->manifestDependencyLockEntryCount; index++) {
        const SZrLibrary_ProjectManifestDependencyLockEntry *entry =
                &project->manifestDependencyLockEntries[index];

        if (entry->providerPhase == providerPhase &&
            strcmp(entry->packageIdentity.packageName, packageName) == 0) {
            return entry;
        }
    }
    return ZR_NULL;
}

static void test_project_manifest_v2_reads_owned_phase_separated_dependency_lock(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *project;
    const SZrLibrary_ProjectManifestDependencyLockEntry *runtimeEntry;
    const SZrLibrary_ProjectManifestDependencyLockEntry *compileEntry;
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar validLock[] =
            "{\"lockVersion\":1,\"dependencies\":{"
            "\"@shared\":{\"version\":\"1.2.0\","
            "\"contentHash\":\"sha256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\","
            "\"transitiveIdentity\":\"sha256:BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBA\","
            "\"provider\":\"path\"}},\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"1.4.0\","
            "\"contentHash\":\"sha256:CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCA\","
            "\"transitiveIdentity\":\"sha256:DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDA\","
            "\"provider\":\"registry\"}}}";
    static const TZrChar manifest[] =
            "{\"manifestVersion\":2,\"name\":\"app\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\","
            "\"dependencies\":{\"@shared\":{\"version\":\"^1.0.0\",\"path\":\"../shared\"}},"
            "\"buildDependencies\":{\"@derive\":{\"version\":\"1.4.0\",\"registry\":\"central\"}}}";
    static const TZrChar *invalidLocks[] = {
            "{\"lockVersion\":2,\"dependencies\":{},\"buildDependencies\":{}}",
            "{\"lockVersion\":1,\"dependencies\":{},\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"1.4.0\",\"contentHash\":\"hash\","
            "\"transitiveIdentity\":\"graph\",\"provider\":\"registry\"}}}",
            "{\"lockVersion\":1,\"dependencies\":{"
            "\"@shared\":{\"version\":\"1.2.0\",\"contentHash\":\"hash\","
            "\"transitiveIdentity\":\"graph\",\"provider\":\"git\"}},"
            "\"buildDependencies\":{\"@derive\":{\"version\":\"1.4.0\","
            "\"contentHash\":\"hash\",\"transitiveIdentity\":\"graph\","
            "\"provider\":\"registry\"}}}",
            "{\"lockVersion\":1,\"dependencies\":{"
            "\"@shared\":{\"version\":\"1.2.0\",\"version\":\"1.3.0\","
            "\"contentHash\":\"hash\",\"transitiveIdentity\":\"graph\","
            "\"provider\":\"path\"}},\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"1.4.0\",\"contentHash\":\"hash\","
            "\"transitiveIdentity\":\"graph\",\"provider\":\"registry\"}}}",
            "{\"lockVersion\":1,\"dependencies\":{"
            "\"@shared\":{\"version\":\"1.2.0\",\"contentHash\":\"hash\","
            "\"transitiveIdentity\":\"graph\",\"provider\":\"path\"}},"
            "\"buildDependencies\":{\"@other\":{\"version\":\"1.0.0\","
            "\"contentHash\":\"hash\",\"transitiveIdentity\":\"graph\","
            "\"provider\":\"registry\"}}}",
            "{\"lockVersion\":1,\"dependencies\":{"
            "\"@shared\":{\"version\":\"1.2.0\",\"contentHash\":\"hash\","
            "\"transitiveIdentity\":\"graph\",\"provider\":\"path\"}},"
            "\"buildDependencies\":{\"@derive\":{\"version\":\"1.4.0\","
            "\"contentHash\":\"hash\",\"transitiveIdentity\":\"graph\","
            "\"provider\":\"registry\"}}} trailing"
    };

    TEST_ASSERT_NOT_NULL(state);
    project = new_project(state, manifest);
    TEST_ASSERT_NOT_NULL(project);
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE(ZrLibrary_ProjectManifestV2_ReadDependencyLock(
            state, project, validLock, error, sizeof(error)));
    TEST_ASSERT_EQUAL_UINT32(2u, project->manifestDependencyLockEntryCount);
    memset(validLock, 0, sizeof(validLock));

    runtimeEntry = find_project_lock_entry(
            project, "shared", ZR_LIBRARY_PROVIDER_PHASE_RUNTIME);
    compileEntry = find_project_lock_entry(
            project, "derive", ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL);
    TEST_ASSERT_NOT_NULL(runtimeEntry);
    TEST_ASSERT_NOT_NULL(compileEntry);
    TEST_ASSERT_EQUAL_STRING("1.2.0", runtimeEntry->resolvedVersion);
    TEST_ASSERT_EQUAL_STRING("sha256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
                             runtimeEntry->contentHash);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH,
                          runtimeEntry->providerSourceKind);
    TEST_ASSERT_EQUAL_STRING("1.4.0", compileEntry->resolvedVersion);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY,
                          compileEntry->providerSourceKind);

    for (TZrSize index = 0u; index < ZR_ARRAY_COUNT(invalidLocks); index++) {
        memset(error, 0, sizeof(error));
        TEST_ASSERT_FALSE(ZrLibrary_ProjectManifestV2_ReadDependencyLock(
                state, project, invalidLocks[index], error, sizeof(error)));
        TEST_ASSERT_TRUE(error[0] != '\0');
        TEST_ASSERT_EQUAL_UINT32(2u, project->manifestDependencyLockEntryCount);
        TEST_ASSERT_EQUAL_STRING(
                "1.4.0",
                find_project_lock_entry(
                        project, "derive", ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL)->resolvedVersion);
    }

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
            "\"dependencies\":{\"@math\":{\"path\":\"../math\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"buildDependencies\":{\"$legacy\":{\"version\":\"1.0.0\",\"path\":\"../legacy\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"buildDependencies\":{\"@derive\":{\"version\":\"1.0.0\",\"path\":\"../derive\","
            "\"git\":\"https://git.example/derive.git\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"buildDependencies\":{\"@derive\":{\"path\":\"../derive\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"aliases\":{\"#derive\":\"@derive\"},"
            "\"buildDependencies\":{\"@derive\":{\"version\":\"1.0.0\",\"path\":\"../derive\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"buildDependencies\":{\"@first\":{\"version\":\"1.0.0\",\"path\":\"../first\"}},"
            "\"buildDependencies\":{\"@second\":{\"version\":\"2.0.0\",\"path\":\"../second\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"buildDependencies\":{\"@derive\":{\"version\":\"1.0.0\",\"version\":\"2.0.0\","
            "\"path\":\"../derive\"}}}"
    };

    TEST_ASSERT_NOT_NULL(state);
    for (TZrSize index = 0u; index < ZR_ARRAY_COUNT(invalidManifests); index++) {
        TEST_ASSERT_NULL(new_project(state, invalidManifests[index]));
    }
    ZrTests_Runtime_State_Destroy(state);
}

static void test_project_manifest_v2_writes_canonical_declarations(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *project;
    SZrLibrary_Project *roundTrippedProject;
    TZrChar output[4096];
    static const TZrChar manifest[] =
            "{"
            "\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"aliases\":{\"#sdk\":\"file:///vendor/sdk\",\"#lib\":\"engine/lib\",\"#math\":\"@math\"},"
            "\"package\":{\"name\":\"@physics\",\"exports\":{\"./matrix\":\"math/matrix\",\".\":\"index\"}},"
            "\"dependencies\":{"
            "\"@render\":{\"version\":\"~2.0.0\",\"registry\":\"central\"},"
            "\"@math\":{\"version\":\"^1.2.0\",\"path\":\"../math\"},"
            "\"@engine\":{\"version\":\"3.0.0\",\"git\":\"ssh://[2001:db8::1]/engine.git\"}"
            "}"
            "}";
    static const TZrChar expected[] =
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"aliases\":{\"#lib\":\"engine/lib\",\"#math\":\"@math\",\"#sdk\":\"file:///vendor/sdk\"},"
            "\"package\":{\"name\":\"@physics\",\"exports\":{\".\":\"index\",\"./matrix\":\"math/matrix\"}},"
            "\"dependencies\":{"
            "\"@engine\":{\"version\":\"3.0.0\",\"git\":\"ssh://[2001:db8::1]/engine.git\"},"
            "\"@math\":{\"version\":\"^1.2.0\",\"path\":\"../math\"},"
            "\"@render\":{\"version\":\"~2.0.0\",\"registry\":\"central\"}"
            "}}";

    TEST_ASSERT_NOT_NULL(state);
    memset(output, 0, sizeof(output));
    project = new_project(state, manifest);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_TRUE(ZrLibrary_ProjectManifestV2_Write(project, output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING(expected, output);
    TEST_ASSERT_NULL(strstr(output, "pathAliases"));
    TEST_ASSERT_NULL(strstr(output, "C:/"));

    roundTrippedProject = new_project(state, output);
    TEST_ASSERT_NOT_NULL(roundTrippedProject);
    TEST_ASSERT_EQUAL_UINT32(2u, roundTrippedProject->manifestVersion);
    TEST_ASSERT_EQUAL_UINT32(3u, roundTrippedProject->manifestAliasCount);
    TEST_ASSERT_EQUAL_UINT32(2u, roundTrippedProject->packageExportCount);
    TEST_ASSERT_EQUAL_UINT32(3u, roundTrippedProject->manifestDependencyCount);
    ZrLibrary_Project_Free(state, roundTrippedProject);
    ZrLibrary_Project_Free(state, project);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_project_manifest_v2_writer_rejects_migration_and_absolute_path_inputs(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *legacyProject;
    SZrLibrary_Project *nonPortableProject;
    TZrChar output[1024];
    static const TZrChar legacyManifest[] =
            "{\"manifestVersion\":1,\"name\":\"legacy\",\"version\":\"1.0.0\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}";
    static const TZrChar *nonPortableManifests[] = {
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"path\":\"C:/cache/math\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"registry\":\"file:///cache/math\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"git\":\"C:/cache/math.git\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"registry\":\"https:///C:/cache/math\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"git\":\"ssh:///C:/cache/math.git\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"registry\":\"https://C:/cache/math\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"registry\":\"https://127.0.0.1/cache/math\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"git\":\"ssh://localhost/repo.git\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"git\":\"git://[::1]/repo.git\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"registry\":\"https://[0:0:0:0:0:0:0:1]/cache/math\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"git\":\"ssh://[0::1]/repo.git\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"git\":\"git://[::ffff:127.0.0.1]/repo.git\"}}}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\","
            "\"dependencies\":{\"@math\":{\"version\":\"1.2.0\",\"git\":\"ssh://[::1%25lo]/repo.git\"}}}"
    };

    TEST_ASSERT_NOT_NULL(state);
    memset(output, 0, sizeof(output));
    legacyProject = new_project(state, legacyManifest);
    TEST_ASSERT_NOT_NULL(legacyProject);
    TEST_ASSERT_FALSE(ZrLibrary_ProjectManifestV2_Write(legacyProject, output, sizeof(output)));
    TEST_ASSERT_EQUAL_CHAR('\0', output[0]);
    ZrLibrary_Project_Free(state, legacyProject);

    for (TZrSize index = 0u; index < ZR_ARRAY_COUNT(nonPortableManifests); index++) {
        nonPortableProject = new_project(state, nonPortableManifests[index]);
        TEST_ASSERT_NOT_NULL(nonPortableProject);
        TEST_ASSERT_FALSE(ZrLibrary_ProjectManifestV2_Write(nonPortableProject, output, sizeof(output)));
        TEST_ASSERT_EQUAL_CHAR('\0', output[0]);
        ZrLibrary_Project_Free(state, nonPortableProject);
    }
    ZrTests_Runtime_State_Destroy(state);
}

static void test_project_manifest_v2_writes_dependency_lock_separately(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *project;
    SZrLibrary_ProjectManifestDependencyLockEntry entries[3];
    TZrChar output[2048];
    static const TZrChar manifest[] =
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\",\"dependencies\":{"
            "\"@render\":{\"version\":\"~2.0.0\",\"registry\":\"https://registry.example/render\"},"
            "\"@math\":{\"version\":\"^1.2.0\",\"path\":\"../math\"},"
            "\"@engine\":{\"version\":\"3.0.0\",\"git\":\"https://git.example/engine.git\"}}}";
    static const TZrChar expected[] =
            "{\"lockVersion\":1,\"dependencies\":{"
            "\"@engine\":{\"version\":\"3.0.1\",\"contentHash\":\"sha256-engine\","
            "\"transitiveIdentity\":\"engine-core@3.0.1\",\"provider\":\"git\"},"
            "\"@math\":{\"version\":\"1.2.3\",\"contentHash\":\"sha256-math\","
            "\"transitiveIdentity\":\"math-core@1.2.3\",\"provider\":\"path\"},"
            "\"@render\":{\"version\":\"2.0.4\",\"contentHash\":\"sha256-render\","
            "\"transitiveIdentity\":\"render-core@2.0.4\",\"provider\":\"registry\"}}}";

    TEST_ASSERT_NOT_NULL(state);
    project = new_project(state, manifest);
    TEST_ASSERT_NOT_NULL(project);
    memset(entries, 0, sizeof(entries));
    entries[0].packageIdentity = project->manifestDependencies[0].packageIdentity;
    entries[0].resolvedVersion = "2.0.4";
    entries[0].contentHash = "sha256-render";
    entries[0].transitiveIdentity = "render-core@2.0.4";
    entries[0].providerSourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY;
    entries[1].packageIdentity = project->manifestDependencies[1].packageIdentity;
    entries[1].resolvedVersion = "1.2.3";
    entries[1].contentHash = "sha256-math";
    entries[1].transitiveIdentity = "math-core@1.2.3";
    entries[1].providerSourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH;
    entries[2].packageIdentity = project->manifestDependencies[2].packageIdentity;
    entries[2].resolvedVersion = "3.0.1";
    entries[2].contentHash = "sha256-engine";
    entries[2].transitiveIdentity = "engine-core@3.0.1";
    entries[2].providerSourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT;

    memset(output, 0, sizeof(output));
    TEST_ASSERT_TRUE(ZrLibrary_ProjectManifestV2_WriteDependencyLock(project, entries, ZR_ARRAY_COUNT(entries), output,
                                                                      sizeof(output)));
    TEST_ASSERT_EQUAL_STRING(expected, output);
    TEST_ASSERT_NULL(strstr(output, "C:/"));
    TEST_ASSERT_NULL(strstr(output, "../math"));
    TEST_ASSERT_FALSE(ZrLibrary_ProjectManifestV2_WriteDependencyLock(project, entries, 2u, output, sizeof(output)));
    TEST_ASSERT_EQUAL_CHAR('\0', output[0]);

    ZrLibrary_Project_Free(state, project);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_project_manifest_v2_reads_required_base_envelope);
    RUN_TEST(test_project_manifest_v2_rejects_incomplete_or_unsupported_base_envelopes);
    RUN_TEST(test_project_manifest_v1_remains_an_explicit_migration_input);
    RUN_TEST(test_project_manifest_v2_reads_structured_alias_package_and_dependency_declarations);
    RUN_TEST(test_project_manifest_v2_keeps_build_dependencies_phase_separated);
    RUN_TEST(test_project_manifest_v2_reads_owned_phase_separated_dependency_lock);
    RUN_TEST(test_project_manifest_v2_rejects_legacy_or_ambiguous_declarations);
    RUN_TEST(test_project_manifest_v2_writes_canonical_declarations);
    RUN_TEST(test_project_manifest_v2_writer_rejects_migration_and_absolute_path_inputs);
    RUN_TEST(test_project_manifest_v2_writes_dependency_lock_separately);

    return UNITY_END();
}
