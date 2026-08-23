#include "interface/lsp_interface_internal.h"

#include "project/lsp_project_internal.h"
#include "semantic/semantic_analyzer_internal.h"

#include <string.h>

typedef struct SZrLspSemanticSnapshotDependency {
    SZrString *uri;
    TZrUInt64 documentGeneration;
    TZrSize version;
    TZrSize contentLength;
    TZrBool isOpenDocument;
} SZrLspSemanticSnapshotDependency;

struct SZrLspSemanticSnapshot {
    SZrLspContext *context;
    SZrString *uri;
    SZrFileVersionContentSnapshot content;
    const SZrAstNode *ast;
    const SZrSemanticAnalyzer *analyzer;
    SZrLspSemanticSnapshotIdentity identity;
    SZrArray dependencies;
};

static TZrUInt64 snapshot_hash_bytes(
        TZrUInt64 hash,
        const TZrChar *bytes,
        TZrSize length) {
    const TZrUInt64 prime = 1099511628211ULL;

    if (bytes == ZR_NULL) {
        return hash;
    }
    for (TZrSize index = 0U; index < length; index++) {
        hash ^= (TZrUInt64)(unsigned char)bytes[index];
        hash *= prime;
    }
    return hash;
}

static TZrUInt64 snapshot_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    for (TZrSize index = 0U; index < sizeof(value); index++) {
        hash ^= (value >> (index * 8U)) & 0xffU;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static const TZrChar *snapshot_string_text(const SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? ZrCore_String_GetNativeStringShort((SZrString *)value)
                   : ZrCore_String_GetNativeString((SZrString *)value);
}

static TZrUInt64 snapshot_hash_string(TZrUInt64 hash, const SZrString *value) {
    const TZrChar *text = snapshot_string_text(value);

    if (text == ZR_NULL) {
        return snapshot_hash_u64(hash, 0U);
    }
    hash = snapshot_hash_u64(hash, strlen(text));
    return snapshot_hash_bytes(hash, text, strlen(text));
}

static TZrUInt64 snapshot_mix(TZrUInt64 value) {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

static TZrUInt64 snapshot_project_generation(
        SZrLspContext *context,
        SZrString *uri) {
    const SZrLspProjectIndex *projectIndex;
    TZrUInt64 hash = 1469598103934665603ULL;

    if (context == ZR_NULL || uri == ZR_NULL) {
        return 0U;
    }
    projectIndex = ZrLanguageServer_LspProject_FindProjectForUri(context, uri);
    if (projectIndex == ZR_NULL) {
        return 0U;
    }

    hash = snapshot_hash_string(hash, projectIndex->projectFileUri);
    hash = snapshot_hash_u64(hash, projectIndex->files.length);
    hash = snapshot_hash_u64(hash, projectIndex->hasSemanticProjectLoad);
    hash = snapshot_hash_u64(hash, projectIndex->hasLightweightSourceGraph);
    for (TZrSize index = 0U; index < projectIndex->files.length; index++) {
        const SZrLspProjectFileRecord *const *recordPtr =
                (const SZrLspProjectFileRecord *const *)ZrCore_Array_Get(
                        (SZrArray *)&projectIndex->files,
                        index);
        const SZrLspProjectFileRecord *record = recordPtr == ZR_NULL ? ZR_NULL : *recordPtr;

        if (record == ZR_NULL) {
            hash = snapshot_hash_u64(hash, 0U);
            continue;
        }
        /* Project view changes with membership, not unrelated semantic content. */
        hash = snapshot_hash_string(hash, record->uri);
    }
    return snapshot_mix(hash);
}

static TZrUInt64 snapshot_semantic_generation(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ast) {
    TZrUInt64 generation = 0U;

    if (analyzer != ZR_NULL && analyzer->cache != ZR_NULL) {
        generation = (TZrUInt64)analyzer->cache->astHash;
    }
    if (generation == 0U && ast != ZR_NULL) {
        generation = (TZrUInt64)ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(ast);
    }
    return generation == 0U ? 1U : generation;
}

static TZrBool snapshot_capture_document_generation(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        TZrUInt64 *outDocumentGeneration,
        TZrSize *outVersion,
        TZrSize *outContentLength,
        TZrBool *outIsOpenDocument) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot content = {0};
    TZrBool captured;

    if (outDocumentGeneration != ZR_NULL) {
        *outDocumentGeneration = 0U;
    }
    if (outVersion != ZR_NULL) {
        *outVersion = 0U;
    }
    if (outContentLength != ZR_NULL) {
        *outContentLength = 0U;
    }
    if (outIsOpenDocument != ZR_NULL) {
        *outIsOpenDocument = ZR_FALSE;
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    captured = ZrLanguageServer_FileVersionContentSnapshot_Acquire(
            state, fileVersion, &content);
    if (!captured) {
        return ZR_FALSE;
    }

    if (outDocumentGeneration != ZR_NULL) {
        *outDocumentGeneration = (TZrUInt64)content.contentGeneration;
    }
    if (outVersion != ZR_NULL) {
        *outVersion = content.version;
    }
    if (outContentLength != ZR_NULL) {
        *outContentLength = content.contentLength;
    }
    if (outIsOpenDocument != ZR_NULL) {
        *outIsOpenDocument = content.isOpenDocument;
    }
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &content);
    return ZR_TRUE;
}

static void snapshot_refresh_fingerprint(SZrLspSemanticSnapshot *snapshot) {
    TZrUInt64 hash = 1469598103934665603ULL;
    TZrUInt64 dependencyMix = 0U;

    if (snapshot == ZR_NULL) {
        return;
    }
    hash = snapshot_hash_string(hash, snapshot->uri);
    hash = snapshot_hash_u64(hash, snapshot->identity.documentGeneration);
    hash = snapshot_hash_u64(hash, snapshot->identity.projectGeneration);
    hash = snapshot_hash_u64(hash, snapshot->identity.providerGeneration);
    hash = snapshot_hash_u64(hash, snapshot->identity.semanticGeneration);
    for (TZrSize index = 0U; index < snapshot->dependencies.length; index++) {
        const SZrLspSemanticSnapshotDependency *dependency =
                (const SZrLspSemanticSnapshotDependency *)ZrCore_Array_Get(
                        &snapshot->dependencies,
                        index);
        TZrUInt64 dependencyHash = 1469598103934665603ULL;

        if (dependency == ZR_NULL) {
            continue;
        }
        dependencyHash = snapshot_hash_string(dependencyHash, dependency->uri);
        dependencyHash = snapshot_hash_u64(
                dependencyHash, dependency->documentGeneration);
        dependencyHash = snapshot_hash_u64(dependencyHash, dependency->version);
        dependencyHash = snapshot_hash_u64(dependencyHash, dependency->contentLength);
        dependencyHash = snapshot_hash_u64(dependencyHash, dependency->isOpenDocument);
        dependencyMix ^= snapshot_mix(dependencyHash);
    }
    hash = snapshot_hash_u64(hash, dependencyMix);
    snapshot->identity.dependencyFingerprint = snapshot_mix(hash);
}

static TZrBool snapshot_dependency_matches_current(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspSemanticSnapshotDependency *dependency) {
    TZrUInt64 documentGeneration;
    TZrSize version;
    TZrSize contentLength;
    TZrBool isOpenDocument;

    if (dependency == ZR_NULL ||
        !snapshot_capture_document_generation(
                state,
                context,
                dependency->uri,
                &documentGeneration,
                &version,
                &contentLength,
                &isOpenDocument)) {
        return ZR_FALSE;
    }
    return documentGeneration == dependency->documentGeneration &&
           version == dependency->version &&
           contentLength == dependency->contentLength &&
           isOpenDocument == dependency->isOpenDocument;
}

static TZrBool snapshot_tracks_uri(
        const SZrLspSemanticSnapshot *snapshot,
        SZrString *uri) {
    if (snapshot == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_Lsp_StringsEqual(snapshot->uri, uri)) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0U; index < snapshot->dependencies.length; index++) {
        const SZrLspSemanticSnapshotDependency *dependency =
                (const SZrLspSemanticSnapshotDependency *)ZrCore_Array_Get(
                        (SZrArray *)&snapshot->dependencies,
                        index);
        if (dependency != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual(dependency->uri, uri)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void snapshot_track_import_dependencies(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticSnapshot *snapshot,
        SZrLspProjectIndex *projectIndex,
        SZrAstNode *ast) {
    SZrArray bindings;

    if (state == ZR_NULL || context == ZR_NULL || snapshot == ZR_NULL || ast == ZR_NULL) {
        return;
    }
    if (projectIndex == ZR_NULL) {
        return;
    }

    ZrCore_Array_Init(state,
                      &bindings,
                      sizeof(SZrLspImportBinding *),
                      ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, ast, &bindings);
    for (TZrSize index = 0U; index < bindings.length; index++) {
        SZrLspImportBinding **bindingPtr =
                (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);
        SZrLspProjectFileRecord *record;

        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL || (*bindingPtr)->moduleName == ZR_NULL) {
            continue;
        }
        record = ZrLanguageServer_LspProject_FindRecordByModuleName(
                projectIndex, (*bindingPtr)->moduleName);
        if (record != ZR_NULL && record->uri != ZR_NULL &&
            !snapshot_tracks_uri(snapshot, record->uri) &&
            ZrLanguageServer_LspSemanticSnapshot_TrackDependency(
                    state, context, snapshot, record->uri)) {
            SZrFileVersion *dependencyVersion =
                    ZrLanguageServer_Lsp_GetDocumentFileVersion(context, record->uri);
            if (dependencyVersion != ZR_NULL && dependencyVersion->ast != ZR_NULL) {
                snapshot_track_import_dependencies(
                        state, context, snapshot, projectIndex, dependencyVersion->ast);
            }
        }
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
}

SZrLspSemanticSnapshot *ZrLanguageServer_LspSemanticSnapshot_Acquire(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri) {
    SZrLspSemanticSnapshot *snapshot;
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *analyzer;

    if (state == ZR_NULL || state->global == ZR_NULL || context == ZR_NULL ||
        uri == ZR_NULL) {
        return ZR_NULL;
    }
    /* Request handlers lazily complete this project transition before semantic reads. */
    (void)ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (fileVersion == ZR_NULL || fileVersion->ast == ZR_NULL || analyzer == ZR_NULL) {
        return ZR_NULL;
    }

    snapshot = (SZrLspSemanticSnapshot *)ZrCore_Memory_RawMalloc(
            state->global, sizeof(SZrLspSemanticSnapshot));
    if (snapshot == ZR_NULL) {
        return ZR_NULL;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(
                state, fileVersion, &snapshot->content)) {
        ZrCore_Memory_RawFree(state->global, snapshot, sizeof(*snapshot));
        return ZR_NULL;
    }

    snapshot->context = context;
    snapshot->uri = uri;
    snapshot->ast = fileVersion->ast;
    snapshot->analyzer = analyzer;
    snapshot->identity.documentGeneration =
            (TZrUInt64)snapshot->content.contentGeneration;
    snapshot->identity.projectGeneration = snapshot_project_generation(context, uri);
    snapshot->identity.providerGeneration = context->semanticSnapshotProviderGeneration;
    snapshot->identity.semanticGeneration = snapshot_semantic_generation(analyzer, fileVersion->ast);
    ZrCore_Array_Init(
            state,
            &snapshot->dependencies,
            sizeof(SZrLspSemanticSnapshotDependency),
            ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    snapshot_track_import_dependencies(
            state,
            context,
            snapshot,
            ZrLanguageServer_LspProject_FindProjectForUri(context, uri),
            fileVersion->ast);
    snapshot_refresh_fingerprint(snapshot);
    return snapshot;
}

void ZrLanguageServer_LspSemanticSnapshot_Release(
        SZrState *state,
        SZrLspSemanticSnapshot *snapshot) {
    if (state == ZR_NULL || state->global == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot->content);
    ZrCore_Array_Free(state, &snapshot->dependencies);
    ZrCore_Memory_RawFree(state->global, snapshot, sizeof(*snapshot));
}

const SZrLspSemanticSnapshotIdentity *
ZrLanguageServer_LspSemanticSnapshot_GetIdentity(
        const SZrLspSemanticSnapshot *snapshot) {
    return snapshot == ZR_NULL ? ZR_NULL : &snapshot->identity;
}

const TZrChar *ZrLanguageServer_LspSemanticSnapshot_Content(
        const SZrLspSemanticSnapshot *snapshot) {
    return snapshot == ZR_NULL ? ZR_NULL : snapshot->content.content;
}

TZrSize ZrLanguageServer_LspSemanticSnapshot_ContentLength(
        const SZrLspSemanticSnapshot *snapshot) {
    return snapshot == ZR_NULL ? 0U : snapshot->content.contentLength;
}

void ZrLanguageServer_LspSemanticSnapshot_FormatResultId(
        const SZrLspSemanticSnapshot *snapshot,
        TZrSize payloadLength,
        TZrChar *buffer,
        TZrSize bufferLength) {
    const SZrLspSemanticSnapshotIdentity *identity =
            ZrLanguageServer_LspSemanticSnapshot_GetIdentity(snapshot);

    if (buffer == ZR_NULL || bufferLength == 0U) {
        return;
    }
    (void)snprintf(buffer,
                   (size_t)bufferLength,
                   "zr-snapshot:%llx:%zu",
                   (unsigned long long)(identity != ZR_NULL ? identity->dependencyFingerprint : 0U),
                   (size_t)payloadLength);
}

TZrBool ZrLanguageServer_LspSemanticSnapshot_TrackDependency(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticSnapshot *snapshot,
        SZrString *uri) {
    SZrLspSemanticSnapshotDependency dependency;

    if (state == ZR_NULL || context == ZR_NULL || snapshot == ZR_NULL ||
        snapshot->context != context || uri == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_Lsp_StringsEqual(snapshot->uri, uri)) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0U; index < snapshot->dependencies.length; index++) {
        const SZrLspSemanticSnapshotDependency *existing =
                (const SZrLspSemanticSnapshotDependency *)ZrCore_Array_Get(
                        &snapshot->dependencies,
                        index);
        if (existing != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual(existing->uri, uri)) {
            return ZR_TRUE;
        }
    }

    memset(&dependency, 0, sizeof(dependency));
    dependency.uri = uri;
    if (!snapshot_capture_document_generation(
                state,
                context,
                uri,
                &dependency.documentGeneration,
                &dependency.version,
                &dependency.contentLength,
                &dependency.isOpenDocument)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(state, &snapshot->dependencies, &dependency);
    snapshot_refresh_fingerprint(snapshot);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspSemanticSnapshot_Validate(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspSemanticSnapshot *snapshot) {
    TZrUInt64 documentGeneration;
    TZrSize version;
    TZrSize contentLength;
    TZrBool isOpenDocument;
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *analyzer;

    if (state == ZR_NULL || context == ZR_NULL || snapshot == ZR_NULL ||
        snapshot->context != context ||
        context->semanticSnapshotProviderGeneration != snapshot->identity.providerGeneration ||
        snapshot_project_generation(context, snapshot->uri) != snapshot->identity.projectGeneration ||
        !snapshot_capture_document_generation(
                state,
                context,
                snapshot->uri,
                &documentGeneration,
                &version,
                &contentLength,
                &isOpenDocument) ||
        documentGeneration != snapshot->identity.documentGeneration ||
        version != snapshot->content.version ||
        contentLength != snapshot->content.contentLength ||
        isOpenDocument != snapshot->content.isOpenDocument) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, snapshot->uri);
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, snapshot->uri);
    if (fileVersion == ZR_NULL || analyzer == ZR_NULL || fileVersion->ast == ZR_NULL ||
        snapshot_semantic_generation(analyzer, fileVersion->ast) !=
                snapshot->identity.semanticGeneration) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < snapshot->dependencies.length; index++) {
        const SZrLspSemanticSnapshotDependency *dependency =
                (const SZrLspSemanticSnapshotDependency *)ZrCore_Array_Get(
                        (SZrArray *)&snapshot->dependencies,
                        index);
        if (!snapshot_dependency_matches_current(state, context, dependency)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

void ZrLanguageServer_LspSemanticSnapshot_SetActive(
        SZrLspContext *context,
        SZrLspSemanticSnapshot *snapshot) {
    if (context == ZR_NULL || (snapshot != ZR_NULL && snapshot->context != context)) {
        return;
    }
    context->activeSemanticSnapshot = snapshot;
}

SZrLspSemanticSnapshot *ZrLanguageServer_LspSemanticSnapshot_GetActive(
        const SZrLspContext *context) {
    return context == ZR_NULL ? ZR_NULL : context->activeSemanticSnapshot;
}

void ZrLanguageServer_LspSemanticSnapshot_ProviderChanged(SZrLspContext *context) {
    if (context == ZR_NULL) {
        return;
    }
    context->semanticSnapshotProviderGeneration++;
    if (context->semanticSnapshotProviderGeneration == 0U) {
        context->semanticSnapshotProviderGeneration = 1U;
    }
}
