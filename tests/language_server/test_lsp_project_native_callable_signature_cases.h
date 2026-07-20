#ifndef ZR_VM_TEST_LSP_PROJECT_NATIVE_CALLABLE_SIGNATURE_CASES_H
#define ZR_VM_TEST_LSP_PROJECT_NATIVE_CALLABLE_SIGNATURE_CASES_H

static const TZrChar *native_callable_parameter_label(
        SZrLspSignatureHelp *help,
        TZrSize index) {
    SZrLspSignatureInformation **signaturePtr;
    SZrLspParameterInformation **parameterPtr;

    if (help == ZR_NULL || help->signatures.length == 0U) {
        return ZR_NULL;
    }
    signaturePtr = (SZrLspSignatureInformation **)ZrCore_Array_Get(
            &help->signatures, 0U);
    if (signaturePtr == ZR_NULL || *signaturePtr == ZR_NULL ||
        index >= (*signaturePtr)->parameters.length) {
        return ZR_NULL;
    }
    parameterPtr = (SZrLspParameterInformation **)ZrCore_Array_Get(
            &(*signaturePtr)->parameters, index);
    return parameterPtr != ZR_NULL && *parameterPtr != ZR_NULL &&
                   (*parameterPtr)->label != ZR_NULL
               ? test_string_ptr((*parameterPtr)->label)
               : ZR_NULL;
}

static const TZrChar *native_callable_parameter_documentation(
        SZrLspSignatureHelp *help,
        TZrSize index) {
    SZrLspSignatureInformation **signaturePtr;
    SZrLspParameterInformation **parameterPtr;

    if (help == ZR_NULL || help->signatures.length == 0U) {
        return ZR_NULL;
    }
    signaturePtr = (SZrLspSignatureInformation **)ZrCore_Array_Get(
            &help->signatures, 0U);
    if (signaturePtr == ZR_NULL || *signaturePtr == ZR_NULL ||
        index >= (*signaturePtr)->parameters.length) {
        return ZR_NULL;
    }
    parameterPtr = (SZrLspParameterInformation **)ZrCore_Array_Get(
            &(*signaturePtr)->parameters, index);
    return parameterPtr != ZR_NULL && *parameterPtr != ZR_NULL &&
                   (*parameterPtr)->documentation != ZR_NULL
               ? test_string_ptr((*parameterPtr)->documentation)
               : ZR_NULL;
}

static void test_lsp_descriptor_plugin_callable_query_hover_and_signature_share_contract(
        SZrState *state) {
    static const TZrChar *mainContent =
            "var plugin = %import(\"zr.pluginprobe\");\n"
            "var total = plugin.combine(1, 2);\n"
            "plugin.incomplete_callable();\n"
            "return total;\n";
    static const TZrChar *expectedLabel =
            "combine(left: int, right: int): int";
    static const TZrChar *expectedReloadedLabel =
            "combine(left: float, right: float): float";
    const TZrChar *summary =
            "LSP Descriptor Plugin Callable Query Hover And Signature Share Contract";
    SZrTestTimer timer;
    SZrGeneratedDescriptorPluginFixture fixture;
    SZrLspContext *context = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *pluginUri = ZR_NULL;
    SZrLspPosition memberPosition;
    SZrLspPosition signaturePosition;
    SZrLspPosition unavailableSignaturePosition;
    SZrLspSemanticQuery query;
    SZrLspSignatureHelp *help = ZR_NULL;
    SZrLspHover *hover = ZR_NULL;
    const TZrChar *label;
    const TZrChar *leftLabel;
    const TZrChar *rightLabel;
    const TZrChar *leftDocumentation;
    const TZrChar *rightDocumentation;
    TZrChar reason[768];
    TZrBool success = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "Native descriptor callable parity",
            "The canonical ModuleIdentity/provider/member query should drive identical hover and signature labels from structured descriptor parameters");

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (!prepare_generated_descriptor_plugin_fixture(
                "project_features_native_callable_signature",
                ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH,
                &fixture) ||
        !write_text_file(fixture.mainPath, mainContent, strlen(mainContent))) {
        TEST_FAIL(timer, summary, "Failed to prepare descriptor callable fixture");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    pluginUri = create_file_uri_from_native_path(state, fixture.pluginPath);
    if (context == ZR_NULL || mainUri == ZR_NULL || pluginUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                mainUri,
                mainContent,
                strlen(mainContent),
                1U) ||
        !lsp_find_position_for_substring(
                mainContent, "plugin.combine", 0U, 8, &memberPosition) ||
        !lsp_find_position_for_substring(
                mainContent,
                "plugin.combine(1, 2)",
                0U,
                18,
                &signaturePosition) ||
        !lsp_find_position_for_substring(
                mainContent,
                "plugin.incomplete_callable()",
                0U,
                26,
                &unavailableSignaturePosition)) {
        TEST_FAIL(timer, summary, "Failed to load descriptor callable fixture");
        goto cleanup;
    }

    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, mainUri, memberPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER ||
        query.sourceKind !=
                ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN ||
        query.moduleName == ZR_NULL ||
        strcmp(test_string_ptr(query.moduleName), "zr.pluginprobe") != 0 ||
        query.memberName == ZR_NULL ||
        strcmp(test_string_ptr(query.memberName), "combine") != 0 ||
        query.resolvedMember.memberKind != ZR_LSP_METADATA_MEMBER_FUNCTION ||
        query.resolvedMember.functionDescriptor == ZR_NULL ||
        query.resolvedMember.functionDescriptor->parameterCount != 2U) {
        snprintf(
                reason,
                sizeof(reason),
                "External callable identity mismatch (kind=%d source=%d module=%s member=%s parameters=%zu)",
                (int)query.kind,
                (int)query.sourceKind,
                query.moduleName != ZR_NULL
                        ? test_string_ptr(query.moduleName)
                        : "<null>",
                query.memberName != ZR_NULL
                        ? test_string_ptr(query.memberName)
                        : "<null>",
                query.resolvedMember.functionDescriptor != ZR_NULL
                        ? (size_t)query.resolvedMember.functionDescriptor->parameterCount
                        : 0U);
        TEST_FAIL(timer, summary, reason);
        goto cleanup;
    }

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, mainUri, signaturePosition, &help) ||
        help == ZR_NULL) {
        TEST_FAIL(timer, summary, "Descriptor callable signature help was unavailable");
        goto cleanup;
    }

    label = signature_help_first_label(help);
    leftLabel = native_callable_parameter_label(help, 0U);
    rightLabel = native_callable_parameter_label(help, 1U);
    leftDocumentation = native_callable_parameter_documentation(help, 0U);
    rightDocumentation = native_callable_parameter_documentation(help, 1U);
    if (label == ZR_NULL || strcmp(label, expectedLabel) != 0 ||
        leftLabel == ZR_NULL || strcmp(leftLabel, "left: int") != 0 ||
        rightLabel == ZR_NULL || strcmp(rightLabel, "right: int") != 0 ||
        leftDocumentation == ZR_NULL ||
        strstr(leftDocumentation, "Left value to combine.") == ZR_NULL ||
        rightDocumentation == ZR_NULL ||
        strstr(rightDocumentation, "Right value to combine.") == ZR_NULL ||
        help->activeParameter != 1) {
        snprintf(
                reason,
                sizeof(reason),
                "Descriptor signature mismatch (label=%s left=%s right=%s active=%d)",
                label != ZR_NULL ? label : "<null>",
                leftLabel != ZR_NULL ? leftLabel : "<null>",
                rightLabel != ZR_NULL ? rightLabel : "<null>",
                (int)help->activeParameter);
        TEST_FAIL(timer, summary, reason);
        goto cleanup;
    }

    {
        SZrLspSignatureHelp *unavailableHelp = ZR_NULL;
        if (ZrLanguageServer_Lsp_GetSignatureHelp(
                    state,
                    context,
                    mainUri,
                    unavailableSignaturePosition,
                    &unavailableHelp) ||
            unavailableHelp != ZR_NULL) {
            if (unavailableHelp != ZR_NULL) {
                ZrLanguageServer_LspSignatureHelp_Free(state, unavailableHelp);
            }
            TEST_FAIL(
                    timer,
                    summary,
                    "Incomplete native descriptor callable must not use AST or name fallback");
            goto cleanup;
        }
    }

    if (!ZrLanguageServer_Lsp_GetHover(
                state, context, mainUri, memberPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, expectedLabel) ||
        !hover_contains_text(hover, "Source: native descriptor plugin")) {
        TEST_FAIL(
                timer,
                summary,
                "Descriptor callable hover did not reuse the canonical signature label");
        goto cleanup;
    }

    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    help = ZR_NULL;
    hover = ZR_NULL;
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(
                state, context, pluginUri) ||
        !copy_fixture_binary_file(
                ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_FLOAT_PATH,
                fixture.pluginPath) ||
        !ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(
                state, context, pluginUri) ||
        !ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, mainUri, memberPosition, &query) ||
        query.sourceKind !=
                ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN ||
        query.resolvedMember.functionDescriptor == ZR_NULL ||
        query.resolvedMember.functionDescriptor->returnTypeName == ZR_NULL ||
        strcmp(query.resolvedMember.functionDescriptor->returnTypeName, "float") !=
                0 ||
        !ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, mainUri, signaturePosition, &help) ||
        help == ZR_NULL ||
        (label = signature_help_first_label(help)) == ZR_NULL ||
        strcmp(label, expectedReloadedLabel) != 0 ||
        (leftLabel = native_callable_parameter_label(help, 0U)) == ZR_NULL ||
        strcmp(leftLabel, "left: float") != 0 ||
        (rightLabel = native_callable_parameter_label(help, 1U)) == ZR_NULL ||
        strcmp(rightLabel, "right: float") != 0 ||
        !ZrLanguageServer_Lsp_GetHover(
                state, context, mainUri, memberPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, expectedReloadedLabel)) {
        snprintf(
                reason,
                sizeof(reason),
                "Reloaded descriptor callable contract stayed stale (label=%s left=%s right=%s)",
                label != ZR_NULL ? label : "<null>",
                leftLabel != ZR_NULL ? leftLabel : "<null>",
                rightLabel != ZR_NULL ? rightLabel : "<null>");
        TEST_FAIL(timer, summary, reason);
        goto cleanup;
    }

    success = ZR_TRUE;

cleanup:
    if (help != ZR_NULL) {
        ZrLanguageServer_LspSignatureHelp_Free(state, help);
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (success) {
        TEST_PASS(timer, summary);
    }
}

#endif
