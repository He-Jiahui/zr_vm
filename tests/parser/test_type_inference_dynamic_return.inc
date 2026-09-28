static void test_dynamic_object_local_return_expression_keeps_unknown_type_identity(void) {
    const char *source =
            "var marker = zr.__forcePoolFfiGc();\n"
            "return 42 + marker;\n";
    SZrState *state = create_test_state();
    SZrCompilerState *cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrAstNode *returnExpression;
    SZrAstNode *markerRead;
    const SZrTypeBinding *binding;
    const SZrSemanticReferenceFact *reference;
    SZrInferredType inferred;

    TEST_ASSERT_NOT_NULL(state);
    cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    sourceName = ZrCore_String_CreateFromNative(state, "dynamic_return_type_test.zr");
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT32(2u, ast->data.script.statements->count);
    cs->scriptAst = ast;

    declaration = ast->data.script.statements->nodes[0];
    compile_test_top_level_statement(cs, declaration);
    TEST_ASSERT_FALSE(cs->hasError);
    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            cs->typeEnv, declaration->data.variableDeclaration.pattern->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE(binding->hasDeclarationRange);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, binding->type.baseType);

    returnExpression = ast->data.script.statements->nodes[1]->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(returnExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, returnExpression->type);
    markerRead = returnExpression->data.binaryExpression.right;
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, markerRead->type);
    ZrParser_InferredType_Init(state, &inferred, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, returnExpression, &inferred));
    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, inferred.baseType);
    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            cs->semanticContext, markerRead, ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(reference);
    TEST_ASSERT_EQUAL_UINT32(binding->symbolId, reference->symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, reference->typeId);

    ZrParser_InferredType_Free(state, &inferred);
    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);
}
