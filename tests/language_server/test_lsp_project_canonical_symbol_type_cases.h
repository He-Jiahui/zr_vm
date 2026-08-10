static TZrBool prepare_generated_project_canonical_symbol_type_fixture(
        const TZrChar *artifactName,
        SZrGeneratedSourceMemberRefreshFixture *fixture) {
    static const TZrChar *projectContent =
        "{\n"
        "  \"name\": \"canonical_symbol_type\",\n"
        "  \"source\": \"src\",\n"
        "  \"binary\": \"bin\",\n"
        "  \"entry\": \"main\"\n"
        "}\n";
    static const TZrChar *mainContent =
        "module main;\n"
        "var values = import(\"values\");\n"
        "return values.seed;\n";
    static const TZrChar *moduleContent =
        "module values;\n"
        "pub var seed: int = 7;\n";
    TZrChar generatedProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRootPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (artifactName == ZR_NULL || fixture == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("language_server",
                                           artifactName,
                                           "canonical_symbol_type",
                                           ".zrp",
                                           generatedProjectPath,
                                           sizeof(generatedProjectPath))) {
        return ZR_FALSE;
    }

    memset(fixture, 0, sizeof(*fixture));
    snprintf(fixture->projectPath, sizeof(fixture->projectPath), "%s", generatedProjectPath);
    snprintf(rootPath, sizeof(rootPath), "%s", generatedProjectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    ZrLibrary_File_PathJoin(rootPath, "src", sourceRootPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "main.zr", fixture->mainPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "values.zr", fixture->modulePath);
    return write_text_file(fixture->projectPath, projectContent, strlen(projectContent)) &&
           write_text_file(fixture->mainPath, mainContent, strlen(mainContent)) &&
           write_text_file(fixture->modulePath, moduleContent, strlen(moduleContent));
}

static TZrBool invalidate_project_symbol_declaration_fact(
        SZrSemanticAnalyzer *analyzer,
        SZrSymbol *symbol) {
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || symbol == ZR_NULL ||
        symbol->semanticId == ZR_SEMANTIC_ID_INVALID ||
        symbol->semanticTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < analyzer->semanticContext->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
            (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                &analyzer->semanticContext->referenceFacts,
                index);
        if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
            !fact->isResolved || fact->symbolId != symbol->semanticId ||
            fact->typeId != symbol->semanticTypeId) {
            continue;
        }

        fact->isResolved = ZR_FALSE;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void test_lsp_project_source_symbol_type_uses_canonical_declaration(
        SZrState *state) {
    static const TZrChar *mainContent =
        "module main;\n"
        "var values = import(\"values\");\n"
        "return values.seed;\n";
    SZrTestTimer timer;
    SZrGeneratedSourceMemberRefreshFixture fixture;
    SZrLspContext *context = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *moduleUri = ZR_NULL;
    SZrLspPosition completionPosition;
    SZrLspPosition hoverPosition;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;
    SZrLspSemanticQuery semanticQuery;
    SZrLspMetadataProvider provider;
    SZrLspResolvedMetadataMember unavailableMember;

    TEST_START("LSP Project Source Symbol Type Uses Canonical Declaration");
    TEST_INFO("Project source canonical symbol type",
              "Imported source completion and hover must consume the declaration SymbolId and canonical TypeId, then fail closed when that declaration fact is unavailable");

    ZrCore_Array_Construct(&completions);
    if (!prepare_generated_project_canonical_symbol_type_fixture(
            "project_features_canonical_symbol_type",
            &fixture)) {
        TEST_FAIL(timer,
                  "LSP Project Source Symbol Type Uses Canonical Declaration",
                  "Failed to prepare the generated project-source symbol fixture");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    moduleUri = create_file_uri_from_native_path(state, fixture.modulePath);
    if (context == ZR_NULL || mainUri == ZR_NULL || moduleUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
            state, context, mainUri, mainContent, strlen(mainContent), 1) ||
        !lsp_find_position_for_substring(mainContent, "values.seed", 0, 7, &completionPosition) ||
        !lsp_find_position_for_substring(mainContent, "values.seed", 0, 8, &hoverPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Symbol Type Uses Canonical Declaration",
                  "Failed to open the project fixture or locate completion and hover positions");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(
            state, context, mainUri, completionPosition, &completions) ||
        !completion_detail_contains_fragment(&completions, "seed", "int") ||
        completion_detail_contains_fragment(&completions, "seed", "cannot infer exact type")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Symbol Type Uses Canonical Declaration",
                  "Project-source completion must initially format seed from its canonical int declaration type");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, hoverPosition, &hover) ||
        hover == ZR_NULL || !hover_contains_text(hover, "seed") ||
        !hover_contains_text(hover, "int") ||
        hover_contains_text(hover, "cannot infer exact type")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Symbol Type Uses Canonical Declaration",
                  "Project-source hover must initially format seed from its canonical int declaration type");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
            state, context, mainUri, mainContent, strlen(mainContent), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Symbol Type Uses Canonical Declaration",
                  "Failed to create an isolated project context for the unavailable declaration boundary");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&semanticQuery);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
            state, context, mainUri, hoverPosition, &semanticQuery) ||
        semanticQuery.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER ||
        semanticQuery.resolvedMember.declarationAnalyzer == ZR_NULL ||
        semanticQuery.resolvedMember.declarationSymbol == ZR_NULL ||
        semanticQuery.resolvedMember.declarationSymbol->typeInfo == ZR_NULL ||
        semanticQuery.resolvedMember.declarationSymbol->typeInfo->baseType != ZR_VALUE_TYPE_INT64 ||
        !invalidate_project_symbol_declaration_fact(
            semanticQuery.resolvedMember.declarationAnalyzer,
            semanticQuery.resolvedMember.declarationSymbol)) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Symbol Type Uses Canonical Declaration",
                  "Failed to preserve the legacy inferred int while invalidating the exact imported declaration fact");
        return;
    }
    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    memset(&unavailableMember, 0, sizeof(unavailableMember));
    hover = ZR_NULL;
    if (!ZrLanguageServer_LspMetadataProvider_ResolveImportedMember(
            &provider,
            semanticQuery.analyzer,
            semanticQuery.projectIndex,
            semanticQuery.moduleName,
            semanticQuery.memberName,
            &unavailableMember) ||
        !ZrLanguageServer_LspMetadataProvider_CreateImportedMemberHover(
            &provider,
            semanticQuery.analyzer,
            &unavailableMember,
            semanticQuery.queryRange,
            &hover) ||
        hover == ZR_NULL || !hover_contains_text(hover, "seed") ||
        !hover_contains_text(hover, "cannot infer exact type") ||
        hover_contains_text(hover, "int")) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Symbol Type Uses Canonical Declaration",
                  "Imported-member hover must fail closed instead of formatting the preserved symbol->typeInfo fallback");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(
            state, context, mainUri, completionPosition, &completions) ||
        !completion_array_contains_label(&completions, "seed") ||
        !completion_detail_contains_fragment(
            &completions, "seed", "cannot infer exact type") ||
        completion_detail_contains_fragment(&completions, "seed", "int")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Symbol Type Uses Canonical Declaration",
                  "Completion must fail closed instead of formatting the preserved symbol->typeInfo fallback");
        return;
    }
    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Project Source Symbol Type Uses Canonical Declaration");
}
