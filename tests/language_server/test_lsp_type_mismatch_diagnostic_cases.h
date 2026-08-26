#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_LSP_TYPE_MISMATCH_DIAGNOSTIC_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_LSP_TYPE_MISMATCH_DIAGNOSTIC_CASES_H

static void test_lsp_diagnostics_publish_detailed_initializer_type_mismatch(
        SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Detailed Initializer Type Mismatch";
    TZrChar uriText[] = "file:///initializer_type_mismatch.zr";
    const TZrChar *content =
            "fn main(): int {\n"
            "    var amount: int = 3.75;\n"
            "    return 0;\n"
            "}\n";
    const SZrLspDiagnostic *diagnostic;
    const SZrLspDiagnosticRelatedInformation *related;
    const SZrLspDiagnosticFix *fix;
    const TZrChar *messageText;
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
        TEST_FAIL(timer, summary, "Failed to prepare initializer type mismatch fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    diagnostic = diagnostic_array_find_code(&diagnostics, "type_mismatch");
    messageText = diagnostic != ZR_NULL ? test_string_text(diagnostic->message) : ZR_NULL;
    if (diagnostic == ZR_NULL ||
        diagnostic->descriptorId != 2011 ||
        diagnostic->severity != 1 ||
        messageText == ZR_NULL ||
        strncmp(messageText,
                "Expected 'int' but found 'float'",
                strlen("Expected 'int' but found 'float'")) != 0 ||
        strstr(messageText,
               "Cause: Type 'float' cannot be implicitly converted to 'int'.") == ZR_NULL ||
        strstr(messageText, "Suggestion: Add an explicit cast") == ZR_NULL ||
        diagnostic->range.start.line != 1 ||
        diagnostic->range.start.character != 22 ||
        diagnostic->range.end.line != 1 ||
        diagnostic->range.end.character != 26 ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 1 ||
        !diagnostic->fixes.isValid ||
        diagnostic->fixes.length != 1) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected detailed mismatch fields at the initializer expression");
        return;
    }

    related = (const SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&diagnostic->relatedInformation,
            0);
    if (related == ZR_NULL ||
        related->location.uri == ZR_NULL ||
        strcmp(test_string_text(related->location.uri), uriText) != 0 ||
        related->location.range.start.line != 1 ||
        related->location.range.start.character != 16 ||
        related->location.range.end.line != 1 ||
        related->location.range.end.character != 19 ||
        related->message == ZR_NULL ||
        strcmp(test_string_text(related->message), "Expected type is declared here") != 0) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected the type annotation as related information");
        return;
    }

    fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get((SZrArray *)&diagnostic->fixes, 0);
    if (fix == ZR_NULL ||
        fix->title == ZR_NULL ||
        strcmp(test_string_text(fix->title), "Cast value to 'int'") != 0 ||
        fix->editText == ZR_NULL ||
        strcmp(test_string_text(fix->editText), "<int> <expression>") != 0 ||
        fix->applicability != ZR_DIAGNOSTIC_FIX_HAS_PLACEHOLDERS ||
        fix->editRange.start.line != 1 ||
        fix->editRange.start.character != 22 ||
        fix->editRange.end.line != 1 ||
        fix->editRange.end.character != 26) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected a placeholder explicit-cast fix over the initializer");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static const SZrLspDiagnostic *type_mismatch_diagnostic_find_at_line(
        SZrArray *diagnostics,
        TZrInt32 line) {
    TZrSize index;

    if (diagnostics == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr =
                (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        const TZrChar *code = diagnosticPtr != ZR_NULL &&
                                     *diagnosticPtr != ZR_NULL &&
                                     (*diagnosticPtr)->code != ZR_NULL
                                     ? test_string_text((*diagnosticPtr)->code)
                                     : ZR_NULL;
        if (code != ZR_NULL &&
            strcmp(code, "type_mismatch") == 0 &&
            (*diagnosticPtr)->range.start.line == line) {
            return *diagnosticPtr;
        }
    }
    return ZR_NULL;
}

static TZrBool type_mismatch_diagnostic_has_expected_relation_and_fix(
        const SZrLspDiagnostic *diagnostic,
        TZrInt32 primaryStart,
        TZrInt32 primaryEnd,
        TZrInt32 relatedLine,
        TZrInt32 relatedStart,
        TZrInt32 relatedEnd) {
    const SZrLspDiagnosticFix *fix;
    const SZrLspDiagnosticRelatedInformation *related;
    const TZrChar *messageText;

    if (diagnostic == ZR_NULL ||
        diagnostic->descriptorId != 2011 ||
        diagnostic->codeDescriptionHref == ZR_NULL ||
        strcmp(test_string_text(diagnostic->codeDescriptionHref),
               "https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md") != 0 ||
        diagnostic->noFixReason != ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED ||
        diagnostic->range.start.character != primaryStart ||
        diagnostic->range.end.character != primaryEnd ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 1 ||
        !diagnostic->fixes.isValid ||
        diagnostic->fixes.length != 1) {
        return ZR_FALSE;
    }

    messageText = test_string_text(diagnostic->message);
    related = (const SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&diagnostic->relatedInformation,
            0);
    fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get((SZrArray *)&diagnostic->fixes, 0);
    return messageText != ZR_NULL &&
           strstr(messageText, "Expected 'int' but found 'float'") != ZR_NULL &&
           related != ZR_NULL &&
           related->location.range.start.line == relatedLine &&
           related->location.range.start.character == relatedStart &&
           related->location.range.end.line == relatedLine &&
           related->location.range.end.character == relatedEnd &&
           fix != ZR_NULL &&
           fix->editText != ZR_NULL &&
           strcmp(test_string_text(fix->editText), "<int> <expression>") == 0 &&
           fix->applicability == ZR_DIAGNOSTIC_FIX_HAS_PLACEHOLDERS;
}

static void test_lsp_diagnostics_publish_detailed_assignment_and_return_type_mismatch(
        SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Detailed Assignment And Return Type Mismatch";
    TZrChar uriText[] = "file:///assignment_return_type_mismatch.zr";
    const TZrChar *content =
            "fn bad(): int {\n"
            "    return 2.5;\n"
            "}\n"
            "fn main(): int {\n"
            "    var amount: int = 0;\n"
            "    amount = 3.75;\n"
            "    return 0;\n"
            "}\n";
    const SZrLspDiagnostic *assignmentDiagnostic;
    const SZrLspDiagnostic *returnDiagnostic;
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
        TEST_FAIL(timer, summary, "Failed to prepare assignment/return mismatch fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    returnDiagnostic = type_mismatch_diagnostic_find_at_line(&diagnostics, 1);
    assignmentDiagnostic = type_mismatch_diagnostic_find_at_line(&diagnostics, 5);
    if (!type_mismatch_diagnostic_has_expected_relation_and_fix(
                returnDiagnostic, 11, 14, 0, 10, 13) ||
        !type_mismatch_diagnostic_has_expected_relation_and_fix(
                assignmentDiagnostic, 13, 17, 4, 16, 19)) {
        TZrSize index;
        for (index = 0; index < diagnostics.length; index++) {
            SZrLspDiagnostic **diagnosticPtr =
                    (SZrLspDiagnostic **)ZrCore_Array_Get(&diagnostics, index);
            const SZrLspDiagnostic *observed =
                    diagnosticPtr != ZR_NULL ? *diagnosticPtr : ZR_NULL;
            const TZrChar *code = observed != ZR_NULL && observed->code != ZR_NULL
                                         ? test_string_text(observed->code)
                                         : ZR_NULL;
            if (code != ZR_NULL && strcmp(code, "type_mismatch") == 0) {
                const SZrLspDiagnosticRelatedInformation *observedRelated =
                        observed->relatedInformation.isValid &&
                                observed->relatedInformation.length > 0
                                ? (const SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get(
                                          (SZrArray *)&observed->relatedInformation,
                                          0)
                                : ZR_NULL;
                printf("Observed type_mismatch id=%u primary=%d:%d..%d:%d related=%llu fixes=%llu",
                       observed->descriptorId,
                       observed->range.start.line,
                       observed->range.start.character,
                       observed->range.end.line,
                       observed->range.end.character,
                       (unsigned long long)observed->relatedInformation.length,
                       (unsigned long long)observed->fixes.length);
                if (observedRelated != ZR_NULL) {
                    printf(" relatedRange=%d:%d..%d:%d",
                           observedRelated->location.range.start.line,
                           observedRelated->location.range.start.character,
                           observedRelated->location.range.end.line,
                           observedRelated->location.range.end.character);
                }
                printf("\n");
            }
        }
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected detailed assignment and return mismatch diagnostics");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif // ZR_VM_TESTS_LANGUAGE_SERVER_LSP_TYPE_MISMATCH_DIAGNOSTIC_CASES_H
