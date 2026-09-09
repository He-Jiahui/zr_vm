#ifndef ZR_TEST_LSP_NATIVE_VIRTUAL_PROVIDER_SCOPE_CASES_H
#define ZR_TEST_LSP_NATIVE_VIRTUAL_PROVIDER_SCOPE_CASES_H

#include "../../zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.h"

static void test_lsp_native_virtual_documents_preserve_provider_scope(SZrState *state) {
    const TZrChar *summary = "LSP Native Virtual Documents Preserve Project And Generation";
    const TZrChar *content = "var provider = import(\"zr.pluginprobe\");\nreturn provider.answer();\n";
    SZrGeneratedDescriptorPluginFixture fixtures[2];
    SZrString *uris[2] = {ZR_NULL, ZR_NULL};
    SZrString *declarations[2] = {ZR_NULL, ZR_NULL};
    SZrString *oldDeclarations[2] = {ZR_NULL, ZR_NULL};
    SZrLspContext *context = ZR_NULL;
    SZrArray definitions = {0};
    SZrArray references = {0};
    SZrArray links = {0};
    SZrLspPosition position;
    SZrTestTimer timer;
    const TZrChar *failure = "two project fixtures must load";
    TZrBool passed = ZR_FALSE;
    TZrSize round = 0U;
    TZrSize project = 0U;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL ||
        !lsp_find_position_for_substring(content, "answer", 0U, 0, &position)) {
        goto cleanup;
    }
    for (project = 0U; project < 2U; project++) {
        if (!prepare_generated_descriptor_plugin_fixture(
                    project == 0U ? "virtual scope & project a" : "virtual scope & project b",
                    project == 0U ? ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH
                                  : ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_FLOAT_PATH,
                    &fixtures[project]) ||
            !write_text_file(fixtures[project].mainPath, content, strlen(content))) {
            goto cleanup;
        }
        uris[project] = create_file_uri_from_native_path(state, fixtures[project].mainPath);
        if (uris[project] == ZR_NULL || !ZrLanguageServer_Lsp_UpdateDocument(
                    state, context, uris[project], content, strlen(content), 1U)) {
            goto cleanup;
        }
    }
    for (round = 0U; round < 2U; round++) {
        for (project = 0U; project < 2U; project++) {
            SZrLspLocation *location;
            SZrString *text = ZR_NULL;
            SZrLspPosition declarationPosition;
            SZrSemanticAnalyzer *analyzer;
            SZrAstNode *savedAst;
            TZrBool foundReferences;
            const TZrChar *expected = project == 0U && round == 0U
                    ? "answer(): int" : "answer(): float";

            failure = "definition must publish one virtual declaration";
            if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uris[project],
                        position, &definitions) || definitions.length != 1U) {
                goto cleanup;
            }
            location = *(SZrLspLocation **)ZrCore_Array_Get(&definitions, 0U);
            if (location == ZR_NULL ||
                !ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(location->uri)) {
                goto cleanup;
            }
            declarations[project] = location->uri;
            failure = "the declaration range must select answer in its provider's rendered text";
            if (!ZrLanguageServer_Lsp_GetNativeDeclarationDocument(
                        state, context, location->uri, &text) || text == ZR_NULL ||
                !lsp_find_position_for_substring(test_string_ptr(text), expected,
                        0U, 0, &declarationPosition) ||
                location->range.start.line != declarationPosition.line ||
                location->range.start.character != declarationPosition.character ||
                location->range.end.line != declarationPosition.line ||
                location->range.end.character != declarationPosition.character + 6) {
                goto cleanup;
            }
            provider_matrix_free_locations(state, &definitions);
            failure = "virtual declaration must navigate to its exact rendered identifier";
            if (!ZrLanguageServer_Lsp_GetDefinition(state, context, declarations[project],
                        declarationPosition, &definitions) || definitions.length != 1U ||
                !location_array_contains_uri_and_range(&definitions, declarations[project],
                        declarationPosition.line, declarationPosition.character,
                        declarationPosition.line, declarationPosition.character + 6)) {
                goto cleanup;
            }
            failure = "virtual declaration references must include only its owning project usage";
            analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uris[project]);
            if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
                goto cleanup;
            }
            savedAst = analyzer->ast;
            analyzer->ast = ZR_NULL;
            foundReferences = ZrLanguageServer_Lsp_FindReferences(state, context, declarations[project],
                    declarationPosition, ZR_TRUE, &references);
            analyzer->ast = savedAst;
            if (!foundReferences || references.length != 2U ||
                !location_array_contains_uri_and_range(&references, uris[project],
                        position.line, position.character, position.line, position.character + 6)) {
                goto cleanup;
            }
            provider_matrix_free_locations(state, &definitions);
            provider_matrix_free_locations(state, &references);
            failure = "source document links must retain the scoped virtual target";
            if (!ZrLanguageServer_Lsp_GetDocumentLinks(state, context, uris[project], &links) ||
                links.length != 1U || !ZrCore_String_Equal(
                        (*(SZrLspDocumentLink **)ZrCore_Array_Get(&links, 0U))->target,
                        declarations[project])) {
                goto cleanup;
            }
            ZrCore_Memory_RawFree(state->global,
                    *(SZrLspDocumentLink **)ZrCore_Array_Get(&links, 0U), sizeof(SZrLspDocumentLink));
            ZrCore_Array_Free(state, &links);
        }
        failure = "same-named providers in different projects need distinct virtual URIs";
        if (ZrCore_String_Equal(declarations[0], declarations[1])) {
            goto cleanup;
        }
        {
            SZrString *text = ZR_NULL;
            SZrLspVirtualDocumentIdentity first;
            SZrLspVirtualDocumentIdentity second;
            SZrString *unscoped = ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(
                    state, "zr.pluginprobe");
            failure = "bare plugin URIs and conflicting provider origins must not resolve";
            if (ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, unscoped, &text) ||
                !ZrLanguageServer_LspVirtualDocumentIdentity_Parse(state, declarations[0], &first) ||
                !ZrLanguageServer_LspVirtualDocumentIdentity_Parse(state, declarations[1], &second)) {
                goto cleanup;
            }
            first.originUri = second.originUri;
            if (ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context,
                        ZrLanguageServer_LspVirtualDocumentIdentity_Create(state, &first), &text)) {
                goto cleanup;
            }
        }
        for (TZrSize request = 0U; request < 8U; request++) {
            SZrString *text = ZR_NULL;
            project = request % 2U;
            failure = "alternating virtual reads must retain their owning provider";
            if (!ZrLanguageServer_Lsp_GetNativeDeclarationDocument(
                        state, context, declarations[project], &text) || text == ZR_NULL ||
                strstr(test_string_ptr(text), project == 0U && round == 0U
                        ? "answer(): int" : "answer(): float") == ZR_NULL) {
                goto cleanup;
            }
        }
        if (round == 0U) {
            SZrString *providerUri = create_file_uri_from_native_path(state, fixtures[0].pluginPath);
            oldDeclarations[0] = declarations[0];
            oldDeclarations[1] = declarations[1];
            failure = "real provider reload must invalidate old virtual document identities";
            if (!copy_fixture_binary_file(ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_FLOAT_PATH,
                        fixtures[0].pluginPath) ||
                !ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(
                        state, context, providerUri)) {
                goto cleanup;
            }
            for (project = 0U; project < 2U; project++) {
                SZrString *text = uris[project];
                if (ZrLanguageServer_Lsp_GetNativeDeclarationDocument(
                            state, context, oldDeclarations[project], &text) || text != ZR_NULL) {
                    goto cleanup;
                }
            }
        }
    }
    failure = "fresh generation must have a new document identity";
    passed = !ZrCore_String_Equal(oldDeclarations[0], declarations[0]) &&
             !ZrCore_String_Equal(oldDeclarations[1], declarations[1]);

cleanup:
    for (TZrSize index = 0U; index < links.length; index++) {
        ZrCore_Memory_RawFree(state->global,
                *(SZrLspDocumentLink **)ZrCore_Array_Get(&links, index), sizeof(SZrLspDocumentLink));
    }
    if (links.isValid) {
        ZrCore_Array_Free(state, &links);
    }
    provider_matrix_free_locations(state, &references);
    provider_matrix_free_locations(state, &definitions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (passed) {
        TEST_PASS(timer, summary);
    } else {
        TZrChar detail[256];
        snprintf(detail, sizeof(detail), "round=%u project=%u: %s",
                (unsigned int)round, (unsigned int)project, failure);
        TEST_FAIL(timer, summary, detail);
    }
}

static void test_lsp_native_virtual_documents_reject_unowned_provider(SZrState *state) {
    const TZrChar *summary = "LSP Native Virtual Documents Reject Another Projects Unowned Provider";
    const TZrChar *content = "return 0;\n";
    SZrGeneratedDescriptorPluginFixture fixtures[2];
    SZrLspContext *context = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrString *text = ZR_NULL;
    SZrLspVirtualDocumentIdentity identity;
    SZrTestTimer timer;
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        goto cleanup;
    }
    for (TZrSize project = 0U; project < 2U; project++) {
        if (!prepare_generated_descriptor_plugin_fixture(
                    project == 0U ? "virtual_provider_owner" : "virtual_provider_missing",
                    ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH, &fixtures[project]) ||
            (project == 1U && remove(fixtures[project].pluginPath) != 0) ||
            !write_text_file(fixtures[project].mainPath, content, strlen(content)) ||
            !ZrLanguageServer_Lsp_UpdateDocument(state, context,
                    create_file_uri_from_native_path(state, fixtures[project].mainPath),
                    content, strlen(content), 1U)) {
            goto cleanup;
        }
    }
    mainUri = create_file_uri_from_native_path(state, fixtures[0].mainPath);
    uri = test_native_virtual_document_uri(state, context, mainUri,
            create_file_uri_from_native_path(state, fixtures[0].pluginPath));
    if (!ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &text) ||
        !ZrLanguageServer_LspVirtualDocumentIdentity_Parse(state, uri, &identity)) {
        goto cleanup;
    }
    identity.projectUri = create_file_uri_from_native_path(state, fixtures[1].projectPath);
    uri = ZrLanguageServer_LspVirtualDocumentIdentity_Create(state, &identity);
    passed = !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &text) &&
             text == ZR_NULL;
cleanup:
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (passed) {
        TEST_PASS(timer, summary);
    } else {
        TEST_FAIL(timer, summary, "a project without a plugin must not borrow another project's registered provider");
    }
}

#endif
