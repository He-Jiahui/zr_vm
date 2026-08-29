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
