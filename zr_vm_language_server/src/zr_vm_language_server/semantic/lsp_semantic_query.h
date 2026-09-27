#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_QUERY_H

#include "metadata/lsp_metadata_provider.h"

/** @brief 供 hover 和补全消费的目标类别；类别由语义事实或外部元数据决定，不代表运行时值类型。 */
typedef enum EZrLspResolvedValueKind {
    ZR_LSP_RESOLVED_VALUE_KIND_UNKNOWN = 0,
    ZR_LSP_RESOLVED_VALUE_KIND_SYMBOL = 1,
    ZR_LSP_RESOLVED_VALUE_KIND_MODULE = 2,
    ZR_LSP_RESOLVED_VALUE_KIND_CALLABLE = 3,
    ZR_LSP_RESOLVED_VALUE_KIND_TYPE = 4
} EZrLspResolvedValueKind;

/** @brief 查询结果中的显示类型与来源。resolvedTypeText 由 VM state 管理，借用查询期间的值类别和模块来源。 */
typedef struct SZrLspResolvedTypeInfo {
    SZrString *resolvedTypeText;
    EZrLspResolvedValueKind valueKind;
    EZrLspImportedModuleSourceKind origin;
} SZrLspResolvedTypeInfo;

/** @brief 定义、引用和高亮分派所用的目标域；外部元数据声明与源码导入成员走不同的身份校验路径。 */
typedef enum EZrLspSemanticQueryTargetKind {
    ZR_LSP_SEMANTIC_QUERY_TARGET_NONE = 0,
    ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL = 1,
    ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER = 2,
    ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION = 3,
    ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER = 4
} EZrLspSemanticQueryTargetKind;

/** @brief 单次文档位置解析的借用视图。analyzer、symbol、projectIndex 与元数据身份均依附当前 LSP context；
 *  调用方须在请求内使用，不得跨文档更新或 provider 重建缓存。hasSemanticVersion 为真时，后续查询会核对文档版本。 */
typedef struct SZrLspSemanticQuery {
    EZrLspSemanticQueryTargetKind kind;
    SZrString *uri;
    TZrSize semanticVersion;
    TZrBool hasSemanticVersion;
    SZrLspPosition position;
    SZrFileRange queryRange;
    SZrLspProjectIndex *projectIndex;
    SZrSemanticAnalyzer *analyzer;
    SZrSymbol *symbol;
    TZrBool hasCanonicalSymbol;
    SZrParserSemanticSymbolQuery canonicalSymbol;
    SZrFileRange canonicalReferenceRange;
    SZrString *moduleName;
    SZrString *memberName;
    EZrLspImportedModuleSourceKind sourceKind;
    SZrLspResolvedTypeInfo resolvedTypeInfo;
    SZrLspResolvedImportedModule resolvedModule;
    SZrLspResolvedMetadataMember resolvedMember;
} SZrLspSemanticQuery;

/** @brief 在解析前清零查询视图；允许传入空指针。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticQuery_Init(SZrLspSemanticQuery *query);
/** @brief 结束查询视图的使用并清零字段；不销毁由 state、analyzer 或 provider 管理的对象。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticQuery_Free(SZrState *state,
                                                                   SZrLspSemanticQuery *query);
/** @brief 将 LSP 光标解析为带来源身份的语义目标，供 hover/definition/references/rename 等复用。
 *  @pre query 可写；uri 在本次请求中有效；返回成功后仍须在请求结束前调用 Free。
 *  @return 命中可信的本地符号、导入成员或外部元数据目标时为真；失败时 query 可能已含借用字段。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspSemanticQuery *query);
/** @brief 为跨快照引用、局部查询和当前请求取得可用 analyzer，必要时从打开文档快照或磁盘重建。
 *  @note 返回的 analyzer 由 context 缓存持有；调用方不得跨缓存失效或下一次文档更新保留。 */
TZrBool ZrLanguageServer_LspSemanticQuery_TryGetAnalyzerForUri(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrSemanticAnalyzer **outAnalyzer);
/** @brief 根据已解析的目标选取规范符号或元数据 hover；只接受当前文档版本的查询。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_BuildHover(SZrState *state,
                                                                            SZrLspContext *context,
                                                                            SZrLspSemanticQuery *query,
                                                                            SZrLspHover **result);
/** @brief 汇总 token、import、receiver 和可见符号补全；由公开 completion 入口消费 result 元素。
 *  @pre result 为可写数组视图；调用方负责释放返回的 completion item。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrArray *result);
/** @brief 将已解析目标映射为定义位置；外部来源的坐标先转换为对应虚拟文档范围。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDefinitions(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result);
/** @brief 基于规范身份汇合局部、跨快照与元数据引用；includeDeclaration 控制声明位置是否加入结果。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendReferences(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    TZrBool includeDeclaration,
    SZrArray *result);
/** @brief 将同文档引用投影为高亮；导入成员仅在规范身份与元数据声明一致时使用引用事实。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result);

#endif
