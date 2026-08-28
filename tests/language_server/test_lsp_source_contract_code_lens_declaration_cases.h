#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_CODE_LENS_DECLARATION_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_CODE_LENS_DECLARATION_CASES_H

static void test_code_lens_uses_canonical_declaration_and_reference_queries(void) {
    char *codeLensSource = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/lsp_code_lens.c");

    if (codeLensSource == NULL) {
        printf("FAIL: could not read canonical CodeLens source\n");
        free(codeLensSource);
        g_failures++;
        return;
    }

    assert_text_contains(codeLensSource, "ZrParser_SemanticQuery_DeclaredSymbols");
    assert_text_contains(codeLensSource, "ZrParser_SemanticQuery_ReferencesOf");
    assert_text_contains(codeLensSource, "lsp_editor_get_file_version");
    assert_text_contains(codeLensSource, "analyzer->ast != fileVersion->ast");
    assert_text_contains(codeLensSource, "lsp_code_lens_ranges_equal");
    assert_text_contains(
            codeLensSource,
            "candidate->declarationNode == current->declarationNode");
    assert_text_contains_none(codeLensSource, "allScopes");
    assert_text_contains_none(codeLensSource, "symbolTable");
    assert_text_contains_none(codeLensSource, "ZrLanguageServer_Lsp_FindReferences");
    assert_text_contains_none(codeLensSource, "SymbolTable_Lookup");

    free(codeLensSource);
}

#endif
