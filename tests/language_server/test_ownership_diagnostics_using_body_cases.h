#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_USING_BODY_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_USING_BODY_CASES_H

static void test_semantic_analyzer_tracks_borrow_rebind_in_using_body(
        SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Tracks Borrow Rebind In Using Body";
    const TZrChar *testCode =
        "class Resource {\n"
        "}\n"
        "use(first: %shared Resource, second: %shared Resource): int {\n"
        "    var alias = %borrow(first);\n"
        "    using (first) {\n"
        "        alias = %borrow(second);\n"
        "    }\n"
        "    var released = %release(second);\n"
        "    alias;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *diagnostic;
    const SZrDiagnosticRelatedInformation *related = ZR_NULL;

    TEST_START(summary);
    TEST_INFO("Using-body borrowed alias rebinding",
              "Body assignment must replace the alias owner before scope-exit release");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "ownership_using_body_borrow_rebind_test.zr",
            strlen("ownership_using_body_borrow_rebind_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) ZrParser_Ast_Free(state, ast);
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare using-body rebind fixture");
        return;
    }

    diagnostic = find_diagnostic_by_code_and_line(analyzer, "borrow_escape", 9);
    if (diagnostic != ZR_NULL &&
        diagnostic->relatedInformation.isValid &&
        diagnostic->relatedInformation.length == 1) {
        related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
                &diagnostic->relatedInformation,
                0);
    }
    if (diagnostic == ZR_NULL ||
        related == ZR_NULL ||
        related->location.start.line != 8 ||
        !test_string_equals(related->message, "Owner was released here")) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Using-body rebind retained the pre-body owner");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_tracks_unique_move_in_using_body(
        SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Tracks Unique Move In Using Body";
    const TZrChar *testCode =
        "class Resource {\n"
        "}\n"
        "consume(value: %unique Resource): int { return 0; }\n"
        "use(resource: %shared Resource, value: %unique Resource): int {\n"
        "    using (resource) {\n"
        "        consume(value);\n"
        "        value;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *diagnostic;
    const SZrDiagnosticRelatedInformation *related = ZR_NULL;

    TEST_START(summary);
    TEST_INFO("Using-body unique move flow",
              "A body-local by-value move must invalidate the following body read");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "ownership_using_body_unique_move_test.zr",
            strlen("ownership_using_body_unique_move_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) ZrParser_Ast_Free(state, ast);
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare using-body move fixture");
        return;
    }

    diagnostic = find_diagnostic_by_code_and_line(analyzer, "use_after_move", 7);
    if (diagnostic != ZR_NULL &&
        diagnostic->relatedInformation.isValid &&
        diagnostic->relatedInformation.length == 1) {
        related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
                &diagnostic->relatedInformation,
                0);
    }
    if (diagnostic == ZR_NULL ||
        related == ZR_NULL ||
        related->location.start.line != 6 ||
        !test_string_equals(related->message, "Value was moved here")) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Using body did not preserve unique move order");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

#endif // ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_USING_BODY_CASES_H
