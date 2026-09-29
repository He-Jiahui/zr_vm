/* 直接用同一规范 int 节点核对四类 owner 的 semantic display 文本，隔离 canonical 格式契约与源码别名投影。 */
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
    /* BUG: 后续 Unity 断言失败会 longjmp 跳过下方 Free；tearDown 只销毁 g_state，独立分配的 context 及其数组会泄漏。 */
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
