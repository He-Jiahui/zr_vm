#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_CANONICAL_COMPLETION_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_CANONICAL_COMPLETION_CASES_H

static void test_lexical_completion_uses_parser_visible_symbol_query(void) {
    char *projector = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c");
    char *consumer = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");
    char *analyzerHeader = read_repo_text_file_owned(
        "zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h");
    char *analyzerSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c");
    char *symbolTableHeader = read_repo_text_file_owned(
        "zr_vm_language_server/include/zr_vm_language_server/symbol_table.h");
    char *symbolTableSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/symbol_table.c");

    if (projector == NULL || consumer == NULL || analyzerHeader == NULL || analyzerSource == NULL ||
        symbolTableHeader == NULL || symbolTableSource == NULL) {
        printf("FAIL: could not read canonical completion sources\n");
        g_failures++;
        free(projector);
        free(consumer);
        free(analyzerHeader);
        free(analyzerSource);
        free(symbolTableHeader);
        free(symbolTableSource);
        return;
    }

    assert_text_contains(
        projector,
        "ZrParser_SemanticQuery_VisibleSymbols");
    assert_text_contains(
        projector,
        "symbol->kind");
    assert_text_contains_none(
        projector,
        "ZrLanguageServer_SymbolTable_GetVisibleSymbolsAtPosition");
    assert_text_contains_none(
        consumer,
        "ZrLanguageServer_SemanticAnalyzer_GetCompletions");
    assert_text_contains_none(
        analyzerHeader,
        "ZrLanguageServer_SemanticAnalyzer_GetCompletions");
    assert_text_contains_none(
        analyzerSource,
        "ZrLanguageServer_SemanticAnalyzer_GetCompletions");
    assert_text_contains_none(
        symbolTableHeader,
        "ZrLanguageServer_SymbolTable_GetVisibleSymbolsAtPosition");
    assert_text_contains_none(
        symbolTableSource,
        "ZrLanguageServer_SymbolTable_GetVisibleSymbolsAtPosition");
    assert_text_contains_none(
        symbolTableHeader,
        "ZrLanguageServer_SymbolTable_GetSymbolsInRange");
    assert_text_contains_none(
        symbolTableSource,
        "ZrLanguageServer_SymbolTable_GetSymbolsInRange");

    free(projector);
    free(consumer);
    free(analyzerHeader);
    free(analyzerSource);
    free(symbolTableHeader);
    free(symbolTableSource);
}

static void test_completion_consumer_does_not_materialize_scoped_analyzer(void) {
    char *consumer = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");
    const char *completionStart;
    const char *completionEnd;

    if (consumer == NULL) {
        printf("FAIL: could not read completion consumer source\n");
        g_failures++;
        return;
    }

    completionStart = strstr(
        consumer,
        "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(");
    completionEnd = completionStart != NULL
                        ? strstr(
                              completionStart,
                              "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDefinitions(")
                        : NULL;
    assert_text_section_contains(
        "LspSemanticQuery_CollectCompletionItems canonical projection",
        completionStart,
        completionEnd,
        "ZrLanguageServer_LspCanonicalCompletion_AppendVisibleSymbols");
    assert_text_section_contains_none(
        "LspSemanticQuery_CollectCompletionItems canonical projection",
        completionStart,
        completionEnd,
        "GetOrCreateScopedQueryAnalyzer");
    assert_text_section_contains_none(
        "LspSemanticQuery_CollectCompletionItems canonical projection",
        completionStart,
        completionEnd,
        "SemanticAnalyzer_AnalyzeScope");
    assert_text_section_contains_none(
        "LspSemanticQuery_CollectCompletionItems canonical projection",
        completionStart,
        completionEnd,
        "SemanticAnalyzer_Analyze(");
    assert_text_section_contains_none(
        "LspSemanticQuery_CollectCompletionItems canonical projection",
        completionStart,
        completionEnd,
        "fallbackAnalyzer");
    assert_text_section_contains_none(
        "LspSemanticQuery_CollectCompletionItems canonical projection",
        completionStart,
        completionEnd,
        "FindAnalysisRootAtPosition");

    free(consumer);
}

static void test_source_hover_uses_parser_symbol_query(void) {
    char *projector = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_hover.c");
    char *consumer = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");

    if (projector == NULL || consumer == NULL) {
        printf("FAIL: could not read canonical hover sources\n");
        g_failures++;
        free(projector);
        free(consumer);
        return;
    }

    assert_text_contains(projector, "ZrParser_CanonicalType_Format");
    assert_text_contains(projector, "symbol->signatureDisplay");
    assert_text_contains(projector, "ZrLanguageServer_Lsp_RangeFromFileRangeForDocument");
    assert_text_contains_none(projector, "ZrLanguageServer_SemanticAnalyzer_GetHoverInfo");
    assert_text_contains(consumer, "ZrLanguageServer_LspCanonicalHover_BuildSymbol");
    assert_text_contains_none(consumer, "ZrLanguageServer_SemanticAnalyzer_GetHoverInfo");
    assert_text_contains_none(
        consumer,
        "query->symbol = semantic_query_lookup_identifier_at_position");

    free(projector);
    free(consumer);
}

static void test_public_hover_consumer_does_not_use_analyzer_hover(void) {
    char *interfaceSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c");
    const char *hoverStart;
    const char *hoverEnd;

    if (interfaceSource == NULL) {
        printf("FAIL: could not read public hover consumer source\n");
        g_failures++;
        return;
    }

    hoverStart = strstr(
        interfaceSource,
        "TZrBool ZrLanguageServer_Lsp_GetHover(");
    hoverEnd = hoverStart != NULL
                   ? strstr(hoverStart, "TZrBool ZrLanguageServer_Lsp_GetRichHover(")
                   : NULL;
    assert_text_section_contains(
        "Lsp_GetHover canonical projection",
        hoverStart,
        hoverEnd,
        "ZrLanguageServer_LspSemanticQuery_BuildHover");
    assert_text_section_contains_none(
        "Lsp_GetHover canonical projection",
        hoverStart,
        hoverEnd,
        "ZrLanguageServer_SemanticAnalyzer_GetHoverInfo");

    free(interfaceSource);
}

#endif
