#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_REGION_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_OWNERSHIP_DIAGNOSTICS_REGION_CASES_H

static void test_semantic_analyzer_records_borrow_and_loan_regions(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Records Borrow And Loan Regions";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn regions(shared: Shared<Resource>, unique: Unique<Resource>): int {\n"
        "    var borrowed = ref shared;\n"
        "    var loaned = ref unique;\n"
        "    borrowed;\n"
        "    loaned;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    const SZrSemanticOwnershipFact *borrowFact;
    const SZrSemanticOwnershipFact *loanFact;

    TEST_START(summary);
    TEST_INFO("Borrow and loan lifetime regions",
              "Borrow and loan facts must identify the alias region and the distinct owner region");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_borrow_loan_regions_test.zr",
                                      strlen("ownership_borrow_loan_regions_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare borrow/loan region fixture");
        return;
    }

    borrowFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "shared", 1));
    loanFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "unique", 1));
    if (borrowFact == ZR_NULL ||
        borrowFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_BORROW ||
        borrowFact->qualifier != ZR_OWNERSHIP_QUALIFIER_BORROWED ||
        borrowFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
        borrowFact->lifetimeRegionId == ZR_SEMANTIC_ID_INVALID ||
        borrowFact->ownerLifetimeRegionId == ZR_SEMANTIC_ID_INVALID ||
        borrowFact->lifetimeRegionId == borrowFact->ownerLifetimeRegionId ||
        loanFact == ZR_NULL ||
        loanFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_MOVE ||
        loanFact->qualifier != ZR_OWNERSHIP_QUALIFIER_LOANED ||
        loanFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
        loanFact->lifetimeRegionId == ZR_SEMANTIC_ID_INVALID ||
        loanFact->ownerLifetimeRegionId == ZR_SEMANTIC_ID_INVALID ||
        loanFact->lifetimeRegionId == loanFact->ownerLifetimeRegionId ||
        loanFact->symbolId == borrowFact->symbolId) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Expected linked alias/owner lifetime regions on borrow and loan facts");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_reports_borrow_after_owner_release(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Reports Borrow After Owner Release";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn use(owner: Shared<Resource>): int {\n"
        "    var borrowed = ref owner;\n"
        "    drop(owner);\n"
        "    borrowed;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *diagnostic;
    const SZrDiagnosticRelatedInformation *related;
    const SZrSemanticOwnershipFact *violationFact;

    TEST_START(summary);
    TEST_INFO("Borrowed alias after explicit owner release",
              "Reading a borrowed alias after its owner is released must report borrow_escape with release evidence");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_borrow_after_release_test.zr",
                                      strlen("ownership_borrow_after_release_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare borrow-after-release fixture");
        return;
    }

    diagnostic = find_diagnostic_by_code_and_line(analyzer, "borrow_escape", 6);
    if (diagnostic == ZR_NULL ||
        diagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 1) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Expected borrow_escape with one owner-release related location");
        return;
    }

    related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &diagnostic->relatedInformation,
            0);
    violationFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "borrowed", 1));
    if (related == ZR_NULL ||
        !test_string_equals(related->message, "Owner was released here") ||
        related->location.start.line != 5 ||
        violationFact == ZR_NULL ||
        violationFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
        violationFact->qualifier != ZR_OWNERSHIP_QUALIFIER_BORROWED ||
        violationFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
        violationFact->lifetimeRegionId == ZR_SEMANTIC_ID_INVALID ||
        violationFact->ownerLifetimeRegionId == ZR_SEMANTIC_ID_INVALID ||
        violationFact->relatedNode == ZR_NULL ||
        !violationFact->isViolation) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Borrow release diagnostic did not retain region and release-point evidence");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_lsp_reports_possible_path_borrow_after_owner_release(SZrState *state) {
    const TZrChar *summary = "LSP Reports Possible-Path Borrow After Owner Release";
    const TZrChar *uriText = "file:///ownership_possible_path_borrow_after_release.zr";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn use(owner: Shared<Resource>, flag: bool): int {\n"
        "    var borrowed = ref owner;\n"
        "    if (flag) { drop(owner); }\n"
        "    borrowed;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    const SZrLspDiagnostic *diagnostic;
    const SZrLspDiagnosticRelatedInformation *related;

    TEST_START(summary);
    TEST_INFO("Borrowed alias after branch-local owner release",
              "The RELEASED path must survive CFG join and remain visible at the LSP boundary");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, testCode, strlen(testCode), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare branch borrow-release fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    diagnostic = find_lsp_diagnostic_by_code(&diagnostics, "borrow_escape");
    if (diagnostic == ZR_NULL ||
        diagnostic->severity != 1 ||
        diagnostic->range.start.line != 5 ||
        diagnostic->range.start.character != 4 ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 1) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected possible-path borrow_escape at the post-join read");
        return;
    }

    related = (const SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&diagnostic->relatedInformation,
            0);
    if (related == ZR_NULL ||
        !test_string_equals(related->message, "Owner was released here") ||
        related->location.range.start.line != 4) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected branch-local release point in LSP related information");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_releases_using_owner_at_scope_exit(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Releases Using Owner At Scope Exit";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn use(owner: Shared<Resource>): int {\n"
        "    var borrowed = ref owner;\n"
        "    using (owner) {\n"
        "        borrowed;\n"
        "    }\n"
        "    borrowed;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *insideDiagnostic;
    SZrDiagnostic *outsideDiagnostic;
    const SZrDiagnosticRelatedInformation *related;
    const SZrSemanticOwnershipFact *releaseFact;

    TEST_START(summary);
    TEST_INFO("Block-scoped using owner cleanup",
              "The resource stays live in the body and becomes RELEASED after the using statement");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_using_owner_scope_exit_test.zr",
                                      strlen("ownership_using_owner_scope_exit_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare using-owner scope fixture");
        return;
    }

    insideDiagnostic = find_diagnostic_by_code_and_line(analyzer, "borrow_escape", 6);
    outsideDiagnostic = find_diagnostic_by_code_and_line(analyzer, "borrow_escape", 8);
    releaseFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "owner", 2));
    if (insideDiagnostic != ZR_NULL ||
        outsideDiagnostic == ZR_NULL ||
        outsideDiagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
        !outsideDiagnostic->relatedInformation.isValid ||
        outsideDiagnostic->relatedInformation.length != 1 ||
        releaseFact == ZR_NULL ||
        releaseFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE ||
        releaseFact->qualifier != ZR_OWNERSHIP_QUALIFIER_SHARED ||
        releaseFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
        releaseFact->lifetimeRegionId == ZR_SEMANTIC_ID_INVALID ||
        releaseFact->ownerLifetimeRegionId == ZR_SEMANTIC_ID_INVALID) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Using owner did not publish the expected post-scope RELEASE flow");
        return;
    }

    related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &outsideDiagnostic->relatedInformation,
            0);
    if (related == ZR_NULL ||
        !test_string_equals(related->message, "Owner was released here") ||
        related->location.start.line != 5) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Using owner release location was not retained");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_releases_using_borrow_at_scope_exit(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Releases Using Borrow At Scope Exit";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn use(owner: Shared<Resource>): int {\n"
        "    var borrowed = ref owner;\n"
        "    using (borrowed) {\n"
        "        borrowed;\n"
        "    }\n"
        "    borrowed;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *insideDiagnostic;
    SZrDiagnostic *outsideDiagnostic;
    const SZrSemanticOwnershipFact *releaseFact;

    TEST_START(summary);
    TEST_INFO("Block-scoped borrowed cleanup",
              "Using a borrowed resource ends that alias at scope exit without invalidating body reads");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_using_borrow_scope_exit_test.zr",
                                      strlen("ownership_using_borrow_scope_exit_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare using-borrow scope fixture");
        return;
    }

    insideDiagnostic = find_diagnostic_by_code_and_line(analyzer, "borrow_escape", 6);
    outsideDiagnostic = find_diagnostic_by_code_and_line(analyzer, "borrow_escape", 8);
    releaseFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "borrowed", 1));
    if (insideDiagnostic != ZR_NULL ||
        outsideDiagnostic == ZR_NULL ||
        outsideDiagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
        releaseFact == ZR_NULL ||
        releaseFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE ||
        releaseFact->qualifier != ZR_OWNERSHIP_QUALIFIER_BORROWED ||
        releaseFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
        releaseFact->lifetimeRegionId == ZR_SEMANTIC_ID_INVALID ||
        releaseFact->ownerLifetimeRegionId == ZR_SEMANTIC_ID_INVALID) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Using borrow did not end the alias at scope exit");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_links_weak_use_to_possible_owner_release(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Links Weak Use To Possible Owner Release";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn observe(resource: ref readonly Resource): int { return 0; }\n"
        "fn use(owner: Shared<Resource>, flag: bool): int {\n"
        "    var watcher = degrade(owner);\n"
        "    if (flag) { drop(owner); }\n"
        "    observe(ref watcher);\n"
        "    var woken = wake(watcher);\n"
        "    woken == null;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *diagnostic;
    const SZrDiagnosticRelatedInformation *related;
    const SZrSemanticOwnershipFact *violationFact;
    TZrSize diagnosticCount = 0;
    TZrSize index;

    TEST_START(summary);
    TEST_INFO("Weak alias after possible owner release",
              "A direct borrowed use must retain release evidence while upgrade and null checks stay legal");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_weak_after_possible_release_test.zr",
                                      strlen("ownership_weak_after_possible_release_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare weak-release fixture");
        return;
    }

    for (index = 0; index < analyzer->diagnostics.length; index++) {
        SZrDiagnostic **candidate =
                (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, index);
        if (candidate != ZR_NULL &&
            *candidate != ZR_NULL &&
            test_string_equals((*candidate)->code, "weak_value_requires_wake")) {
            diagnosticCount++;
        }
    }
    diagnostic = find_diagnostic_by_code_and_line(analyzer, "weak_value_requires_wake", 7);
    violationFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "watcher", 1));
    if (diagnosticCount != 1 ||
        diagnostic == ZR_NULL ||
        diagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 1 ||
        violationFact == ZR_NULL ||
        violationFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
        violationFact->qualifier != ZR_OWNERSHIP_QUALIFIER_WEAK ||
        violationFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
        violationFact->lifetimeRegionId == ZR_SEMANTIC_ID_INVALID ||
        violationFact->ownerLifetimeRegionId == ZR_SEMANTIC_ID_INVALID ||
        violationFact->relatedNode == ZR_NULL ||
        !violationFact->isViolation ||
        !ownership_fact_message_contains(violationFact, "Weak")) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Weak borrowed use did not merge owner-release evidence");
        return;
    }

    related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &diagnostic->relatedInformation,
            0);
    if (related == ZR_NULL ||
        !test_string_equals(related->message, "Owner was released here") ||
        related->location.start.line != 6) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Weak diagnostic did not retain the possible release point");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_rebinds_borrowed_alias_owner(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Rebinds Borrowed Alias Owner";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn use(first: Shared<Resource>, second: Shared<Resource>): int {\n"
        "    var alias = ref first;\n"
        "    alias = ref second;\n"
        "    drop(first);\n"
        "    alias;\n"
        "    drop(second);\n"
        "    alias;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *oldOwnerDiagnostic;
    SZrDiagnostic *newOwnerDiagnostic;
    const SZrDiagnosticRelatedInformation *related;
    const SZrSemanticOwnershipFact *initialFact;
    const SZrSemanticOwnershipFact *rebindFact;

    TEST_START(summary);
    TEST_INFO("Straight-line borrowed alias rebinding",
              "The alias must stop depending on the old owner and track the replacement owner region");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_borrow_assignment_rebind_test.zr",
                                      strlen("ownership_borrow_assignment_rebind_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) ZrParser_Ast_Free(state, ast);
        if (analyzer != ZR_NULL) ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to prepare borrowed-rebind fixture");
        return;
    }

    oldOwnerDiagnostic = find_diagnostic_by_code_and_line(analyzer, "borrow_escape", 7);
    newOwnerDiagnostic = find_diagnostic_by_code_and_line(analyzer, "borrow_escape", 9);
    initialFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "first", 1));
    rebindFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "second", 1));
    if (oldOwnerDiagnostic != ZR_NULL ||
        newOwnerDiagnostic == ZR_NULL ||
        !newOwnerDiagnostic->relatedInformation.isValid ||
        newOwnerDiagnostic->relatedInformation.length != 1 ||
        initialFact == ZR_NULL ||
        initialFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_BORROW ||
        rebindFact == ZR_NULL ||
        rebindFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_BORROW ||
        initialFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
        rebindFact->symbolId != initialFact->symbolId ||
        rebindFact->lifetimeRegionId != initialFact->lifetimeRegionId ||
        rebindFact->ownerLifetimeRegionId == initialFact->ownerLifetimeRegionId) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Borrowed assignment did not replace the owner-region binding");
        return;
    }

    related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &newOwnerDiagnostic->relatedInformation,
            0);
    if (related == ZR_NULL || related->location.start.line != 8 ||
        !test_string_equals(related->message, "Owner was released here")) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Borrowed rebind diagnostic retained the wrong release point");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_rebinds_loaned_alias_owner(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Rebinds Loaned Alias Owner";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn use(first: Unique<Resource>, second: Unique<Resource>): int {\n"
        "    var alias = ref first;\n"
        "    alias = ref second;\n"
        "    drop(first);\n"
        "    alias;\n"
        "    drop(second);\n"
        "    alias;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *oldOwnerDiagnostic;
    SZrDiagnostic *newOwnerDiagnostic;
    const SZrDiagnosticRelatedInformation *related;
    const SZrSemanticOwnershipFact *initialFact;
    const SZrSemanticOwnershipFact *rebindFact;

    TEST_START(summary);
    TEST_INFO("Straight-line loaned alias rebinding",
              "A reassigned loan must diagnose only after the replacement owner is released");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_loan_assignment_rebind_test.zr",
                                      strlen("ownership_loan_assignment_rebind_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) ZrParser_Ast_Free(state, ast);
        if (analyzer != ZR_NULL) ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to prepare loaned-rebind fixture");
        return;
    }

    oldOwnerDiagnostic = find_diagnostic_by_code_and_line(analyzer, "loan_escape", 7);
    newOwnerDiagnostic = find_diagnostic_by_code_and_line(analyzer, "loan_escape", 9);
    initialFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "first", 1));
    rebindFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "second", 1));
    if (oldOwnerDiagnostic != ZR_NULL ||
        newOwnerDiagnostic == ZR_NULL ||
        !newOwnerDiagnostic->relatedInformation.isValid ||
        newOwnerDiagnostic->relatedInformation.length != 1 ||
        initialFact == ZR_NULL ||
        initialFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_MOVE ||
        rebindFact == ZR_NULL ||
        rebindFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_MOVE ||
        initialFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
        rebindFact->symbolId != initialFact->symbolId ||
        rebindFact->lifetimeRegionId != initialFact->lifetimeRegionId ||
        rebindFact->ownerLifetimeRegionId == initialFact->ownerLifetimeRegionId) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Loaned assignment did not replace the owner-region binding");
        return;
    }

    related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &newOwnerDiagnostic->relatedInformation,
            0);
    if (related == ZR_NULL || related->location.start.line != 8 ||
        !test_string_equals(related->message, "Owner was released here")) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Loaned rebind diagnostic retained the wrong release point");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_rebinds_weak_alias_owner(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Rebinds Weak Alias Owner";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn observe(resource: ref readonly Resource): int { return 0; }\n"
        "fn use(first: Shared<Resource>, second: Shared<Resource>): int {\n"
        "    var watcher = degrade(first);\n"
        "    watcher = degrade(second);\n"
        "    drop(first);\n"
        "    observe(ref watcher);\n"
        "    drop(second);\n"
        "    observe(ref watcher);\n"
        "    var woken = wake(watcher);\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *oldOwnerDiagnostic;
    SZrDiagnostic *newOwnerDiagnostic;
    const SZrDiagnosticRelatedInformation *related;
    const SZrSemanticOwnershipFact *initialFact;
    const SZrSemanticOwnershipFact *rebindFact;
    TZrSize weakDiagnosticCount = 0;
    TZrSize index;

    TEST_START(summary);
    TEST_INFO("Straight-line weak alias rebinding",
              "Only the replacement owner's release may enrich later direct borrowed use");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_weak_assignment_rebind_test.zr",
                                      strlen("ownership_weak_assignment_rebind_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) ZrParser_Ast_Free(state, ast);
        if (analyzer != ZR_NULL) ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to prepare weak-rebind fixture");
        return;
    }

    for (index = 0; index < analyzer->diagnostics.length; index++) {
        SZrDiagnostic **candidate =
                (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, index);
        if (candidate != ZR_NULL && *candidate != ZR_NULL &&
            test_string_equals((*candidate)->code, "weak_value_requires_wake")) {
            weakDiagnosticCount++;
        }
    }
    oldOwnerDiagnostic = find_diagnostic_by_code_and_line(
            analyzer,
            "weak_value_requires_wake",
            8);
    newOwnerDiagnostic = find_diagnostic_by_code_and_line(
            analyzer,
            "weak_value_requires_wake",
            10);
    initialFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "first", 1));
    rebindFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "second", 1));
    if (weakDiagnosticCount != 2 ||
        oldOwnerDiagnostic == ZR_NULL ||
        (oldOwnerDiagnostic->relatedInformation.isValid &&
         oldOwnerDiagnostic->relatedInformation.length != 0) ||
        newOwnerDiagnostic == ZR_NULL ||
        !newOwnerDiagnostic->relatedInformation.isValid ||
        newOwnerDiagnostic->relatedInformation.length != 1 ||
        initialFact == ZR_NULL ||
        initialFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_COPY ||
        rebindFact == ZR_NULL ||
        rebindFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_COPY ||
        initialFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
        rebindFact->symbolId != initialFact->symbolId ||
        rebindFact->lifetimeRegionId != initialFact->lifetimeRegionId ||
        rebindFact->ownerLifetimeRegionId == initialFact->ownerLifetimeRegionId) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Weak assignment did not replace the owner-region binding");
        return;
    }

    related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &newOwnerDiagnostic->relatedInformation,
            0);
    if (related == ZR_NULL || related->location.start.line != 9 ||
        !test_string_equals(related->message, "Owner was released here")) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Weak rebind diagnostic retained the wrong release point");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

#endif
