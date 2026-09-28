// 单次 LSP 语义请求的文档身份和依赖一致性边界。

#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_SNAPSHOT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_SNAPSHOT_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

/** @brief LSP 上下文持有项目、文档和分析器；快照仅借用该上下文。 */
typedef struct SZrLspContext SZrLspContext;
/** @brief 请求私有快照；Acquire 创建，Release 回收，字段不向调用方开放。 */
typedef struct SZrLspSemanticSnapshot SZrLspSemanticSnapshot;

/** @brief 汇总文档、项目、provider 与分析器的代际身份，供请求发送前验证和结果 ID 复用。
 * @note TrackDependency 可更新 dependencyFingerprint；GetIdentity 返回的地址仅在快照存活时有效。 */
typedef struct SZrLspSemanticSnapshotIdentity {
    TZrUInt64 documentGeneration;
    TZrUInt64 projectGeneration;
    TZrUInt64 providerGeneration;
    TZrUInt64 semanticGeneration;
    TZrUInt64 dependencyFingerprint;
} SZrLspSemanticSnapshotIdentity;

/** @brief 在语义请求开始时获取文档文本和当前项目/分析器身份，用于隔离请求期间的更新。
 * @note 仅文本块被独立保活；AST、分析器和上下文仍为借用引用，请求发出响应前须 Validate。
 * @return 成功后必须 Release；无法取得当前文档或分析器时为 NULL。 */
ZR_LANGUAGE_SERVER_API SZrLspSemanticSnapshot *
ZrLanguageServer_LspSemanticSnapshot_Acquire(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri);
/** @brief 结束语义请求时释放文本引用与已登记依赖；活动槽应先清空。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticSnapshot_Release(
        SZrState *state,
        SZrLspSemanticSnapshot *snapshot);
/** @brief 读取请求身份，供诊断缓存和编辑版本栅栏比较。
 * @return 借用指针；TrackDependency 后其中的指纹可能变化。 */
ZR_LANGUAGE_SERVER_API const SZrLspSemanticSnapshotIdentity *
ZrLanguageServer_LspSemanticSnapshot_GetIdentity(
        const SZrLspSemanticSnapshot *snapshot);
/** @brief 读取请求开始时固定的文本，供语义结果与原文一致性校验。
 * @return 借用缓冲区，Release 后失效；长度由 ContentLength 给出。 */
ZR_LANGUAGE_SERVER_API const TZrChar *
ZrLanguageServer_LspSemanticSnapshot_Content(
        const SZrLspSemanticSnapshot *snapshot);
/** @brief 读取固定文本的字节长度，不能将其误作 UTF-16 列数。 */
ZR_LANGUAGE_SERVER_API TZrSize ZrLanguageServer_LspSemanticSnapshot_ContentLength(
        const SZrLspSemanticSnapshot *snapshot);
/** @brief 为语义 token 全量/增量响应生成同一依赖身份下的结果 ID。
 * @note payloadLength 属于 ID 的一部分，避免长度变化时复用旧 delta；buffer 应足够容纳完整 ID。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticSnapshot_FormatResultId(
        const SZrLspSemanticSnapshot *snapshot,
        TZrSize payloadLength,
        TZrChar *buffer,
        TZrSize bufferLength);

/** @brief 登记请求实际读取的跨文档依赖，让发布前验证和结果 ID 包含该文档。
 * @return 未能捕获依赖版本时为假；调用方不应把未登记的读取视为受快照保护。
 * TODO: Lsp_FindAnalyzer 当前丢弃本函数失败结果；需核对缺失版本但分析器仍存在的调用路径是否可发布过期结果。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticSnapshot_TrackDependency(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticSnapshot *snapshot,
        SZrString *uri);
/** @brief 响应发布前复核文档、项目、provider、分析器及已登记依赖的代际状态。
 * @return 任一身份变化时为假；stdio 请求处理器据此返回 Content modified。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticSnapshot_Validate(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspSemanticSnapshot *snapshot);
/** @brief 把本次请求快照设为上下文活动槽，以便跨文档分析器读取自动登记依赖。
 * @pre 传入快照必须属于同一 context；Release 前应先设 NULL 清空活动槽。
 * @note 活动槽不增加引用计数，同一上下文只有一个槽。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticSnapshot_SetActive(
        SZrLspContext *context,
        SZrLspSemanticSnapshot *snapshot);
/** @brief 供语义 token 处理器取得当前请求快照，以生成与请求一致的结果 ID。
 * @return 借用指针；调用方不能释放该槽中的快照。 */
ZR_LANGUAGE_SERVER_API SZrLspSemanticSnapshot *
ZrLanguageServer_LspSemanticSnapshot_GetActive(const SZrLspContext *context);

/** @brief 元数据 provider 重载后推进代际标记，使旧语义快照在发布前验证失败。
 * @note 由项目元数据更新路径调用；不暴露 provider 的内部生命周期。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticSnapshot_ProviderChanged(
        SZrLspContext *context);

#endif
