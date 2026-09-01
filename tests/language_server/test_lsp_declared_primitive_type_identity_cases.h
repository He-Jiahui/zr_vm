#ifndef ZR_VM_TEST_LSP_DECLARED_PRIMITIVE_TYPE_IDENTITY_CASES_H
#define ZR_VM_TEST_LSP_DECLARED_PRIMITIVE_TYPE_IDENTITY_CASES_H

static void test_declared_primitive_types_publish_canonical_identity(
        SZrState *state) {
    static const TZrChar *content =
            "fn make(seed: int) {\n"
            "    return seed + 0;\n"
            "}\n"
            "class Derived<T, const N: int> {\n"
            "}\n"
            "fn measure<const M: int>(): int {\n"
            "    return 0;\n"
            "}\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrLspPosition parameterPosition;
    SZrLspPosition constBoundPosition;
    SZrLspPosition functionConstBoundPosition;
    SZrFilePosition parameterFilePosition;
    SZrFilePosition constBoundFilePosition;
    SZrFilePosition functionConstBoundFilePosition;
    SZrFileRange parameterRange;
    SZrFileRange constBoundRange;
    SZrFileRange functionConstBoundRange;
    SZrParserSemanticTypeQuery parameterQuery = {0};
    SZrParserSemanticTypeQuery constBoundQuery = {0};
    SZrParserSemanticTypeQuery functionConstBoundQuery = {0};
    TZrChar parameterDisplay[64] = {0};
    TZrChar constBoundDisplay[64] = {0};
    TZrChar functionConstBoundDisplay[64] = {0};
    TZrChar failure[640] = {0};
    TZrBool passed = ZR_FALSE;

    TEST_START("LSP Declared Primitive Types Publish Canonical Identity");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///declared_primitive_type_identity.zr",
            strlen("file:///declared_primitive_type_identity.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "seed: int", 0U, 6, &parameterPosition) ||
        !find_position(content, "N: int", 0U, 3, &constBoundPosition) ||
        !find_position(
                content,
                "M: int",
                0U,
                3,
                &functionConstBoundPosition)) {
        TEST_FAIL(
                timer,
                "LSP Declared Primitive Types Publish Canonical Identity",
                "Could not prepare declared primitive type fixture");
        goto cleanup;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        TEST_FAIL(
                timer,
                "LSP Declared Primitive Types Publish Canonical Identity",
                "Fixture did not expose a semantic snapshot");
        goto cleanup;
    }

    parameterFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, parameterPosition);
    constBoundFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, constBoundPosition);
    functionConstBoundFilePosition =
            ZrLanguageServer_Lsp_GetDocumentFilePosition(
                    context, uri, functionConstBoundPosition);
    parameterRange = ZrParser_FileRange_Create(
            parameterFilePosition, parameterFilePosition, uri);
    constBoundRange = ZrParser_FileRange_Create(
            constBoundFilePosition, constBoundFilePosition, uri);
    functionConstBoundRange = ZrParser_FileRange_Create(
            functionConstBoundFilePosition,
            functionConstBoundFilePosition,
            uri);

    passed = ZrParser_SemanticQuery_CanonicalTypeAt(
                     analyzer->semanticContext,
                     parameterRange,
                     ZR_NULL,
                     &parameterQuery) &&
             ZrParser_SemanticQuery_CanonicalTypeAt(
                     analyzer->semanticContext,
                     constBoundRange,
                     ZR_NULL,
                     &constBoundQuery) &&
             ZrParser_SemanticQuery_CanonicalTypeAt(
                     analyzer->semanticContext,
                     functionConstBoundRange,
                     ZR_NULL,
                     &functionConstBoundQuery) &&
             parameterQuery.typeId != ZR_SEMANTIC_ID_INVALID &&
             parameterQuery.typeId == constBoundQuery.typeId &&
             parameterQuery.typeId == functionConstBoundQuery.typeId &&
             parameterQuery.reference != ZR_NULL &&
             parameterQuery.reference->kind == ZR_SEMANTIC_REFERENCE_TYPE &&
             parameterQuery.reference->isResolved &&
             constBoundQuery.reference != ZR_NULL &&
             constBoundQuery.reference->kind == ZR_SEMANTIC_REFERENCE_TYPE &&
             constBoundQuery.reference->isResolved &&
             functionConstBoundQuery.reference != ZR_NULL &&
             functionConstBoundQuery.reference->kind ==
                     ZR_SEMANTIC_REFERENCE_TYPE &&
             functionConstBoundQuery.reference->isResolved &&
             ZrParser_CanonicalType_Format(
                     analyzer->semanticContext,
                     parameterQuery.typeId,
                     parameterDisplay,
                     sizeof(parameterDisplay)) &&
             ZrParser_CanonicalType_Format(
                     analyzer->semanticContext,
                     constBoundQuery.typeId,
                     constBoundDisplay,
                     sizeof(constBoundDisplay)) &&
             ZrParser_CanonicalType_Format(
                     analyzer->semanticContext,
                     functionConstBoundQuery.typeId,
                     functionConstBoundDisplay,
                     sizeof(functionConstBoundDisplay)) &&
             strcmp(parameterDisplay, "int") == 0 &&
             strcmp(constBoundDisplay, "int") == 0 &&
             strcmp(functionConstBoundDisplay, "int") == 0;

    if (passed) {
        TEST_PASS(
                timer,
                "LSP Declared Primitive Types Publish Canonical Identity");
    } else {
        snprintf(
                failure,
                sizeof(failure),
                "parameter={type=%llu reference=%p resolved=%d kind=%d display=%s} "
                "const={type=%llu reference=%p resolved=%d kind=%d display=%s} "
                "functionConst={type=%llu reference=%p resolved=%d kind=%d display=%s}",
                (unsigned long long)parameterQuery.typeId,
                (void *)parameterQuery.reference,
                parameterQuery.reference != ZR_NULL
                        ? parameterQuery.reference->isResolved
                        : 0,
                parameterQuery.reference != ZR_NULL
                        ? parameterQuery.reference->kind
                        : 0,
                parameterDisplay,
                (unsigned long long)constBoundQuery.typeId,
                (void *)constBoundQuery.reference,
                constBoundQuery.reference != ZR_NULL
                        ? constBoundQuery.reference->isResolved
                        : 0,
                constBoundQuery.reference != ZR_NULL
                        ? constBoundQuery.reference->kind
                        : 0,
                constBoundDisplay,
                (unsigned long long)functionConstBoundQuery.typeId,
                (void *)functionConstBoundQuery.reference,
                functionConstBoundQuery.reference != ZR_NULL
                        ? functionConstBoundQuery.reference->isResolved
                        : 0,
                functionConstBoundQuery.reference != ZR_NULL
                        ? functionConstBoundQuery.reference->kind
                        : 0,
                functionConstBoundDisplay);
        TEST_FAIL(
                timer,
                "LSP Declared Primitive Types Publish Canonical Identity",
                failure);
    }

cleanup:
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#endif
