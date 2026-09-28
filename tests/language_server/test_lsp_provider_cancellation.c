#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "runtime_support.h"
#include "unity.h"

#include <string.h>

/* 同一取消时机矩阵覆盖符号、引用、rename、调用层级及子类型 provider。 */
typedef enum EQueryKind {
    QUERY_WORKSPACE_SYMBOLS,
    QUERY_DOCUMENT_SYMBOLS,
    QUERY_REFERENCES,
    QUERY_RENAME,
    QUERY_INCOMING_CALLS,
    QUERY_OUTGOING_CALLS,
    QUERY_SUBTYPES,
} EQueryKind;

/* 每类查询至少产出两个结果，才能区分“循环中止”与进入查询前就取消。 */
static const char g_source[] =
        "class CancellationBase {}\n"
        "class CancellationChildA : CancellationBase {}\n"
        "class CancellationChildB : CancellationBase {}\n"
        "fn cancellationTarget(value: int): int { return value; }\n"
        "fn cancellationOther(value: int): int { return value; }\n"
        "fn cancellationCallerA(): int { return cancellationTarget(1) + cancellationOther(2); }\n"
        "fn cancellationCallerB(): int { return cancellationTarget(3); }\n";

/* Unity 用例共享签名一致的查询夹具，但状态、结果数组和取消回调逐例重置。 */
static SZrState *g_state;
static SZrLspContext *g_context;
static SZrString *g_uri;
static SZrString *g_query;
static SZrString *g_newName;
static SZrArray g_result;
static SZrArray g_prepared;
static EQueryKind g_kind;
static TZrBool g_cancelObserved;

/* 生产查询循环通过 context 回调观察首个已写结果，随后应停止追加第二项。 */
static TZrBool cancel_after_first_result(void *userData) {
    const SZrArray *result = (const SZrArray *)userData;
    if (result->length > 0) {
        g_cancelObserved = ZR_TRUE;
    }
    return g_cancelObserved;
}

/* 不同 provider 的结果元素由不同释放契约持有，下一轮普通查询前必须完全清空。 */
static void free_result(void) {
    if (!g_result.isValid) {
        return;
    }
    if (g_kind == QUERY_INCOMING_CALLS || g_kind == QUERY_OUTGOING_CALLS) {
        ZrLanguageServer_Lsp_FreeHierarchyCalls(g_state, &g_result);
    } else if (g_kind == QUERY_SUBTYPES) {
        ZrLanguageServer_Lsp_FreeHierarchyItems(g_state, &g_result);
    } else {
        TZrSize itemSize = g_kind == QUERY_WORKSPACE_SYMBOLS || g_kind == QUERY_DOCUMENT_SYMBOLS
                                  ? sizeof(SZrLspSymbolInformation) : sizeof(SZrLspLocation);
        for (TZrSize index = 0; index < g_result.length; index++) {
            void **slot = (void **)ZrCore_Array_Get(&g_result, index);
            if (slot != ZR_NULL && *slot != ZR_NULL) {
                ZrCore_Memory_RawFree(g_state->global, *slot, itemSize);
            }
        }
        ZrCore_Array_Free(g_state, &g_result);
    }
    memset(&g_result, 0, sizeof(g_result));
}

/* 每个用例建立独立的语义文档和 context，避免上轮取消探针污染下一查询。 */
void setUp(void) {
    g_context = ZR_NULL;
    g_uri = ZR_NULL;
    g_query = ZR_NULL;
    g_newName = ZR_NULL;
    memset(&g_result, 0, sizeof(g_result));
    memset(&g_prepared, 0, sizeof(g_prepared));
    g_cancelObserved = ZR_FALSE;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_context = ZrLanguageServer_LspContext_New(g_state);
    TEST_ASSERT_NOT_NULL(g_context);
    g_uri = ZrCore_String_Create(g_state, "file:///provider-cancellation.zr",
                                 strlen("file:///provider-cancellation.zr"));
    g_query = ZrCore_String_Create(g_state, "", 0);
    g_newName = ZrCore_String_Create(g_state, "renamedTarget", strlen("renamedTarget"));
    TEST_ASSERT_NOT_NULL(g_uri);
    TEST_ASSERT_NOT_NULL(g_query);
    TEST_ASSERT_NOT_NULL(g_newName);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_state, g_context, g_uri, g_source, strlen(g_source), 1));
}

/* 先撤回借用 g_result 的回调，再回收 query/hierarchy 结果和语义上下文。 */
void tearDown(void) {
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(g_context, ZR_NULL, ZR_NULL);
    if (g_state != ZR_NULL) {
        free_result();
        ZrLanguageServer_Lsp_FreeHierarchyItems(g_state, &g_prepared);
        ZrLanguageServer_LspContext_Free(g_state, g_context);
        ZrTests_Runtime_State_Destroy(g_state);
    }
}

/* 将各 provider 投影为相同的成功/取消断言；层级查询使用预备阶段生成的 item。 */
static TZrBool run_query(void) {
    SZrLspPosition target = {3, 3};
    const SZrLspHierarchyItem *item = g_prepared.length > 0
            ? *(SZrLspHierarchyItem **)ZrCore_Array_Get(&g_prepared, 0) : ZR_NULL;
    switch (g_kind) {
        case QUERY_WORKSPACE_SYMBOLS:
            return ZrLanguageServer_Lsp_GetWorkspaceSymbols(g_state, g_context, g_query, &g_result);
        case QUERY_DOCUMENT_SYMBOLS:
            return ZrLanguageServer_Lsp_GetDocumentSymbols(g_state, g_context, g_uri, &g_result);
        case QUERY_REFERENCES:
            return ZrLanguageServer_Lsp_FindReferences(g_state, g_context, g_uri, target,
                                                      ZR_TRUE, &g_result);
        case QUERY_RENAME:
            return ZrLanguageServer_Lsp_Rename(g_state, g_context, g_uri, target, g_newName, &g_result);
        case QUERY_INCOMING_CALLS:
            return ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(g_state, g_context, item,
                                                                     &g_result);
        case QUERY_OUTGOING_CALLS:
            return ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(g_state, g_context, item,
                                                                     &g_result);
        case QUERY_SUBTYPES:
            return ZrLanguageServer_Lsp_GetTypeHierarchySubtypes(g_state, g_context, item, &g_result);
    }
    return ZR_FALSE;
}

/* 基线先证明多结果，再在首结果之后注入取消，最后清回调验证查询可重入。 */
static void expect_mid_query_cancellation(EQueryKind kind) {
    TZrSize fullCount;
    g_kind = kind;
    if (kind == QUERY_INCOMING_CALLS || kind == QUERY_OUTGOING_CALLS) {
        SZrLspPosition position = {kind == QUERY_INCOMING_CALLS ? 3 : 5, 3};
        TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_PrepareCallHierarchy(
                g_state, g_context, g_uri, position, &g_prepared));
        TEST_ASSERT_EQUAL_UINT64(1, g_prepared.length);
    } else if (kind == QUERY_SUBTYPES) {
        SZrLspPosition position = {0, 6};
        TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_PrepareTypeHierarchy(
                g_state, g_context, g_uri, position, &g_prepared));
        TEST_ASSERT_EQUAL_UINT64(1, g_prepared.length);
    }
    TEST_ASSERT_TRUE_MESSAGE(run_query(), "ordinary provider query must succeed");
    fullCount = g_result.length;
    TEST_ASSERT_TRUE_MESSAGE(fullCount > 1, "fixture must produce multiple results before cancellation");
    free_result();

    ZrLanguageServer_LspContext_SetRequestCancellationCheck(
            g_context, cancel_after_first_result, &g_result);
    TEST_ASSERT_FALSE_MESSAGE(run_query(), "provider must report cancellation within its result loop");
    TEST_ASSERT_TRUE_MESSAGE(g_cancelObserved, "callback must observe a produced result");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1, g_result.length, "provider must stop before the second result");
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(g_context, ZR_NULL, ZR_NULL);
    TEST_ASSERT_NULL(g_context->requestCancellationUserData);
    free_result();

    TEST_ASSERT_TRUE_MESSAGE(run_query(), "clearing cancellation must restore the provider query");
    TEST_ASSERT_EQUAL_UINT64(fullCount, g_result.length);
}

/* 工作区符号枚举应在第一个结果后观察请求取消。 */
static void test_workspace_symbols_cancel_inside_loop(void) {
    expect_mid_query_cancellation(QUERY_WORKSPACE_SYMBOLS);
}

/* 文档符号枚举应在第一个结果后观察请求取消。 */
static void test_document_symbols_cancel_inside_loop(void) {
    expect_mid_query_cancellation(QUERY_DOCUMENT_SYMBOLS);
}

/* 引用收集应在第一个结果后观察请求取消。 */
static void test_references_cancel_inside_loop(void) {
    expect_mid_query_cancellation(QUERY_REFERENCES);
}

/* rename 编辑位置收集应在第一个结果后观察请求取消。 */
static void test_rename_cancel_inside_loop(void) {
    expect_mid_query_cancellation(QUERY_RENAME);
}

/* incoming call 结果收集应在第一个结果后观察请求取消。 */
static void test_incoming_calls_cancel_inside_loop(void) {
    expect_mid_query_cancellation(QUERY_INCOMING_CALLS);
}

/* outgoing call 结果收集应在第一个结果后观察请求取消。 */
static void test_outgoing_calls_cancel_inside_loop(void) {
    expect_mid_query_cancellation(QUERY_OUTGOING_CALLS);
}

/* 子类型枚举应在第一个结果后观察请求取消。 */
static void test_subtypes_cancel_inside_loop(void) {
    expect_mid_query_cancellation(QUERY_SUBTYPES);
}

/* CMake 单独注册此 provider 取消矩阵，避免其他测试改变回调时序。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_workspace_symbols_cancel_inside_loop);
    RUN_TEST(test_document_symbols_cancel_inside_loop);
    RUN_TEST(test_references_cancel_inside_loop);
    RUN_TEST(test_rename_cancel_inside_loop);
    RUN_TEST(test_incoming_calls_cancel_inside_loop);
    RUN_TEST(test_outgoing_calls_cancel_inside_loop);
    RUN_TEST(test_subtypes_cancel_inside_loop);
    return UNITY_END();
}
