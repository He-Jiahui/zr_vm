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

/* 恒假条件阻止循环体执行，但 header 赋值仍会发生；查询出口范围以锁定这一时序。 */
static TZrBool test_local_expression_query_applies_for_constant_false_init_assignment(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (narrowed = 1; 1 == 2; ) {\n"
        "        narrowed = 10;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///local_for_constant_false_condition_assignment_dataflow_numeric_range_fact.zr",
            strlen("file:///local_for_constant_false_condition_assignment_dataflow_numeric_range_fact.zr"));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "+", &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare for constant false condition local query fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: ExpressionAt returned false for for constant false condition range\n");
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
             query.numericFact->minValue == 2 &&
             query.numericFact->maxValue == 2 &&
             query.numericFact->hasUnsignedRange &&
             query.numericFact->minUnsignedValue == 2 &&
             query.numericFact->maxUnsignedValue == 2 &&
             !query.numericFact->mayOverflow;

    if (!passed) {
        printf("FAIL: expected for constant false condition range fact; status=%d expr=%p exprKind=%d exprType=%d "
               "numeric=%p kind=%d target=%d hasRange=%d min=%lld max=%lld hasUnsigned=%d umin=%llu "
               "umax=%llu mayOverflow=%d\n",
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

/* 让 for header 的局部变量落在恒假循环中，核对其范围不泄漏至循环外的同名读取。 */
static TZrBool test_local_expression_query_for_false_var_init_does_not_leak(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (var step: int = 10; false; ) {\n"
        "        narrowed = step;\n"
        "    }\n"
        "    return step + 1;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///local_for_false_condition_var_init_no_leak_numeric_range_fact.zr",
            strlen("file:///local_for_false_condition_var_init_no_leak_numeric_range_fact.zr"));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "+", &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare for false-condition var init no-leak fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: ExpressionAt returned false for false-condition var init no-leak range\n");
        return ZR_FALSE;
    }

    passed = query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->kind == ZR_SEMANTIC_EXPRESSION_FACT_BINARY &&
             query.numericFact == ZR_NULL;

    if (!passed) {
        printf("FAIL: expected false-condition for var init to avoid leaking numeric range; "
               "status=%d expr=%p exprKind=%d numeric=%p hasRange=%d min=%lld max=%lld\n",
               (int)query.status,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->kind : -1,
               (void *)query.numericFact,
               query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
               query.numericFact != ZR_NULL ? (long long)query.numericFact->minValue : 0LL,
               query.numericFact != ZR_NULL ? (long long)query.numericFact->maxValue : 0LL);
    }

    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

/* 复用单次循环体赋值后立即 break 的事实检查；调用者区分常量真与省略条件、初始化及 step，确保 step 不污染出口。 */
static TZrBool expect_local_for_body_assignment_before_break_range_equals(
        SZrState *state,
        const TZrChar *content,
        const TZrChar *uriText,
        const TZrChar *fixtureName,
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
        printf("FAIL: unable to prepare %s fixture\n", fixtureName);
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: ExpressionAt returned false for %s range\n", fixtureName);
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
               fixtureName,
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

/* 复用单次循环体赋值后立即 break 的事实检查；调用者区分常量真与省略条件、初始化及 step，确保 step 不污染出口。 */
static TZrBool expect_local_for_body_assignment_before_break_range(
        SZrState *state,
        const TZrChar *content,
        const TZrChar *uriText,
        const TZrChar *fixtureName) {
    return expect_local_for_body_assignment_before_break_range_equals(
            state,
            content,
            uriText,
            fixtureName,
            11,
            11);
}

/* 用 for (; true; ) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool test_local_expression_query_for_true_condition_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (; true; ) {\n"
        "        narrowed = 10;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_true_condition_body_assignment_before_break_numeric_range_fact.zr",
            "for true condition body assignment before break");
}

/* 用 for (;;) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool test_local_expression_query_for_omitted_condition_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (;;) {\n"
        "        narrowed = 10;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_omitted_condition_body_assignment_before_break_numeric_range_fact.zr",
            "for omitted condition body assignment before break");
}

/* 用 for (; true; narrowed = 20) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_true_condition_step_assignment_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (; true; narrowed = 20) {\n"
        "        narrowed = 10;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_true_condition_step_assignment_body_assignment_before_break_numeric_range_fact.zr",
            "for true condition step assignment body assignment before break");
}

/* 用 for (;; narrowed = 20) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_omitted_condition_step_assignment_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (;; narrowed = 20) {\n"
        "        narrowed = 10;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_omitted_condition_step_assignment_body_assignment_before_break_numeric_range_fact.zr",
            "for omitted condition step assignment body assignment before break");
}

/* 用 for (narrowed = 1; true; ) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_true_condition_assignment_init_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (narrowed = 1; true; ) {\n"
        "        narrowed = 10;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_true_condition_assignment_init_body_assignment_before_break_numeric_range_fact.zr",
            "for true condition assignment init body assignment before break");
}

/* 用 for (narrowed = 1;;) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_omitted_condition_assignment_init_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (narrowed = 1;;) {\n"
        "        narrowed = 10;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_omitted_condition_assignment_init_body_assignment_before_break_numeric_range_fact.zr",
            "for omitted condition assignment init body assignment before break");
}

/* 用 for (var step: int = 10; true; ) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_true_condition_var_init_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (var step: int = 10; true; ) {\n"
        "        narrowed = step;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_true_condition_var_init_body_assignment_before_break_numeric_range_fact.zr",
            "for true condition var init body assignment before break");
}

/* 用 for (var step: int = 10;;) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_omitted_condition_var_init_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (var step: int = 10;;) {\n"
        "        narrowed = step;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_omitted_condition_var_init_body_assignment_before_break_numeric_range_fact.zr",
            "for omitted condition var init body assignment before break");
}

/* 用 for (narrowed = 1; true; narrowed = 20) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_true_condition_assignment_init_step_assignment_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (narrowed = 1; true; narrowed = 20) {\n"
        "        narrowed = 10;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_true_condition_assignment_init_step_assignment_body_assignment_before_break_numeric_range_fact.zr",
            "for true condition assignment init step assignment body assignment before break");
}

/* 用 for (narrowed = 1;; narrowed = 20) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_omitted_condition_assignment_init_step_assignment_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (narrowed = 1;; narrowed = 20) {\n"
        "        narrowed = 10;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_omitted_condition_assignment_init_step_assignment_body_assignment_before_break_numeric_range_fact.zr",
            "for omitted condition assignment init step assignment body assignment before break");
}

/* 用 for (var step: int = 10; true; narrowed = 20) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_true_condition_var_init_step_assignment_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (var step: int = 10; true; narrowed = 20) {\n"
        "        narrowed = step;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_true_condition_var_init_step_assignment_body_assignment_before_break_numeric_range_fact.zr",
            "for true condition var init step assignment body assignment before break");
}

/* 用 for (var step: int = 10;; narrowed = 20) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_omitted_condition_var_init_step_assignment_body_assignment_before_break(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (var step: int = 10;; narrowed = 20) {\n"
        "        narrowed = step;\n"
        "        break;\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_omitted_condition_var_init_step_assignment_body_assignment_before_break_numeric_range_fact.zr",
            "for omitted condition var init step assignment body assignment before break");
}

/* 用 for (; true; narrowed = 20) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_true_condition_step_assignment_nested_if_break_branches(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(flag: bool): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (; true; narrowed = 20) {\n"
        "        if (flag) {\n"
        "            narrowed = 10;\n"
        "            break;\n"
        "        } else {\n"
        "            narrowed = 12;\n"
        "            break;\n"
        "        }\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range_equals(
            state,
            content,
            "file:///local_for_true_condition_step_assignment_nested_if_break_branches_numeric_range_fact.zr",
            "for true condition step assignment nested if break branches",
            11,
            13);
}

/* 用 for (; true; narrowed = 20) 中的 break 路径核对出口事实；循环体已执行而 step 不可达，避免把未执行写入并入区间。 */
static TZrBool
test_local_expression_query_for_true_condition_step_assignment_known_true_if_break_branch(
        SZrState *state) {
    const TZrChar *content =
        "fn calc(): int {\n"
        "    var narrowed: int = 5;\n"
        "    for (; true; narrowed = 20) {\n"
        "        if (true) {\n"
        "            narrowed = 10;\n"
        "            break;\n"
        "        }\n"
        "    }\n"
        "    return narrowed + 1;\n"
        "}\n";

    return expect_local_for_body_assignment_before_break_range(
            state,
            content,
            "file:///local_for_true_condition_step_assignment_known_true_if_break_branch_numeric_range_fact.zr",
            "for true condition step assignment known true if break branch");
}

/* 此可执行文件由 CTest 的 language_server 套件调用，汇总常量循环条件、break 与 header 执行顺序用例；创建 VM state 后调用本文件场景，并在退出前释放全局状态。 */
int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool passed;
    TZrBool noLeakPassed;
    TZrBool trueConditionPassed;
    TZrBool omittedConditionPassed;
    TZrBool trueStepAssignmentPassed;
    TZrBool omittedStepAssignmentPassed;
    TZrBool trueAssignmentInitPassed;
    TZrBool omittedAssignmentInitPassed;
    TZrBool trueVarInitPassed;
    TZrBool omittedVarInitPassed;
    TZrBool trueAssignmentInitStepPassed;
    TZrBool omittedAssignmentInitStepPassed;
    TZrBool trueVarInitStepPassed;
    TZrBool omittedVarInitStepPassed;
    TZrBool trueStepNestedIfBreakBranchesPassed;
    TZrBool trueStepKnownTrueIfBreakBranchPassed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Numeric Loop Constant Condition Semantic Query Tests\n");
    printf("============================================================\n");
    passed = test_local_expression_query_applies_for_constant_false_init_assignment(state);
    printf("%s: LSP Local Expression Query Applies For Constant False Init Assignment Range\n",
           passed ? "PASS" : "FAIL");
    noLeakPassed = test_local_expression_query_for_false_var_init_does_not_leak(state);
    printf("%s: LSP Local Expression Query Keeps False-Condition Var Init Scoped\n",
           noLeakPassed ? "PASS" : "FAIL");
    trueConditionPassed =
            test_local_expression_query_for_true_condition_body_assignment_before_break(state);
    printf("%s: LSP Local Expression Query Joins For True-Condition Body Assignment Before Break\n",
           trueConditionPassed ? "PASS" : "FAIL");
    omittedConditionPassed =
            test_local_expression_query_for_omitted_condition_body_assignment_before_break(state);
    printf("%s: LSP Local Expression Query Joins For Omitted-Condition Body Assignment Before Break\n",
           omittedConditionPassed ? "PASS" : "FAIL");
    trueStepAssignmentPassed =
            test_local_expression_query_for_true_condition_step_assignment_body_assignment_before_break(
                    state);
    printf("%s: LSP Local Expression Query Joins For True-Condition Step Assignment Body Assignment Before Break\n",
           trueStepAssignmentPassed ? "PASS" : "FAIL");
    omittedStepAssignmentPassed =
            test_local_expression_query_for_omitted_condition_step_assignment_body_assignment_before_break(
                    state);
    printf("%s: LSP Local Expression Query Joins For Omitted-Condition Step Assignment Body Assignment Before Break\n",
           omittedStepAssignmentPassed ? "PASS" : "FAIL");
    trueAssignmentInitPassed =
            test_local_expression_query_for_true_condition_assignment_init_body_assignment_before_break(
                    state);
    printf("%s: LSP Local Expression Query Joins For True-Condition Assignment-Init Body Assignment Before Break\n",
           trueAssignmentInitPassed ? "PASS" : "FAIL");
    omittedAssignmentInitPassed =
            test_local_expression_query_for_omitted_condition_assignment_init_body_assignment_before_break(
                    state);
    printf("%s: LSP Local Expression Query Joins For Omitted-Condition Assignment-Init Body Assignment Before Break\n",
           omittedAssignmentInitPassed ? "PASS" : "FAIL");
    trueVarInitPassed =
            test_local_expression_query_for_true_condition_var_init_body_assignment_before_break(
                    state);
    printf("%s: LSP Local Expression Query Joins For True-Condition Var-Init Body Assignment Before Break\n",
           trueVarInitPassed ? "PASS" : "FAIL");
    omittedVarInitPassed =
            test_local_expression_query_for_omitted_condition_var_init_body_assignment_before_break(
                    state);
    printf("%s: LSP Local Expression Query Joins For Omitted-Condition Var-Init Body Assignment Before Break\n",
           omittedVarInitPassed ? "PASS" : "FAIL");
    trueAssignmentInitStepPassed =
            test_local_expression_query_for_true_condition_assignment_init_step_assignment_body_assignment_before_break(
                    state);
    printf("%s: LSP Local Expression Query Joins For True-Condition Assignment-Init Step Assignment Body Assignment Before Break\n",
           trueAssignmentInitStepPassed ? "PASS" : "FAIL");
    omittedAssignmentInitStepPassed =
            test_local_expression_query_for_omitted_condition_assignment_init_step_assignment_body_assignment_before_break(
                    state);
    printf("%s: LSP Local Expression Query Joins For Omitted-Condition Assignment-Init Step Assignment Body Assignment Before Break\n",
           omittedAssignmentInitStepPassed ? "PASS" : "FAIL");
    trueVarInitStepPassed =
            test_local_expression_query_for_true_condition_var_init_step_assignment_body_assignment_before_break(
                    state);
    printf("%s: LSP Local Expression Query Joins For True-Condition Var-Init Step Assignment Body Assignment Before Break\n",
           trueVarInitStepPassed ? "PASS" : "FAIL");
    omittedVarInitStepPassed =
            test_local_expression_query_for_omitted_condition_var_init_step_assignment_body_assignment_before_break(
                    state);
    printf("%s: LSP Local Expression Query Joins For Omitted-Condition Var-Init Step Assignment Body Assignment Before Break\n",
           omittedVarInitStepPassed ? "PASS" : "FAIL");
    trueStepNestedIfBreakBranchesPassed =
            test_local_expression_query_for_true_condition_step_assignment_nested_if_break_branches(
                    state);
    printf("%s: LSP Local Expression Query Joins For True-Condition Step Assignment Nested If Break Branches\n",
           trueStepNestedIfBreakBranchesPassed ? "PASS" : "FAIL");
    trueStepKnownTrueIfBreakBranchPassed =
            test_local_expression_query_for_true_condition_step_assignment_known_true_if_break_branch(
                    state);
    printf("%s: LSP Local Expression Query Joins For True-Condition Step Assignment Known-True If Break Branch\n",
           trueStepKnownTrueIfBreakBranchPassed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return (passed &&
            noLeakPassed &&
            trueConditionPassed &&
            omittedConditionPassed &&
            trueStepAssignmentPassed &&
            omittedStepAssignmentPassed &&
            trueAssignmentInitPassed &&
            omittedAssignmentInitPassed &&
            trueVarInitPassed &&
            omittedVarInitPassed &&
            trueAssignmentInitStepPassed &&
            omittedAssignmentInitStepPassed &&
            trueVarInitStepPassed &&
            omittedVarInitStepPassed &&
            trueStepNestedIfBreakBranchesPassed &&
            trueStepKnownTrueIfBreakBranchPassed)
                   ? 0
                   : 1;
}
