#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_SIGNATURE_SNAPSHOT_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_SIGNATURE_SNAPSHOT_CASES_H

static void test_signature_semantic_facts_are_snapshot_read_only(void) {
    char *source = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/interface/"
            "lsp_signature_semantic_facts.c");
    char *dispatcher = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/"
            "lsp_signature_help.c");
    const char *canonicalResolve;
    const char *legacyCompilerGuard;

    if (source == NULL || dispatcher == NULL) {
        printf("FAIL: could not read signature semantic fact sources\n");
        g_failures++;
        free(source);
        free(dispatcher);
        return;
    }

    assert_text_contains(source, "ZrParser_SemanticFacts_FindExpressionByNode");
    assert_text_contains(source, "ZrParser_SemanticFacts_FindNumericByNode");
    assert_text_contains_none(source, "signature_fact_materialize_argument");
    assert_text_contains_none(source, "InferExactExpressionType");
    assert_text_contains_none(dispatcher, "signature_resolve_function_help");
    assert_text_contains_none(
            dispatcher,
            "signature_lookup_unresolved_function_candidate");
    assert_text_contains_none(
            dispatcher,
            "ZrLanguageServer_SymbolTable_LookupAtPosition");
    assert_text_contains_none(dispatcher, "signature_resolve_method_help");

    canonicalResolve = strstr(
            dispatcher,
            "ZrLanguageServer_LspCanonicalSignatureHelp_Resolve(");
    legacyCompilerGuard = strstr(
            dispatcher,
            "if (analyzer->compilerState == ZR_NULL)");
    if (canonicalResolve == NULL || legacyCompilerGuard == NULL ||
        canonicalResolve >= legacyCompilerGuard) {
        printf("FAIL: canonical signature resolution must precede the legacy compiler-state guard\n");
        g_failures++;
    }

    free(source);
    free(dispatcher);
}

#endif
