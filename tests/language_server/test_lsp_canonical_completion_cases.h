#ifndef ZR_VM_TEST_LSP_CANONICAL_COMPLETION_CASES_H
#define ZR_VM_TEST_LSP_CANONICAL_COMPLETION_CASES_H

static SZrCompletionItem *canonical_completion_find_label(
        SZrArray *items,
        const TZrChar *expectedLabel) {
    if (items == ZR_NULL || expectedLabel == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < items->length; index++) {
        SZrCompletionItem **itemPtr =
                (SZrCompletionItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL &&
            (*itemPtr)->label != ZR_NULL &&
            strcmp(
                    ZrCore_String_GetNativeString((*itemPtr)->label),
                    expectedLabel) == 0) {
            return *itemPtr;
        }
    }
    return ZR_NULL;
}

static void test_canonical_visible_symbol_completion_survives_symbol_table_detachment(
        SZrState *state) {
    static const TZrChar *content =
            "fn make(seed: int) {\n"
            "    return seed + 0;\n"
            "}\n"
            "fn main() {\n"
            "    var canonicalLocal: int = 1;\n"
            "    var missing;\n"
            "    make(1);\n"
            "}\n";
    static const TZrChar *uriText =
            "file:///canonical_visible_symbol_completion.zr";
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrSymbolTable *detachedSymbolTable = ZR_NULL;
    SZrFileVersion *fileVersion = ZR_NULL;
    SZrAstNode *detachedFileAst = ZR_NULL;
    SZrArray completions = {0};
    SZrLspPosition position = {6, 4};
    SZrParityTimer timer;
    SZrCompletionItem *canonicalItem = ZR_NULL;
    SZrCompletionItem *missingItem = ZR_NULL;
    SZrCompletionItem *makeItem = ZR_NULL;
    TZrBool passed = ZR_FALSE;

    TEST_START("LSP Canonical Visible Symbol Completion Is Independent Of Symbol Table");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U)) {
        TEST_FAIL(
                timer,
                "LSP Canonical Visible Symbol Completion Is Independent Of Symbol Table",
                "Could not prepare completion snapshot");
        goto cleanup;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        analyzer->symbolTable == ZR_NULL) {
        TEST_FAIL(
                timer,
                "LSP Canonical Visible Symbol Completion Is Independent Of Symbol Table",
                "Completion analyzer did not expose a semantic snapshot");
        goto cleanup;
    }
    detachedSymbolTable = analyzer->symbolTable;
    analyzer->symbolTable = ZR_NULL;
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->ast == ZR_NULL) {
        TEST_FAIL(
                timer,
                "LSP Canonical Visible Symbol Completion Is Independent Of Symbol Table",
                "Completion document did not expose a current AST");
        goto cleanup;
    }
    detachedFileAst = fileVersion->ast;
    fileVersion->ast = ZR_NULL;
    ZrCore_Array_Init(
            state,
            &completions,
            sizeof(SZrCompletionItem *),
            ZR_LSP_ARRAY_INITIAL_CAPACITY);
    passed = ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(
            state, context, uri, position, &completions);
    canonicalItem = passed
            ? canonical_completion_find_label(&completions, "canonicalLocal")
            : ZR_NULL;
    missingItem = passed
            ? canonical_completion_find_label(&completions, "missing")
            : ZR_NULL;
    makeItem = passed
            ? canonical_completion_find_label(&completions, "make")
            : ZR_NULL;
    passed = passed && canonicalItem != ZR_NULL &&
             canonicalItem->kind != ZR_NULL &&
             strcmp(
                     ZrCore_String_GetNativeString(canonicalItem->kind),
                     "variable") == 0 &&
             canonicalItem->detail != ZR_NULL &&
             strcmp(
                     ZrCore_String_GetNativeString(canonicalItem->detail),
                     "int") == 0 &&
             missingItem != ZR_NULL &&
             missingItem->detail != ZR_NULL &&
             strcmp(
                     ZrCore_String_GetNativeString(missingItem->detail),
                     "cannot infer exact type") == 0 &&
             makeItem != ZR_NULL &&
             makeItem->detail != ZR_NULL &&
             strstr(
                     ZrCore_String_GetNativeString(makeItem->detail),
                     "make(seed: int): int") != ZR_NULL;
    analyzer->symbolTable = detachedSymbolTable;
    detachedSymbolTable = ZR_NULL;
    fileVersion->ast = detachedFileAst;
    detachedFileAst = ZR_NULL;
    if (passed) {
        TEST_PASS(
                timer,
                "LSP Canonical Visible Symbol Completion Is Independent Of Symbol Table");
    } else {
        TEST_FAIL(
                timer,
                "LSP Canonical Visible Symbol Completion Is Independent Of Symbol Table",
                "Lexical completion did not come from parser VisibleSymbols facts");
    }

cleanup:
    if (detachedSymbolTable != ZR_NULL && analyzer != ZR_NULL) {
        analyzer->symbolTable = detachedSymbolTable;
    }
    if (detachedFileAst != ZR_NULL && fileVersion != ZR_NULL) {
        fileVersion->ast = detachedFileAst;
    }
    for (TZrSize index = 0; index < completions.length; index++) {
        SZrCompletionItem **itemPtr =
                (SZrCompletionItem **)ZrCore_Array_Get(&completions, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            ZrLanguageServer_CompletionItem_Free(state, *itemPtr);
        }
    }
    if (completions.isValid) {
        ZrCore_Array_Free(state, &completions);
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#endif
