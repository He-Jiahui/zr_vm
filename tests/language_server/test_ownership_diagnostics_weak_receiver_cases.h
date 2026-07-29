#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_WEAK_RECEIVER_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_WEAK_RECEIVER_CASES_H

static void test_semantic_analyzer_links_weak_receiver_to_owner_release(
        SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Links Weak Receiver To Owner Release";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "    fn ping(): int { return 0; }\n"
        "}\n"
        "fn use(owner: Shared<Resource>): int {\n"
        "    var watcher = owner.weak();\n"
        "    watcher.ping();\n"
        "    drop(owner);\n"
        "    watcher.ping();\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *beforeRelease;
    SZrDiagnostic *afterRelease;
    const SZrDiagnosticRelatedInformation *related = ZR_NULL;

    TEST_START(summary);
    TEST_INFO("Weak method receiver ownership flow",
              "Only the receiver call after owner release receives upgrade evidence");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "ownership_weak_receiver_release_test.zr",
            strlen("ownership_weak_receiver_release_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) ZrParser_Ast_Free(state, ast);
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare weak receiver fixture");
        return;
    }

    beforeRelease = find_diagnostic_by_code_and_line(
            analyzer,
            "weak_value_requires_upgrade",
            6);
    afterRelease = find_diagnostic_by_code_and_line(
            analyzer,
            "weak_value_requires_upgrade",
            8);
    if (afterRelease != ZR_NULL &&
        afterRelease->relatedInformation.isValid &&
        afterRelease->relatedInformation.length == 1) {
        related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
                &afterRelease->relatedInformation,
                0);
    }
    if (beforeRelease != ZR_NULL ||
        afterRelease == ZR_NULL ||
        related == ZR_NULL ||
        related->location.start.line != 7 ||
        !test_string_equals(related->message, "Owner was released here")) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Weak receiver did not retain release-sensitive evidence");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_links_rebound_weak_receiver_owner_set(
        SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Links Rebound Weak Receiver Owner Set";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "    fn ping(): int { return 0; }\n"
        "}\n"
        "fn use(first: Shared<Resource>, second: Shared<Resource>, choose: bool): int {\n"
        "    var watcher = first.weak();\n"
        "    if (choose) {\n"
        "        watcher = second.weak();\n"
        "    }\n"
        "    drop(second);\n"
        "    watcher.ping();\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *diagnostic;
    const SZrDiagnosticRelatedInformation *related = ZR_NULL;

    TEST_START(summary);
    TEST_INFO("Branch-joined weak method receiver",
              "The replacement owner must survive CFG join for receiver diagnostics");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "ownership_weak_receiver_owner_set_test.zr",
            strlen("ownership_weak_receiver_owner_set_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) ZrParser_Ast_Free(state, ast);
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare weak receiver owner-set fixture");
        return;
    }

    diagnostic = find_diagnostic_by_code_and_line(
            analyzer,
            "weak_value_requires_upgrade",
            10);
    if (diagnostic != ZR_NULL &&
        diagnostic->relatedInformation.isValid &&
        diagnostic->relatedInformation.length == 1) {
        related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
                &diagnostic->relatedInformation,
                0);
    }
    if (diagnostic == ZR_NULL ||
        related == ZR_NULL ||
        related->location.start.line != 9 ||
        !test_string_equals(related->message, "Owner was released here")) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Weak receiver lost the replacement owner release");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

#endif // ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_WEAK_RECEIVER_CASES_H
