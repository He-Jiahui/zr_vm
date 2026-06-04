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

static TZrBool test_local_expression_query_returns_unsigned_range_fact(SZrState *state) {
    const TZrChar *uriText = "file:///local_unsigned_numeric_range_fact.zr";
    const TZrChar *content =
        "func calc(seed: u64): uint {\n"
        "    return seed;\n"
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
        !find_position_for_substring(content, "seed", 1, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare unsigned local query fixture\n");
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
             query.expressionFact->inferredType.baseType == ZR_VALUE_TYPE_UINT64 &&
             query.numericFact != ZR_NULL &&
             query.numericFact->kind == ZR_SEMANTIC_NUMERIC_FACT_RANGE &&
             query.numericFact->targetType == ZR_VALUE_TYPE_UINT64 &&
             query.numericFact->hasRange &&
             query.numericFact->hasUnsignedRange &&
             query.numericFact->minUnsignedValue == (TZrUInt64)0 &&
             query.numericFact->maxUnsignedValue == (TZrUInt64)UINT64_MAX &&
             !query.numericFact->mayOverflow;

    if (!passed) {
        printf("FAIL: expected unsigned u64 range fact; status=%d expr=%p exprType=%d numeric=%p kind=%d target=%d hasRange=%d hasUnsigned=%d min=%llu max=%llu mayOverflow=%d\n",
               (int)query.status,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->inferredType.baseType : -1,
               (void *)query.numericFact,
               query.numericFact != ZR_NULL ? (int)query.numericFact->kind : -1,
               query.numericFact != ZR_NULL ? (int)query.numericFact->targetType : -1,
               query.numericFact != ZR_NULL ? (int)query.numericFact->hasRange : -1,
               query.numericFact != ZR_NULL ? (int)query.numericFact->hasUnsignedRange : -1,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->minUnsignedValue : 0ULL,
               query.numericFact != ZR_NULL ? (unsigned long long)query.numericFact->maxUnsignedValue : 0ULL,
               query.numericFact != ZR_NULL ? (int)query.numericFact->mayOverflow : -1);
    }

    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

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

    printf("ZR VM LSP Numeric Semantic Query Tests\n");
    printf("======================================\n");
    passed = test_local_expression_query_returns_unsigned_range_fact(state);
    printf("%s: LSP Local Expression Query Returns Unsigned Range Fact\n",
           passed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return passed ? 0 : 1;
}
