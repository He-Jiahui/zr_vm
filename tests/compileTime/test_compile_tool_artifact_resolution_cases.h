#ifndef ZR_VM_TEST_COMPILE_TOOL_ARTIFACT_RESOLUTION_CASES_H
#define ZR_VM_TEST_COMPILE_TOOL_ARTIFACT_RESOLUTION_CASES_H

#define ZR_TEST_TRANSITIVE_IDENTITY_A \
    "sha256:uZUQq8UmbWYobrUOMIYjSt_JG2D7HnXK8ukUtWWZfpA"
#define ZR_TEST_TRANSITIVE_IDENTITY_B \
    "sha256:4fXo9MO0fon89fs4DUbqNBrH81gjQtk_-nsfkGaBxXk"
#define ZR_TEST_TRANSITIVE_IDENTITY_SUBMODULE \
    "sha256:jDSiSBqyKdp5fDuV2h-bsYn_HROwH1fQPhLP42j23Qg"

// CompileTool package resolution fixtures and compiler-sandbox contract cases.
static TZrBool write_compile_tool_fixture_file(
        const TZrChar *path,
        const TZrByte *bytes,
        TZrSize byteCount) {
    FILE *file;
    TZrBool ok;

    if (!ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }
    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    ok = (TZrBool)(fwrite(bytes, 1U, byteCount, file) == byteCount);
    fclose(file);
    return ok;
}

static TZrBool format_compile_tool_fixture_hash(
        const TZrByte *bytes,
        TZrSize byteCount,
        TZrChar *outHash,
        TZrSize outHashSize) {
    return ZrParser_CompileToolContentHash_Bytes(
            bytes, byteCount, outHash, outHashSize);
}

static void assert_compile_tool_sha256_boundaries(void) {
    static const struct {
        TZrSize length;
        const TZrChar *expected;
    } cases[] = {
            {0U, "sha256:47DEQpj8HBSa-_TImW-5JCeuQeRkm5NMpJWZG3hSuFU"},
            {55U, "sha256:n0OQ-NMMLdkuyfCVtl4rmumwqSWlJY4kHJ8ekQ9zQxg"},
            {56U, "sha256:s1Q5pKxvCUi21vnjxq8PX1kM4g8b3nCQ73lwaG7Gc4o"},
            {63U, "sha256:fT50oF19sVvOStnsBljqmOPwbu7PFrTG__LaRX3cLzQ"},
            {64U, "sha256:_-BU_nrgy23GXDr5th1SCfQ5hR20PQulmXM33xVGaOs"},
            {65U, "sha256:Y1NhxIu56rFBmOduqKt_GkFoXWrWKqkUbTAdTxfrCuA"},
            {129U, "sha256:wSywJKLlVRzKDgj86PHF4xRVXMP-9jKe6ZSj23UhZq4"},
    };
    TZrByte bytes[129];
    TZrChar actual[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];

    memset(bytes, 'a', sizeof(bytes));
    for (TZrSize index = 0U; index < ZR_ARRAY_COUNT(cases); index++) {
        TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
                cases[index].length == 0U ? ZR_NULL : bytes,
                cases[index].length,
                actual,
                sizeof(actual)));
        TEST_ASSERT_EQUAL_STRING(cases[index].expected, actual);
    }
}

static TZrBool hash_compile_tool_fixture_file(
        const TZrChar *path,
        TZrChar *outHash,
        TZrSize outHashSize) {
    TZrByte *bytes = ZR_NULL;
    TZrSize byteCount = 0U;

    if (!ZrTests_ReadFileBytes(path, &bytes, &byteCount)) {
        return ZR_FALSE;
    }
    if (!format_compile_tool_fixture_hash(
                bytes, byteCount, outHash, outHashSize)) {
        free(bytes);
        return ZR_FALSE;
    }
    free(bytes);
    return ZR_TRUE;
}

static SZrLibrary_Project *new_compile_tool_fixture_project_with_requirement(
        const TZrChar *versionRequirement) {
    TZrChar manifest[1024];
    int written = snprintf(
            manifest,
            sizeof(manifest),
            "{"
            "\"manifestVersion\":2,"
            "\"name\":\"consumer\","
            "\"version\":\"1.0.0\","
            "\"kind\":\"executable\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\","
            "\"dependencies\":{"
            "\"@derive\":{\"version\":\"^1.0.0\",\"path\":\"../derive-runtime\"}},"
            "\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"%s\",\"path\":\"../derive-compile\"}}"
            "}",
            versionRequirement);

    if (written < 0 || (TZrSize)written >= sizeof(manifest)) {
        return ZR_NULL;
    }

    return ZrLibrary_Project_New(
            g_state,
            (TZrNativeString)manifest,
            "E:/repo/compile-tool-sandbox/consumer.zrp");
}

static SZrLibrary_Project *new_compile_tool_fixture_project(void) {
    return new_compile_tool_fixture_project_with_requirement("^1.0.0");
}

static TZrBool write_compile_tool_fixture_archive_named(
        const TZrChar *suiteName,
        EZrLibrary_ProviderPhase phase,
        const TZrChar *publicContractHash,
        const TZrChar *declaredModuleHash,
        const TZrChar *assemblyName,
        const TZrChar *moduleKey,
        TZrChar *outArchivePath,
        TZrSize outArchivePathSize,
        TZrChar *outActualModuleHash,
        TZrSize outActualModuleHashSize,
        TZrChar *outArchiveHash,
        TZrSize outArchiveHashSize) {
    static const TZrByte moduleBytes[] = "compile-tool-zro-v1";
    TZrChar modulePath[ZR_TESTS_PATH_MAX];
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];
    SZrLibrary_ZrmPackModule module = {0};
    SZrLibrary_ZrmPackRequest request = {0};

    if (!ZrTests_Path_GetGeneratedArtifact(
                "compile_time",
                suiteName,
                "provider",
                ".zrm",
                outArchivePath,
                outArchivePathSize) ||
        !ZrTests_Path_GetGeneratedArtifact(
                "compile_time",
                suiteName,
                "provider_main",
                ".zro",
                modulePath,
                sizeof(modulePath)) ||
        !write_compile_tool_fixture_file(
                modulePath, moduleBytes, sizeof(moduleBytes) - 1U)) {
        return ZR_FALSE;
    }

    if (!format_compile_tool_fixture_hash(
                moduleBytes,
                sizeof(moduleBytes) - 1U,
                outActualModuleHash,
                outActualModuleHashSize)) {
        return ZR_FALSE;
    }
    module.moduleKey = moduleKey;
    module.sourcePath = modulePath;
    module.hash = declaredModuleHash != ZR_NULL
                          ? declaredModuleHash
                          : outActualModuleHash;
    request.outputPath = outArchivePath;
    request.assembly.name = assemblyName;
    request.assembly.version = "1.4.0";
    request.assembly.kind = "compile-tool";
    request.assembly.entryModule = moduleKey;
    request.assembly.providerPhase = phase;
    request.assembly.publicContractHash = publicContractHash;
    request.modules = &module;
    request.moduleCount = 1U;
    if (!ZrLibrary_Zrm_WriteArchive(&request, error, sizeof(error))) {
        return ZR_FALSE;
    }
    return hash_compile_tool_fixture_file(
            outArchivePath, outArchiveHash, outArchiveHashSize);
}

static TZrBool write_compile_tool_fixture_archive(
        const TZrChar *suiteName,
        EZrLibrary_ProviderPhase phase,
        const TZrChar *publicContractHash,
        const TZrChar *declaredModuleHash,
        TZrChar *outArchivePath,
        TZrSize outArchivePathSize,
        TZrChar *outActualModuleHash,
        TZrSize outActualModuleHashSize,
        TZrChar *outArchiveHash,
        TZrSize outArchiveHashSize) {
    return write_compile_tool_fixture_archive_named(
            suiteName,
            phase,
            publicContractHash,
            declaredModuleHash,
            "derive",
            "main",
            outArchivePath,
            outArchivePathSize,
            outActualModuleHash,
            outActualModuleHashSize,
            outArchiveHash,
            outArchiveHashSize);
}

static SZrLibrary_ProjectManifestDependencyLockEntry compile_tool_fixture_lock(
        const SZrLibrary_Project *project,
        const TZrChar *contentHash,
        const TZrChar *transitiveIdentity) {
    SZrLibrary_ProjectManifestDependencyLockEntry lock = {0};

    lock.packageIdentity = project->manifestBuildDependencies[0].packageIdentity;
    lock.resolvedVersion = "1.4.0";
    lock.contentHash = contentHash;
    lock.transitiveIdentity = transitiveIdentity;
    lock.providerSourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH;
    lock.providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;
    return lock;
}

static void test_compile_tool_artifact_resolution_hands_owned_identity_to_cache(void) {
    const SZrParserCompileToolModuleDescriptor *descriptor =
            ZrParser_CompileTool_FindModule(ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    SZrLibrary_Project *project = new_compile_tool_fixture_project();
    SZrLibrary_ProjectManifestDependencyLockEntry firstLock;
    SZrLibrary_ProjectManifestDependencyLockEntry secondLock;
    SZrLibrary_ProjectManifestDependencyLockEntry submoduleLock;
    SZrLibrary_ProjectManifestDependencyLockEntry helperLock;
    SZrLibrary_ProjectManifestDependencyLockEntry orderedLocks[2];
    SZrLibrary_ProjectManifestDependencyLockEntry reversedLocks[2];
    SZrLibrary_ProjectManifestDependencyLockEntry changedLocks[2];
    SZrLibrary_ProjectManifestDependencyLockEntry locksWithRuntime[3];
    SZrParserCompileToolResolvedArtifact firstArtifact = {0};
    SZrParserCompileToolResolvedArtifact projectOwnedArtifact = {0};
    SZrParserCompileToolResolvedArtifact secondArtifact = {0};
    SZrParserCompileToolResolvedArtifact submoduleArtifact = {0};
    SZrParserCompileToolResolvedArtifact orderedArtifact = {0};
    SZrParserCompileToolResolvedArtifact reversedArtifact = {0};
    SZrParserCompileToolResolvedArtifact changedArtifact = {0};
    SZrParserCompileToolResolvedArtifact runtimeIgnoredArtifact = {0};
    SZrLibrary_ProjectImportProviderLocation runtimeLocation;
    SZrCompileTimeFunction function = {0};
    SZrCompilerState compiler;
    SZrString *alias;
    TZrChar archivePath[ZR_TESTS_PATH_MAX];
    TZrChar submoduleArchivePath[ZR_TESTS_PATH_MAX];
    TZrChar moduleHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar archiveHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar submoduleArchiveHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar knownVectorHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar resolvedModuleKey[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];
    TZrChar projectLock[1024];
    int projectLockLength;
    SZrComptimeCacheKey firstKey;
    SZrComptimeCacheKey secondKey;

    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
            (const TZrByte *)"abc", 3U, knownVectorHash, sizeof(knownVectorHash)));
    TEST_ASSERT_EQUAL_STRING(
            "sha256:ungWv48Bz-pBQUDeXa4iI7ADYaOWF3qctBD_YfIAFa0",
            knownVectorHash);
    assert_compile_tool_sha256_boundaries();
    TEST_ASSERT_TRUE(write_compile_tool_fixture_archive(
            "resolved_identity",
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
            descriptor->publicContractHash,
            ZR_NULL,
            archivePath,
            sizeof(archivePath),
            moduleHash,
            sizeof(moduleHash),
            archiveHash,
            sizeof(archiveHash)));
    firstLock = compile_tool_fixture_lock(
            project, archiveHash, ZR_TEST_TRANSITIVE_IDENTITY_A);
    projectLockLength = snprintf(
            projectLock,
            sizeof(projectLock),
            "{\"lockVersion\":1,\"dependencies\":{\"@derive\":{"
            "\"version\":\"1.4.0\",\"contentHash\":\"%s\","
            "\"transitiveIdentity\":\"%s\",\"provider\":\"path\"}},"
            "\"buildDependencies\":{\"@derive\":{\"version\":\"1.4.0\","
            "\"contentHash\":\"%s\",\"transitiveIdentity\":\"%s\","
            "\"provider\":\"path\"}}}",
            archiveHash,
            ZR_TEST_TRANSITIVE_IDENTITY_A,
            archiveHash,
            ZR_TEST_TRANSITIVE_IDENTITY_A);
    TEST_ASSERT_TRUE(projectLockLength > 0);
    TEST_ASSERT_TRUE((TZrSize)projectLockLength < sizeof(projectLock));
    TEST_ASSERT_TRUE_MESSAGE(
            ZrLibrary_ProjectManifestV2_ReadDependencyLock(
                    g_state,
                    project,
                    projectLock,
                    error,
                    sizeof(error)),
            error);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_CompileToolArtifact_OpenProjectBuildDependency(
                    project,
                    "@derive",
                    archivePath,
                    &projectOwnedArtifact,
                    error,
                    sizeof(error)),
            error);
    TEST_ASSERT_EQUAL_STRING(
            archiveHash, projectOwnedArtifact.packageContentHash);
    secondLock = compile_tool_fixture_lock(
            project, archiveHash, ZR_TEST_TRANSITIVE_IDENTITY_B);
    helperLock = firstLock;
    strcpy(helperLock.packageIdentity.packageName, "helper");
    helperLock.resolvedVersion = "2.0.0";
    orderedLocks[0] = firstLock;
    orderedLocks[1] = helperLock;
    reversedLocks[0] = helperLock;
    reversedLocks[1] = firstLock;
    changedLocks[0] = firstLock;
    changedLocks[1] = helperLock;
    changedLocks[1].transitiveIdentity = ZR_TEST_TRANSITIVE_IDENTITY_B;
    locksWithRuntime[0] = helperLock;
    locksWithRuntime[0].providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME;
    locksWithRuntime[0].contentHash = "runtime hash is outside CompileTool graph";
    locksWithRuntime[0].transitiveIdentity = "runtime identity is ignored";
    locksWithRuntime[1] = firstLock;
    locksWithRuntime[2] = helperLock;
    TEST_ASSERT_TRUE(write_compile_tool_fixture_archive_named(
            "resolved_submodule",
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
            descriptor->publicContractHash,
            ZR_NULL,
            "derive",
            "tools/derive",
            submoduleArchivePath,
            sizeof(submoduleArchivePath),
            moduleHash,
            sizeof(moduleHash),
            submoduleArchiveHash,
            sizeof(submoduleArchiveHash)));
    submoduleLock = compile_tool_fixture_lock(
            project,
            submoduleArchiveHash,
            ZR_TEST_TRANSITIVE_IDENTITY_SUBMODULE);

    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_CompileToolArtifact_OpenBuildDependency(
                    project,
                    "@derive",
                    &firstLock,
                    1U,
                    archivePath,
                    &firstArtifact,
                    error,
                    sizeof(error)),
            error);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_CompileToolArtifact_OpenBuildDependency(
                    project, "@derive", orderedLocks, 2U, archivePath,
                    &orderedArtifact, error, sizeof(error)),
            error);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_CompileToolArtifact_OpenBuildDependency(
                    project, "@derive", reversedLocks, 2U, archivePath,
                    &reversedArtifact, error, sizeof(error)),
            error);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_CompileToolArtifact_OpenBuildDependency(
                    project, "@derive", changedLocks, 2U, archivePath,
                    &changedArtifact, error, sizeof(error)),
            error);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_CompileToolArtifact_OpenBuildDependency(
                    project, "@derive", locksWithRuntime, 3U, archivePath,
                    &runtimeIgnoredArtifact, error, sizeof(error)),
            error);
    TEST_ASSERT_EQUAL_STRING(
            orderedArtifact.lockGraphHash, reversedArtifact.lockGraphHash);
    TEST_ASSERT_TRUE(strcmp(
            orderedArtifact.lockGraphHash,
            changedArtifact.lockGraphHash) != 0);
    TEST_ASSERT_EQUAL_STRING(
            orderedArtifact.lockGraphHash,
            runtimeIgnoredArtifact.lockGraphHash);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_CompileToolArtifact_OpenBuildDependency(
                    project,
                    "@derive.tools.derive",
                    &submoduleLock,
                    1U,
                    submoduleArchivePath,
                    &submoduleArtifact,
                    error,
                    sizeof(error)),
            error);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_CompileToolArtifact_OpenBuildDependency(
                    project,
                    "@derive",
                    &secondLock,
                    1U,
                    archivePath,
                    &secondArtifact,
                    error,
                    sizeof(error)),
            error);
    TEST_ASSERT_EQUAL_INT(
            ZR_LIBRARY_MODULE_DOMAIN_PACKAGE,
            firstArtifact.moduleIdentity.domain);
    TEST_ASSERT_EQUAL_STRING("derive", firstArtifact.moduleIdentity.packageName);
    TEST_ASSERT_EQUAL_STRING("1.4.0", firstArtifact.resolvedVersion);
    TEST_ASSERT_EQUAL_STRING(archiveHash, firstArtifact.packageContentHash);
    TEST_ASSERT_TRUE(firstArtifact.lockGraphHash[0] != '\0');
    TEST_ASSERT_TRUE(strcmp(
            firstArtifact.lockGraphHash, secondArtifact.lockGraphHash) != 0);
    TEST_ASSERT_EQUAL_STRING(moduleHash, firstArtifact.artifactContentHash);
    TEST_ASSERT_EQUAL_STRING("modules/main.zro", firstArtifact.artifactEntry);
    TEST_ASSERT_EQUAL_STRING(
            descriptor->publicContractHash,
            firstArtifact.publicContractHash);
    TEST_ASSERT_NOT_NULL(firstArtifact.artifactBytes);
    TEST_ASSERT_EQUAL_UINT32(19U, (TZrUInt32)firstArtifact.artifactByteCount);
    TEST_ASSERT_EQUAL_STRING("tools.derive", submoduleArtifact.moduleIdentity.segments);
    TEST_ASSERT_EQUAL_STRING(
            "modules/tools/derive.zro", submoduleArtifact.artifactEntry);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)project->dependencyPackageCount);

    memset(&runtimeLocation, 0, sizeof(runtimeLocation));
    TEST_ASSERT_FALSE(ZrLibrary_Project_ResolveImportProviderLocation(
            project,
            "main",
            "@derive",
            resolvedModuleKey,
            sizeof(resolvedModuleKey),
            &runtimeLocation,
            error,
            sizeof(error)));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)project->dependencyPackageCount);

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentModuleKey = ZrCore_String_CreateFromNative(
            g_state, "consumer/main");
    function.name = ZrCore_String_CreateFromNative(g_state, "generatedValue");
    alias = ZrCore_String_CreateFromNative(g_state, "derive");
    TEST_ASSERT_TRUE(ZrParser_CompileToolBinding_DeclareResolvedProvider(
            &compiler, alias, descriptor, &firstArtifact));
    TEST_ASSERT_TRUE(ZrParser_ComptimeCache_BeginKey(
            &compiler, &function, &firstKey));
    ZrParser_CompileToolBinding_Reset(&compiler);
    TEST_ASSERT_TRUE(ZrParser_CompileToolBinding_DeclareResolvedProvider(
            &compiler, alias, descriptor, &secondArtifact));
    TEST_ASSERT_TRUE(ZrParser_ComptimeCache_BeginKey(
            &compiler, &function, &secondKey));
    TEST_ASSERT_FALSE(ZrParser_ComptimeCache_KeyEquals(
            &firstKey, &secondKey));

    ZrParser_CompileToolBinding_Reset(&compiler);
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_CompileToolArtifact_Close(&runtimeIgnoredArtifact);
    ZrParser_CompileToolArtifact_Close(&changedArtifact);
    ZrParser_CompileToolArtifact_Close(&reversedArtifact);
    ZrParser_CompileToolArtifact_Close(&orderedArtifact);
    ZrParser_CompileToolArtifact_Close(&submoduleArtifact);
    ZrParser_CompileToolArtifact_Close(&secondArtifact);
    ZrParser_CompileToolArtifact_Close(&firstArtifact);
    ZrParser_CompileToolArtifact_Close(&projectOwnedArtifact);
    TEST_ASSERT_NULL(firstArtifact.artifactBytes);
    TEST_ASSERT_EQUAL_UINT64(0U, firstArtifact.signature);
    ZrLibrary_Project_Free(g_state, project);
}

static void test_compile_tool_artifact_resolution_rejects_untrusted_or_wrong_phase_inputs(void) {
    const SZrParserCompileToolModuleDescriptor *descriptor =
            ZrParser_CompileTool_FindModule(ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    SZrParserCompileToolModuleDescriptor mismatchedDescriptor;
    SZrLibrary_Project *project = new_compile_tool_fixture_project();
    SZrLibrary_Project *staleRequirementProject =
            new_compile_tool_fixture_project_with_requirement("^2.0.0");
    SZrLibrary_ProjectManifestDependencyLockEntry lock;
    SZrLibrary_ProjectManifestDependencyLockEntry duplicateLocks[2];
    SZrParserCompileToolResolvedArtifact artifact = {0};
    SZrCompilerState compiler;
    TZrChar compileArchivePath[ZR_TESTS_PATH_MAX];
    TZrChar runtimeArchivePath[ZR_TESTS_PATH_MAX];
    TZrChar staleEntryArchivePath[ZR_TESTS_PATH_MAX];
    TZrChar wrongIdentityArchivePath[ZR_TESTS_PATH_MAX];
    TZrChar moduleHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar archiveHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar unusedHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];
    SZrString *alias;

    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_NOT_NULL(staleRequirementProject);
    TEST_ASSERT_TRUE(write_compile_tool_fixture_archive(
            "reject_compile",
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
            descriptor->publicContractHash,
            ZR_NULL,
            compileArchivePath,
            sizeof(compileArchivePath),
            moduleHash,
            sizeof(moduleHash),
            archiveHash,
            sizeof(archiveHash)));
    lock = compile_tool_fixture_lock(
            project, archiveHash, ZR_TEST_TRANSITIVE_IDENTITY_A);

    memset(&artifact, 0xA5, sizeof(artifact));
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@missing", &lock, 1U, compileArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "build_dependency"));
    TEST_ASSERT_EQUAL_UINT64(0U, artifact.signature);

    memset(&artifact, 0xA5, sizeof(artifact));
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            ZR_NULL, "@derive", &lock, 1U, compileArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_EQUAL_UINT64(0U, artifact.signature);

    lock = compile_tool_fixture_lock(
            staleRequirementProject,
            archiveHash,
            ZR_TEST_TRANSITIVE_IDENTITY_A);
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            staleRequirementProject, "@derive", &lock, 1U, compileArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "version requirement"));
    lock = compile_tool_fixture_lock(
            project, archiveHash, ZR_TEST_TRANSITIVE_IDENTITY_A);

    lock.providerSourceKind =
            ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT;
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@derive", &lock, 1U, compileArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "lock_contract"));
    lock.providerSourceKind =
            ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH;

    lock.transitiveIdentity = "derive@1.4.0/untrusted-label";
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@derive", &lock, 1U, compileArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "lock_graph"));
    lock.transitiveIdentity =
            "sha256:uZUQq8UmbWYobrUOMIYjSt_JG2D7HnXK8ukUtWWZfpB";
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@derive", &lock, 1U, compileArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "lock_graph"));
    lock.transitiveIdentity = ZR_TEST_TRANSITIVE_IDENTITY_A;

    lock.resolvedVersion = "1.5.0";
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@derive", &lock, 1U, compileArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "version mismatch"));
    lock.resolvedVersion = "1.4.0";

    duplicateLocks[0] = lock;
    duplicateLocks[1] = lock;
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@derive", duplicateLocks, 2U, compileArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "lock_duplicate"));

    TEST_ASSERT_TRUE(write_compile_tool_fixture_archive_named(
            "reject_package_identity",
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
            descriptor->publicContractHash,
            ZR_NULL,
            "not-derive",
            "main",
            wrongIdentityArchivePath,
            sizeof(wrongIdentityArchivePath),
            moduleHash,
            sizeof(moduleHash),
            unusedHash,
            sizeof(unusedHash)));
    lock.contentHash = unusedHash;
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@derive", &lock, 1U, wrongIdentityArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "package identity"));
    lock.contentHash = archiveHash;

    lock.providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME;
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@derive", &lock, 1U, compileArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "lock phase"));
    lock.providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;

    lock.contentHash = "0000000000000000";
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@derive", &lock, 1U, compileArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "lock_graph"));
    lock.contentHash = archiveHash;

    TEST_ASSERT_TRUE(write_compile_tool_fixture_archive(
            "reject_runtime",
            ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
            descriptor->publicContractHash,
            ZR_NULL,
            runtimeArchivePath,
            sizeof(runtimeArchivePath),
            moduleHash,
            sizeof(moduleHash),
            unusedHash,
            sizeof(unusedHash)));
    lock.contentHash = unusedHash;
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@derive", &lock, 1U, runtimeArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "provider phase"));

    TEST_ASSERT_TRUE(write_compile_tool_fixture_archive(
            "reject_entry_hash",
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
            descriptor->publicContractHash,
            "0000000000000000",
            staleEntryArchivePath,
            sizeof(staleEntryArchivePath),
            moduleHash,
            sizeof(moduleHash),
            unusedHash,
            sizeof(unusedHash)));
    lock.contentHash = unusedHash;
    TEST_ASSERT_FALSE(ZrParser_CompileToolArtifact_OpenBuildDependency(
            project, "@derive", &lock, 1U, staleEntryArchivePath,
            &artifact, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "artifact content hash"));

    lock.contentHash = archiveHash;
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_CompileToolArtifact_OpenBuildDependency(
                    project, "@derive", &lock, 1U, compileArchivePath,
                    &artifact, error, sizeof(error)),
            error);
    mismatchedDescriptor = *descriptor;
    mismatchedDescriptor.publicContractHash = "fnv1a64:wrong";
    ZrParser_CompilerState_Init(&compiler, g_state);
    alias = ZrCore_String_CreateFromNative(g_state, "derive");
    TEST_ASSERT_FALSE(ZrParser_CompileToolBinding_DeclareResolvedProvider(
            &compiler, alias, &mismatchedDescriptor, &artifact));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)compiler.compileToolBindings.length);
    ZrParser_CompilerState_Free(&compiler);

    ZrParser_CompileToolArtifact_Close(&artifact);
    ZrLibrary_Project_Free(g_state, staleRequirementProject);
    ZrLibrary_Project_Free(g_state, project);
}

#endif // ZR_VM_TEST_COMPILE_TOOL_ARTIFACT_RESOLUTION_CASES_H
