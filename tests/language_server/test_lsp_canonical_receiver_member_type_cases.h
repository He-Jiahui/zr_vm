#ifndef ZR_VM_TEST_LSP_CANONICAL_RECEIVER_MEMBER_TYPE_CASES_H
#define ZR_VM_TEST_LSP_CANONICAL_RECEIVER_MEMBER_TYPE_CASES_H

static void test_lsp_receiver_member_type_fails_closed_without_declaration_fact(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Receiver Member Type Fails Closed Without Declaration Fact";
    const TZrChar *uriText = "file:///canonical_receiver_member_type.zr";
    const TZrChar *content =
            "class Meter {\n"
            "    pub var value: int = 1;\n"
            "}\n"
            "fn read(meter: Meter): int { return meter.value; }\n";
    const TZrChar *memberUse = strstr(content, "meter.value");
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrSymbol *declarationSymbol = ZR_NULL;
    SZrLspResolvedMetadataMember resolvedMember;
    TZrSymbolId declarationSymbolId = ZR_SEMANTIC_ID_INVALID;
    TZrSize memberOffset = 0U;
    TZrBool positiveCanonicalType = ZR_FALSE;
    TZrBool missingFactFailsClosed = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "Canonical receiver member type",
            "Project member type text must require the resolved declaration SymbolId and canonical TypeId");
    memset(&resolvedMember, 0, sizeof(resolvedMember));
    if (memberUse == ZR_NULL) {
        TEST_FAIL(timer, summary, "Failed to locate the receiver member fixture");
        return;
    }
    memberOffset = (TZrSize)(memberUse - content) + strlen("meter.") + 1U;
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            (TZrNativeString)uriText,
            strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                content,
                strlen(content),
                1U)) {
        goto cleanup;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        analyzer->ast == ZR_NULL ||
        !ZrLanguageServer_Lsp_TryResolveReceiverProjectMember(
                state,
                context,
                ZR_NULL,
                analyzer,
                uri,
                analyzer->ast,
                content,
                strlen(content),
                memberOffset,
                &resolvedMember)) {
        goto cleanup;
    }
    positiveCanonicalType =
            resolvedMember.hasDeclaration &&
            resolvedMember.resolvedTypeText != ZR_NULL &&
            strcmp(test_string_ptr(resolvedMember.resolvedTypeText), "int") == 0;
    declarationSymbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(
            analyzer,
            resolvedMember.declarationRange);
    if (declarationSymbol == ZR_NULL ||
        declarationSymbol->semanticId == ZR_SEMANTIC_ID_INVALID) {
        goto cleanup;
    }
    declarationSymbolId = declarationSymbol->semanticId;
    for (TZrSize referenceIndex = 0U;
         referenceIndex < analyzer->semanticContext->referenceFacts.length;
         referenceIndex++) {
        SZrSemanticReferenceFact *reference =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &analyzer->semanticContext->referenceFacts,
                        referenceIndex);
        if (reference != ZR_NULL &&
            reference->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
            reference->symbolId == declarationSymbolId) {
            reference->isResolved = ZR_FALSE;
        }
    }

    memset(&resolvedMember, 0, sizeof(resolvedMember));
    missingFactFailsClosed =
            ZrLanguageServer_Lsp_TryResolveReceiverProjectMember(
                    state,
                    context,
                    ZR_NULL,
                    analyzer,
                    uri,
                    analyzer->ast,
                    content,
                    strlen(content),
                    memberOffset,
                    &resolvedMember) &&
            resolvedMember.hasDeclaration &&
            resolvedMember.resolvedTypeText == ZR_NULL;

cleanup:
    if (positiveCanonicalType && missingFactFailsClosed) {
        TEST_PASS(timer, summary);
    } else {
        TEST_FAIL(
                timer,
                summary,
                "Receiver member type text did not follow declaration-fact availability");
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#endif
