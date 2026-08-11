#ifndef ZR_VM_TEST_LSP_REFERENCE_CALLABLE_CONSUMER_CASES_H
#define ZR_VM_TEST_LSP_REFERENCE_CALLABLE_CONSUMER_CASES_H

#include "zr_vm_parser/semantic_query.h"

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
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

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
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

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

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_direct_call_signature_fails_closed_without_canonical_call_fact(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Direct Call Signature Fails Closed Without Canonical Call Fact";
    const TZrChar *uriText = "file:///direct_call_signature_fact.zr";
    const TZrChar *content =
            "fn inspect(value: int): int { return value; }\n"
            "fn use(): int { return inspect(1); }\n";
    const TZrChar *expectedLabel = "inspect(value: int): int";
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
            "Canonical direct-call signature consumer",
            "A resolved source call must not recover signature help from a local overload "
            "or callee-name fallback after its canonical call fact is unavailable");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(
                content, "inspect(1)", 0U, 8U, &callPosition)) {
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
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, fileRange, ZR_NULL, &query) ||
        query.expression == ZR_NULL ||
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
        TEST_FAIL(timer, summary, "Valid direct-call canonical signature was unavailable");
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
                  "Direct-call signature help recovered from a local overload or callee-name fallback");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif
