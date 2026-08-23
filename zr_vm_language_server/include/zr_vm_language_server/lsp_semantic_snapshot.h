// Immutable identity and dependency fence for one semantic LSP request.

#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_SNAPSHOT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_SNAPSHOT_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

typedef struct SZrLspContext SZrLspContext;
typedef struct SZrLspSemanticSnapshot SZrLspSemanticSnapshot;

typedef struct SZrLspSemanticSnapshotIdentity {
    TZrUInt64 documentGeneration;
    TZrUInt64 projectGeneration;
    TZrUInt64 providerGeneration;
    TZrUInt64 semanticGeneration;
    TZrUInt64 dependencyFingerprint;
} SZrLspSemanticSnapshotIdentity;

/*
 * Acquire pins the document content for the caller's semantic work. The
 * implementation keeps parser and analyzer details private; callers must
 * validate before publishing a response and then release the snapshot.
 */
ZR_LANGUAGE_SERVER_API SZrLspSemanticSnapshot *
ZrLanguageServer_LspSemanticSnapshot_Acquire(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticSnapshot_Release(
        SZrState *state,
        SZrLspSemanticSnapshot *snapshot);
ZR_LANGUAGE_SERVER_API const SZrLspSemanticSnapshotIdentity *
ZrLanguageServer_LspSemanticSnapshot_GetIdentity(
        const SZrLspSemanticSnapshot *snapshot);
ZR_LANGUAGE_SERVER_API const TZrChar *
ZrLanguageServer_LspSemanticSnapshot_Content(
        const SZrLspSemanticSnapshot *snapshot);
ZR_LANGUAGE_SERVER_API TZrSize ZrLanguageServer_LspSemanticSnapshot_ContentLength(
        const SZrLspSemanticSnapshot *snapshot);
/* Semantic response IDs share this identity; payload length keeps delta edits valid. */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticSnapshot_FormatResultId(
        const SZrLspSemanticSnapshot *snapshot,
        TZrSize payloadLength,
        TZrChar *buffer,
        TZrSize bufferLength);

/* Record only modules/documents actually read by the request. */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticSnapshot_TrackDependency(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticSnapshot *snapshot,
        SZrString *uri);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticSnapshot_Validate(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspSemanticSnapshot *snapshot);
/* Active request snapshots collect cross-document analyzer reads. */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticSnapshot_SetActive(
        SZrLspContext *context,
        SZrLspSemanticSnapshot *snapshot);
ZR_LANGUAGE_SERVER_API SZrLspSemanticSnapshot *
ZrLanguageServer_LspSemanticSnapshot_GetActive(const SZrLspContext *context);

/* Metadata reload paths advance this view without exposing provider internals. */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticSnapshot_ProviderChanged(
        SZrLspContext *context);

#endif
