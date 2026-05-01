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

static void test_lsp_document_formatting_returns_single_document_edit(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP document formatting returns a full-document edit";
    const TZrChar *content =
        "class Sample {\n"
        "pub func run(value: int): int {\n"
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
        !ZrLanguageServer_Lsp_GetFormatting(state, context, uri, &edits) ||
        edits.length != 1 ||
        !text_edit_contains(&edits, "    pub func run") ||
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
        "    pub func run(value: int): int {\n"
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
        !ZrLanguageServer_Lsp_GetFormatting(state, context, uri, &documentEdits) ||
        !ZrLanguageServer_Lsp_GetRangeFormatting(state, context, uri, range, &rangeEdits)) {
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
        "    pub func run(value: int): int {\n"
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
        "%import(\"zr.system\");\n"
        "%import(\"zr.math\");\n"
        "%import(\"zr.container\");\n"
        "\n"
        "// first note\n"
        "// second note\n"
        "\n"
        "//#region setup\n"
        "func main(): int {\n"
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
        "func render(): string {\n"
        "let text = \"{\";\n"
        "// }\n"
        "/* { */\n"
        "return text;\n"
        "}\n";
    const TZrChar *expected =
        "func render(): string {\n"
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
                ZrLanguageServer_Lsp_GetFormatting(state, context, uri, &edits)
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
        "func render(): string {\n"
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
    const TZrChar *content = "%import(\"zr.math\");\n";
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
        "var text = \"%import(\\\"zr.system\\\")\";\n"
        "// %import(\"zr.network\");\n"
        "/*\n"
        "%import(\"zr.container\");\n"
        "*/\n"
        "%import(\"zr.math\");\n";
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
        "%import(\"zr.system\");\n"
        "%import(\"zr.math\");\n"
        "%import(\"zr.system\");\n"
        "\n"
        "func main(): int { return 0; }\n";
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
                                "%import(\"zr.math\");\n%import(\"zr.system\");")) {
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
        "module \"demo\";\n"
        "\n"
        "%import(\"zr.system\");\n"
        "%import(\"zr.math\");\n"
        "\n"
        "func main(): int { return 0; }\n";
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
                                "%import(\"zr.math\");\n%import(\"zr.system\");")) {
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
        "var system = %import(\"zr.system\");\n"
        "var math = %import(\"zr.math\");\n"
        "var system = %import(\"zr.system\");\n"
        "\n"
        "func main(): int { return 0; }\n";
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
                                "var math = %import(\"zr.math\");\nvar system = %import(\"zr.system\");")) {
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
        "var system = %import(\"zr.system\");\n"
        "var math = %import(\"zr.math\");\n"
        "var system = %import(\"zr.network\");\n"
        "\n"
        "func main(): int { return 0; }\n";
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
                                "var math = %import(\"zr.math\");\nvar system = %import(\"zr.system\");") ||
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
        "var system = %import(\"zr.system\");\n"
        "var text = \"%import(\\\"zr.math\\\")\";\n"
        "\n"
        "func main(): int { return 0; }\n";
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
        "%import(\"zr.math\");\n"
        "%import(\"zr.system\");\n"
        "\n"
        "func main(): int { return 0; }\n";
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

static void test_lsp_code_action_removes_unused_alias_imports(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action removes unused alias imports";
    const TZrChar *content =
        "var math = %import(\"zr.math\");\n"
        "var system = %import(\"zr.system\");\n"
        "\n"
        "func main(value: int): int {\n"
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
        "var math = %import(\"zr.math\");\n"
        "var system = %import(\"zr.system\");\n"
        "\n"
        "func main(value: int): int {\n"
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

static void test_lsp_code_action_inserts_missing_native_import(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action inserts a missing native module import";
    const TZrChar *content =
        "func main(value: int): int {\n"
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
                text_edit_contains(&(*actionPtr)->edits, "var math = %import(\"zr.math\");\n")) {
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

static void test_lsp_code_action_uses_requested_range_for_missing_import(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP code action uses requested range for missing import";
    const TZrChar *content =
        "func main(value: int): int {\n"
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
                                                           "var math = %import(\"zr.math\");\n");
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
        "var math = %import(\"zr.math\");\n"
        "\n"
        "func main(value: int): int {\n"
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
        "func main(value: int): int {\n"
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
        "func main(value: int): int {\n"
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
        "%test(\"advanced\") {\n"
        "    return 1;\n"
        "}\n";
    SZrString *uri = ZR_NULL;
    SZrLspContext *context;
    SZrArray lenses = {0};

    TEST_START(summary);
    context = test_open_document(state, "file:///tmp/zr_lsp_lens.zr", content, &uri);
    if (context == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetCodeLens(state, context, uri, &lenses) ||
        lenses.length != 1) {
        (*failures)++;
        TEST_FAIL(timer, summary, "codeLens did not return a test command");
    } else {
        SZrLspCodeLens **lensPtr = (SZrLspCodeLens **)ZrCore_Array_Get(&lenses, 0);
        const TZrChar *command = (lensPtr != ZR_NULL && *lensPtr != ZR_NULL)
                                     ? test_string_text((*lensPtr)->command)
                                     : ZR_NULL;
        if (command == ZR_NULL || strcmp(command, "zr.runCurrentProject") != 0) {
            (*failures)++;
            TEST_FAIL(timer, summary, "codeLens command did not match the VS Code run command");
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
        "func helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "\n"
        "func first(value: int): int {\n"
        "    return helper(value);\n"
        "}\n"
        "\n"
        "func second(value: int): int {\n"
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
        "    pub func run(value: int): int {\n"
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
        "func helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "\n"
        "func run(value: int): int {\n"
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
        "func helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "\n"
        "func run(value: int): int {\n"
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

static void test_lsp_type_hierarchy_prepare_returns_type_item(SZrState *state, int *failures) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP type hierarchy prepare returns a type symbol item";
    const TZrChar *content =
        "class Sample {\n"
        "    pub func run(): int { return 1; }\n"
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
        !ZrLanguageServer_Lsp_GetFormatting(state, context, uri, &formatting) ||
        !ZrLanguageServer_Lsp_GetRangeFormatting(state, context, uri, emptyRange, &rangeFormatting) ||
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
        (void)ZrLanguageServer_Lsp_GetFormatting(state, context, uri, &formatting);
        (void)ZrLanguageServer_Lsp_GetRangeFormatting(state, context, uri, range, &rangeFormatting);
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
    test_lsp_folding_and_selection_ranges(state, &failures);
    test_lsp_folding_ranges_include_import_regions(state, &failures);
    test_lsp_formatting_ignores_non_code_braces(state, &failures);
    test_lsp_folding_ignores_non_code_braces(state, &failures);
    test_lsp_document_links_resolve_import_literals(state, &failures);
    test_lsp_document_links_ignore_non_code_import_text(state, &failures);
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
    test_lsp_code_action_inserts_missing_semicolon(state, &failures);
    test_lsp_code_action_inserts_semicolon_before_line_comment(state, &failures);
    test_lsp_code_action_inserts_missing_native_import(state, &failures);
    test_lsp_code_action_uses_requested_range_for_missing_import(state, &failures);
    test_lsp_code_action_skips_existing_import_alias(state, &failures);
    test_lsp_code_action_ignores_non_code_missing_import_text(state, &failures);
    test_lsp_code_action_ignores_multiline_block_comment_missing_import(state, &failures);
    test_lsp_code_lens_exposes_test_command(state, &failures);
    test_lsp_code_lens_exposes_reference_count(state, &failures);
    test_lsp_call_hierarchy_prepare_returns_symbol_item(state, &failures);
    test_lsp_call_hierarchy_outgoing_returns_direct_calls(state, &failures);
    test_lsp_call_hierarchy_incoming_returns_direct_callers(state, &failures);
    test_lsp_type_hierarchy_prepare_returns_type_item(state, &failures);
    test_lsp_type_hierarchy_supertypes_returns_direct_base(state, &failures);
    test_lsp_type_hierarchy_subtypes_returns_direct_derived(state, &failures);
    test_lsp_advanced_editor_features_return_empty_for_empty_document(state, &failures);
    test_lsp_advanced_editor_features_return_empty_for_unopened_documents(state, &failures);

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("Advanced editor feature tests completed with %d failure(s)\n", failures);
    printf("==========\n");
    return failures == 0 ? 0 : 1;
}
