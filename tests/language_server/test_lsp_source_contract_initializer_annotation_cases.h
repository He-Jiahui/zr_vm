#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_INITIALIZER_ANNOTATION_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_INITIALIZER_ANNOTATION_CASES_H

static void test_initializer_annotation_uses_parser_diagnostic_projection(void) {
    char *symbols = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c");
    char *typecheck = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");
    char *parserDiagnostics = read_repo_text_file_owned(
            "zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c");
    char *compileStatement = read_repo_text_file_owned(
            "zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c");

    if (symbols == NULL || typecheck == NULL ||
        parserDiagnostics == NULL || compileStatement == NULL) {
        printf("FAIL: could not read initializer annotation diagnostic sources\n");
        g_failures++;
        free(symbols);
        free(typecheck);
        free(parserDiagnostics);
        free(compileStatement);
        return;
    }

    assert_text_contains(symbols, "ZrParser_Compiler_ValidateVariableDeclaration");
    assert_text_contains(
            symbols,
            "ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic");
    assert_text_contains_none(symbols, "\"initializer_requires_annotation\"");
    assert_text_contains_none(typecheck, "\"initializer_requires_annotation\"");
    assert_text_contains(
            parserDiagnostics,
            "ZrParser_Compiler_ValidateVariableDeclaration");
    assert_text_contains(
            parserDiagnostics,
            "\"initializer_requires_annotation\"");
    assert_text_contains(
            compileStatement,
            "ZrParser_Compiler_ValidateVariableDeclaration");

    free(symbols);
    free(typecheck);
    free(parserDiagnostics);
    free(compileStatement);
}

#endif // ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_INITIALIZER_ANNOTATION_CASES_H
