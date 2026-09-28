#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic/lsp_local_semantic_query.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

/* 为本测试进程的 VM GlobalState 提供内存回调；调用者是 VM 内存层，生命周期由 main 释放全局状态结束。 */
static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL &&
            (TZrPtr)pointer >= (TZrPtr)0x1000 &&
            originalSize > 0 &&
            originalSize < 1024 * 1024 * 1024) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }
    /* TODO: 地址与 originalSize 阈值不能证明归属；合法大块释放时可能跳过 free，
     * 扩容时可能回退 malloc 并丢失旧内容。核查 VM 回调的尺寸与指针契约。 */
    if ((TZrPtr)pointer >= (TZrPtr)0x1000 &&
        originalSize > 0 &&
        originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }
    return malloc(newSize);
}

/* 把当前 ASCII fixture 的匹配文本换算成 ExpressionAt 光标；调用方须选中目标表达式，不能直接用于非 ASCII 的 UTF-16 坐标。 */
static TZrBool find_position_for_substring(const TZrChar *content,
                                           const TZrChar *needle,
                                           SZrLspPosition *outPosition) {
    const TZrChar *match;
    TZrInt32 line = 0;
    TZrInt32 character = 0;
    const TZrChar *cursor = content;

    if (content == ZR_NULL || needle == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    if (match == ZR_NULL) {
        return ZR_FALSE;
    }

    while (cursor < match) {
        if (*cursor == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
        cursor++;
    }

    outPosition->line = line;
    outPosition->character = character;
    return ZR_TRUE;
}

/* 为 foreach 基数变体建立临时文档并查询循环出口范围；调用者以集合可空性决定是否保留零次迭代路径。 */
static TZrBool run_foreach_cardinality_case(SZrState *state,
                                            const TZrChar *label,
                                            const TZrChar *uriText,
                                            const TZrChar *content,
                                            TZrInt64 expectedMin,
                                            TZrInt64 expectedMax) {
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "+", &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare %s local query fixture\n", label);
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: ExpressionAt returned false for %s range\n", label);
        return ZR_FALSE;
    }

    passed = query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->kind == ZR_SEMANTIC_EXPRESSION_FACT_BINARY &&
             query.expressionFact->inferredType.baseType == ZR_VALUE_TYPE_INT64 &&
             query.numericFact != ZR_NULL &&
             query.numericFact->kind == ZR_SEMANTIC_NUMERIC_FACT_PROMOTION &&
             query.numericFact->targetType == ZR_VALUE_TYPE_INT64 &&
             query.numericFact->hasRange &&
             query.numericFact->minValue == expectedMin &&
             query.numericFact->maxValue == expectedMax &&
             query.numericFact->hasUnsignedRange &&
             query.numericFact->minUnsignedValue == (TZrUInt64)expectedMin &&
             query.numericFact->maxUnsignedValue == (TZrUInt64)expectedMax &&
             !query.numericFact->mayOverflow;

    if (!passed) {
        printf("FAIL: expected %s range fact; status=%d expr=%p exprKind=%d exprType=%d "
               "numeric=%p kind=%d target=%d hasRange=%d min=%lld max=%lld hasUnsigned=%d "
               "umin=%llu umax=%llu mayOverflow=%d\n",
               label,
               (int)query.status,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->kind : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
               (void *)query.numericFact,
               query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
               query.numericFact != ZR_NULL ? (int)query.numericFact->targetType : -1,
               query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
               query.numericFact != ZR_NULL ? (long long)query.numericFact->minValue : 0LL,
               query.numericFact != ZR_NULL ? (long long)query.numericFact->maxValue : 0LL,
               query.numericFact != ZR_NULL ? (int)query.numericFact->hasUnsignedRange : -1,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->minUnsignedValue : 0ULL,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->maxUnsignedValue : 0ULL,
               query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
    }

    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

/* 以基数未知的集合保留零次迭代，验证出口仍包含循环前初始值。 */
static TZrBool test_local_expression_query_keeps_unknown_foreach_zero_iteration_path(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(items: int[]): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (var item in items) {\n"
        "        narrowed = 10;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_foreach_cardinality_case(
            state,
            "unknown foreach cardinality",
            "file:///local_foreach_unknown_cardinality_numeric_range_fact.zr",
            content,
            6,
            11);
}

/* 以已知非空集合保证至少一次迭代，验证出口不再包含初始值路径。 */
static TZrBool test_local_expression_query_drops_nonempty_foreach_zero_iteration_path(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(items: int[2]): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (var item in items) {\n"
        "        narrowed = 10;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return run_foreach_cardinality_case(
            state,
            "nonempty foreach cardinality",
            "file:///local_foreach_nonempty_cardinality_numeric_range_fact.zr",
            content,
            11,
            11);
}

/* 此可执行文件由 CTest 的 language_server 套件调用，汇总 foreach 可达次数与零次迭代路径用例；创建 VM state 后调用本文件场景，并在退出前释放全局状态。 */
int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool unknownCardinalityPassed;
    TZrBool nonemptyCardinalityPassed;
    TZrBool passed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Numeric Foreach Cardinality Semantic Query Tests\n");
    printf("==========================================================\n");
    unknownCardinalityPassed =
            test_local_expression_query_keeps_unknown_foreach_zero_iteration_path(state);
    printf("%s: LSP Local Expression Query Keeps Unknown Foreach Zero-Iteration Path\n",
           unknownCardinalityPassed ? "PASS" : "FAIL");
    nonemptyCardinalityPassed =
            test_local_expression_query_drops_nonempty_foreach_zero_iteration_path(state);
    printf("%s: LSP Local Expression Query Drops Nonempty Foreach Zero-Iteration Path\n",
           nonemptyCardinalityPassed ? "PASS" : "FAIL");

    passed = unknownCardinalityPassed && nonemptyCardinalityPassed;
    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
