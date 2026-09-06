#ifndef ZR_VM_TEST_SEMANTIC_ANALYZER_EXACT_TYPE_CASES_H
#define ZR_VM_TEST_SEMANTIC_ANALYZER_EXACT_TYPE_CASES_H

static SZrAstNode *semantic_analyzer_exact_type_initializer_at(
        SZrAstNode *functionNode,
        TZrSize statementIndex) {
    SZrAstNodeArray *body;
    SZrAstNode *statement;

    if (functionNode == ZR_NULL ||
        functionNode->type != ZR_AST_FUNCTION_DECLARATION ||
        functionNode->data.functionDeclaration.body == ZR_NULL ||
        functionNode->data.functionDeclaration.body->type != ZR_AST_BLOCK) {
        return ZR_NULL;
    }
    body = functionNode->data.functionDeclaration.body->data.block.body;
    if (body == ZR_NULL || body->nodes == ZR_NULL || statementIndex >= body->count) {
        return ZR_NULL;
    }
    statement = body->nodes[statementIndex];
    return statement != ZR_NULL && statement->type == ZR_AST_VARIABLE_DECLARATION
               ? statement->data.variableDeclaration.value
               : ZR_NULL;
}

static void test_semantic_analyzer_expression_metadata_records_exact_types(
        SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Expression Facts Record Exact Types";
    const TZrChar *testCode =
            "fn compute(left: int, right: int) {\n"
            "    var sum: int = left + right;\n"
            "    var widened: float = left + 0.0;\n"
            "    return widened;\n"
            "}\n";
    TZrChar sourceText[] = "expression_hover_exact_type_test.zr";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *computeNode;
    SZrAstNode *sumExpr;
    SZrAstNode *widenedExpr;
    const SZrSemanticExpressionFact *sumFact;
    const SZrSemanticExpressionFact *widenedFact;

    TEST_START(summary);
    TEST_INFO("Expression exact type facts",
              "Canonical type records are structural; expression facts retain the exact inferred type for each AST node");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state, sourceText, strlen(sourceText));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare exact expression-type fixture");
        return;
    }

    computeNode = ast->type == ZR_AST_SCRIPT &&
                          ast->data.script.statements != ZR_NULL &&
                          ast->data.script.statements->nodes != ZR_NULL &&
                          ast->data.script.statements->count > 0
                      ? ast->data.script.statements->nodes[0]
                      : ZR_NULL;
    sumExpr = semantic_analyzer_exact_type_initializer_at(computeNode, 0);
    widenedExpr = semantic_analyzer_exact_type_initializer_at(computeNode, 1);
    sumFact = ZrParser_SemanticFacts_FindExpressionByNode(
            analyzer->semanticContext,
            sumExpr);
    widenedFact = ZrParser_SemanticFacts_FindExpressionByNode(
            analyzer->semanticContext,
            widenedExpr);

    if (sumFact == ZR_NULL ||
        sumFact->exactness != ZR_SEMANTIC_FACT_EXACT ||
        !ZR_VALUE_IS_TYPE_INT(sumFact->inferredType.baseType)) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Expected an exact int expression fact for left + right");
        return;
    }
    if (widenedFact == ZR_NULL ||
        widenedFact->exactness != ZR_SEMANTIC_FACT_EXACT ||
        !ZR_VALUE_IS_TYPE_FLOAT(widenedFact->inferredType.baseType)) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Expected an exact float expression fact for left + 0.0");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_type_resolution_rejects_approximate_expression_fact(
        SZrState *state) {
    const TZrChar *summary =
            "Semantic Analyzer Type Resolution Rejects Approximate Expression Fact";
    const TZrChar *testCode =
            "fn compute() {\n"
            "    return 1 + 2;\n"
            "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrString *sourceName = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrFileRange operatorRange;
    SZrSemanticExpressionFact *fact = ZR_NULL;
    SZrSemanticContext *savedSemanticContext = ZR_NULL;
    SZrInferredType resolvedType;
    EZrSemanticFactExactness savedExactness = ZR_SEMANTIC_FACT_UNKNOWN;
    TZrBool semanticContextDetached = ZR_FALSE;
    const TZrChar *failure = ZR_NULL;

    TEST_START(summary);
    TEST_INFO("Canonical expression type exactness",
              "An approximate parser fact must not be replaced by request-time AST inference");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "type_resolution_approximate_fact_test.zr",
            strlen("type_resolution_approximate_fact_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        failure = "Failed to prepare canonical expression-type fixture";
        goto cleanup;
    }

    operatorRange = file_range_for_nth_substring_in_source(
            testCode, "+", 0, ZR_FALSE, sourceName);
    fact = (SZrSemanticExpressionFact *)
            ZrParser_SemanticFacts_FindExpressionAtPosition(
                    analyzer->semanticContext, operatorRange);
    if (fact == ZR_NULL || fact->exactness != ZR_SEMANTIC_FACT_EXACT) {
        failure = "The baseline must publish an exact expression fact";
        goto cleanup;
    }

    ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_SemanticQuery_TypeAt(
                analyzer->semanticContext, operatorRange, ZR_NULL, &resolvedType) ||
        !ZR_VALUE_IS_TYPE_INT(resolvedType.baseType)) {
        ZrParser_InferredType_Free(state, &resolvedType);
        failure = "The baseline parser TypeAt query must resolve exact int";
        goto cleanup;
    }
    ZrParser_InferredType_Free(state, &resolvedType);

    savedExactness = fact->exactness;
    fact->exactness = ZR_SEMANTIC_FACT_APPROXIMATE;
    ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_SemanticQuery_TypeAt(
                analyzer->semanticContext, operatorRange, ZR_NULL, &resolvedType)) {
        failure = "Parser TypeAt accepted an approximate expression fact";
    } else if (ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition(
                       state, analyzer, operatorRange, &resolvedType)) {
        failure = "LSP type resolver revived an approximate fact through AST inference";
    }
    ZrParser_InferredType_Free(state, &resolvedType);

    savedSemanticContext = analyzer->semanticContext;
    analyzer->semanticContext = ZR_NULL;
    semanticContextDetached = ZR_TRUE;
    ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
    if (ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition(
                state, analyzer, operatorRange, &resolvedType)) {
        failure = "LSP type resolver revived unavailable canonical type through AST inference";
    }
    ZrParser_InferredType_Free(state, &resolvedType);

cleanup:
    if (semanticContextDetached && analyzer != ZR_NULL) {
        analyzer->semanticContext = savedSemanticContext;
    }
    if (savedExactness != ZR_SEMANTIC_FACT_UNKNOWN && fact != ZR_NULL) {
        fact->exactness = savedExactness;
    }
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    if (analyzer != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }
    if (failure == ZR_NULL) {
        TEST_PASS(timer, summary);
    } else {
        TEST_FAIL(timer, summary, failure);
    }
}

#endif // ZR_VM_TEST_SEMANTIC_ANALYZER_EXACT_TYPE_CASES_H
