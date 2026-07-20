#ifndef ZR_VM_TEST_LSP_PROJECT_PUBLIC_CONTRACT_CASES_H
#define ZR_VM_TEST_LSP_PROJECT_PUBLIC_CONTRACT_CASES_H

static void test_lsp_source_module_refresh_uses_canonical_public_contract_hash(
        SZrState *state) {
    static const TZrChar *initialModuleContent =
        "pub answer(): int {\n"
        "    return 1;\n"
        "}\n"
        "pub inferred() {\n"
        "    return 1;\n"
        "}\n"
        "pub named(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "pub var publicSeed: int = 7;\n"
        "var hidden: int = 1;\n";
    static const TZrChar *privateTypeUpdatedModuleContent =
        "pub answer(): int {\n"
        "    return 1;\n"
        "}\n"
        "pub inferred() {\n"
        "    return 1;\n"
        "}\n"
        "pub named(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "pub var publicSeed: int = 7;\n"
        "var hidden: float = 1.5;\n";
    static const TZrChar *parameterNameUpdatedModuleContent =
        "pub answer(): int {\n"
        "    return 1;\n"
        "}\n"
        "pub inferred() {\n"
        "    return 1;\n"
        "}\n"
        "pub named(item: int): int {\n"
        "    return item;\n"
        "}\n"
        "pub var publicSeed: int = 7;\n"
        "var hidden: float = 1.5;\n";
    static const TZrChar *publicSignatureUpdatedModuleContent =
        "pub answer(): float {\n"
        "    return 1.5;\n"
        "}\n"
        "pub inferred() {\n"
        "    return 1;\n"
        "}\n"
        "pub named(item: int): int {\n"
        "    return item;\n"
        "}\n"
        "pub var publicSeed: int = 7;\n"
        "var hidden: float = 1.5;\n";
    static const TZrChar *unsupportedPublicTypeModuleContent =
        "pub answer(): float {\n"
        "    return 1.5;\n"
        "}\n"
        "pub inferred() {\n"
        "    return 1;\n"
        "}\n"
        "pub named(item: int): int {\n"
        "    return item;\n"
        "}\n"
        "pub var publicSeed: int = 7;\n"
        "var hidden: float = 1.5;\n"
        "pub struct Exposed {\n"
        "    var value: int;\n"
        "}\n";
    const TZrChar *summary =
            "LSP Source Module Refresh Uses Canonical Public Contract Hash";
    SZrTestTimer timer;
    SZrGeneratedSourceMemberRefreshFixture fixture;
    SZrLspContext *context = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *moduleUri = ZR_NULL;
    SZrSemanticAnalyzer *mainAnalyzer;
    SZrSemanticAnalyzer *mainAnalyzerAfterRefresh;
    SZrSemanticAnalysisMetrics initialMetrics;
    SZrSemanticAnalysisMetrics refreshedMetrics;
    SZrLspProjectIndex *projectIndex;
    SZrLspProjectFileRecord *moduleRecord;
    TZrChar *mainContent = ZR_NULL;
    TZrSize mainLength = 0U;
    TZrSize initialPreservationCount;
    TZrSize initialReanalysisCount;
    TZrSize initialHashMatchCount;
    TZrSize initialHashChangeCount;
    TZrSize initialHashUnavailableCount;
    TZrUInt64 initialHash;
    TZrUInt64 parameterNameHash;
    TZrBool success = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "Canonical public contract invalidation",
            "Private declaration changes preserve importers, public signature changes invalidate them, and unsupported public types remain conservative");

    if (!prepare_generated_source_member_refresh_fixture(
                "project_features_public_contract_hash", &fixture) ||
        !write_text_file(
                fixture.modulePath,
                initialModuleContent,
                strlen(initialModuleContent))) {
        TEST_FAIL(timer, summary, "Failed to prepare the public contract fixture");
        return;
    }
    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    moduleUri = create_file_uri_from_native_path(state, fixture.modulePath);
    if (mainContent == ZR_NULL || context == ZR_NULL ||
        mainUri == ZR_NULL || moduleUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, mainUri, mainContent, mainLength, 1U)) {
        TEST_FAIL(timer, summary, "Failed to load the public contract project");
        goto cleanup;
    }

    mainAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, mainUri);
    projectIndex = test_find_project_for_uri(context, mainUri);
    moduleRecord = projectIndex != ZR_NULL
                           ? test_find_project_record_by_uri(projectIndex, moduleUri)
                           : ZR_NULL;
    if (mainAnalyzer == ZR_NULL || projectIndex == ZR_NULL || moduleRecord == ZR_NULL ||
        !moduleRecord->hasPublicContractHash || moduleRecord->publicContractExportCount != 4U) {
        TEST_FAIL(timer, summary, "Initial source record did not publish a canonical hash");
        goto cleanup;
    }
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(mainAnalyzer, &initialMetrics);
    initialHash = moduleRecord->publicContractHash;
    initialPreservationCount = projectIndex->reverseDependencyPreservationCount;
    initialReanalysisCount = projectIndex->reverseDependencyReanalysisCount;
    initialHashMatchCount = projectIndex->publicContractHashMatchCount;
    initialHashChangeCount = projectIndex->publicContractHashChangeCount;
    initialHashUnavailableCount = projectIndex->publicContractHashUnavailableCount;

    if (!write_text_file(
                fixture.modulePath,
                privateTypeUpdatedModuleContent,
                strlen(privateTypeUpdatedModuleContent)) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                moduleUri,
                privateTypeUpdatedModuleContent,
                strlen(privateTypeUpdatedModuleContent),
                2U)) {
        TEST_FAIL(timer, summary, "Failed to apply the private declaration update");
        goto cleanup;
    }
    mainAnalyzerAfterRefresh = ZrLanguageServer_Lsp_FindAnalyzer(state, context, mainUri);
    moduleRecord = test_find_project_record_by_uri(projectIndex, moduleUri);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(mainAnalyzerAfterRefresh, &refreshedMetrics);
    if (mainAnalyzerAfterRefresh != mainAnalyzer || moduleRecord == ZR_NULL ||
        !moduleRecord->hasPublicContractHash ||
        moduleRecord->publicContractHash != initialHash ||
        moduleRecord->publicContractExportCount != 4U ||
        refreshedMetrics.executionCount != initialMetrics.executionCount ||
        projectIndex->reverseDependencyPreservationCount != initialPreservationCount + 1U ||
        projectIndex->reverseDependencyReanalysisCount != initialReanalysisCount ||
        projectIndex->lastReverseDependencyReanalysisCount != 0U ||
        projectIndex->publicContractHashMatchCount != initialHashMatchCount + 1U ||
        projectIndex->publicContractHashChangeCount != initialHashChangeCount ||
        projectIndex->publicContractHashUnavailableCount != initialHashUnavailableCount) {
        TEST_FAIL(timer, summary, "A private type change did not preserve the importer by hash");
        goto cleanup;
    }

    if (!write_text_file(
                fixture.modulePath,
                parameterNameUpdatedModuleContent,
                strlen(parameterNameUpdatedModuleContent)) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                moduleUri,
                parameterNameUpdatedModuleContent,
                strlen(parameterNameUpdatedModuleContent),
                3U)) {
        TEST_FAIL(timer, summary, "Failed to apply the public parameter-name update");
        goto cleanup;
    }
    mainAnalyzerAfterRefresh = ZrLanguageServer_Lsp_FindAnalyzer(state, context, mainUri);
    moduleRecord = test_find_project_record_by_uri(projectIndex, moduleUri);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(mainAnalyzerAfterRefresh, &refreshedMetrics);
    if (mainAnalyzerAfterRefresh != mainAnalyzer || moduleRecord == ZR_NULL ||
        !moduleRecord->hasPublicContractHash ||
        moduleRecord->publicContractHash == initialHash ||
        moduleRecord->publicContractExportCount != 4U ||
        refreshedMetrics.executionCount != initialMetrics.executionCount + 1U ||
        projectIndex->reverseDependencyReanalysisCount != initialReanalysisCount + 1U ||
        projectIndex->lastReverseDependencyReanalysisCount != 1U ||
        projectIndex->publicContractHashMatchCount != initialHashMatchCount + 1U ||
        projectIndex->publicContractHashChangeCount != initialHashChangeCount + 1U ||
        projectIndex->publicContractHashUnavailableCount != initialHashUnavailableCount) {
        TEST_FAIL(timer, summary, "A public parameter-name change did not invalidate the importer");
        goto cleanup;
    }
    parameterNameHash = moduleRecord->publicContractHash;

    if (!write_text_file(
                fixture.modulePath,
                publicSignatureUpdatedModuleContent,
                strlen(publicSignatureUpdatedModuleContent)) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                moduleUri,
                publicSignatureUpdatedModuleContent,
                strlen(publicSignatureUpdatedModuleContent),
                4U)) {
        TEST_FAIL(timer, summary, "Failed to apply the public signature update");
        goto cleanup;
    }
    mainAnalyzerAfterRefresh = ZrLanguageServer_Lsp_FindAnalyzer(state, context, mainUri);
    moduleRecord = test_find_project_record_by_uri(projectIndex, moduleUri);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(mainAnalyzerAfterRefresh, &refreshedMetrics);
    if (mainAnalyzerAfterRefresh != mainAnalyzer || moduleRecord == ZR_NULL ||
        !moduleRecord->hasPublicContractHash ||
        moduleRecord->publicContractHash == parameterNameHash ||
        refreshedMetrics.executionCount != initialMetrics.executionCount + 2U ||
        projectIndex->reverseDependencyReanalysisCount != initialReanalysisCount + 2U ||
        projectIndex->lastReverseDependencyReanalysisCount != 1U ||
        projectIndex->publicContractHashMatchCount != initialHashMatchCount + 1U ||
        projectIndex->publicContractHashChangeCount != initialHashChangeCount + 2U ||
        projectIndex->publicContractHashUnavailableCount != initialHashUnavailableCount) {
        TEST_FAIL(timer, summary, "A public signature change did not invalidate the importer");
        goto cleanup;
    }

    if (!write_text_file(
                fixture.modulePath,
                unsupportedPublicTypeModuleContent,
                strlen(unsupportedPublicTypeModuleContent)) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                moduleUri,
                unsupportedPublicTypeModuleContent,
                strlen(unsupportedPublicTypeModuleContent),
                5U)) {
        TEST_FAIL(timer, summary, "Failed to apply the unsupported public type update");
        goto cleanup;
    }
    mainAnalyzerAfterRefresh = ZrLanguageServer_Lsp_FindAnalyzer(state, context, mainUri);
    moduleRecord = test_find_project_record_by_uri(projectIndex, moduleUri);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(mainAnalyzerAfterRefresh, &refreshedMetrics);
    if (mainAnalyzerAfterRefresh != mainAnalyzer || moduleRecord == ZR_NULL ||
        moduleRecord->hasPublicContractHash || moduleRecord->publicContractHash != 0U ||
        moduleRecord->publicContractExportCount != 0U ||
        refreshedMetrics.executionCount != initialMetrics.executionCount + 3U ||
        projectIndex->reverseDependencyReanalysisCount != initialReanalysisCount + 3U ||
        projectIndex->lastReverseDependencyReanalysisCount != 1U ||
        projectIndex->publicContractHashMatchCount != initialHashMatchCount + 1U ||
        projectIndex->publicContractHashChangeCount != initialHashChangeCount + 2U ||
        projectIndex->publicContractHashUnavailableCount != initialHashUnavailableCount + 1U) {
        TEST_FAIL(timer, summary, "An unsupported public type did not use conservative invalidation");
        goto cleanup;
    }

    success = ZR_TRUE;

cleanup:
    free(mainContent);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (success) {
        TEST_PASS(timer, summary);
    }
}

#endif
