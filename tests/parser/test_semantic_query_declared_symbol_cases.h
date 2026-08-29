#ifndef ZR_VM_TEST_SEMANTIC_QUERY_DECLARED_SYMBOL_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_DECLARED_SYMBOL_CASES_H

static const SZrParserSemanticSymbolQuery *declared_symbol_at(
        SZrArray *symbols,
        TZrSize index) {
    return (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(symbols, index);
}

static void test_declared_symbols_projects_exact_snapshot_declarations(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode firstNode;
    SZrAstNode secondNode;
    SZrAstNode unresolvedNode;
    SZrString *firstName = ZrCore_String_CreateFromNative(g_state, "first");
    SZrString *secondName = ZrCore_String_CreateFromNative(g_state, "second");
    SZrString *unresolvedName = ZrCore_String_CreateFromNative(g_state, "unresolved");
    SZrArray declarations;
    const SZrParserSemanticSymbolQuery *first;
    const SZrParserSemanticSymbolQuery *second;

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(firstName);
    TEST_ASSERT_NOT_NULL(secondName);
    TEST_ASSERT_NOT_NULL(unresolvedName);
    symbol_init_node(&firstNode, 32U, 40U);
    symbol_init_node(&secondNode, 8U, 16U);
    symbol_init_node(&unresolvedNode, 48U, 58U);

    TEST_ASSERT_EQUAL_UINT32(
            901U,
            ZrParser_Semantic_RegisterSymbolWithId(
                    context,
                    901U,
                    firstName,
                    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                    71U,
                    ZR_SEMANTIC_ID_INVALID,
                    &firstNode,
                    firstNode.location));
    TEST_ASSERT_EQUAL_UINT32(
            902U,
            ZrParser_Semantic_RegisterSymbolWithId(
                    context,
                    902U,
                    secondName,
                    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                    72U,
                    ZR_SEMANTIC_ID_INVALID,
                    &secondNode,
                    secondNode.location));

    symbol_append_reference(context,
                            &firstNode,
                            ZR_SEMANTIC_REFERENCE_DECLARATION,
                            901U,
                            71U,
                            firstNode.location,
                            firstNode.location,
                            ZR_TRUE,
                            ZR_TRUE,
                            firstName,
                            ZR_NULL);
    symbol_append_reference(context,
                            &secondNode,
                            ZR_SEMANTIC_REFERENCE_DECLARATION,
                            902U,
                            72U,
                            secondNode.location,
                            secondNode.location,
                            ZR_TRUE,
                            ZR_TRUE,
                            secondName,
                            ZR_NULL);
    symbol_append_reference(context,
                            &unresolvedNode,
                            ZR_SEMANTIC_REFERENCE_DECLARATION,
                            ZR_SEMANTIC_ID_INVALID,
                            ZR_SEMANTIC_ID_INVALID,
                            unresolvedNode.location,
                            unresolvedNode.location,
                            ZR_FALSE,
                            ZR_FALSE,
                            unresolvedName,
                            ZR_NULL);

    ZrCore_Array_Construct(&declarations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_DeclaredSymbols(
            context, ZR_NULL, &declarations));
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)declarations.length);
    first = declared_symbol_at(&declarations, 0U);
    second = declared_symbol_at(&declarations, 1U);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_UINT32(902U, first->symbolId);
    TEST_ASSERT_EQUAL_UINT32(72U, first->typeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_VARIABLE, first->kind);
    TEST_ASSERT_EQUAL_PTR(&secondNode, first->declarationNode);
    TEST_ASSERT_EQUAL_UINT32(8U, (TZrUInt32)first->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32(901U, second->symbolId);
    TEST_ASSERT_EQUAL_UINT32(71U, second->typeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_VARIABLE, second->kind);
    TEST_ASSERT_EQUAL_PTR(&firstNode, second->declarationNode);

    ((SZrSemanticReferenceFact *)ZrCore_Array_Get(
             &context->referenceFacts, 1U))->isResolved = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_DeclaredSymbols(
            context, ZR_NULL, &declarations));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)declarations.length);
    first = declared_symbol_at(&declarations, 0U);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_UINT32(901U, first->symbolId);

    ZrCore_Array_Free(g_state, &declarations);
    ZrParser_SemanticContext_Free(context);
}

#endif
