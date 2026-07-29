#ifndef ZR_VM_TEST_LSP_PROJECT_NATIVE_RECEIVER_CALLABLE_CASES_H
#define ZR_VM_TEST_LSP_PROJECT_NATIVE_RECEIVER_CALLABLE_CASES_H

#include "zr_vm_parser/semantic_query.h"

static void test_lsp_native_receiver_callable_query_hover_and_signature_share_closed_contract(
        SZrState *state) {
    static const TZrChar *content =
            "var {LinkedList} = import(\"zr.container\");\n"
            "var list: LinkedList<int> = null;\n"
            "var node = list.addLast(1);\n"
            "return node;\n";
    static const TZrChar *expectedLabel =
            "fn addLast(value: int): LinkedNode<int>";
    const TZrChar *summary =
            "LSP Native Receiver Callable Query Hover And Signature Share Closed Contract";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition memberPosition;
    SZrLspPosition signaturePosition;
    SZrLspSemanticQuery query;
    SZrLspSignatureHelp *help = ZR_NULL;
    SZrLspHover *hover = ZR_NULL;
    const TZrChar *label = ZR_NULL;
    const TZrChar *parameterLabel = ZR_NULL;
    TZrChar reason[768];
    TZrBool success = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "Native receiver callable parity",
            "The exact native method query should merge descriptor identity with the parser canonical closed callable TypeId for hover and signature help");

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///native_receiver_callable.zr",
            strlen("file:///native_receiver_callable.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(
                content, "list.addLast(1)", 0U, 7, &memberPosition) ||
        !lsp_find_position_for_substring(
                content, "list.addLast(1)", 0U, 13, &signaturePosition)) {
        TEST_FAIL(timer, summary, "Failed to prepare native receiver callable fixture");
        goto cleanup;
    }

    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, memberPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        query.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN ||
        query.moduleName == ZR_NULL ||
        strcmp(test_string_ptr(query.moduleName), "zr.container") != 0 ||
        query.memberName == ZR_NULL ||
        strcmp(test_string_ptr(query.memberName), "addLast") != 0 ||
        query.resolvedMember.memberKind != ZR_LSP_METADATA_MEMBER_METHOD ||
        query.resolvedMember.methodDescriptor == ZR_NULL ||
        query.resolvedMember.ownerTypeName == ZR_NULL ||
        strcmp(test_string_ptr(query.resolvedMember.ownerTypeName),
               "LinkedList<int>") != 0) {
        snprintf(
                reason,
                sizeof(reason),
                "Native method identity mismatch (kind=%d source=%d module=%s member=%s owner=%s)",
                (int)query.kind,
                (int)query.sourceKind,
                query.moduleName != ZR_NULL
                        ? test_string_ptr(query.moduleName)
                        : "<null>",
                query.memberName != ZR_NULL
                        ? test_string_ptr(query.memberName)
                        : "<null>",
                query.resolvedMember.ownerTypeName != ZR_NULL
                        ? test_string_ptr(query.resolvedMember.ownerTypeName)
                        : "<null>");
        TEST_FAIL(timer, summary, reason);
        goto cleanup;
    }

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, signaturePosition, &help) ||
        help == ZR_NULL) {
        TEST_FAIL(timer, summary, "Native receiver signature help was unavailable");
        goto cleanup;
    }
    label = signature_help_first_label(help);
    parameterLabel = native_callable_parameter_label(help, 0U);
    if (label == ZR_NULL || strcmp(label, expectedLabel) != 0 ||
        parameterLabel == ZR_NULL || strcmp(parameterLabel, "value: int") != 0 ||
        help->activeParameter != 0) {
        snprintf(
                reason,
                sizeof(reason),
                "Native receiver signature mismatch (label=%s parameter=%s active=%d)",
                label != ZR_NULL ? label : "<null>",
                parameterLabel != ZR_NULL ? parameterLabel : "<null>",
                (int)help->activeParameter);
        TEST_FAIL(timer, summary, reason);
        goto cleanup;
    }

    if (!ZrLanguageServer_Lsp_GetHover(
                state, context, uri, memberPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, expectedLabel) ||
        !hover_contains_text(hover, "Source: native builtin")) {
        TEST_FAIL(
                timer,
                summary,
                "Native receiver hover did not reuse the closed canonical signature label");
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

static void test_lsp_descriptor_plugin_receiver_callable_tracks_provider_generation(
        SZrState *state) {
    static const TZrChar *content =
            "var plugin = import(\"zr.pluginprobe\");\n"
            "var point = plugin.makePoint();\n"
            "var total = point.total();\n"
            "var echoed = point.echo(1);\n"
            "var constrained = point.constrained_echo(point);\n"
            "point.incomplete_total();\n"
            "return total;\n";
    static const TZrChar *expectedLabel = "fn total(): int";
    static const TZrChar *expectedReloadedLabel = "fn total(): double";
    static const TZrChar *expectedGenericLabel =
            "fn echo<T>(value: int): int";
    const TZrChar *summary =
            "LSP Descriptor Plugin Receiver Callable Tracks Provider Generation";
    SZrTestTimer timer;
    SZrGeneratedDescriptorPluginFixture fixture;
    SZrLspContext *context = ZR_NULL;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrString *pluginUri = ZR_NULL;
    SZrLspPosition memberPosition;
    SZrLspPosition signaturePosition;
    SZrLspPosition genericMemberPosition;
    SZrLspPosition genericSignaturePosition;
    SZrLspPosition constrainedMemberPosition;
    SZrLspPosition constrainedSignaturePosition;
    SZrLspPosition unavailableSignaturePosition;
    SZrLspSemanticQuery query;
    SZrLspSemanticQuery genericQuery;
    SZrLspSemanticQuery constrainedQuery;
    SZrParserSemanticCallQuery parserGenericQuery;
    SZrFilePosition genericFilePosition;
    SZrFileRange genericFileRange;
    SZrLspSignatureHelp *help = ZR_NULL;
    SZrLspSignatureHelp *genericHelp = ZR_NULL;
    SZrLspHover *hover = ZR_NULL;
    SZrLspHover *genericHover = ZR_NULL;
    const TZrChar *label = ZR_NULL;
    const TZrChar *genericParameterLabel = ZR_NULL;
    TZrChar reason[768];
    TZrChar parserGenericLabel[256];
    TZrBool success = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "Descriptor receiver generation",
            "A descriptor-plugin instance method should merge the current provider descriptor identity with the current parser canonical callable TypeId after reload");

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    ZrLanguageServer_LspSemanticQuery_Init(&genericQuery);
    ZrLanguageServer_LspSemanticQuery_Init(&constrainedQuery);
    memset(&parserGenericQuery, 0, sizeof(parserGenericQuery));
    memset(parserGenericLabel, 0, sizeof(parserGenericLabel));
    if (!prepare_generated_descriptor_plugin_fixture(
                "project_features_native_receiver_callable",
                ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH,
                &fixture) ||
        !write_text_file(fixture.mainPath, content, strlen(content))) {
        TEST_FAIL(timer, summary, "Failed to prepare descriptor receiver fixture");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    uri = create_file_uri_from_native_path(state, fixture.mainPath);
    pluginUri = create_file_uri_from_native_path(state, fixture.pluginPath);
    if (context == ZR_NULL || uri == ZR_NULL || pluginUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(
                content, "point.total()", 0U, 8, &memberPosition) ||
        !lsp_find_position_for_substring(
                content, "point.total()", 0U, 12, &signaturePosition) ||
        !lsp_find_position_for_substring(
                content, "point.echo(1)", 0U, 8, &genericMemberPosition) ||
        !lsp_find_position_for_substring(
                content,
                "point.echo(1)",
                0U,
                11,
                &genericSignaturePosition) ||
        !lsp_find_position_for_substring(
                content,
                "point.constrained_echo(point)",
                0U,
                8,
                &constrainedMemberPosition) ||
        !lsp_find_position_for_substring(
                content,
                "point.constrained_echo(point)",
                0U,
                23,
                &constrainedSignaturePosition) ||
        !lsp_find_position_for_substring(
                content,
                "point.incomplete_total()",
                0U,
                22,
                &unavailableSignaturePosition)) {
        TEST_FAIL(timer, summary, "Failed to load descriptor receiver fixture");
        goto cleanup;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    genericFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, genericMemberPosition);
    genericFileRange = ZrParser_FileRange_Create(
            genericFilePosition, genericFilePosition, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext,
                genericFileRange,
                ZR_NULL,
                &parserGenericQuery) ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext,
                &parserGenericQuery,
                parserGenericLabel,
                sizeof(parserGenericLabel)) ||
        strcmp(parserGenericLabel, expectedGenericLabel) != 0) {
        snprintf(
                reason,
                sizeof(reason),
                "Parser generic receiver call contract unavailable (label=%s)",
                parserGenericLabel[0] != '\0' ? parserGenericLabel
                                                : "<unavailable>");
        TEST_FAIL(timer, summary, reason);
        goto cleanup;
    }

    label = ZR_NULL;
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, memberPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        query.sourceKind !=
                ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN ||
        query.moduleName == ZR_NULL ||
        strcmp(test_string_ptr(query.moduleName), "zr.pluginprobe") != 0 ||
        query.memberName == ZR_NULL ||
        strcmp(test_string_ptr(query.memberName), "total") != 0 ||
        query.resolvedMember.memberKind != ZR_LSP_METADATA_MEMBER_METHOD ||
        query.resolvedMember.methodDescriptor == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, signaturePosition, &help) ||
        help == ZR_NULL ||
        (label = signature_help_first_label(help)) == ZR_NULL ||
        strcmp(label, expectedLabel) != 0 ||
        !ZrLanguageServer_Lsp_GetHover(
                state, context, uri, memberPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, expectedLabel) ||
        !hover_contains_text(hover, "Returns the total coordinate value.") ||
        !hover_contains_text(hover, "Source: native descriptor plugin")) {
        snprintf(
                reason,
                sizeof(reason),
                "Descriptor receiver callable mismatch before reload (kind=%d source=%d label=%s)",
                (int)query.kind,
                (int)query.sourceKind,
                label != ZR_NULL ? label : "<null>");
        TEST_FAIL(timer, summary, reason);
        goto cleanup;
    }

    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, genericMemberPosition, &genericQuery) ||
        genericQuery.kind !=
                ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        genericQuery.resolvedMember.memberKind !=
                ZR_LSP_METADATA_MEMBER_METHOD ||
        genericQuery.resolvedMember.methodDescriptor == ZR_NULL ||
        genericQuery.resolvedMember.methodDescriptor->genericParameterCount !=
                1U ||
        genericQuery.resolvedMember.methodDescriptor->genericParameters ==
                ZR_NULL ||
        strcmp(genericQuery.resolvedMember.methodDescriptor
                       ->genericParameters[0]
                       .name,
               "T") != 0 ||
        !ZrLanguageServer_Lsp_GetSignatureHelp(
                state,
                context,
                uri,
                genericSignaturePosition,
                &genericHelp) ||
        genericHelp == ZR_NULL ||
        (label = signature_help_first_label(genericHelp)) == ZR_NULL ||
        strcmp(label, expectedGenericLabel) != 0 ||
        (genericParameterLabel =
                 native_callable_parameter_label(genericHelp, 0U)) == ZR_NULL ||
        strcmp(genericParameterLabel, "value: int") != 0 ||
        !ZrLanguageServer_Lsp_GetHover(
                state, context, uri, genericMemberPosition, &genericHover) ||
        genericHover == ZR_NULL ||
        !hover_contains_text(genericHover, expectedGenericLabel) ||
        !hover_contains_text(
                genericHover,
                "Returns a value using an unconstrained method generic.")) {
        snprintf(
                reason,
                sizeof(reason),
                "Descriptor generic receiver callable mismatch (kind=%d label=%s)",
                (int)genericQuery.kind,
                label != ZR_NULL ? label : "<null>");
        TEST_FAIL(timer, summary, reason);
        goto cleanup;
    }

    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state,
                context,
                uri,
                constrainedMemberPosition,
                &constrainedQuery) ||
        constrainedQuery.kind !=
                ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        constrainedQuery.resolvedMember.memberKind !=
                ZR_LSP_METADATA_MEMBER_METHOD ||
        constrainedQuery.resolvedMember.methodDescriptor == ZR_NULL ||
        constrainedQuery.resolvedMember.methodDescriptor
                        ->genericParameterCount != 1U ||
        constrainedQuery.resolvedMember.methodDescriptor->genericParameters ==
                ZR_NULL ||
        constrainedQuery.resolvedMember.methodDescriptor->genericParameters[0]
                        .constraintTypeCount != 1U) {
        TEST_FAIL(
                timer,
                summary,
                "Constrained generic receiver metadata identity was not available");
        goto cleanup;
    }
    {
        SZrLspSignatureHelp *constrainedHelp = ZR_NULL;
        if (ZrLanguageServer_Lsp_GetSignatureHelp(
                    state,
                    context,
                    uri,
                    constrainedSignaturePosition,
                    &constrainedHelp) ||
            constrainedHelp != ZR_NULL) {
            if (constrainedHelp != ZR_NULL) {
                ZrLanguageServer_LspSignatureHelp_Free(
                        state, constrainedHelp);
            }
            TEST_FAIL(
                    timer,
                    summary,
                    "Constrained generic receiver callable must remain unavailable until constraints have a canonical display contract");
            goto cleanup;
        }
    }

    {
        SZrLspSignatureHelp *unavailableHelp = ZR_NULL;
        if (ZrLanguageServer_Lsp_GetSignatureHelp(
                    state,
                    context,
                    uri,
                    unavailableSignaturePosition,
                    &unavailableHelp) ||
            unavailableHelp != ZR_NULL) {
            if (unavailableHelp != ZR_NULL) {
                ZrLanguageServer_LspSignatureHelp_Free(state, unavailableHelp);
            }
            TEST_FAIL(
                    timer,
                    summary,
                    "Incomplete native receiver descriptor must not use canonical-wide or name fallback");
            goto cleanup;
        }
    }

    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    help = ZR_NULL;
    hover = ZR_NULL;
    label = ZR_NULL;
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
                state, context, uri, memberPosition, &query) ||
        query.sourceKind !=
                ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN ||
        query.resolvedMember.methodDescriptor == ZR_NULL ||
        query.resolvedMember.methodDescriptor->returnTypeName == ZR_NULL ||
        strcmp(query.resolvedMember.methodDescriptor->returnTypeName, "float") != 0 ||
        !ZrLanguageServer_Lsp_GetSignatureHelp(
                state, context, uri, signaturePosition, &help) ||
        help == ZR_NULL ||
        (label = signature_help_first_label(help)) == ZR_NULL ||
        strcmp(label, expectedReloadedLabel) != 0 ||
        !ZrLanguageServer_Lsp_GetHover(
                state, context, uri, memberPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, expectedReloadedLabel)) {
        snprintf(
                reason,
                sizeof(reason),
                "Descriptor receiver callable stayed stale after reload (label=%s return=%s)",
                label != ZR_NULL ? label : "<null>",
                query.resolvedMember.methodDescriptor != ZR_NULL &&
                                query.resolvedMember.methodDescriptor
                                                ->returnTypeName != ZR_NULL
                        ? query.resolvedMember.methodDescriptor->returnTypeName
                        : "<null>");
        TEST_FAIL(timer, summary, reason);
        goto cleanup;
    }

    success = ZR_TRUE;

cleanup:
    if (help != ZR_NULL) {
        ZrLanguageServer_LspSignatureHelp_Free(state, help);
    }
    if (genericHelp != ZR_NULL) {
        ZrLanguageServer_LspSignatureHelp_Free(state, genericHelp);
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspSemanticQuery_Free(state, &genericQuery);
    ZrLanguageServer_LspSemanticQuery_Free(state, &constrainedQuery);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (success) {
        TEST_PASS(timer, summary);
    }
}

#endif
