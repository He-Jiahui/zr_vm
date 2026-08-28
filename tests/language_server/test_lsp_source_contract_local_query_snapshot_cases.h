#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_LOCAL_QUERY_SNAPSHOT_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_LOCAL_QUERY_SNAPSHOT_CASES_H

static void test_local_semantic_query_is_snapshot_read_only(void) {
    char *source = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/"
            "lsp_local_semantic_query.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_local_semantic_query.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "local_query_collect_facts");
    assert_text_contains_none(source, "local_query_materialize_expression_fact");
    assert_text_contains_none(source, "InferExactExpressionType");
    assert_text_contains_none(source, "Semantic_RegisterInferredType");
    assert_text_contains_none(source, "SymbolTable_LookupAtPosition");
    assert_text_contains_none(source, "ResolveTypeAtPosition");

    free(source);
}

#endif
