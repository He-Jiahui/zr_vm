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

    if (projector == NULL || consumer == NULL || analyzerHeader == NULL || analyzerSource == NULL) {
        printf("FAIL: could not read canonical completion sources\n");
        g_failures++;
        free(projector);
        free(consumer);
        free(analyzerHeader);
        free(analyzerSource);
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

    free(projector);
    free(consumer);
    free(analyzerHeader);
    free(analyzerSource);
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

#endif
