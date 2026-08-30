#ifndef ZR_VM_TEST_SEMANTIC_QUERY_CALL_SELECTION_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_CALL_SELECTION_CASES_H

static void test_call_at_prefers_resolved_reference_over_earlier_display_fact(void) {
    const TZrChar *source = "target()";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_selection.zr");
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticExpressionFact expression;
    SZrSemanticReferenceFact reference;
    SZrParserSemanticCallQuery call;
    TZrTypeId returnTypeId;
    TZrTypeId callableTypeId;
    TZrSymbolId targetId;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(context);
    returnTypeId = ZrParser_CanonicalType_InternPrimitive(
            context, ZR_VALUE_TYPE_INT64);
    callableTypeId = ZrParser_CanonicalType_InternFunction(
            context,
            ZR_NULL,
            0U,
            returnTypeId,
            ZR_CANONICAL_RECEIVER_NONE,
            0U);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, callableTypeId);
    targetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableTypeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("target", sourceName, "target", 0U));
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, targetId);

    memset(&expression, 0, sizeof(expression));
    expression.kind = ZR_SEMANTIC_EXPRESSION_FACT_CALL;
    expression.exactness = ZR_SEMANTIC_FACT_EXACT;
    expression.range = call_source_position(source, sourceName, "target", 0U);
    expression.range.end.offset = strlen(source);
    expression.callTargetRange = call_source_position(
            source, sourceName, "target", 0U);
    expression.typeId = returnTypeId;
    expression.hasCallInfo = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(
            context, &expression));

    memset(&reference, 0, sizeof(reference));
    reference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    reference.range = expression.callTargetRange;
    reference.typeId = callableTypeId;
    reference.signatureDisplay = ZrCore_String_CreateFromNative(
            g_state, "target(): int");
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &reference));

    reference.symbolId = targetId;
    reference.declarationRange = call_source_position(
            "target", sourceName, "target", 0U);
    reference.signatureDisplay = ZR_NULL;
    reference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &reference));

    memset(&call, 0, sizeof(call));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            context,
            call_source_position(source, sourceName, "target", 0U),
            ZR_NULL,
            &call));
    TEST_ASSERT_TRUE(call.hasResolvedTarget);
    TEST_ASSERT_EQUAL_UINT(targetId, call.targetSymbolId);
    TEST_ASSERT_EQUAL_PTR(
            ZrCore_Array_Get(&context->referenceFacts, 1U), call.reference);

    ZrParser_SemanticContext_Free(context);
}

#endif
