#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_METADATA_IDENTITY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_METADATA_IDENTITY_H

#include "metadata/lsp_metadata_provider.h"

typedef struct SZrLspExternalMetadataIdentityDeclaration {
    SZrString *uri;
    SZrFileRange range;
    EZrLspImportedModuleSourceKind sourceKind;
} SZrLspExternalMetadataIdentityDeclaration;

TZrBool ZrLanguageServer_LspExternalMetadataIdentity_ResolveDeclaration(
        SZrLspMetadataProvider *provider,
        SZrSemanticAnalyzer *analyzer,
        SZrLspProjectIndex *projectIndex,
        const SZrParserSemanticExternalReferenceQuery *identity,
        SZrLspExternalMetadataIdentityDeclaration *outDeclaration);

#endif
