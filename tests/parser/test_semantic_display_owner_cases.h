static void test_semantic_display_formats_all_owner_variants(void) {
    const struct {
        EZrCanonicalOwnerKind kind;
        const TZrChar *expected;
    } cases[] = {
            {ZR_CANONICAL_OWNER_UNIQUE, "Unique<int>"},
            {ZR_CANONICAL_OWNER_SHARED, "Shared<int>"},
            {ZR_CANONICAL_OWNER_WEAK, "Weak<int>"},
            {ZR_CANONICAL_OWNER_ATOMIC_SHARED, "AtomicShared<int>"},
    };
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    TZrTypeId intType;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(context);
    intType = ZrParser_CanonicalType_InternPrimitive(
            context, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, intType);

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        TZrTypeId ownerType = ZrParser_CanonicalType_InternOwner(
                context, intType, cases[index].kind);
        TZrChar buffer[64];

        TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, ownerType);
        TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
                context, ownerType, buffer, sizeof(buffer)));
        TEST_ASSERT_EQUAL_STRING(cases[index].expected, buffer);
    }

    ZrParser_SemanticContext_Free(context);
}
