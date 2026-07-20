#ifndef ZR_VM_TEST_LSP_PROJECT_MODULE_IDENTITY_EDGE_CASES_H
#define ZR_VM_TEST_LSP_PROJECT_MODULE_IDENTITY_EDGE_CASES_H

static void test_lsp_source_module_identity_change_refreshes_old_and_new_importers(
        SZrState *state) {
    static const TZrChar *projectContent =
        "{\n"
        "  \"name\": \"module_identity_edge_migration\",\n"
        "  \"source\": \"src\",\n"
        "  \"binary\": \"bin\",\n"
        "  \"entry\": \"old_user\"\n"
        "}\n";
    static const TZrChar *oldUserContent =
        "var legacy = %import(\"legacy\");\n"
        "var cached = legacy.value();\n"
        "return cached;\n";
    static const TZrChar *newUserContent =
        "var legacy = %import(\"legacy\");\n"
        "var modern = %import(\"modern\");\n"
        "var prior = legacy.value();\n"
        "var cached = modern.value();\n"
        "return cached;\n";
    static const TZrChar *initialProviderContent =
        "%module \"legacy\";\n"
        "pub value(): int {\n"
        "    return 1;\n"
        "}\n";
    static const TZrChar *renamedProviderContent =
        "%module \"modern\";\n"
        "pub value(): float {\n"
        "    return 1.5;\n"
        "}\n";
    const TZrChar *summary =
            "LSP Source Module Identity Change Refreshes Old And New Importers";
    SZrTestTimer timer;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRootPath[ZR_TESTS_PATH_MAX];
    TZrChar oldUserPath[ZR_TESTS_PATH_MAX];
    TZrChar newUserPath[ZR_TESTS_PATH_MAX];
    TZrChar providerPath[ZR_TESTS_PATH_MAX];
    TZrChar renamedProviderPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;
    SZrLspContext *context = ZR_NULL;
    SZrString *oldUserUri = ZR_NULL;
    SZrString *newUserUri = ZR_NULL;
    SZrString *providerUri = ZR_NULL;
    SZrString *renamedProviderUri = ZR_NULL;
    SZrSemanticAnalyzer *oldUserAnalyzer;
    SZrSemanticAnalyzer *newUserAnalyzer;
    SZrSemanticAnalyzer *oldUserAnalyzerAfterRefresh;
    SZrSemanticAnalyzer *newUserAnalyzerAfterRefresh;
    SZrSemanticAnalysisMetrics oldUserInitialMetrics;
    SZrSemanticAnalysisMetrics newUserInitialMetrics;
    SZrSemanticAnalysisMetrics oldUserRefreshedMetrics;
    SZrSemanticAnalysisMetrics newUserRefreshedMetrics;
    SZrLspProjectIndex *projectIndex;
    SZrLspProjectFileRecord *providerRecord;
    SZrLspPosition cachedHoverPosition;
    SZrLspHover *hover = ZR_NULL;
    TZrSize initialReanalysisCount;
    TZrBool success = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "ModuleIdentity edge migration",
            "Changing an explicit source module identity should refresh importers attached to both the removed and added reverse dependency edges");

    if (!ZrTests_Path_GetGeneratedArtifact(
                "language_server",
                "project_features_module_identity_edge_migration",
                "module_identity_edge_migration",
                ".zrp",
                projectPath,
                sizeof(projectPath))) {
        TEST_FAIL(timer, summary, "Failed to allocate the generated project path");
        return;
    }

    snprintf(rootPath, sizeof(rootPath), "%s", projectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        TEST_FAIL(timer, summary, "Failed to derive the generated project root");
        return;
    }
    *lastSeparator = '\0';
    ZrLibrary_File_PathJoin(rootPath, "src", sourceRootPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "old_user.zr", oldUserPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "new_user.zr", newUserPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "legacy.zr", providerPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "modern.zr", renamedProviderPath);

    errno = 0;
    if (remove(renamedProviderPath) != 0 && errno != ENOENT) {
        TEST_FAIL(timer, summary, "Failed to remove a stale renamed provider fixture");
        return;
    }

    if (!write_text_file(projectPath, projectContent, strlen(projectContent)) ||
        !write_text_file(oldUserPath, oldUserContent, strlen(oldUserContent)) ||
        !write_text_file(newUserPath, newUserContent, strlen(newUserContent)) ||
        !write_text_file(providerPath, initialProviderContent, strlen(initialProviderContent))) {
        TEST_FAIL(timer, summary, "Failed to prepare the generated module identity fixture");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    oldUserUri = create_file_uri_from_native_path(state, oldUserPath);
    newUserUri = create_file_uri_from_native_path(state, newUserPath);
    providerUri = create_file_uri_from_native_path(state, providerPath);
    renamedProviderUri = create_file_uri_from_native_path(state, renamedProviderPath);
    if (context == ZR_NULL || oldUserUri == ZR_NULL || newUserUri == ZR_NULL ||
        providerUri == ZR_NULL || renamedProviderUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                oldUserUri,
                oldUserContent,
                strlen(oldUserContent),
                1U) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                newUserUri,
                newUserContent,
                strlen(newUserContent),
                1U) ||
        !lsp_find_position_for_substring(
                newUserContent, "cached", 1U, 0, &cachedHoverPosition)) {
        TEST_FAIL(timer, summary, "Failed to load both importer documents");
        goto cleanup;
    }

    oldUserAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, oldUserUri);
    newUserAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, newUserUri);
    projectIndex = test_find_project_for_uri(context, providerUri);
    providerRecord = projectIndex != ZR_NULL
                             ? test_find_project_record_by_uri(projectIndex, providerUri)
                             : ZR_NULL;
    if (oldUserAnalyzer == ZR_NULL || newUserAnalyzer == ZR_NULL ||
        projectIndex == ZR_NULL || providerRecord == ZR_NULL ||
        strcmp(test_string_ptr(providerRecord->moduleName), "legacy") != 0) {
        TEST_FAIL(timer, summary, "Initial canonical module identity was not registered");
        goto cleanup;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(oldUserAnalyzer, &oldUserInitialMetrics);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(newUserAnalyzer, &newUserInitialMetrics);
    initialReanalysisCount = projectIndex->reverseDependencyReanalysisCount;

    if (rename(providerPath, renamedProviderPath) != 0 ||
        !write_text_file(
                renamedProviderPath,
                renamedProviderContent,
                strlen(renamedProviderContent)) ||
        !ZrLanguageServer_LspProject_PrepareSourceRename(
                state, context, providerUri, renamedProviderUri) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                renamedProviderUri,
                renamedProviderContent,
                strlen(renamedProviderContent),
                0U)) {
        TEST_FAIL(timer, summary, "Failed to apply the explicit module identity update");
        goto cleanup;
    }

    oldUserAnalyzerAfterRefresh =
            ZrLanguageServer_Lsp_FindAnalyzer(state, context, oldUserUri);
    newUserAnalyzerAfterRefresh =
            ZrLanguageServer_Lsp_FindAnalyzer(state, context, newUserUri);
    providerRecord = test_find_project_record_by_uri(projectIndex, renamedProviderUri);
    if (oldUserAnalyzerAfterRefresh == ZR_NULL || newUserAnalyzerAfterRefresh == ZR_NULL ||
        providerRecord == ZR_NULL ||
        test_find_project_record_by_uri(projectIndex, providerUri) != ZR_NULL ||
        strcmp(test_string_ptr(providerRecord->moduleName), "modern") != 0) {
        TEST_FAIL(timer, summary, "Updated canonical module identity was not registered");
        goto cleanup;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(
            oldUserAnalyzerAfterRefresh, &oldUserRefreshedMetrics);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(
            newUserAnalyzerAfterRefresh, &newUserRefreshedMetrics);
    if (oldUserAnalyzerAfterRefresh != oldUserAnalyzer ||
        newUserAnalyzerAfterRefresh != newUserAnalyzer ||
        oldUserRefreshedMetrics.executionCount != oldUserInitialMetrics.executionCount + 1U ||
        newUserRefreshedMetrics.executionCount != newUserInitialMetrics.executionCount + 1U ||
        projectIndex->reverseDependencyReanalysisCount != initialReanalysisCount + 2U ||
        projectIndex->lastReverseDependencyReanalysisCount != 2U) {
        TEST_FAIL(timer, summary, "ModuleIdentity edge migration did not refresh both importers exactly once");
        goto cleanup;
    }

    if (!ZrLanguageServer_Lsp_GetHover(
                state, context, newUserUri, cachedHoverPosition, &hover) ||
        hover == ZR_NULL || !hover_contains_text(hover, "cached") ||
        !hover_contains_text(hover, "float")) {
        TEST_FAIL(timer, summary, "The added ModuleIdentity edge retained stale semantic facts");
        goto cleanup;
    }

    success = ZR_TRUE;

cleanup:
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (success) {
        TEST_PASS(timer, summary);
    }
}

#endif
