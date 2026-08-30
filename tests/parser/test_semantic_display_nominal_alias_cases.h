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

static void test_owner_type_value_alias_preserves_inner_source_alias(void) {
    const TZrChar *source = "var handle: Unique<Word> = null;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "owner_type_value_alias_display.zr");
    SZrString *aliasName = ZrCore_String_CreateFromNative(g_state, "Word");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrGenericType *genericType;
    SZrAstNode *argumentNode;
    SZrCompilerState cs;
    SZrTypeBinding binding;
    SZrInferredType inferred;
    TZrTypeId ownerTypeId;
    TZrTypeId intTypeId;
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
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.typeInfo->name);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_GENERIC_TYPE,
            declaration->data.variableDeclaration.typeInfo->name->type);
    genericType = &declaration->data.variableDeclaration.typeInfo->name->data.genericType;
    TEST_ASSERT_NOT_NULL(genericType->params);
    TEST_ASSERT_EQUAL_UINT(1U, genericType->params->count);
    argumentNode = genericType->params->nodes[0];
    TEST_ASSERT_NOT_NULL(argumentNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, argumentNode->type);
    TEST_ASSERT_NOT_NULL(argumentNode->data.type.name);
    TEST_ASSERT_EQUAL_UINT64(19U, argumentNode->data.type.name->location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(23U, argumentNode->data.type.name->location.end.offset);

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
    ownerTypeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &inferred);
    intTypeId = ZrParser_CanonicalType_InternPrimitive(
            cs.semanticContext, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, ownerTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, intTypeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            cs.semanticContext, ownerTypeId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("Unique<int>", buffer);

    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            cs.semanticContext,
            intTypeId,
            &argumentNode->data.type.name->location);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_STRING("Word", ZrCore_String_GetNativeString(alias));

    ZrParser_InferredType_Free(g_state, &inferred);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void semantic_display_register_gc_bridge_alias_target(
        SZrCompilerState *cs,
        TZrNativeString aliasText,
        TZrNativeString targetText,
        TZrBool isResource) {
    SZrTypePrototypeInfo prototype;
    SZrTypeBinding binding;

    memset(&prototype, 0, sizeof(prototype));
    prototype.name = ZrCore_String_CreateFromNative(g_state, targetText);
    TEST_ASSERT_NOT_NULL(prototype.name);
    prototype.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    prototype.accessModifier = ZR_ACCESS_PUBLIC;
    prototype.modifierFlags = isResource
                                      ? ZR_DECLARATION_MODIFIER_RESOURCE
                                      : ZR_DECLARATION_MODIFIER_NONE;
    ZrCore_Array_Init(g_state, &prototype.inherits, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(g_state, &prototype.implements, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(
            g_state,
            &prototype.genericParameters,
            sizeof(SZrTypeGenericParameterInfo),
            1U);
    ZrCore_Array_Init(g_state, &prototype.members, sizeof(SZrTypeMemberInfo), 1U);
    ZrCore_Array_Init(
            g_state,
            &prototype.decorators,
            sizeof(SZrTypeDecoratorInfo),
            1U);
    ZrCore_Array_Push(g_state, &cs->typePrototypes, &prototype);

    memset(&binding, 0, sizeof(binding));
    binding.name = ZrCore_String_CreateFromNative(g_state, aliasText);
    TEST_ASSERT_NOT_NULL(binding.name);
    ZrParser_InferredType_InitFull(
            g_state,
            &binding.type,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            prototype.name);
    ZrCore_Array_Push(g_state, &cs->typeValueAliases, &binding);
}

static SZrAstNode *semantic_display_gc_bridge_inner_argument(
        SZrAstNode *declaration) {
    SZrGenericType *genericType;

    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.typeInfo);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.typeInfo->name);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_GENERIC_TYPE,
            declaration->data.variableDeclaration.typeInfo->name->type);
    genericType =
            &declaration->data.variableDeclaration.typeInfo->name->data.genericType;
    TEST_ASSERT_NOT_NULL(genericType->params);
    TEST_ASSERT_EQUAL_UINT(1U, genericType->params->count);
    TEST_ASSERT_NOT_NULL(genericType->params->nodes[0]);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, genericType->params->nodes[0]->type);
    return genericType->params->nodes[0];
}

static void test_gc_bridge_type_value_aliases_preserve_inner_source_aliases(void) {
    const TZrChar *source =
            "var document: Gc<DocAlias> = null;\n"
            "var boxed: GcBox<ResourceAlias> = null;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "gc_bridge_type_value_alias_display.zr");
    SZrAstNode *ast;
    SZrAstNode *handleDeclaration;
    SZrAstNode *boxDeclaration;
    SZrAstNode *handleArgument;
    SZrAstNode *boxArgument;
    SZrCompilerState cs;
    SZrInferredType inferred;
    SZrInferredType target;
    TZrTypeId targetTypeId;
    TZrTypeId wrapperTypeId;
    SZrString *alias;
    TZrChar buffer[96];

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(2U, ast->data.script.statements->count);
    handleDeclaration = ast->data.script.statements->nodes[0];
    boxDeclaration = ast->data.script.statements->nodes[1];
    handleArgument = semantic_display_gc_bridge_inner_argument(handleDeclaration);
    boxArgument = semantic_display_gc_bridge_inner_argument(boxDeclaration);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    semantic_display_register_gc_bridge_alias_target(
            &cs, "DocAlias", "Document", ZR_FALSE);
    semantic_display_register_gc_bridge_alias_target(
            &cs, "ResourceAlias", "BoxedResource", ZR_TRUE);

    ZrParser_InferredType_Init(g_state, &inferred, ZR_VALUE_TYPE_UNKNOWN);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &cs,
            handleDeclaration->data.variableDeclaration.typeInfo,
            &inferred));
    wrapperTypeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &inferred);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, wrapperTypeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            cs.semanticContext, wrapperTypeId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("Gc<Document>", buffer);
    ZrParser_InferredType_Free(g_state, &inferred);

    ZrParser_InferredType_InitFull(
            g_state,
            &target,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            ZrCore_String_CreateFromNative(g_state, "Document"));
    targetTypeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &target);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, targetTypeId);
    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            cs.semanticContext,
            targetTypeId,
            &handleArgument->data.type.name->location);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_STRING("DocAlias", ZrCore_String_GetNativeString(alias));
    ZrParser_InferredType_Free(g_state, &target);

    ZrParser_InferredType_Init(g_state, &inferred, ZR_VALUE_TYPE_UNKNOWN);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &cs,
            boxDeclaration->data.variableDeclaration.typeInfo,
            &inferred));
    wrapperTypeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &inferred);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, wrapperTypeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            cs.semanticContext, wrapperTypeId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("GcBox<BoxedResource>", buffer);
    ZrParser_InferredType_Free(g_state, &inferred);

    ZrParser_InferredType_InitFull(
            g_state,
            &target,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            ZrCore_String_CreateFromNative(g_state, "BoxedResource"));
    targetTypeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &target);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, targetTypeId);
    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            cs.semanticContext,
            targetTypeId,
            &boxArgument->data.type.name->location);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_STRING("ResourceAlias", ZrCore_String_GetNativeString(alias));
    ZrParser_InferredType_Free(g_state, &target);

    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_reference_readonly_type_value_aliases_preserve_inner_alias(void) {
    const struct {
        const TZrChar *source;
        const TZrChar *expectedDisplay;
    } cases[] = {
            {"var view: readonly Word = null;\n", "readonly Document"},
            {"var writable: ref Word = null;\n", "ref Document"},
            {"var observed: ref readonly Word = null;\n", "ref readonly Document"},
    };
    TZrSize index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        SZrString *sourceName = ZrCore_String_CreateFromNative(
                g_state, "wrapped_type_value_alias_display.zr");
        SZrString *aliasName = ZrCore_String_CreateFromNative(g_state, "Word");
        SZrString *targetName = ZrCore_String_CreateFromNative(g_state, "Document");
        SZrAstNode *ast;
        SZrAstNode *declaration;
        SZrType *typeUse;
        SZrCompilerState cs;
        SZrTypeBinding binding;
        SZrInferredType inferred;
        SZrInferredType target;
        TZrTypeId wrapperTypeId;
        TZrTypeId targetTypeId;
        SZrString *alias;
        TZrChar buffer[64];

        TEST_ASSERT_NOT_NULL(sourceName);
        TEST_ASSERT_NOT_NULL(aliasName);
        TEST_ASSERT_NOT_NULL(targetName);
        ast = ZrParser_Parse(
                g_state,
                cases[index].source,
                strlen(cases[index].source),
                sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_UINT(1U, ast->data.script.statements->count);
        declaration = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(declaration);
        typeUse = declaration->data.variableDeclaration.typeInfo;
        TEST_ASSERT_NOT_NULL(typeUse);
        TEST_ASSERT_NOT_NULL(typeUse->name);

        memset(&cs, 0, sizeof(cs));
        ZrParser_CompilerState_Init(&cs, g_state);
        TEST_ASSERT_NOT_NULL(cs.semanticContext);
        memset(&binding, 0, sizeof(binding));
        binding.name = aliasName;
        ZrParser_InferredType_InitFull(
                g_state,
                &binding.type,
                ZR_VALUE_TYPE_OBJECT,
                ZR_FALSE,
                targetName);
        ZrCore_Array_Push(g_state, &cs.typeValueAliases, &binding);

        ZrParser_InferredType_Init(g_state, &inferred, ZR_VALUE_TYPE_UNKNOWN);
        TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
                &cs, typeUse, &inferred));
        wrapperTypeId = ZrParser_CanonicalType_FromInferred(
                cs.semanticContext, &inferred);
        TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, wrapperTypeId);
        TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
                cs.semanticContext, wrapperTypeId, buffer, sizeof(buffer)));
        TEST_ASSERT_EQUAL_STRING(cases[index].expectedDisplay, buffer);

        ZrParser_InferredType_InitFull(
                g_state,
                &target,
                ZR_VALUE_TYPE_OBJECT,
                ZR_FALSE,
                targetName);
        targetTypeId = ZrParser_CanonicalType_FromInferred(
                cs.semanticContext, &target);
        TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, targetTypeId);
        alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
                cs.semanticContext, targetTypeId, &typeUse->name->location);
        TEST_ASSERT_NOT_NULL(alias);
        TEST_ASSERT_EQUAL_STRING("Word", ZrCore_String_GetNativeString(alias));

        ZrParser_InferredType_Free(g_state, &target);
        ZrParser_InferredType_Free(g_state, &inferred);
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(g_state, ast);
    }
}
