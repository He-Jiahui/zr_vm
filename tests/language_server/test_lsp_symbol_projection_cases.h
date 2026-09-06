#ifndef ZR_VM_TEST_LSP_SYMBOL_PROJECTION_CASES_H
#define ZR_VM_TEST_LSP_SYMBOL_PROJECTION_CASES_H

static void test_symbol_projection_rejects_unavailable_canonical_identity(
        SZrState *state,
        TZrBool detachSemanticContext) {
    static const TZrChar *content =
            "fn read(): int {\n"
            "    var value = 1;\n"
            "    return value;\n"
            "}\n";
    const TZrChar *summary = detachSemanticContext
            ? "LSP Symbol Projection Rejects Missing Semantic Context"
            : "LSP Symbol Projection Rejects Mismatched Canonical Identity";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition position;
    SZrFilePosition filePosition;
    SZrFileRange queryRange;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrParserSemanticSymbolQuery canonicalSymbol = {0};
    SZrSemanticContext *savedSemanticContext = ZR_NULL;
    SZrSymbol *symbol = ZR_NULL;
    SZrSymbol *resolvedSymbol = ZR_NULL;
    TZrSymbolId savedSemanticId = ZR_SEMANTIC_ID_INVALID;
    const TZrChar *failure = "Could not prepare canonical declaration fixture";
    TZrBool valid = ZR_FALSE;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_CreateFromNative(
            state, "file:///symbol_projection_identity.zr");
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "var value", 0U, 5, &position)) {
        goto cleanup;
    }
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, position);
    queryRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        !ZrParser_SemanticQuery_SymbolAt(
                analyzer->semanticContext, queryRange, ZR_NULL, &canonicalSymbol) ||
        canonicalSymbol.symbolId == ZR_SEMANTIC_ID_INVALID) {
        goto cleanup;
    }
    symbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(
            analyzer, queryRange);
    if (symbol == ZR_NULL || symbol->semanticId != canonicalSymbol.symbolId) {
        goto cleanup;
    }

    if (detachSemanticContext) {
        savedSemanticContext = analyzer->semanticContext;
        analyzer->semanticContext = ZR_NULL;
    } else {
        savedSemanticId = symbol->semanticId;
        symbol->semanticId = ZR_SEMANTIC_ID_INVALID;
    }
    if (ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, queryRange) != ZR_NULL) {
        failure = "Canonical analyzer lookup accepted unavailable symbol identity";
        goto cleanup;
    }
    resolvedSymbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(
            analyzer, queryRange);
    valid = resolvedSymbol == ZR_NULL;
    failure = "Declaration range fallback revived a symbol after canonical lookup failed";

cleanup:
    if (analyzer != ZR_NULL && savedSemanticContext != ZR_NULL) {
        analyzer->semanticContext = savedSemanticContext;
    }
    if (symbol != ZR_NULL && savedSemanticId != ZR_SEMANTIC_ID_INVALID) {
        symbol->semanticId = savedSemanticId;
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (valid) {
        TEST_PASS(timer, summary);
    } else {
        TEST_FAIL(timer, summary, failure);
    }
}

#endif
