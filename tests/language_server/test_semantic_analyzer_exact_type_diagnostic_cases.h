#ifndef ZR_VM_TEST_SEMANTIC_ANALYZER_EXACT_TYPE_DIAGNOSTIC_CASES_H
#define ZR_VM_TEST_SEMANTIC_ANALYZER_EXACT_TYPE_DIAGNOSTIC_CASES_H

static void test_semantic_analyzer_preserves_cannot_infer_exact_type_golden_parity(
        SZrState *state) {
    const TZrChar *summary =
            "Semantic Analyzer Preserves Cannot Infer Exact Type Golden Parity";
    const TZrChar *testCode =
            "fn redact(value): void {\n"
            "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics queryDiagnostics;
    SZrFileRange expectedRange;
    SZrAstNode *functionNode;
    SZrAstNode *parameterNode;
    TZrSize queryCount = 0U;
    TZrSize expectedRangeCount = 0U;
    TZrSize projectedCount;
    TZrSize compilerErrorCount;
    TZrSize index;

    TEST_START(summary);
    TEST_INFO("Compiler query to LSP projection",
              "An unavailable exact type must preserve parser descriptor 2020, the exact type range, and the no-fix disposition");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "cannot_infer_exact_type_golden_parity_test.zr",
            strlen("cannot_infer_exact_type_golden_parity_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to analyze exact-type diagnostic fixture");
        return;
    }

    functionNode = ast->type == ZR_AST_SCRIPT &&
                           ast->data.script.statements != ZR_NULL &&
                           ast->data.script.statements->nodes != ZR_NULL &&
                           ast->data.script.statements->count > 0U
                       ? ast->data.script.statements->nodes[0]
                       : ZR_NULL;
    parameterNode = functionNode != ZR_NULL &&
                            functionNode->type == ZR_AST_FUNCTION_DECLARATION &&
                            functionNode->data.functionDeclaration.params != ZR_NULL &&
                            functionNode->data.functionDeclaration.params->nodes != ZR_NULL &&
                            functionNode->data.functionDeclaration.params->count > 0U
                        ? functionNode->data.functionDeclaration.params->nodes[0]
                        : ZR_NULL;
    if (parameterNode == ZR_NULL ||
        parameterNode->type != ZR_AST_PARAMETER ||
        parameterNode->data.parameter.name == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to locate exact-type failure parameter");
        return;
    }
    expectedRange = parameterNode->data.parameter.nameLocation;
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
        TEST_FAIL(timer, summary, "Failed to query exact-type diagnostics");
        return;
    }

    for (index = 0U; index < queryDiagnostics.count; index++) {
        const SZrStructuredDiagnostic *structured = &queryDiagnostics.items[index];
        const TZrChar *code = structured->code != ZR_NULL
                                      ? ZrCore_String_GetNativeString(structured->code)
                                      : ZR_NULL;
        const SZrDiagnostic *projected;

        if (code == ZR_NULL || strcmp(code, "cannot_infer_exact_type") != 0) {
            continue;
        }
        queryCount++;
        if (diagnostic_ranges_equal(&structured->location, &expectedRange)) {
            expectedRangeCount++;
        }
        projected = find_projected_query_diagnostic(analyzer, structured);
        if (!diagnostic_projection_matches_query(projected, structured) ||
            structured->descriptorId != 2020U ||
            structured->noFixReason !=
                    ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
            diagnostic_array_length(&structured->fixes) != 0U) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary,
                      "The projected exact-type diagnostic diverged from the compiler query fact");
            return;
        }
    }

    projectedCount = count_diagnostics_with_code(
            analyzer,
            "cannot_infer_exact_type");
    compilerErrorCount = count_diagnostics_with_code(
            analyzer,
            "compiler_error");
    if (queryCount == 0U || expectedRangeCount != 1U ||
        projectedCount != queryCount || compilerErrorCount != 0U) {
        TZrChar details[256];

        snprintf(details,
                 sizeof(details),
                 "Expected canonical parity: query=%zu targetRange=%zu projected=%zu compilerError=%zu",
                 (size_t)queryCount,
                 (size_t)expectedRangeCount,
                 (size_t)projectedCount,
                 (size_t)compilerErrorCount);
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, details);
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

#endif
