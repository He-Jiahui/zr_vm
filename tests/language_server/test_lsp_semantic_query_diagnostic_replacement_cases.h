static SZrFileRange diagnostic_replacement_range(
        SZrState *state,
        TZrSize startOffset,
        TZrSize endOffset) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_CreateFromNative(
            state, "semantic_query_diagnostic_replacement.zr");
    return range;
}

static void test_semantic_query_replaces_stale_duplicate_diagnostic(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Semantic Query Replaces Stale Duplicate Diagnostic";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(state);
    SZrSemanticAnalyzer *analyzer =
            ZrLanguageServer_SemanticAnalyzer_New(state);
    SZrStructuredDiagnostic structured;
    SZrSemanticDiagnosticFact fact;
    SZrDiagnostic *stale;
    SZrDiagnostic *projected;
    SZrDiagnostic **projectedSlot;
    SZrDiagnosticRelatedInformation *related;
    SZrFileRange location = diagnostic_replacement_range(state, 8U, 12U);
    SZrFileRange relatedLocation = diagnostic_replacement_range(state, 2U, 6U);
    TZrUInt32 descriptorId;
    SZrTestTimer timer;

    TEST_START(summary);
    if (context == ZR_NULL || analyzer == ZR_NULL ||
        !ZrParser_DiagnosticBuilder_Build(
                state,
                &structured,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "numeric_overflow",
                "Canonical numeric overflow diagnostic.",
                "The expression exceeds the canonical integer range.",
                "Change the expression or select a wider numeric contract.")) {
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        if (context != ZR_NULL) {
            ZrParser_SemanticContext_Free(context);
        }
        TEST_FAIL(timer, summary, "Failed to initialize diagnostic fixture");
        return;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                &structured,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION) ||
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                state,
                &structured,
                relatedLocation,
                "The constrained declaration is here")) {
        ZrParser_StructuredDiagnostic_Free(state, &structured);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        ZrParser_SemanticContext_Free(context);
        TEST_FAIL(timer, summary, "Failed to complete structured diagnostic fixture");
        return;
    }
    descriptorId = structured.descriptorId;
    memset(&fact, 0, sizeof(fact));
    fact.diagnostic = structured;
    if (!ZrParser_SemanticFacts_AppendDiagnostic(context, &fact)) {
        ZrParser_StructuredDiagnostic_Free(state, &structured);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        ZrParser_SemanticContext_Free(context);
        TEST_FAIL(timer, summary, "Failed to publish canonical diagnostic fact");
        return;
    }
    ZrParser_StructuredDiagnostic_Free(state, &structured);

    stale = ZrLanguageServer_Diagnostic_New(
            state,
            ZR_DIAGNOSTIC_WARNING,
            location,
            "Stale locally reconstructed diagnostic.",
            "numeric_overflow");
    if (stale == ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        ZrParser_SemanticContext_Free(context);
        TEST_FAIL(timer, summary, "Failed to create stale diagnostic");
        return;
    }
    analyzer->semanticContext = context;
    ZrCore_Array_Push(state, &analyzer->diagnostics, &stale);
    ZrLanguageServer_SemanticAnalyzer_AppendSemanticQueryDiagnostics(
            state, analyzer);

    projectedSlot = analyzer->diagnostics.length == 1U
                            ? (SZrDiagnostic **)ZrCore_Array_Get(
                                      &analyzer->diagnostics, 0U)
                            : ZR_NULL;
    projected = projectedSlot != ZR_NULL ? *projectedSlot : ZR_NULL;
    related = projected != ZR_NULL && projected->relatedInformation.isValid &&
                      projected->relatedInformation.length == 1U
                      ? (SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
                                &projected->relatedInformation, 0U)
                      : ZR_NULL;
    if (projected == ZR_NULL || projected == stale ||
        projected->severity != ZR_DIAGNOSTIC_ERROR ||
        projected->descriptorId != descriptorId || descriptorId == 0U ||
        projected->noFixReason !=
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
        projected->codeDescriptionHref == ZR_NULL ||
        test_string_text(projected->code) == ZR_NULL ||
        strcmp(test_string_text(projected->code), "numeric_overflow") != 0 ||
        test_string_text(projected->message) == ZR_NULL ||
        strcmp(test_string_text(projected->message),
               "Canonical numeric overflow diagnostic.") != 0 ||
        test_string_text(projected->cause) == ZR_NULL ||
        strcmp(test_string_text(projected->cause),
               "The expression exceeds the canonical integer range.") != 0 ||
        test_string_text(projected->suggestion) == ZR_NULL ||
        strcmp(test_string_text(projected->suggestion),
               "Change the expression or select a wider numeric contract.") != 0 ||
        projected->location.start.offset != location.start.offset ||
        projected->location.end.offset != location.end.offset ||
        related == ZR_NULL ||
        related->location.start.offset != relatedLocation.start.offset ||
        related->location.end.offset != relatedLocation.end.offset ||
        test_string_text(related->message) == ZR_NULL ||
        strcmp(test_string_text(related->message),
               "The constrained declaration is here") != 0 ||
        projected->fixes.isValid) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        ZrParser_SemanticContext_Free(context);
        TEST_FAIL(timer, summary, "Canonical query projection did not replace stale duplicate");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    ZrParser_SemanticContext_Free(context);
    TEST_PASS(timer, summary);
}
