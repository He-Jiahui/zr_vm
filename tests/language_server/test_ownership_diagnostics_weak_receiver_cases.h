#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_WEAK_RECEIVER_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_WEAK_RECEIVER_CASES_H

static TZrSize count_direct_weak_receiver_guards(
        const SZrSemanticContext *context) {
    TZrSize count = 0;

    if (context == ZR_NULL || !context->receiverGuardFacts.isValid) {
        return 0;
    }
    for (TZrSize index = 0; index < context->receiverGuardFacts.length; index++) {
        const SZrReceiverGuardFact *guard =
                (const SZrReceiverGuardFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->receiverGuardFacts,
                        index);

        if (guard != ZR_NULL &&
            guard->kind == ZR_RECEIVER_GUARD_WEAK_WAKE &&
            guard->mode == ZR_RECEIVER_GUARD_DIRECT) {
            count++;
        }
    }
    return count;
}

static void test_semantic_analyzer_guards_direct_weak_receiver_after_owner_release(
        SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Guards Direct Weak Receiver After Owner Release";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "    fn ping(): int { return 0; }\n"
        "}\n"
        "fn use(owner: Shared<Resource>): int {\n"
        "    var watcher = degrade(owner);\n"
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
    TZrSize directGuardCount;

    TEST_START(summary);
    TEST_INFO("Direct weak method receiver flow",
              "Direct member access uses receiver guards and does not require an explicit wake intrinsic");

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
            "weak_value_requires_wake",
            6);
    afterRelease = find_diagnostic_by_code_and_line(
            analyzer,
            "weak_value_requires_wake",
            8);
    directGuardCount = count_direct_weak_receiver_guards(analyzer->semanticContext);
    if (beforeRelease != ZR_NULL || afterRelease != ZR_NULL || directGuardCount != 2) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Direct weak receiver access did not retain its receiver guards");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_guards_rebound_direct_weak_receiver(
        SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Guards Rebound Direct Weak Receiver";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "    fn ping(): int { return 0; }\n"
        "}\n"
        "fn use(first: Shared<Resource>, second: Shared<Resource>, choose: bool): int {\n"
        "    var watcher = degrade(first);\n"
        "    if (choose) {\n"
        "        watcher = degrade(second);\n"
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
    TZrSize directGuardCount;

    TEST_START(summary);
    TEST_INFO("Branch-joined direct weak method receiver",
              "A rebound weak receiver keeps direct guard semantics without an explicit wake diagnostic");

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
            "weak_value_requires_wake",
            10);
    directGuardCount = count_direct_weak_receiver_guards(analyzer->semanticContext);
    if (diagnostic != ZR_NULL || directGuardCount != 1) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Rebound direct weak receiver did not retain its guard contract");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

#endif // ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_WEAK_RECEIVER_CASES_H
