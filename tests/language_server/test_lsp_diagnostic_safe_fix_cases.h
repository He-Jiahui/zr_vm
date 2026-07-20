#ifndef ZR_VM_TEST_LSP_DIAGNOSTIC_SAFE_FIX_CASES_H
#define ZR_VM_TEST_LSP_DIAGNOSTIC_SAFE_FIX_CASES_H

static const SZrLspDiagnostic *diagnostic_safe_fix_find_code(
        SZrArray *diagnostics,
        const TZrChar *code) {
    for (TZrSize index = 0U;
         diagnostics != ZR_NULL && index < diagnostics->length;
         index++) {
        SZrLspDiagnostic **diagnosticPtr =
                (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        const TZrChar *diagnosticCode =
                diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL
                    ? test_string_text((*diagnosticPtr)->code)
                    : ZR_NULL;
        if (diagnosticCode != ZR_NULL && strcmp(diagnosticCode, code) == 0) {
            return *diagnosticPtr;
        }
    }
    return ZR_NULL;
}

static TZrBool diagnostic_safe_fix_action_matches(
        SZrArray *actions,
        const TZrChar *title,
        const TZrChar *editText) {
    for (TZrSize index = 0U;
         actions != ZR_NULL && index < actions->length;
         index++) {
        SZrLspCodeAction **actionPtr =
                (SZrLspCodeAction **)ZrCore_Array_Get(actions, index);
        const TZrChar *actionTitle =
                actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                    ? test_string_text((*actionPtr)->title)
                    : ZR_NULL;
        const TZrChar *actionEditText =
                actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                    ? first_text_edit_text(&(*actionPtr)->edits)
                    : ZR_NULL;
        if (actionTitle != ZR_NULL && strcmp(actionTitle, title) == 0 &&
            actionEditText != ZR_NULL && strcmp(actionEditText, editText) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_lsp_code_action_consumes_machine_applicable_diagnostic_fix(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action consumes machine-applicable diagnostic fix";
    const TZrChar *content =
            "while (true) {\n"
            "    break\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrArray diagnostics = {0};
    SZrArray actions = {0};
    const SZrLspDiagnostic *diagnostic;
    const SZrLspDiagnosticFix *fix = ZR_NULL;
    TZrBool valid = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(
            state,
            "file:///tmp/zr_lsp_diagnostic_semicolon_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_statement_semicolon");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 1 &&
            fix->editRange.start.character == 9 &&
            fix->editRange.end.line == 1 &&
            fix->editRange.end.character == 9 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing semicolon") == 0 &&
            strcmp(test_string_text(fix->editText), ";") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing semicolon", ";")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "structured semicolon diagnostic did not publish and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_skips_placeholder_diagnostic_fix(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action skips placeholder diagnostic fix";
    const TZrChar *content =
            "func choose(flag: bool): int {\n"
            "    var seed: int;\n"
            "    if (flag) {\n"
            "        seed = 1;\n"
            "    }\n"
            "    return seed;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrArray diagnostics = {0};
    SZrArray actions = {0};
    const SZrLspDiagnostic *diagnostic;
    const SZrLspDiagnosticFix *fix = ZR_NULL;
    TZrBool valid = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(
            state,
            "file:///tmp/zr_lsp_diagnostic_placeholder_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "possibly_uninitialized_read");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_HAS_PLACEHOLDERS &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            !diagnostic_safe_fix_action_matches(
                    &actions,
                    "Replace with an initialized value",
                    "<value>")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "placeholder diagnostic fix was missing or was incorrectly promoted to a safe code action");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#endif
