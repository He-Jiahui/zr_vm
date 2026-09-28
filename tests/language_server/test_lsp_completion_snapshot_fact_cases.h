#ifndef ZR_VM_TEST_LSP_COMPLETION_SNAPSHOT_FACT_CASES_H
#define ZR_VM_TEST_LSP_COMPLETION_SNAPSHOT_FACT_CASES_H

/* 从已发布的声明事实取得测试目标，避免通过补全接口准备被测快照。返回节点由 analyzer 持有。 */
static SZrAstNode *completion_snapshot_declaration_for_name(
        SZrSemanticAnalyzer *analyzer,
        const TZrChar *name) {
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0U;
         index < analyzer->semanticContext->referenceFacts.length;
         index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &analyzer->semanticContext->referenceFacts, index);
        const TZrChar *factName;

        if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
            !fact->isResolved || fact->node == ZR_NULL) {
            continue;
        }
        factName = test_string_ptr(fact->name);
        if (factName != ZR_NULL && strcmp(factName, name) == 0) {
            return fact->node;
        }
    }
    return ZR_NULL;
}

/* completion 只消费已提交的 snapshot 事实，不能因补全请求反向补造缺失的声明事实。 */
static void test_completion_does_not_materialize_missing_snapshot_facts(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Completion Does Not Materialize Missing Snapshot Facts";
    const TZrChar *uriText = "file:///completion_snapshot_facts.zr";
    const TZrChar *content =
            "fn calc(): int {\n"
            "    var sum = 1 + 2;\n"
            "    su\n"
            "    return sum;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrAstNode *declaration;
    SZrAstNode *initializer;
    SZrSemanticExpressionFact *expressionFact;
    SZrAstNode *savedFactNode;
    TZrSize factCountBefore;
    TZrSize factCountAfter;
    SZrLspPosition position;
    SZrArray completions;
    SZrLspCompletionItem *sumItem;
    TZrBool requestSucceeded;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "su", 1U, 2, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare completion snapshot fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    declaration = completion_snapshot_declaration_for_name(analyzer, "sum");
    initializer = declaration != ZR_NULL &&
                  declaration->type == ZR_AST_VARIABLE_DECLARATION
            ? declaration->data.variableDeclaration.value
            : ZR_NULL;
    expressionFact = analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL
            ? (SZrSemanticExpressionFact *)ZrParser_SemanticFacts_FindExpressionByNode(
                    analyzer->semanticContext, initializer)
            : ZR_NULL;
    if (expressionFact == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected compiler-published initializer fact");
        return;
    }

    /* 暂时令已存在的初始化表达式事实不可定位，观察补全是否擅自重建事实；请求后立即恢复共享快照。 */
    factCountBefore = analyzer->semanticContext->expressionFacts.length;
    savedFactNode = expressionFact->node;
    expressionFact->node = ZR_NULL;
    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8U);
    requestSucceeded = ZrLanguageServer_Lsp_GetCompletion(
            state, context, uri, position, &completions);
    factCountAfter = analyzer->semanticContext->expressionFacts.length;
    expressionFact->node = savedFactNode;
    sumItem = completion_item_find_by_label(&completions, "sum");

    /* BUG: GetCompletion 的原生候选项由 RawMalloc 分配；下面两条退出路径只释放外层数组，
     * 成功找到 sumItem 时必泄漏候选项。须逐项 RawFree 后再释放容器。 */
    if (!requestSucceeded || sumItem == ZR_NULL || factCountAfter != factCountBefore) {
        snprintf(reason,
                 sizeof(reason),
                 "Completion must consume an immutable snapshot; success=%d item=%p facts=%llu->%llu",
                 requestSucceeded,
                 (void *)sumItem,
                 (unsigned long long)factCountBefore,
                 (unsigned long long)factCountAfter);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif
