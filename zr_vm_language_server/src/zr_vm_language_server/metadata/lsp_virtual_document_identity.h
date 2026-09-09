#ifndef ZR_LSP_VIRTUAL_DOCUMENT_IDENTITY_H
#define ZR_LSP_VIRTUAL_DOCUMENT_IDENTITY_H

#include "module/lsp_module_metadata.h"

typedef struct SZrLspVirtualDocumentIdentity {
    SZrString *moduleName;
    SZrString *projectUri;
    SZrString *originUri;
    TZrUInt64 providerGeneration;
} SZrLspVirtualDocumentIdentity;

/* Identity strings are GC-owned; descriptors remain borrowed from the live provider. */
ZR_LANGUAGE_SERVER_API SZrString *ZrLanguageServer_LspVirtualDocumentIdentity_Create(
        SZrState *state, const SZrLspVirtualDocumentIdentity *identity);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_Parse(
        SZrState *state, SZrString *uri, SZrLspVirtualDocumentIdentity *outIdentity);
TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_IsScoped(SZrString *uri);
TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeUri(
        SZrState *state, SZrLspContext *context, SZrLspProjectIndex *projectIndex,
        SZrString *moduleName, SZrString **outUri);
TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeDescriptor(
        SZrState *state, SZrLspContext *context, SZrString *uri,
        SZrLspVirtualDocumentIdentity *outIdentity, SZrLspProjectIndex **outProject,
        const ZrLibModuleDescriptor **outDescriptor);
SZrLspProjectIndex *ZrLanguageServer_LspVirtualDocumentIdentity_FindProject(
        SZrLspContext *context, SZrString *uri);

#endif
