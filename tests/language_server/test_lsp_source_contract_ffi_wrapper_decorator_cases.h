#ifndef ZR_VM_TEST_LSP_SOURCE_CONTRACT_FFI_WRAPPER_DECORATOR_CASES_H
#define ZR_VM_TEST_LSP_SOURCE_CONTRACT_FFI_WRAPPER_DECORATOR_CASES_H

static void test_ffi_wrapper_decorators_use_parser_diagnostic_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");
    char *compilerClass = read_repo_text_file_owned(
        "zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c");

    if (typecheck == NULL || compilerClass == NULL) {
        printf("FAIL: could not read wrapper decorator production sources\n");
        g_failures++;
        free(typecheck);
        free(compilerClass);
        return;
    }

    assert_text_contains(
        typecheck,
        "ZrParser_Compiler_ValidateFfiWrapperDecorators");
    assert_text_contains(
        typecheck,
        "ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic");
    assert_text_contains_none(
        typecheck,
        "semantic_validate_class_wrapper_decorators");
    assert_text_contains_none(
        typecheck,
        "semantic_ffi_integer_type_name_supported");
    assert_text_contains_none(
        typecheck,
        "semantic_view_type_is_source_extern_struct");
    assert_text_contains_none(
        typecheck,
        "semantic_call_has_single_string_arg");

    assert_text_contains(
        compilerClass,
        "compiler_ffi_wrapper_bind_decorators");
    assert_text_contains_none(
        compilerClass,
        "compiler_class_extract_builtin_ffi_string_decorator");
    assert_text_contains_none(
        compilerClass,
        "compiler_class_ffi_integer_type_name_supported");
    assert_text_contains_none(
        compilerClass,
        "compiler_class_view_type_is_source_extern_struct");

    printf("PASS: FFI wrapper decorators use parser diagnostic projection\n");
    free(typecheck);
    free(compilerClass);
}

#endif
