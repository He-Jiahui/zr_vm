#include "interface/lsp_interface_internal.h"

#include "zr_vm_core/hash.h"
#include "zr_vm_library/file.h"

#include <string.h>

/* 编辑令牌除文本外还绑定依赖与提供者代数，防止同文重算后沿用旧位置。 */
static void workspace_edit_capture_semantic_identity(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspWorkspaceEditDocumentSnapshot *outSnapshot) {
    SZrLspSemanticSnapshot *semanticSnapshot;
    const SZrLspSemanticSnapshotIdentity *identity;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outSnapshot == ZR_NULL) {
        return;
    }
    semanticSnapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(state, context, uri);
    if (semanticSnapshot == ZR_NULL) {
        /* TODO: 确认语义快照暂时不可用时是否允许继续发布工作区编辑；
         * 当前捕获与验证都可能只比较文本，需沿 code action/rename 调用链验证依赖变化场景。 */
        return;
    }
    identity = ZrLanguageServer_LspSemanticSnapshot_GetIdentity(semanticSnapshot);
    if (identity != ZR_NULL) {
        outSnapshot->semanticIdentity = *identity;
        outSnapshot->hasSemanticIdentity = ZR_TRUE;
    }
    ZrLanguageServer_LspSemanticSnapshot_Release(state, semanticSnapshot);
}

/* 与语义快照身份字段保持同步，用于重命名和 code action 的失效判断。 */
static TZrBool workspace_edit_semantic_identities_equal(
        const SZrLspSemanticSnapshotIdentity *left,
        const SZrLspSemanticSnapshotIdentity *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->documentGeneration == right->documentGeneration &&
           left->projectGeneration == right->projectGeneration &&
           left->providerGeneration == right->providerGeneration &&
           left->semanticGeneration == right->semanticGeneration &&
           left->dependencyFingerprint == right->dependencyFingerprint;
}

/* 关闭文档以磁盘内容为准；若解析器还缓存旧内容则拒绝生成编辑。 */
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
    /* BUG: ReadAll 可返回带内嵌 NUL 的文件；strlen 只覆盖前缀。若 NUL 后内容变化，
     * ValidateDocumentSnapshot 的长度与哈希仍相同，旧的跨文件编辑可被放行。 */
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

    /* 只有磁盘与关闭文档缓存一致时，语义身份才对应这份磁盘内容。 */
    if (cacheMatches) {
        memset(outSnapshot, 0, sizeof(*outSnapshot));
        outSnapshot->uri = uri;
        outSnapshot->contentHash = contentHash;
        outSnapshot->contentLength = contentLength;
        outSnapshot->isOpenDocument = ZR_FALSE;
        workspace_edit_capture_semantic_identity(state, context, uri, outSnapshot);
    }
    ZrCore_Memory_RawFreeWithType(
            state->global,
            content,
            /* BUG: ReadAll 按磁盘字节数分配，Windows 文本模式可折叠 CRLF，
             * contentLength + 1 小于原申请尺寸，违背带类型释放接口的尺寸契约。 */
            contentLength + 1U,
            ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return cacheMatches;
}

TZrBool ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshot(
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
    /* 打开的编辑器缓冲区优先于磁盘；关闭文档必须重新读取磁盘并核对缓存。 */
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
    workspace_edit_capture_semantic_identity(state, context, uri, outSnapshot);
    ZrLanguageServer_FileVersionContentSnapshot_Free(
            state, &contentSnapshot);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshot(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspWorkspaceEditDocumentSnapshot *documentSnapshot) {
    SZrLspWorkspaceEditDocumentSnapshot current;

    if (documentSnapshot == ZR_NULL || documentSnapshot->uri == ZR_NULL ||
        !ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshot(
                state, context, documentSnapshot->uri, &current)) {
        return ZR_FALSE;
    }
    /* 文本、编辑器版本与语义身份三者均稳定时才允许发送先前生成的编辑。 */
    return current.isOpenDocument == documentSnapshot->isOpenDocument &&
            current.contentHash == documentSnapshot->contentHash &&
            current.contentLength == documentSnapshot->contentLength &&
            current.version == documentSnapshot->version &&
            current.contentGeneration == documentSnapshot->contentGeneration &&
            current.hasSemanticIdentity == documentSnapshot->hasSemanticIdentity &&
            (!current.hasSemanticIdentity ||
             workspace_edit_semantic_identities_equal(
                     &current.semanticIdentity, &documentSnapshot->semanticIdentity));
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
        /* BUG: Array_Init 分配失败仍标记为有效；后续 Array_Push 可向空指针复制，
         * 多文档重命名遇到内存不足时不能按返回值安全失败。 */
        ZrCore_Array_Init(
                state,
                outDocumentSnapshots,
                sizeof(SZrLspWorkspaceEditDocumentSnapshot),
                ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    /* 重命名的位置列表可重复引用一个文档，工作区编辑仅需每个 URI 一个令牌。 */
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
        if (!ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshot(
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
        if (!ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshot(
                    state, context, expected)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
