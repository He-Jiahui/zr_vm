#ifndef ZR_VM_TEST_LSP_INLAY_CANONICAL_DECLARATION_CASES_H
#define ZR_VM_TEST_LSP_INLAY_CANONICAL_DECLARATION_CASES_H

/* 暂时断开旧 symbol table，证明 inlay 枚举仍来自 parser canonical 声明。 */
static void test_inlay_hint_enumerates_canonical_declarations_without_symbol_table(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Inlay Hint Enumerates Canonical Declarations Without Symbol Table";
    const TZrChar *uriText = "file:///inlay_canonical_declaration_enumeration.zr";
    const TZrChar *content =
            "fn run(): void {\n"
            "    var inferred = 1;\n"
            "    var explicit: int = 2;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrSymbolTable *savedSymbolTable;
    SZrLspRange range;
    SZrArray hints;
    SZrLspInlayHint **hintPtr;
    const TZrChar *label;
    TZrBool querySucceeded;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare canonical declaration fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        analyzer->symbolTable == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected analyzed semantic snapshot and symbol table");
        return;
    }

    range.start.line = 0;
    range.start.character = 0;
    range.end.line = 4;
    range.end.character = 0;
    ZrCore_Array_Init(state, &hints, sizeof(SZrLspInlayHint *), 4);

    savedSymbolTable = analyzer->symbolTable;
    /* 查询窗口内主动断开旧表；随后恢复以便上下文销毁仍遵守原所有权。 */
    analyzer->symbolTable = ZR_NULL;
    querySucceeded = ZrLanguageServer_Lsp_GetInlayHints(
            state, context, uri, range, &hints);
    analyzer->symbolTable = savedSymbolTable;

    hintPtr = hints.length == 1U
            ? (SZrLspInlayHint **)ZrCore_Array_Get(&hints, 0U)
            : ZR_NULL;
    label = hintPtr != ZR_NULL && *hintPtr != ZR_NULL && (*hintPtr)->label != ZR_NULL
            ? test_string_ptr((*hintPtr)->label)
            : ZR_NULL;
    if (!querySucceeded || hints.length != 1U || label == ZR_NULL ||
        strstr(label, ": int") == ZR_NULL || (*hintPtr)->position.line != 1 ||
        (*hintPtr)->position.character != 16) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected one canonical local hint at 1:16 without LSP symbol enumeration; "
                 "success=%d hintCount=%llu label=%s",
                 querySucceeded,
                 (unsigned long long)hints.length,
                 label != ZR_NULL ? label : "<null>");
        ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

/* 共用两种泛型返回 fixture，验证推断标签在旧 symbol table 缺席时仍可生成。 */
static void check_inlay_hint_generic_inferred_return(
        SZrState *state, TZrSize index, const TZrChar *summary) {
    const TZrChar *uriText = "file:///inlay_generic_inferred_returns.zr";
    const TZrChar *contents[] = {
            "fn identity<T>(value: T) { return value; }\n",
            "class Box<T> { }\n"
            "fn identity<T>(value: Box<Box<T>>) { return value; }\n"
    };
    /* TODO: 此处只比较返回类型标签；若要锁定源参数名绑定，应补同型多参数或改名场景。 */
    const TZrChar *expectedLabels[] = { ": T", ": Box<Box<T>>" };
    SZrTestTimer timer;

    TEST_START(summary);
    {
        SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
        SZrString *uri = ZrCore_String_Create(
                state, (TZrNativeString)uriText, strlen(uriText));
        SZrSemanticAnalyzer *analyzer;
        SZrSymbolTable *savedSymbolTable;
        SZrLspRange range;
        SZrArray hints;
        SZrLspInlayHint **hintPtr;
        const TZrChar *label;
        TZrBool querySucceeded;
        TZrChar reason[512];

        if (context == ZR_NULL || uri == ZR_NULL ||
            !ZrLanguageServer_Lsp_UpdateDocument(
                    state, context, uri, contents[index], strlen(contents[index]), 1)) {
            if (context != ZR_NULL) {
                ZrLanguageServer_LspContext_Free(state, context);
            }
            TEST_FAIL(timer, summary, "Failed to prepare generic return fixture");
            return;
        }
        analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
        if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
            TEST_FAIL(timer, summary, "Expected canonical semantic snapshot");
            return;
        }
        memset(&range, 0, sizeof(range));
        range.end.line = 3;
        ZrCore_Array_Init(state, &hints, sizeof(SZrLspInlayHint *), 4);

        savedSymbolTable = analyzer->symbolTable;
        analyzer->symbolTable = ZR_NULL;
        querySucceeded = ZrLanguageServer_Lsp_GetInlayHints(
                state, context, uri, range, &hints);
        analyzer->symbolTable = savedSymbolTable;

        hintPtr = hints.length == 1U
                ? (SZrLspInlayHint **)ZrCore_Array_Get(&hints, 0U)
                : ZR_NULL;
        label = hintPtr != ZR_NULL && *hintPtr != ZR_NULL && (*hintPtr)->label != ZR_NULL
                ? test_string_ptr((*hintPtr)->label)
                : ZR_NULL;
        if (!querySucceeded || label == ZR_NULL ||
            strcmp(label, expectedLabels[index]) != 0) {
            snprintf(reason, sizeof(reason),
                     "Expected inferred return %s; success=%d hintCount=%llu label=%s",
                     expectedLabels[index], querySucceeded,
                     (unsigned long long)hints.length,
                     label != ZR_NULL ? label : "<null>");
            ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
            ZrLanguageServer_LspContext_Free(state, context);
            TEST_FAIL(timer, summary, reason);
            return;
        }
        ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
        ZrLanguageServer_LspContext_Free(state, context);
    }
    TEST_PASS(timer, summary);
}

/* 用简单泛型返回场景检查 inlay 标签含预期类型参数文本。 */
static void test_inlay_hint_generic_inferred_return_uses_source_parameter_name(
        SZrState *state) {
    check_inlay_hint_generic_inferred_return(state, 0U,
            "LSP Inlay Hint Generic Inferred Return Uses Source Parameter Name");
}

/* 用嵌套泛型返回场景检查 inlay 标签含预期层级类型参数文本。 */
static void test_inlay_hint_nested_generic_inferred_return_uses_source_parameter_name(
        SZrState *state) {
    check_inlay_hint_generic_inferred_return(state, 1U,
            "LSP Inlay Hint Nested Generic Inferred Return Uses Source Parameter Name");
}

#endif
