#ifndef ZR_TEST_LSP_VIRTUAL_DOCUMENT_IDENTITY_CASES_H
#define ZR_TEST_LSP_VIRTUAL_DOCUMENT_IDENTITY_CASES_H

#include "../../zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.h"
#include "../../zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.h"
#include "../../zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h"
#include <stdint.h>

static void test_virtual_document_identity_round_trip(SZrState *state) {
    const TZrChar *summary = "LSP Virtual Identity Preserves Reserved Bytes And Uint64 Generation";
    SZrParityTimer timer;
    SZrLspVirtualDocumentIdentity identity = {0};
    SZrLspVirtualDocumentIdentity decoded;
    SZrString *uri;
    TZrBool passed;
    TEST_START(summary);
    identity.moduleName = ZrCore_String_CreateFromNative(state, "zr.test.\xF0\xA0\x80\x80");
    identity.projectUri = ZrCore_String_CreateFromNative(state, "file:///tmp/project & #%\".zrp");
    identity.originUri = ZrCore_String_CreateFromNative(state, "file:///tmp/native & #%\".so");
    identity.providerGeneration = UINT64_MAX;
    uri = ZrLanguageServer_LspVirtualDocumentIdentity_Create(state, &identity);
    passed = uri != ZR_NULL &&
            ZrLanguageServer_LspVirtualDocumentIdentity_Parse(state, uri, &decoded) &&
            ZrCore_String_Equal(identity.moduleName, decoded.moduleName) &&
            ZrCore_String_Equal(identity.projectUri, decoded.projectUri) &&
            ZrCore_String_Equal(identity.originUri, decoded.originUri) &&
            decoded.providerGeneration == UINT64_MAX;
    if (passed) TEST_PASS(timer, summary);
    else TEST_FAIL(timer, summary, "URI encoding must preserve every identity byte and all 64 generation bits");
}

static void test_virtual_document_identity_rejects_malformed(SZrState *state) {
    const TZrChar *summary = "LSP Virtual Identity Rejects Malformed Or Incomplete Scope";
    static TZrChar *invalid[] = {
        "zr-decompiled:/a.zr?%",
        "zr-decompiled:/a.zr?%0X",
        "zr-decompiled:/a.zr?%00",
        "zr-decompiled:/a.zr?{}",
        "zr-decompiled:/.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":\"1\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":\"0\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":\"-1\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":\"18446744073709551616\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":1}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"project\":\"q\",\"generation\":\"1\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\\u0000hidden\",\"origin\":\"o\",\"generation\":\"1\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":\"1\"}trailing"
    };
    SZrParityTimer timer;
    TZrBool passed = ZR_TRUE;
    TEST_START(summary);
    for (TZrSize index = 0U; index < sizeof(invalid) / sizeof(invalid[0]); index++) {
        SZrLspVirtualDocumentIdentity decoded;
        memset(&decoded, 0xA5, sizeof(decoded));
        if (ZrLanguageServer_LspVirtualDocumentIdentity_Parse(state,
                    ZrCore_String_CreateFromNative(state, invalid[index]), &decoded) ||
            decoded.moduleName != ZR_NULL || decoded.projectUri != ZR_NULL ||
            decoded.originUri != ZR_NULL || decoded.providerGeneration != 0U) {
            fprintf(stderr, "malformed virtual URI case %u was not rejected cleanly\n", (unsigned int)index);
            passed = ZR_FALSE;
        }
    }
    if (passed) TEST_PASS(timer, summary);
    else TEST_FAIL(timer, summary, "malformed scopes must fail with every output cleared");
}

static void test_binary_virtual_document_identity_is_project_scoped(SZrState *state) {
    const TZrChar *summary = "LSP Binary Virtual Identity Is Project Scoped And Generation Bound";
    SZrParityTimer timer;
    SZrParityBinaryFixture fixture = {0};
    SZrLspContext *context = ZR_NULL;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrAstNode *savedAst = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *moduleName = ZR_NULL;
    SZrString *binaryUri = ZR_NULL;
    SZrString *virtualUri = ZR_NULL;
    SZrString *resolvedVirtualUri = ZR_NULL;
    SZrString *refreshedVirtualUri = ZR_NULL;
    SZrLspProjectIndex *project = ZR_NULL;
    SZrLspMetadataProvider provider;
    SZrLspResolvedImportedModuleEntry entry;
    SZrLspVirtualDocumentIdentity identity;
    SZrSemanticRelationFact *originFact = ZR_NULL;
    SZrLspSemanticQuery query;
    SZrLspPosition aliasUsePosition;
    TZrChar *content = ZR_NULL;
    TZrSize contentLength = 0U;
    TZrBool queryInitialized = ZR_FALSE;
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    memset(&entry, 0, sizeof(entry));
    memset(&identity, 0, sizeof(identity));
    memset(&query, 0, sizeof(query));
    if (!prepare_binary_fixture(state, &fixture) ||
        (content = ZrTests_ReadTextFile(fixture.mainPath, &contentLength)) == ZR_NULL ||
        (context = ZrLanguageServer_LspContext_New(state)) == ZR_NULL ||
        (mainUri = create_file_uri(state, fixture.mainPath)) == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, content, contentLength, 1U) ||
        (project = ZrLanguageServer_LspProject_FindProjectForUri(context, mainUri)) == ZR_NULL ||
        (moduleName = ZrCore_String_CreateFromNative(state, "semantic_query_provider")) == ZR_NULL ||
        (analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, mainUri)) == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || analyzer->ast == ZR_NULL ||
        !find_position(content, "binary.binarySeed()", 0U, 1, &aliasUsePosition)) {
        goto cleanup;
    }

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    if (!ZrLanguageServer_LspMetadataProvider_ResolveImportedModuleEntry(
                &provider, ZR_NULL, project, moduleName, &entry) ||
        !entry.hasDeclaration ||
        entry.module.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA ||
        (binaryUri = entry.declarationUri) == ZR_NULL ||
        (virtualUri = entry.virtualDeclarationUri) == ZR_NULL ||
        !ZrLanguageServer_LspVirtualDocumentIdentity_ResolveBinaryUri(
                state, context, project, moduleName, &resolvedVirtualUri) ||
        resolvedVirtualUri == ZR_NULL ||
        !ZrCore_String_Equal(virtualUri, resolvedVirtualUri) ||
        ZrCore_String_Equal(binaryUri, virtualUri) ||
        !ZrLanguageServer_LspVirtualDocumentIdentity_IsScoped(virtualUri) ||
        !ZrLanguageServer_LspVirtualDocumentIdentity_Parse(state, virtualUri, &identity) ||
        !ZrCore_String_Equal(identity.moduleName, moduleName) ||
        !ZrCore_String_Equal(identity.projectUri, project->projectFileUri) ||
        !ZrCore_String_Equal(identity.originUri, binaryUri) ||
        identity.providerGeneration == 0U ||
        identity.providerGeneration != context->semanticSnapshotProviderGeneration) {
        goto cleanup;
    }

    /* The parser relation owns the virtual identity, while the established
     * source-backed binary navigation result keeps the physical .zro URI. */
    savedAst = analyzer->ast;
    analyzer->ast = ZR_NULL;
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    queryInitialized = ZR_TRUE;
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, mainUri, aliasUsePosition, &query) ||
        !query.hasCanonicalSymbol ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION ||
        query.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA ||
        query.resolvedMember.declarationUri == ZR_NULL ||
        !ZrCore_String_Equal(query.resolvedMember.declarationUri, binaryUri) ||
        query.resolvedMember.declarationRange.source == ZR_NULL ||
        !ZrCore_String_Equal(query.resolvedMember.declarationRange.source, binaryUri)) {
        goto cleanup;
    }
    analyzer->ast = savedAst;
    savedAst = ZR_NULL;

    for (TZrSize index = 0U;
         index < analyzer->semanticContext->relationFacts.length;
         index++) {
        SZrSemanticRelationFact *candidate =
                (SZrSemanticRelationFact *)ZrCore_Array_Get(
                        &analyzer->semanticContext->relationFacts, index);
        if (candidate != ZR_NULL &&
            candidate->kind == ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN &&
            candidate->sourceSymbolId == query.canonicalSymbol.symbolId) {
            originFact = candidate;
            break;
        }
    }
    if (originFact == ZR_NULL || !originFact->hasSourceRange ||
        originFact->externalOriginUri == ZR_NULL ||
        !ZrCore_String_Equal(originFact->externalOriginUri, moduleName) ||
        originFact->virtualDeclarationUri == ZR_NULL ||
        !ZrCore_String_Equal(originFact->virtualDeclarationUri, virtualUri) ||
        ZrLanguageServer_LspVirtualDocumentIdentity_FindProject(context, virtualUri) != project) {
        goto cleanup;
    }

    ZrLanguageServer_LspSemanticSnapshot_ProviderChanged(context);
    if (ZrLanguageServer_LspVirtualDocumentIdentity_FindProject(context, virtualUri) != ZR_NULL ||
        !ZrLanguageServer_LspVirtualDocumentIdentity_ResolveBinaryUri(
                state, context, project, moduleName, &refreshedVirtualUri) ||
        refreshedVirtualUri == ZR_NULL ||
        ZrCore_String_Equal(refreshedVirtualUri, virtualUri) ||
        !ZrLanguageServer_LspVirtualDocumentIdentity_Parse(
                state, refreshedVirtualUri, &identity) ||
        identity.providerGeneration != context->semanticSnapshotProviderGeneration) {
        goto cleanup;
    }
    passed = ZR_TRUE;

cleanup:
    if (savedAst != ZR_NULL && analyzer != ZR_NULL) {
        analyzer->ast = savedAst;
    }
    if (queryInitialized) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    }
    free(content);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (passed) {
        TEST_PASS(timer, summary);
    } else {
        TEST_FAIL(timer, summary,
                  "binary metadata must publish a generation-bound parser relation identity without replacing physical .zro navigation");
    }
}

#endif
