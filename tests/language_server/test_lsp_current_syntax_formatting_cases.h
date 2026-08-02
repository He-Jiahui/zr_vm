#ifndef ZR_VM_TEST_LSP_CURRENT_SYNTAX_FORMATTING_CASES_H
#define ZR_VM_TEST_LSP_CURRENT_SYNTAX_FORMATTING_CASES_H

static void test_lsp_formatting_does_not_emit_removed_syntax(
        SZrState *state,
        int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary =
            "LSP formatting does not emit removed syntax";
    const TZrChar *content =
            "%compileTime fn derive(value: int): int {\n"
            "return value;\n"
            "}\n"
            "let callback: %func(int) => int;\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray documentEdits = {0};
    SZrArray rangeEdits = {0};
    SZrLspRange range = {{0, 0}, {3, 0}};

    TEST_START(summary);
    context = test_open_document(
            state,
            "file:///tmp/zr_lsp_format_removed_syntax.zr",
            content,
            &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFormatting(
                state, context, uri, &documentEdits) ||
        !ZrLanguageServer_Lsp_GetRangeFormatting(
                state, context, uri, range, &rangeEdits)) {
        (*failures)++;
        TEST_FAIL(timer,
                  summary,
                  "formatting failed while rejecting removed syntax");
    } else if (documentEdits.length != 0U || rangeEdits.length != 0U) {
        (*failures)++;
        TEST_FAIL(timer,
                  summary,
                  "formatter produced an edit containing removed syntax");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeTextEdits(state, &documentEdits);
    ZrLanguageServer_Lsp_FreeTextEdits(state, &rangeEdits);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_formatting_preserves_current_compile_tool_syntax(
        SZrState *state,
        int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary =
            "LSP formatting preserves current CompileTool syntax";
    const TZrChar *content =
            "#zr.compile.declarationTransform#\n"
            "comptime fn derive(value: int): int {\n"
            "let tool = import(\"@derive\");\n"
            "var remainder = value % 3;\n"
            "var compact = remainder%value;\n"
            "var leftSpaced = remainder %value;\n"
            "remainder %= 2;\n"
            "return remainder;\n"
            "}\n";
    const TZrChar *expected =
            "#zr.compile.declarationTransform#\n"
            "comptime fn derive(value: int): int {\n"
            "    let tool = import(\"@derive\");\n"
            "    var remainder = value % 3;\n"
            "    var compact = remainder%value;\n"
            "    var leftSpaced = remainder %value;\n"
            "    remainder %= 2;\n"
            "    return remainder;\n"
            "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray edits = {0};
    const TZrChar *formatted;

    TEST_START(summary);
    context = test_open_document(
            state,
            "file:///tmp/zr_lsp_format_current_compile_tool.zr",
            content,
            &uri);
    formatted = context != ZR_NULL &&
                        ZrLanguageServer_Lsp_GetFormatting(
                                state, context, uri, &edits)
                        ? first_text_edit_text(&edits)
                        : ZR_NULL;
    if (formatted == ZR_NULL || strcmp(formatted, expected) != 0 ||
        strstr(formatted, "%compileTime") != ZR_NULL ||
        strstr(formatted, "%func") != ZR_NULL ||
        strstr(formatted, "%import") != ZR_NULL) {
        (*failures)++;
        TEST_FAIL(timer,
                  summary,
                  "formatter did not emit the canonical CompileTool surface");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#endif
