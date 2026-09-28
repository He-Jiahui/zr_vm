#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_DUPLICATE_DIAGNOSTIC_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_DUPLICATE_DIAGNOSTIC_CASES_H

/* 重复类型诊断由 parser 注册绑定时产生，LSP 不维护第二个诊断生产者。 */
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
    /* TODO: read_repo_text_file_owned 对文件不存在和读取失败均返回 NULL；需独立检查文件是否存在，
     * 否则旧生产者重新出现但不可读时会漏报，无法可靠排除两套规则并行。 */
    if (legacyProducer != NULL) {
        printf("FAIL: legacy LSP duplicate-type producer still exists\n");
        g_failures++;
    }

    free(symbols);
    free(parserDiagnostics);
    free(legacyProducer);
}

#endif // ZR_VM_TESTS_LANGUAGE_SERVER_LSP_SOURCE_CONTRACT_DUPLICATE_DIAGNOSTIC_CASES_H
