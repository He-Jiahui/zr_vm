static void test_generic_type_use_publishes_exact_whole_display_alias(void) {
    const TZrChar *source = "var value: Box<i64> = null;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "generic_alias_display.zr");
    SZrString *boxName = ZrCore_String_CreateFromNative(g_state, "Box");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrAstNode *genericNode;
    SZrFileRange expectedRange;
    SZrCompilerState cs;
    SZrInferredType inferred;
    TZrTypeId typeId;
    SZrString *alias;
    TZrChar buffer[64];

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(boxName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(1U, ast->data.script.statements->count);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.typeInfo);
    genericNode = declaration->data.variableDeclaration.typeInfo->name;
    TEST_ASSERT_NOT_NULL(genericNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, genericNode->type);

    memset(&expectedRange, 0, sizeof(expectedRange));
    expectedRange.start.offset = 11U;
    expectedRange.start.line = 1;
    expectedRange.start.column = 12;
    expectedRange.end.offset = 19U;
    expectedRange.end.line = 1;
    expectedRange.end.column = 20;
    expectedRange.source = sourceName;
    TEST_ASSERT_EQUAL_UINT(
            expectedRange.start.offset,
            genericNode->data.genericType.wholeRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(
            expectedRange.end.offset,
            genericNode->data.genericType.wholeRange.end.offset);
    TEST_ASSERT_EQUAL_INT(
            expectedRange.start.column,
            genericNode->data.genericType.wholeRange.start.column);
    TEST_ASSERT_EQUAL_INT(
            expectedRange.end.column,
            genericNode->data.genericType.wholeRange.end.column);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterType(
            g_state, cs.typeEnv, boxName));
    ZrParser_InferredType_Init(g_state, &inferred, ZR_VALUE_TYPE_UNKNOWN);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &cs, declaration->data.variableDeclaration.typeInfo, &inferred));
    typeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &inferred);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, typeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            cs.semanticContext, typeId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("Box<int>", buffer);

    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            cs.semanticContext, typeId, &expectedRange);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_STRING("Box<i64>", ZrCore_String_GetNativeString(alias));

    ZrParser_InferredType_Free(g_state, &inferred);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_nested_generic_type_uses_preserve_split_angle_ranges(void) {
    const TZrChar *source = "var value: Box<Box<i64>> = null;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "nested_generic_alias_display.zr");
    SZrString *boxName = ZrCore_String_CreateFromNative(g_state, "Box");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrAstNode *outerNode;
    SZrAstNode *innerNode;
    SZrFileRange outerRange;
    SZrFileRange innerRange;
    SZrCompilerState cs;
    SZrInferredType inferred;
    const SZrInferredType *innerInferred;
    TZrTypeId outerTypeId;
    TZrTypeId innerTypeId;
    SZrString *alias;
    TZrChar buffer[64];

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(boxName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    outerNode = declaration->data.variableDeclaration.typeInfo->name;
    TEST_ASSERT_NOT_NULL(outerNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, outerNode->type);
    TEST_ASSERT_NOT_NULL(outerNode->data.genericType.params);
    TEST_ASSERT_EQUAL_UINT(1U, outerNode->data.genericType.params->count);
    TEST_ASSERT_NOT_NULL(outerNode->data.genericType.params->nodes[0]);
    innerNode = outerNode->data.genericType.params->nodes[0]->data.type.name;
    TEST_ASSERT_NOT_NULL(innerNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, innerNode->type);

    memset(&outerRange, 0, sizeof(outerRange));
    outerRange.start.offset = 11U;
    outerRange.start.line = 1;
    outerRange.start.column = 12;
    outerRange.end.offset = 24U;
    outerRange.end.line = 1;
    outerRange.end.column = 25;
    outerRange.source = sourceName;
    memset(&innerRange, 0, sizeof(innerRange));
    innerRange.start.offset = 15U;
    innerRange.start.line = 1;
    innerRange.start.column = 16;
    innerRange.end.offset = 23U;
    innerRange.end.line = 1;
    innerRange.end.column = 24;
    innerRange.source = sourceName;
    TEST_ASSERT_EQUAL_UINT(
            outerRange.start.offset,
            outerNode->data.genericType.wholeRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(
            outerRange.end.offset,
            outerNode->data.genericType.wholeRange.end.offset);
    TEST_ASSERT_EQUAL_UINT(
            innerRange.start.offset,
            innerNode->data.genericType.wholeRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(
            innerRange.end.offset,
            innerNode->data.genericType.wholeRange.end.offset);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterType(
            g_state, cs.typeEnv, boxName));
    ZrParser_InferredType_Init(g_state, &inferred, ZR_VALUE_TYPE_UNKNOWN);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &cs, declaration->data.variableDeclaration.typeInfo, &inferred));
    TEST_ASSERT_EQUAL_UINT(1U, inferred.elementTypes.length);
    innerInferred = (const SZrInferredType *)ZrCore_Array_Get(
            &inferred.elementTypes, 0U);
    TEST_ASSERT_NOT_NULL(innerInferred);
    outerTypeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &inferred);
    innerTypeId = ZrParser_CanonicalType_FromInferred(
            cs.semanticContext, innerInferred);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, outerTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, innerTypeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            cs.semanticContext, outerTypeId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("Box<Box<int>>", buffer);

    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            cs.semanticContext, outerTypeId, &outerRange);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_STRING("Box<Box<i64>>", ZrCore_String_GetNativeString(alias));
    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            cs.semanticContext, innerTypeId, &innerRange);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_STRING("Box<i64>", ZrCore_String_GetNativeString(alias));

    ZrParser_InferredType_Free(g_state, &inferred);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}
