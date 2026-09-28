#ifndef ZR_VM_TEST_LSP_REFERENCE_CALLABLE_CONSUMER_CASES_H
#define ZR_VM_TEST_LSP_REFERENCE_CALLABLE_CONSUMER_CASES_H

#include "zr_vm_parser/semantic_query.h"

/* 签名用例只借读首个参数标签，以对比 parser 格式化事实与 LSP 投影的限定符。 */
static const TZrChar *reference_callable_first_parameter_label(
        SZrLspSignatureHelp *help) {
    SZrLspSignatureInformation **signaturePtr;
    SZrLspParameterInformation **parameterPtr;

    if (help == ZR_NULL || help->signatures.length == 0U) {
        return ZR_NULL;
    }
    signaturePtr = (SZrLspSignatureInformation **)ZrCore_Array_Get(
            &help->signatures, 0U);
    if (signaturePtr == ZR_NULL || *signaturePtr == ZR_NULL ||
        (*signaturePtr)->parameters.length == 0U) {
        return ZR_NULL;
    }
    parameterPtr = (SZrLspParameterInformation **)ZrCore_Array_Get(
            &(*signaturePtr)->parameters, 0U);
    return parameterPtr != ZR_NULL && *parameterPtr != ZR_NULL &&
                   (*parameterPtr)->label != ZR_NULL
               ? test_string_ptr((*parameterPtr)->label)
               : ZR_NULL;
}

/* 模块语义诊断与 LSP 诊断先按代码及信息片段对齐，避免把本地展示规则当成事实源。 */
static TZrBool reference_callable_query_diagnostics_contain(
        const SZrParserSemanticQueryDiagnostics *diagnostics,
        const TZrChar *code,
        const TZrChar *messageFragment) {
    TZrSize index;

    if (diagnostics == ZR_NULL || code == ZR_NULL || messageFragment == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < diagnostics->count; index++) {
        const SZrStructuredDiagnostic *diagnostic = &diagnostics->items[index];
        const TZrChar *actualCode = diagnostic->code != ZR_NULL
                                           ? test_string_ptr(diagnostic->code)
                                           : ZR_NULL;
        const TZrChar *actualMessage = diagnostic->message != ZR_NULL
                                              ? test_string_ptr(diagnostic->message)
                                              : ZR_NULL;
        if (actualCode != ZR_NULL && actualMessage != ZR_NULL &&
            strcmp(actualCode, code) == 0 &&
            strstr(actualMessage, messageFragment) != ZR_NULL) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* ref/readonly/scoped 参数的调用悬停与签名必须复用 CallAt/FormatCall 的规范文本及范围。 */
static void test_lsp_reference_callable_hover_and_signature_use_canonical_contract(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Reference Callable Hover And Signature Use Canonical Contract";
    const TZrChar *uriText = "file:///reference_callable_canonical_consumer.zr";
    const TZrChar *content =
            "fn inspect(value: scoped ref readonly int): int { return 1; }\n"
            "fn use(value: ref readonly int): int { return inspect(ref value); }\n";
    const TZrChar *expectedLabel =
            "inspect(value: scoped ref readonly int): int";
    const TZrChar *expectedParameterLabel = "scoped ref readonly int";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrLspPosition callPosition;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrParserSemanticCallQuery query;
    SZrLspSignatureHelp *help = ZR_NULL;
    SZrLspHover *hover = ZR_NULL;
    SZrLspRange expectedHoverRange;
    const TZrChar *signatureLabel;
    const TZrChar *parameterLabel;
    TZrChar canonicalLabel[256];
    TZrChar reason[768];

    TEST_START(summary);
    TEST_INFO(
            "Canonical reference-call consumers",
            "CallAt and FormatCall must drive identical hover/signature text, "
            "including scoped parameter metadata");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(
                content, "inspect(ref value)", 0U, 8, &callPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare reference callable fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, callPosition);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    memset(&query, 0, sizeof(query));
    memset(canonicalLabel, 0, sizeof(canonicalLabel));
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, fileRange, ZR_NULL, &query) ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext,
                &query,
                canonicalLabel,
                sizeof(canonicalLabel)) ||
        strcmp(canonicalLabel, expectedLabel) != 0) {
        snprintf(
                reason,
                sizeof(reason),
                "Canonical query label mismatch (actual=%s)",
                canonicalLabel[0] != '\0' ? canonicalLabel : "<unavailable>");
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }
    expectedHoverRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context,
            uri,
            query.reference->range);

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
        help == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Canonical signature help was unavailable");
        return;
    }
    signatureLabel = signature_help_first_label(help);
    parameterLabel = reference_callable_first_parameter_label(help);
    if (signatureLabel == ZR_NULL || strcmp(signatureLabel, canonicalLabel) != 0 ||
        parameterLabel == ZR_NULL ||
        strcmp(parameterLabel, expectedParameterLabel) != 0) {
        snprintf(
                reason,
                sizeof(reason),
                "Signature projection diverged (signature=%s, parameter=%s)",
                signatureLabel != ZR_NULL ? signatureLabel : "<null>",
                parameterLabel != ZR_NULL ? parameterLabel : "<null>");
        ZrLanguageServer_LspSignatureHelp_Free(state, help);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(
                state, context, uri, callPosition, &hover) ||
        hover == ZR_NULL || !hover_contains_text(hover, canonicalLabel) ||
        !lsp_range_equals(
                hover->range,
                expectedHoverRange.start.line,
                expectedHoverRange.start.character,
                expectedHoverRange.end.line,
                expectedHoverRange.end.character)) {
        snprintf(
                reason,
                sizeof(reason),
                "Call-site hover mismatch (actual=%d:%d-%d:%d, expected=%d:%d-%d:%d)",
                hover != ZR_NULL ? hover->range.start.line : -1,
                hover != ZR_NULL ? hover->range.start.character : -1,
                hover != ZR_NULL ? hover->range.end.line : -1,
                hover != ZR_NULL ? hover->range.end.character : -1,
                expectedHoverRange.start.line,
                expectedHoverRange.start.character,
                expectedHoverRange.end.line,
                expectedHoverRange.end.character);
        ZrLanguageServer_LspSignatureHelp_Free(state, help);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    /* BUG: GetHover 返回需调用方释放的 RawMalloc 容器；成功路径及上方悬停失败路径
     * 都只释放 context，故取得 hover 后每次运行都会遗漏容器及 contents 缓冲区。 */
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

/* readonly 接收者调用应保留精确方法身份与 const 效果，并驱动签名及悬停的同一目标。 */
static void test_lsp_receiver_call_consumers_use_resolved_canonical_target(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Receiver Call Consumers Use Resolved Canonical Target";
    const TZrChar *uriText = "file:///receiver_call_canonical_consumer.zr";
    const TZrChar *content =
            "class Counter {\n"
            "    pub var value: int;\n"
            "    pub const fn read(): int { return this.value; }\n"
            "}\n"
            "fn use(counter: readonly Counter): int { return counter.read(); }\n";
    const TZrChar *expectedLabel = "const fn read(): int";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrAstNode *classNode;
    SZrAstNode *methodNode;
    SZrLspPosition callPosition;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrParserSemanticCallQuery query;
    SZrLspSignatureHelp *help = ZR_NULL;
    SZrLspHover *hover = ZR_NULL;
    SZrLspRange expectedHoverRange;
    const TZrChar *signatureLabel;
    TZrChar canonicalLabel[256];
    TZrChar reason[768];

    TEST_START(summary);
    TEST_INFO(
            "Resolved receiver-call consumers",
            "Readonly receiver effect and target identity must flow from "
            "CallAt without member-name inference");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(
                content, "counter.read()", 0U, 12, &callPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare receiver call fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    classNode = analyzer != ZR_NULL && analyzer->ast != ZR_NULL &&
                        analyzer->ast->data.script.statements != ZR_NULL &&
                        analyzer->ast->data.script.statements->count > 0U
                    ? analyzer->ast->data.script.statements->nodes[0]
                    : ZR_NULL;
    /* 用声明 AST 仅作测试参照，消费层仍须从 CallAt 的已解析目标取身份与声明范围。 */
    methodNode = classNode != ZR_NULL &&
                         classNode->type == ZR_AST_CLASS_DECLARATION &&
                         classNode->data.classDeclaration.members != ZR_NULL &&
                         classNode->data.classDeclaration.members->count > 1U
                     ? classNode->data.classDeclaration.members->nodes[1]
                     : ZR_NULL;
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, callPosition);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    memset(&query, 0, sizeof(query));
    memset(canonicalLabel, 0, sizeof(canonicalLabel));
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
         methodNode == ZR_NULL || methodNode->type != ZR_AST_CLASS_METHOD ||
         !ZrParser_SemanticQuery_CallAt(
                 analyzer->semanticContext, fileRange, ZR_NULL, &query) ||
         query.reference == ZR_NULL || !query.reference->isResolved ||
         !query.hasResolvedTarget ||
         query.targetSymbolId == ZR_SEMANTIC_ID_INVALID ||
         query.targetSymbolId != query.reference->symbolId ||
         query.targetDeclarationRange.start.offset != methodNode->location.start.offset ||
         query.targetDeclarationRange.end.offset != methodNode->location.end.offset ||
         query.reference->declarationRange.start.offset !=
                 query.targetDeclarationRange.start.offset ||
         query.reference->declarationRange.end.offset !=
                 query.targetDeclarationRange.end.offset ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext,
                &query,
                canonicalLabel,
                sizeof(canonicalLabel)) ||
        strcmp(canonicalLabel, expectedLabel) != 0) {
        snprintf(
                reason,
                sizeof(reason),
                "Resolved receiver query mismatch (resolved=%d, symbol=%u, label=%s)",
                (int)query.hasResolvedTarget,
                (unsigned int)query.targetSymbolId,
                canonicalLabel[0] != '\0' ? canonicalLabel : "<unavailable>");
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }
    expectedHoverRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context,
            uri,
            query.reference->range);

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
         help == ZR_NULL ||
         (signatureLabel = signature_help_first_label(help)) == ZR_NULL ||
         strcmp(signatureLabel, canonicalLabel) != 0) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Receiver signature help diverged from FormatCall");
        return;
    }
    if (!ZrLanguageServer_Lsp_GetHover(
                state, context, uri, callPosition, &hover) ||
        hover == ZR_NULL || !hover_contains_text(hover, canonicalLabel) ||
        !lsp_range_equals(
                hover->range,
                expectedHoverRange.start.line,
                expectedHoverRange.start.character,
                expectedHoverRange.end.line,
                expectedHoverRange.end.character)) {
        snprintf(reason,
                 sizeof(reason),
                 "Receiver call hover diverged from FormatCall (hover=%s)",
                 hover != ZR_NULL && hover_first_text(hover) != ZR_NULL
                         ? hover_first_text(hover)
                         : "<null>");
        ZrLanguageServer_LspSignatureHelp_Free(state, help);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    /* BUG: GetHover 的结果独立于 context 分配；取得 hover 后，成功路径及后续断言失败出口均未释放。 */
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

/* 缺少 ref 实参标记的错误应先存在于模块查询事实，再由 LSP 原样投影为单条诊断。 */
static void test_lsp_reference_call_diagnostic_is_published_from_query_facts(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Reference Call Diagnostic Is Published From Query Facts";
    const TZrChar *uriText = "file:///reference_call_query_diagnostic.zr";
    const TZrChar *content =
            "fn inspect(value: scoped ref readonly int): int { return 1; }\n"
            "fn use(value: ref readonly int): int { return inspect(value); }\n";
    const TZrChar *expectedCode = "compiler_error";
    const TZrChar *expectedMessage =
            "ref parameter requires the 'ref' argument marker";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics queryDiagnostics;
    SZrArray diagnostics;

    TEST_START(summary);
    TEST_INFO(
            "Reference-call semantic diagnostics",
            "The LSP diagnostic must match the module semantic-query fact "
            "instead of a local signature-name rule");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare reference diagnostic fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    ZrParser_SemanticQueryScope_Module(&scope);
    memset(&queryDiagnostics, 0, sizeof(queryDiagnostics));
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
         !ZrParser_SemanticQuery_Diagnostics(
                 analyzer->semanticContext, &scope, &queryDiagnostics) ||
         queryDiagnostics.count != 1U ||
         !reference_callable_query_diagnostics_contain(
                 &queryDiagnostics, expectedCode, expectedMessage)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Canonical query did not publish the call diagnostic");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics) ||
        diagnostics.length != 1U ||
        !diagnostic_array_contains_code(&diagnostics, expectedCode) ||
        !diagnostic_array_contains_message(&diagnostics, expectedMessage)) {
        SZrLspDiagnostic **firstDiagnosticPtr = diagnostics.length > 0U
                                                       ? (SZrLspDiagnostic **)ZrCore_Array_Get(
                                                                 &diagnostics, 0U)
                                                       : ZR_NULL;
        SZrLspDiagnostic *firstDiagnostic = firstDiagnosticPtr != ZR_NULL
                                                    ? *firstDiagnosticPtr
                                                    : ZR_NULL;
        SZrLspDiagnostic **secondDiagnosticPtr = diagnostics.length > 1U
                                                        ? (SZrLspDiagnostic **)ZrCore_Array_Get(
                                                                  &diagnostics, 1U)
                                                        : ZR_NULL;
        SZrLspDiagnostic *secondDiagnostic = secondDiagnosticPtr != ZR_NULL
                                                     ? *secondDiagnosticPtr
                                                     : ZR_NULL;
        TZrChar reason[768];

        snprintf(reason,
                 sizeof(reason),
                 "LSP diagnostic projection mismatch "
                 "(count=%zu, first=%s:%s@%d:%d-%d:%d, "
                 "second=%s:%s@%d:%d-%d:%d)",
                 (size_t)diagnostics.length,
                 firstDiagnostic != ZR_NULL && firstDiagnostic->code != ZR_NULL
                         ? test_string_ptr(firstDiagnostic->code)
                         : "<null>",
                 firstDiagnostic != ZR_NULL && firstDiagnostic->message != ZR_NULL
                         ? test_string_ptr(firstDiagnostic->message)
                         : "<null>",
                 firstDiagnostic != ZR_NULL ? firstDiagnostic->range.start.line : -1,
                 firstDiagnostic != ZR_NULL ? firstDiagnostic->range.start.character : -1,
                 firstDiagnostic != ZR_NULL ? firstDiagnostic->range.end.line : -1,
                 firstDiagnostic != ZR_NULL ? firstDiagnostic->range.end.character : -1,
                 secondDiagnostic != ZR_NULL && secondDiagnostic->code != ZR_NULL
                         ? test_string_ptr(secondDiagnostic->code)
                         : "<null>",
                 secondDiagnostic != ZR_NULL && secondDiagnostic->message != ZR_NULL
                         ? test_string_ptr(secondDiagnostic->message)
                         : "<null>",
                 secondDiagnostic != ZR_NULL ? secondDiagnostic->range.start.line : -1,
                 secondDiagnostic != ZR_NULL ? secondDiagnostic->range.start.character : -1,
                 secondDiagnostic != ZR_NULL ? secondDiagnostic->range.end.line : -1,
                 secondDiagnostic != ZR_NULL ? secondDiagnostic->range.end.character : -1);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    /* BUG: GetDiagnostics 的成功结果含 RawMalloc 的诊断条目；这里只释放数组缓冲区，
     * 上方错误报告出口同样遗漏条目及 relatedInformation 内部缓冲区。 */
    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

/* 直接调用和源码构造器先证实规范签名可用，再撤去调用事实以禁止本地 AST 回退。 */
static void test_lsp_direct_call_signature_fails_closed_without_canonical_call_fact(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Direct Call Signature Fails Closed Without Canonical Call Fact";
    const TZrChar *uriText = "file:///direct_call_signature_fact.zr";
    const TZrChar *content =
            "class Hero {\n"
            "  pub @constructor(seed: int) { }\n"
            "}\n"
            "fn inspect(value: int): int { return value; }\n"
            "fn use(): int { var hero: Hero = new Hero(1); return inspect(1); }\n";
    const TZrChar *expectedLabel = "inspect(value: int): int";
    const TZrChar *expectedConstructorLabel = "@constructor(seed: int): null";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrLspPosition callPosition;
    SZrLspPosition constructorPosition;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrParserSemanticCallQuery query;
    SZrParserSemanticCallQuery constructorQuery;
    SZrCompilerState *detachedCompilerState;
    SZrSymbolTable *detachedSymbolTable;
    SZrSemanticExpressionFact *callFact;
    SZrSemanticExpressionFact *constructorCallFact;
    SZrLspSignatureHelp *help = ZR_NULL;
    const TZrChar *label;
    TZrChar formattedCall[ZR_LSP_TEXT_BUFFER_LENGTH];

    TEST_START(summary);
    TEST_INFO(
            "Canonical direct-call signature consumer",
            "A resolved source call must not recover signature help from a local overload "
            "or callee-name fallback after its canonical call fact is unavailable; "
            "canonical facts remain usable without analyzer compiler or symbol state");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(
                content, "inspect(1)", 0U, 8U, &callPosition) ||
        !lsp_find_position_for_substring(
                content, "new Hero(1)", 0U, 9U, &constructorPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare direct-call fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, callPosition);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    memset(&query, 0, sizeof(query));
    memset(&constructorQuery, 0, sizeof(constructorQuery));
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, fileRange, ZR_NULL, &query) ||
        query.expression == ZR_NULL ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext,
                &query,
                formattedCall,
                sizeof(formattedCall)) ||
        strcmp(formattedCall, expectedLabel) != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Canonical direct-call fact was unavailable");
        return;
    }
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, constructorPosition);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    if (!ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, fileRange, ZR_NULL, &constructorQuery)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Canonical source-constructor CallAt was unavailable");
        return;
    }
    if (constructorQuery.expression == ZR_NULL ||
        !constructorQuery.hasResolvedTarget) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Canonical source-constructor target was unresolved");
        return;
    }
    if (ZrParser_SemanticQuery_DeclarationOf(
                analyzer->semanticContext,
                constructorQuery.targetSymbolId,
                ZR_NULL) == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Canonical source-constructor declaration was unavailable");
        return;
    }
    if (!ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext,
                &constructorQuery,
                formattedCall,
                sizeof(formattedCall)) ||
        strcmp(formattedCall, expectedConstructorLabel) != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Canonical source-constructor label was unavailable");
        return;
    }
    if (analyzer->compilerState == ZR_NULL || analyzer->symbolTable == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Analyzer state was unavailable for detachment");
        return;
    }

    /* 暂时移除旧编译器及符号表通道，正向结果仍须由规范调用事实独立提供。 */
    detachedCompilerState = analyzer->compilerState;
    detachedSymbolTable = analyzer->symbolTable;
    analyzer->compilerState = ZR_NULL;
    analyzer->symbolTable = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
        help == ZR_NULL ||
        (label = signature_help_first_label(help)) == ZR_NULL ||
        strcmp(label, expectedLabel) != 0) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        analyzer->compilerState = detachedCompilerState;
        analyzer->symbolTable = detachedSymbolTable;
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Valid direct-call canonical signature was unavailable");
        return;
    }
    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    help = ZR_NULL;

    analyzer->compilerState = ZR_NULL;
    analyzer->symbolTable = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, constructorPosition, &help) ||
        help == ZR_NULL ||
        (label = signature_help_first_label(help)) == ZR_NULL ||
        strcmp(label, expectedConstructorLabel) != 0) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        analyzer->compilerState = detachedCompilerState;
        analyzer->symbolTable = detachedSymbolTable;
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  summary,
                  "Valid source-constructor canonical signature was unavailable");
        return;
    }
    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    help = ZR_NULL;

    /* 分别撤去构造器和普通函数的 callInfo，确认两类签名在事实缺失时都失败关闭。 */
    constructorCallFact =
            (SZrSemanticExpressionFact *)constructorQuery.expression;
    constructorCallFact->hasCallInfo = ZR_FALSE;
    analyzer->compilerState = detachedCompilerState;
    analyzer->symbolTable = detachedSymbolTable;
    if (ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, constructorPosition, &help) ||
        help != ZR_NULL) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        analyzer->compilerState = detachedCompilerState;
        analyzer->symbolTable = detachedSymbolTable;
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  summary,
                  "Constructor signature help recovered from local AST after fact removal");
        return;
    }

    analyzer->compilerState = ZR_NULL;
    analyzer->symbolTable = ZR_NULL;
    callFact = (SZrSemanticExpressionFact *)query.expression;
    callFact->hasCallInfo = ZR_FALSE;
    if (ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
        help != ZR_NULL) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        analyzer->compilerState = detachedCompilerState;
        analyzer->symbolTable = detachedSymbolTable;
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  summary,
                  "Direct-call signature help recovered from a local overload or callee-name fallback");
        return;
    }

    analyzer->compilerState = detachedCompilerState;
    analyzer->symbolTable = detachedSymbolTable;
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

/* Lambda 值的调用、悬停、签名与定义须共同指向声明的 SymbolId、TypeId 和范围。 */
static void test_lsp_lambda_callable_value_consumers_use_canonical_identity(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Lambda Callable Value Consumers Use Canonical Identity";
    const TZrChar *uriText = "file:///lambda_callable_value_consumer.zr";
    const TZrChar *content =
            "pub var add = fn(left: int, right: int): int => left + right;\n"
            "fn useAdd(): int { return add(20, 22); }\n";
    const TZrChar *expectedLabel = "add(left: int, right: int): int";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrAstNode *variableNode;
    SZrAstNode *lambdaNode;
    SZrLspPosition callPosition;
    SZrLspPosition definitionPosition;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrParserSemanticCallQuery query;
    const SZrSemanticReferenceFact *declaration;
    SZrSemanticExpressionFact *callFact;
    SZrLspSignatureHelp *help = ZR_NULL;
    SZrLspHover *hover = ZR_NULL;
    SZrLspRange expectedCallRange;
    SZrLspRange expectedDeclarationRange;
    SZrArray definitions;
    const TZrChar *label;
    TZrChar canonicalLabel[ZR_LSP_TEXT_BUFFER_LENGTH];

    TEST_START(summary);
    TEST_INFO(
            "Canonical lambda callable-value consumer",
            "Hover, signature help, and definition must consume the lambda SymbolId, "
            "TypeId, declaration range, and FormatCall fact without AST reconstruction");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(
                content, "add(20, 22)", 0U, 4U, &callPosition) ||
        !lsp_find_position_for_substring(
                content, "add(20, 22)", 0U, 1U, &definitionPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare lambda callable-value fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    variableNode = analyzer != ZR_NULL && analyzer->ast != ZR_NULL &&
                           analyzer->ast->data.script.statements != ZR_NULL &&
                           analyzer->ast->data.script.statements->count > 0U
                   ? analyzer->ast->data.script.statements->nodes[0]
                   : ZR_NULL;
    /* 声明节点仅用于交叉核验规范身份，不允许消费入口从 AST 重建目标。 */
    lambdaNode = variableNode != ZR_NULL &&
                         variableNode->type == ZR_AST_VARIABLE_DECLARATION
                 ? variableNode->data.variableDeclaration.value
                 : ZR_NULL;
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, callPosition);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    memset(&query, 0, sizeof(query));
    memset(canonicalLabel, 0, sizeof(canonicalLabel));
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        lambdaNode == ZR_NULL || lambdaNode->type != ZR_AST_LAMBDA_EXPRESSION ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, fileRange, ZR_NULL, &query) ||
        query.expression == ZR_NULL || query.reference == ZR_NULL ||
        !query.reference->isResolved || !query.hasResolvedTarget ||
        query.targetSymbolId == ZR_SEMANTIC_ID_INVALID ||
        query.callableTypeId == ZR_SEMANTIC_ID_INVALID ||
        query.targetDeclarationRange.start.offset != lambdaNode->location.start.offset ||
        query.targetDeclarationRange.end.offset != lambdaNode->location.end.offset ||
        (declaration = ZrParser_SemanticQuery_DeclarationOf(
                 analyzer->semanticContext,
                 query.targetSymbolId,
                 ZR_NULL)) == ZR_NULL ||
        declaration->node != lambdaNode ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext,
                &query,
                canonicalLabel,
                sizeof(canonicalLabel)) ||
        strcmp(canonicalLabel, expectedLabel) != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Canonical lambda call identity was unavailable");
        return;
    }
    expectedCallRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, uri, query.reference->range);
    expectedDeclarationRange =
            ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
                    context, uri, query.targetDeclarationRange);

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
        help == ZR_NULL ||
        (label = signature_help_first_label(help)) == ZR_NULL ||
        strcmp(label, canonicalLabel) != 0 ||
        !ZrLanguageServer_Lsp_GetHover(
                state, context, uri, callPosition, &hover) ||
        hover == ZR_NULL || !hover_contains_text(hover, canonicalLabel) ||
        !lsp_range_equals(
                hover->range,
                expectedCallRange.start.line,
                expectedCallRange.start.character,
                expectedCallRange.end.line,
                expectedCallRange.end.character)) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Lambda hover/signature diverged from FormatCall");
        return;
    }
    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    help = ZR_NULL;

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 1U);
    if (!ZrLanguageServer_Lsp_GetDefinition(
                state, context, uri, definitionPosition, &definitions) ||
        !location_array_contains_range(
                &definitions,
                expectedDeclarationRange.start.line,
                expectedDeclarationRange.start.character,
                expectedDeclarationRange.end.line,
                expectedDeclarationRange.end.character)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Lambda definition did not use the canonical target range");
        return;
    }
    /* BUG: GetDefinition 返回的 location 是逐项 RawMalloc；这里仅释放数组缓冲区，
     * 成功及上方定义断言失败路径都会遗漏结果对象。 */
    ZrCore_Array_Free(state, &definitions);

    /* 有效身份保留，单独撤去 callInfo，验证签名不能借声明或 AST 猜测参数。 */
    callFact = (SZrSemanticExpressionFact *)query.expression;
    callFact->hasCallInfo = ZR_FALSE;
    if (ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
        help != ZR_NULL) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  summary,
                  "Lambda signature help recovered from local AST after fact removal");
        return;
    }

    /* BUG: GetHover 的 RawMalloc 结果在成功及后续失败出口均未释放；context 不拥有该容器。 */
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

/* 别名保存的可调用值一旦失去规范 callInfo，不可从局部变量或同名函数补出签名。 */
static void test_lsp_callable_value_signature_fails_closed_without_canonical_call_fact(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Callable Value Signature Fails Closed Without Canonical Call Fact";
    const TZrChar *uriText = "file:///callable_value_signature_fact.zr";
    const TZrChar *content =
            "fn runBossScenarioImpl(seed: int, prepareAmount: int, battleAmount: int): int {\n"
            "    return seed + prepareAmount + battleAmount;\n"
            "}\n"
            "pub var runBossScenario = runBossScenarioImpl;\n"
            "fn useScenario(): int { return runBossScenario(30, 7, 5); }\n";
    const TZrChar *expectedLabel =
            "runBossScenario(seed: int, prepareAmount: int, battleAmount: int): int";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrLspPosition callPosition;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrParserSemanticCallQuery query;
    SZrSemanticExpressionFact *callFact;
    SZrLspSignatureHelp *help = ZR_NULL;
    const TZrChar *label = ZR_NULL;
    TZrChar formattedCall[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrChar reason[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrBool hasCanonicalQuery;
    TZrBool hasCanonicalLabel;
    TZrBool hasSignatureHelp;

    TEST_START(summary);
    TEST_INFO(
            "Canonical callable-value signature consumer",
            "A source callable value must not recover signature help from local variable, "
            "overload, or callee-name fallback after its canonical call fact is unavailable");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(
                content, "runBossScenario(30, 7, 5)", 0U, 16U, &callPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare callable-value fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, callPosition);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    memset(&query, 0, sizeof(query));
    memset(formattedCall, 0, sizeof(formattedCall));
    hasCanonicalQuery = analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL &&
                        ZrParser_SemanticQuery_CallAt(
                                analyzer->semanticContext, fileRange, ZR_NULL, &query);
    hasCanonicalLabel = hasCanonicalQuery &&
                        ZrParser_SemanticQuery_FormatCall(
                                analyzer->semanticContext,
                                &query,
                                formattedCall,
                                sizeof(formattedCall));
    hasSignatureHelp = hasCanonicalLabel &&
                       ZrLanguageServer_Lsp_GetSignatureHelp(
                               state, context, uri, callPosition, &help);
    label = help != ZR_NULL ? signature_help_first_label(help) : ZR_NULL;
    if (!hasCanonicalQuery ||
        query.expression == ZR_NULL || query.reference == ZR_NULL ||
        !query.reference->isResolved ||
        !hasCanonicalLabel ||
        strcmp(formattedCall, expectedLabel) != 0 ||
        !hasSignatureHelp ||
        help == ZR_NULL ||
        label == ZR_NULL ||
        strcmp(label, expectedLabel) != 0) {
        snprintf(reason,
                 sizeof(reason),
                 "Canonical callable-value signature mismatch (query=%d, resolved=%d, format=%d, call=%s, help=%d, label=%s)",
                 (int)hasCanonicalQuery,
                 (int)(query.reference != ZR_NULL && query.reference->isResolved),
                 (int)hasCanonicalLabel,
                 formattedCall[0] != '\0' ? formattedCall : "<unavailable>",
                 (int)hasSignatureHelp,
                 label != ZR_NULL ? label : "<null>");
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }
    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    help = ZR_NULL;

    /* 正向签名已与 FormatCall 对齐；现在只移除事实，隔离消费层的回退行为。 */
    callFact = (SZrSemanticExpressionFact *)query.expression;
    callFact->hasCallInfo = ZR_FALSE;
    if (ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
        help != ZR_NULL) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  summary,
                  "Callable-value signature help recovered from a local variable or callee-name fallback");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

/* 接收者方法失去规范调用事实时不能借成员名或本地方法声明继续展示签名。 */
static void test_lsp_receiver_call_signature_fails_closed_without_canonical_call_fact(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Receiver Call Signature Fails Closed Without Canonical Call Fact";
    const TZrChar *uriText = "file:///receiver_call_signature_fact.zr";
    const TZrChar *content =
            "class Counter {\n"
            "    pub const fn read(): int { return 1; }\n"
            "}\n"
            "fn use(counter: readonly Counter): int { return counter.read(); }\n";
    const TZrChar *expectedLabel = "const fn read(): int";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrLspPosition callPosition;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrParserSemanticCallQuery query;
    SZrSemanticExpressionFact *callFact;
    SZrLspSignatureHelp *help = ZR_NULL;
    const TZrChar *label;
    TZrChar formattedCall[ZR_LSP_TEXT_BUFFER_LENGTH];

    TEST_START(summary);
    TEST_INFO(
            "Canonical receiver-call signature consumer",
            "A resolved source method must not recover signature help from a local member "
            "or callee-name fallback after its canonical call fact is unavailable");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(
                content, "counter.read()", 0U, 12U, &callPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare receiver-call fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, callPosition);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    memset(&query, 0, sizeof(query));
    memset(formattedCall, 0, sizeof(formattedCall));
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, fileRange, ZR_NULL, &query) ||
        query.expression == ZR_NULL || query.reference == ZR_NULL ||
        !query.reference->isResolved || !query.hasResolvedTarget ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext,
                &query,
                formattedCall,
                sizeof(formattedCall)) ||
        strcmp(formattedCall, expectedLabel) != 0 ||
        !ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
        help == ZR_NULL ||
        (label = signature_help_first_label(help)) == ZR_NULL ||
        strcmp(label, expectedLabel) != 0) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Valid receiver-call canonical signature was unavailable");
        return;
    }
    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    help = ZR_NULL;

    callFact = (SZrSemanticExpressionFact *)query.expression;
    callFact->hasCallInfo = ZR_FALSE;
    if (ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
        help != ZR_NULL) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  summary,
                  "Receiver-call signature help recovered from a local member or callee-name fallback");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

/* 泛型接收者的闭合实参签名只能消费已解析事实，不能在事实缺失时由 AST 再特化。 */
static void test_lsp_generic_receiver_signature_fails_closed_without_canonical_call_fact(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Generic Receiver Signature Fails Closed Without Canonical Call Fact";
    const TZrChar *uriText = "file:///generic_receiver_signature_fact.zr";
    const TZrChar *content =
            "class Matrix<T, const N: int> { }\n"
            "class Box<T> {\n"
            "    fn shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }\n"
            "}\n"
            "fn use(): void {\n"
            "    var box = new Box<int>();\n"
            "    var matrix = new Matrix<int, 2 + 2>();\n"
            "    box.shape(matrix);\n"
            "}\n";
    const TZrChar *expectedLabel =
            "fn shape<const N: int>(value: Matrix<int, 4>): Matrix<int, 4>";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrLspPosition callPosition;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrParserSemanticCallQuery query;
    SZrSemanticExpressionFact *callFact;
    SZrLspSignatureHelp *help = ZR_NULL;
    const TZrChar *label;
    TZrChar formattedCall[ZR_LSP_TEXT_BUFFER_LENGTH];

    TEST_START(summary);
    TEST_INFO(
            "Canonical generic receiver signature consumer",
            "A resolved generic source method must not rebuild a closed signature from "
            "local AST specialization after its canonical call fact is unavailable");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(
                content, "box.shape(matrix)", 0U, 10U, &callPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare generic receiver-call fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, callPosition);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    memset(&query, 0, sizeof(query));
    memset(formattedCall, 0, sizeof(formattedCall));
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, fileRange, ZR_NULL, &query) ||
        query.expression == ZR_NULL || query.reference == ZR_NULL ||
        !query.reference->isResolved || !query.hasResolvedTarget ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext,
                &query,
                formattedCall,
                sizeof(formattedCall)) ||
        strcmp(formattedCall, expectedLabel) != 0 ||
        !ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
        help == ZR_NULL ||
        (label = signature_help_first_label(help)) == ZR_NULL ||
        strcmp(label, expectedLabel) != 0) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Valid generic receiver canonical signature was unavailable");
        return;
    }
    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    help = ZR_NULL;

    callFact = (SZrSemanticExpressionFact *)query.expression;
    callFact->hasCallInfo = ZR_FALSE;
    if (ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, callPosition, &help) ||
        help != ZR_NULL) {
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  summary,
                  "Generic receiver signature recovered from local AST specialization");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif
