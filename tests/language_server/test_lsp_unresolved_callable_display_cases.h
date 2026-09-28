#ifndef ZR_VM_TEST_LSP_UNRESOLVED_CALLABLE_DISPLAY_CASES_H
#define ZR_VM_TEST_LSP_UNRESOLVED_CALLABLE_DISPLAY_CASES_H

/* 三个场景共用的缺失类型源码，随后可由同 URI 新版本覆盖。 */
static const char g_unresolved_callable_source[] =
        "fn redact(value: MissingType): MissingType { return value; }\n"
        "fn use(): void { redact(null); }\n";
/* 解析未完成时给补全与 hover 对齐的保守签名，不显示猜测出的类型。 */
static const char g_unresolved_callable_signature[] =
        "redact(value: cannot infer exact type): cannot infer exact type";

/* 补全项对缺失类型必须展示未解析状态，而非污染为任意准确类型。 */
static void test_completion_preserves_unresolved_callable_type_state(void) {
    SZrLspPosition position = type_use_position(g_unresolved_callable_source, "redact(null)");
    TZrBool found = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_state, g_context, g_uri, g_unresolved_callable_source,
            strlen(g_unresolved_callable_source), 2U));
    /* BUG: 此公开 API 产出 SZrLspCompletionItem；宿主 tearDown 把共用数组按
     * SZrCompletionItem 析构，向分配器传错原生块大小。需按结果来源分别归还。 */
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetCompletion(
            g_state, g_context, g_uri, position, &g_completions));
    for (TZrSize index = 0U; index < g_completions.length; index++) {
        SZrCompletionItem **item = (SZrCompletionItem **)ZrCore_Array_Get(&g_completions, index);
        if (item == ZR_NULL || *item == ZR_NULL || (*item)->label == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString((*item)->label), "redact") != 0) {
            continue;
        }
        TEST_ASSERT_NOT_NULL((*item)->detail);
        TEST_ASSERT_EQUAL_STRING(g_unresolved_callable_signature,
                                 ZrCore_String_GetNativeString((*item)->detail));
        found = ZR_TRUE;
    }
    TEST_ASSERT_TRUE(found);
}

/* hover 与补全保持同一未解析签名，不能泄露原始 MissingType 猜测。 */
static void test_hover_preserves_unresolved_callable_type_state(void) {
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_state, g_context, g_uri, g_unresolved_callable_source,
            strlen(g_unresolved_callable_source), 2U));
    assert_closed_hover(g_unresolved_callable_source, "redact(null)",
                        g_unresolved_callable_signature);
    for (TZrSize index = 0U; index < g_hover->contents.length; index++) {
        SZrString **content = (SZrString **)ZrCore_Array_Get(&g_hover->contents, index);
        TEST_ASSERT_NULL(strstr(ZrCore_String_GetNativeString(*content), "MissingType"));
    }
}

/* 同 URI 更新为已声明类型后，展示应来自新快照并恢复准确签名。 */
static void test_callable_display_replaces_unresolved_snapshot_state(void) {
    const char *resolved =
            "class MissingType { }\n"
            "fn redact(value: MissingType): MissingType { return value; }\n"
            "fn use(): void { redact(null); }\n";

    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_state, g_context, g_uri, g_unresolved_callable_source,
            strlen(g_unresolved_callable_source), 2U));
    assert_closed_hover(g_unresolved_callable_source, "redact(null)",
                        g_unresolved_callable_signature);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_state, g_context, g_uri, resolved, strlen(resolved), 3U));
    assert_closed_hover(resolved, "redact(null)", "redact(value: MissingType): MissingType");
}

#endif
