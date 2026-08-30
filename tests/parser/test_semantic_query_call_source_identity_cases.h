#ifndef ZR_VM_TEST_SEMANTIC_QUERY_CALL_SOURCE_IDENTITY_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_CALL_SOURCE_IDENTITY_CASES_H

static SZrSemanticContext *call_source_identity_create_context(
        SZrString *sourceName,
        TZrBool includeExpressionSource,
        TZrBool includeCallTargetSource,
        TZrBool includeReferenceSource,
        SZrFileRange *outPosition) {
    const TZrChar *source = "target()";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticExpressionFact expression;
    SZrSemanticReferenceFact reference;
    TZrTypeId returnTypeId;
    TZrTypeId callableTypeId;
    TZrSymbolId targetId;

    if (context == ZR_NULL || sourceName == ZR_NULL || outPosition == ZR_NULL) {
        ZrParser_SemanticContext_Free(context);
        return ZR_NULL;
    }
    returnTypeId = ZrParser_CanonicalType_InternPrimitive(
            context, ZR_VALUE_TYPE_INT64);
    callableTypeId = ZrParser_CanonicalType_InternFunction(
            context,
            ZR_NULL,
            0U,
            returnTypeId,
            ZR_CANONICAL_RECEIVER_NONE,
            0U);
    targetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableTypeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("target", sourceName, "target", 0U));
    if (returnTypeId == ZR_SEMANTIC_ID_INVALID ||
        callableTypeId == ZR_SEMANTIC_ID_INVALID ||
        targetId == ZR_SEMANTIC_ID_INVALID) {
        ZrParser_SemanticContext_Free(context);
        return ZR_NULL;
    }

    *outPosition = call_source_position(source, sourceName, "target", 0U);
    memset(&expression, 0, sizeof(expression));
    expression.kind = ZR_SEMANTIC_EXPRESSION_FACT_CALL;
    expression.exactness = ZR_SEMANTIC_FACT_EXACT;
    expression.range = *outPosition;
    expression.range.end.offset = strlen(source);
    expression.callTargetRange = *outPosition;
    expression.typeId = returnTypeId;
    expression.hasCallInfo = ZR_TRUE;
    if (!includeExpressionSource) {
        expression.range.source = ZR_NULL;
    }
    if (!includeCallTargetSource) {
        expression.callTargetRange.source = ZR_NULL;
    }
    if (!ZrParser_SemanticFacts_AppendExpression(context, &expression)) {
        ZrParser_SemanticContext_Free(context);
        return ZR_NULL;
    }

    memset(&reference, 0, sizeof(reference));
    reference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    reference.range = expression.callTargetRange;
    reference.typeId = callableTypeId;
    reference.symbolId = targetId;
    reference.declarationRange = call_source_position(
            "target", sourceName, "target", 0U);
    reference.isResolved = ZR_TRUE;
    if (!includeReferenceSource) {
        reference.range.source = ZR_NULL;
    }
    if (!ZrParser_SemanticFacts_AppendReference(context, &reference)) {
        ZrParser_SemanticContext_Free(context);
        return ZR_NULL;
    }
    return context;
}

static void test_call_at_rejects_source_less_expression_for_sourced_request(void) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_expression_source.zr");
    SZrFileRange position;
    SZrParserSemanticCallQuery call;
    SZrSemanticContext *context = call_source_identity_create_context(
            sourceName, ZR_FALSE, ZR_TRUE, ZR_TRUE, &position);

    TEST_ASSERT_NOT_NULL(context);
    memset(&call, 0xA5, sizeof(call));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            context, position, ZR_NULL, &call));
    TEST_ASSERT_NULL(call.expression);
    TEST_ASSERT_NULL(call.reference);
    ZrParser_SemanticContext_Free(context);
}

static void test_call_at_rejects_source_less_reference_for_sourced_call(void) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_reference_source.zr");
    SZrFileRange position;
    SZrParserSemanticCallQuery call;
    SZrSemanticContext *context = call_source_identity_create_context(
            sourceName, ZR_TRUE, ZR_TRUE, ZR_FALSE, &position);

    TEST_ASSERT_NOT_NULL(context);
    memset(&call, 0xA5, sizeof(call));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            context, position, ZR_NULL, &call));
    TEST_ASSERT_NULL(call.expression);
    TEST_ASSERT_NULL(call.reference);
    ZrParser_SemanticContext_Free(context);
}

static void test_call_at_rejects_source_less_target_for_sourced_expression(void) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_target_source.zr");
    SZrFileRange position;
    SZrParserSemanticCallQuery call;
    SZrSemanticContext *context = call_source_identity_create_context(
            sourceName, ZR_TRUE, ZR_FALSE, ZR_FALSE, &position);

    TEST_ASSERT_NOT_NULL(context);
    memset(&call, 0xA5, sizeof(call));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            context, position, ZR_NULL, &call));
    TEST_ASSERT_NULL(call.expression);
    TEST_ASSERT_NULL(call.reference);
    ZrParser_SemanticContext_Free(context);
}

#endif
