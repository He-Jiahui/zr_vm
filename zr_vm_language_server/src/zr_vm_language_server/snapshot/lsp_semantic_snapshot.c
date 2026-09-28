#include "interface/lsp_interface_internal.h"

#include "project/lsp_project_internal.h"
#include "semantic/lsp_semantic_query.h"
#include "semantic/semantic_analyzer_internal.h"

#include <string.h>

/* 请求实际读取或 import 图可达的文档身份；uri 借用 context/项目记录，其他字段是捕获值。 */
typedef struct SZrLspSemanticSnapshotDependency {
    SZrString *uri;
    TZrUInt64 documentGeneration;
    TZrSize version;
    TZrSize contentLength;
    TZrBool isOpenDocument;
} SZrLspSemanticSnapshotDependency;

/* 文本块由快照持有引用，AST、分析器和 context 只在请求有效期内借用；
 * identity 与 dependencies 用于响应发布前的失效栅栏。 */
struct SZrLspSemanticSnapshot {
    SZrLspContext *context;
    SZrString *uri;
    SZrFileVersionContentSnapshot content;
    const SZrAstNode *ast;
    const SZrSemanticAnalyzer *analyzer;
    SZrLspSemanticSnapshotIdentity identity;
    SZrArray dependencies;
};

/* 将固定长度的身份片段并入结果指纹；调用方决定片段边界。 */
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

/* 项目、文档和 provider 代际共用稳定的整数哈希表示。 */
static TZrUInt64 snapshot_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    for (TZrSize index = 0U; index < sizeof(value); index++) {
        hash ^= (value >> (index * 8U)) & 0xffU;
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* 快照身份可能引用短串或长串；只借出原生文本，不延长 GC 字符串寿命。 */
static const TZrChar *snapshot_string_text(const SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? ZrCore_String_GetNativeStringShort((SZrString *)value)
                   : ZrCore_String_GetNativeString((SZrString *)value);
}

/* 项目、成员和依赖 URI 进入结果身份前先带长度混合，避免不同分段拼接出同一字节流。 */
static TZrUInt64 snapshot_hash_string(TZrUInt64 hash, const SZrString *value) {
    const TZrChar *text = snapshot_string_text(value);

    if (text == ZR_NULL) {
        return snapshot_hash_u64(hash, 0U);
    }
    /* TODO: SZrString 自带字节长度，此处却用 strlen；若虚拟 URI 允许内嵌 NUL，
     * 不同的项目成员可能得到相同指纹。需核对 URI 接收边界并补长度编码用例。 */
    hash = snapshot_hash_u64(hash, strlen(text));
    return snapshot_hash_bytes(hash, text, strlen(text));
}

/* 统一扩散项目与依赖身份，供结果 ID 和编辑/诊断缓存作相等性栅栏。 */
static TZrUInt64 snapshot_mix(TZrUInt64 value) {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

/* 只把所属项目和成员视图纳入代际；项目内语义变化由 provider/文档/依赖栅栏承担。 */
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

/* 语义重建后缓存 AST 哈希优先；无缓存时用当前 AST 指纹防止旧分析投影复用。 */
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

/* 以 FileVersionContentSnapshot 的短暂引用读取代际/版本，随后归还文本引用；
 * TrackDependency 登记及主文档/依赖的发布前验证走此路径。 */
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

/* 依赖加入后重算统一结果身份；异或依赖摘要使 import 发现顺序不影响指纹。 */
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

/* 发布前逐项重取已登记文档的版本，防止跨文件读取后继续使用旧事实。 */
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

/* import 递归去重时将主文档也视为已登记，避免环依赖无限回访。 */
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

/* 获取快照时沿项目 import 图预登记直接与传递依赖；实际跨文档读取还可追加依赖。 */
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
        /* TODO: TrackDependency 捕获失败时仅跳过该分支，Acquire 仍可成功；
         * 需核对缺少 FileVersion 的项目记录是否还能贡献语义事实，并补失败注入测试。 */
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

/* stdio 文档请求和独立诊断/编辑投影共用入口：先完成项目视图，再固定文本与多层身份；
 * stdio 响应由请求层在发布前 Validate，独立身份消费者按各自的失效检查使用。 */
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
    /* 项目懒加载先于分析器查询，否则 import 与 provider 身份会来自不同视图。 */
    (void)ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    if (!ZrLanguageServer_LspSemanticQuery_TryGetAnalyzerForUri(
                state, context, uri, &analyzer)) {
        return ZR_NULL;
    }
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
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

/* 请求出口归还文本块和依赖容器；若此快照曾绑定 active 槽，调用方须先清空。 */
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

/* 诊断与 workspace edit 借用此身份；追加依赖后指纹会变，调用方需及时复制。 */
const SZrLspSemanticSnapshotIdentity *
ZrLanguageServer_LspSemanticSnapshot_GetIdentity(
        const SZrLspSemanticSnapshot *snapshot) {
    return snapshot == ZR_NULL ? ZR_NULL : &snapshot->identity;
}

/* 增量等价性检查读取请求固定文本；返回的原始指针随 Release 失效。 */
const TZrChar *ZrLanguageServer_LspSemanticSnapshot_Content(
        const SZrLspSemanticSnapshot *snapshot) {
    return snapshot == ZR_NULL ? ZR_NULL : snapshot->content.content;
}

/* 文本长度是字节数，必须与 Content 一起使用，不能据此推断 UTF-16 光标列。 */
TZrSize ZrLanguageServer_LspSemanticSnapshot_ContentLength(
        const SZrLspSemanticSnapshot *snapshot) {
    return snapshot == ZR_NULL ? 0U : snapshot->content.contentLength;
}

/* semantic token 全量和 delta 以同一依赖指纹及结果长度生成可复用身份。 */
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
    /* BUG: stdio 的 full/delta 处理器在 AST 缺失时仍可得到文本 token，却向这里传入
     * 空快照；零指纹只加 payload 长度会复用不同内容的旧 resultId，delta 随后返回空编辑。 */
    (void)snprintf(buffer,
                   (size_t)bufferLength,
                   "zr-snapshot:%llx:%zu",
                   (unsigned long long)(identity != ZR_NULL ? identity->dependencyFingerprint : 0U),
                   (size_t)payloadLength);
}

/* 获取时预登记 import，active 请求跨文档读取时追加；同一 URI 只登记一次并统一重验。 */
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

/* 响应写出前核对源文档、所属项目、provider、分析器和实际依赖；
 * stdio 失败时丢弃结果并返回 Content modified，而不是发布过期范围。 */
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

/* context 只保存借用指针；stdio 单请求分发先绑定，所有出口在 Release 前清空。 */
void ZrLanguageServer_LspSemanticSnapshot_SetActive(
        SZrLspContext *context,
        SZrLspSemanticSnapshot *snapshot) {
    if (context == ZR_NULL || (snapshot != ZR_NULL && snapshot->context != context)) {
        return;
    }
    context->activeSemanticSnapshot = snapshot;
}

/* semantic token 处理器读取当前请求绑定的快照；取得者不接管快照。 */
SZrLspSemanticSnapshot *ZrLanguageServer_LspSemanticSnapshot_GetActive(
        const SZrLspContext *context) {
    return context == ZR_NULL ? ZR_NULL : context->activeSemanticSnapshot;
}

/* 项目刷新推进 provider 代际，阻止旧请求身份在内容未变时继续通过验证。 */
void ZrLanguageServer_LspSemanticSnapshot_ProviderChanged(SZrLspContext *context) {
    if (context == ZR_NULL) {
        return;
    }
    context->semanticSnapshotProviderGeneration++;
    if (context->semanticSnapshotProviderGeneration == 0U) {
        context->semanticSnapshotProviderGeneration = 1U;
    }
}
