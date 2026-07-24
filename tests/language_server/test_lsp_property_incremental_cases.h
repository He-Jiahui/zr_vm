#ifndef ZR_VM_TEST_LSP_PROPERTY_INCREMENTAL_CASES_H
#define ZR_VM_TEST_LSP_PROPERTY_INCREMENTAL_CASES_H

#define ZR_LSP_PROPERTY_STRESS_COUNT 64U
#define ZR_LSP_PROPERTY_STRESS_CAPACITY 32768U

static TZrChar *property_incremental_build_source(
        TZrUInt32 bodyRevision,
        TZrBool stringContract) {
    TZrChar *source = (TZrChar *)malloc(ZR_LSP_PROPERTY_STRESS_CAPACITY);
    TZrSize length = 0U;

    if (source == ZR_NULL) {
        return ZR_NULL;
    }
    length += (TZrSize)snprintf(
            source + length,
            ZR_LSP_PROPERTY_STRESS_CAPACITY - length,
            "class PropertyCatalog {\n");
    for (TZrUInt32 index = 0U;
         index < ZR_LSP_PROPERTY_STRESS_COUNT;
         index++) {
        TZrInt32 written;

        if (index == 32U && stringContract) {
            written = snprintf(
                    source + length,
                    ZR_LSP_PROPERTY_STRESS_CAPACITY - length,
                    "  pub property p032: string { get { return \"changed\"; } }\n");
        } else {
            TZrUInt32 value = index == 32U ? index + bodyRevision : index;
            written = snprintf(
                    source + length,
                    ZR_LSP_PROPERTY_STRESS_CAPACITY - length,
                    "  pub property p%03u: int { get { return %u; } }\n",
                    index,
                    value);
        }
        if (written <= 0 ||
            (TZrSize)written >= ZR_LSP_PROPERTY_STRESS_CAPACITY - length) {
            free(source);
            return ZR_NULL;
        }
        length += (TZrSize)written;
    }
    {
        TZrInt32 written = snprintf(
                source + length,
                ZR_LSP_PROPERTY_STRESS_CAPACITY - length,
                stringContract
                        ? "}\nfn read(c: PropertyCatalog): string { return c.p032; }\n"
                        : "}\nfn read(c: PropertyCatalog): int { return c.p032; }\n");
        if (written <= 0 ||
            (TZrSize)written >= ZR_LSP_PROPERTY_STRESS_CAPACITY - length) {
            free(source);
            return ZR_NULL;
        }
    }
    return source;
}

static TZrBool property_incremental_query(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const TZrChar *content,
        const TZrChar *propertyText,
        SZrParserSemanticPropertyQuery *outQuery) {
    SZrLspPosition position;
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePosition;
    SZrFileRange range;

    if (!lsp_find_position_for_substring(
                content,
                propertyText,
                0U,
                9,
                &position)) {
        return ZR_FALSE;
    }
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_FALSE;
    }
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context,
            uri,
            position);
    range = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    return ZrParser_SemanticQuery_PropertyAt(
            analyzer->semanticContext,
            range,
            ZR_NULL,
            outQuery);
}

static void test_lsp_property_incremental_contract_boundaries(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Property Incremental Contract Boundaries";
    const TZrChar *uriText = "file:///property_incremental_stress.zr";
    TZrChar *initial = property_incremental_build_source(0U, ZR_FALSE);
    TZrChar *bodyEdit = property_incremental_build_source(1U, ZR_FALSE);
    TZrChar *contractEdit = property_incremental_build_source(0U, ZR_TRUE);
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrParserSemanticPropertyQuery initialStable;
    SZrParserSemanticPropertyQuery initialTarget;
    SZrParserSemanticPropertyQuery bodyStable;
    SZrParserSemanticPropertyQuery bodyTarget;
    SZrParserSemanticPropertyQuery contractStable;
    SZrParserSemanticPropertyQuery contractTarget;
    SZrLspPosition usagePosition;
    SZrLspHover *hover = ZR_NULL;
    TZrBool valid = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "Large property document",
            "Body-only edits preserve exact property facts while a contract edit invalidates only the changed property type");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            (TZrNativeString)uriText,
            strlen(uriText));
    if (initial == ZR_NULL || bodyEdit == ZR_NULL || contractEdit == ZR_NULL ||
        context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                initial,
                strlen(initial),
                1U) ||
        !property_incremental_query(
                state,
                context,
                uri,
                initial,
                "property p010",
                &initialStable) ||
        !property_incremental_query(
                state,
                context,
                uri,
                initial,
                "property p032",
                &initialTarget) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                bodyEdit,
                strlen(bodyEdit),
                2U) ||
        !property_incremental_query(
                state,
                context,
                uri,
                bodyEdit,
                "property p010",
                &bodyStable) ||
        !property_incremental_query(
                state,
                context,
                uri,
                bodyEdit,
                "property p032",
                &bodyTarget) ||
        initialStable.propertySymbolId != bodyStable.propertySymbolId ||
        initialStable.propertyTypeId != bodyStable.propertyTypeId ||
        initialTarget.propertySymbolId != bodyTarget.propertySymbolId ||
        initialTarget.propertyTypeId != bodyTarget.propertyTypeId ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                contractEdit,
                strlen(contractEdit),
                3U) ||
        !property_incremental_query(
                state,
                context,
                uri,
                contractEdit,
                "property p010",
                &contractStable) ||
        !property_incremental_query(
                state,
                context,
                uri,
                contractEdit,
                "property p032",
                &contractTarget) ||
        contractStable.propertySymbolId != initialStable.propertySymbolId ||
        contractStable.propertyTypeId != initialStable.propertyTypeId ||
        contractTarget.propertyTypeId == initialTarget.propertyTypeId ||
        !lsp_find_position_for_substring(
                contractEdit,
                "c.p032",
                0U,
                2,
                &usagePosition) ||
        !ZrLanguageServer_Lsp_GetHover(
                state,
                context,
                uri,
                usagePosition,
                &hover) ||
        !hover_contains_text(hover, "property p032: string")) {
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    if (!valid) {
        TEST_FAIL(
                timer,
                summary,
                "Property facts did not preserve body-only identity or invalidate the changed contract");
    } else {
        TEST_PASS(timer, summary);
    }
    free(initial);
    free(bodyEdit);
    free(contractEdit);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#endif
