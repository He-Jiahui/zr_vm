#ifndef ZR_VM_TEST_LSP_TEMPLATE_FORMATTING_CASES_H
#define ZR_VM_TEST_LSP_TEMPLATE_FORMATTING_CASES_H

/* 由 advanced_editor_features.c 的 main 内联调用：多行反引号模板与块注释内部的行
 * 属于字面文本，格式化不得剪掉首尾空白或重加缩进；模板前后的代码行照常缩进。 */

/* 用整篇或区间格式化取得唯一编辑文本并与期望逐字节比较；不一致时打印实际文本便于诊断。 */
static TZrBool test_lsp_template_format_matches(SZrState *state,
                                                const TZrChar *uriText,
                                                const TZrChar *content,
                                                const SZrLspFormattingOptions *options,
                                                const TZrChar *expected) {
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray edits = {0};
    const TZrChar *formatted = ZR_NULL;
    TZrBool matches = ZR_FALSE;

    context = test_open_document(state, uriText, content, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetFormatting(state, context, uri, options, &edits)) {
        formatted = first_text_edit_text(&edits);
        matches = formatted != ZR_NULL && strcmp(formatted, expected) == 0;
        if (!matches) {
            printf("  expected:\n%s\n  actual:\n%s\n", expected, formatted != ZR_NULL ? formatted : "(null)");
        }
    }

    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    return matches;
}

/* 1+2：模板内部行（含首尾空白与纯空白行）原样保留，首行模板文本的尾随空白不被剪掉，
 * 模板外的代码行仍重新缩进。 */
static void test_lsp_formatting_preserves_multiline_template_bytes(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting preserves multi-line template bytes";
    const TZrChar *content =
        "fn main(): int {\n"
        "var t = `line one   \n"
        "   indented  \n"
        "\t\n"
        "end`;\n"
        "return 0;\n"
        "}\n";
    const TZrChar *expected =
        "fn main(): int {\n"
        "    var t = `line one   \n"
        "   indented  \n"
        "\t\n"
        "end`;\n"
        "    return 0;\n"
        "}\n";

    TEST_START(summary);
    if (!test_lsp_template_format_matches(state,
                                          "file:///tmp/zr_lsp_format_template_bytes.zr",
                                          content,
                                          ZR_NULL,
                                          expected)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "formatting changed bytes inside a multi-line template");
    } else {
        TEST_PASS(timer, summary);
    }
}

/* 3：块注释首行（从代码态开始）照常缩进，其后的内部行原样保留。 */
static void test_lsp_formatting_preserves_block_comment_interior(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting preserves block comment interior lines";
    const TZrChar *content =
        "fn main(): int {\n"
        "/**\n"
        " * Doc line   \n"
        " *   nested\n"
        " */\n"
        "return 0;\n"
        "}\n";
    const TZrChar *expected =
        "fn main(): int {\n"
        "    /**\n"
        " * Doc line   \n"
        " *   nested\n"
        " */\n"
        "    return 0;\n"
        "}\n";

    TEST_START(summary);
    if (!test_lsp_template_format_matches(state,
                                          "file:///tmp/zr_lsp_format_block_comment.zr",
                                          content,
                                          ZR_NULL,
                                          expected)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "formatting re-indented block comment interior lines");
    } else {
        TEST_PASS(timer, summary);
    }
}

/* 4：模板内部行首的 `}` 是文本，不得降级缩进，模板之后的语句缩进保持正确。 */
static void test_lsp_formatting_template_leading_brace_is_text(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting treats a leading template brace as text";
    const TZrChar *content =
        "fn main(): int {\n"
        "if (true) {\n"
        "var t = `a\n"
        "} not a brace\n"
        "`;\n"
        "return 1;\n"
        "}\n"
        "return 0;\n"
        "}\n";
    const TZrChar *expected =
        "fn main(): int {\n"
        "    if (true) {\n"
        "        var t = `a\n"
        "} not a brace\n"
        "`;\n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    TEST_START(summary);
    if (!test_lsp_template_format_matches(state,
                                          "file:///tmp/zr_lsp_format_template_brace.zr",
                                          content,
                                          ZR_NULL,
                                          expected)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "a template-interior brace changed indentation or bytes");
    } else {
        TEST_PASS(timer, summary);
    }
}

/* 5：多行插值里的代码行照常缩进，模板字面文本行原样保留，模板后的语句缩进正确。 */
static void test_lsp_formatting_template_interpolation_keeps_literal_lines(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting keeps template literal lines around interpolation";
    const TZrChar *content =
        "fn main(): int {\n"
        "var t = `head\n"
        "  literal  ${\n"
        "call({\n"
        "a: 1\n"
        "})\n"
        "}  tail  \n"
        "more`;\n"
        "return 0;\n"
        "}\n";
    const TZrChar *expected =
        "fn main(): int {\n"
        "    var t = `head\n"
        "  literal  ${\n"
        "    call({\n"
        "        a: 1\n"
        "    })\n"
        "    }  tail  \n"
        "more`;\n"
        "    return 0;\n"
        "}\n";

    TEST_START(summary);
    if (!test_lsp_template_format_matches(state,
                                          "file:///tmp/zr_lsp_format_template_interpolation.zr",
                                          content,
                                          ZR_NULL,
                                          expected)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "interpolation formatting changed template literal lines");
    } else {
        TEST_PASS(timer, summary);
    }
}

/* 6：区间从模板内部行开始时，该行不产生任何改变（扫描状态从文档开头交接）。 */
static void test_lsp_range_formatting_inside_template_is_noop(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP range formatting inside a template changes nothing";
    const TZrChar *content =
        "fn main(): int {\n"
        "    var t = `line one\n"
        "   indented  \n"
        "end`;\n"
        "    return 0;\n"
        "}\n";
    SZrLspRange range = {{2, 0}, {3, 0}};
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray edits = {0};
    const TZrChar *formatted = ZR_NULL;
    TZrBool ok = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_range_format_template.zr", content, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetRangeFormatting(state, context, uri, range, ZR_NULL, &edits)) {
        formatted = edits.length > 0 ? first_text_edit_text(&edits) : ZR_NULL;
        ok = edits.length == 0 ||
             (formatted != ZR_NULL && strcmp(formatted, "   indented  \n") == 0);
        if (!ok) {
            printf("  range edit:\n%s\n", formatted != ZR_NULL ? formatted : "(null)");
        }
    }
    if (!ok) {
        (*failures)++;
        TEST_FAIL(timer, summary, "range formatting changed a template-interior line");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

/* 7：使用制表符缩进时模板内部行仍逐字节不变，不注入制表符。 */
static void test_lsp_formatting_template_bytes_with_tabs(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting keeps template bytes with tab indentation";
    SZrLspFormattingOptions tabs = {4, ZR_FALSE};
    const TZrChar *content =
        "fn main(): int {\n"
        "var t = `line one\n"
        "   indented  \n"
        "end`;\n"
        "return 0;\n"
        "}\n";
    const TZrChar *expected =
        "fn main(): int {\n"
        "\tvar t = `line one\n"
        "   indented  \n"
        "end`;\n"
        "\treturn 0;\n"
        "}\n";

    TEST_START(summary);
    if (!test_lsp_template_format_matches(state,
                                          "file:///tmp/zr_lsp_format_template_tabs.zr",
                                          content,
                                          &tabs,
                                          expected)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "tab formatting changed template-interior bytes");
    } else {
        TEST_PASS(timer, summary);
    }
}

/* 8：CRLF 文档保持 CRLF，模板内部行（含尾随空白）原样保留。 */
static void test_lsp_formatting_template_bytes_with_crlf(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting keeps template bytes and CRLF";
    const TZrChar *content =
        "fn main(): int {\r\n"
        "var t = `line one   \r\n"
        "   indented  \r\n"
        "end`;\r\n"
        "return 0;\r\n"
        "}\r\n";
    const TZrChar *expected =
        "fn main(): int {\r\n"
        "    var t = `line one   \r\n"
        "   indented  \r\n"
        "end`;\r\n"
        "    return 0;\r\n"
        "}\r\n";

    TEST_START(summary);
    if (!test_lsp_template_format_matches(state,
                                          "file:///tmp/zr_lsp_format_template_crlf.zr",
                                          content,
                                          ZR_NULL,
                                          expected)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "CRLF formatting changed template-interior bytes");
    } else {
        TEST_PASS(timer, summary);
    }
}

#endif
