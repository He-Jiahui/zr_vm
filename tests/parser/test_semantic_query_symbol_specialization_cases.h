#ifndef ZR_VM_TEST_SEMANTIC_QUERY_SYMBOL_SPECIALIZATION_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_SYMBOL_SPECIALIZATION_CASES_H

static void test_symbol_at_preserves_declaration_identity_for_specialized_type(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode declarationNode;
    SZrAstNode referenceNode;
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "map");
    SZrParserSemanticSymbolQuery query;

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(name);
    symbol_init_node(&declarationNode, 8U, 16U);
    declarationNode.type = ZR_AST_CLASS_METHOD;
    symbol_init_node(&referenceNode, 40U, 43U);

    TEST_ASSERT_EQUAL_UINT32(
            903U,
            ZrParser_Semantic_RegisterSymbolWithId(
                    context,
                    903U,
                    name,
                    ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                    71U,
                    ZR_SEMANTIC_ID_INVALID,
                    &declarationNode,
                    declarationNode.location));
    symbol_append_reference(context,
                            &referenceNode,
                            ZR_SEMANTIC_REFERENCE_CALL,
                            903U,
                            72U,
                            declarationNode.location,
                            declarationNode.location,
                            ZR_TRUE,
                            ZR_TRUE,
                            name,
                            ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
            context, referenceNode.location, ZR_NULL, &query));
    TEST_ASSERT_EQUAL_UINT32(903U, query.symbolId);
    TEST_ASSERT_EQUAL_UINT32(72U, query.typeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, query.kind);
    TEST_ASSERT_EQUAL_PTR(&declarationNode, query.declarationNode);

    ZrParser_SemanticContext_Free(context);
}

#endif
