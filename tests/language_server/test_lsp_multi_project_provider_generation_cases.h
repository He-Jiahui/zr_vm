#ifndef ZR_VM_TEST_LSP_MULTI_PROJECT_PROVIDER_GENERATION_CASES_H
#define ZR_VM_TEST_LSP_MULTI_PROJECT_PROVIDER_GENERATION_CASES_H

#include "zr_vm_language_server/lsp_semantic_snapshot.h"
#include "../../zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h"

static void provider_matrix_free_locations(SZrState *state, SZrArray *locations) {
    if (!locations->isValid) {
        return;
    }
    for (TZrSize index = 0U; index < locations->length; index++) {
        SZrLspLocation **slot = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (slot != ZR_NULL && *slot != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *slot, sizeof(SZrLspLocation));
        }
    }
    ZrCore_Array_Free(state, locations);
}

static TZrBool provider_matrix_check_project(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrString *providerUri,
        SZrLspPosition memberPosition,
        SZrLspPosition localPosition,
        TZrSize memberLength,
        EZrValueType expectedType,
        TZrSize acquisitionMode,
        const TZrChar **failure) {
    SZrLspSemanticQuery query;
    SZrLspLocalSemanticQueryResult localQuery;
    SZrLspSemanticSnapshot *snapshot = ZR_NULL;
    SZrLspHover *hover = ZR_NULL;
    SZrSemanticAnalyzer *analyzer;
    SZrArray definitions = {0};
    SZrArray references = {0};
    SZrLspSemanticCacheStorageInfo storageInfo;
    const SZrLspLocation *location;
    TZrBool valid = ZR_FALSE;
    TZrBool hasMemberQuery = ZR_FALSE;

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    ZrLanguageServer_LspLocalSemanticQuery_Init(&localQuery);
    *failure = "the first request after reload must acquire current facts";
    if (acquisitionMode == 1U) {
        snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(state, context, uri);
        analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
        if (snapshot == ZR_NULL || analyzer == ZR_NULL ||
            analyzer->semanticContext == ZR_NULL ||
            analyzer->semanticContext->externalProviderGeneration !=
                    context->semanticSnapshotProviderGeneration ||
            !ZrLanguageServer_LspSemanticSnapshot_Validate(state, context, snapshot)) {
            goto cleanup;
        }
    } else if (acquisitionMode == 2U || acquisitionMode == 4U) {
        TZrBool acquired = acquisitionMode == 2U
                ? ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(
                        state, context, uri, localPosition, &localQuery)
                : ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(
                        state, context, uri, localPosition, &localQuery);
        if (!acquired || localQuery.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT) {
            goto cleanup;
        }
        ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
    } else if (acquisitionMode == 3U) {
        hasMemberQuery = ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, memberPosition, &query);
        if (!hasMemberQuery) {
            goto cleanup;
        }
    }
    *failure = "importer local hover must reflect its own provider";
    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, localPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, expectedType == ZR_VALUE_TYPE_INT64
                ? "Resolved Type: int" : "Resolved Type: double")) {
        if (hover != ZR_NULL && hover->contents.length > 0U) {
            SZrString **text = (SZrString **)ZrCore_Array_Get(&hover->contents, 0U);
            fprintf(stderr, "provider matrix hover: %s\n",
                    text != ZR_NULL && *text != ZR_NULL ? test_string_ptr(*text) : "<empty>");
        }
        goto cleanup;
    }
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    *failure = "importer must publish the current generation and canonical primitive";
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        context->semanticSnapshotProviderGeneration == 0U ||
        analyzer->semanticContext->externalProviderGeneration !=
                context->semanticSnapshotProviderGeneration ||
        !module_identity_has_canonical_primitive(
                context, analyzer, uri, localPosition, expectedType)) {
        goto cleanup;
    }
    *failure = "member query must carry a complete current external identity";
    if ((!hasMemberQuery && !ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, memberPosition, &query)) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER ||
        !query.hasCanonicalSymbol ||
        query.canonicalSymbol.externalOwnerIdentity == ZR_NULL ||
        query.canonicalSymbol.externalMetadataToken == 0U ||
        query.canonicalSymbol.externalSignatureToken == 0U ||
        query.canonicalSymbol.externalSignatureHash == 0U ||
        query.canonicalSymbol.externalProviderGeneration !=
                context->semanticSnapshotProviderGeneration) {
        goto cleanup;
    }
    *failure = "definition must remain in the selected project's provider";
    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 2U);
    if (!ZrLanguageServer_LspSemanticQuery_AppendDefinitions(
                state, context, &query, &definitions) || definitions.length != 1U) {
        goto cleanup;
    }
    location = *(SZrLspLocation **)ZrCore_Array_Get(&definitions, 0U);
    if (location == ZR_NULL || location->uri == ZR_NULL ||
        !ZrCore_String_Equal(location->uri, providerUri)) {
        goto cleanup;
    }
    *failure = "references must contain only this project's exact member range";
    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 2U);
    if (!ZrLanguageServer_LspSemanticQuery_AppendReferences(
                state, context, &query, ZR_FALSE, &references) || references.length != 1U ||
        !location_array_contains_uri_and_range(
                &references, uri, memberPosition.line, memberPosition.character,
                memberPosition.line, memberPosition.character + (TZrInt32)memberLength)) {
        goto cleanup;
    }
    *failure = "lazy analysis must respect the configured semantic cache budget";
    if (!ZrLanguageServer_Lsp_GetSemanticCacheStorageInfo(context, &storageInfo) ||
        storageInfo.storageBytes > storageInfo.limitBytes) {
        goto cleanup;
    }
    *failure = "the admitted snapshot must stay valid across current-generation queries";
    valid = snapshot == ZR_NULL ||
            ZrLanguageServer_LspSemanticSnapshot_Validate(state, context, snapshot);

cleanup:
    if (snapshot != ZR_NULL) {
        ZrLanguageServer_LspSemanticSnapshot_Release(state, snapshot);
    }
    ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
    provider_matrix_free_locations(state, &references);
    provider_matrix_free_locations(state, &definitions);
    if (hover != ZR_NULL) {
        ZrCore_Array_Free(state, &hover->contents);
        ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    return valid;
}

static void test_lsp_multi_project_provider_generation(SZrState *state, TZrBool native) {
    const TZrChar *summary = native
            ? "LSP Native Provider Reload Preserves Other Project Facts"
            : "LSP Binary Provider Reload Preserves Other Project Facts";
    const TZrChar *content = native
            ? "var provider = import(\"zr.pluginprobe\");\n"
              "var cached = provider.answer();\nreturn cached;\n"
            : "var provider = import(\"graph_binary_stage\");\n"
              "var cached = provider.binarySeed();\nreturn cached;\n";
    const TZrChar *member = native ? "answer" : "binarySeed";
    const TZrChar *binarySources[] = {
        "pub var binarySeed = fn(): int => 40;\n",
        "pub var binarySeed = fn(): float => 40.5;\n"
    };
    SZrGeneratedBinaryMetadataFixture binaryFixtures[2];
    SZrGeneratedDescriptorPluginFixture nativeFixtures[2];
    SZrString *uris[2] = {ZR_NULL, ZR_NULL};
    SZrString *providerUris[2] = {ZR_NULL, ZR_NULL};
    SZrLspContext *context = ZR_NULL;
    SZrLspSemanticSnapshot *snapshot = ZR_NULL;
    SZrLspPosition memberPosition;
    SZrLspPosition localPosition;
    SZrTestTimer timer;
    TZrChar fixtureName[96];
    const TZrChar *failure = "two-project fixture preparation";
    const TZrChar *phase = "initial";
    TZrBool valid = ZR_FALSE;
    TZrSize project = 0U;
    TZrSize round = 0U;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL ||
        !lsp_find_position_for_substring(content, member, 0U, 0, &memberPosition) ||
        !lsp_find_position_for_substring(content, "cached", 1U, 0, &localPosition)) {
        goto cleanup;
    }
    for (project = 0U; project < 2U; project++) {
        const TZrChar *mainPath;
        const TZrChar *providerPath;
        snprintf(fixtureName, sizeof(fixtureName), "provider_generation_%s_project_%u",
                 native ? "native" : "binary", (unsigned int)project);
        if (native) {
            if (!prepare_generated_descriptor_plugin_fixture(
                        fixtureName, ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH,
                        &nativeFixtures[project])) {
                goto cleanup;
            }
            mainPath = nativeFixtures[project].mainPath;
            providerPath = nativeFixtures[project].pluginPath;
        } else {
            if (!prepare_generated_binary_metadata_fixture(state, fixtureName,
                                                          &binaryFixtures[project])) {
                goto cleanup;
            }
            mainPath = binaryFixtures[project].mainPath;
            providerPath = binaryFixtures[project].binaryPath;
        }
        if (!write_text_file(mainPath, content, strlen(content))) {
            goto cleanup;
        }
        uris[project] = create_file_uri_from_native_path(state, mainPath);
        providerUris[project] = create_file_uri_from_native_path(state, providerPath);
        if (uris[project] == ZR_NULL || providerUris[project] == ZR_NULL ||
            !ZrLanguageServer_Lsp_UpdateDocument(
                    state, context, uris[project], content, strlen(content), 1U)) {
            goto cleanup;
        }
    }
    for (project = 0U; project < 2U; project++) {
        if (!provider_matrix_check_project(state, context, uris[project],
                    providerUris[project], memberPosition, localPosition,
                    strlen(member), ZR_VALUE_TYPE_INT64, 0U, &failure)) {
            goto cleanup;
        }
    }
    phase = "reload";
    for (round = 0U; round < 8U; round++) {
        TZrBool floating = round % 2U == 0U;
        TZrUInt64 generation = context->semanticSnapshotProviderGeneration;
        failure = "zero-budget queries must preserve canonical facts after cache eviction";
        if (round == 4U &&
            !ZrLanguageServer_Lsp_SetSemanticCacheStorageLimit(state, context, 0U)) {
            goto cleanup;
        }
        failure = "fresh project snapshot must validate before reload";
        snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(state, context, uris[0]);
        if (snapshot == ZR_NULL ||
            !ZrLanguageServer_LspSemanticSnapshot_Validate(state, context, snapshot)) {
            goto cleanup;
        }
        failure = "replace only the first project's provider";
        if (native) {
            if (!copy_fixture_binary_file(floating
                        ? ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_FLOAT_PATH
                        : ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH,
                        nativeFixtures[0].pluginPath)) {
                goto cleanup;
            }
        } else if (!regenerate_binary_metadata_fixture_artifacts(
                    state, &binaryFixtures[0], binarySources[floating ? 1U : 0U])) {
            goto cleanup;
        }
        if (!ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(
                    state, context, providerUris[0])) {
            goto cleanup;
        }
        failure = "real reload must advance generation and reject the previous snapshot";
        if (context->semanticSnapshotProviderGeneration <= generation ||
            ZrLanguageServer_LspSemanticSnapshot_Validate(state, context, snapshot)) {
            goto cleanup;
        }
        ZrLanguageServer_LspSemanticSnapshot_Release(state, snapshot);
        snapshot = ZR_NULL;
        for (project = 0U; project < 2U; project++) {
            EZrValueType expected = project == 0U && floating
                    ? ZR_VALUE_TYPE_DOUBLE : ZR_VALUE_TYPE_INT64;
            if (!provider_matrix_check_project(state, context, uris[project],
                        providerUris[project], memberPosition, localPosition,
                        strlen(member), expected, round % 5U, &failure)) {
                goto cleanup;
            }
        }
    }
    valid = ZR_TRUE;

cleanup:
    if (snapshot != ZR_NULL) {
        ZrLanguageServer_LspSemanticSnapshot_Release(state, snapshot);
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (valid) {
        TEST_PASS(timer, summary);
    } else {
        TZrChar detail[256];
        snprintf(detail, sizeof(detail), "%s round=%u project=%u: %s",
                 phase, (unsigned int)round, (unsigned int)project, failure);
        TEST_FAIL(timer, summary, detail);
    }
}

#endif
