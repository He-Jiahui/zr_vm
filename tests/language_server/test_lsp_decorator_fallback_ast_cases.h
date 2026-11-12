#ifndef ZR_VM_TEST_LSP_DECORATOR_FALLBACK_AST_CASES_H
#define ZR_VM_TEST_LSP_DECORATOR_FALLBACK_AST_CASES_H

/* 归还一次普通 hover 查询的结果容器并清空句柄；内容字符串由 VM GC 管理。 */
static void decorator_fallback_release_hover(SZrState *state, SZrLspHover **hover) {
    if (hover == ZR_NULL || *hover == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &(*hover)->contents);
    ZrCore_Memory_RawFree(state->global, *hover, sizeof(**hover));
    *hover = ZR_NULL;
}

/* 当前 AST 可用时：装饰器 definition 须精确选中受修饰声明的名称，hover 须描述同一装饰器与目标。
 * targetName 在 fixture 中须唯一；失败原因写入 reason，并带上 stage 以区分编辑前后。 */
static TZrBool decorator_fallback_expect_current_target(SZrState *state,
                                                        SZrLspContext *context,
                                                        SZrString *uri,
                                                        const TZrChar *content,
                                                        const TZrChar *stage,
                                                        const TZrChar *decoratorName,
                                                        const TZrChar *targetKind,
                                                        const TZrChar *targetName,
                                                        TZrChar *reason,
                                                        TZrSize reasonSize) {
    TZrChar decoratorText[64];
    TZrChar expectedDecorator[96];
    TZrChar expectedTarget[128];
    SZrLspPosition decoratorPosition;
    SZrLspPosition namePosition;
    SZrArray definitions;
    SZrLspHover *hover = ZR_NULL;
    TZrBool passed = ZR_TRUE;

    snprintf(decoratorText, sizeof(decoratorText), "#%s#", decoratorName);
    snprintf(expectedDecorator, sizeof(expectedDecorator), "Decorator: %s", decoratorName);
    snprintf(expectedTarget, sizeof(expectedTarget), "Target: %s %s", targetKind, targetName);
    if (!lsp_find_position_for_substring(content, decoratorText, 0, 1, &decoratorPosition) ||
        !lsp_find_position_for_substring(content, targetName, 0, 0, &namePosition)) {
        snprintf(reason, reasonSize, "%s: failed to locate %s or %s in the fixture", stage, decoratorText, targetName);
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 2);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, decoratorPosition, &definitions) ||
        !location_array_contains_range(&definitions,
                                       namePosition.line,
                                       namePosition.character,
                                       namePosition.line,
                                       namePosition.character + (TZrInt32)strlen(targetName))) {
        snprintf(reason,
                 reasonSize,
                 "%s: definition on %s should select exactly the %s name %s (count=%zu)",
                 stage,
                 decoratorText,
                 targetKind,
                 targetName,
                 (size_t)definitions.length);
        passed = ZR_FALSE;
    }
    free_local_reference_projection_results(state, &definitions, ZR_NULL);
    if (!passed) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, decoratorPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, expectedDecorator) ||
        !hover_contains_text(hover, expectedTarget)) {
        snprintf(reason,
                 reasonSize,
                 "%s: hover on %s should contain '%s' and '%s'",
                 stage,
                 decoratorText,
                 expectedDecorator,
                 expectedTarget);
        passed = ZR_FALSE;
    }
    decorator_fallback_release_hover(state, &hover);
    return passed;
}

/* 回退 AST 期间：光标位于已被本次编辑改写的装饰器上，任何 definition 位置或提到上一版本名称的
 * hover 都只可能来自旧树。失败时把旧结果写入 reason 供日志定位。 */
static TZrBool decorator_fallback_expect_no_stale_target(SZrState *state,
                                                         SZrLspContext *context,
                                                         SZrString *uri,
                                                         const TZrChar *content,
                                                         const TZrChar *decoratorName,
                                                         const TZrChar *staleDecoratorName,
                                                         const TZrChar *staleTargetName,
                                                         TZrChar *reason,
                                                         TZrSize reasonSize) {
    TZrChar decoratorText[64];
    SZrLspPosition decoratorPosition;
    SZrArray definitions;
    SZrLspHover *hover = ZR_NULL;
    TZrBool passed = ZR_TRUE;

    snprintf(decoratorText, sizeof(decoratorText), "#%s#", decoratorName);
    if (!lsp_find_position_for_substring(content, decoratorText, 0, 1, &decoratorPosition)) {
        snprintf(reason, reasonSize, "Fallback AST: failed to locate %s in the fixture", decoratorText);
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 2);
    (void)ZrLanguageServer_Lsp_GetDefinition(state, context, uri, decoratorPosition, &definitions);
    if (definitions.length != 0) {
        SZrLspLocation **firstLocationPtr = (SZrLspLocation **)ZrCore_Array_Get(&definitions, 0);
        SZrLspLocation *firstLocation = firstLocationPtr != ZR_NULL ? *firstLocationPtr : ZR_NULL;

        snprintf(reason,
                 reasonSize,
                 "Fallback AST: definition on %s returned %zu stale location(s) (first=%d:%d-%d:%d)",
                 decoratorText,
                 (size_t)definitions.length,
                 firstLocation != ZR_NULL ? firstLocation->range.start.line : -1,
                 firstLocation != ZR_NULL ? firstLocation->range.start.character : -1,
                 firstLocation != ZR_NULL ? firstLocation->range.end.line : -1,
                 firstLocation != ZR_NULL ? firstLocation->range.end.character : -1);
        passed = ZR_FALSE;
    }
    free_local_reference_projection_results(state, &definitions, ZR_NULL);
    if (!passed) {
        return ZR_FALSE;
    }

    (void)ZrLanguageServer_Lsp_GetHover(state, context, uri, decoratorPosition, &hover);
    if (hover != ZR_NULL &&
        (hover_contains_text(hover, staleDecoratorName) || hover_contains_text(hover, staleTargetName))) {
        snprintf(reason,
                 reasonSize,
                 "Fallback AST: hover on %s still described the previous version (%s / %s)",
                 decoratorText,
                 staleDecoratorName,
                 staleTargetName);
        passed = ZR_FALSE;
    }
    decorator_fallback_release_hover(state, &hover);
    return passed;
}

/* 语法错误让增量解析保留上一棵 AST 时，装饰器 definition/hover 不得把旧树的目标投影到当前文本；
 * 文本恢复合法后须按新声明继续导航，证明门禁只覆盖回退窗口而不是永久关闭装饰器导航。 */
static void test_lsp_decorator_navigation_ignores_fallback_ast(SZrState *state) {
    const TZrChar *summary = "LSP Decorator Navigation Ignores Fallback AST";
    const TZrChar *uriText = "file:///decorator_fallback_ast.zr";
    const TZrChar *validContent =
        "#singleton#\n"
        "class SingletonClass {\n"
        "    #trace#\n"
        "    pub fn runOnce(): int {\n"
        "        return 1;\n"
        "    }\n"
        "}\n";
    const TZrChar *brokenContent =
        "#transient#\n"
        "class RenamedClass {\n"
        "    #audit#\n"
        "    pub fn stopTwice(): int {\n"
        "        return 1 +;\n"
        "    }\n"
        "}\n";
    const TZrChar *recoveredContent =
        "#transient#\n"
        "class RenamedClass {\n"
        "    #audit#\n"
        "    pub fn stopTwice(): int {\n"
        "        return 1 + 2;\n"
        "    }\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    TZrChar reason[384];

    reason[0] = '\0';
    TEST_START(summary);
    TEST_INFO("Decorator fallback AST",
              "While a syntax error keeps the last-good AST, decorator definition and hover must not publish "
              "the previous version's target; both must resume on the recovered text");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, validContent, strlen(validContent), 1)) {
        snprintf(reason, sizeof(reason), "Failed to open the valid decorator fixture");
        goto cleanup;
    }
    if (!decorator_fallback_expect_current_target(state, context, uri, validContent, "Valid baseline",
                                                  "singleton", "class", "SingletonClass",
                                                  reason, sizeof(reason)) ||
        !decorator_fallback_expect_current_target(state, context, uri, validContent, "Valid baseline",
                                                  "trace", "method", "runOnce",
                                                  reason, sizeof(reason))) {
        goto cleanup;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, brokenContent, strlen(brokenContent), 2)) {
        snprintf(reason, sizeof(reason), "Failed to update the document to the syntax-error version");
        goto cleanup;
    }
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || !fileVersion->usesFallbackAst ||
        fileVersion->parserDiagnostics.length == 0) {
        snprintf(reason, sizeof(reason), "The syntax-error fixture must keep the last-good AST as a fallback");
        goto cleanup;
    }
    if (!decorator_fallback_expect_no_stale_target(state, context, uri, brokenContent,
                                                   "transient", "singleton", "SingletonClass",
                                                   reason, sizeof(reason)) ||
        !decorator_fallback_expect_no_stale_target(state, context, uri, brokenContent,
                                                   "audit", "trace", "runOnce",
                                                   reason, sizeof(reason))) {
        goto cleanup;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, recoveredContent, strlen(recoveredContent), 3)) {
        snprintf(reason, sizeof(reason), "Failed to update the document to the recovered version");
        goto cleanup;
    }
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->usesFallbackAst) {
        snprintf(reason, sizeof(reason), "The recovered text must replace the fallback AST");
        goto cleanup;
    }
    if (!decorator_fallback_expect_current_target(state, context, uri, recoveredContent, "Recovered text",
                                                  "transient", "class", "RenamedClass",
                                                  reason, sizeof(reason)) ||
        !decorator_fallback_expect_current_target(state, context, uri, recoveredContent, "Recovered text",
                                                  "audit", "method", "stopTwice",
                                                  reason, sizeof(reason))) {
        goto cleanup;
    }

cleanup:
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (reason[0] != '\0') {
        TEST_FAIL(timer, summary, reason);
        return;
    }
    TEST_PASS(timer, summary);
}

#endif
