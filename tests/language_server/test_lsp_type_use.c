#include "unity.h"
#include "runtime_support.h"
#include "interface/lsp_interface_internal.h"
#include "semantic/lsp_canonical_completion.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_query.h"

#include <string.h>

/* 默认 Unity fixture 同时含泛型声明和使用点；每个 setUp 都重建该文档。 */
static const char g_source[] =
        "class Item { }\n"
        "class Derived<T, const N: int> { }\n"
        "fn use(): void {\n"
        "    var value: Derived<Item, 2 + 2> = null;\n"
        "    value;\n"
        "}\n";

/* Unity 案例共享单次 setUp 创建的状态；tearDown 必须先释放响应再销毁 VM。 */
static SZrState *g_state;
static SZrLspContext *g_context;
static SZrString *g_uri;
static SZrArray g_completions;
static SZrLspHover *g_hover;

/* 从 ASCII fixture 定位类型使用点；调用方在 UTF-16 范围测试中只提供 ASCII 前缀。 */
static SZrLspPosition type_use_position(const char *source, const char *needle) {
    const char *match = strstr(source, needle);
    SZrLspPosition result = {0};

    TEST_ASSERT_NOT_NULL(match);
    for (const char *cursor = source; cursor < match; cursor++) {
        if (*cursor == '\n') {
            result.line++;
            result.character = 0;
        } else {
            result.character++;
        }
    }
    return result;
}

/* Unity 每案重建文档、上下文和结果容器，使 canonical 类型事实不会跨用例泄漏。 */
void setUp(void) {
    memset(&g_completions, 0, sizeof(g_completions));
    g_hover = ZR_NULL;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_context = ZrLanguageServer_LspContext_New(g_state);
    TEST_ASSERT_NOT_NULL(g_context);
    g_uri = ZrCore_String_CreateFromNative(g_state, "file:///type-use.zr");
    TEST_ASSERT_NOT_NULL(g_uri);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_state, g_context, g_uri, g_source, strlen(g_source), 1U));
}

/* Unity 每案释放悬停、补全和 VM 上下文，保证借用的事实先于状态销毁。 */
void tearDown(void) {
    if (g_hover != ZR_NULL) {
        ZrCore_Array_Free(g_state, &g_hover->contents);
        ZrCore_Memory_RawFree(g_state->global, g_hover, sizeof(*g_hover));
        g_hover = ZR_NULL;
    }
    /* BUG: include 用例把 SZrLspCompletionItem* 放入共用数组；这里按 SZrCompletionItem*
     * 析构，RawFree 尺寸与原分配不符。后续需按实际结果类型分别逐项归还。 */
    for (TZrSize index = 0U; index < g_completions.length; index++) {
        SZrCompletionItem **item = (SZrCompletionItem **)ZrCore_Array_Get(
                &g_completions, index);
        if (item != ZR_NULL && *item != ZR_NULL) {
            ZrLanguageServer_CompletionItem_Free(g_state, *item);
        }
    }
    if (g_completions.isValid) {
        ZrCore_Array_Free(g_state, &g_completions);
    }
    ZrLanguageServer_LspContext_Free(g_state, g_context);
    ZrTests_Runtime_State_Destroy(g_state);
}

/* 类型使用点需同时关联声明身份和已代入泛型参数的闭合类型。 */
static void test_generic_type_use_has_canonical_symbol_and_closed_type(void) {
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_Lsp_FindAnalyzer(
            g_state, g_context, g_uri);
    SZrLspPosition position = type_use_position(g_source, "Derived<Item");
    SZrFilePosition filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            g_context, g_uri, position);
    SZrFileRange range = ZrParser_FileRange_Create(filePosition, filePosition, g_uri);
    SZrParserSemanticSymbolQuery symbol;
    TZrChar text[128];

    TEST_ASSERT_NOT_NULL(analyzer);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
            analyzer->semanticContext, range, ZR_NULL, &symbol));
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_TYPE, symbol.kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_TYPE, symbol.role);
    TEST_ASSERT_EQUAL_STRING("Derived", ZrCore_String_GetNativeString(symbol.displayName));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            analyzer->semanticContext, symbol.typeId, text, sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("Derived<Item, 4>", text);
    TEST_ASSERT_EQUAL_UINT64(filePosition.offset, symbol.referenceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(filePosition.offset + strlen("Derived"), symbol.referenceRange.end.offset);
    TEST_ASSERT_EQUAL_UINT64(strstr(g_source, "Derived<T") - g_source,
                             symbol.declarationRange.start.offset);
}

/* 沿公开悬停接口验证 use-site 类型文本，而非仅检查 parser 内部类型 ID。 */
static void assert_closed_hover(const char *source, const char *needle, const char *expected) {
    SZrLspPosition position = type_use_position(source, needle);
    TZrBool found = ZR_FALSE;

    if (g_hover != ZR_NULL) {
        ZrCore_Array_Free(g_state, &g_hover->contents);
        ZrCore_Memory_RawFree(g_state->global, g_hover, sizeof(*g_hover));
        g_hover = ZR_NULL;
    }
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetHover(
            g_state, g_context, g_uri, position, &g_hover));
    TEST_ASSERT_NOT_NULL(g_hover);
    for (TZrSize index = 0U; index < g_hover->contents.length; index++) {
        SZrString **content = (SZrString **)ZrCore_Array_Get(&g_hover->contents, index);
        if (content != ZR_NULL && *content != ZR_NULL &&
            strstr(ZrCore_String_GetNativeString(*content), expected) != ZR_NULL) {
            found = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "Hover must preserve the exact use-site type");
}

/* 公开悬停必须展示泛型类型使用点的闭合结果。 */
static void test_generic_type_use_hover_preserves_closed_type(void) {
    assert_closed_hover(g_source, "Derived<Item", "Resolved Type: Derived<Item, 4>");
}

/* 可见符号补全只把闭合类型说明附着到正确的符号身份。 */
static void test_canonical_completion_projects_use_type_by_symbol_identity(void) {
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_Lsp_FindAnalyzer(
            g_state, g_context, g_uri);
    SZrLspPosition position = type_use_position(g_source, "Derived<Item");
    SZrFilePosition filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            g_context, g_uri, position);
    SZrFileRange range = ZrParser_FileRange_Create(filePosition, filePosition, g_uri);
    TZrBool found = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(analyzer);
    ZrCore_Array_Init(g_state, &g_completions, sizeof(SZrCompletionItem *), 8U);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspCanonicalCompletion_AppendVisibleSymbols(
            g_state, analyzer->semanticContext, range, &g_completions));
    for (TZrSize index = 0U; index < g_completions.length; index++) {
        SZrCompletionItem **item = (SZrCompletionItem **)ZrCore_Array_Get(&g_completions, index);
        TEST_ASSERT_NOT_NULL(item);
        TEST_ASSERT_NOT_NULL(*item);
        if (strcmp(ZrCore_String_GetNativeString((*item)->label), "Derived") == 0) {
            TEST_ASSERT_NOT_NULL((*item)->detail);
            TEST_ASSERT_NOT_NULL(strstr(ZrCore_String_GetNativeString((*item)->detail),
                                         "Resolved Type: Derived<Item, 4>"));
            found = ZR_TRUE;
        } else if ((*item)->detail != ZR_NULL) {
            TEST_ASSERT_NULL(strstr(ZrCore_String_GetNativeString((*item)->detail),
                                     "Resolved Type: Derived<Item, 4>"));
        }
    }
    TEST_ASSERT_TRUE(found);
}

/* 文档更新后应从新版 AST 重建闭合类型，不能复用旧版代入值。 */
static void test_generic_type_use_reloads_closed_type_with_document(void) {
    const char *source =
            "class Derived<T, const N: int> { }\n"
            "fn use(): void {\n"
            "    var value: Derived<bool, 2 + 3> = null;\n"
            "    value;\n"
            "}\n";
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_state, g_context, g_uri, source, strlen(source), 2U));
    assert_closed_hover(source, "Derived<bool", "Resolved Type: Derived<bool, 5>");
}

/* 空白和 CRLF 变化不应扩大类型标识符的悬停范围。 */
static void test_generic_type_use_whitespace_and_crlf_preserve_exact_range(void) {
    const char *source =
            "class Derived<T, const N: int> { }\r\n"
            "fn use(): void {\r\n"
            "    var value: Derived <bool, 2 + 3> = null;\r\n"
            "    value;\r\n"
            "}\r\n";
    SZrLspPosition position = type_use_position(source, "Derived <");
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_state, g_context, g_uri, source, strlen(source), 2U));
    assert_closed_hover(source, "Derived <", "Resolved Type: Derived<bool, 5>");
    TEST_ASSERT_EQUAL_UINT32(position.line, g_hover->range.start.line);
    TEST_ASSERT_EQUAL_UINT32(position.character, g_hover->range.start.character);
    TEST_ASSERT_EQUAL_UINT32(position.character + strlen("Derived"), g_hover->range.end.character);
}

/* 嵌套泛型两个使用点须保有各自的类型代入结果。 */
static void test_nested_generic_type_uses_keep_independent_closed_types(void) {
    const char *source =
            "class Box<T> { }\n"
            "fn use(): void {\n"
            "    var value: Box <Box<int>> = null;\n"
            "    value;\n"
            "}\n";
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_state, g_context, g_uri, source, strlen(source), 2U));
    assert_closed_hover(source, "Box <", "Resolved Type: Box<Box<int>>");
    assert_closed_hover(source, "Box<int>", "Resolved Type: Box<int>");
}

#include "test_lsp_unresolved_callable_display_cases.h"

/* 由独立 Unity CTest 目标运行类型使用与外部片段用例，每案经 setUp 和 tearDown 隔离。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_generic_type_use_has_canonical_symbol_and_closed_type);
    RUN_TEST(test_generic_type_use_hover_preserves_closed_type);
    RUN_TEST(test_canonical_completion_projects_use_type_by_symbol_identity);
    RUN_TEST(test_generic_type_use_reloads_closed_type_with_document);
    RUN_TEST(test_generic_type_use_whitespace_and_crlf_preserve_exact_range);
    RUN_TEST(test_nested_generic_type_uses_keep_independent_closed_types);
    RUN_TEST(test_completion_preserves_unresolved_callable_type_state);
    RUN_TEST(test_hover_preserves_unresolved_callable_type_state);
    RUN_TEST(test_callable_display_replaces_unresolved_snapshot_state);
    return UNITY_END();
}
