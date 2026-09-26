#ifndef ZR_TEST_SEMANTIC_DECLARATION_BINDING_CASES_H
#define ZR_TEST_SEMANTIC_DECLARATION_BINDING_CASES_H

static void test_declaration_bindings_share_identity_across_environments(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrTypeEnvironment *prepass = ZrParser_TypeEnvironment_New(g_state);
    SZrTypeEnvironment *formal = ZrParser_TypeEnvironment_New(g_state);
    SZrAstNode declaration;
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "seed");
    SZrInferredType type;
    const SZrTypeBinding *first;
    const SZrTypeBinding *second;

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(prepass);
    TEST_ASSERT_NOT_NULL(formal);
    symbol_init_node(&declaration, 4U, 8U);
    declaration.type = ZR_AST_PARAMETER;
    prepass->semanticContext = context;
    formal->semanticContext = context;
    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state, prepass, name, &type, &declaration, declaration.location));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state, formal, name, &type, &declaration, declaration.location));
    first = ZrParser_TypeEnvironment_FindVariableBinding(prepass, name);
    second = ZrParser_TypeEnvironment_FindVariableBinding(formal, name);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_UINT32(first->symbolId, second->symbolId);
    TEST_ASSERT_EQUAL_UINT32(first->typeId, second->typeId);
    TEST_ASSERT_EQUAL_UINT32(1U, context->symbols.length);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_PARAMETER,
            ZrParser_Semantic_FindSymbolById(context, second->symbolId)->kind);
    ZrParser_InferredType_Free(g_state, &type);
    ZrParser_TypeEnvironment_Free(g_state, prepass);
    ZrParser_TypeEnvironment_Free(g_state, formal);
    ZrParser_SemanticContext_Free(context);
}

static void test_unknown_declaration_remains_queryable_without_object_type(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrTypeEnvironment *env = ZrParser_TypeEnvironment_New(g_state);
    SZrTypeEnvironment *formal = ZrParser_TypeEnvironment_New(g_state);
    SZrAstNode declaration;
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "missing");
    SZrInferredType type;
    SZrArray declarations;
    const SZrTypeBinding *binding;
    TZrSymbolId symbolId;

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(env);
    TEST_ASSERT_NOT_NULL(formal);
    symbol_init_node(&declaration, 4U, 11U);
    declaration.type = ZR_AST_VARIABLE_DECLARATION;
    env->semanticContext = context;
    formal->semanticContext = context;
    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state, env, name, &type, &declaration, declaration.location));
    binding = ZrParser_TypeEnvironment_FindVariableBinding(env, name);
    TEST_ASSERT_NOT_NULL(binding);
    symbolId = binding->symbolId;
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    ZrCore_Array_Construct(&declarations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_DeclaredSymbols(context, ZR_NULL, &declarations));
    TEST_ASSERT_EQUAL_UINT32(1U, declarations.length);
    TEST_ASSERT_EQUAL_UINT32(symbolId,
            ((SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(&declarations, 0U))->symbolId);
    ZrCore_Array_Free(g_state, &declarations);
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_RegisterCanonicalVariable(
            g_state, formal, name, &type, symbolId + 1U,
            ZR_SEMANTIC_ID_INVALID, declaration.location));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterCanonicalVariable(
            g_state, formal, name, &type, symbolId,
            ZR_SEMANTIC_ID_INVALID, declaration.location));
    binding = ZrParser_TypeEnvironment_FindVariableBinding(formal, name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(symbolId, binding->symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, binding->typeId);

    ZrParser_InferredType_Free(g_state, &type);
    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state, env, name, &type, &declaration, declaration.location));
    binding = ZrParser_TypeEnvironment_FindVariableBinding(env, name);
    TEST_ASSERT_EQUAL_UINT32(symbolId, binding->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    TEST_ASSERT_EQUAL_UINT32(1U, context->symbols.length);
    ZrParser_InferredType_Free(g_state, &type);
    ZrParser_TypeEnvironment_Free(g_state, env);
    ZrParser_TypeEnvironment_Free(g_state, formal);
    ZrParser_SemanticContext_Free(context);
}

static void test_anonymous_inference_bindings_do_not_publish_source_queries(void) {
    const char *source = "temporary; temporary = 2;";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "temporary_binding.zr");
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "temporary");
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrCompilerState cs;
    SZrInferredType type;
    SZrParserSemanticSymbolQuery query;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.scriptAst = ast;
    cs.suppressErrorOutput = ZR_TRUE;
    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(g_state, cs.typeEnv, name, &type));
    for (TZrSize index = 0U; index < ast->data.script.statements->count; index++) {
        SZrAstNode *expression = ast->data.script.statements->nodes[index];
        if (expression->type == ZR_AST_EXPRESSION_STATEMENT) {
            expression = expression->data.expressionStatement.expr;
        }
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(&cs, expression, &type));
        TEST_ASSERT_FALSE(ZrParser_SemanticQuery_SymbolAt(
                cs.semanticContext,
                symbol_source_position(source, sourceName, "temporary", index),
                ZR_NULL, &query));
    }
    ZrParser_InferredType_Free(g_state, &type);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_external_binding_reads_and_writes_preserve_opaque_identity(void) {
    const char *source = "paused; paused = 2;";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "external_binding.zr");
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "paused");
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrFileRange declarationRange = symbol_range(400U, 406U);
    SZrCompilerState cs;
    SZrInferredType type;
    SZrParserSemanticSymbolQuery query;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.scriptAst = ast;
    cs.suppressErrorOutput = ZR_TRUE;
    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace(
            g_state, cs.typeEnv, name, &type, 7001U, 7002U, 7003U, declarationRange));
    TEST_ASSERT_NULL(ZrParser_Semantic_FindSymbolById(cs.semanticContext, 7001U));
    for (TZrSize index = 0U; index < ast->data.script.statements->count; index++) {
        const SZrSemanticReferenceFact *reference;
        SZrAstNode *expression = ast->data.script.statements->nodes[index];
        SZrAstNode *referenceNode;
        SZrFileRange position;
        if (expression->type == ZR_AST_EXPRESSION_STATEMENT) {
            expression = expression->data.expressionStatement.expr;
        }
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(&cs, expression, &type));
        referenceNode = index == 0U ? expression : expression->data.assignmentExpression.left;
        position = referenceNode->location;
        position.end = position.start;
        reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                cs.semanticContext, referenceNode,
                index == 0U ? ZR_SEMANTIC_REFERENCE_READ : ZR_SEMANTIC_REFERENCE_WRITE);
        TEST_ASSERT_NOT_NULL(reference);
        TEST_ASSERT_EQUAL_UINT32(7001U, reference->symbolId);
        TEST_ASSERT_EQUAL_UINT32(7002U, reference->typeId);
        TEST_ASSERT_EQUAL_UINT32(7003U, reference->placeId);
        TEST_ASSERT_EQUAL_UINT64(400U, reference->declarationRange.start.offset);
        TEST_ASSERT_EQUAL_UINT64(406U, reference->declarationRange.end.offset);
        TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
                cs.semanticContext, position, ZR_NULL, &query));
        TEST_ASSERT_EQUAL_UINT32(7001U, query.symbolId);
        TEST_ASSERT_EQUAL_UINT32(7002U, query.typeId);
        TEST_ASSERT_NULL(query.declarationNode);
    }
    TEST_ASSERT_NULL(ZrParser_Semantic_FindSymbolById(cs.semanticContext, 7001U));
    ZrParser_InferredType_Free(g_state, &type);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

#endif
