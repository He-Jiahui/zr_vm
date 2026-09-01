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

static void test_call_at_fails_closed_for_conflicting_resolved_references(void) {
    const TZrChar *source = "invoke()";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_conflict.zr");
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticExpressionFact expression;
    SZrSemanticReferenceFact reference;
    SZrParserSemanticCallQuery call;
    TZrTypeId returnTypeId;
    TZrTypeId callableTypeId;
    TZrSymbolId firstTargetId;
    TZrSymbolId secondTargetId;

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
    firstTargetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "first_target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableTypeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("first_target", sourceName, "first_target", 0U));
    secondTargetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "second_target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableTypeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("second_target", sourceName, "second_target", 0U));
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, firstTargetId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, secondTargetId);
    TEST_ASSERT_NOT_EQUAL_UINT(firstTargetId, secondTargetId);

    memset(&expression, 0, sizeof(expression));
    expression.kind = ZR_SEMANTIC_EXPRESSION_FACT_CALL;
    expression.exactness = ZR_SEMANTIC_FACT_EXACT;
    expression.range = call_source_position(source, sourceName, "invoke", 0U);
    expression.range.end.offset = strlen(source);
    expression.callTargetRange = call_source_position(
            source, sourceName, "invoke", 0U);
    expression.typeId = returnTypeId;
    expression.hasCallInfo = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(
            context, &expression));

    memset(&reference, 0, sizeof(reference));
    reference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    reference.range = expression.callTargetRange;
    reference.typeId = callableTypeId;
    reference.symbolId = firstTargetId;
    reference.declarationRange = call_source_position(
            "first_target", sourceName, "first_target", 0U);
    reference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &reference));

    reference.symbolId = secondTargetId;
    reference.declarationRange = call_source_position(
            "second_target", sourceName, "second_target", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &reference));

    memset(&call, 0xA5, sizeof(call));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            context,
            call_source_position(source, sourceName, "invoke", 0U),
            ZR_NULL,
            &call));
    TEST_ASSERT_FALSE(call.hasResolvedTarget);
    TEST_ASSERT_NULL(call.expression);
    TEST_ASSERT_NULL(call.reference);

    ZrParser_SemanticContext_Free(context);
}

static void test_call_at_fails_closed_for_conflicting_equal_call_expressions(void) {
    const TZrChar *source = "invoke()";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_expression_conflict.zr");
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
            ZrCore_String_CreateFromNative(g_state, "invoke"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableTypeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("invoke", sourceName, "invoke", 0U));
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, targetId);

    memset(&expression, 0, sizeof(expression));
    expression.kind = ZR_SEMANTIC_EXPRESSION_FACT_CALL;
    expression.exactness = ZR_SEMANTIC_FACT_EXACT;
    expression.range = call_source_position(source, sourceName, "invoke", 0U);
    expression.range.end.offset = strlen(source);
    expression.callTargetRange = call_source_position(
            source, sourceName, "invoke", 0U);
    expression.typeId = returnTypeId;
    expression.hasCallInfo = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(
            context, &expression));

    memset(&reference, 0, sizeof(reference));
    reference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    reference.range = expression.callTargetRange;
    reference.typeId = callableTypeId;
    reference.symbolId = targetId;
    reference.declarationRange = call_source_position(
            "invoke", sourceName, "invoke", 0U);
    reference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &reference));

    expression.callTargetRange.start.offset++;
    expression.callTargetRange.start.column++;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(
            context, &expression));

    memset(&call, 0xA5, sizeof(call));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            context,
            call_source_position(source, sourceName, "invoke", 0U),
            ZR_NULL,
            &call));
    TEST_ASSERT_NULL(call.expression);
    TEST_ASSERT_NULL(call.reference);

    ZrParser_SemanticContext_Free(context);
}

static void test_call_at_keeps_distinct_nested_reference_ranges_separate(void) {
    const TZrChar *source = "outer(inner)";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_nested_call_selection.zr");
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticExpressionFact expression;
    SZrSemanticReferenceFact reference;
    SZrParserSemanticCallQuery call;
    TZrTypeId returnTypeId;
    TZrTypeId callableTypeId;
    TZrSymbolId outerTargetId;
    TZrSymbolId innerTargetId;
    TZrSymbolId refinedInnerTargetId;

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
    outerTargetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "outer_target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableTypeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("outer_target", sourceName, "outer_target", 0U));
    innerTargetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "inner_target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableTypeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("inner_target", sourceName, "inner_target", 0U));
    refinedInnerTargetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "refined_inner_target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableTypeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position(
                    "refined_inner_target",
                    sourceName,
                    "refined_inner_target",
                    0U));
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, outerTargetId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, innerTargetId);
    TEST_ASSERT_NOT_EQUAL_UINT(
            ZR_SEMANTIC_ID_INVALID, refinedInnerTargetId);

    memset(&expression, 0, sizeof(expression));
    expression.kind = ZR_SEMANTIC_EXPRESSION_FACT_CALL;
    expression.exactness = ZR_SEMANTIC_FACT_EXACT;
    expression.range = call_source_position(source, sourceName, "outer", 0U);
    expression.range.end.offset = strlen(source);
    expression.callTargetRange = expression.range;
    expression.typeId = returnTypeId;
    expression.hasCallInfo = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(
            context, &expression));

    memset(&reference, 0, sizeof(reference));
    reference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    reference.range = call_source_position(source, sourceName, "outer", 0U);
    reference.typeId = callableTypeId;
    reference.symbolId = outerTargetId;
    reference.declarationRange = call_source_position(
            "outer_target", sourceName, "outer_target", 0U);
    reference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &reference));

    reference.range = call_source_position(source, sourceName, "inner", 0U);
    reference.symbolId = innerTargetId;
    reference.declarationRange = call_source_position(
            "inner_target", sourceName, "inner_target", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &reference));

    memset(&call, 0, sizeof(call));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            context,
            call_source_position(source, sourceName, "outer", 0U),
            ZR_NULL,
            &call));
    TEST_ASSERT_TRUE(call.hasResolvedTarget);
    TEST_ASSERT_EQUAL_UINT(outerTargetId, call.targetSymbolId);

    reference.symbolId = refinedInnerTargetId;
    reference.declarationRange = call_source_position(
            "refined_inner_target",
            sourceName,
            "refined_inner_target",
            0U);
    reference.signatureDisplay = ZrCore_String_CreateFromNative(
            g_state, "refined_inner_target(): int");
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &reference));

    memset(&call, 0xA5, sizeof(call));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            context,
            call_source_position(source, sourceName, "outer", 0U),
            ZR_NULL,
            &call));
    TEST_ASSERT_NULL(call.reference);

    ZrParser_SemanticContext_Free(context);
}

static void test_call_at_requires_receiver_type_shape_to_match_member_call(void) {
    const TZrChar *source = "receiver.invoke()";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_receiver_type.zr");
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticExpressionFact expression;
    SZrSemanticReferenceFact reference;
    SZrSemanticExpressionFact *storedExpression;
    SZrSemanticReferenceFact *storedReference;
    SZrParserSemanticCallQuery call;
    TZrTypeId returnTypeId;
    TZrTypeId callableTypeId;
    TZrTypeId receiverTypeId;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(context);
    returnTypeId = ZrParser_CanonicalType_InternPrimitive(
            context, ZR_VALUE_TYPE_INT64);
    callableTypeId = ZrParser_CanonicalType_InternFunction(
            context,
            ZR_NULL,
            0U,
            returnTypeId,
            ZR_CANONICAL_RECEIVER_MUTABLE,
            0U);
    receiverTypeId = ZrParser_CanonicalType_InternNominal(
            context,
            ZrCore_String_CreateFromNative(g_state, "app"),
            ZrCore_String_CreateFromNative(g_state, "Receiver"),
            1U);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, callableTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, receiverTypeId);

    memset(&expression, 0, sizeof(expression));
    expression.kind = ZR_SEMANTIC_EXPRESSION_FACT_CALL;
    expression.exactness = ZR_SEMANTIC_FACT_EXACT;
    expression.range = call_source_position(
            source, sourceName, "receiver", 0U);
    expression.range.end.offset = strlen(source);
    expression.callTargetRange = call_source_position(
            source, sourceName, "invoke", 0U);
    expression.typeId = returnTypeId;
    expression.hasCallInfo = ZR_TRUE;
    expression.isMemberCall = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(
            context, &expression));

    memset(&reference, 0, sizeof(reference));
    reference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    reference.range = expression.callTargetRange;
    reference.typeId = callableTypeId;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &reference));

    memset(&call, 0xA5, sizeof(call));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            context, expression.callTargetRange, ZR_NULL, &call));
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, call.receiverTypeId);

    storedReference = (SZrSemanticReferenceFact *)ZrCore_Array_Get(
            &context->referenceFacts, 0U);
    TEST_ASSERT_NOT_NULL(storedReference);
    storedReference->receiverTypeId = receiverTypeId;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            context, expression.callTargetRange, ZR_NULL, &call));
    TEST_ASSERT_EQUAL_UINT(receiverTypeId, call.receiverTypeId);

    storedExpression = (SZrSemanticExpressionFact *)ZrCore_Array_Get(
            &context->expressionFacts, 0U);
    TEST_ASSERT_NOT_NULL(storedExpression);
    storedExpression->isMemberCall = ZR_FALSE;
    memset(&call, 0xA5, sizeof(call));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallAt(
            context, expression.callTargetRange, ZR_NULL, &call));
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, call.receiverTypeId);

    ZrParser_SemanticContext_Free(context);
}

#endif
