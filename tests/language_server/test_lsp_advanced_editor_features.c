//
// Advanced editor feature coverage for LSP protocol parity.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

/* TEST_START/PASS/FAIL 只报告单例用时；失败计数由各测试函数写回 main。 */
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while (0)

#define TEST_PASS(timer, summary) do { \
    timer.endTime = clock(); \
    printf("Pass - Cost Time:%.3fms - %s\n", \
           ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary); \
    fflush(stdout); \
} while (0)

#define TEST_FAIL(timer, summary, reason) do { \
    timer.endTime = clock(); \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", \
           ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary, \
           reason); \
    fflush(stdout); \
} while (0)

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        free(pointer);
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    return realloc(pointer, newSize);
}

static const TZrChar *test_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

/* 用首个 needle 匹配定位请求光标；本文件的定位样例均为 ASCII，
 * 因而字节列与 UTF-16 列相同，非 ASCII 位置由专门的 UTF-16 测试覆盖。 */
static TZrBool test_find_position(const TZrChar *content,
                                  const TZrChar *needle,
                                  TZrInt32 extraCharacterOffset,
                                  SZrLspPosition *outPosition) {
    const TZrChar *match;
    TZrInt32 line = 0;
    TZrInt32 character = 0;

    if (content == ZR_NULL || needle == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    if (match == ZR_NULL) {
        return ZR_FALSE;
    }

    for (const TZrChar *cursor = content; cursor < match; cursor++) {
        if (*cursor == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
    }

    outPosition->line = line;
    outPosition->character = character + extraCharacterOffset;
    return ZR_TRUE;
}

/* 每例创建隔离的 LSP 上下文和 URI；成功后由调用方释放 context，
 * URI 随 VM 状态存活，不能在释放 state 后使用。 */
static SZrLspContext *test_open_document(SZrState *state,
                                         const TZrChar *uriText,
                                         const TZrChar *content,
                                         SZrString **outUri) {
    SZrLspContext *context;
    SZrString *uri;

    if (state == ZR_NULL || uriText == ZR_NULL || content == ZR_NULL || outUri == ZR_NULL) {
        return ZR_NULL;
    }

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        return ZR_NULL;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        return ZR_NULL;
    }

    *outUri = uri;
    return context;
}

/* CodeAction/格式化用例只关心候选编辑是否包含预期文本，不依赖列表顺序。 */
static TZrBool text_edit_contains(SZrArray *edits, const TZrChar *needle) {
    for (TZrSize index = 0; edits != ZR_NULL && index < edits->length; index++) {
        SZrLspTextEdit **editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(edits, index);
        const TZrChar *text = (editPtr != ZR_NULL && *editPtr != ZR_NULL)
                                  ? test_string_text((*editPtr)->newText)
                                  : ZR_NULL;
        if (text != ZR_NULL && strstr(text, needle) != ZR_NULL) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static const TZrChar *first_text_edit_text(SZrArray *edits) {
    SZrLspTextEdit **editPtr;

    if (edits == ZR_NULL || edits->length == 0) {
        return ZR_NULL;
    }

    editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(edits, 0);
    return editPtr != ZR_NULL && *editPtr != ZR_NULL ? test_string_text((*editPtr)->newText) : ZR_NULL;
}

static SZrLspTextEdit *first_text_edit(SZrArray *edits) {
    SZrLspTextEdit **editPtr;

    if (edits == ZR_NULL || !edits->isValid || edits->length == 0) {
        return ZR_NULL;
    }
    editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(edits, 0);
    return editPtr != ZR_NULL ? *editPtr : ZR_NULL;
}

/* CRLF 文档里的 \\n 必须带着前面的 \\r；单独的 LF 说明换行被重写了。 */
static TZrBool text_contains_bare_lf(const TZrChar *text) {
    const TZrChar *cursor;

    if (text == ZR_NULL) {
        return ZR_FALSE;
    }
    for (cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor == '\n' && (cursor == text || cursor[-1] != '\r')) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_lsp_document_formatting_returns_single_document_edit(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP document formatting returns a full-document edit";
    const TZrChar *content =
        "class Sample {\n"
        "pub fn run(value: int): int {\n"
        "let local = value;\n"
        "return local;\n"
        "}\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray edits = {0};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_format.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &edits) ||
        edits.length != 1 ||
        !text_edit_contains(&edits, "    pub fn run") ||
        !text_edit_contains(&edits, "        return local;")) {
        (*failures)++;
        TEST_FAIL(timer, summary, "formatting did not return the expected indented full-document edit");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_formatting_skips_noop_edits(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting skips no-op edits";
    const TZrChar *content =
        "class Sample {\n"
        "    pub fn run(value: int): int {\n"
        "        return value;\n"
        "    }\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray documentEdits = {0};
    SZrArray rangeEdits = {0};
    SZrLspRange range = {{1, 0}, {3, 0}};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_format_noop.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &documentEdits) ||
        !ZrLanguageServer_Lsp_GetRangeFormatting(state, context, uri, range, ZR_NULL, &rangeEdits)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "formatting failed for already formatted content");
    } else if (documentEdits.length != 0 || rangeEdits.length != 0) {
        (*failures)++;
        TEST_FAIL(timer, summary, "already formatted content produced a no-op edit");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeTextEdits(state, &documentEdits);
    ZrLanguageServer_Lsp_FreeTextEdits(state, &rangeEdits);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_folding_and_selection_ranges(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP folding and selection ranges use source structure";
    const TZrChar *content =
        "class Sample {\n"
        "    pub fn run(value: int): int {\n"
        "        let local = value;\n"
        "        return local;\n"
        "    }\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray folds = {0};
    SZrArray selections = {0};
    SZrLspPosition position;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_folding.zr", content, &uri);
    if (context == ZR_NULL ||
        !test_find_position(content, "local;", 1, &position) ||
        !ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds) ||
        folds.length < 2 ||
        !ZrLanguageServer_Lsp_GetSelectionRanges(state, context, uri, &position, 1, &selections) ||
        selections.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "folding or selection range provider returned incomplete results");
    } else {
        SZrLspSelectionRange **selectionPtr =
            (SZrLspSelectionRange **)ZrCore_Array_Get(&selections, 0);
        if (selectionPtr == ZR_NULL ||
            *selectionPtr == ZR_NULL ||
            (*selectionPtr)->range.start.line != 3 ||
            (*selectionPtr)->range.start.character >= (*selectionPtr)->range.end.character ||
            !(*selectionPtr)->hasParent ||
            !(*selectionPtr)->hasGrandParent ||
            (*selectionPtr)->grandParentRange.start.line != 1 ||
            (*selectionPtr)->grandParentRange.end.line != 4) {
            (*failures)++;
            TEST_FAIL(timer, summary, "selection range did not include word, line, and block ranges");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
    ZrLanguageServer_Lsp_FreeSelectionRanges(state, &selections);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_folding_ranges_include_import_regions(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP folding ranges include import regions";
    const TZrChar *content =
        "let system = import(\"zr.system\");\n"
        "let math = import(\"zr.math\");\n"
        "let container = import(\"zr.container\");\n"
        "\n"
        "// first note\n"
        "// second note\n"
        "\n"
        "//#region setup\n"
        "fn main(): int {\n"
        "    return 0;\n"
        "}\n"
        "//#endregion\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray folds = {0};
    TZrBool foundImportRange = ZR_FALSE;
    TZrBool foundCommentRange = ZR_FALSE;
    TZrBool foundRegionRange = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_import_folding.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "folding range provider failed");
    } else {
        for (TZrSize index = 0; index < folds.length; index++) {
            SZrLspFoldingRange **rangePtr = (SZrLspFoldingRange **)ZrCore_Array_Get(&folds, index);
            const TZrChar *kind = (rangePtr != ZR_NULL && *rangePtr != ZR_NULL)
                                      ? test_string_text((*rangePtr)->kind)
                                      : ZR_NULL;
            if (rangePtr != ZR_NULL &&
                *rangePtr != ZR_NULL &&
                (*rangePtr)->startLine == 0 &&
                (*rangePtr)->endLine == 2 &&
                kind != ZR_NULL &&
                strcmp(kind, "imports") == 0) {
                foundImportRange = ZR_TRUE;
            }
            if (rangePtr != ZR_NULL &&
                *rangePtr != ZR_NULL &&
                (*rangePtr)->startLine == 4 &&
                (*rangePtr)->endLine == 5 &&
                kind != ZR_NULL &&
                strcmp(kind, "comment") == 0) {
                foundCommentRange = ZR_TRUE;
            }
            if (rangePtr != ZR_NULL &&
                *rangePtr != ZR_NULL &&
                (*rangePtr)->startLine == 7 &&
                (*rangePtr)->endLine == 11 &&
                kind != ZR_NULL &&
                strcmp(kind, "region") == 0) {
                foundRegionRange = ZR_TRUE;
            }
        }

        if (!foundImportRange || !foundCommentRange || !foundRegionRange) {
            (*failures)++;
            TEST_FAIL(timer, summary, "expected folding ranges for imports, comments, and explicit regions");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_formatting_ignores_non_code_braces(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting ignores braces in strings and comments";
    const TZrChar *content =
        "fn render(): string {\n"
        "let text = \"{\";\n"
        "// }\n"
        "/* { */\n"
        "return text;\n"
        "}\n";
    const TZrChar *expected =
        "fn render(): string {\n"
        "    let text = \"{\";\n"
        "    // }\n"
        "    /* { */\n"
        "    return text;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray edits = {0};
    const TZrChar *formatted;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_format_non_code_braces.zr", content, &uri);
    formatted = context != ZR_NULL &&
                ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &edits)
                    ? first_text_edit_text(&edits)
                    : ZR_NULL;
    if (formatted == ZR_NULL || strcmp(formatted, expected) != 0) {
        (*failures)++;
        TEST_FAIL(timer, summary, "formatting treated string/comment braces as structural braces");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_folding_ignores_non_code_braces(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP folding ignores braces in strings and comments";
    const TZrChar *content =
        "fn render(): string {\n"
        "    let text = \"{\";\n"
        "    // }\n"
        "    /* { */\n"
        "    return text;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray folds = {0};
    TZrBool foundFunctionRange = ZR_FALSE;
    TZrBool foundSpuriousRange = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_fold_non_code_braces.zr", content, &uri);
    if (context == ZR_NULL || !ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "folding range provider failed");
    } else {
        for (TZrSize index = 0; index < folds.length; index++) {
            SZrLspFoldingRange **rangePtr = (SZrLspFoldingRange **)ZrCore_Array_Get(&folds, index);
            if (rangePtr == ZR_NULL || *rangePtr == ZR_NULL) {
                continue;
            }
            if ((*rangePtr)->startLine == 0 && (*rangePtr)->endLine == 5) {
                foundFunctionRange = ZR_TRUE;
            }
            if ((*rangePtr)->startLine == 1 || (*rangePtr)->startLine == 3) {
                foundSpuriousRange = ZR_TRUE;
            }
        }

        if (!foundFunctionRange || foundSpuriousRange) {
            (*failures)++;
            TEST_FAIL(timer, summary, "folding created ranges from non-code braces");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_document_links_resolve_import_literals(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP document links resolve import literals";
    const TZrChar *content = "import(\"zr.math\");\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray links = {0};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_links.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uri, &links) ||
        links.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "documentLink did not return a target for the import literal");
    } else {
        SZrLspDocumentLink **linkPtr = (SZrLspDocumentLink **)ZrCore_Array_Get(&links, 0);
        const TZrChar *target = (linkPtr != ZR_NULL && *linkPtr != ZR_NULL)
                                    ? test_string_text((*linkPtr)->target)
                                    : ZR_NULL;
        if (target == ZR_NULL || strstr(target, "zr.math") == ZR_NULL) {
            (*failures)++;
            TEST_FAIL(timer, summary, "documentLink target did not point at the resolved native module");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &links);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_document_links_ignore_non_code_import_text(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP document links ignore import text in strings and comments";
    const TZrChar *content =
        "var text = \"import(\\\"zr.system\\\")\";\n"
        "// import(\"zr.network\");\n"
        "/*\n"
        "import(\"zr.container\");\n"
        "*/\n"
        "import(\"zr.math\");\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray links = {0};
    const TZrChar *target = ZR_NULL;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_links_non_code_imports.zr", content, &uri);
    if (context == ZR_NULL || !ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uri, &links)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "documentLink failed for non-code import text");
    } else if (links.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "documentLink produced links for non-code import text");
    } else {
        SZrLspDocumentLink **linkPtr = (SZrLspDocumentLink **)ZrCore_Array_Get(&links, 0);
        target = linkPtr != ZR_NULL && *linkPtr != ZR_NULL ? test_string_text((*linkPtr)->target) : ZR_NULL;
        if (target == ZR_NULL || strstr(target, "zr.math") == ZR_NULL) {
            (*failures)++;
            TEST_FAIL(timer, summary, "documentLink did not keep the real import target");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &links);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_document_links_resolve_zrp_paths(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP document links resolve zrp project paths";
    const TZrChar *content =
        "{\n"
        "  \"name\": \"demo\",\n"
        "  \"source\": \"src\",\n"
        "  \"binary\": \"bin\",\n"
        "  \"entry\": \"main\",\n"
        "  \"dependency\": \"deps\",\n"
        "  \"local\": \"local_modules\"\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray links = {0};
    TZrBool hasSource = ZR_FALSE;
    TZrBool hasBinary = ZR_FALSE;
    TZrBool hasEntry = ZR_FALSE;
    TZrBool hasDependency = ZR_FALSE;
    TZrBool hasLocal = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_project/game.zrp", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uri, &links) ||
        links.length < 5) {
        (*failures)++;
        TEST_FAIL(timer, summary, "documentLink did not return all zrp path links");
    } else {
        for (TZrSize index = 0; index < links.length; index++) {
            SZrLspDocumentLink **linkPtr = (SZrLspDocumentLink **)ZrCore_Array_Get(&links, index);
            const TZrChar *target = (linkPtr != ZR_NULL && *linkPtr != ZR_NULL)
                                        ? test_string_text((*linkPtr)->target)
                                        : ZR_NULL;
            if (target == ZR_NULL) {
                continue;
            }
            hasSource = hasSource || strstr(target, "/src") != ZR_NULL;
            hasBinary = hasBinary || strstr(target, "/bin") != ZR_NULL;
            hasEntry = hasEntry || strstr(target, "/src/main.zr") != ZR_NULL;
            hasDependency = hasDependency || strstr(target, "/deps") != ZR_NULL;
            hasLocal = hasLocal || strstr(target, "/local_modules") != ZR_NULL;
        }
        if (!hasSource || !hasBinary || !hasEntry || !hasDependency || !hasLocal) {
            (*failures)++;
            TEST_FAIL(timer, summary, "zrp document links did not target all project path fields");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &links);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

/* 先取得父模块的虚拟声明文档，再由文档链接 API 查询子模块；
 * 仅核对链接 target 是约定的虚拟声明 URI。 */
static void test_lsp_document_links_resolve_virtual_module_links(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP document links resolve virtual native module links";
    SZrLspContext *context;
    SZrString *uri;
    SZrString *documentText = ZR_NULL;
    const TZrChar *renderedText;
    SZrArray links;
    TZrBool foundTcpLink = ZR_FALSE;

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               (TZrNativeString)"zr-decompiled:/zr.network.zr",
                               strlen("zr-decompiled:/zr.network.zr"));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &documentText) ||
        documentText == ZR_NULL) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        (*failures)++;
        TEST_FAIL(timer, summary, "failed to render the zr.network virtual declaration document");
        return;
    }

    renderedText = test_string_text(documentText);
    if (renderedText == ZR_NULL ||
        strstr(renderedText, "pub module tcp: zr.network.tcp;") == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, renderedText, strlen(renderedText), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        (*failures)++;
        TEST_FAIL(timer, summary, "zr.network virtual declaration did not contain module link fixture text");
        return;
    }

    ZrCore_Array_Init(state, &links, sizeof(SZrLspDocumentLink *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uri, &links)) {
        ZrLanguageServer_Lsp_FreeDocumentLinks(state, &links);
        ZrLanguageServer_LspContext_Free(state, context);
        (*failures)++;
        TEST_FAIL(timer, summary, "documentLink request failed for virtual declaration URI");
        return;
    }

    for (TZrSize index = 0; index < links.length; index++) {
        SZrLspDocumentLink **linkPtr = (SZrLspDocumentLink **)ZrCore_Array_Get(&links, index);
        const TZrChar *target = (linkPtr != ZR_NULL && *linkPtr != ZR_NULL)
                                    ? test_string_text((*linkPtr)->target)
                                    : ZR_NULL;
        if (target != ZR_NULL && strcmp(target, "zr-decompiled:/zr.network.tcp.zr") == 0) {
            foundTcpLink = ZR_TRUE;
            break;
        }
    }

    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &links);
    ZrLanguageServer_LspContext_Free(state, context);

    if (!foundTcpLink) {
        (*failures)++;
        TEST_FAIL(timer, summary, "expected a DocumentLink from pub module tcp to zr.network.tcp");
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_code_action_organizes_imports(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action organizes import directives";
    const TZrChar *content =
        "let system = import(\"zr.system\");\n"
        "let math = import(\"zr.math\");\n"
        "let system = import(\"zr.system\");\n"
        "\n"
        "fn main(): int { return 0; }\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {4, 0}};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_actions.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions) ||
        actions.length == 0) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction did not return organize imports");
    } else {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, 0);
        const TZrChar *title = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                   ? test_string_text((*actionPtr)->title)
                                   : ZR_NULL;
        if (title == ZR_NULL ||
            strstr(title, "Organize") == ZR_NULL ||
            !text_edit_contains(&(*actionPtr)->edits,
                                "let math = import(\"zr.math\");\nlet system = import(\"zr.system\");")) {
            (*failures)++;
            TEST_FAIL(timer, summary, "organize imports action did not contain the sorted import edit");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_organizes_imports_after_module(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action organizes imports after module declaration";
    const TZrChar *content =
        "module demo;\n"
        "\n"
        "let system = import(\"zr.system\");\n"
        "let math = import(\"zr.math\");\n"
        "\n"
        "fn main(): int { return 0; }\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {5, 0}};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_actions_module.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions) ||
        actions.length == 0) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction did not organize imports after a module declaration");
    } else {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, 0);
        if (actionPtr == ZR_NULL ||
            *actionPtr == ZR_NULL ||
            !text_edit_contains(&(*actionPtr)->edits,
                                "let math = import(\"zr.math\");\nlet system = import(\"zr.system\");")) {
            (*failures)++;
            TEST_FAIL(timer, summary, "organize imports did not sort the module-local import block");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_organizes_import_aliases(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action organizes import aliases";
    const TZrChar *content =
        "var system = import(\"zr.system\");\n"
        "var math = import(\"zr.math\");\n"
        "var system = import(\"zr.system\");\n"
        "\n"
        "fn main(): int { return 0; }\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {4, 0}};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_alias_import_actions.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions) ||
        actions.length == 0) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction did not organize alias imports");
    } else {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, 0);
        if (actionPtr == ZR_NULL ||
            *actionPtr == ZR_NULL ||
            !text_edit_contains(&(*actionPtr)->edits,
                                "var math = import(\"zr.math\");\nvar system = import(\"zr.system\");")) {
            (*failures)++;
            TEST_FAIL(timer, summary, "organize imports did not sort and dedupe alias import lines");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_organizes_duplicate_alias_imports(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action organizes duplicate alias imports";
    const TZrChar *content =
        "var system = import(\"zr.system\");\n"
        "var math = import(\"zr.math\");\n"
        "var system = import(\"zr.network\");\n"
        "\n"
        "fn main(): int { return 0; }\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {4, 0}};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_duplicate_alias_import_actions.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions) ||
        actions.length == 0) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction did not organize duplicate alias imports");
    } else {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, 0);
        if (actionPtr == ZR_NULL ||
            *actionPtr == ZR_NULL ||
            !text_edit_contains(&(*actionPtr)->edits,
                                "var math = import(\"zr.math\");\nvar system = import(\"zr.system\");") ||
            text_edit_contains(&(*actionPtr)->edits, "zr.network")) {
            (*failures)++;
            TEST_FAIL(timer, summary, "organize imports did not keep the first duplicate alias import");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_does_not_organize_import_text_in_strings(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action does not organize import text inside strings";
    const TZrChar *content =
        "var system = import(\"zr.system\");\n"
        "var text = \"import(\\\"zr.math\\\")\";\n"
        "\n"
        "fn main(): int { return 0; }\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {3, 0}};
    TZrBool foundOrganizeAction = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_string_import_text_actions.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for string import text fixture");
    } else {
        for (TZrSize index = 0; index < actions.length; index++) {
            SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
            const TZrChar *kind = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                      ? test_string_text((*actionPtr)->kind)
                                      : ZR_NULL;
            if (kind != ZR_NULL && strcmp(kind, ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS) == 0) {
                foundOrganizeAction = ZR_TRUE;
                break;
            }
        }
        if (foundOrganizeAction) {
            (*failures)++;
            TEST_FAIL(timer, summary, "organize imports treated a string literal as an import line");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_skips_organized_imports(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action skips already organized imports";
    const TZrChar *content =
        "let math = import(\"zr.math\");\n"
        "let system = import(\"zr.system\");\n"
        "\n"
        "fn main(): int {\n"
        "    let activeMath = math;\n"
        "    let activeSystem = system;\n"
        "    return 0;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {3, 0}};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_actions_organized.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for organized imports");
    } else if (actions.length != 0) {
        (*failures)++;
        TEST_FAIL(timer, summary, "organized imports produced a no-op code action");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

/* 同时放置已使用的 math 和未使用的 system：清理动作只能删除后者。 */
static void test_lsp_code_action_removes_unused_alias_imports(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action removes unused alias imports";
    const TZrChar *content =
        "let math = import(\"zr.math\");\n"
        "let system = import(\"zr.system\");\n"
        "\n"
        "fn main(value: int): int {\n"
        "    return math.abs(value);\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {5, 0}};
    TZrBool foundCleanupAction = ZR_FALSE;
    TZrBool removedUsedImport = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_unused_alias_import_actions.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for unused alias import fixture");
    } else {
        for (TZrSize index = 0; index < actions.length; index++) {
            SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
            const TZrChar *kind = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                      ? test_string_text((*actionPtr)->kind)
                                      : ZR_NULL;
            SZrLspTextEdit **editPtr =
                (actionPtr != ZR_NULL && *actionPtr != ZR_NULL && (*actionPtr)->edits.length > 0)
                    ? (SZrLspTextEdit **)ZrCore_Array_Get(&(*actionPtr)->edits, 0)
                    : ZR_NULL;
            const TZrChar *newText = editPtr != ZR_NULL && *editPtr != ZR_NULL
                                         ? test_string_text((*editPtr)->newText)
                                         : ZR_NULL;
            if (kind != ZR_NULL &&
                strcmp(kind, ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED) == 0 &&
                editPtr != ZR_NULL &&
                *editPtr != ZR_NULL &&
                (*editPtr)->range.start.line == 0) {
                removedUsedImport = ZR_TRUE;
            }
            if (kind != ZR_NULL &&
                strcmp(kind, ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED) == 0 &&
                editPtr != ZR_NULL &&
                *editPtr != ZR_NULL &&
                newText != ZR_NULL &&
                strcmp(newText, "") == 0 &&
                (*editPtr)->range.start.line == 1 &&
                (*editPtr)->range.end.line == 2) {
                foundCleanupAction = ZR_TRUE;
                break;
            }
        }
        if (!foundCleanupAction || removedUsedImport) {
            (*failures)++;
            TEST_FAIL(timer,
                      summary,
                      removedUsedImport
                          ? "remove-unused source action deleted an alias import that is still used"
                          : "unused alias import did not produce a remove-unused source action");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_ignores_text_mentions_when_removing_unused_alias_imports(SZrState *state,
                                                                                         int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP remove-unused action ignores alias mentions in comments and strings";
    const TZrChar *content =
        "let math = import(\"zr.math\");\n"
        "let system = import(\"zr.system\");\n"
        "\n"
        "fn main(value: int): int {\n"
        "    // system.console\n"
        "    var text = \"system.console\";\n"
        "    /* system.print(value); */\n"
        "    return math.abs(value);\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {8, 0}};
    TZrBool foundCleanupAction = ZR_FALSE;
    TZrBool removedUsedImport = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_unused_alias_text_mentions.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for unused alias text mention fixture");
    } else {
        for (TZrSize index = 0; index < actions.length; index++) {
            SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
            const TZrChar *kind = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                      ? test_string_text((*actionPtr)->kind)
                                      : ZR_NULL;
            if (kind == ZR_NULL ||
                strcmp(kind, ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED) != 0) {
                continue;
            }
            for (TZrSize editIndex = 0; editIndex < (*actionPtr)->edits.length; editIndex++) {
                SZrLspTextEdit **editPtr =
                    (SZrLspTextEdit **)ZrCore_Array_Get(&(*actionPtr)->edits, editIndex);
                const TZrChar *newText = editPtr != ZR_NULL && *editPtr != ZR_NULL
                                             ? test_string_text((*editPtr)->newText)
                                             : ZR_NULL;
                if (editPtr == ZR_NULL || *editPtr == ZR_NULL || newText == ZR_NULL ||
                    strcmp(newText, "") != 0) {
                    continue;
                }
                if ((*editPtr)->range.start.line == 0) {
                    removedUsedImport = ZR_TRUE;
                }
                if ((*editPtr)->range.start.line == 1 && (*editPtr)->range.end.line == 2) {
                    foundCleanupAction = ZR_TRUE;
                }
            }
        }
        if (!foundCleanupAction || removedUsedImport) {
            (*failures)++;
            TEST_FAIL(timer,
                      summary,
                      removedUsedImport
                          ? "remove-unused source action deleted an alias import that is used in code"
                          : "comment/string alias mentions prevented unused import cleanup");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_semicolon(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action inserts a missing semicolon";
    const TZrChar *content = "var answer = 42\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {0, 0}};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_semicolon_action.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions) ||
        actions.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction did not return a semicolon quick fix");
    } else {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, 0);
        const TZrChar *title = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                   ? test_string_text((*actionPtr)->title)
                                   : ZR_NULL;
        const TZrChar *kind = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                  ? test_string_text((*actionPtr)->kind)
                                  : ZR_NULL;
        if (title == ZR_NULL ||
            strstr(title, "semicolon") == ZR_NULL ||
            kind == ZR_NULL ||
            strcmp(kind, ZR_LSP_CODE_ACTION_KIND_QUICK_FIX) != 0 ||
            !text_edit_contains(&(*actionPtr)->edits, ";")) {
            (*failures)++;
            TEST_FAIL(timer, summary, "semicolon quick fix did not contain the expected edit");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_semicolon_before_line_comment(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action inserts missing semicolon before line comment";
    const TZrChar *content = "return answer // keep note\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {0, 0}};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_semicolon_comment_action.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions) ||
        actions.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction did not return a semicolon quick fix for a commented line");
    } else {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, 0);
        SZrLspTextEdit **editPtr =
            (actionPtr != ZR_NULL && *actionPtr != ZR_NULL && (*actionPtr)->edits.length > 0)
                ? (SZrLspTextEdit **)ZrCore_Array_Get(&(*actionPtr)->edits, 0)
                : ZR_NULL;
        const TZrChar *newText = (editPtr != ZR_NULL && *editPtr != ZR_NULL)
                                     ? test_string_text((*editPtr)->newText)
                                     : ZR_NULL;
        if (newText == ZR_NULL ||
            strcmp(newText, ";") != 0 ||
            (*editPtr)->range.start.line != 0 ||
            (*editPtr)->range.start.character != 13) {
            (*failures)++;
            TEST_FAIL(timer, summary, "semicolon quick fix was not inserted before the line comment");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_skips_semicolon_inside_block_comment(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action skips semicolon quickfix inside block comment";
    const TZrChar *content =
        "/*\n"
        "return answer\n"
        "*/\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{1, 0}, {1, 0}};
    TZrBool foundSemicolonAction = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_semicolon_block_comment_action.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for block-comment semicolon fixture");
    } else {
        for (TZrSize index = 0; index < actions.length; index++) {
            SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
            const TZrChar *title = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                       ? test_string_text((*actionPtr)->title)
                                       : ZR_NULL;
            if (title != ZR_NULL && strstr(title, "semicolon") != ZR_NULL) {
                foundSemicolonAction = ZR_TRUE;
                break;
            }
        }
        if (foundSemicolonAction) {
            (*failures)++;
            TEST_FAIL(timer, summary, "semicolon quick fix was offered inside a block comment");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_inserts_missing_native_import(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action inserts a missing native module import";
    const TZrChar *content =
        "fn main(value: int): int {\n"
        "    return math.abs(value);\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{1, 11}, {1, 15}};
    TZrBool foundImportAction = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_missing_native_import_action.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for a missing native module alias");
    } else {
        for (TZrSize index = 0; index < actions.length; index++) {
            SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
            const TZrChar *title = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                       ? test_string_text((*actionPtr)->title)
                                       : ZR_NULL;
            if (title != ZR_NULL &&
                strcmp(title, "Import zr.math as math") == 0 &&
                text_edit_contains(&(*actionPtr)->edits, "let math = import(\"zr.math\");\n")) {
                foundImportAction = ZR_TRUE;
                break;
            }
        }
        if (!foundImportAction) {
            (*failures)++;
            TEST_FAIL(timer, summary, "missing native module alias did not produce an import quick fix");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

/* 同一文档有多个候选诊断时，CodeAction 应只响应客户端请求范围。 */
static void test_lsp_code_action_uses_requested_range_for_missing_import(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action uses requested range for missing import";
    const TZrChar *content =
        "fn main(value: int): int {\n"
        "    return system.print(math.abs(value));\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{1, 24}, {1, 28}};
    TZrBool foundMathImportAction = ZR_FALSE;
    TZrBool foundSystemImportAction = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_range_missing_import_action.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for range-specific missing imports");
    } else {
        for (TZrSize index = 0; index < actions.length; index++) {
            SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
            const TZrChar *title = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                       ? test_string_text((*actionPtr)->title)
                                       : ZR_NULL;
            if (title != ZR_NULL && strcmp(title, "Import zr.math as math") == 0) {
                foundMathImportAction = text_edit_contains(&(*actionPtr)->edits,
                                                           "let math = import(\"zr.math\");\n");
            }
            if (title != ZR_NULL && strcmp(title, "Import zr.system as system") == 0) {
                foundSystemImportAction = ZR_TRUE;
            }
        }
        if (!foundMathImportAction || foundSystemImportAction) {
            (*failures)++;
            TEST_FAIL(timer, summary, "missing import quick fix ignored the requested identifier range");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_skips_existing_import_alias(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action skips missing import when alias already exists";
    const TZrChar *content =
        "var math = import(\"zr.math\");\n"
        "\n"
        "fn main(value: int): int {\n"
        "    return math.abs(value);\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{3, 11}, {3, 15}};
    TZrBool foundImportAction = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_existing_native_import_action.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for an existing native module alias");
    } else {
        for (TZrSize index = 0; index < actions.length; index++) {
            SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
            const TZrChar *title = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                       ? test_string_text((*actionPtr)->title)
                                       : ZR_NULL;
            if (title != ZR_NULL && strstr(title, "Import zr.math as math") != ZR_NULL) {
                foundImportAction = ZR_TRUE;
                break;
            }
        }
        if (foundImportAction) {
            (*failures)++;
            TEST_FAIL(timer, summary, "existing import alias produced a duplicate import quick fix");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_ignores_non_code_missing_import_text(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action ignores missing imports in strings and comments";
    const TZrChar *content =
        "fn main(value: int): int {\n"
        "    // math.abs(value);\n"
        "    var text = \"math.abs\";\n"
        "    /* math.abs(value); */\n"
        "    return value;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray commentActions = {0};
    SZrArray stringActions = {0};
    SZrArray blockCommentActions = {0};
    SZrLspRange commentRange = {{1, 7}, {1, 11}};
    SZrLspRange stringRange = {{2, 16}, {2, 20}};
    SZrLspRange blockCommentRange = {{3, 8}, {3, 12}};
    TZrBool foundImportAction = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_non_code_missing_import_action.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, commentRange, &commentActions) ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, stringRange, &stringActions) ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, blockCommentRange, &blockCommentActions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for non-code missing import probes");
    } else {
        SZrArray *actionLists[] = {&commentActions, &stringActions, &blockCommentActions};
        for (TZrSize listIndex = 0; listIndex < sizeof(actionLists) / sizeof(actionLists[0]); listIndex++) {
            for (TZrSize actionIndex = 0; actionIndex < actionLists[listIndex]->length; actionIndex++) {
                SZrLspCodeAction **actionPtr =
                    (SZrLspCodeAction **)ZrCore_Array_Get(actionLists[listIndex], actionIndex);
                const TZrChar *title = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                           ? test_string_text((*actionPtr)->title)
                                           : ZR_NULL;
                if (title != ZR_NULL && strstr(title, "Import zr.math as math") != ZR_NULL) {
                    foundImportAction = ZR_TRUE;
                    break;
                }
            }
        }

        if (foundImportAction) {
            (*failures)++;
            TEST_FAIL(timer, summary, "non-code math.abs text produced a missing import quick fix");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &commentActions);
    ZrLanguageServer_Lsp_FreeCodeActions(state, &stringActions);
    ZrLanguageServer_Lsp_FreeCodeActions(state, &blockCommentActions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_action_ignores_multiline_block_comment_missing_import(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action ignores missing imports in multiline block comments";
    const TZrChar *content =
        "fn main(value: int): int {\n"
        "    /*\n"
        "    math.abs(value);\n"
        "    */\n"
        "    return value;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{2, 4}, {2, 8}};
    TZrBool foundImportAction = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_multiline_comment_missing_import.zr", content, &uri);
    if (context == ZR_NULL || !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for multiline block comment missing import text");
    } else {
        for (TZrSize index = 0; index < actions.length; index++) {
            SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
            const TZrChar *title = actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                                       ? test_string_text((*actionPtr)->title)
                                       : ZR_NULL;
            if (title != ZR_NULL && strstr(title, "Import zr.math as math") != ZR_NULL) {
                foundImportAction = ZR_TRUE;
                break;
            }
        }
        if (foundImportAction) {
            (*failures)++;
            TEST_FAIL(timer, summary, "multiline block comment math.abs text produced an import quick fix");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_lens_exposes_test_command(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code lens exposes test commands";
    const TZrChar *content =
        "#zr.testing.test#\n"
        "fn advanced(): void {\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray lenses = {0};
    TZrBool foundRun = ZR_FALSE;
    TZrBool foundDebug = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_lens.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeLens(state, context, uri, &lenses) ||
        lenses.length < 2U) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeLens did not return manifest-backed run/debug commands");
    } else {
        for (TZrSize index = 0U; index < lenses.length; index++) {
            SZrLspCodeLens **lensPtr =
                    (SZrLspCodeLens **)ZrCore_Array_Get(&lenses, index);
            const TZrChar *command = lensPtr != ZR_NULL && *lensPtr != ZR_NULL
                                             ? test_string_text((*lensPtr)->command)
                                             : ZR_NULL;
            foundRun = foundRun ||
                       (command != ZR_NULL && strcmp(command, "zr.runCurrentProject") == 0);
            foundDebug = foundDebug ||
                         (command != ZR_NULL && strcmp(command, "zr.debugCurrentProject") == 0);
        }
        if (!foundRun || !foundDebug) {
            (*failures)++;
            TEST_FAIL(timer, summary, "manifest CodeLens omitted run or debug command");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeLens(state, &lenses);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_lens_ignores_unbound_test_like_attribute(
        SZrState *state,
        int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code lens consumes bound test attribute roles";
    const TZrChar *content =
        "#zr.testing.tests#\n"
        "fn notATest(): void {\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray lenses = {0};
    TZrBool foundRunLens = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(
            state, "file:///tmp/zr_lsp_lens_unbound_attribute.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeLens(state, context, uri, &lenses)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeLens failed for unbound attribute fixture");
    } else {
        for (TZrSize index = 0U; index < lenses.length; index++) {
            SZrLspCodeLens **lensPtr =
                    (SZrLspCodeLens **)ZrCore_Array_Get(&lenses, index);
            const TZrChar *command =
                    lensPtr != ZR_NULL && *lensPtr != ZR_NULL
                    ? test_string_text((*lensPtr)->command)
                    : ZR_NULL;
            if (command != ZR_NULL && strcmp(command, "zr.runCurrentProject") == 0) {
                foundRunLens = ZR_TRUE;
                break;
            }
        }
        if (foundRunLens) {
            (*failures)++;
            TEST_FAIL(timer, summary, "unbound test-like attribute produced a run CodeLens");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeLens(state, &lenses);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

/* 已绑定 test 标记仍须检查可调用签名，不能仅凭属性文本发出运行命令。 */
static void test_lsp_code_lens_rejects_invalid_bound_test_signature(
        SZrState *state,
        int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code lens requires a valid TestManifest entry";
    const TZrChar *content =
        "#zr.testing.test#\n"
        "fn invalidTest(): int {\n"
        "    return 0;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray lenses = {0};
    TZrBool foundTestLens = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(
            state, "file:///tmp/zr_lsp_lens_invalid_test.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeLens(state, context, uri, &lenses)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeLens failed for invalid test signature fixture");
    } else {
        for (TZrSize index = 0U; index < lenses.length; index++) {
            SZrLspCodeLens **lensPtr =
                    (SZrLspCodeLens **)ZrCore_Array_Get(&lenses, index);
            const TZrChar *title = lensPtr != ZR_NULL && *lensPtr != ZR_NULL
                                           ? test_string_text((*lensPtr)->commandTitle)
                                           : ZR_NULL;
            if (title != ZR_NULL && strstr(title, "Zr test") != ZR_NULL) {
                foundTestLens = ZR_TRUE;
                break;
            }
        }
        if (foundTestLens) {
            (*failures)++;
            TEST_FAIL(timer, summary, "invalid test signature was discovered by AST instead of manifest");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeLens(state, &lenses);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_lens_ignores_non_code_test_markers(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code lens ignores test markers in comments and strings";
    const TZrChar *content =
        "fn run(): int {\n"
        "    var text = \"#zr.testing.test#\";\n"
        "    // #zr.testing.test#\n"
        "    /*\n"
        "    #zr.testing.test#\n"
        "    */\n"
        "    return 0;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray lenses = {0};
    TZrBool foundRunLens = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_lens_non_code_tests.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeLens(state, context, uri, &lenses)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeLens failed for non-code test marker fixture");
    } else {
        for (TZrSize index = 0; index < lenses.length; index++) {
            SZrLspCodeLens **lensPtr = (SZrLspCodeLens **)ZrCore_Array_Get(&lenses, index);
            const TZrChar *command = (lensPtr != ZR_NULL && *lensPtr != ZR_NULL)
                                         ? test_string_text((*lensPtr)->command)
                                         : ZR_NULL;
            if (command != ZR_NULL && strcmp(command, "zr.runCurrentProject") == 0) {
                foundRunLens = ZR_TRUE;
                break;
            }
        }
        if (foundRunLens) {
            (*failures)++;
        TEST_FAIL(timer, summary, "non-code test attribute text produced a run CodeLens");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeLens(state, &lenses);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_code_lens_exposes_reference_count(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code lens exposes function reference counts";
    const TZrChar *content =
        "fn helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "\n"
        "fn first(value: int): int {\n"
        "    return helper(value);\n"
        "}\n"
        "\n"
        "fn second(value: int): int {\n"
        "    return helper(value);\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray lenses = {0};
    TZrBool foundReferenceLens = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_lens_references.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeLens(state, context, uri, &lenses)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeLens did not return reference count results");
    } else {
        for (TZrSize index = 0; index < lenses.length; index++) {
            SZrLspCodeLens **lensPtr = (SZrLspCodeLens **)ZrCore_Array_Get(&lenses, index);
            const TZrChar *title = (lensPtr != ZR_NULL && *lensPtr != ZR_NULL)
                                       ? test_string_text((*lensPtr)->commandTitle)
                                       : ZR_NULL;
            if (title != ZR_NULL && strcmp(title, "2 references") == 0) {
                foundReferenceLens = ZR_TRUE;
                break;
            }
        }
        if (!foundReferenceLens) {
            (*failures)++;
            TEST_FAIL(timer, summary, "codeLens did not expose the helper() reference count");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeCodeLens(state, &lenses);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_call_hierarchy_prepare_returns_symbol_item(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP call hierarchy prepare returns a callable symbol item";
    const TZrChar *content =
        "class Sample {\n"
        "    pub fn run(value: int): int {\n"
        "        return value;\n"
        "    }\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray items = {0};
    SZrLspPosition position;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_call_hierarchy.zr", content, &uri);
    if (context == ZR_NULL ||
        !test_find_position(content, "run(value", 1, &position) ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(state, context, uri, position, &items) ||
        items.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "prepareCallHierarchy did not return the function symbol");
    } else {
        SZrLspHierarchyItem **itemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(&items, 0);
        const TZrChar *name = (itemPtr != ZR_NULL && *itemPtr != ZR_NULL)
                                  ? test_string_text((*itemPtr)->name)
                                  : ZR_NULL;
        if (name == ZR_NULL || strcmp(name, "run") != 0 ||
            ((*itemPtr)->kind != ZR_LSP_SYMBOL_KIND_FUNCTION &&
             (*itemPtr)->kind != ZR_LSP_SYMBOL_KIND_METHOD)) {
            (*failures)++;
            TEST_FAIL(timer, summary, "prepareCallHierarchy item did not describe run()");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &items);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_call_hierarchy_outgoing_returns_direct_calls(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP call hierarchy outgoing returns direct calls";
    const TZrChar *content =
        "fn helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "\n"
        "fn run(value: int): int {\n"
        "    return helper(value);\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray items = {0};
    SZrArray calls = {0};
    SZrLspPosition position;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_call_hierarchy_outgoing.zr", content, &uri);
    if (context == ZR_NULL ||
        !test_find_position(content, "run(value", 1, &position) ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(state, context, uri, position, &items) ||
        items.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "prepareCallHierarchy did not return run()");
    } else {
        SZrLspHierarchyItem **itemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(&items, 0);
        if (itemPtr == ZR_NULL ||
            *itemPtr == ZR_NULL ||
            !ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(state, context, *itemPtr, &calls) ||
            calls.length != 1) {
            (*failures)++;
            TEST_FAIL(timer, summary, "outgoingCalls did not return helper()");
        } else {
            SZrLspHierarchyCall **callPtr = (SZrLspHierarchyCall **)ZrCore_Array_Get(&calls, 0);
            const TZrChar *name = (callPtr != ZR_NULL && *callPtr != ZR_NULL && (*callPtr)->item != ZR_NULL)
                                      ? test_string_text((*callPtr)->item->name)
                                      : ZR_NULL;
            if (name == ZR_NULL || strcmp(name, "helper") != 0 || (*callPtr)->fromRanges.length == 0) {
                (*failures)++;
                TEST_FAIL(timer, summary, "outgoing call did not describe helper() and its call range");
            } else {
                TEST_PASS(timer, summary);
            }
        }
    }

    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &calls);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &items);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_call_hierarchy_incoming_returns_direct_callers(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP call hierarchy incoming returns direct callers";
    const TZrChar *content =
        "fn helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "\n"
        "fn run(value: int): int {\n"
        "    return helper(value);\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray items = {0};
    SZrArray calls = {0};
    SZrLspPosition position;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_call_hierarchy_incoming.zr", content, &uri);
    if (context == ZR_NULL ||
        !test_find_position(content, "helper(value", 1, &position) ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(state, context, uri, position, &items) ||
        items.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "prepareCallHierarchy did not return helper()");
    } else {
        SZrLspHierarchyItem **itemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(&items, 0);
        if (itemPtr == ZR_NULL ||
            *itemPtr == ZR_NULL ||
            !ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(state, context, *itemPtr, &calls) ||
            calls.length != 1) {
            (*failures)++;
            TEST_FAIL(timer, summary, "incomingCalls did not return run()");
        } else {
            SZrLspHierarchyCall **callPtr = (SZrLspHierarchyCall **)ZrCore_Array_Get(&calls, 0);
            const TZrChar *name = (callPtr != ZR_NULL && *callPtr != ZR_NULL && (*callPtr)->item != ZR_NULL)
                                      ? test_string_text((*callPtr)->item->name)
                                      : ZR_NULL;
            if (name == ZR_NULL || strcmp(name, "run") != 0 || (*callPtr)->fromRanges.length == 0) {
                (*failures)++;
                TEST_FAIL(timer, summary, "incoming call did not describe run() and the helper call range");
            } else {
                TEST_PASS(timer, summary);
            }
        }
    }

    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &calls);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &items);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

/* 注释和字符串中的同名调用不能成为 incoming/outgoing 语义边。 */
static void test_lsp_call_hierarchy_ignores_non_code_call_mentions(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP call hierarchy ignores call-looking text in comments and strings";
    const TZrChar *content =
        "fn helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "\n"
        "fn run(value: int): int {\n"
        "    // helper(value) is prose\n"
        "    var text = \"helper(value) is text\";\n"
        "    return value;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray runItems = {0};
    SZrArray helperItems = {0};
    SZrArray outgoing = {0};
    SZrArray incoming = {0};
    SZrLspPosition runPosition;
    SZrLspPosition helperPosition;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_call_hierarchy_non_code_calls.zr", content, &uri);
    if (context == ZR_NULL ||
        !test_find_position(content, "run(value", 1, &runPosition) ||
        !test_find_position(content, "helper(value", 1, &helperPosition) ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(state, context, uri, runPosition, &runItems) ||
        runItems.length != 1 ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(state, context, uri, helperPosition, &helperItems) ||
        helperItems.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "failed to prepare call hierarchy non-code fixture");
    } else {
        SZrLspHierarchyItem **runItemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(&runItems, 0);
        SZrLspHierarchyItem **helperItemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(&helperItems, 0);
        if (runItemPtr == ZR_NULL ||
            *runItemPtr == ZR_NULL ||
            helperItemPtr == ZR_NULL ||
            *helperItemPtr == ZR_NULL ||
            !ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(state, context, *runItemPtr, &outgoing) ||
            !ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(state, context, *helperItemPtr, &incoming)) {
            (*failures)++;
            TEST_FAIL(timer, summary, "call hierarchy requests failed for non-code fixture");
        } else if (outgoing.length != 0 || incoming.length != 0) {
            (*failures)++;
            TEST_FAIL(timer, summary, "call hierarchy counted comment or string text as a call");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &outgoing);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &incoming);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &runItems);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &helperItems);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_type_hierarchy_prepare_returns_type_item(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP type hierarchy prepare returns a type symbol item";
    const TZrChar *content =
        "class Sample {\n"
        "    pub fn run(): int { return 1; }\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray items = {0};
    SZrLspPosition position;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_type_hierarchy.zr", content, &uri);
    if (context == ZR_NULL ||
        !test_find_position(content, "Sample", 1, &position) ||
        !ZrLanguageServer_Lsp_PrepareTypeHierarchy(state, context, uri, position, &items) ||
        items.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "prepareTypeHierarchy did not return the class symbol");
    } else {
        SZrLspHierarchyItem **itemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(&items, 0);
        const TZrChar *name = (itemPtr != ZR_NULL && *itemPtr != ZR_NULL)
                                  ? test_string_text((*itemPtr)->name)
                                  : ZR_NULL;
        if (name == ZR_NULL || strcmp(name, "Sample") != 0 ||
            (*itemPtr)->kind != ZR_LSP_SYMBOL_KIND_CLASS) {
            (*failures)++;
            TEST_FAIL(timer, summary, "prepareTypeHierarchy item did not describe Sample");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &items);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_type_hierarchy_supertypes_returns_direct_base(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP type hierarchy supertypes returns direct base";
    const TZrChar *content =
        "class Base {\n"
        "}\n"
        "\n"
        "class Derived : Base {\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray items = {0};
    SZrArray supertypes = {0};
    SZrLspPosition position;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_type_hierarchy_supertypes.zr", content, &uri);
    if (context == ZR_NULL ||
        !test_find_position(content, "Derived", 1, &position) ||
        !ZrLanguageServer_Lsp_PrepareTypeHierarchy(state, context, uri, position, &items) ||
        items.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "prepareTypeHierarchy did not return Derived");
    } else {
        SZrLspHierarchyItem **itemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(&items, 0);
        if (itemPtr == ZR_NULL ||
            *itemPtr == ZR_NULL ||
            !ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(state, context, *itemPtr, &supertypes) ||
            supertypes.length != 1) {
            (*failures)++;
            TEST_FAIL(timer, summary, "supertypes did not return Base");
        } else {
            SZrLspHierarchyItem **basePtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(&supertypes, 0);
            const TZrChar *name = (basePtr != ZR_NULL && *basePtr != ZR_NULL)
                                      ? test_string_text((*basePtr)->name)
                                      : ZR_NULL;
            if (name == ZR_NULL || strcmp(name, "Base") != 0) {
                (*failures)++;
                TEST_FAIL(timer, summary, "supertype item did not describe Base");
            } else {
                TEST_PASS(timer, summary);
            }
        }
    }

    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &supertypes);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &items);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_type_hierarchy_subtypes_returns_direct_derived(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP type hierarchy subtypes returns direct derived";
    const TZrChar *content =
        "class Base {\n"
        "}\n"
        "\n"
        "class Derived : Base {\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray items = {0};
    SZrArray subtypes = {0};
    SZrLspPosition position;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_type_hierarchy_subtypes.zr", content, &uri);
    if (context == ZR_NULL ||
        !test_find_position(content, "Base", 1, &position) ||
        !ZrLanguageServer_Lsp_PrepareTypeHierarchy(state, context, uri, position, &items) ||
        items.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "prepareTypeHierarchy did not return Base");
    } else {
        SZrLspHierarchyItem **itemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(&items, 0);
        if (itemPtr == ZR_NULL ||
            *itemPtr == ZR_NULL ||
            !ZrLanguageServer_Lsp_GetTypeHierarchySubtypes(state, context, *itemPtr, &subtypes) ||
            subtypes.length != 1) {
            (*failures)++;
            TEST_FAIL(timer, summary, "subtypes did not return Derived");
        } else {
            SZrLspHierarchyItem **derivedPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(&subtypes, 0);
            const TZrChar *name = (derivedPtr != ZR_NULL && *derivedPtr != ZR_NULL)
                                      ? test_string_text((*derivedPtr)->name)
                                      : ZR_NULL;
            if (name == ZR_NULL || strcmp(name, "Derived") != 0) {
                (*failures)++;
                TEST_FAIL(timer, summary, "subtype item did not describe Derived");
            } else {
                TEST_PASS(timer, summary);
            }
        }
    }

    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &subtypes);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &items);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_definition_ignores_non_code_receiver_member_text(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP definition ignores receiver member text in comments and strings";
    const TZrChar *content =
        "class Box {\n"
        "    pub var value: int = 0;\n"
        "}\n"
        "\n"
        "fn read(): int {\n"
        "    var box = new Box();\n"
        "    // box.value should stay prose\n"
        "    var text = \"box.value should stay text\";\n"
        "    return box.value;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrLspPosition commentPosition;
    SZrLspPosition stringPosition;
    SZrLspPosition realPosition;
    SZrArray commentDefinitions = {0};
    SZrArray stringDefinitions = {0};
    SZrArray realDefinitions = {0};
    TZrBool commentResolved;
    TZrBool stringResolved;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_non_code_definition.zr", content, &uri);
    if (context == ZR_NULL ||
        !test_find_position(content, "// box.value", (TZrInt32)strlen("// box."), &commentPosition) ||
        !test_find_position(content, "\"box.value", (TZrInt32)strlen("\"box."), &stringPosition) ||
        !test_find_position(content, "return box.value", (TZrInt32)strlen("return box."), &realPosition)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "failed to prepare receiver-member definition fixture");
    } else {
        commentResolved =
            ZrLanguageServer_Lsp_GetDefinition(state, context, uri, commentPosition, &commentDefinitions);
        stringResolved =
            ZrLanguageServer_Lsp_GetDefinition(state, context, uri, stringPosition, &stringDefinitions);

        if ((commentResolved && commentDefinitions.length != 0) ||
            (stringResolved && stringDefinitions.length != 0)) {
            (*failures)++;
            TEST_FAIL(timer, summary, "definition resolved receiver-member text from a non-code span");
        } else if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, realPosition, &realDefinitions) ||
                   realDefinitions.length == 0) {
            (*failures)++;
            TEST_FAIL(timer, summary, "real receiver-member definition stopped resolving");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    /* BUG: GetDefinition 的位置项独立 RawMalloc；以下三组只 Array_Free
     * 外层数组，真实定义非空时必泄漏，异常非代码结果也会泄漏。 */
    ZrCore_Array_Free(state, &commentDefinitions);
    ZrCore_Array_Free(state, &stringDefinitions);
    ZrCore_Array_Free(state, &realDefinitions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_advanced_editor_features_return_empty_for_empty_document(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP advanced editor features return empty results for empty documents";
    const TZrChar *content = "";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray formatting = {0};
    SZrArray rangeFormatting = {0};
    SZrArray codeActions = {0};
    SZrArray foldingRanges = {0};
    SZrArray selectionRanges = {0};
    SZrArray documentLinks = {0};
    SZrArray codeLens = {0};
    SZrArray callHierarchy = {0};
    SZrArray typeHierarchy = {0};
    SZrLspRange emptyRange = {{0, 0}, {0, 0}};
    SZrLspPosition positions[1] = {{0, 0}};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_empty_advanced_features.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &formatting) ||
        !ZrLanguageServer_Lsp_GetRangeFormatting(state, context, uri, emptyRange, ZR_NULL, &rangeFormatting) ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, emptyRange, &codeActions) ||
        !ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &foldingRanges) ||
        !ZrLanguageServer_Lsp_GetSelectionRanges(state, context, uri, positions, 0, &selectionRanges) ||
        !ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uri, &documentLinks) ||
        !ZrLanguageServer_Lsp_GetCodeLens(state, context, uri, &codeLens) ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(state, context, uri, positions[0], &callHierarchy) ||
        !ZrLanguageServer_Lsp_PrepareTypeHierarchy(state, context, uri, positions[0], &typeHierarchy)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "one or more providers failed instead of returning an empty result");
    } else if (formatting.length != 0 ||
               rangeFormatting.length != 0 ||
               codeActions.length != 0 ||
               foldingRanges.length != 0 ||
               selectionRanges.length != 0 ||
               documentLinks.length != 0 ||
               codeLens.length != 0 ||
               callHierarchy.length != 0 ||
               typeHierarchy.length != 0) {
        (*failures)++;
        TEST_FAIL(timer, summary, "empty document produced non-empty advanced editor feature results");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeTextEdits(state, &formatting);
    ZrLanguageServer_Lsp_FreeTextEdits(state, &rangeFormatting);
    ZrLanguageServer_Lsp_FreeCodeActions(state, &codeActions);
    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &foldingRanges);
    ZrLanguageServer_Lsp_FreeSelectionRanges(state, &selectionRanges);
    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &documentLinks);
    ZrLanguageServer_Lsp_FreeCodeLens(state, &codeLens);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &callHierarchy);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &typeHierarchy);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

/* 未打开 URI 的各编辑器请求应安全返回空集合，避免借用其他文档的缓存。 */
static void test_lsp_advanced_editor_features_return_empty_for_unopened_documents(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP advanced editor features return empty results for unopened documents";
    SZrLspContext *context;
    SZrString *uri;
    SZrArray formatting = {0};
    SZrArray rangeFormatting = {0};
    SZrArray codeActions = {0};
    SZrArray foldingRanges = {0};
    SZrArray selectionRanges = {0};
    SZrArray documentLinks = {0};
    SZrArray codeLens = {0};
    SZrArray callHierarchy = {0};
    SZrArray typeHierarchy = {0};
    SZrLspRange range = {{0, 0}, {0, 0}};
    SZrLspPosition position = {0, 0};

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               (TZrNativeString)"file:///tmp/zr_lsp_unopened_advanced_features.zr",
                               strlen("file:///tmp/zr_lsp_unopened_advanced_features.zr"));
    if (context == ZR_NULL || uri == ZR_NULL) {
        (*failures)++;
        TEST_FAIL(timer, summary, "failed to allocate test context");
    } else {
        (void)ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &formatting);
        (void)ZrLanguageServer_Lsp_GetRangeFormatting(state, context, uri, range, ZR_NULL, &rangeFormatting);
        (void)ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &codeActions);
        (void)ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &foldingRanges);
        (void)ZrLanguageServer_Lsp_GetSelectionRanges(state, context, uri, &position, 1, &selectionRanges);
        (void)ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uri, &documentLinks);
        (void)ZrLanguageServer_Lsp_GetCodeLens(state, context, uri, &codeLens);
        (void)ZrLanguageServer_Lsp_PrepareCallHierarchy(state, context, uri, position, &callHierarchy);
        (void)ZrLanguageServer_Lsp_PrepareTypeHierarchy(state, context, uri, position, &typeHierarchy);

        if (formatting.length != 0 ||
            rangeFormatting.length != 0 ||
            codeActions.length != 0 ||
            foldingRanges.length != 0 ||
            selectionRanges.length != 0 ||
            documentLinks.length != 0 ||
            codeLens.length != 0 ||
            callHierarchy.length != 0 ||
            typeHierarchy.length != 0) {
            (*failures)++;
            TEST_FAIL(timer, summary, "unopened documents produced advanced editor feature results");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeTextEdits(state, &formatting);
    ZrLanguageServer_Lsp_FreeTextEdits(state, &rangeFormatting);
    ZrLanguageServer_Lsp_FreeCodeActions(state, &codeActions);
    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &foldingRanges);
    ZrLanguageServer_Lsp_FreeSelectionRanges(state, &selectionRanges);
    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &documentLinks);
    ZrLanguageServer_Lsp_FreeCodeLens(state, &codeLens);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &callHierarchy);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &typeHierarchy);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

/* 无尾换行的单行才会把结束列留在同一行；尾随 \\n 会把结束位置重置到下一行 0 列。 */
static void test_lsp_formatting_reports_utf16_columns(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting reports UTF-16 columns";
    const TZrChar *cases[][2] = {
        {"  return 中 ; ", "return 中 ;"},
        {"  return 😀;", "return 😀;"},
    };
    const TZrInt32 expectedEndCharacters[] = {13, 12};
    TZrSize caseIndex;

    TEST_START(summary);
    for (caseIndex = 0; caseIndex < sizeof(cases) / sizeof(cases[0]); caseIndex++) {
        SZrString *uri = ZR_NULL;
        SZrLspContext *context;
        SZrArray edits = {0};
        SZrLspTextEdit *edit;
        const TZrChar *text;
        char uriText[64];

        snprintf(uriText, sizeof(uriText), "file:///tmp/zr_lsp_utf16_format_%u.zr", (unsigned)caseIndex);
        context = test_open_document(state, uriText, cases[caseIndex][0], &uri);
        if (context == ZR_NULL ||
            !ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &edits)) {
            (*failures)++;
            TEST_FAIL(timer, summary, "formatting failed for a non-ASCII line");
            ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
            if (context != ZR_NULL) {
                ZrLanguageServer_LspContext_Free(state, context);
            }
            return;
        }
        edit = first_text_edit(&edits);
        text = edit != ZR_NULL ? test_string_text(edit->newText) : ZR_NULL;
        if (edits.length != 1 ||
            edit == ZR_NULL ||
            edit->range.start.line != 0 ||
            edit->range.start.character != 0 ||
            edit->range.end.line != 0 ||
            edit->range.end.character != expectedEndCharacters[caseIndex] ||
            text == ZR_NULL ||
            strcmp(text, cases[caseIndex][1]) != 0) {
            char detail[192];
            snprintf(detail,
                     sizeof(detail),
                     "UTF-16 format range mismatch: end=%d:%d text=%s",
                     edit != ZR_NULL ? edit->range.end.line : -1,
                     edit != ZR_NULL ? edit->range.end.character : -1,
                     text != ZR_NULL ? text : "<null>");
            (*failures)++;
            TEST_FAIL(timer, summary, detail);
            ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
            ZrLanguageServer_LspContext_Free(state, context);
            return;
        }
        ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
        ZrLanguageServer_LspContext_Free(state, context);
    }
    TEST_PASS(timer, summary);
}

static void test_lsp_formatting_honors_options_and_crlf(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting honors options and keeps CRLF";
    const TZrChar *lfSource = "fn main(): int {\nreturn 1;\n}\n";
    const TZrChar *crlfSource = "fn main(): int {\r\nreturn 1;\r\n}\r\n";
    const TZrChar *crlfFormatted = "fn main(): int {\r\n    return 1;\r\n}\r\n";
    const TZrChar *crlfRangeSource = "fn main(): int {\r\nreturn 1;\r\nreturn 2;\r\n}\r\n";
    SZrLspFormattingOptions twoSpaces = {2, ZR_TRUE};
    SZrLspFormattingOptions tabs = {4, ZR_FALSE};
    SZrLspFormattingOptions invalidTab = {0, ZR_TRUE};
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray edits = {0};
    SZrLspTextEdit *edit;
    const TZrChar *text;
    SZrLspRange range = {{1, 0}, {2, 4}};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_format_options.zr", lfSource, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFormatting(state, context, uri, &twoSpaces, &edits)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "tabSize 2 formatting failed");
        goto cleanup;
    }
    if (!text_edit_contains(&edits, "\n  return 1;\n") ||
        text_edit_contains(&edits, "\n    return 1;\n")) {
        (*failures)++;
        TEST_FAIL(timer, summary, "tabSize 2 did not indent with two spaces");
        goto cleanup;
    }
    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    memset(&edits, 0, sizeof(edits));
    if (!ZrLanguageServer_Lsp_GetFormatting(state, context, uri, &tabs, &edits) ||
        !text_edit_contains(&edits, "\n\treturn 1;\n")) {
        (*failures)++;
        TEST_FAIL(timer, summary, "insertSpaces false did not indent with a tab");
        goto cleanup;
    }
    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    memset(&edits, 0, sizeof(edits));
    if (!ZrLanguageServer_Lsp_GetFormatting(state, context, uri, &invalidTab, &edits) ||
        !text_edit_contains(&edits, "\n    return 1;\n")) {
        (*failures)++;
        TEST_FAIL(timer, summary, "invalid tabSize did not fall back to four spaces");
        goto cleanup;
    }
    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    memset(&edits, 0, sizeof(edits));
    ZrLanguageServer_LspContext_Free(state, context);
    context = ZR_NULL;

    context = test_open_document(state, "file:///tmp/zr_lsp_format_crlf_noop.zr", crlfFormatted, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &edits) ||
        edits.length != 0) {
        (*failures)++;
        TEST_FAIL(timer, summary, "already indented CRLF document was rewritten");
        goto cleanup;
    }
    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    memset(&edits, 0, sizeof(edits));
    ZrLanguageServer_LspContext_Free(state, context);

    context = test_open_document(state, "file:///tmp/zr_lsp_format_crlf.zr", crlfSource, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &edits)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "CRLF formatting failed");
        goto cleanup;
    }
    edit = first_text_edit(&edits);
    text = edit != ZR_NULL ? test_string_text(edit->newText) : ZR_NULL;
    if (edits.length != 1 || text == ZR_NULL || strcmp(text, crlfFormatted) != 0 || text_contains_bare_lf(text)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "CRLF formatting rewrote line endings");
        goto cleanup;
    }
    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    memset(&edits, 0, sizeof(edits));
    ZrLanguageServer_LspContext_Free(state, context);

    context = test_open_document(state, "file:///tmp/zr_lsp_format_crlf_range.zr", crlfRangeSource, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetRangeFormatting(state, context, uri, range, ZR_NULL, &edits)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "CRLF range formatting failed");
        goto cleanup;
    }
    edit = first_text_edit(&edits);
    text = edit != ZR_NULL ? test_string_text(edit->newText) : ZR_NULL;
    if (edits.length != 1 ||
        edit == ZR_NULL ||
        edit->range.start.line != 1 ||
        edit->range.start.character != 0 ||
        edit->range.end.line != 3 ||
        edit->range.end.character != 0 ||
        text == ZR_NULL ||
        strcmp(text, "    return 1;\r\n    return 2;\r\n") != 0 ||
        text_contains_bare_lf(text)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "CRLF range formatting dropped the carriage return");
        goto cleanup;
    }

    TEST_PASS(timer, summary);
cleanup:
    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_selection_word_uses_utf16_columns(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP selection word range uses UTF-16 columns";
    const TZrChar *content = "  中foo\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray selections = {0};
    SZrLspPosition position = {0, 3};
    SZrLspSelectionRange **selectionPtr;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_utf16_selection.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetSelectionRanges(state, context, uri, &position, 1, &selections) ||
        selections.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "selection range provider failed on a non-ASCII line");
    } else {
        selectionPtr = (SZrLspSelectionRange **)ZrCore_Array_Get(&selections, 0);
        if (selectionPtr == ZR_NULL ||
            *selectionPtr == ZR_NULL ||
            (*selectionPtr)->range.start.line != 0 ||
            (*selectionPtr)->range.start.character != 3 ||
            (*selectionPtr)->range.end.line != 0 ||
            (*selectionPtr)->range.end.character != 6) {
            (*failures)++;
            TEST_FAIL(timer, summary, "word range counted UTF-8 bytes instead of UTF-16 columns");
        } else {
            TEST_PASS(timer, summary);
        }
    }

    ZrLanguageServer_Lsp_FreeSelectionRanges(state, &selections);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_folding_columns_are_utf16(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP folding columns are UTF-16";
    const TZrChar *braceContent = "fn 中(): int {\n    return 0;\n}\n";
    const TZrChar *commentContent = "// 中甲\n// 中乙\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray folds = {0};
    TZrBool foundBrace = ZR_FALSE;
    TZrBool foundComment = ZR_FALSE;
    TZrSize index;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_utf16_fold_brace.zr", braceContent, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "folding failed for a non-ASCII brace");
        goto cleanup;
    }
    for (index = 0; index < folds.length; index++) {
        SZrLspFoldingRange **rangePtr = (SZrLspFoldingRange **)ZrCore_Array_Get(&folds, index);
        const TZrChar *kind = rangePtr != ZR_NULL && *rangePtr != ZR_NULL
                                  ? test_string_text((*rangePtr)->kind)
                                  : ZR_NULL;
        if (rangePtr != ZR_NULL &&
            *rangePtr != ZR_NULL &&
            kind != ZR_NULL &&
            strcmp(kind, "region") == 0 &&
            (*rangePtr)->startLine == 0 &&
            (*rangePtr)->startCharacter == 12 &&
            (*rangePtr)->endLine == 2) {
            foundBrace = ZR_TRUE;
        }
    }
    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
    memset(&folds, 0, sizeof(folds));
    ZrLanguageServer_LspContext_Free(state, context);

    context = test_open_document(state, "file:///tmp/zr_lsp_utf16_fold_comment.zr", commentContent, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "comment folding failed");
        goto cleanup;
    }
    for (index = 0; index < folds.length; index++) {
        SZrLspFoldingRange **rangePtr = (SZrLspFoldingRange **)ZrCore_Array_Get(&folds, index);
        const TZrChar *kind = rangePtr != ZR_NULL && *rangePtr != ZR_NULL
                                  ? test_string_text((*rangePtr)->kind)
                                  : ZR_NULL;
        if (rangePtr != ZR_NULL &&
            *rangePtr != ZR_NULL &&
            kind != ZR_NULL &&
            strcmp(kind, "comment") == 0 &&
            (*rangePtr)->startLine == 0 &&
            (*rangePtr)->startCharacter == 0 &&
            (*rangePtr)->endLine == 1 &&
            (*rangePtr)->endCharacter == 5) {
            foundComment = ZR_TRUE;
        }
    }
    if (!foundBrace || !foundComment) {
        (*failures)++;
        TEST_FAIL(timer, summary, "folding columns still follow UTF-8 byte offsets");
        goto cleanup;
    }
    TEST_PASS(timer, summary);
cleanup:
    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_missing_import_range_uses_utf16_columns(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP missing import range uses UTF-16 columns";
    const TZrChar *content = "let 名称 = math.sin;\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange mathRange = {{0, 9}, {0, 13}};
    SZrLspRange nameRange = {{0, 4}, {0, 6}};
    TZrBool foundMath = ZR_FALSE;
    TZrSize index;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_utf16_missing_import.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, nameRange, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for the non-alias range");
        goto cleanup;
    }
    for (index = 0; index < actions.length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
        const TZrChar *title = actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                                   ? test_string_text((*actionPtr)->title)
                                   : ZR_NULL;
        if (title != ZR_NULL && strcmp(title, "Import zr.math as math") == 0) {
            (*failures)++;
            TEST_FAIL(timer, summary, "a range on the CJK binding produced a math import");
            goto cleanup;
        }
    }
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    memset(&actions, 0, sizeof(actions));
    if (!ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, mathRange, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for the math alias range");
        goto cleanup;
    }
    for (index = 0; index < actions.length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
        const TZrChar *title = actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                                   ? test_string_text((*actionPtr)->title)
                                   : ZR_NULL;
        if (title != ZR_NULL && strcmp(title, "Import zr.math as math") == 0) {
            foundMath = ZR_TRUE;
        }
    }
    if (!foundMath) {
        (*failures)++;
        TEST_FAIL(timer, summary, "UTF-16 columns on math.sin did not select the missing import");
        goto cleanup;
    }
    TEST_PASS(timer, summary);
cleanup:
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_organize_imports_preserves_crlf(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP organize imports preserves CRLF";
    const TZrChar *content =
        "let system = import(\"zr.system\");\r\n"
        "let math = import(\"zr.math\");\r\n"
        "\r\n"
        "fn main(): int { return 0; }\r\n";
    const TZrChar *expected =
        "let math = import(\"zr.math\");\r\n"
        "let system = import(\"zr.system\");\r\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {4, 0}};
    TZrBool found = ZR_FALSE;
    TZrSize index;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_organize_crlf.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "organize imports failed for a CRLF document");
        goto cleanup;
    }
    for (index = 0; index < actions.length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
        const TZrChar *title = actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                                   ? test_string_text((*actionPtr)->title)
                                   : ZR_NULL;
        const TZrChar *text = actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                                  ? first_text_edit_text(&(*actionPtr)->edits)
                                  : ZR_NULL;
        if (title != ZR_NULL &&
            strstr(title, "Organize") != ZR_NULL &&
            text != ZR_NULL &&
            strcmp(text, expected) == 0 &&
            !text_contains_bare_lf(text)) {
            found = ZR_TRUE;
        }
    }
    if (!found) {
        (*failures)++;
        TEST_FAIL(timer, summary, "organized imports rewrote CRLF as LF");
        goto cleanup;
    }
    TEST_PASS(timer, summary);
cleanup:
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

/* 只接受恰好一段编辑，并同时核对范围两端和替换文本，范围与文本必须描述同一段字节。 */
static TZrBool single_text_edit_matches(SZrArray *edits,
                                        TZrInt32 startLine,
                                        TZrInt32 startCharacter,
                                        TZrInt32 endLine,
                                        TZrInt32 endCharacter,
                                        const TZrChar *expectedText) {
    SZrLspTextEdit *edit = first_text_edit(edits);
    const TZrChar *text = edit != ZR_NULL ? test_string_text(edit->newText) : ZR_NULL;

    return edits != ZR_NULL &&
           edits->length == 1 &&
           edit != ZR_NULL &&
           edit->range.start.line == startLine &&
           edit->range.start.character == startCharacter &&
           edit->range.end.line == endLine &&
           edit->range.end.character == endCharacter &&
           text != ZR_NULL &&
           expectedText != ZR_NULL &&
           strcmp(text, expectedText) == 0;
}

static TZrBool folding_ranges_contain(SZrArray *folds,
                                      const TZrChar *kind,
                                      TZrInt32 startLine,
                                      TZrInt32 startCharacter,
                                      TZrInt32 endLine,
                                      TZrInt32 endCharacter) {
    for (TZrSize index = 0; folds != ZR_NULL && index < folds->length; index++) {
        SZrLspFoldingRange **rangePtr = (SZrLspFoldingRange **)ZrCore_Array_Get(folds, index);
        const TZrChar *rangeKind = rangePtr != ZR_NULL && *rangePtr != ZR_NULL
                                       ? test_string_text((*rangePtr)->kind)
                                       : ZR_NULL;
        if (rangeKind != ZR_NULL &&
            strcmp(rangeKind, kind) == 0 &&
            (*rangePtr)->startLine == startLine &&
            (*rangePtr)->startCharacter == startCharacter &&
            (*rangePtr)->endLine == endLine &&
            (*rangePtr)->endCharacter == endCharacter) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool test_fold_has_kind_span(SZrArray *folds,
                                       TZrInt32 startLine,
                                       TZrInt32 endLine,
                                       const TZrChar *kindText) {
    if (folds == ZR_NULL || kindText == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < folds->length; index++) {
        SZrLspFoldingRange **rangePtr = (SZrLspFoldingRange **)ZrCore_Array_Get(folds, index);
        const TZrChar *kind = (rangePtr != ZR_NULL && *rangePtr != ZR_NULL)
                                  ? test_string_text((*rangePtr)->kind)
                                  : ZR_NULL;
        if (rangePtr != ZR_NULL && *rangePtr != ZR_NULL && kind != ZR_NULL &&
            (*rangePtr)->startLine == startLine &&
            (*rangePtr)->endLine == endLine &&
            strcmp(kind, kindText) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 文末不带 LF 的 \\r 不占 UTF-16 列，范围终点落在它前面；替换文本若带上它，应用后会多出一个 \\r。 */
static void test_lsp_formatting_keeps_trailing_lone_cr_outside_edit(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting keeps a trailing lone CR outside the edit";
    const TZrChar *sources[] = {
        "fn main(): int {\nreturn 1;\n}\r",
        "fn main(): int {\r\nreturn 1;\r\n}\r",
        "fn main(): int {\n    return 1;\n}\r",
    };
    const TZrChar *expectedDocumentTexts[] = {
        "fn main(): int {\n    return 1;\n}",
        "fn main(): int {\r\n    return 1;\r\n}",
        ZR_NULL,
    };
    const TZrChar *expectedRangeTexts[] = {
        "    return 1;\n}",
        "    return 1;\r\n}",
        ZR_NULL,
    };
    SZrLspRange range = {{1, 0}, {2, 0}};
    SZrLspRange pastEndRange = {{9, 0}, {9, 0}};
    TZrSize caseIndex;

    TEST_START(summary);
    for (caseIndex = 0; caseIndex < sizeof(sources) / sizeof(sources[0]); caseIndex++) {
        SZrString *uri = ZR_NULL;
        SZrLspContext *context;
        SZrArray documentEdits = {0};
        SZrArray rangeEdits = {0};
        SZrArray pastEndEdits = {0};
        TZrBool valid;
        char uriText[64];

        snprintf(uriText, sizeof(uriText), "file:///tmp/zr_lsp_format_lone_cr_%u.zr", (unsigned)caseIndex);
        context = test_open_document(state, uriText, sources[caseIndex], &uri);
        valid = context != ZR_NULL &&
                ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &documentEdits) &&
                ZrLanguageServer_Lsp_GetRangeFormatting(state, context, uri, range, ZR_NULL, &rangeEdits) &&
                ZrLanguageServer_Lsp_GetRangeFormatting(state, context, uri, pastEndRange, ZR_NULL, &pastEndEdits) &&
                /* 越过文末的区间没有可格式化的行，不能因为文末的 \\r 冒出一段空编辑。 */
                pastEndEdits.length == 0;
        ZrLanguageServer_Lsp_FreeTextEdits(state, &pastEndEdits);
        if (valid && expectedDocumentTexts[caseIndex] == ZR_NULL) {
            /* 已排好版的文档只差文末的 \\r：两条入口都不该提出编辑。 */
            valid = documentEdits.length == 0 && rangeEdits.length == 0;
        } else if (valid) {
            valid = single_text_edit_matches(&documentEdits, 0, 0, 2, 1, expectedDocumentTexts[caseIndex]) &&
                    single_text_edit_matches(&rangeEdits, 1, 0, 2, 1, expectedRangeTexts[caseIndex]);
        }
        ZrLanguageServer_Lsp_FreeTextEdits(state, &documentEdits);
        ZrLanguageServer_Lsp_FreeTextEdits(state, &rangeEdits);
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        if (!valid) {
            char detail[96];
            snprintf(detail,
                     sizeof(detail),
                     "case %u: the edit range and text disagree about the trailing CR",
                     (unsigned)caseIndex);
            (*failures)++;
            TEST_FAIL(timer, summary, detail);
            return;
        }
    }
    TEST_PASS(timer, summary);
}

/* 列号只在所在行内换算：非 ASCII 首行和 CRLF 都不能让后面各行的折叠列漂移。 */
static void test_lsp_folding_columns_stay_utf16_on_later_lines(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP folding columns stay UTF-16 on later CRLF lines";
    const TZrChar *content =
        "// 标题\r\n"
        "\r\n"
        "  let system = import(\"zr.system\"); // 系统\r\n"
        "  let math = import(\"zr.math\"); // 数学\r\n"
        "\r\n"
        "  //#region 区域\r\n"
        "fn 中(): int {\r\n"
        "    return 0;\r\n"
        "}\r\n"
        "  //#endregion 结束\r\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray folds = {0};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_utf16_fold_later_lines.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "folding failed for a CRLF document with non-ASCII lines");
    } else if (!folding_ranges_contain(&folds, ZR_LSP_FOLDING_RANGE_KIND_IMPORTS, 2, 2, 3, 37)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "imports fold columns are not line-relative UTF-16 columns");
    } else if (!folding_ranges_contain(&folds, ZR_LSP_FOLDING_RANGE_KIND_REGION, 5, 2, 9, 17)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "region marker fold columns are not line-relative UTF-16 columns");
    } else if (!folding_ranges_contain(&folds, ZR_LSP_FOLDING_RANGE_KIND_REGION, 6, 12, 8, 0)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "brace fold columns are not line-relative UTF-16 columns");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#define ZR_TEST_FOLDING_LINE_RUN_COUNT 20000
/* 线性实现只需毫秒级；逐行从文件头重算列号时，同一份输入在 Debug 下要一分钟以上。 */
#define ZR_TEST_FOLDING_LINE_RUN_BUDGET_MS 5000.0

/* 整行注释的折叠列若每行都从文件头换算，耗时随行数平方增长，编辑器在大文件上会卡住。 */
static void test_lsp_folding_line_runs_stay_linear(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP folding line runs stay linear in document size";
    const TZrChar *line = "// 注释 lorem ipsum dolor\n";
    TZrSize lineLength = strlen(line);
    TZrChar *content = (TZrChar *)malloc(lineLength * ZR_TEST_FOLDING_LINE_RUN_COUNT + 1);
    SZrString *uri = ZR_NULL;
    SZrLspContext *context = ZR_NULL;
    SZrArray folds = {0};
    clock_t started;
    double elapsedMs;
    TZrBool folded;

    TEST_START(summary);
    if (content == ZR_NULL) {
        (*failures)++;
        TEST_FAIL(timer, summary, "failed to allocate the large comment document");
        return;
    }
    for (TZrSize index = 0; index < ZR_TEST_FOLDING_LINE_RUN_COUNT; index++) {
        memcpy(content + index * lineLength, line, lineLength);
    }
    content[lineLength * ZR_TEST_FOLDING_LINE_RUN_COUNT] = '\0';

    context = test_open_document(state, "file:///tmp/zr_lsp_fold_line_run_scale.zr", content, &uri);
    if (context == ZR_NULL) {
        (*failures)++;
        TEST_FAIL(timer, summary, "failed to open the large comment document");
        free(content);
        return;
    }

    started = clock();
    folded = ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds);
    elapsedMs = ((double)(clock() - started) / CLOCKS_PER_SEC) * 1000.0;
    if (!folded ||
        !folding_ranges_contain(&folds,
                                ZR_LSP_FOLDING_RANGE_KIND_COMMENT,
                                0,
                                0,
                                ZR_TEST_FOLDING_LINE_RUN_COUNT - 1,
                                23)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "the comment run did not fold with UTF-16 end columns");
    } else if (elapsedMs > ZR_TEST_FOLDING_LINE_RUN_BUDGET_MS) {
        char detail[128];
        snprintf(detail,
                 sizeof(detail),
                 "folding %d comment lines took %.0f ms; columns are being recomputed from the file start",
                 ZR_TEST_FOLDING_LINE_RUN_COUNT,
                 elapsedMs);
        (*failures)++;
        TEST_FAIL(timer, summary, detail);
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
    ZrLanguageServer_LspContext_Free(state, context);
    free(content);
}

/* 文档链接、整理/删除导入与 property 重构的范围都经同一个出口投影，非 ASCII 之后不能回到字节列。 */
static void test_lsp_link_and_action_ranges_are_utf16(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP link and code action ranges are UTF-16 after non-ASCII text";
    const TZrChar *linkContent = "/* 中 */ import(\"zr.math\");\n";
    const TZrChar *importContent = "let math = import(\"zr.math\"); // 中文";
    const TZrChar *propertyContent =
        "class Box {\n"
        "    pub property label: string { get { return \"中文\"; } }\n"
        "}\n";
    SZrLspRange importRange = {{0, 0}, {0, 0}};
    SZrLspRange propertyRange = {{1, 17}, {1, 17}};
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray links = {0};
    SZrArray actions = {0};
    SZrLspDocumentLink **linkPtr;
    TZrBool organized = ZR_FALSE;
    TZrBool removed = ZR_FALSE;
    TZrBool introduced = ZR_FALSE;
    TZrSize index;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_utf16_link_range.zr", linkContent, &uri);
    linkPtr = context != ZR_NULL &&
              ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uri, &links) &&
              links.length == 1
                  ? (SZrLspDocumentLink **)ZrCore_Array_Get(&links, 0)
                  : ZR_NULL;
    if (linkPtr == ZR_NULL ||
        *linkPtr == ZR_NULL ||
        (*linkPtr)->range.start.line != 0 ||
        (*linkPtr)->range.start.character != 16 ||
        (*linkPtr)->range.end.line != 0 ||
        (*linkPtr)->range.end.character != 23) {
        (*failures)++;
        TEST_FAIL(timer, summary, "document link range counted UTF-8 bytes after a CJK comment");
        goto cleanup;
    }
    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &links);
    memset(&links, 0, sizeof(links));
    ZrLanguageServer_LspContext_Free(state, context);

    context = test_open_document(state, "file:///tmp/zr_lsp_utf16_import_action_range.zr", importContent, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, importRange, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for an import line without a final newline");
        goto cleanup;
    }
    for (index = 0; index < actions.length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
        const TZrChar *kind = actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                                  ? test_string_text((*actionPtr)->kind)
                                  : ZR_NULL;
        if (kind == ZR_NULL) {
            continue;
        }
        if (strcmp(kind, ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS) == 0) {
            organized = single_text_edit_matches(&(*actionPtr)->edits,
                                                 0,
                                                 0,
                                                 0,
                                                 35,
                                                 "let math = import(\"zr.math\"); // 中文\n");
        } else if (strcmp(kind, ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED) == 0) {
            removed = single_text_edit_matches(&(*actionPtr)->edits, 0, 0, 0, 35, "");
        }
    }
    if (!organized || !removed) {
        (*failures)++;
        TEST_FAIL(timer, summary, "import action ranges ran past the UTF-16 end of the line");
        goto cleanup;
    }
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    memset(&actions, 0, sizeof(actions));
    ZrLanguageServer_LspContext_Free(state, context);

    context = test_open_document(state, "file:///tmp/zr_lsp_utf16_property_action_range.zr", propertyContent, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, propertyRange, &actions)) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeAction failed for an inline property accessor");
        goto cleanup;
    }
    for (index = 0; index < actions.length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
        const TZrChar *title = actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                                   ? test_string_text((*actionPtr)->title)
                                   : ZR_NULL;
        SZrLspTextEdit *edit = title != ZR_NULL &&
                               strcmp(title, "Introduce explicit field for property label") == 0
                                   ? first_text_edit(&(*actionPtr)->edits)
                                   : ZR_NULL;
        if (edit != ZR_NULL &&
            edit->range.start.line == 1 &&
            edit->range.start.character == 4 &&
            edit->range.end.line == 1 &&
            edit->range.end.character == 55) {
            introduced = ZR_TRUE;
        }
    }
    if (!introduced) {
        (*failures)++;
        TEST_FAIL(timer, summary, "property refactor range ran past the UTF-16 end of the declaration");
        goto cleanup;
    }
    TEST_PASS(timer, summary);
cleanup:
    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &links);
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static TZrBool test_remove_unused_deletes_line(SZrArray *actions, TZrInt32 startLine) {
    if (actions == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < actions->length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(actions, index);
        const TZrChar *kind = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                  ? test_string_text((*actionPtr)->kind)
                                  : ZR_NULL;
        if (kind == ZR_NULL || strcmp(kind, ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED) != 0 ||
            actionPtr == ZR_NULL || *actionPtr == ZR_NULL) {
            continue;
        }
        for (TZrSize editIndex = 0; editIndex < (*actionPtr)->edits.length; editIndex++) {
            SZrLspTextEdit **editPtr =
                (SZrLspTextEdit **)ZrCore_Array_Get(&(*actionPtr)->edits, editIndex);
            const TZrChar *newText = editPtr != ZR_NULL && *editPtr != ZR_NULL
                                         ? test_string_text((*editPtr)->newText)
                                         : ZR_NULL;
            if (editPtr != ZR_NULL && *editPtr != ZR_NULL && newText != ZR_NULL &&
                strcmp(newText, "") == 0 && (*editPtr)->range.start.line == startLine) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool test_actions_offer_import(SZrArray *actions, const TZrChar *title) {
    if (actions == ZR_NULL || title == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < actions->length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(actions, index);
        const TZrChar *actionTitle = (actionPtr != ZR_NULL && *actionPtr != ZR_NULL)
                                         ? test_string_text((*actionPtr)->title)
                                         : ZR_NULL;
        if (actionTitle != ZR_NULL && strcmp(actionTitle, title) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_lsp_template_interpolation_preserves_following_indent(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP formatting keeps statement indent after template interpolation";
    const TZrChar *content =
        "fn main(): int {\n"
        "var text = `value ${\n"
        "1\n"
        "}`;\n"
        "return 0;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray edits = {0};
    const TZrChar *formatted;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_format_interpolation_indent.zr", content, &uri);
    formatted = context != ZR_NULL &&
                ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &edits)
                    ? first_text_edit_text(&edits)
                    : ZR_NULL;
    if (formatted == ZR_NULL || strstr(formatted, "\n    return 0;\n") == ZR_NULL) {
        (*failures)++;
        TEST_FAIL(timer, summary, "interpolation closer changed the indent of the following statement");
    } else {
        TEST_PASS(timer, summary);
    }

    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_selection_ranges_keep_ancestor_containment(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP selection ranges keep ancestor containment";
    const TZrChar *header = "fn main() {\n}\n";
    const TZrChar *indented =
        "fn main(): int {\n"
        "    return 0;\n"
        "}\n";
    const TZrChar *stringBrace =
        "fn main(): int {\n"
        "    var text = \"{\";\n"
        "    return 0;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context = ZR_NULL;
    SZrArray selections = {0};
    SZrLspPosition position;
    SZrLspSelectionRange *selection = ZR_NULL;
    TZrBool headerOk = ZR_FALSE;
    TZrBool indentOk = ZR_FALSE;
    TZrBool stringOk = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_selection_header.zr", header, &uri);
    if (context != ZR_NULL &&
        test_find_position(header, "main", 0, &position) &&
        ZrLanguageServer_Lsp_GetSelectionRanges(state, context, uri, &position, 1, &selections) &&
        selections.length == 1) {
        SZrLspSelectionRange **selectionPtr = (SZrLspSelectionRange **)ZrCore_Array_Get(&selections, 0);
        selection = selectionPtr != ZR_NULL ? *selectionPtr : ZR_NULL;
        /* 块从行中的 `{` 开始；块层起点扩到行首后仍包含整行，三层都保留。 */
        headerOk = selection != ZR_NULL &&
                   selection->hasParent &&
                   selection->hasGrandParent &&
                   selection->parentRange.start.line == 0 &&
                   selection->parentRange.start.character <= selection->range.start.character &&
                   selection->parentRange.end.line >= selection->range.end.line &&
                   selection->grandParentRange.start.line == 0 &&
                   selection->grandParentRange.start.character == 0 &&
                   selection->grandParentRange.end.line == 1;
    }
    ZrLanguageServer_Lsp_FreeSelectionRanges(state, &selections);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    context = test_open_document(state, "file:///tmp/zr_lsp_selection_indent.zr", indented, &uri);
    position.line = 1;
    position.character = 0;
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetSelectionRanges(state, context, uri, &position, 1, &selections) &&
        selections.length == 1) {
        SZrLspSelectionRange **selectionPtr = (SZrLspSelectionRange **)ZrCore_Array_Get(&selections, 0);
        selection = selectionPtr != ZR_NULL ? *selectionPtr : ZR_NULL;
        indentOk = selection != ZR_NULL &&
                   selection->hasParent &&
                   selection->parentRange.start.line == 1 &&
                   selection->parentRange.start.character == 0 &&
                   selection->parentRange.start.character <= selection->range.start.character &&
                   (selection->parentRange.end.line > selection->range.end.line ||
                    selection->parentRange.end.character >= selection->range.end.character);
    }
    ZrLanguageServer_Lsp_FreeSelectionRanges(state, &selections);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    context = test_open_document(state, "file:///tmp/zr_lsp_selection_string_brace.zr", stringBrace, &uri);
    if (context != ZR_NULL &&
        test_find_position(stringBrace, "\"{", 1, &position) &&
        ZrLanguageServer_Lsp_GetSelectionRanges(state, context, uri, &position, 1, &selections) &&
        selections.length == 1) {
        SZrLspSelectionRange **selectionPtr = (SZrLspSelectionRange **)ZrCore_Array_Get(&selections, 0);
        selection = selectionPtr != ZR_NULL ? *selectionPtr : ZR_NULL;
        stringOk = selection != ZR_NULL &&
                   selection->hasParent &&
                   selection->hasGrandParent &&
                   selection->grandParentRange.start.line == 0 &&
                   selection->grandParentRange.end.line == 3;
    }
    ZrLanguageServer_Lsp_FreeSelectionRanges(state, &selections);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (!headerOk || !indentOk || !stringOk) {
        (*failures)++;
        TEST_FAIL(timer, summary, "selection ancestor did not strictly contain its descendant");
    } else {
        TEST_PASS(timer, summary);
    }
}

static void test_lsp_folding_ignores_non_code_regions_and_keeps_depth(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP folding ignores non-code regions and keeps depth-32 pairing";
    const TZrChar *lexical =
        "var text = `\n"
        "//#region hidden\n"
        "let math = import(\"zr.math\");\n"
        "let system = import(\"zr.system\");\n"
        "// one\n"
        "// two\n"
        "//#endregion\n"
        "`;\n"
        "/*\n"
        "//#region commented\n"
        "// one\n"
        "// two\n"
        "//#endregion\n"
        "*/\n"
        "let math = import(\"zr.math\");\n"
        "let system = import(\"zr.system\");\n"
        "// one\n"
        "// two\n"
        "//#region real\n"
        "fn main(): int {\n"
        "    return 1;\n"
        "}\n"
        "//#endregion\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray folds = {0};
    TZrChar *regions = ZR_NULL;
    TZrChar *braces = ZR_NULL;
    TZrBool lexicalOk = ZR_FALSE;
    TZrBool regionDepthOk = ZR_FALSE;
    TZrBool braceDepthOk = ZR_FALSE;
    const TZrInt32 depth = 33;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_fold_lexical_regions.zr", lexical, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds)) {
        lexicalOk = test_fold_has_kind_span(&folds, 14, 15, ZR_LSP_FOLDING_RANGE_KIND_IMPORTS) &&
                    test_fold_has_kind_span(&folds, 16, 18, ZR_LSP_FOLDING_RANGE_KIND_COMMENT) &&
                    test_fold_has_kind_span(&folds, 18, 22, ZR_LSP_FOLDING_RANGE_KIND_REGION) &&
                    !test_fold_has_kind_span(&folds, 1, 6, ZR_LSP_FOLDING_RANGE_KIND_REGION) &&
                    !test_fold_has_kind_span(&folds, 2, 3, ZR_LSP_FOLDING_RANGE_KIND_IMPORTS) &&
                    !test_fold_has_kind_span(&folds, 4, 5, ZR_LSP_FOLDING_RANGE_KIND_COMMENT) &&
                    !test_fold_has_kind_span(&folds, 10, 11, ZR_LSP_FOLDING_RANGE_KIND_COMMENT) &&
                    !test_fold_has_kind_span(&folds, 9, 12, ZR_LSP_FOLDING_RANGE_KIND_REGION);
    }
    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        context = ZR_NULL;
    }

    regions = (TZrChar *)malloc((size_t)depth * (strlen("//#region\n") + strlen("//#endregion\n")) + 1);
    if (regions != ZR_NULL) {
        TZrChar *cursor = regions;
        for (TZrInt32 index = 0; index < depth; index++) {
            memcpy(cursor, "//#region\n", strlen("//#region\n"));
            cursor += strlen("//#region\n");
        }
        for (TZrInt32 index = 0; index < depth; index++) {
            memcpy(cursor, "//#endregion\n", strlen("//#endregion\n"));
            cursor += strlen("//#endregion\n");
        }
        *cursor = '\0';
        context = test_open_document(state, "file:///tmp/zr_lsp_fold_region_depth.zr", regions, &uri);
        if (context != ZR_NULL &&
            ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds)) {
            regionDepthOk = test_fold_has_kind_span(&folds, 0, depth * 2 - 1, ZR_LSP_FOLDING_RANGE_KIND_REGION);
        }
        ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
            context = ZR_NULL;
        }
        free(regions);
    }

    braces = (TZrChar *)malloc((size_t)depth * 8 + 64);
    if (braces != ZR_NULL) {
        TZrChar *cursor = braces;
        memcpy(cursor, "fn outer(): int {\n", strlen("fn outer(): int {\n"));
        cursor += strlen("fn outer(): int {\n");
        for (TZrInt32 index = 0; index < depth - 1; index++) {
            memcpy(cursor, "{\n", 2);
            cursor += 2;
        }
        for (TZrInt32 index = 0; index < depth; index++) {
            memcpy(cursor, "}\n", 2);
            cursor += 2;
        }
        *cursor = '\0';
        context = test_open_document(state, "file:///tmp/zr_lsp_fold_brace_depth.zr", braces, &uri);
        if (context != ZR_NULL &&
            ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds)) {
            braceDepthOk = test_fold_has_kind_span(&folds, 0, depth * 2 - 1, ZR_LSP_FOLDING_RANGE_KIND_REGION);
        }
        ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        free(braces);
    }

    if (!lexicalOk || !regionDepthOk || !braceDepthOk) {
        (*failures)++;
        TEST_FAIL(timer, summary, "folding paired a non-code region or dropped an outer depth-32 range");
    } else {
        TEST_PASS(timer, summary);
    }
}

static void test_lsp_document_links_bind_only_import_string_argument(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP document links bind only the import string argument";
    const TZrChar *unrelated = "import(x); let name = \"zr.math\";\n";
    const TZrChar *spaced = "import( \"zr.math\" );\n";
    const TZrChar *wrapped = "import(\n    \"zr.math\"\n);\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context = ZR_NULL;
    SZrArray links = {0};
    TZrBool unrelatedOk = ZR_FALSE;
    TZrBool spacedOk = ZR_FALSE;
    TZrBool wrappedOk = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_links_unrelated_string.zr", unrelated, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uri, &links) &&
        links.length == 0) {
        unrelatedOk = ZR_TRUE;
    }
    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &links);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    context = test_open_document(state, "file:///tmp/zr_lsp_links_spaced_import.zr", spaced, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uri, &links) &&
        links.length == 1) {
        SZrLspDocumentLink **linkPtr = (SZrLspDocumentLink **)ZrCore_Array_Get(&links, 0);
        const TZrChar *target = linkPtr != ZR_NULL && *linkPtr != ZR_NULL
                                    ? test_string_text((*linkPtr)->target)
                                    : ZR_NULL;
        spacedOk = target != ZR_NULL && strstr(target, "zr.math") != ZR_NULL;
    }
    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &links);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    context = test_open_document(state, "file:///tmp/zr_lsp_links_wrapped_import.zr", wrapped, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uri, &links) &&
        links.length == 1) {
        SZrLspDocumentLink **linkPtr = (SZrLspDocumentLink **)ZrCore_Array_Get(&links, 0);
        const TZrChar *target = linkPtr != ZR_NULL && *linkPtr != ZR_NULL
                                    ? test_string_text((*linkPtr)->target)
                                    : ZR_NULL;
        wrappedOk = target != ZR_NULL && strstr(target, "zr.math") != ZR_NULL;
    }
    ZrLanguageServer_Lsp_FreeDocumentLinks(state, &links);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (!unrelatedOk || !spacedOk || !wrappedOk) {
        (*failures)++;
        TEST_FAIL(timer, summary, "documentLink did not stay on the first import string argument");
    } else {
        TEST_PASS(timer, summary);
    }
}

static void test_lsp_remove_unused_counts_template_interpolation(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP remove-unused counts template interpolation uses";
    const TZrChar *used =
        "let math = import(\"zr.math\");\n"
        "fn main(value: int): int {\n"
        "    return `${math.abs(value)}`;\n"
        "}\n";
    const TZrChar *unused =
        "let math = import(\"zr.math\");\n"
        "fn main(): int {\n"
        "    var text = `math.abs`;\n"
        "    return `${/* math.abs */ 1}`;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context = ZR_NULL;
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {6, 0}};
    TZrBool keptUse = ZR_FALSE;
    TZrBool removedText = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_unused_template_use.zr", used, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        keptUse = !test_remove_unused_deletes_line(&actions, 0);
    }
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    context = test_open_document(state, "file:///tmp/zr_lsp_unused_template_text.zr", unused, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        removedText = test_remove_unused_deletes_line(&actions, 0);
    }
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (!keptUse || !removedText) {
        (*failures)++;
        TEST_FAIL(timer, summary, "template interpolation was not treated as a real alias use");
    } else {
        TEST_PASS(timer, summary);
    }
}

/* 缺失导入插入的整行必须沿用文档的 CRLF；有导入段时看上一行行尾，没有时看文档第一处换行。 */
static void test_lsp_missing_import_insert_preserves_crlf(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP missing import insert preserves CRLF";
    const TZrChar *sources[] = {
        "let system = import(\"zr.system\");\r\n"
        "\r\n"
        "fn main(value: int): int {\r\n"
        "    return math.abs(value);\r\n"
        "}\r\n",
        "fn main(value: int): int {\r\n"
        "    return math.abs(value);\r\n"
        "}\r\n",
    };
    const SZrLspRange ranges[] = {
        {{3, 11}, {3, 15}},
        {{1, 11}, {1, 15}},
    };
    const TZrInt32 insertLines[] = {2, 0};
    TZrSize caseIndex;

    TEST_START(summary);
    for (caseIndex = 0; caseIndex < sizeof(sources) / sizeof(sources[0]); caseIndex++) {
        SZrString *uri = ZR_NULL;
        SZrLspContext *context;
        SZrArray actions = {0};
        TZrBool found = ZR_FALSE;
        char uriText[64];

        snprintf(uriText, sizeof(uriText), "file:///tmp/zr_lsp_missing_import_crlf_%u.zr", (unsigned)caseIndex);
        context = test_open_document(state, uriText, sources[caseIndex], &uri);
        if (context != ZR_NULL &&
            ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, ranges[caseIndex], &actions)) {
            for (TZrSize index = 0; index < actions.length; index++) {
                SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
                const TZrChar *title = actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                                           ? test_string_text((*actionPtr)->title)
                                           : ZR_NULL;
                if (title != ZR_NULL &&
                    strcmp(title, "Import zr.math as math") == 0 &&
                    single_text_edit_matches(&(*actionPtr)->edits,
                                             insertLines[caseIndex],
                                             0,
                                             insertLines[caseIndex],
                                             0,
                                             "let math = import(\"zr.math\");\r\n")) {
                    found = ZR_TRUE;
                }
            }
        }
        ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        if (!found) {
            char detail[96];
            snprintf(detail,
                     sizeof(detail),
                     "case %u: the inserted import line did not end with CRLF",
                     (unsigned)caseIndex);
            (*failures)++;
            TEST_FAIL(timer, summary, detail);
            return;
        }
    }
    TEST_PASS(timer, summary);
}

static void test_lsp_missing_import_is_scoped_and_sees_interpolation(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP missing import is scoped and sees template interpolation";
    const TZrChar *otherFunction =
        "fn other(value: int): int {\n"
        "    let math = import(\"zr.math\");\n"
        "    return math.abs(value);\n"
        "}\n"
        "fn main(value: int): int {\n"
        "    return math.abs(value);\n"
        "}\n";
    const TZrChar *interpolation =
        "fn main(value: int): int {\n"
        "    return `${math.abs(value)}`;\n"
        "}\n";
    const TZrChar *templateText =
        "fn main(): int {\n"
        "    var text = `math.abs`;\n"
        "    return 0;\n"
        "}\n";
    const TZrChar *untyped =
        "fn main(): int {\n"
        "    var missing;\n"
        "    return 0;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context = ZR_NULL;
    SZrArray actions = {0};
    SZrLspRange otherRange = {{5, 0}, {5, 40}};
    SZrLspRange interpolationRange = {{1, 0}, {1, 40}};
    SZrLspRange textRange = {{1, 0}, {1, 40}};
    SZrLspRange missingRange = {{1, 0}, {1, 20}};
    TZrBool otherOk = ZR_FALSE;
    TZrBool interpolationOk = ZR_FALSE;
    TZrBool textOk = ZR_FALSE;
    TZrBool missingOk = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_missing_import_other_fn.zr", otherFunction, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, otherRange, &actions)) {
        otherOk = test_actions_offer_import(&actions, "Import zr.math as math");
    }
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    context = test_open_document(state, "file:///tmp/zr_lsp_missing_import_interpolation.zr", interpolation, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, interpolationRange, &actions)) {
        interpolationOk = test_actions_offer_import(&actions, "Import zr.math as math");
    }
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    context = test_open_document(state, "file:///tmp/zr_lsp_missing_import_template_text.zr", templateText, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, textRange, &actions)) {
        textOk = !test_actions_offer_import(&actions, "Import zr.math as math");
    }
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    context = test_open_document(state, "file:///tmp/zr_lsp_no_inferred_type_action.zr", untyped, &uri);
    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, missingRange, &actions)) {
        missingOk = !test_actions_offer_import(&actions, "Insert inferred type");
        for (TZrSize index = 0; index < actions.length; index++) {
            SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(&actions, index);
            const TZrChar *title = actionPtr != ZR_NULL && *actionPtr != ZR_NULL
                                       ? test_string_text((*actionPtr)->title)
                                       : ZR_NULL;
            if (title != ZR_NULL && strstr(title, "inferred type") != ZR_NULL) {
                missingOk = ZR_FALSE;
            }
            if (actionPtr == ZR_NULL || *actionPtr == ZR_NULL) {
                continue;
            }
            for (TZrSize editIndex = 0; editIndex < (*actionPtr)->edits.length; editIndex++) {
                SZrLspTextEdit **editPtr =
                    (SZrLspTextEdit **)ZrCore_Array_Get(&(*actionPtr)->edits, editIndex);
                const TZrChar *newText = editPtr != ZR_NULL && *editPtr != ZR_NULL
                                             ? test_string_text((*editPtr)->newText)
                                             : ZR_NULL;
                if (newText != ZR_NULL && strstr(newText, ": ") != ZR_NULL &&
                    strstr(newText, "missing") != ZR_NULL) {
                    missingOk = ZR_FALSE;
                }
            }
        }
    }
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (!otherOk || !interpolationOk || !textOk || !missingOk) {
        (*failures)++;
        TEST_FAIL(timer, summary, "missing import scope, interpolation, or untyped declaration action was wrong");
    } else {
        TEST_PASS(timer, summary);
    }
}

static TZrBool test_tail_folds_survive(SZrState *state, const TZrChar *uriText, const TZrChar *content) {
    SZrString *uri = ZR_NULL;
    SZrLspContext *context = test_open_document(state, uriText, content, &uri);
    SZrArray folds = {0};
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {40, 0}};
    TZrBool ok = ZR_FALSE;

    if (context != ZR_NULL &&
        ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds) &&
        ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        ok = test_fold_has_kind_span(&folds, 1, 4, ZR_LSP_FOLDING_RANGE_KIND_REGION) &&
             test_fold_has_kind_span(&folds, 5, 7, ZR_LSP_FOLDING_RANGE_KIND_REGION) &&
             test_fold_has_kind_span(&folds, 8, 12, ZR_LSP_FOLDING_RANGE_KIND_REGION) &&
             test_fold_has_kind_span(&folds, 9, 11, ZR_LSP_FOLDING_RANGE_KIND_REGION) &&
             !test_remove_unused_deletes_line(&actions, 0);
    }
    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    return ok;
}

/* lexer 在第一个未转义的反引号结束模板；之后的代码必须照常折叠，导入也仍算被使用。 */
static void test_lsp_template_ends_at_first_unescaped_backtick(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP scanner ends a template at the first unescaped backtick";
    const TZrChar *nested =
        "let math = import(\"zr.math\");\n"
        "fn first(name: int): int {\n"
        "    var text = `value ${ \\`in${name}ner\\` } done`;\n"
        "    return name;\n"
        "}\n"
        "fn later(value: int): int {\n"
        "    return math.abs(value);\n"
        "}\n"
        "//#region tail\n"
        "fn second(): int {\n"
        "    return 2;\n"
        "}\n"
        "//#endregion\n";
    const TZrChar *unterminated =
        "let math = import(\"zr.math\");\n"
        "fn first(name: int): int {\n"
        "    var text = `value ${ call({ name`;\n"
        "    return name;\n"
        "}\n"
        "fn later(value: int): int {\n"
        "    return math.abs(value);\n"
        "}\n"
        "//#region tail\n"
        "fn second(): int {\n"
        "    return 2;\n"
        "}\n"
        "//#endregion\n";
    const TZrChar *comment =
        "let math = import(\"zr.math\");\n"
        "fn first(name: int): int {\n"
        "    var text = `value ${ name // note }`;\n"
        "    return name;\n"
        "}\n"
        "fn later(value: int): int {\n"
        "    return math.abs(value);\n"
        "}\n"
        "//#region tail\n"
        "fn second(): int {\n"
        "    return 2;\n"
        "}\n"
        "//#endregion\n";
    const TZrChar *openString =
        "var broken = \"abc\n"
        "//#region real\n"
        "fn main(): int {\n"
        "    return 1;\n"
        "}\n"
        "//#endregion\n"
        "// one\n"
        "// two\n";
    const TZrChar *unterminatedFormat =
        "fn main(): int {\n"
        "var text = `value ${name`;\n"
        "return 0;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray folds = {0};
    SZrArray edits = {0};
    const TZrChar *formatted;
    TZrBool nestedOk;
    TZrBool unterminatedOk;
    TZrBool commentOk;
    TZrBool stringOk = ZR_FALSE;
    TZrBool formatOk = ZR_FALSE;

    TEST_START(summary);
    nestedOk = test_tail_folds_survive(state, "file:///tmp/zr_lsp_scan_nested_template.zr", nested);
    unterminatedOk = test_tail_folds_survive(state, "file:///tmp/zr_lsp_scan_open_interpolation.zr", unterminated);
    commentOk = test_tail_folds_survive(state, "file:///tmp/zr_lsp_scan_interpolation_comment.zr", comment);

    context = test_open_document(state, "file:///tmp/zr_lsp_scan_open_string.zr", openString, &uri);
    if (context != ZR_NULL && ZrLanguageServer_Lsp_GetFoldingRanges(state, context, uri, &folds)) {
        stringOk = test_fold_has_kind_span(&folds, 1, 5, ZR_LSP_FOLDING_RANGE_KIND_REGION) &&
                   test_fold_has_kind_span(&folds, 2, 4, ZR_LSP_FOLDING_RANGE_KIND_REGION) &&
                   test_fold_has_kind_span(&folds, 5, 7, ZR_LSP_FOLDING_RANGE_KIND_COMMENT);
    }
    ZrLanguageServer_Lsp_FreeFoldingRanges(state, &folds);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    context = test_open_document(state, "file:///tmp/zr_lsp_scan_open_interpolation_format.zr",
                                 unterminatedFormat, &uri);
    formatted = context != ZR_NULL &&
                ZrLanguageServer_Lsp_GetFormatting(state, context, uri, ZR_NULL, &edits)
                    ? first_text_edit_text(&edits)
                    : ZR_NULL;
    formatOk = formatted != ZR_NULL &&
               strstr(formatted, "\n    return 0;\n}\n") != ZR_NULL;
    ZrLanguageServer_Lsp_FreeTextEdits(state, &edits);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (!nestedOk || !unterminatedOk || !commentOk || !stringOk || !formatOk) {
        (*failures)++;
        TEST_FAIL(timer, summary, "text after a closed template or an unterminated string was not scanned as code");
    } else {
        TEST_PASS(timer, summary);
    }
}

/* 声明头所在行仍要给出词、行、块三层，且每层包含下一层。 */
static void test_lsp_selection_header_line_keeps_block_layer(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP selection on a declaration header keeps the block layer";
    const TZrChar *content =
        "class Sample {\n"
        "    pub fn run(value: int): int {\n"
        "        return value;\n"
        "    }\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray selections = {0};
    SZrLspPosition position;
    TZrBool ok = ZR_FALSE;

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_selection_header_block.zr", content, &uri);
    if (context != ZR_NULL &&
        test_find_position(content, "run", 0, &position) &&
        ZrLanguageServer_Lsp_GetSelectionRanges(state, context, uri, &position, 1, &selections) &&
        selections.length == 1) {
        SZrLspSelectionRange **selectionPtr = (SZrLspSelectionRange **)ZrCore_Array_Get(&selections, 0);
        SZrLspSelectionRange *selection = selectionPtr != ZR_NULL ? *selectionPtr : ZR_NULL;
        ok = selection != ZR_NULL &&
             selection->hasParent &&
             selection->hasGrandParent &&
             selection->range.start.line == 1 &&
             selection->parentRange.start.line == 1 &&
             selection->parentRange.start.character == 0 &&
             selection->parentRange.end.line == 1 &&
             selection->grandParentRange.start.line == 1 &&
             selection->grandParentRange.start.character == 0 &&
             selection->grandParentRange.end.line == 3;
    }
    ZrLanguageServer_Lsp_FreeSelectionRanges(state, &selections);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (!ok) {
        (*failures)++;
        TEST_FAIL(timer, summary, "header line selection lost the block layer or broke containment");
    } else {
        TEST_PASS(timer, summary);
    }
}

// These static cases depend on the diagnostic helpers declared above.
#include "test_lsp_diagnostic_safe_fix_cases.h"
#include "test_lsp_current_syntax_formatting_cases.h"

/* CMake 独立目标在同一 VM state 上运行格式化、折叠、链接、CodeAction、
 * CodeLens 与层级查询；内联头文件补充当前语法及安全修复的案例。 */
int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    int failures = 0;

    printf("==========\n");
    printf("Language Server - Advanced Editor Feature Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("Fail - failed to create VM state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    test_lsp_document_formatting_returns_single_document_edit(state, &failures);
    test_lsp_formatting_skips_noop_edits(state, &failures);
    test_lsp_formatting_reports_utf16_columns(state, &failures);
    test_lsp_formatting_honors_options_and_crlf(state, &failures);
    test_lsp_selection_word_uses_utf16_columns(state, &failures);
    test_lsp_folding_columns_are_utf16(state, &failures);
    test_lsp_missing_import_range_uses_utf16_columns(state, &failures);
    test_lsp_organize_imports_preserves_crlf(state, &failures);
    test_lsp_formatting_keeps_trailing_lone_cr_outside_edit(state, &failures);
    test_lsp_folding_columns_stay_utf16_on_later_lines(state, &failures);
    test_lsp_folding_line_runs_stay_linear(state, &failures);
    test_lsp_link_and_action_ranges_are_utf16(state, &failures);
    test_lsp_missing_import_insert_preserves_crlf(state, &failures);
    test_lsp_formatting_does_not_emit_removed_syntax(state, &failures);
    test_lsp_formatting_preserves_current_compile_tool_syntax(state, &failures);
    test_lsp_folding_and_selection_ranges(state, &failures);
    test_lsp_folding_ranges_include_import_regions(state, &failures);
    test_lsp_formatting_ignores_non_code_braces(state, &failures);
    test_lsp_folding_ignores_non_code_braces(state, &failures);
    test_lsp_template_interpolation_preserves_following_indent(state, &failures);
    test_lsp_selection_ranges_keep_ancestor_containment(state, &failures);
    test_lsp_folding_ignores_non_code_regions_and_keeps_depth(state, &failures);
    test_lsp_document_links_resolve_import_literals(state, &failures);
    test_lsp_document_links_ignore_non_code_import_text(state, &failures);
    test_lsp_document_links_bind_only_import_string_argument(state, &failures);
    test_lsp_document_links_resolve_zrp_paths(state, &failures);
    test_lsp_document_links_resolve_virtual_module_links(state, &failures);
    test_lsp_code_action_organizes_imports(state, &failures);
    test_lsp_code_action_organizes_imports_after_module(state, &failures);
    test_lsp_code_action_organizes_import_aliases(state, &failures);
    test_lsp_code_action_organizes_duplicate_alias_imports(state, &failures);
    test_lsp_code_action_does_not_organize_import_text_in_strings(state, &failures);
    test_lsp_code_action_skips_organized_imports(state, &failures);
    test_lsp_code_action_removes_unused_alias_imports(state, &failures);
    test_lsp_code_action_ignores_text_mentions_when_removing_unused_alias_imports(state, &failures);
    test_lsp_remove_unused_counts_template_interpolation(state, &failures);
    test_lsp_missing_import_is_scoped_and_sees_interpolation(state, &failures);
    test_lsp_template_ends_at_first_unescaped_backtick(state, &failures);
    test_lsp_selection_header_line_keeps_block_layer(state, &failures);
    test_lsp_code_action_inserts_missing_semicolon(state, &failures);
    test_lsp_code_action_inserts_semicolon_before_line_comment(state, &failures);
    test_lsp_code_action_skips_semicolon_inside_block_comment(state, &failures);
    test_lsp_code_action_consumes_machine_applicable_diagnostic_fix(
            state, &failures);
    test_lsp_code_action_inserts_missing_declaration_body_open(
            state, &failures);
    test_lsp_code_action_inserts_missing_statement_body_open(
            state, &failures);
    test_lsp_code_action_inserts_missing_block_close(state, &failures);
    test_lsp_code_action_inserts_missing_catch_pattern_close(
            state, &failures);
    test_lsp_code_action_inserts_missing_using_resource_close(
            state, &failures);
    test_lsp_code_action_inserts_missing_for_header_close(state, &failures);
    test_lsp_code_action_inserts_missing_for_header_separator(
            state, &failures);
    test_lsp_code_action_inserts_missing_foreach_header_close(
            state, &failures);
    test_lsp_code_action_inserts_missing_foreach_in_keyword(
            state, &failures);
    test_lsp_code_action_inserts_missing_switch_case_header_close(
            state, &failures);
    test_lsp_code_action_inserts_missing_switch_body_close(state, &failures);
    test_lsp_code_action_inserts_missing_extern_spec_close(state, &failures);
    test_lsp_code_action_inserts_missing_declaration_body_close(
            state, &failures);
    test_lsp_code_action_inserts_missing_condition_close(state, &failures);
    test_lsp_code_action_inserts_missing_index_close(state, &failures);
    test_lsp_code_action_inserts_missing_parameter_list_close(
            state, &failures);
    test_lsp_code_action_inserts_missing_call_close(state, &failures);
    test_lsp_code_action_inserts_missing_group_close(state, &failures);
    test_lsp_code_action_inserts_missing_array_close(state, &failures);
    test_lsp_code_action_inserts_missing_array_element_separator(
            state, &failures);
    test_lsp_code_action_skips_array_element_assignment_fix(
            state, &failures);
    test_lsp_code_action_inserts_missing_object_close(state, &failures);
    test_lsp_code_action_inserts_missing_object_computed_key_close(
            state, &failures);
    test_lsp_code_action_inserts_missing_object_property_colon(
            state, &failures);
    test_lsp_code_action_inserts_missing_object_property_separator(
            state, &failures);
    test_lsp_code_action_inserts_missing_conditional_colon(
            state, &failures);
    test_lsp_code_action_skips_conditional_branch_expression_fixes(
            state, &failures);
    test_lsp_code_action_inserts_missing_using_object_pattern_close(
            state, &failures);
    test_lsp_code_action_skips_placeholder_diagnostic_fix(state, &failures);
    test_lsp_code_action_migrates_removed_ownership_member_call(
            state, &failures);
    test_lsp_code_action_inserts_missing_native_import(state, &failures);
    test_lsp_code_action_uses_requested_range_for_missing_import(state, &failures);
    test_lsp_code_action_skips_existing_import_alias(state, &failures);
    test_lsp_code_action_ignores_non_code_missing_import_text(state, &failures);
    test_lsp_code_action_ignores_multiline_block_comment_missing_import(state, &failures);
    test_lsp_code_lens_exposes_test_command(state, &failures);
    test_lsp_code_lens_rejects_invalid_bound_test_signature(state, &failures);
    test_lsp_code_lens_ignores_non_code_test_markers(state, &failures);
    test_lsp_code_lens_ignores_unbound_test_like_attribute(state, &failures);
    test_lsp_code_lens_exposes_reference_count(state, &failures);
    test_lsp_call_hierarchy_prepare_returns_symbol_item(state, &failures);
    test_lsp_call_hierarchy_outgoing_returns_direct_calls(state, &failures);
    test_lsp_call_hierarchy_incoming_returns_direct_callers(state, &failures);
    test_lsp_call_hierarchy_ignores_non_code_call_mentions(state, &failures);
    test_lsp_type_hierarchy_prepare_returns_type_item(state, &failures);
    test_lsp_type_hierarchy_supertypes_returns_direct_base(state, &failures);
    test_lsp_type_hierarchy_subtypes_returns_direct_derived(state, &failures);
    test_lsp_definition_ignores_non_code_receiver_member_text(state, &failures);
    test_lsp_advanced_editor_features_return_empty_for_empty_document(state, &failures);
    test_lsp_advanced_editor_features_return_empty_for_unopened_documents(state, &failures);

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("Advanced editor feature tests completed with %d failure(s)\n", failures);
    printf("==========\n");
    return failures == 0 ? 0 : 1;
}
