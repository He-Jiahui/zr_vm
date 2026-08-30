static void test_type_value_alias_use_preserves_nominal_source_alias(void) {
    const TZrChar *source = "var value: Word = null;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "type_value_alias_display.zr");
    SZrString *aliasName = ZrCore_String_CreateFromNative(g_state, "Word");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrFileRange expectedRange;
    SZrCompilerState cs;
    SZrTypeBinding binding;
    SZrInferredType inferred;
    TZrTypeId typeId;
    SZrString *alias;
    TZrChar buffer[64];

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(aliasName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.typeInfo);

    memset(&expectedRange, 0, sizeof(expectedRange));
    expectedRange.start.offset = 11U;
    expectedRange.start.line = 1;
    expectedRange.start.column = 12;
    expectedRange.end.offset = 15U;
    expectedRange.end.line = 1;
    expectedRange.end.column = 16;
    expectedRange.source = sourceName;

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    memset(&binding, 0, sizeof(binding));
    binding.name = aliasName;
    ZrParser_InferredType_Init(g_state, &binding.type, ZR_VALUE_TYPE_INT64);
    ZrCore_Array_Push(g_state, &cs.typeValueAliases, &binding);

    ZrParser_InferredType_Init(g_state, &inferred, ZR_VALUE_TYPE_UNKNOWN);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &cs, declaration->data.variableDeclaration.typeInfo, &inferred));
    typeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &inferred);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, typeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            cs.semanticContext, typeId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("int", buffer);

    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            cs.semanticContext, typeId, &expectedRange);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_STRING("Word", ZrCore_String_GetNativeString(alias));

    ZrParser_InferredType_Free(g_state, &inferred);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}
