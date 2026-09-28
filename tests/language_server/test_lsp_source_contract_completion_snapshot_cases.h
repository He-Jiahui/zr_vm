#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_COMPLETION_SNAPSHOT_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_COMPLETION_SNAPSHOT_CASES_H

/* 补全语义事实只读当前解析快照，不能为候选项单独运行表达式推断。 */
static void test_completion_semantic_facts_are_snapshot_read_only(void) {
    char *source = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/interface/"
            "lsp_completion_semantic_facts.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_completion_semantic_facts.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrParser_SemanticFacts_FindExpressionByNode");
    assert_text_contains(source, "ZrParser_SemanticFacts_FindNumericByNode");
    assert_text_contains_none(source, "completion_fact_materialize_initializer");
    assert_text_contains_none(source, "InferExactExpressionType");

    free(source);
}

#endif
