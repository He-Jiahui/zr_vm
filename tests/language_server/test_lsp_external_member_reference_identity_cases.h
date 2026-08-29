static void test_external_member_references_reject_mismatched_declaration_identity(
        SZrState *state) {
    static const TZrChar *content =
            "var {LinkedList} = import(\"zr.container\");\n"
            "var list: LinkedList<int> = null;\n"
            "list.addLast(1);\n"
            "list.addLast(2);\n";
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition memberPosition;
    SZrLspSemanticQuery query;
    SZrFileRange originalDeclarationRange;
    SZrArray locations = {0};
    SZrParityTimer timer;
    TZrChar failureBuffer[256] = {0};
    const TZrChar *failure = "fixture initialization";
    TZrBool valid = ZR_FALSE;
    TZrBool resolved = ZR_FALSE;
    TZrBool appended = ZR_FALSE;

    TEST_START("LSP External Member References Reject Mismatched Declaration Identity");
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///external_member_identity.zr",
            strlen("file:///external_member_identity.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "list.addLast", 0U, 5, &memberPosition)) {
        goto cleanup;
    }

    failure = "exact external member resolution";
    resolved = ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
            state, context, uri, memberPosition, &query);
    if (resolved) {
        appended = ZrLanguageServer_LspSemanticQuery_AppendReferences(
                state, context, &query, ZR_FALSE, &locations);
    }
    if (!resolved ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        !query.resolvedMember.hasDeclaration ||
        query.resolvedMember.declarationUri == ZR_NULL ||
        !appended ||
        locations.length != 2U) {
        snprintf(
                failureBuffer,
                sizeof(failureBuffer),
                "resolved=%d kind=%d declaration=%d uri=%p project=%p appended=%d count=%zu",
                resolved,
                query.kind,
                query.resolvedMember.hasDeclaration,
                (void *)query.resolvedMember.declarationUri,
                (void *)query.projectIndex,
                appended,
                (size_t)locations.length);
        failure = failureBuffer;
        goto cleanup;
    }

    free_local_reference_projection_results(state, &locations, ZR_NULL);
    originalDeclarationRange = query.resolvedMember.declarationRange;
    query.resolvedMember.declarationRange.start.line++;
    query.resolvedMember.declarationRange.start.offset++;
    query.resolvedMember.declarationRange.end.line++;
    query.resolvedMember.declarationRange.end.offset++;

    failure = "mismatched exact declaration must fail closed";
    if (ZrLanguageServer_LspSemanticQuery_AppendReferences(
                state, context, &query, ZR_FALSE, &locations) ||
        locations.length != 0U) {
        query.resolvedMember.declarationRange = originalDeclarationRange;
        goto cleanup;
    }
    query.resolvedMember.declarationRange = originalDeclarationRange;
    valid = ZR_TRUE;

cleanup:
    free_local_reference_projection_results(state, &locations, ZR_NULL);
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (valid) {
        TEST_PASS(
                timer,
                "LSP External Member References Reject Mismatched Declaration Identity");
    } else {
        TEST_FAIL(
                timer,
                "LSP External Member References Reject Mismatched Declaration Identity",
                failure);
    }
}

static void test_external_member_query_rejects_stale_document_snapshot(
        SZrState *state) {
    static const TZrChar *content =
            "var {LinkedList} = import(\"zr.container\");\n"
            "var list: LinkedList<int> = null;\n"
            "list.addLast(1);\n"
            "list.addLast(2);\n";
    static const TZrChar *updatedContent =
            "var {LinkedList} = import(\"zr.container\");\n"
            "var list: LinkedList<int> = null;\n"
            "list.addLast(1);\n"
            "list.addLast(2);\n"
            "\n";
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition memberPosition;
    SZrLspSemanticQuery query;
    SZrLspHover *hover = ZR_NULL;
    SZrArray definitions = {0};
    SZrArray references = {0};
    SZrArray highlights = {0};
    SZrParityTimer timer;
    TZrChar failureBuffer[256] = {0};
    const TZrChar *failure = "fixture initialization";
    TZrBool valid = ZR_FALSE;
    TZrBool hoverBuilt;
    TZrBool definitionAppended;
    TZrBool referencesAppended;
    TZrBool highlightsAppended;

    TEST_START("LSP External Member Query Rejects Stale Document Snapshot");
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///external_member_snapshot.zr",
            strlen("file:///external_member_snapshot.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "list.addLast", 0U, 5, &memberPosition) ||
        !ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, memberPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        !query.hasSemanticVersion ||
        !query.resolvedMember.hasDeclaration) {
        failure = "fresh query document version must be captured";
        goto cleanup;
    }

    failure = "document update";
    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                updatedContent,
                strlen(updatedContent),
                2U)) {
        goto cleanup;
    }

    failure = "stale query consumers must fail closed";
    hoverBuilt = ZrLanguageServer_LspSemanticQuery_BuildHover(
            state, context, &query, &hover);
    definitionAppended = ZrLanguageServer_LspSemanticQuery_AppendDefinitions(
            state, context, &query, &definitions);
    referencesAppended = ZrLanguageServer_LspSemanticQuery_AppendReferences(
            state, context, &query, ZR_FALSE, &references);
    highlightsAppended = ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
            state, context, &query, &highlights);
    if (hoverBuilt || hover != ZR_NULL ||
        definitionAppended || definitions.length != 0U ||
        referencesAppended || references.length != 0U ||
        highlightsAppended || highlights.length != 0U) {
        snprintf(
                failureBuffer,
                sizeof(failureBuffer),
                "hover=%d/%p definitions=%d/%zu references=%d/%zu highlights=%d/%zu",
                hoverBuilt,
                (void *)hover,
                definitionAppended,
                (size_t)definitions.length,
                referencesAppended,
                (size_t)references.length,
                highlightsAppended,
                (size_t)highlights.length);
        failure = failureBuffer;
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    if (hover != ZR_NULL) {
        if (hover->contents.isValid) {
            ZrCore_Array_Free(state, &hover->contents);
        }
        ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
    }
    free_local_reference_projection_results(state, &definitions, ZR_NULL);
    free_local_reference_projection_results(state, &references, &highlights);
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (valid) {
        TEST_PASS(timer, "LSP External Member Query Rejects Stale Document Snapshot");
    } else {
        TEST_FAIL(
                timer,
                "LSP External Member Query Rejects Stale Document Snapshot",
                failure);
    }
}
