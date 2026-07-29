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

#endif // ZR_VM_TEST_SEMANTIC_ANALYZER_EXACT_TYPE_CASES_H
