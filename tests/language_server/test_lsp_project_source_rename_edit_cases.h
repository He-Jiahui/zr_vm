#ifndef ZR_VM_TEST_LSP_PROJECT_SOURCE_RENAME_EDIT_CASES_H
#define ZR_VM_TEST_LSP_PROJECT_SOURCE_RENAME_EDIT_CASES_H

static void test_free_source_rename_locations(SZrState *state,
                                              SZrArray *locations) {
    if (state == ZR_NULL || locations == ZR_NULL || !locations->isValid) {
        return;
    }

    for (TZrSize index = 0U; index < locations->length; index++) {
        SZrLspLocation **locationPtr =
                (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(
                    state->global, *locationPtr, sizeof(SZrLspLocation));
        }
    }
    ZrCore_Array_Free(state, locations);
}

static void test_lsp_source_rename_collects_canonical_workspace_edits(
        SZrState *state) {
    static const TZrChar *projectContent =
            "{\n"
            "  \"name\": \"source_rename_workspace_edits\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}\n";
    static const TZrChar *mainContent =
            "var legacy = %import(\"legacy\");\n"
            "return legacy.value();\n";
    static const TZrChar *secondaryContent =
            "var dep = %import(\"legacy\");\n"
            "return dep.value();\n";
    static const TZrChar *providerContent =
            "%module \"legacy\";\n"
            "pub value(): int {\n"
            "    return 1;\n"
            "}\n";
    const TZrChar *summary =
            "LSP Source Rename Collects Canonical Workspace Edits";
    SZrTestTimer timer;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRootPath[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar secondaryPath[ZR_TESTS_PATH_MAX];
    TZrChar providerPath[ZR_TESTS_PATH_MAX];
    TZrChar renamedProviderPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;
    SZrLspContext *context = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *secondaryUri = ZR_NULL;
    SZrString *providerUri = ZR_NULL;
    SZrString *renamedProviderUri = ZR_NULL;
    SZrString *newModuleName = ZR_NULL;
    SZrArray locations = {0};
    SZrLspPosition mainImportStart;
    SZrLspPosition secondaryImportStart;
    SZrLspPosition providerModuleStart;
    TZrBool collected;
    TZrBool success = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "Canonical source rename edits",
            "willRenameFiles should edit the provider module declaration and opened or unopened import specifiers from canonical AST bindings");

    if (!ZrTests_Path_GetGeneratedArtifact(
                "language_server",
                "project_features_source_rename_workspace_edits",
                "source_rename_workspace_edits",
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
    ZrLibrary_File_PathJoin(sourceRootPath, "main.zr", mainPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "secondary.zr", secondaryPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "legacy.zr", providerPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "modern.zr", renamedProviderPath);

    if (!write_text_file(projectPath, projectContent, strlen(projectContent)) ||
        !write_text_file(mainPath, mainContent, strlen(mainContent)) ||
        !write_text_file(
                secondaryPath, secondaryContent, strlen(secondaryContent)) ||
        !write_text_file(providerPath, providerContent, strlen(providerContent))) {
        TEST_FAIL(timer, summary, "Failed to prepare the source rename fixture");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    mainUri = create_file_uri_from_native_path(state, mainPath);
    secondaryUri = create_file_uri_from_native_path(state, secondaryPath);
    providerUri = create_file_uri_from_native_path(state, providerPath);
    renamedProviderUri = create_file_uri_from_native_path(
            state, renamedProviderPath);
    if (context == ZR_NULL || mainUri == ZR_NULL || secondaryUri == ZR_NULL ||
        providerUri == ZR_NULL || renamedProviderUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                mainUri,
                mainContent,
                strlen(mainContent),
                1U) ||
        !lsp_find_position_for_substring(
                mainContent, "legacy", 1U, 0, &mainImportStart) ||
        !lsp_find_position_for_substring(
                secondaryContent, "legacy", 0U, 0, &secondaryImportStart) ||
        !lsp_find_position_for_substring(
                providerContent, "legacy", 0U, 0, &providerModuleStart)) {
        TEST_FAIL(timer, summary, "Failed to load the source rename project");
        goto cleanup;
    }

    collected = ZrLanguageServer_LspProject_CollectSourceRenameEdits(
            state,
            context,
            providerUri,
            renamedProviderUri,
            &newModuleName,
            &locations);

    if (!collected ||
        newModuleName == ZR_NULL ||
        strcmp(test_string_ptr(newModuleName), "modern") != 0 ||
        locations.length != 3U ||
        !location_array_contains_uri_and_range(
                &locations,
                mainUri,
                mainImportStart.line,
                mainImportStart.character,
                mainImportStart.line,
                mainImportStart.character + 6) ||
        !location_array_contains_uri_and_range(
                &locations,
                secondaryUri,
                secondaryImportStart.line,
                secondaryImportStart.character,
                secondaryImportStart.line,
                secondaryImportStart.character + 6) ||
        !location_array_contains_uri_and_range(
                &locations,
                providerUri,
                providerModuleStart.line,
                providerModuleStart.character,
                providerModuleStart.line,
                providerModuleStart.character + 6)) {
        TEST_FAIL(timer,
                  summary,
                  "Canonical rename edits did not cover the declaration and both import specifiers exactly once");
        goto cleanup;
    }

    test_free_source_rename_locations(state, &locations);
    ZrCore_Array_Construct(&locations);
    newModuleName = ZR_NULL;
    if (ZrLanguageServer_LspProject_CollectSourceRenameEdits(
                state,
                context,
                providerUri,
                providerUri,
                &newModuleName,
                &locations) ||
        newModuleName != ZR_NULL || locations.length != 0U) {
        TEST_FAIL(timer, summary, "A same-URI rename should not emit workspace edits");
        goto cleanup;
    }

    success = ZR_TRUE;

cleanup:
    test_free_source_rename_locations(state, &locations);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (success) {
        TEST_PASS(timer, summary);
    }
}

#endif
