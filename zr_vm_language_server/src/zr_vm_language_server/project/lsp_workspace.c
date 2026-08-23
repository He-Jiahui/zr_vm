#include "project/lsp_workspace.h"

#include "interface/lsp_interface_internal.h"
#include "project/lsp_project_internal.h"
#include "zr_vm_language_server/lsp_uri.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <ctype.h>
#include <string.h>

struct SZrLspWorkspace {
    SZrArray rootUris; /* SZrString*, canonical file URIs */
};

static const TZrChar *workspace_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

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

static TZrBool workspace_path_is_within_root(const TZrChar *candidatePath, const TZrChar *rootPath) {
    TZrChar normalizedCandidate[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize rootLength;

    workspace_normalize_path(candidatePath, normalizedCandidate, sizeof(normalizedCandidate));
    workspace_normalize_path(rootPath, normalizedRoot, sizeof(normalizedRoot));
    rootLength = strlen(normalizedRoot);

    if (rootLength == 0 || strncmp(normalizedCandidate, normalizedRoot, rootLength) != 0) {
        return ZR_FALSE;
    }

    return normalizedCandidate[rootLength] == '\0' || normalizedRoot[rootLength - 1] == '/' ||
           normalizedCandidate[rootLength] == '/';
}

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

static void workspace_clear_selected_project_if_under_root(SZrState *state,
                                                           SZrLspContext *context,
                                                           const TZrChar *rootPath) {
    if (state == ZR_NULL || context == ZR_NULL || context->clientSelectedZrpNativePath == ZR_NULL) {
        return;
    }

    if (workspace_path_is_within_root(context->clientSelectedZrpNativePath, rootPath)) {
        ZrLanguageServer_LspContext_SetClientSelectedZrpUri(state, context, ZR_NULL);
    }
}

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

void ZrLanguageServer_LspWorkspace_Free(SZrState *state, SZrLspWorkspace *workspace) {
    if (state == ZR_NULL || workspace == ZR_NULL) {
        return;
    }

    if (workspace->rootUris.isValid) {
        ZrCore_Array_Free(state, &workspace->rootUris);
    }
    ZrCore_Memory_RawFree(state->global, workspace, sizeof(SZrLspWorkspace));
}

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
