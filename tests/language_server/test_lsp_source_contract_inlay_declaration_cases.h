#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_INLAY_DECLARATION_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_INLAY_DECLARATION_CASES_H

static void test_inlay_uses_canonical_declaration_query(void) {
    char *inlaySource = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_inlay_hints.c");
    char *querySource = read_repo_text_file_owned(
            "zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c");
    const char *queryStart;
    const char *queryEnd;

    if (inlaySource == NULL || querySource == NULL) {
        printf("FAIL: could not read canonical inlay declaration sources\n");
        free(inlaySource);
        free(querySource);
        g_failures++;
        return;
    }

    assert_text_contains(inlaySource, "ZrParser_SemanticQuery_DeclaredSymbols");
    assert_text_contains(inlaySource, "ZrLanguageServer_Lsp_FormatCanonicalDeclarationType");
    assert_text_contains_none(inlaySource, "allScopes");
    assert_text_contains_none(inlaySource, "symbolTable");
    assert_text_contains_none(inlaySource, "InferExactExpressionType");
    assert_text_contains_none(inlaySource, "SymbolTable_Lookup");

    queryStart = strstr(querySource, "TZrBool ZrParser_SemanticQuery_DeclaredSymbols(");
    queryEnd = queryStart != NULL
            ? strstr(queryStart, "TZrBool ZrParser_SemanticQuery_VisibleSymbols(")
            : NULL;
    assert_text_section_contains("ZrParser_SemanticQuery_DeclaredSymbols",
                                 queryStart,
                                 queryEnd,
                                 "ZrParser_Semantic_FindSymbolById");
    assert_text_section_contains("ZrParser_SemanticQuery_DeclaredSymbols",
                                 queryStart,
                                 queryEnd,
                                 "record->astNode != declaration->node");
    assert_text_section_contains_none("ZrParser_SemanticQuery_DeclaredSymbols",
                                      queryStart,
                                      queryEnd,
                                      "FindSymbolByName");

    free(inlaySource);
    free(querySource);
}

#endif
