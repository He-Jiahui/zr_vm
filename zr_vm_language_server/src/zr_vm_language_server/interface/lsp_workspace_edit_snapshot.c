#include "interface/lsp_interface_internal.h"

#include "zr_vm_core/hash.h"
#include "zr_vm_library/file.h"

#include <string.h>

static TZrBool workspace_edit_capture_disk_snapshot(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspWorkspaceEditDocumentSnapshot *outSnapshot) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot cachedSnapshot = {0};
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrNativeString content;
    TZrSize contentLength;
    TZrUInt64 contentHash;
    TZrBool cacheMatches = ZR_TRUE;

    if (state == ZR_NULL || state->global == ZR_NULL || context == ZR_NULL ||
        uri == ZR_NULL || outSnapshot == ZR_NULL ||
        !ZrLanguageServer_Lsp_FileUriToNativePath(
                uri, nativePath, sizeof(nativePath))) {
        return ZR_FALSE;
    }

    content = ZrLibrary_File_ReadAll(state->global, nativePath);
    if (content == ZR_NULL) {
        return ZR_FALSE;
    }
    contentLength = strlen(content);
    contentHash = ZrCore_Hash_CreateStable64(
            (const TZrByte *)content, contentLength);

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion != ZR_NULL && fileVersion->isOpenDocument) {
        cacheMatches = ZR_FALSE;
    } else if (fileVersion != ZR_NULL) {
        cacheMatches = ZrLanguageServer_FileVersionContentSnapshot_Acquire(
                state, fileVersion, &cachedSnapshot);
        if (cacheMatches) {
            cacheMatches = cachedSnapshot.contentLength == contentLength &&
                           ZrCore_Hash_CreateStable64(
                                   (const TZrByte *)cachedSnapshot.content,
                                   cachedSnapshot.contentLength) == contentHash;
            ZrLanguageServer_FileVersionContentSnapshot_Free(
                    state, &cachedSnapshot);
        }
    }

    if (cacheMatches) {
        memset(outSnapshot, 0, sizeof(*outSnapshot));
        outSnapshot->uri = uri;
        outSnapshot->contentHash = contentHash;
        outSnapshot->contentLength = contentLength;
        outSnapshot->isOpenDocument = ZR_FALSE;
    }
    ZrCore_Memory_RawFreeWithType(
            state->global,
            content,
            contentLength + 1U,
            ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return cacheMatches;
}

static TZrBool workspace_edit_capture_document_snapshot(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspWorkspaceEditDocumentSnapshot *outSnapshot) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot contentSnapshot = {0};

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        outSnapshot == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || !fileVersion->isOpenDocument) {
        return workspace_edit_capture_disk_snapshot(
                state, context, uri, outSnapshot);
    }
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(
                state, fileVersion, &contentSnapshot) ||
        !contentSnapshot.isOpenDocument) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(
                state, &contentSnapshot);
        return ZR_FALSE;
    }

    memset(outSnapshot, 0, sizeof(*outSnapshot));
    outSnapshot->uri = uri;
    outSnapshot->contentHash = ZrCore_Hash_CreateStable64(
            (const TZrByte *)contentSnapshot.content,
            contentSnapshot.contentLength);
    outSnapshot->contentLength = contentSnapshot.contentLength;
    outSnapshot->version = contentSnapshot.version;
    outSnapshot->contentGeneration = contentSnapshot.contentGeneration;
    outSnapshot->isOpenDocument = ZR_TRUE;
    ZrLanguageServer_FileVersionContentSnapshot_Free(
            state, &contentSnapshot);
    return ZR_TRUE;
}

const SZrLspWorkspaceEditDocumentSnapshot *
ZrLanguageServer_LspWorkspaceEdit_FindDocumentSnapshot(
        const SZrArray *documentSnapshots,
        SZrString *uri) {
    if (documentSnapshots == ZR_NULL || !documentSnapshots->isValid ||
        uri == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0U; index < documentSnapshots->length; index++) {
        const SZrLspWorkspaceEditDocumentSnapshot *snapshot =
                (const SZrLspWorkspaceEditDocumentSnapshot *)ZrCore_Array_Get(
                        (SZrArray *)documentSnapshots, index);
        if (snapshot != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual(snapshot->uri, uri)) {
            return snapshot;
        }
    }
    return ZR_NULL;
}

TZrBool ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshots(
        SZrState *state,
        SZrLspContext *context,
        const SZrArray *locations,
        SZrArray *outDocumentSnapshots) {
    if (state == ZR_NULL || context == ZR_NULL || locations == ZR_NULL ||
        outDocumentSnapshots == ZR_NULL || outDocumentSnapshots->length != 0U) {
        return ZR_FALSE;
    }
    if (!outDocumentSnapshots->isValid) {
        ZrCore_Array_Init(
                state,
                outDocumentSnapshots,
                sizeof(SZrLspWorkspaceEditDocumentSnapshot),
                ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    for (TZrSize index = 0U; index < locations->length; index++) {
        SZrLspLocation **locationPtr =
                (SZrLspLocation **)ZrCore_Array_Get(
                        (SZrArray *)locations, index);
        SZrLspWorkspaceEditDocumentSnapshot snapshot;

        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL ||
            (*locationPtr)->uri == ZR_NULL) {
            return ZR_FALSE;
        }
        if (ZrLanguageServer_LspWorkspaceEdit_FindDocumentSnapshot(
                    outDocumentSnapshots, (*locationPtr)->uri) != ZR_NULL) {
            continue;
        }
        if (!workspace_edit_capture_document_snapshot(
                    state, context, (*locationPtr)->uri, &snapshot)) {
            return ZR_FALSE;
        }
        ZrCore_Array_Push(state, outDocumentSnapshots, &snapshot);
    }
    return outDocumentSnapshots->length > 0U;
}

TZrBool ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshots(
        SZrState *state,
        SZrLspContext *context,
        const SZrArray *documentSnapshots) {
    if (state == ZR_NULL || context == ZR_NULL || documentSnapshots == ZR_NULL ||
        !documentSnapshots->isValid || documentSnapshots->length == 0U) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0U; index < documentSnapshots->length; index++) {
        const SZrLspWorkspaceEditDocumentSnapshot *expected =
                (const SZrLspWorkspaceEditDocumentSnapshot *)ZrCore_Array_Get(
                        (SZrArray *)documentSnapshots, index);
        SZrLspWorkspaceEditDocumentSnapshot current;

        if (expected == ZR_NULL || expected->uri == ZR_NULL ||
            !workspace_edit_capture_document_snapshot(
                    state, context, expected->uri, &current) ||
            current.isOpenDocument != expected->isOpenDocument ||
            current.contentHash != expected->contentHash ||
            current.contentLength != expected->contentLength ||
            current.version != expected->version ||
            current.contentGeneration != expected->contentGeneration) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
