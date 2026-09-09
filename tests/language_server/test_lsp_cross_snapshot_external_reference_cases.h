static TZrBool external_reference_locations_match(
        const SZrArray *locations,
        SZrString *mainUri,
        SZrLspPosition mainPosition,
        SZrString *siblingUri,
        SZrLspPosition siblingPosition,
        TZrSize memberLength,
        TZrBool includeSibling) {
    TZrSize index;
    TZrBool foundMain = ZR_FALSE;
    TZrBool foundSibling = ZR_FALSE;

    if (mainUri == ZR_NULL || (includeSibling && siblingUri == ZR_NULL) ||
        locations->length != (includeSibling ? 2U : 1U)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < locations->length; index++) {
        SZrLspLocation **slot = (SZrLspLocation **)ZrCore_Array_Get(
                (SZrArray *)locations, index);
        const SZrLspLocation *location = slot != ZR_NULL ? *slot : ZR_NULL;
        SZrLspPosition expected;

        if (location == ZR_NULL || location->uri == ZR_NULL) {
            return ZR_FALSE;
        }
        if (ZrCore_String_Equal(location->uri, mainUri)) {
            expected = mainPosition;
            foundMain = ZR_TRUE;
        } else if (includeSibling &&
                   ZrCore_String_Equal(location->uri, siblingUri)) {
            expected = siblingPosition;
            foundSibling = ZR_TRUE;
        } else {
            return ZR_FALSE;
        }
        if (location->range.start.line != expected.line ||
            location->range.end.line != expected.line ||
            location->range.start.character != expected.character ||
            location->range.end.character !=
                    expected.character + (TZrInt32)memberLength) {
            return ZR_FALSE;
        }
    }
    return foundMain && (!includeSibling || foundSibling);
}

static TZrBool external_reference_highlight_matches(
        const SZrArray *highlights,
        SZrLspPosition position,
        TZrSize memberLength,
        TZrInt32 kind) {
    const SZrLspDocumentHighlight *highlight;

    if (highlights->length != 1U) {
        return ZR_FALSE;
    }
    highlight = *(SZrLspDocumentHighlight **)ZrCore_Array_Get(
            (SZrArray *)highlights, 0U);
    return highlight != ZR_NULL && highlight->kind == kind &&
           highlight->range.start.line == position.line &&
           highlight->range.end.line == position.line &&
           highlight->range.start.character == position.character &&
           highlight->range.end.character ==
                   position.character + (TZrInt32)memberLength;
}

static void test_cross_snapshot_imported_references_use_external_identity(
        SZrState *state,
        TZrBool native) {
    const TZrChar *summary = native
            ? "LSP Native Cross Snapshot References Use External Identity"
            : "LSP Binary Cross Snapshot References Use External Identity";
    const TZrChar *label = native
            ? "external_reference_identity_native"
            : "external_reference_identity_binary";
    const TZrChar *member = native ? "console" : "binarySeed";
    const TZrChar *mainContent = native
            ? "var external = import(\"zr.system\");\n"
              "var siblingModule = import(\"sibling\");\n"
              "var decoyModule = import(\"decoy\");\n"
              "return external.console;\n"
            : "var external = import(\"external_reference_provider\");\n"
              "var siblingModule = import(\"sibling\");\n"
              "var decoyModule = import(\"decoy\");\n"
              "return external.binarySeed();\n";
    const TZrChar *siblingContent = native
            ? "var provider = import(\"zr.system\");\nreturn provider.console;\n"
            : "var provider = import(\"external_reference_provider\");\n"
              "return provider.binarySeed();\n";
    const TZrChar *decoyContent = native
            ? "var console = 1;\nreturn console;\n"
            : "fn binarySeed(): int { return 0; }\nreturn binarySeed();\n";
    static const TZrChar *projectContent =
            "{\"name\":\"external_reference_identity\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}\n";
    static const TZrChar *binaryContent =
            "pub var binarySeed = fn(): int => 40;\n";
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar binaryRoot[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar siblingPath[ZR_TESTS_PATH_MAX];
    TZrChar decoyPath[ZR_TESTS_PATH_MAX];
    TZrChar binaryPath[ZR_TESTS_PATH_MAX];
    TZrChar *separator;
    SZrLspContext *context = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *siblingUri = ZR_NULL;
    SZrLspPosition mainPosition;
    SZrLspPosition siblingPosition;
    SZrLspSemanticQuery query;
    SZrLspSemanticQuery siblingQuery;
    SZrSemanticReferenceFact *savedReferences = ZR_NULL;
    TZrSize savedReferenceCount = 0U;
    TZrSize matchingReferenceCount = 0U;
    TZrUInt32 targetMetadataToken = 0U;
    SZrAstNode *savedAst = ZR_NULL;
    SZrSymbolTable *savedSymbolTable = ZR_NULL;
    SZrReferenceTracker *savedReferenceTracker = ZR_NULL;
    SZrArray references = {0};
    SZrArray highlights = {0};
    SZrParityTimer timer;
    TZrChar failureBuffer[256];
    const TZrChar *failure = "fixture preparation";
    TZrBool valid = ZR_FALSE;
    TZrBool appended;
    TZrSize index;

    TEST_START(summary);
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    ZrLanguageServer_LspSemanticQuery_Init(&siblingQuery);
    if (!ZrTests_Path_GetGeneratedArtifact(
                "language_server", label, label, ".zrp",
                projectPath, sizeof(projectPath))) {
        goto cleanup;
    }
    snprintf(rootPath, sizeof(rootPath), "%s", projectPath);
    separator = strrchr(rootPath, '/');
    if (separator == ZR_NULL) {
        separator = strrchr(rootPath, '\\');
    }
    if (separator == ZR_NULL) {
        goto cleanup;
    }
    *separator = '\0';
    ZrLibrary_File_PathJoin(rootPath, "src", sourceRoot);
    ZrLibrary_File_PathJoin(rootPath, "bin", binaryRoot);
    ZrLibrary_File_PathJoin(sourceRoot, "main.zr", mainPath);
    ZrLibrary_File_PathJoin(sourceRoot, "sibling.zr", siblingPath);
    ZrLibrary_File_PathJoin(sourceRoot, "decoy.zr", decoyPath);
    ZrLibrary_File_PathJoin(
            binaryRoot, "external_reference_provider.zro", binaryPath);
    if (!write_text_file(projectPath, projectContent, strlen(projectContent)) ||
        !write_text_file(mainPath, mainContent, strlen(mainContent)) ||
        !write_text_file(siblingPath, siblingContent, strlen(siblingContent)) ||
        !write_text_file(decoyPath, decoyContent, strlen(decoyContent))) {
        goto cleanup;
    }
    if (!native) {
        SZrString *sourceName = ZrCore_String_CreateFromNative(state, binaryPath);
        SZrFunction *function = ZrParser_Source_Compile(
                state, binaryContent, strlen(binaryContent), sourceName);
        SZrBinaryWriterOptions options = {0};
        TZrBool written;

        options.moduleName = "external_reference_provider";
        written = function != ZR_NULL &&
                  ZrTests_Path_EnsureParentDirectory(binaryPath) &&
                  ZrParser_Writer_WriteBinaryFileWithOptions(
                          state, function, binaryPath, &options);
        if (function != ZR_NULL) {
            ZrCore_Function_Free(state, function);
        }
        if (!written) {
            goto cleanup;
        }
    }
    context = ZrLanguageServer_LspContext_New(state);
    mainUri = create_file_uri(state, mainPath);
    siblingUri = create_file_uri(state, siblingPath);
    failure = "canonical imported member resolution";
    if (context == ZR_NULL || mainUri == ZR_NULL || siblingUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, mainUri, mainContent, strlen(mainContent), 1U) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, siblingUri, siblingContent, strlen(siblingContent), 1U) ||
        !find_position(mainContent, member, 0U, 0, &mainPosition) ||
        !find_position(siblingContent, member, 0U, 0, &siblingPosition) ||
        !ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, siblingUri, siblingPosition, &siblingQuery) ||
        !ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, mainUri, mainPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER ||
        !query.hasCanonicalSymbol || !query.canonicalSymbol.hasExternalTarget ||
        query.projectIndex == ZR_NULL || siblingQuery.analyzer == ZR_NULL) {
        goto cleanup;
    }

    failure = "external facts must carry the current nonzero host generation";
    if (context->semanticSnapshotProviderGeneration == 0U ||
        query.canonicalSymbol.externalProviderGeneration !=
                context->semanticSnapshotProviderGeneration ||
        siblingQuery.canonicalSymbol.externalProviderGeneration !=
                context->semanticSnapshotProviderGeneration) {
        goto cleanup;
    }

    for (index = 0U;
         index < siblingQuery.analyzer->semanticContext->referenceFacts.length;
         index++) {
        SZrSemanticReferenceFact *candidate =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &siblingQuery.analyzer->semanticContext->referenceFacts,
                        index);
        if (candidate != ZR_NULL && candidate->isResolved &&
            candidate->hasExternalTarget &&
            candidate->externalOwnerIdentity != ZR_NULL &&
            candidate->externalMetadataToken != 0U &&
            candidate->externalSignatureToken != 0U &&
            candidate->externalSignatureHash != 0U &&
            candidate->externalTargetKind != ZR_SEMANTIC_EXTERNAL_TARGET_UNKNOWN &&
            candidate->range.start.line == siblingPosition.line + 1 &&
            candidate->range.start.column == siblingPosition.character + 1) {
            matchingReferenceCount++;
        }
    }
    if (matchingReferenceCount == 0U) {
        failure = "canonical sibling external reference";
        goto cleanup;
    }
    savedReferenceCount = siblingQuery.analyzer->semanticContext->referenceFacts.length;
    savedReferences = (SZrSemanticReferenceFact *)malloc(
            savedReferenceCount * sizeof(*savedReferences));
    if (savedReferences == ZR_NULL) {
        failure = "saving sibling reference facts";
        goto cleanup;
    }
    memcpy(savedReferences,
           siblingQuery.analyzer->semanticContext->referenceFacts.head,
           savedReferenceCount * sizeof(*savedReferences));
    targetMetadataToken = query.canonicalSymbol.externalMetadataToken;
    savedAst = siblingQuery.analyzer->ast;
    savedSymbolTable = siblingQuery.analyzer->symbolTable;
    savedReferenceTracker = siblingQuery.analyzer->referenceTracker;
    siblingQuery.analyzer->ast = ZR_NULL;
    siblingQuery.analyzer->symbolTable = ZR_NULL;
    siblingQuery.analyzer->referenceTracker = ZR_NULL;

    appended = ZrLanguageServer_LspSemanticQuery_AppendReferences(
            state, context, &query, ZR_FALSE, &references);
    if (!appended || !external_reference_locations_match(
                &references, mainUri, mainPosition, siblingUri, siblingPosition,
                strlen(member), ZR_TRUE)) {
        snprintf(failureBuffer, sizeof(failureBuffer),
                 "cross-snapshot references: appended=%d count=%zu kind=%d external=%d generation=%llu",
                 appended, (size_t)references.length, query.kind,
                 query.canonicalSymbol.hasExternalTarget,
                 (unsigned long long)query.canonicalSymbol.externalProviderGeneration);
        failure = failureBuffer;
        goto cleanup;
    }
    free_local_reference_projection_results(state, &references, ZR_NULL);

    failure = "external highlights must survive detached legacy state";
    if (!ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
                state, context, &siblingQuery, &highlights) ||
        !external_reference_highlight_matches(
                &highlights, siblingPosition, strlen(member), 2)) {
        goto cleanup;
    }
    free_local_reference_projection_results(state, ZR_NULL, &highlights);

    for (index = 0U; index < 10U; index++) {
        memcpy(siblingQuery.analyzer->semanticContext->referenceFacts.head,
               savedReferences, savedReferenceCount * sizeof(*savedReferences));
        for (TZrSize referenceIndex = 0U;
             referenceIndex < savedReferenceCount;
             referenceIndex++) {
            SZrSemanticReferenceFact *siblingReference =
                    (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                            &siblingQuery.analyzer->semanticContext->referenceFacts,
                            referenceIndex);
            if (siblingReference == ZR_NULL || !siblingReference->hasExternalTarget ||
                siblingReference->range.start.line != siblingPosition.line + 1 ||
                siblingReference->range.start.column != siblingPosition.character + 1) {
                continue;
            }
            switch (index) {
                case 0U: siblingReference->externalMetadataToken = 0U; break;
                case 1U: siblingReference->externalSignatureHash++; break;
                case 2U: siblingReference->externalOwnerIdentity = mainUri; break;
                case 3U:
                    siblingReference->externalProviderGeneration =
                            context->semanticSnapshotProviderGeneration + 1U;
                    break;
                case 4U: siblingReference->isResolved = ZR_FALSE; break;
                case 5U: siblingReference->externalSignatureToken++; break;
                case 6U: siblingReference->externalMetadataToken++; break;
                case 8U: siblingReference->hasExternalTarget = ZR_FALSE; break;
                case 9U:
                    siblingReference->symbolId = ZR_SEMANTIC_ID_INVALID;
                    break;
                default:
                    siblingReference->externalTargetKind =
                            ZR_SEMANTIC_EXTERNAL_TARGET_UNKNOWN;
                    break;
            }
        }
        appended = ZrLanguageServer_LspSemanticQuery_AppendReferences(
                state, context, &query, ZR_FALSE, &references);
        if (!appended || !external_reference_locations_match(
                    &references, mainUri, mainPosition, siblingUri, siblingPosition,
                    strlen(member), ZR_FALSE)) {
            snprintf(failureBuffer, sizeof(failureBuffer),
                     "invalid sibling identity case=%zu returned %zu references",
                     (size_t)index, (size_t)references.length);
            failure = failureBuffer;
            goto cleanup;
        }
        free_local_reference_projection_results(state, &references, ZR_NULL);
        if (ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
                    state, context, &siblingQuery, &highlights) ||
            highlights.length != 0U) {
            snprintf(failureBuffer, sizeof(failureBuffer),
                     "invalid local external identity case=%zu returned %zu highlights",
                     (size_t)index, (size_t)highlights.length);
            failure = failureBuffer;
            goto cleanup;
        }
        free_local_reference_projection_results(state, ZR_NULL, &highlights);
    }
    memcpy(siblingQuery.analyzer->semanticContext->referenceFacts.head,
           savedReferences, savedReferenceCount * sizeof(*savedReferences));
    for (index = 0U; index < savedReferenceCount; index++) {
        SZrSemanticReferenceFact *reference =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &siblingQuery.analyzer->semanticContext->referenceFacts,
                        index);
        if (reference->hasExternalTarget &&
            reference->range.start.line == siblingPosition.line + 1 &&
            reference->range.start.column == siblingPosition.character + 1) {
            reference->kind = ZR_SEMANTIC_REFERENCE_MEMBER_WRITE;
        }
    }
    failure = "external highlights must preserve the published write role";
    if (!ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
                state, context, &siblingQuery, &highlights) ||
        !external_reference_highlight_matches(
                &highlights, siblingPosition, strlen(member), 3)) {
        goto cleanup;
    }
    free_local_reference_projection_results(state, ZR_NULL, &highlights);
    memcpy(siblingQuery.analyzer->semanticContext->referenceFacts.head,
           savedReferences, savedReferenceCount * sizeof(*savedReferences));
    query.canonicalSymbol.externalMetadataToken = 0U;
    failure = "incomplete target identity must reject declaration and references";
    if (ZrLanguageServer_LspSemanticQuery_AppendReferences(
                state, context, &query, ZR_TRUE, &references) ||
        references.length != 0U ||
        ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
                state, context, &query, &highlights) ||
        highlights.length != 0U) {
        goto cleanup;
    }
    query.canonicalSymbol.externalMetadataToken = targetMetadataToken;
    query.canonicalSymbol.externalProviderGeneration =
            context->semanticSnapshotProviderGeneration + 1U;
    failure = "stale target identity must reject references and highlights";
    if (ZrLanguageServer_LspSemanticQuery_AppendReferences(
                state, context, &query, ZR_FALSE, &references) ||
        references.length != 0U ||
        ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
                state, context, &query, &highlights) ||
        highlights.length != 0U) {
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    if (savedReferences != ZR_NULL) {
        memcpy(siblingQuery.analyzer->semanticContext->referenceFacts.head,
               savedReferences, savedReferenceCount * sizeof(*savedReferences));
        free(savedReferences);
        siblingQuery.analyzer->ast = savedAst;
        siblingQuery.analyzer->symbolTable = savedSymbolTable;
        siblingQuery.analyzer->referenceTracker = savedReferenceTracker;
    }
    free_local_reference_projection_results(state, &references, &highlights);
    ZrLanguageServer_LspSemanticQuery_Free(state, &siblingQuery);
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspContext_Free(state, context);
    if (valid) {
        TEST_PASS(timer, summary);
    } else {
        TEST_FAIL(timer, summary, failure);
    }
}
