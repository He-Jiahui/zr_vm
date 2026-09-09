#ifndef ZR_TEST_LSP_NATIVE_VIRTUAL_FIXTURE_H
#define ZR_TEST_LSP_NATIVE_VIRTUAL_FIXTURE_H

#include "../../zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.h"

static SZrString *test_native_virtual_document_uri(SZrState *state, SZrLspContext *context,
                                                  SZrString *mainUri, SZrString *originUri) {
    SZrLspProjectIndex *project = test_find_project_for_uri(context, mainUri);
    SZrLspVirtualDocumentIdentity identity = {0};
    if (project == ZR_NULL) return ZR_NULL;
    identity.moduleName = ZrCore_String_Create(state, "zr.pluginprobe", 14U);
    identity.projectUri = project->projectFileUri;
    identity.originUri = originUri;
    identity.providerGeneration = context->semanticSnapshotProviderGeneration;
    return ZrLanguageServer_LspVirtualDocumentIdentity_Create(state, &identity);
}

#endif
