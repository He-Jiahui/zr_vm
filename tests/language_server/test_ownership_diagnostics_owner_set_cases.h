#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_OWNER_SET_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_OWNER_SET_CASES_H

/* 四种 alias 夹具的共同断言：分析后以错误码与行号取诊断，并核对相关位置属于候选 owner 集合。 */
static void test_possible_owner_release_case(
        SZrState *state,
        const TZrChar *summary,
        const TZrChar *details,
        const TZrChar *sourceNameText,
        const TZrChar *testCode,
        const TZrChar *diagnosticCode,
        TZrInt32 diagnosticLine,
        TZrInt32 releaseLine,
        TZrBool expectDiagnostic) {
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *diagnostic;
    const SZrDiagnosticRelatedInformation *related = ZR_NULL;

    TEST_START(summary);
    TEST_INFO("Branch-joined alias owner set", details);

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            (TZrNativeString)sourceNameText,
            strlen(sourceNameText));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare owner-set fixture");
        return;
    }

    diagnostic = find_diagnostic_by_code_and_line(
            analyzer,
            diagnosticCode,
            diagnosticLine);
    if (diagnostic != ZR_NULL &&
        diagnostic->relatedInformation.isValid &&
        diagnostic->relatedInformation.length == 1) {
        related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
                &diagnostic->relatedInformation,
                0);
    }
    if ((expectDiagnostic &&
         (diagnostic == ZR_NULL ||
          related == ZR_NULL ||
          related->location.start.line != releaseLine ||
          !test_string_equals(related->message, "Owner was released here"))) ||
        (!expectDiagnostic && diagnostic != ZR_NULL)) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  expectDiagnostic
                          ? "Expected the branch owner-set release location"
                          : "Unrelated release was attributed to the alias owner set");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

/* 条件重绑定后仍须保留初始 Shared owner；无论分支取值如何，后续释放都可能影响借用。 */
static void test_semantic_analyzer_joins_borrowed_alias_owner_set(SZrState *state) {
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn use(first: Shared<Resource>, second: Shared<Resource>, choose: bool): int {\n"
        "    var alias = ref first;\n"
        "    if (choose) {\n"
        "        alias = ref second;\n"
        "    }\n"
        "    drop(first);\n"
        "    drop(second);\n"
        "    alias;\n"
        "    return 0;\n"
        "}\n";

    test_possible_owner_release_case(
            state,
            "Semantic Analyzer Joins Borrowed Alias Owner Set",
            "The declaration owner must survive a conditional rebind path",
            "ownership_borrow_owner_set_test.zr",
            testCode,
            "borrow_escape",
            10,
            8,
            ZR_TRUE);
}

/* Unique loan 的条件替换 owner 须参与合并，错误应指向仅在分支内采用的 second。 */
static void test_semantic_analyzer_joins_loaned_alias_owner_set(SZrState *state) {
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn use(first: Unique<Resource>, second: Unique<Resource>, choose: bool): int {\n"
        "    var alias = ref first;\n"
        "    if (choose) {\n"
        "        alias = ref second;\n"
        "    }\n"
        "    drop(second);\n"
        "    alias;\n"
        "    return 0;\n"
        "}\n";

    test_possible_owner_release_case(
            state,
            "Semantic Analyzer Joins Loaned Alias Owner Set",
            "The conditional replacement owner must remain a possible loan source",
            "ownership_loan_owner_set_test.zr",
            testCode,
            "loan_escape",
            9,
            8,
            ZR_TRUE);
}

/* degrade 后的 weak alias 在条件赋值中保留候选 owner，直接借用时应关联可能的释放点。 */
static void test_semantic_analyzer_joins_weak_alias_owner_set(SZrState *state) {
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn observe(resource: ref readonly Resource): int { return 0; }\n"
        "fn use(first: Shared<Resource>, second: Shared<Resource>, choose: bool): int {\n"
        "    var watcher = degrade(first);\n"
        "    if (choose) {\n"
        "        watcher = degrade(second);\n"
        "    }\n"
        "    drop(second);\n"
        "    observe(ref watcher);\n"
        "    return 0;\n"
        "}\n";

    test_possible_owner_release_case(
            state,
            "Semantic Analyzer Joins Weak Alias Owner Set",
            "A direct borrowed use must retain a conditional weak owner release",
            "ownership_weak_owner_set_test.zr",
            testCode,
            "weak_value_requires_wake",
            10,
            9,
            ZR_TRUE);
}

/* 无关 Shared 值不属于合并后的 owner 集合，释放它不能令 alias 产生虚假的 borrow_escape。 */
static void test_semantic_analyzer_ignores_release_outside_alias_owner_set(
        SZrState *state) {
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn use(first: Shared<Resource>, second: Shared<Resource>, unrelated: Shared<Resource>, choose: bool): int {\n"
        "    var alias = ref first;\n"
        "    if (choose) {\n"
        "        alias = ref second;\n"
        "    }\n"
        "    drop(unrelated);\n"
        "    alias;\n"
        "    return 0;\n"
        "}\n";

    test_possible_owner_release_case(
            state,
            "Semantic Analyzer Ignores Release Outside Alias Owner Set",
            "A release outside the joined owner set must not create a borrow error",
            "ownership_unrelated_owner_set_release_test.zr",
            testCode,
            "borrow_escape",
            9,
            8,
            ZR_FALSE);
}

#endif // ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_OWNER_SET_CASES_H
