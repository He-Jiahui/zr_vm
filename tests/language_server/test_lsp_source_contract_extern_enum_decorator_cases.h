#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_EXTERN_ENUM_DECORATOR_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_EXTERN_ENUM_DECORATOR_CASES_H

static void test_extern_enum_decorators_use_parser_diagnostic_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");

    if (typecheck == NULL) {
        printf("FAIL: could not read semantic analyzer typecheck source\n");
        g_failures++;
        return;
    }

    assert_text_contains(
        typecheck,
        "ZrParser_Compiler_ValidateExternEnumDecorators");
    assert_text_contains(
        typecheck,
        "ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic");
    assert_text_contains_none(
        typecheck,
        "semantic_validate_extern_enum_decorators");
    assert_text_contains_none(
        typecheck,
        "semantic_validate_extern_enum_member_decorators");
    assert_text_contains_none(
        typecheck,
        "semantic_call_has_single_integer_arg");

    printf("PASS: Extern enum decorators use parser diagnostic projection\n");
    free(typecheck);
}

#endif
