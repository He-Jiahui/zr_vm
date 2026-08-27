#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_DUPLICATE_DIAGNOSTIC_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_DUPLICATE_DIAGNOSTIC_CASES_H

static void test_duplicate_type_uses_parser_diagnostic_projection(void) {
    char *symbols = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c");
    char *parserDiagnostics = read_repo_text_file_owned(
            "zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c");
    char *legacyProducer = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_duplicate_diagnostics.c");

    if (symbols == NULL || parserDiagnostics == NULL) {
        printf("FAIL: could not read duplicate-type diagnostic sources\n");
        g_failures++;
        free(symbols);
        free(parserDiagnostics);
        free(legacyProducer);
        return;
    }

    assert_text_contains(
            symbols,
            "ZrParser_Compiler_RegisterTypeBinding");
    assert_text_contains(
            symbols,
            "ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic");
    assert_text_contains_none(symbols, "ZrParser_DiagnosticBuilder_Build");
    assert_text_contains_none(symbols, "\"duplicate_type\"");
    assert_text_contains(
            parserDiagnostics,
            "ZrParser_Compiler_ReportDuplicateTypeDeclaration");
    assert_text_contains(
            parserDiagnostics,
            "ZrParser_Compiler_RegisterTypeBinding");
    assert_text_contains(parserDiagnostics, "\"duplicate_type\"");
    if (legacyProducer != NULL) {
        printf("FAIL: legacy LSP duplicate-type producer still exists\n");
        g_failures++;
    }

    free(symbols);
    free(parserDiagnostics);
    free(legacyProducer);
}

#endif // ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_DUPLICATE_DIAGNOSTIC_CASES_H
