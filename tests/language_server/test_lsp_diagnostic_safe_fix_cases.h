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

static void test_lsp_code_action_inserts_missing_condition_close(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing condition close";
    const TZrChar *content = "if (ready { return 1; }\n";
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
            "file:///tmp/zr_lsp_diagnostic_condition_close_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_condition_close");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 0 &&
            fix->editRange.start.character == 10 &&
            fix->editRange.end.line == 0 &&
            fix->editRange.end.character == 10 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing ')'") == 0 &&
            strcmp(test_string_text(fix->editText), ")") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing ')'", ")")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing condition close did not publish and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_index_close(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing index close";
    const TZrChar *content =
            "var value = [1, 2];\n"
            "return value[0;\n";
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
            "file:///tmp/zr_lsp_diagnostic_index_close_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_index_close");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 1 &&
            fix->editRange.start.character == 14 &&
            fix->editRange.end.line == 1 &&
            fix->editRange.end.character == 14 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing ']'") == 0 &&
            strcmp(test_string_text(fix->editText), "]") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing ']'", "]")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing index close did not publish and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_parameter_list_close(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing parameter list close";
    const TZrChar *content =
            "func pick(value: int: int { return value; }\n";
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
            "file:///tmp/zr_lsp_diagnostic_parameter_list_close_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_parameter_list_close");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 0 &&
            fix->editRange.start.character == 20 &&
            fix->editRange.end.line == 0 &&
            fix->editRange.end.character == 20 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing ')'") == 0 &&
            strcmp(test_string_text(fix->editText), ")") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing ')'", ")")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing parameter list close did not publish and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_call_close(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing call close";
    const TZrChar *content =
            "func pick(value: int): int { return value; }\n"
            "return pick(1 + 2;\n";
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
            "file:///tmp/zr_lsp_diagnostic_call_close_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_call_close");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            diagnostic->range.start.line == 1 &&
            diagnostic->range.start.character == 11 &&
            diagnostic->range.end.line == 1 &&
            diagnostic->range.end.character == 12 &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 1 &&
            fix->editRange.start.character == 17 &&
            fix->editRange.end.line == 1 &&
            fix->editRange.end.character == 17 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing ')'") == 0 &&
            strcmp(test_string_text(fix->editText), ")") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing ')'", ")")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing call close did not preserve its opener range and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_group_close(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing group close";
    const TZrChar *content = "return (1 + 2;\n";
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
            "file:///tmp/zr_lsp_diagnostic_group_close_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_group_close");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            diagnostic->range.start.line == 0 &&
            diagnostic->range.start.character == 7 &&
            diagnostic->range.end.line == 0 &&
            diagnostic->range.end.character == 8 &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 0 &&
            fix->editRange.start.character == 13 &&
            fix->editRange.end.line == 0 &&
            fix->editRange.end.character == 13 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing ')'") == 0 &&
            strcmp(test_string_text(fix->editText), ")") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing ')'", ")")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing group close did not preserve its opener range and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_array_close(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing array close";
    const TZrChar *content = "return [1, 2";
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
            "file:///tmp/zr_lsp_diagnostic_array_close_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_array_close");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            diagnostic->range.start.line == 0 &&
            diagnostic->range.start.character == 7 &&
            diagnostic->range.end.line == 0 &&
            diagnostic->range.end.character == 8 &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 0 &&
            fix->editRange.start.character == 12 &&
            fix->editRange.end.line == 0 &&
            fix->editRange.end.character == 12 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing ']'") == 0 &&
            strcmp(test_string_text(fix->editText), "]") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing ']'", "]")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing array close did not preserve its opener range and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_object_close(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing object close";
    const TZrChar *content = "return {a: 1";
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
            "file:///tmp/zr_lsp_diagnostic_object_close_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_object_close");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            diagnostic->range.start.line == 0 &&
            diagnostic->range.start.character == 7 &&
            diagnostic->range.end.line == 0 &&
            diagnostic->range.end.character == 8 &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 0 &&
            fix->editRange.start.character == 12 &&
            fix->editRange.end.line == 0 &&
            fix->editRange.end.character == 12 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing '}'") == 0 &&
            strcmp(test_string_text(fix->editText), "}") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing '}'", "}")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing object close did not preserve its opener range and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_object_computed_key_close(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing object computed-key close";
    const TZrChar *content = "return {a: 0, [1: 2};";
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
            "file:///tmp/zr_lsp_diagnostic_object_computed_key_close_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_object_computed_key_close");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            diagnostic->range.start.line == 0 &&
            diagnostic->range.start.character == 14 &&
            diagnostic->range.end.line == 0 &&
            diagnostic->range.end.character == 15 &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 0 &&
            fix->editRange.start.character == 16 &&
            fix->editRange.end.line == 0 &&
            fix->editRange.end.character == 16 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing ']'") == 0 &&
            strcmp(test_string_text(fix->editText), "]") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing ']'", "]")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing object computed-key close did not preserve its opener range and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_object_property_colon(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing object property colon";
    const TZrChar *content = "return {a: 0, b 1};";
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
            "file:///tmp/zr_lsp_diagnostic_object_property_colon_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_object_property_colon");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            diagnostic->range.start.line == 0 &&
            diagnostic->range.start.character == 16 &&
            diagnostic->range.end.line == 0 &&
            diagnostic->range.end.character == 17 &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 0 &&
            fix->editRange.start.character == 16 &&
            fix->editRange.end.line == 0 &&
            fix->editRange.end.character == 16 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing ':'") == 0 &&
            strcmp(test_string_text(fix->editText), ":") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing ':'", ":")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing object property colon did not preserve the value-token range and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_object_property_separator(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing object property separator";
    const TZrChar *content = "return {a: 1 b: 2};";
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
            "file:///tmp/zr_lsp_diagnostic_object_property_separator_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_object_property_separator");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            diagnostic->range.start.line == 0 &&
            diagnostic->range.start.character == 13 &&
            diagnostic->range.end.line == 0 &&
            diagnostic->range.end.character == 14 &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 0 &&
            fix->editRange.start.character == 13 &&
            fix->editRange.end.line == 0 &&
            fix->editRange.end.character == 13 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing ','") == 0 &&
            strcmp(test_string_text(fix->editText), ",") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing ','", ",")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing object property separator did not preserve the next-key range and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_conditional_colon(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing conditional colon";
    const TZrChar *content = "return true ? 1 2;";
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
            "file:///tmp/zr_lsp_diagnostic_conditional_colon_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_conditional_colon");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            diagnostic->range.start.line == 0 &&
            diagnostic->range.start.character == 12 &&
            diagnostic->range.end.line == 0 &&
            diagnostic->range.end.character == 13 &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 0 &&
            fix->editRange.start.character == 16 &&
            fix->editRange.end.line == 0 &&
            fix->editRange.end.character == 16 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing ':'") == 0 &&
            strcmp(test_string_text(fix->editText), ":") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing ':'", ":")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing conditional colon did not preserve the question-token range and drive one exact machine-applicable quick fix");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_skips_conditional_branch_expression_fixes(
        SZrState *state,
        int *failures) {
    static const struct {
        const TZrChar *uri;
        const TZrChar *content;
        const TZrChar *code;
    } cases[] = {
        {"file:///tmp/zr_lsp_diagnostic_conditional_consequent_fix.zr",
         "return true ? : 2;",
         "missing_conditional_consequent"},
        {"file:///tmp/zr_lsp_diagnostic_conditional_alternate_fix.zr",
         "return true ? 1 : ;",
         "missing_conditional_alternate"},
        {"file:///tmp/zr_lsp_diagnostic_conditional_without_alternate_fix.zr",
         "return true ? 1;",
         "missing_conditional_colon"},
    };
    const TZrChar *summary =
            "LSP code action skips conditional branch expression fixes";
    SZrTestTimer timer;
    TZrBool valid = ZR_TRUE;

    TEST_START(summary);
    for (TZrSize index = 0U;
         index < sizeof(cases) / sizeof(cases[0]);
         index++) {
        SZrLspContext *context;
        SZrString *uri = ZR_NULL;
        SZrArray diagnostics = {0};
        SZrArray actions = {0};
        const SZrLspDiagnostic *diagnostic = ZR_NULL;

        context = test_open_document(
                state, cases[index].uri, cases[index].content, &uri);
        ZrCore_Array_Init(
                state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
        if (context == ZR_NULL ||
            !ZrLanguageServer_Lsp_GetDiagnostics(
                    state, context, uri, &diagnostics)) {
            valid = ZR_FALSE;
        } else {
            diagnostic = diagnostic_safe_fix_find_code(
                    &diagnostics, cases[index].code);
            if (diagnostic == ZR_NULL ||
                (diagnostic->fixes.isValid &&
                 diagnostic->fixes.length != 0U) ||
                !ZrLanguageServer_Lsp_GetCodeActions(
                        state,
                        context,
                        uri,
                        diagnostic->range,
                        &actions) ||
                diagnostic_safe_fix_action_matches(
                        &actions, "Insert missing ':'", ":")) {
                valid = ZR_FALSE;
            }
        }

        ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
        ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "conditional consequent or alternate expression diagnostics exposed a machine-applicable punctuation fix");
    } else {
        TEST_PASS(timer, summary);
    }
}

static void test_lsp_code_action_inserts_missing_using_object_pattern_close(
        SZrState *state,
        int *failures) {
    const TZrChar *summary =
            "LSP code action inserts missing using object pattern close";
    const TZrChar *content = "using (var {value,";
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
            "file:///tmp/zr_lsp_diagnostic_using_object_close_fix.zr",
            content,
            &uri);
    ZrCore_Array_Init(
            state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        diagnostic = diagnostic_safe_fix_find_code(
                &diagnostics, "missing_object_close");
        if (diagnostic != ZR_NULL && diagnostic->fixes.isValid &&
            diagnostic->fixes.length == 1U) {
            fix = (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostic->fixes, 0U);
        }
        if (fix != ZR_NULL &&
            diagnostic->range.start.line == 0 &&
            diagnostic->range.start.character == 11 &&
            diagnostic->range.end.line == 0 &&
            diagnostic->range.end.character == 12 &&
            fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editRange.start.line == 0 &&
            fix->editRange.start.character == 18 &&
            fix->editRange.end.line == 0 &&
            fix->editRange.end.character == 18 &&
            strcmp(test_string_text(fix->title),
                   "Insert missing '}'") == 0 &&
            strcmp(test_string_text(fix->editText), "}") == 0 &&
            ZrLanguageServer_Lsp_GetCodeActions(
                    state,
                    context,
                    uri,
                    diagnostic->range,
                    &actions) &&
            diagnostic_safe_fix_action_matches(
                    &actions, "Insert missing '}'", "}")) {
            valid = ZR_TRUE;
        }
    }

    if (!valid) {
        (*failures)++;
        TEST_FAIL(
                timer,
                summary,
                "missing using object pattern close did not preserve its opener range and drive one exact machine-applicable quick fix");
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
