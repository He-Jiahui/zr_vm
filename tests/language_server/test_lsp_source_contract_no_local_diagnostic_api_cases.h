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

static void test_semantic_analyzer_rules_only_publish_structured_query_diagnostics(void) {
    static const char *ruleSourcePaths[] = {
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c",
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_constant_condition.c",
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_extern_bindings.c",
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c",
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_references.c",
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c",
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c",
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c",
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck_bindings.c",
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_union_patterns.c",
    };
    char *queryProjection = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c");
    char *protocolProjection = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_diagnostic_projection.c");

    if (queryProjection == NULL || protocolProjection == NULL) {
        printf("FAIL: could not read semantic diagnostic projection sources\n");
        g_failures++;
        free(queryProjection);
        free(protocolProjection);
        return;
    }

    for (size_t index = 0U;
         index < sizeof(ruleSourcePaths) / sizeof(ruleSourcePaths[0]);
         index++) {
        char *source = read_repo_text_file_owned(ruleSourcePaths[index]);

        if (source == NULL) {
            printf("FAIL: could not read semantic rule source: %s\n",
                   ruleSourcePaths[index]);
            g_failures++;
            continue;
        }
        assert_text_contains_none(source, "ZrLanguageServer_Diagnostic_New(");
        assert_text_contains_none(source, "ZrParser_DiagnosticBuilder_Build(");
        assert_text_contains_none(source, "ZrParser_SemanticFacts_AppendDiagnostic(");
        free(source);
    }

    assert_text_contains(
            queryProjection,
            "ZrParser_SemanticQuery_MaterializeDiagnostics");
    assert_text_contains(
            queryProjection,
            "ZrParser_SemanticQuery_Diagnostics");
    assert_text_contains(
            queryProjection,
            "ZrLanguageServer_Diagnostic_FromStructured");
    assert_text_contains_none(
            queryProjection,
            "ZrLanguageServer_Diagnostic_New(");
    assert_text_contains(
            protocolProjection,
            "ZrLanguageServer_Diagnostic_New(");
    assert_text_contains(
            protocolProjection,
            "structured->relatedInformation");
    assert_text_contains(
            protocolProjection,
            "structured->fixes");
    assert_text_contains(
            protocolProjection,
            "structured->noFixReason");

    free(queryProjection);
    free(protocolProjection);
}

#endif
