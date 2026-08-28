#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_NO_LOCAL_DIAGNOSTIC_API_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_NO_LOCAL_DIAGNOSTIC_API_CASES_H

static void test_semantic_analyzer_has_no_unstructured_diagnostic_escape_hatch(void) {
    char *header = read_repo_text_file_owned(
            "zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h");
    char *source = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c");
    char *analysisSource = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c");
    char *typecheckSource = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");

    if (header == NULL || source == NULL || analysisSource == NULL ||
        typecheckSource == NULL) {
        printf("FAIL: could not read semantic analyzer diagnostic API sources\n");
        g_failures++;
        free(header);
        free(source);
        free(analysisSource);
        free(typecheckSource);
        return;
    }

    assert_text_contains_none(
            header,
            "ZrLanguageServer_SemanticAnalyzer_AddDiagnostic");
    assert_text_contains_none(
            source,
            "ZrLanguageServer_SemanticAnalyzer_AddDiagnostic");
    assert_text_contains(
            analysisSource,
            "ZrParser_Compiler_ValidateReferenceEscapes");
    assert_text_contains_none(
            typecheckSource,
            "semantic_prepare_return_ownership_escape_diagnostic");
    assert_text_contains_none(
            typecheckSource,
            "semantic_emit_ownership_diagnostic");
    assert_text_contains_none(
            typecheckSource,
            "SemanticOwnership_AddEscapeRelatedInformation");

    free(header);
    free(source);
    free(analysisSource);
    free(typecheckSource);
}

#endif
