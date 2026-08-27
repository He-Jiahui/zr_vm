#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_RETURN_TYPE_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_RETURN_TYPE_CASES_H

static void test_return_type_inference_uses_parser_diagnostic_projection(void) {
    char *symbols = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c");
    char *parserInference = read_repo_text_file_owned(
            "zr_vm_parser/src/zr_vm_parser/compiler/compiler_callable_return_inference.c");

    if (symbols == NULL || parserInference == NULL) {
        printf("FAIL: could not read return inference diagnostic sources\n");
        g_failures++;
        free(symbols);
        free(parserInference);
        return;
    }

    assert_text_contains(
            symbols,
            "ZrParser_Compiler_InferCallableReturnType");
    assert_text_contains(
            symbols,
            "ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic");
    assert_text_contains_none(symbols, "\"return_type_not_provable\"");
    assert_text_contains_none(symbols, "merge_callable_return_type");
    assert_text_contains_none(symbols, "collect_callable_return_types");
    assert_text_contains(
            parserInference,
            "ZrParser_Compiler_InferCallableReturnType");
    assert_text_contains(
            parserInference,
            "\"return_type_not_provable\"");

    free(symbols);
    free(parserInference);
}

#endif // ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_RETURN_TYPE_CASES_H
