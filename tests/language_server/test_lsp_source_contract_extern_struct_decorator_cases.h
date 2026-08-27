#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_EXTERN_STRUCT_DECORATOR_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_EXTERN_STRUCT_DECORATOR_CASES_H

static void test_extern_struct_decorators_use_parser_diagnostic_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");

    if (typecheck == NULL) {
        printf("FAIL: could not read semantic analyzer typecheck source\n");
        g_failures++;
        return;
    }

    assert_text_contains(
        typecheck,
        "ZrParser_Compiler_ValidateExternStructDecorators");
    assert_text_contains(
        typecheck,
        "ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic");
    assert_text_contains_none(
        typecheck,
        "semantic_validate_extern_struct_decorators");
    assert_text_contains_none(
        typecheck,
        "semantic_validate_extern_struct_field_decorators");
    assert_text_contains_none(
        typecheck,
        "zr.ffi.pack/align require a single integer argument");
    assert_text_contains_none(
        typecheck,
        "zr.ffi.offset requires a single integer argument");

    printf("PASS: Extern struct decorators use parser diagnostic projection\n");
    free(typecheck);
}

#endif
