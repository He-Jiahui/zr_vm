//
// Focused code-lens LSP range regressions for UTF-16 columns.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"
#include "zr_vm_language_server/semantic_analyzer.h"

#include "../../zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h"

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

static TZrBool test_lens_matches(SZrArray *lenses,
                                 const TZrChar *title,
                                 TZrInt32 line,
                                 TZrInt32 character) {
    if (lenses == ZR_NULL || title == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < lenses->length; index++) {
        SZrLspCodeLens **lensPtr = (SZrLspCodeLens **)ZrCore_Array_Get(lenses, index);
        SZrLspCodeLens *lens = lensPtr != ZR_NULL ? *lensPtr : ZR_NULL;
        const TZrChar *lensTitle = lens != ZR_NULL ? test_string_text(lens->commandTitle) : ZR_NULL;
        if (lensTitle != ZR_NULL &&
            strcmp(lensTitle, title) == 0 &&
            lens->range.start.line == line &&
            lens->range.start.character == character &&
            lens->hasPositionArgument &&
            lens->positionArgument.line == line &&
            lens->positionArgument.character == character) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool test_lens_has_title(SZrArray *lenses, const TZrChar *title) {
    if (lenses == ZR_NULL || title == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < lenses->length; index++) {
        SZrLspCodeLens **lensPtr =
                (SZrLspCodeLens **)ZrCore_Array_Get(lenses, index);
        SZrLspCodeLens *lens = lensPtr != ZR_NULL ? *lensPtr : ZR_NULL;
        const TZrChar *lensTitle =
                lens != ZR_NULL ? test_string_text(lens->commandTitle) : ZR_NULL;
        if (lensTitle != ZR_NULL && strcmp(lensTitle, title) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void describe_first_lens(SZrArray *lenses) {
    if (lenses != ZR_NULL && lenses->length > 0) {
        SZrLspCodeLens **lensPtr = (SZrLspCodeLens **)ZrCore_Array_Get(lenses, 0);
        SZrLspCodeLens *lens = lensPtr != ZR_NULL ? *lensPtr : ZR_NULL;
        if (lens != ZR_NULL) {
            printf(" first=%d:%d pos=%d:%d title=%s",
                   lens->range.start.line,
                   lens->range.start.character,
                   lens->positionArgument.line,
                   lens->positionArgument.character,
                   test_string_text(lens->commandTitle));
        }
    }
}

static TZrBool test_code_lens_reference_count_after_utf8_prefix_uses_utf16_columns(SZrState *state) {
    const TZrChar *content =
        "/* \xCE\xBB */ fn helper(value: int): int {\n"
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
    SZrLspContext *context;
    SZrString *uri;
    SZrArray lenses = {0};
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, "file:///code_lens_utf16_ranges.zr", strlen("file:///code_lens_utf16_ranges.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !ZrLanguageServer_Lsp_GetCodeLens(state, context, uri, &lenses)) {
        printf("FAIL: CodeLens UTF-16 ranges could not open fixture or collect lenses\n");
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        return ZR_FALSE;
    }

    passed = test_lens_matches(&lenses, "2 references", 0, 11);
    if (!passed) {
        printf("FAIL: CodeLens reference count expected UTF-16 range/position start 0:11 but got count=%llu",
               (unsigned long long)lenses.length);
        describe_first_lens(&lenses);
        printf("\n");
    } else {
        printf("PASS: CodeLens reference count after UTF-8 prefix uses UTF-16 columns\n");
    }

    ZrLanguageServer_Lsp_FreeCodeLens(state, &lenses);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_code_lens_enumerates_canonical_declarations_without_symbol_table(
        SZrState *state) {
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
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrSymbolTable *savedSymbolTable;
    SZrArray lenses = {0};
    TZrBool querySucceeded;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///code_lens_canonical_declarations.zr",
            strlen("file:///code_lens_canonical_declarations.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1)) {
        printf("FAIL: CodeLens canonical declaration fixture could not be prepared\n");
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        analyzer->symbolTable == ZR_NULL) {
        printf("FAIL: CodeLens canonical declaration fixture has no semantic snapshot\n");
        ZrLanguageServer_LspContext_Free(state, context);
        return ZR_FALSE;
    }

    savedSymbolTable = analyzer->symbolTable;
    analyzer->symbolTable = ZR_NULL;
    querySucceeded = ZrLanguageServer_Lsp_GetCodeLens(
            state, context, uri, &lenses);
    analyzer->symbolTable = savedSymbolTable;

    passed = querySucceeded && lenses.length == 1U &&
             test_lens_matches(&lenses, "2 references", 0, 3);
    if (!passed) {
        printf("FAIL: CodeLens expected canonical declaration reference count without "
               "LSP symbol enumeration; success=%d count=%llu",
               querySucceeded,
               (unsigned long long)lenses.length);
        describe_first_lens(&lenses);
        printf("\n");
    } else {
        printf("PASS: CodeLens enumerates canonical declarations without symbol table\n");
    }

    ZrLanguageServer_Lsp_FreeCodeLens(state, &lenses);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_code_lens_rebinds_to_current_semantic_snapshot(
        SZrState *state) {
    const TZrChar *contentV1 =
        "fn helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "fn first(value: int): int { return helper(value); }\n"
        "fn second(value: int): int { return helper(value); }\n";
    const TZrChar *contentV2 =
        "fn helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "fn latest(value: int): int { return helper(value); }\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrArray firstLenses = {0};
    SZrArray secondLenses = {0};
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///code_lens_current_snapshot.zr",
            strlen("file:///code_lens_current_snapshot.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, contentV1, strlen(contentV1), 1) ||
        !ZrLanguageServer_Lsp_GetCodeLens(
                state, context, uri, &firstLenses)) {
        printf("FAIL: CodeLens current-snapshot fixture version 1 failed\n");
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        return ZR_FALSE;
    }

    passed = test_lens_has_title(&firstLenses, "2 references");
    ZrLanguageServer_Lsp_FreeCodeLens(state, &firstLenses);
    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, contentV2, strlen(contentV2), 2) ||
        !ZrLanguageServer_Lsp_GetCodeLens(
                state, context, uri, &secondLenses)) {
        printf("FAIL: CodeLens current-snapshot fixture version 2 failed\n");
        ZrLanguageServer_LspContext_Free(state, context);
        return ZR_FALSE;
    }

    passed = passed && secondLenses.length == 1U &&
             test_lens_has_title(&secondLenses, "1 reference") &&
             !test_lens_has_title(&secondLenses, "2 references");
    if (!passed) {
        printf("FAIL: CodeLens retained a stale reference count after version update");
        describe_first_lens(&secondLenses);
        printf("\n");
    } else {
        printf("PASS: CodeLens rebinds to current semantic snapshot\n");
    }

    ZrLanguageServer_Lsp_FreeCodeLens(state, &secondLenses);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_code_lens_fails_closed_for_unresolved_declaration(
        SZrState *state) {
    const TZrChar *content =
        "fn helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "fn caller(value: int): int { return helper(value); }\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrArray lenses = {0};
    TZrSize unresolvedDeclarationCount = 0U;
    TZrBool querySucceeded;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///code_lens_unresolved_declaration.zr",
            strlen("file:///code_lens_unresolved_declaration.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1)) {
        printf("FAIL: CodeLens unresolved declaration fixture could not be prepared\n");
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL) {
        for (TZrSize index = 0U;
             index < analyzer->semanticContext->referenceFacts.length;
             index++) {
            SZrSemanticReferenceFact *fact =
                    (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                            &analyzer->semanticContext->referenceFacts, index);
            if (fact != ZR_NULL &&
                fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
                fact->isResolved && fact->name != ZR_NULL &&
                strcmp(test_string_text(fact->name), "helper") == 0) {
                fact->isResolved = ZR_FALSE;
                unresolvedDeclarationCount++;
            }
        }
    }
    if (unresolvedDeclarationCount == 0U) {
        printf("FAIL: CodeLens unresolved declaration fixture has no helper fact\n");
        ZrLanguageServer_LspContext_Free(state, context);
        return ZR_FALSE;
    }

    querySucceeded = ZrLanguageServer_Lsp_GetCodeLens(
            state, context, uri, &lenses);
    passed = querySucceeded && !test_lens_has_title(&lenses, "1 reference");
    if (!passed) {
        printf("FAIL: CodeLens inferred an unresolved declaration identity");
        describe_first_lens(&lenses);
        printf("\n");
    } else {
        printf("PASS: CodeLens fails closed for unresolved declaration identity\n");
    }

    ZrLanguageServer_Lsp_FreeCodeLens(state, &lenses);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    TZrBool passed;

    printf("==========\n");
    printf("Language Server - CodeLens UTF-16 Range Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: CodeLens UTF-16 range tests could not create VM state\n");
        return 1;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    passed = test_code_lens_reference_count_after_utf8_prefix_uses_utf16_columns(
            global->mainThreadState);
    passed = test_code_lens_enumerates_canonical_declarations_without_symbol_table(
            global->mainThreadState) && passed;
    passed = test_code_lens_rebinds_to_current_semantic_snapshot(
            global->mainThreadState) && passed;
    passed = test_code_lens_fails_closed_for_unresolved_declaration(
            global->mainThreadState) && passed;
    ZrCore_GlobalState_Free(global);

    if (!passed) {
        printf("\nFAILED: CodeLens UTF-16 range tests failed\n");
        return 1;
    }

    printf("\nPASSED: CodeLens UTF-16 range tests\n");
    return 0;
}
