#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic/lsp_local_semantic_query.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

/* 为本目标的 VM 全局状态提供测试分配回调，跟随 main 创建和销毁。 */
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
    if ((TZrPtr)pointer >= (TZrPtr)0x1000 &&
        originalSize > 0 &&
        originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }
    return malloc(newSize);
}

/* 在 ASCII 源码片段中选择指定出现次序的查询点，供局部表达式及签名用例定位请求。 */
static TZrBool find_position_for_substring(const TZrChar *content,
                                           const TZrChar *needle,
                                           TZrSize occurrence,
                                           SZrLspPosition *outPosition) {
    const TZrChar *match;
    TZrSize currentOccurrence = 0;
    TZrInt32 line = 0;
    TZrInt32 character = 0;
    const TZrChar *cursor = content;

    if (content == ZR_NULL || needle == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    while (match != ZR_NULL && currentOccurrence < occurrence) {
        match = strstr(match + 1, needle);
        currentOccurrence++;
    }
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

/* 把查询返回的可选 VM 字符串作为借用文本比较，所有权仍留在上下文。 */
static const TZrChar *string_text(SZrString *value) {
    return value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
}

/* 只取首个候选签名的标签，测试调用目标在不完整编辑后是否仍可呈现。 */
static const TZrChar *signature_help_first_label(SZrLspSignatureHelp *help) {
    SZrLspSignatureInformation **signaturePtr;

    if (help == ZR_NULL || help->signatures.length == 0) {
        return ZR_NULL;
    }

    signaturePtr = (SZrLspSignatureInformation **)ZrCore_Array_Get(&help->signatures, 0);
    if (signaturePtr == ZR_NULL ||
        *signaturePtr == ZR_NULL ||
        (*signaturePtr)->label == ZR_NULL) {
        return ZR_NULL;
    }

    return string_text((*signaturePtr)->label);
}

/* 复用首签名标签断言调用名称、参数及返回类型均可见。 */
static TZrBool signature_help_contains_text(SZrLspSignatureHelp *help, const TZrChar *needle) {
    const TZrChar *label = signature_help_first_label(help);
    return label != ZR_NULL && needle != ZR_NULL && strstr(label, needle) != ZR_NULL;
}

/* 校验 parser 快照对普通调用发布目标、实参计数与返回类型事实。 */
static TZrBool test_local_expression_query_returns_call_target_payload(SZrState *state) {
    const TZrChar *uriText = "file:///local_call_expression_payload.zr";
    const TZrChar *content =
        "fn pick(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "fn read(): int {\n"
        "    return pick(42);\n"
        "}\n";
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
        !find_position_for_substring(content, "pick", 1, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare call payload local query fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: ExpressionAt returned false\n");
        return ZR_FALSE;
    }

    passed = query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->inferredType.baseType == ZR_VALUE_TYPE_INT64 &&
             query.expressionFact->hasCallInfo &&
             query.expressionFact->callTargetName != ZR_NULL &&
             strcmp(string_text(query.expressionFact->callTargetName), "pick") == 0 &&
             query.expressionFact->callTargetRange.start.offset <
                     query.expressionFact->callTargetRange.end.offset &&
             query.expressionFact->argumentCount == 1 &&
             !query.expressionFact->hasNamedArguments &&
             !query.expressionFact->isMemberCall;

    if (!passed) {
        printf("FAIL: expected call payload fact; status=%d expr=%p exprType=%d hasCall=%d target=%s targetRange=(%llu,%llu) args=%llu named=%d memberCall=%d\n",
               (int)query.status,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->hasCallInfo : -1,
               query.expressionFact != ZR_NULL ? string_text(query.expressionFact->callTargetName) : "<none>",
               query.expressionFact != ZR_NULL
                   ? (unsigned long long)query.expressionFact->callTargetRange.start.offset
                   : 0ULL,
               query.expressionFact != ZR_NULL
                   ? (unsigned long long)query.expressionFact->callTargetRange.end.offset
                   : 0ULL,
               query.expressionFact != ZR_NULL
                   ? (unsigned long long)query.expressionFact->argumentCount
                   : 0ULL,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->hasNamedArguments : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->isMemberCall : -1);
    }

    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

/* 在新版本尾部语法未闭合时核查已解析调用的 last-good 快照事实仍可查询。 */
static TZrBool test_local_expression_query_preserves_call_payload_after_incomplete_edit(SZrState *state) {
    const TZrChar *uriText = "file:///local_call_payload_incomplete_edit.zr";
    const TZrChar *validContent =
        "fn pick(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "fn read(): int {\n"
        "    return pick(42);\n"
        "}\n"
        "var tail: int = 7;\n";
    const TZrChar *brokenContent =
        "fn pick(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "fn read(): int {\n"
        "    return pick(42);\n"
        "\n"
        "var tail: int = 7;\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, validContent, strlen(validContent), 1) ||
        !find_position_for_substring(validContent, "pick", 1, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare incomplete-edit call payload fixture\n");
        return ZR_FALSE;
    }

    /* 保留前一版本的完整调用，同时让新版本尾部缺闭合以检验 last-good 查询。 */
    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, brokenContent, strlen(brokenContent), 2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: unable to apply incomplete edit to call payload fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: ExpressionAt returned false for incomplete-edit call payload\n");
        return ZR_FALSE;
    }

    passed = query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->hasCallInfo &&
             query.expressionFact->callTargetName != ZR_NULL &&
             strcmp(string_text(query.expressionFact->callTargetName), "pick") == 0 &&
             query.expressionFact->argumentCount == 1 &&
             query.diagnostic == ZR_NULL;

    if (!passed) {
        printf("FAIL: expected last-good call payload after incomplete edit; status=%d diagnostic=%p expr=%p exprType=%d hasCall=%d target=%s args=%llu queryOffset=%llu queryLine=%d queryColumn=%d\n",
               (int)query.status,
               (void *)query.diagnostic,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->hasCallInfo : -1,
               query.expressionFact != ZR_NULL ? string_text(query.expressionFact->callTargetName) : "<none>",
               query.expressionFact != ZR_NULL
                   ? (unsigned long long)query.expressionFact->argumentCount
                   : 0ULL,
               (unsigned long long)query.queryRange.start.offset,
               (int)query.queryRange.start.line,
               (int)query.queryRange.start.column);
    }

    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

/* 通过公开签名帮助验证不完整编辑没有抹去已发布的调用事实。 */
static TZrBool test_signature_help_preserves_call_payload_after_incomplete_edit(SZrState *state) {
    const TZrChar *uriText = "file:///signature_payload_incomplete_edit.zr";
    const TZrChar *validContent =
        "fn pick(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "fn read(): int {\n"
        "    return pick(42);\n"
        "}\n"
        "var tail: int = 7;\n";
    const TZrChar *brokenContent =
        "fn pick(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "fn read(): int {\n"
        "    return pick(42);\n"
        "\n"
        "var tail: int = 7;\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspSignatureHelp *help = ZR_NULL;
    const TZrChar *label;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, validContent, strlen(validContent), 1) ||
        !find_position_for_substring(validContent, "42", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare incomplete-edit signature help fixture\n");
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, brokenContent, strlen(brokenContent), 2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: unable to apply incomplete edit to signature help fixture\n");
        return ZR_FALSE;
    }

    passed = ZrLanguageServer_Lsp_GetSignatureHelp(state, context, uri, position, &help) &&
             help != ZR_NULL &&
             signature_help_contains_text(help, "pick") &&
             signature_help_contains_text(help, "value: int") &&
             signature_help_contains_text(help, ": int");

    if (!passed) {
        label = signature_help_first_label(help);
        printf("FAIL: expected last-good call payload to preserve signature help; help=%p label=%s position=(%d,%d)\n",
               (void *)help,
               label != ZR_NULL ? label : "<null>",
               (int)position.line,
               (int)position.character);
    }

    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

/* 校验成员访问事实保留成员名和范围，且不会被误标为函数调用。 */
static TZrBool test_local_expression_query_returns_member_payload(SZrState *state) {
    const TZrChar *uriText = "file:///local_member_expression_payload.zr";
    const TZrChar *content =
        "var seed = 2;\n"
        "fn read(): int {\n"
        "    return seed.value;\n"
        "}\n";
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
        !find_position_for_substring(content, "value", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare member payload local query fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: ExpressionAt returned false for member payload\n");
        return ZR_FALSE;
    }

    passed = query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->hasMemberInfo &&
             query.expressionFact->memberName != ZR_NULL &&
             strcmp(string_text(query.expressionFact->memberName), "value") == 0 &&
             query.expressionFact->memberRange.start.offset <
                     query.expressionFact->memberRange.end.offset &&
             !query.expressionFact->memberIsComputed &&
             !query.expressionFact->hasCallInfo;

    if (!passed) {
        printf("FAIL: expected member payload fact; status=%d expr=%p exprType=%d hasMember=%d member=%s memberRange=(%llu,%llu) computed=%d hasCall=%d\n",
               (int)query.status,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->hasMemberInfo : -1,
               query.expressionFact != ZR_NULL ? string_text(query.expressionFact->memberName) : "<none>",
               query.expressionFact != ZR_NULL
                   ? (unsigned long long)query.expressionFact->memberRange.start.offset
                   : 0ULL,
               query.expressionFact != ZR_NULL
                   ? (unsigned long long)query.expressionFact->memberRange.end.offset
                   : 0ULL,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->memberIsComputed : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->hasCallInfo : -1);
    }

    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

/* 建立 VM 测试状态并按依赖顺序运行调用、编辑恢复、签名及成员用例；前案失败时跳过后案。 */
int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool passed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Call/Member Semantic Query Tests\n");
    printf("==========================================\n");
    passed = test_local_expression_query_returns_call_target_payload(state);
    printf("%s: LSP Local Expression Query Returns Call Target Payload\n",
           passed ? "PASS" : "FAIL");
    if (passed) {
        passed = test_local_expression_query_preserves_call_payload_after_incomplete_edit(state);
        printf("%s: LSP Local Expression Query Preserves Call Payload After Incomplete Edit\n",
               passed ? "PASS" : "FAIL");
    }
    if (passed) {
        passed = test_signature_help_preserves_call_payload_after_incomplete_edit(state);
        printf("%s: LSP Signature Help Preserves Call Payload After Incomplete Edit\n",
               passed ? "PASS" : "FAIL");
    }
    if (passed) {
        passed = test_local_expression_query_returns_member_payload(state);
        printf("%s: LSP Local Expression Query Returns Member Payload\n",
               passed ? "PASS" : "FAIL");
    }

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
