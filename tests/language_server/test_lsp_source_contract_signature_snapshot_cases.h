#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_SIGNATURE_SNAPSHOT_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_SIGNATURE_SNAPSHOT_CASES_H

static void test_signature_semantic_facts_are_snapshot_read_only(void) {
    char *source = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/interface/"
            "lsp_signature_semantic_facts.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_signature_semantic_facts.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrParser_SemanticFacts_FindExpressionByNode");
    assert_text_contains(source, "ZrParser_SemanticFacts_FindNumericByNode");
    assert_text_contains_none(source, "signature_fact_materialize_argument");
    assert_text_contains_none(source, "InferExactExpressionType");

    free(source);
}

#endif
