#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_EXACT_TYPE_DIAGNOSTIC_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_EXACT_TYPE_DIAGNOSTIC_CASES_H

static void test_cannot_infer_exact_type_uses_parser_diagnostic_projection(void) {
    const char *lspPaths[] = {
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c",
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c",
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c"
    };
    char *queryDiagnostics = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c");
    char *parserDiagnostics = read_repo_text_file_owned(
            "zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c");
    size_t index;

    if (queryDiagnostics == NULL || parserDiagnostics == NULL) {
        printf("FAIL: could not read exact-type diagnostic sources\n");
        g_failures++;
        free(queryDiagnostics);
        free(parserDiagnostics);
        return;
    }

    assert_text_contains(
            queryDiagnostics,
            "ZrParser_Compiler_ReportCannotInferExactType");
    assert_text_contains_none(
            queryDiagnostics,
            "query_diagnostic_remove_shadowed_inference");
    assert_text_contains(parserDiagnostics, "\"cannot_infer_exact_type\"");

    for (index = 0U; index < sizeof(lspPaths) / sizeof(lspPaths[0]); index++) {
        char *source = read_repo_text_file_owned(lspPaths[index]);

        if (source == NULL) {
            printf("FAIL: could not read %s\n", lspPaths[index]);
            g_failures++;
            continue;
        }
        assert_text_contains_none(source, "\"cannot_infer_exact_type\"");
        assert_text_contains_none(
                source,
                "semantic_add_cannot_infer_exact_type_diagnostic");
        free(source);
    }

    free(queryDiagnostics);
    free(parserDiagnostics);
}

#endif
