#include "semantic/lsp_semantic_relation_query.h"

#include "zr_vm_parser/semantic_query.h"

#include <string.h>

static TZrBool semantic_relation_query_is_import_origin(
        const SZrParserSemanticRelationQuery *relation,
        const SZrParserSemanticSymbolQuery *symbol) {
    return relation != ZR_NULL && symbol != ZR_NULL &&
           relation->kind == ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN &&
           relation->isExternal && relation->externalOriginUri != ZR_NULL &&
           relation->sourceSymbolId == symbol->symbolId &&
           relation->sourceTypeId == symbol->typeId &&
           relation->targetTypeId == symbol->typeId;
}

EZrLspSemanticImportOriginResolution
ZrLanguageServer_LspSemanticRelationQuery_ResolveImportOrigin(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        SZrSemanticAnalyzer *analyzer,
        const SZrParserSemanticSymbolQuery *symbol,
        SZrLspSemanticImportOriginTarget *outTarget) {
    SZrArray relations = {0};
    const SZrParserSemanticRelationQuery *origin = ZR_NULL;
    SZrLspMetadataProvider provider;
    TZrSize index;

    if (outTarget != ZR_NULL) {
        memset(outTarget, 0, sizeof(*outTarget));
    }
    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || outTarget == ZR_NULL) {
        return ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID;
    }
    if (symbol == ZR_NULL || symbol->symbolId == ZR_SEMANTIC_ID_INVALID ||
        symbol->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_LSP_SEMANTIC_IMPORT_ORIGIN_NOT_APPLICABLE;
    }
    if (!symbol->isImport) {
        return ZR_LSP_SEMANTIC_IMPORT_ORIGIN_NOT_APPLICABLE;
    }
    if (symbol->externalOriginUri == ZR_NULL) {
        return ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID;
    }
    if (!ZrParser_SemanticQuery_RelationsOfSymbol(
                analyzer->semanticContext, symbol->symbolId, ZR_NULL, &relations)) {
        if (relations.isValid) {
            ZrCore_Array_Free(state, &relations);
        }
        return ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID;
    }

    for (index = 0U; index < relations.length; index++) {
        const SZrParserSemanticRelationQuery *candidate =
                (const SZrParserSemanticRelationQuery *)ZrCore_Array_Get(
                        &relations, index);

        if (!semantic_relation_query_is_import_origin(candidate, symbol)) {
            continue;
        }
        if (origin != ZR_NULL) {
            ZrCore_Array_Free(state, &relations);
            return ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID;
        }
        origin = candidate;
    }

    if (origin == ZR_NULL) {
        ZrCore_Array_Free(state, &relations);
        return ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID;
    }

    if (!ZrCore_String_Equal(symbol->externalOriginUri,
                             origin->externalOriginUri)) {
        ZrCore_Array_Free(state, &relations);
        return ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID;
    }

    outTarget->originIdentity = origin->externalOriginUri;
    outTarget->virtualDeclarationUri = origin->virtualDeclarationUri;
    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    if (!ZrLanguageServer_LspMetadataProvider_ResolveImportedModuleEntry(
                &provider,
                analyzer,
                projectIndex,
                outTarget->originIdentity,
                &outTarget->declaration) ||
        !outTarget->declaration.hasDeclaration ||
        outTarget->declaration.declarationUri == ZR_NULL ||
        (outTarget->virtualDeclarationUri != ZR_NULL &&
         !ZrCore_String_Equal(outTarget->virtualDeclarationUri,
                             outTarget->declaration.declarationUri))) {
        memset(outTarget, 0, sizeof(*outTarget));
        ZrCore_Array_Free(state, &relations);
        return ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID;
    }

    outTarget->module = outTarget->declaration.module;
    ZrCore_Array_Free(state, &relations);
    return ZR_LSP_SEMANTIC_IMPORT_ORIGIN_RESOLVED;
}
