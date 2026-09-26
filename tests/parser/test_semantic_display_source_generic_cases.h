#ifndef ZR_VM_TEST_SEMANTIC_DISPLAY_SOURCE_GENERIC_CASES_H
#define ZR_VM_TEST_SEMANTIC_DISPLAY_SOURCE_GENERIC_CASES_H

static void test_source_type_display_projects_nested_generic_parameter_names(void) {
    const TZrChar *source = "fn source<T, U, const N: int>() { }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "source_generic.zr");
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode *declaration;
    SZrCanonicalGenericArgument arguments[2] = {0};
    TZrSymbolId owner;
    TZrTypeId typeParameter;
    TZrTypeId arrayType;
    TZrTypeId boxDefinition;
    TZrTypeId boxType;
    TZrChar buffer[128];

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(context);
    declaration = ast->data.script.statements->nodes[0];
    owner = ZrParser_Semantic_RegisterSymbol(
            context, declaration->data.functionDeclaration.name->name,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID, declaration, declaration->location);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, owner);
    typeParameter = ZrParser_CanonicalType_InternGenericParameter(context, owner, 0U);
    arrayType = ZrParser_CanonicalType_InternArray(
            context, typeParameter, 1U, ZR_CANONICAL_ARRAY_STORAGE_MANAGED);
    boxDefinition = ZrParser_CanonicalType_InternNominal(
            context, ZR_NULL, ZrCore_String_CreateFromNative(g_state, "Box"), 0U);
    arguments[0].kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
    arguments[0].data.typeId = arrayType;
    arguments[1].kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER;
    arguments[1].data.constParameter.ownerSymbolId = owner;
    arguments[1].data.constParameter.ordinal = 2U;
    arguments[1].data.constParameter.displayName =
            ZrCore_String_CreateFromNative(g_state, "UnrelatedAlias");
    boxType = ZrParser_CanonicalType_InternGenericInstanceEx(
            context, boxDefinition, arguments, 2U);

    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatSourceType(
            context, typeParameter, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("T", buffer);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatSourceType(
            context, boxType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("Box<T[], N>", buffer);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            context, boxType, buffer, sizeof(buffer)));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "!"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "$const("));
    ZrParser_SemanticContext_Free(context);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_type_display_rejects_missing_generic_declaration_identity(void) {
    const TZrChar *source = "fn source<T, const N: int>() { }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "missing_generic.zr");
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode *declaration;
    TZrSymbolId owner;
    TZrSymbolId anonymousOwner;
    TZrTypeId invalidTypes[4];
    TZrSize index;
    TZrChar buffer[128];

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(context);
    declaration = ast->data.script.statements->nodes[0];
    owner = ZrParser_Semantic_RegisterSymbol(
            context, declaration->data.functionDeclaration.name->name,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID, declaration, declaration->location);
    anonymousOwner = ZrParser_Semantic_RegisterSymbol(
            context, declaration->data.functionDeclaration.name->name,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID, ZR_NULL, declaration->location);
    invalidTypes[0] = ZrParser_CanonicalType_InternGenericParameter(
            context, owner + anonymousOwner + 1U, 0U);
    invalidTypes[1] = ZrParser_CanonicalType_InternGenericParameter(
            context, anonymousOwner, 0U);
    invalidTypes[2] = ZrParser_CanonicalType_InternGenericParameter(context, owner, 2U);
    invalidTypes[3] = ZrParser_CanonicalType_InternGenericParameter(context, owner, 1U);
    for (index = 0U; index < sizeof(invalidTypes) / sizeof(invalidTypes[0]); ++index) {
        strcpy(buffer, "sentinel");
        TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatSourceType(
                context, invalidTypes[index], buffer, sizeof(buffer)));
        TEST_ASSERT_EQUAL_STRING("", buffer);
    }
    ZrParser_SemanticContext_Free(context);
    ZrParser_Ast_Free(g_state, ast);
}

#endif
