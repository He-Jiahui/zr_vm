#ifndef ZR_VM_TEST_SEMANTIC_DISPLAY_UNRESOLVED_CASES_H
#define ZR_VM_TEST_SEMANTIC_DISPLAY_UNRESOLVED_CASES_H

static void check_callable_signature_type_resolution(
        TZrBool parameterResolved, TZrBool returnResolved,
        TZrBool mismatchedParameter, const TZrChar *expected) {
    const TZrChar *source =
            "fn redact(value: MissingType): MissingType { return value; }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "unresolved_display.zr");
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrCanonicalParameterContract parameter = {0};
    SZrSemanticReferenceFact fact = {0};
    SZrAstNode *declaration;
    TZrTypeId missingType;
    TZrTypeId callableType;
    TZrSymbolId symbolId;
    SZrString *signature;
    SZrString *preserved = ZrCore_String_CreateFromNative(g_state, "preserved signature");
    SZrSemanticReferenceFact *published;
    TZrSize firstReference;
    TZrChar actual[256] = {0};

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(context);
    declaration = ast->data.script.statements->nodes[0];
    missingType = ZrParser_CanonicalType_FromName(
            context, ZrCore_String_CreateFromNative(g_state, "MissingType"));
    parameter.typeId = missingType;
    parameter.passingForm = ZR_CANONICAL_PASSING_VALUE;
    parameter.escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameter.entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameter.exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameter.acceptsTemporary = ZR_TRUE;
    callableType = ZrParser_CanonicalType_InternFunction(
            context, &parameter, 1U, missingType,
            ZR_CANONICAL_RECEIVER_NONE, ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    symbolId = ZrParser_Semantic_RegisterSymbol(
            context, declaration->data.functionDeclaration.name->name,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, callableType,
            ZR_SEMANTIC_ID_INVALID, declaration, declaration->location);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, symbolId);
    fact.kind = ZR_SEMANTIC_REFERENCE_TYPE;
    fact.typeId = mismatchedParameter
            ? ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64)
            : missingType;
    fact.node = declaration->data.functionDeclaration.params->nodes[0]
                       ->data.parameter.typeInfo->name;
    fact.range = fact.node->location;
    fact.isResolved = parameterResolved;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));
    fact.typeId = missingType;
    fact.node = declaration->data.functionDeclaration.returnType->name;
    fact.range = fact.node->location;
    fact.isResolved = returnResolved;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));

    firstReference = context->referenceFacts.length;
    fact.node = declaration;
    fact.kind = ZR_SEMANTIC_REFERENCE_READ;
    fact.symbolId = symbolId;
    fact.typeId = callableType;
    fact.isResolved = ZR_TRUE;
    fact.signatureDisplay = preserved;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));
    fact.isResolved = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));
    fact.isResolved = ZR_TRUE;
    fact.typeId = missingType;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));
    fact.typeId = callableType;
    fact.symbolId = symbolId + 1U;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));

    signature = ZrParser_SemanticDisplay_CreateCallableSignature(context, symbolId);
    if (signature != ZR_NULL) {
        strncpy(actual, ZrCore_String_GetNativeString(signature), sizeof(actual) - 1U);
    }
    signature = ZrParser_SemanticDisplay_PublishCallableSignature(context, symbolId);
    TEST_ASSERT_NOT_NULL(signature);
    published = (SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, firstReference);
    TEST_ASSERT_EQUAL_PTR(signature, published->signatureDisplay);
    for (TZrSize index = firstReference + 1U; index < context->referenceFacts.length; index++) {
        published = (SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, index);
        TEST_ASSERT_EQUAL_STRING("preserved signature",
                                 ZrCore_String_GetNativeString(published->signatureDisplay));
    }
    ZrParser_SemanticContext_Free(context);
    ZrParser_Ast_Free(g_state, ast);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

static void test_callable_signature_redacts_unresolved_parameter_type(void) {
    check_callable_signature_type_resolution(ZR_FALSE, ZR_TRUE, ZR_FALSE,
            "redact(value: cannot infer exact type): MissingType");
}

static void test_callable_signature_redacts_unresolved_return_type(void) {
    check_callable_signature_type_resolution(ZR_TRUE, ZR_FALSE, ZR_FALSE,
            "redact(value: MissingType): cannot infer exact type");
}

static void test_callable_signature_redacts_both_unresolved_type_uses(void) {
    check_callable_signature_type_resolution(ZR_FALSE, ZR_FALSE, ZR_FALSE,
            "redact(value: cannot infer exact type): cannot infer exact type");
}

static void test_callable_signature_keeps_resolved_type_uses(void) {
    check_callable_signature_type_resolution(ZR_TRUE, ZR_TRUE, ZR_FALSE,
            "redact(value: MissingType): MissingType");
}

static void test_callable_signature_redacts_conflicting_parameter_type_identity(void) {
    check_callable_signature_type_resolution(ZR_TRUE, ZR_TRUE, ZR_TRUE,
            "redact(value: cannot infer exact type): MissingType");
}

#endif
