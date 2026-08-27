#ifndef ZR_VM_TEST_SEMANTIC_ANALYZER_FFI_WRAPPER_DECORATOR_CASES_H
#define ZR_VM_TEST_SEMANTIC_ANALYZER_FFI_WRAPPER_DECORATOR_CASES_H

static void assert_ffi_wrapper_decorator_golden_parity(
        SZrState *state,
        const TZrChar *summary,
        const TZrChar *sourceNameText,
        const TZrChar *testCode,
        TZrInt32 expectedLine,
        TZrInt32 expectedStartColumn,
        TZrInt32 expectedEndColumn) {
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics queryDiagnostics;
    TZrSize queryCount = 0U;

    TEST_START(summary);
    TEST_INFO("Compiler query to LSP projection",
              "FFI wrapper decorator diagnostics must preserve the parser-owned descriptor, exact decorator range, and no-fix disposition");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to analyze FFI wrapper decorator fixture");
        return;
    }

    ZrParser_SemanticQueryScope_Module(&scope);
    if (!ZrParser_SemanticQuery_MaterializeDiagnostics(
                analyzer->semanticContext,
                &scope) ||
        !ZrParser_SemanticQuery_Diagnostics(
                analyzer->semanticContext,
                &scope,
                &queryDiagnostics)) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to query FFI wrapper diagnostics");
        return;
    }

    for (TZrSize index = 0U; index < queryDiagnostics.count; index++) {
        const SZrStructuredDiagnostic *structured = &queryDiagnostics.items[index];
        const TZrChar *code = structured->code != ZR_NULL
                                      ? ZrCore_String_GetNativeString(structured->code)
                                      : ZR_NULL;
        const SZrDiagnostic *projected;

        if (code == ZR_NULL || strcmp(code, "invalid_decorator") != 0) {
            continue;
        }
        queryCount++;
        projected = find_projected_query_diagnostic(analyzer, structured);
        if (!diagnostic_projection_matches_query(projected, structured) ||
            structured->descriptorId != 2019U ||
            structured->location.start.line != expectedLine ||
            structured->location.start.column != expectedStartColumn ||
            structured->location.end.line != expectedLine ||
            structured->location.end.column != expectedEndColumn ||
            structured->noFixReason !=
                    ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
            diagnostic_array_length(&structured->fixes) != 0U) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      summary,
                      "The projected FFI wrapper diagnostic diverged from the compiler query fact");
            return;
        }
    }

    if (queryCount != 1U ||
        count_diagnostics_with_code(analyzer, "invalid_decorator") != 1U ||
        count_diagnostics_with_code(analyzer, "compiler_error") != 0U) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Expected one compiler invalid-decorator fact and one canonical LSP projection");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_preserves_invalid_ffi_wrapper_lowering_golden_parity(
        SZrState *state) {
    const TZrChar *testCode =
            "#zr.ffi.lowering(\"bad\")#\n"
            "class Handle {}\n";

    assert_ffi_wrapper_decorator_golden_parity(
            state,
            "Semantic Analyzer Preserves Invalid FFI Wrapper Lowering Golden Parity",
            "invalid_ffi_wrapper_lowering_golden_parity.zr",
            testCode,
            1,
            1,
            25);
}

static void test_semantic_analyzer_preserves_invalid_ffi_wrapper_view_type_golden_parity(
        SZrState *state) {
    const TZrChar *testCode =
            "struct PlainView { var raw:i32; }\n"
            "#zr.ffi.viewType(\"PlainView\")#\n"
            "class Handle {}\n";

    assert_ffi_wrapper_decorator_golden_parity(
            state,
            "Semantic Analyzer Preserves Invalid FFI Wrapper View Type Golden Parity",
            "invalid_ffi_wrapper_view_type_golden_parity.zr",
            testCode,
            2,
            1,
            31);
}

#endif
