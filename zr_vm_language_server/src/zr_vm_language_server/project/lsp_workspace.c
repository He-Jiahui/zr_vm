#include "project/lsp_workspace.h"

#include "interface/lsp_interface_internal.h"
#include "project/lsp_project_internal.h"
#include "zr_vm_language_server/lsp_uri.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <ctype.h>
#include <string.h>

/* 根集合只保存经文件 URI 往返规范化的 SZrString*；字符串由 VM 状态管理，数组缓冲由 workspace 持有。 */
struct SZrLspWorkspace {
    SZrArray rootUris; /* SZrString*, canonical file URIs */
};

/* 为根目录与项目记录比较借用 VM 字符串内容；调用方不得跨字符串生命周期保存返回指针。 */
static const TZrChar *workspace_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

/* 比较时统一分隔符与平台大小写，避免同一根目录因客户端 URI 拼写差异被重复处理。 */
static void workspace_normalize_path(const TZrChar *source, TZrChar *target, TZrSize targetSize) {
    TZrSize length = 0;

    if (target == ZR_NULL || targetSize == 0) {
        return;
    }
    target[0] = '\0';
    if (source == ZR_NULL) {
        return;
    }

    while (*source != '\0' && length + 1 < targetSize) {
        TZrChar value = *source++;
        if (value == '\\') {
            value = '/';
        }
#ifdef ZR_VM_PLATFORM_IS_WIN
        value = (TZrChar)tolower((unsigned char)value);
#endif
        target[length++] = value;
    }

    while (length > 1 && target[length - 1] == '/') {
#ifdef ZR_VM_PLATFORM_IS_WIN
        if (length == 3 && target[1] == ':') {
            break;
        }
#endif
        length--;
    }
    target[length] = '\0';
}

/* 以目录边界判断归属，防止相同字符串前缀的兄弟目录接收文件事件。 */
static TZrBool workspace_path_is_within_root(const TZrChar *candidatePath, const TZrChar *rootPath) {
    TZrChar normalizedCandidate[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize rootLength;

    workspace_normalize_path(candidatePath, normalizedCandidate, sizeof(normalizedCandidate));
    workspace_normalize_path(rootPath, normalizedRoot, sizeof(normalizedRoot));
    rootLength = strlen(normalizedRoot);

    /* BUG: file:///workspace/../outside.zr 仍通过 /workspace 的词法前缀检查；
     * FileToNativePath 保留 '..'，didChangeWatchedFiles 因此可越过工作区边界读取磁盘文件。
     * 证据：lsp_uri.c 的解码路径与 stdio_workspace_files.c 的事件读取调用链。 */
    if (rootLength == 0 || strncmp(normalizedCandidate, normalizedRoot, rootLength) != 0) {
        return ZR_FALSE;
    }

    return normalizedCandidate[rootLength] == '\0' || normalizedRoot[rootLength - 1] == '/' ||
           normalizedCandidate[rootLength] == '/';
}

/* 供文件监听过滤和根移除时的项目保留判定共用；只读借用根集合。 */
static TZrBool workspace_contains_native_path(const SZrLspWorkspace *workspace,
                                              const TZrChar *nativePath) {
    if (workspace == ZR_NULL || nativePath == ZR_NULL || !workspace->rootUris.isValid) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < workspace->rootUris.length; index++) {
        SZrString **uriPtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&workspace->rootUris, index);
        TZrChar rootPath[ZR_LIBRARY_MAX_PATH_LENGTH];

        if (uriPtr == ZR_NULL || *uriPtr == ZR_NULL ||
            !ZrLanguageServer_LspUri_FileToNativePath(*uriPtr, rootPath, sizeof(rootPath))) {
            continue;
        }
        if (workspace_path_is_within_root(nativePath, rootPath)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 在 AddFolder 中去重，按 URI 语义而非原始字节比较根目录。 */
static TZrBool workspace_contains_equivalent_uri(const SZrLspWorkspace *workspace, SZrString *uri) {
    if (workspace == ZR_NULL || uri == ZR_NULL || !workspace->rootUris.isValid) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < workspace->rootUris.length; index++) {
        SZrString **existingPtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&workspace->rootUris, index);
        if (existingPtr != ZR_NULL && *existingPtr != ZR_NULL &&
            ZrLanguageServer_LspUri_Equivalent(*existingPtr, uri)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 根撤销时清除属于该根的客户端显式项目选择，避免后续发现继续偏向已退出的项目。 */
static void workspace_clear_selected_project_if_under_root(SZrState *state,
                                                           SZrLspContext *context,
                                                           const TZrChar *rootPath) {
    if (state == ZR_NULL || context == ZR_NULL || context->clientSelectedZrpNativePath == ZR_NULL) {
        return;
    }

    /* TODO: 若选中项目仍由剩余的嵌套根覆盖，此处也会清除它；
     * 核查多根同时覆盖时客户端显式选择应否保留，并补根移除用例。 */
    if (workspace_path_is_within_root(context->clientSelectedZrpNativePath, rootPath)) {
        ZrLanguageServer_LspContext_SetClientSelectedZrpUri(state, context, ZR_NULL);
    }
}

/* 删除根后只释放失去所有剩余根覆盖的项目；索引移除会保留编辑器仍打开的文档。 */
static void workspace_release_removed_root_projects(SZrState *state,
                                                    SZrLspContext *context,
                                                    const TZrChar *removedRootPath) {
    SZrLspWorkspace *workspace;
    TZrSize index = 0;

    if (state == ZR_NULL || context == ZR_NULL || removedRootPath == ZR_NULL) {
        return;
    }

    workspace = context->workspace;
    while (index < context->projectIndexes.length) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        SZrLspProjectIndex *projectIndex = projectPtr != ZR_NULL ? *projectPtr : ZR_NULL;
        const TZrChar *projectRootPath =
            projectIndex != ZR_NULL ? workspace_string_text(projectIndex->projectRootPath) : ZR_NULL;

        if (projectIndex == ZR_NULL || projectRootPath == ZR_NULL ||
            !workspace_path_is_within_root(projectRootPath, removedRootPath) ||
            workspace_contains_native_path(workspace, projectRootPath)) {
            index++;
            continue;
        }

        if (!ZrLanguageServer_LspProject_RemoveProjectByProjectUriPreservingOpenDocuments(
                    state, context, projectIndex->projectFileUri)) {
            index++;
        }
    }
}

/* 构造仅建立根集合；由 context 的初始化/销毁统一拥有此对象。 */
SZrLspWorkspace *ZrLanguageServer_LspWorkspace_New(SZrState *state) {
    SZrLspWorkspace *workspace;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    workspace = (SZrLspWorkspace *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspWorkspace));
    if (workspace == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Array_Init(state,
                      &workspace->rootUris,
                      sizeof(SZrString *),
                      ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    return workspace;
}

/* 与 context 的项目索引释放分开，避免将根 URI 误当成项目对象的所有权入口。 */
void ZrLanguageServer_LspWorkspace_Free(SZrState *state, SZrLspWorkspace *workspace) {
    if (state == ZR_NULL || workspace == ZR_NULL) {
        return;
    }

    if (workspace->rootUris.isValid) {
        ZrCore_Array_Free(state, &workspace->rootUris);
    }
    ZrCore_Memory_RawFree(state->global, workspace, sizeof(SZrLspWorkspace));
}

/* initialize 重新建立根目录名单；此步骤本身不触发文件系统扫描或项目驱逐。 */
void ZrLanguageServer_LspWorkspace_Reset(SZrState *state, SZrLspContext *context) {
    SZrLspWorkspace *workspace;

    if (state == ZR_NULL || context == ZR_NULL || context->workspace == ZR_NULL) {
        return;
    }

    workspace = context->workspace;
    if (workspace->rootUris.isValid) {
        ZrCore_Array_Free(state, &workspace->rootUris);
    }
    ZrCore_Array_Init(state,
                      &workspace->rootUris,
                      sizeof(SZrString *),
                      ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
}

/* 客户端的 URI 先转本地路径再转规范 URI，供监听过滤与删除通知使用同一根身份。 */
TZrBool ZrLanguageServer_LspWorkspace_AddFolder(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri) {
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrString *canonicalUri;

    if (state == ZR_NULL || context == ZR_NULL || context->workspace == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_LspUri_FileToNativePath(uri, nativePath, sizeof(nativePath))) {
        return ZR_FALSE;
    }

    canonicalUri = ZrLanguageServer_LspUri_FromNativePath(state, nativePath);
    if (canonicalUri == ZR_NULL) {
        return ZR_FALSE;
    }
    if (workspace_contains_equivalent_uri(context->workspace, canonicalUri)) {
        return ZR_TRUE;
    }

    ZrCore_Array_Push(state, &context->workspace->rootUris, &canonicalUri);
    return ZR_TRUE;
}

/* 文件夹通知的删除要同时更新范围、项目选择与索引；先移出根，才可判断其他根的覆盖。 */
TZrBool ZrLanguageServer_LspWorkspace_RemoveFolder(SZrState *state,
                                                   SZrLspContext *context,
                                                   SZrString *uri) {
    SZrLspWorkspace *workspace;
    TZrChar removedRootPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || context == ZR_NULL || context->workspace == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    workspace = context->workspace;
    for (TZrSize index = 0; index < workspace->rootUris.length; index++) {
        SZrString **existingPtr = (SZrString **)ZrCore_Array_Get(&workspace->rootUris, index);
        if (existingPtr == ZR_NULL || *existingPtr == ZR_NULL ||
            !ZrLanguageServer_LspUri_Equivalent(*existingPtr, uri)) {
            continue;
        }
        if (!ZrLanguageServer_LspUri_FileToNativePath(*existingPtr,
                                                       removedRootPath,
                                                       sizeof(removedRootPath))) {
            return ZR_FALSE;
        }

        if (index + 1 < workspace->rootUris.length) {
            memmove(workspace->rootUris.head + index * workspace->rootUris.elementSize,
                    workspace->rootUris.head + (index + 1) * workspace->rootUris.elementSize,
                    (workspace->rootUris.length - index - 1) * workspace->rootUris.elementSize);
        }
        workspace->rootUris.length--;
        workspace_clear_selected_project_if_under_root(state, context, removedRootPath);
        workspace_release_removed_root_projects(state, context, removedRootPath);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/* 允许工作区外的已打开文档继续接收文件事件，以保持编辑器覆盖层的同步。 */
TZrBool ZrLanguageServer_LspWorkspace_CanProcessFileEvent(SZrLspContext *context,
                                                          SZrString *uri) {
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrFileVersion *fileVersion;

    if (context == ZR_NULL || context->workspace == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_LspUri_FileToNativePath(uri, nativePath, sizeof(nativePath))) {
        return ZR_FALSE;
    }

    if (workspace_contains_native_path(context->workspace, nativePath)) {
        return ZR_TRUE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    return fileVersion != ZR_NULL && fileVersion->isOpenDocument;
}
