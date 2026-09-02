#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_NO_LOCAL_REFERENCE_COLLECTION_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_NO_LOCAL_REFERENCE_COLLECTION_CASES_H

static void test_semantic_analyzer_uses_canonical_symbol_query_for_references(void) {
    char *analysis = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c");
    char *analyzer = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c");
    char *internalHeader = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h");
    char *symbolTable = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/symbol_table.c");

    if (analysis == NULL || analyzer == NULL || internalHeader == NULL || symbolTable == NULL) {
        printf("FAIL: could not read semantic reference collection sources\n");
        g_failures++;
        free(analysis);
        free(analyzer);
        free(internalHeader);
        free(symbolTable);
        return;
    }

    assert_text_contains_none(
            analysis,
            "ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst");
    assert_text_contains_none(
            internalHeader,
            "ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst");
    assert_text_contains(analyzer, "ZrParser_SemanticQuery_SymbolAt");
    assert_text_contains(analyzer, "ZrLanguageServer_SymbolTable_FindBySemanticId");
    assert_text_contains_none(analyzer, "ReferenceTracker_FindReferenceAt");
    assert_text_contains(symbolTable, "ZrLanguageServer_SymbolTable_FindBySemanticId");

    free(analysis);
    free(analyzer);
    free(internalHeader);
    free(symbolTable);
}

#endif
