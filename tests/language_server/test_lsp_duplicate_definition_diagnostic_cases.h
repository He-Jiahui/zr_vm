#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_LSP_DUPLICATE_DEFINITION_DIAGNOSTIC_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_LSP_DUPLICATE_DEFINITION_DIAGNOSTIC_CASES_H

static void test_lsp_diagnostics_publish_duplicate_type_related_information(
        SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Duplicate Type Related Information";
    TZrChar uriText[] = "file:///duplicate_type_related.zr";
    const TZrChar *content =
            "class Pair {\n"
            "}\n"
            "class Pair {\n"
            "}\n";
    const SZrLspDiagnostic *diagnostic;
    const SZrLspDiagnosticRelatedInformation *related;
    const TZrChar *relatedMessage;
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to prepare duplicate type fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    diagnostic = diagnostic_array_find_code(&diagnostics, "duplicate_type");
    if (diagnostic == ZR_NULL ||
        diagnostic->descriptorId != 2010 ||
        diagnostic->message == ZR_NULL ||
        strstr(test_string_text(diagnostic->message), "Pair") == ZR_NULL ||
        diagnostic->range.start.line != 2 ||
        diagnostic->range.start.character != 6 ||
        diagnostic->range.end.line != 2 ||
        diagnostic->range.end.character != 10 ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 1) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected duplicate token range and one related definition");
        return;
    }

    related = (const SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&diagnostic->relatedInformation,
            0);
    relatedMessage = related != ZR_NULL ? test_string_text(related->message) : ZR_NULL;
    if (related == ZR_NULL ||
        related->location.uri == ZR_NULL ||
        strcmp(test_string_text(related->location.uri), uriText) != 0 ||
        related->location.range.start.line != 0 ||
        related->location.range.start.character != 6 ||
        related->location.range.end.line != 0 ||
        related->location.range.end.character != 10 ||
        relatedMessage == ZR_NULL ||
        strcmp(relatedMessage, "Type was first declared here") != 0) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected related information at the first type name");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif // ZR_VM_TESTS_LANGUAGE_SERVER_LSP_DUPLICATE_DEFINITION_DIAGNOSTIC_CASES_H
