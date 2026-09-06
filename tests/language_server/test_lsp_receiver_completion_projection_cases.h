static void test_lsp_receiver_completion_does_not_reinfer_initializer(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Receiver Completion Does Not Reinfer Initializer";
    const TZrChar *failure = ZR_NULL;
    const TZrChar *content =
        "var math = import(\"zr.math\");\n"
        "fn runImpl() {\n"
        "    var vector = init math.Vector3(4.0, 5.0, 6.0);\n"
        "    return vector.x;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrSymbol *receiverSymbol;
    SZrInferredType *savedSymbolType;
    SZrSemanticContext *savedSemanticContext;
    SZrLspPosition position;
    SZrFilePosition filePosition;
    SZrFileRange range;
    SZrArray completions;
    TZrBool collected;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///receiver_completion_projection.zr",
                               strlen("file:///receiver_completion_projection.zr"));
    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "vector.x", 0, 0, &position) ||
        (analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri)) == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL) {
        failure = "Failed to prepare the receiver completion projection fixture";
        goto cleanup;
    }
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    range = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    receiverSymbol = ZrLanguageServer_SymbolTable_LookupAtPosition(
            analyzer->symbolTable, ZrCore_String_Create(state, "vector", strlen("vector")), range);
    if (receiverSymbol == ZR_NULL || receiverSymbol->typeInfo == ZR_NULL) {
        failure = "The receiver must have a projected symbol type before completion";
        goto cleanup;
    }
    if (!lsp_find_position_for_substring(content, "vector.x", 0, 7, &position)) {
        failure = "Failed to locate the completion cursor after the receiver dot";
        goto cleanup;
    }
    if (!ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(
                state, context, uri, position, &completions) ||
        !completion_array_contains_label(&completions, "x") ||
        !completion_array_contains_label(&completions, "y") ||
        !completion_array_contains_label(&completions, "z")) {
        failure = "Published receiver facts must project all Vector3 fields";
        goto cleanup;
    }
    for (TZrSize index = 0; index < completions.length; index++) {
        SZrCompletionItem **item = (SZrCompletionItem **)ZrCore_Array_Get(&completions, index);
        ZrLanguageServer_CompletionItem_Free(state, *item);
    }
    ZrCore_Array_Empty(&completions);

    savedSemanticContext = analyzer->semanticContext;
    savedSymbolType = receiverSymbol->typeInfo;
    analyzer->semanticContext = ZR_NULL;
    receiverSymbol->typeInfo = ZR_NULL;
    collected = ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(
            state, context, uri, position, &completions);
    receiverSymbol->typeInfo = savedSymbolType;
    analyzer->semanticContext = savedSemanticContext;
    if (!collected || completions.length != 0) {
        failure = "Receiver completion reconstructed fields from the initializer after type projection became unavailable";
    }

cleanup:
    for (TZrSize index = 0; index < completions.length; index++) {
        SZrCompletionItem **item = (SZrCompletionItem **)ZrCore_Array_Get(&completions, index);
        ZrLanguageServer_CompletionItem_Free(state, *item);
    }
    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    if (failure != ZR_NULL) {
        TEST_FAIL(timer, summary, failure);
        return;
    }
    TEST_PASS(timer, summary);
}
