#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_EXTERN_PARAMETER_DECORATOR_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_EXTERN_PARAMETER_DECORATOR_CASES_H

static void test_extern_parameter_decorators_use_parser_diagnostic_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");
    char *externDeclaration = read_repo_text_file_owned(
        "zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c");

    if (typecheck == NULL || externDeclaration == NULL) {
        printf("FAIL: could not read extern parameter decorator production sources\n");
        g_failures++;
        free(typecheck);
        free(externDeclaration);
        return;
    }

    assert_text_contains(
        typecheck,
        "ZrParser_Compiler_ValidateExternParameterDecorators");
    assert_text_contains(
        typecheck,
        "ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic");
    assert_text_contains_none(
        typecheck,
        "semantic_validate_extern_parameter_decorators");
    assert_text_contains_none(typecheck, "semantic_extract_ffi_decorator");
    assert_text_contains_none(typecheck, "semantic_add_invalid_decorator");

    assert_text_contains(
        externDeclaration,
        "ZrParser_Compiler_ValidateExternParameterDecorators");
    assert_text_contains_none(
        externDeclaration,
        "kExternParameterDecoratorRules");
    assert_text_contains_none(
        externDeclaration,
        "compiler_decorators_validate_static_rules");

    printf("PASS: Extern parameter decorators use parser diagnostic projection\n");
    free(typecheck);
    free(externDeclaration);
}

#endif
