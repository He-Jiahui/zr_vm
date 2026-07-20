#ifndef ZR_VM_LANGUAGE_SERVER_LSP_WORKSPACE_EDIT_SNAPSHOT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_WORKSPACE_EDIT_SNAPSHOT_H

#include "zr_vm_language_server/lsp_interface.h"

#ifndef ZR_LSP_WORKSPACE_EDIT_DOCUMENT_SNAPSHOT_DEFINED
#define ZR_LSP_WORKSPACE_EDIT_DOCUMENT_SNAPSHOT_DEFINED
typedef struct SZrLspWorkspaceEditDocumentSnapshot {
    SZrString *uri;
    TZrUInt64 contentHash;
    TZrSize contentLength;
    TZrSize version;
    TZrSize contentGeneration;
    TZrBool isOpenDocument;
} SZrLspWorkspaceEditDocumentSnapshot;
#endif

typedef SZrLspWorkspaceEditDocumentSnapshot
        SZrLspSourceRenameDocumentSnapshot;

ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshot(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspWorkspaceEditDocumentSnapshot *outDocumentSnapshot);
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshot(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspWorkspaceEditDocumentSnapshot *documentSnapshot);
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshots(
        SZrState *state,
        SZrLspContext *context,
        const SZrArray *locations,
        SZrArray *outDocumentSnapshots);
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshots(
        SZrState *state,
        SZrLspContext *context,
        const SZrArray *documentSnapshots);
ZR_LANGUAGE_SERVER_API const SZrLspWorkspaceEditDocumentSnapshot *
ZrLanguageServer_LspWorkspaceEdit_FindDocumentSnapshot(
        const SZrArray *documentSnapshots,
        SZrString *uri);

#endif
