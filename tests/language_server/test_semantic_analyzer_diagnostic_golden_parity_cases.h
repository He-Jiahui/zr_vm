#ifndef ZR_VM_TEST_SEMANTIC_ANALYZER_DIAGNOSTIC_GOLDEN_PARITY_CASES_H
#define ZR_VM_TEST_SEMANTIC_ANALYZER_DIAGNOSTIC_GOLDEN_PARITY_CASES_H

static TZrBool diagnostic_strings_equal(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }

    return ZrCore_String_Equal(left, right);
}

static TZrBool diagnostic_ranges_equal(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !diagnostic_strings_equal(left->source, right->source)) {
        return ZR_FALSE;
    }

    return left->start.offset == right->start.offset &&
           left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.offset == right->end.offset &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

static EZrDiagnosticSeverity diagnostic_severity_from_structured(
        EZrStructuredDiagnosticSeverity severity) {
    switch (severity) {
        case ZR_STRUCTURED_DIAGNOSTIC_WARNING:
            return ZR_DIAGNOSTIC_WARNING;
        case ZR_STRUCTURED_DIAGNOSTIC_INFO:
            return ZR_DIAGNOSTIC_INFO;
        case ZR_STRUCTURED_DIAGNOSTIC_HINT:
            return ZR_DIAGNOSTIC_HINT;
        case ZR_STRUCTURED_DIAGNOSTIC_ERROR:
        default:
            return ZR_DIAGNOSTIC_ERROR;
    }
}

static TZrSize diagnostic_array_length(const SZrArray *array) {
    return array != ZR_NULL && array->isValid ? array->length : 0U;
}

static const SZrDiagnostic *find_projected_query_diagnostic(
        SZrSemanticAnalyzer *analyzer,
        const SZrStructuredDiagnostic *structured) {
    TZrSize index;

    if (analyzer == ZR_NULL || structured == ZR_NULL ||
        !analyzer->diagnostics.isValid) {
        return ZR_NULL;
    }

    for (index = 0U; index < analyzer->diagnostics.length; index++) {
        SZrDiagnostic **candidatePtr =
                (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, index);
        const SZrDiagnostic *candidate =
                candidatePtr != ZR_NULL ? *candidatePtr : ZR_NULL;
        if (candidate != ZR_NULL &&
            diagnostic_strings_equal(candidate->code, structured->code) &&
            diagnostic_ranges_equal(&candidate->location, &structured->location)) {
            return candidate;
        }
    }

    return ZR_NULL;
}

static TZrBool diagnostic_projection_matches_query(
        const SZrDiagnostic *projected,
        const SZrStructuredDiagnostic *structured) {
    const SZrDiagnosticDescriptor *descriptor;
    TZrSize relatedCount;
    TZrSize fixCount;
    TZrSize index;

    if (projected == ZR_NULL || structured == ZR_NULL ||
        projected->severity !=
                diagnostic_severity_from_structured(structured->severity) ||
        !diagnostic_ranges_equal(&projected->location, &structured->location) ||
        !diagnostic_strings_equal(projected->code, structured->code) ||
        !diagnostic_strings_equal(projected->message, structured->message) ||
        !diagnostic_strings_equal(projected->cause, structured->cause) ||
        !diagnostic_strings_equal(projected->suggestion, structured->suggestion) ||
        projected->descriptorId != structured->descriptorId ||
        projected->noFixReason != structured->noFixReason) {
        return ZR_FALSE;
    }

    descriptor = ZrParser_DiagnosticRegistry_FindById(structured->descriptorId);
    if (descriptor != ZR_NULL && descriptor->helpUri != ZR_NULL) {
        const TZrChar *href = projected->codeDescriptionHref != ZR_NULL
                                      ? ZrCore_String_GetNativeString(
                                                projected->codeDescriptionHref)
                                      : ZR_NULL;
        if (href == ZR_NULL || strcmp(href, descriptor->helpUri) != 0) {
            return ZR_FALSE;
        }
    } else if (projected->codeDescriptionHref != ZR_NULL) {
        return ZR_FALSE;
    }

    relatedCount = diagnostic_array_length(&structured->relatedInformation);
    if (diagnostic_array_length(&projected->relatedInformation) != relatedCount) {
        return ZR_FALSE;
    }
    for (index = 0U; index < relatedCount; index++) {
        const SZrStructuredDiagnosticRelatedInformation *expected =
                (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                        (SZrArray *)&structured->relatedInformation,
                        index);
        const SZrDiagnosticRelatedInformation *actual =
                (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
                        (SZrArray *)&projected->relatedInformation,
                        index);
        if (expected == ZR_NULL || actual == ZR_NULL ||
            !diagnostic_ranges_equal(&actual->location, &expected->location) ||
            !diagnostic_strings_equal(actual->message, expected->message)) {
            return ZR_FALSE;
        }
    }

    fixCount = diagnostic_array_length(&structured->fixes);
    if (diagnostic_array_length(&projected->fixes) != fixCount) {
        return ZR_FALSE;
    }
    for (index = 0U; index < fixCount; index++) {
        const SZrStructuredDiagnosticFix *expected =
                (const SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
                        (SZrArray *)&structured->fixes,
                        index);
        const SZrDiagnosticFix *actual =
                (const SZrDiagnosticFix *)ZrCore_Array_Get(
                        (SZrArray *)&projected->fixes,
                        index);
        if (expected == ZR_NULL || actual == ZR_NULL ||
            !diagnostic_strings_equal(actual->title, expected->title) ||
            !diagnostic_ranges_equal(&actual->editRange, &expected->editRange) ||
            !diagnostic_strings_equal(actual->editText, expected->editText) ||
            actual->applicability != expected->applicability) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static void test_semantic_analyzer_preserves_query_diagnostic_golden_parity(
        SZrState *state) {
    const TZrChar *summary =
            "Semantic Analyzer Preserves Query Diagnostic Golden Parity";
    const TZrChar *testCode =
            "resource class Resource {}\n"
            "fn initialize(source: Shared<Resource>) {\n"
            "    var target: Unique<Resource> = source;\n"
            "}\n"
            "fn assign(target: Unique<Resource>, source: Shared<Resource>) {\n"
            "    target = source;\n"
            "}\n"
            "fn upgrade(source: Shared<Resource>): Unique<Resource> {\n"
            "    return source;\n"
            "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics queryDiagnostics;
    TZrSize queryOwnershipMismatchCount = 0U;
    TZrSize index;

    TEST_START(summary);
    TEST_INFO("Compiler query to LSP projection",
              "Initializer, assignment, and return diagnostics must preserve every structured field from the same semantic snapshot");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "diagnostic_golden_parity_test.zr",
            strlen("diagnostic_golden_parity_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to analyze diagnostic parity fixture");
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
        TEST_FAIL(timer, summary, "Failed to query compiler diagnostics");
        return;
    }

    for (index = 0U; index < queryDiagnostics.count; index++) {
        const SZrStructuredDiagnostic *structured = &queryDiagnostics.items[index];
        const TZrChar *code = structured->code != ZR_NULL
                                      ? ZrCore_String_GetNativeString(structured->code)
                                      : ZR_NULL;
        const SZrDiagnostic *projected;

        if (code == ZR_NULL || strcmp(code, "ownership_mismatch") != 0) {
            continue;
        }
        queryOwnershipMismatchCount++;
        projected = find_projected_query_diagnostic(analyzer, structured);
        if (!diagnostic_projection_matches_query(projected, structured)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      summary,
                      "An LSP diagnostic diverged from its compiler query diagnostic");
            return;
        }
    }

    if (queryOwnershipMismatchCount != 3U ||
        count_diagnostics_with_code(analyzer, "ownership_mismatch") != 3U) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Expected exactly three compiler and three projected ownership mismatch diagnostics");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_preserves_method_call_mismatch_golden_parity(
        SZrState *state) {
    const TZrChar *summary =
            "Semantic Analyzer Preserves Method Call Mismatch Golden Parity";
    const TZrChar *testCode =
            "class Meter {\n"
            "    pub fn write(value: int): int { return value; }\n"
            "}\n"
            "fn main(meter: Meter): int {\n"
            "    meter.write(2.5);\n"
            "    return 0;\n"
            "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics queryDiagnostics;
    TZrSize queryCount = 0U;

    TEST_START(summary);
    TEST_INFO("Compiler query to LSP projection",
              "Receiver method argument mismatches must preserve parser-owned ranges, relation, and fix fields");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "method_call_mismatch_golden_parity_test.zr",
            strlen("method_call_mismatch_golden_parity_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to analyze method mismatch parity fixture");
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
        TEST_FAIL(timer, summary, "Failed to query method mismatch diagnostics");
        return;
    }

    for (TZrSize index = 0U; index < queryDiagnostics.count; index++) {
        const SZrStructuredDiagnostic *structured = &queryDiagnostics.items[index];
        const TZrChar *code = structured->code != ZR_NULL
                                      ? ZrCore_String_GetNativeString(structured->code)
                                      : ZR_NULL;
        const SZrDiagnostic *projected;

        if (code == ZR_NULL || strcmp(code, "type_mismatch") != 0) {
            continue;
        }
        queryCount++;
        projected = find_projected_query_diagnostic(analyzer, structured);
        if (!diagnostic_projection_matches_query(projected, structured) ||
            structured->descriptorId != 2011U ||
            structured->location.start.line != 5 ||
            structured->location.start.column != 17 ||
            structured->location.end.line != 5 ||
            structured->location.end.column != 20 ||
            diagnostic_array_length(&structured->relatedInformation) != 1U ||
            diagnostic_array_length(&structured->fixes) != 1U) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      summary,
                      "The projected method mismatch diverged from the compiler query fact");
            return;
        }
    }

    if (queryCount != 1U ||
        count_diagnostics_with_code(analyzer, "type_mismatch") != 1U ||
        count_diagnostics_with_code(analyzer, "compiler_error") != 0U) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Expected one compiler method mismatch fact and one canonical LSP projection");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_preserves_duplicate_type_golden_parity(
        SZrState *state) {
    const TZrChar *summary =
            "Semantic Analyzer Preserves Duplicate Type Golden Parity";
    const TZrChar *testCode =
            "class Pair {}\n"
            "class Pair {}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics queryDiagnostics;
    TZrSize queryDuplicateCount = 0U;
    TZrSize index;

    TEST_START(summary);
    TEST_INFO("Compiler query to LSP projection",
              "Duplicate type diagnostics must preserve the parser-owned descriptor, ranges, relation, and no-fix disposition");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "duplicate_type_golden_parity_test.zr",
            strlen("duplicate_type_golden_parity_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to analyze duplicate type parity fixture");
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
        TEST_FAIL(timer, summary, "Failed to query duplicate type diagnostics");
        return;
    }

    for (index = 0U; index < queryDiagnostics.count; index++) {
        const SZrStructuredDiagnostic *structured = &queryDiagnostics.items[index];
        const TZrChar *code = structured->code != ZR_NULL
                                      ? ZrCore_String_GetNativeString(structured->code)
                                      : ZR_NULL;
        const SZrDiagnostic *projected;

        if (code == ZR_NULL || strcmp(code, "duplicate_type") != 0) {
            continue;
        }
        queryDuplicateCount++;
        projected = find_projected_query_diagnostic(analyzer, structured);
        if (!diagnostic_projection_matches_query(projected, structured)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      summary,
                      "The projected duplicate diagnostic diverged from the compiler query fact");
            return;
        }
    }

    if (queryDuplicateCount != 1U ||
        count_diagnostics_with_code(analyzer, "duplicate_type") != 1U ||
        count_diagnostics_with_code(analyzer, "compiler_error") != 0U) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Expected one compiler duplicate fact and one canonical LSP projection");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_preserves_initializer_annotation_golden_parity(
        SZrState *state) {
    const TZrChar *summary =
            "Semantic Analyzer Preserves Initializer Annotation Golden Parity";
    const TZrChar *testCode =
            "fn probe() {\n"
            "    var missing;\n"
            "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics queryDiagnostics;
    TZrSize queryCount = 0U;
    TZrSize index;

    TEST_START(summary);
    TEST_INFO("Compiler query to LSP projection",
              "Untyped uninitialized variables must preserve the parser-owned descriptor, exact name range, and no-fix disposition");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "initializer_annotation_golden_parity_test.zr",
            strlen("initializer_annotation_golden_parity_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to analyze initializer annotation parity fixture");
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
        TEST_FAIL(timer, summary, "Failed to query initializer annotation diagnostics");
        return;
    }

    for (index = 0U; index < queryDiagnostics.count; index++) {
        const SZrStructuredDiagnostic *structured = &queryDiagnostics.items[index];
        const TZrChar *code = structured->code != ZR_NULL
                                      ? ZrCore_String_GetNativeString(structured->code)
                                      : ZR_NULL;
        const SZrDiagnostic *projected;

        if (code == ZR_NULL ||
            strcmp(code, "initializer_requires_annotation") != 0) {
            continue;
        }
        queryCount++;
        projected = find_projected_query_diagnostic(analyzer, structured);
        if (!diagnostic_projection_matches_query(projected, structured)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      summary,
                      "The projected initializer annotation diagnostic diverged from the compiler query fact");
            return;
        }
    }

    if (queryCount != 1U ||
        count_diagnostics_with_code(
                analyzer, "initializer_requires_annotation") != 1U ||
        count_diagnostics_with_code(analyzer, "compiler_error") != 0U) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Expected one compiler annotation fact and one canonical LSP projection");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_preserves_return_type_not_provable_golden_parity(
        SZrState *state) {
    const TZrChar *summary =
            "Semantic Analyzer Preserves Return Type Not Provable Golden Parity";
    const TZrChar *testCode =
            "fn probe(flag: bool) {\n"
            "    if (flag) {\n"
            "        return 1;\n"
            "    }\n"
            "    return \"text\";\n"
            "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics queryDiagnostics;
    TZrSize queryCount = 0U;
    TZrSize index;

    TEST_START(summary);
    TEST_INFO("Compiler query to LSP projection",
              "Incompatible unannotated returns must preserve the parser-owned callable range, related returns, and no-fix disposition");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "return_type_not_provable_golden_parity_test.zr",
            strlen("return_type_not_provable_golden_parity_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to analyze return inference parity fixture");
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
        TEST_FAIL(timer, summary, "Failed to query return inference diagnostics");
        return;
    }

    for (index = 0U; index < queryDiagnostics.count; index++) {
        const SZrStructuredDiagnostic *structured = &queryDiagnostics.items[index];
        const TZrChar *code = structured->code != ZR_NULL
                                      ? ZrCore_String_GetNativeString(structured->code)
                                      : ZR_NULL;
        const SZrDiagnostic *projected;

        if (code == ZR_NULL || strcmp(code, "return_type_not_provable") != 0) {
            continue;
        }
        queryCount++;
        projected = find_projected_query_diagnostic(analyzer, structured);
        if (!diagnostic_projection_matches_query(projected, structured) ||
            structured->descriptorId != 2018U ||
            structured->noFixReason !=
                    ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
            diagnostic_array_length(&structured->relatedInformation) != 2U ||
            diagnostic_array_length(&structured->fixes) != 0U) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      summary,
                      "The projected return inference diagnostic diverged from the compiler query fact");
            return;
        }
    }

    if (queryCount != 1U ||
        count_diagnostics_with_code(analyzer, "return_type_not_provable") != 1U ||
        count_diagnostics_with_code(analyzer, "compiler_error") != 0U) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Expected one compiler return inference fact and one canonical LSP projection");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

#endif
