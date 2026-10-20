#ifndef ZR_TEST_SEMANTIC_DA_AST_FACT_CASES_H
#define ZR_TEST_SEMANTIC_DA_AST_FACT_CASES_H

static SZrAstNode *g_daAstFactRoot;
static SZrSemanticContext *g_daAstFactContext;

static void semantic_da_ast_fact_cleanup(void) {
    if (g_daAstFactContext != ZR_NULL) {
        ZrParser_SemanticContext_Free(g_daAstFactContext);
        g_daAstFactContext = ZR_NULL;
    }
    if (g_daAstFactRoot != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_daAstFactRoot);
        g_daAstFactRoot = ZR_NULL;
    }
}

static void test_cfg_definite_assignment_accepts_standalone_body_root(void) {
    const TZrChar *source = "fn choose(): int { var missing: int; return missing; }";
    const TZrChar *name = "facts_standalone_body.zr";
    SZrString *sourceName = ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name));
    SZrAstNode *functionNode;
    SZrAstNode *body;
    SZrAstNode *declaration;
    SZrAstNode *returnNode;
    SZrAstNode *readNode;
    const SZrSemanticReferenceFact *read;

    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Fixture prerequisite: source name allocation failed");
    g_daAstFactRoot = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_daAstFactRoot, "Fixture prerequisite: standalone body parsing failed");
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_daAstFactRoot->type);
    TEST_ASSERT_NOT_NULL(g_daAstFactRoot->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1U, g_daAstFactRoot->data.script.statements->count);
    functionNode = g_daAstFactRoot->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, functionNode->type);
    body = functionNode->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(2U, body->data.block.body->count);
    declaration = body->data.block.body->nodes[0];
    returnNode = body->data.block.body->nodes[1];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NULL(declaration->data.variableDeclaration.value);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.pattern);
    TEST_ASSERT_NOT_NULL(returnNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, returnNode->type);
    readNode = returnNode->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(readNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, readNode->type);

    g_daAstFactContext = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_daAstFactContext, "Fixture prerequisite: semantic context allocation failed");
    append_reference_fact(g_daAstFactContext, declaration, ZR_SEMANTIC_REFERENCE_DECLARATION,
                          117U, declaration->data.variableDeclaration.pattern->location);
    append_reference_fact(g_daAstFactContext, readNode, ZR_SEMANTIC_REFERENCE_READ,
                          117U, declaration->data.variableDeclaration.pattern->location);
    read = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            g_daAstFactContext, readNode, ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL_MESSAGE(read, "Fixture prerequisite: actual body READ missing");
    TEST_ASSERT_TRUE(read->isResolved);
    TEST_ASSERT_FALSE(read->hasDefiniteAssignmentState);

    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(g_daAstFactContext, body),
            "Public resolver must accept the actual standalone BLOCK root");
    read = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            g_daAstFactContext, readNode, ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_TRUE_MESSAGE(read->hasDefiniteAssignmentState,
                             "Standalone BLOCK root must run its CFG rather than succeed without visiting it");
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT, read->definiteAssignmentState);
}

#endif
