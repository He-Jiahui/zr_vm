#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_SNAPSHOT_CACHE_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_SNAPSHOT_CACHE_H

#include "zr_vm_language_server/lsp_interface.h"

/** 每个 URI 最多保留与文件内容历史相同数量的语义分析器；上下文持有缓存。 */
typedef struct SZrLspSemanticSnapshotCache SZrLspSemanticSnapshotCache;
/** 借用缓存中的分析器供 LRU 统计或重排；回调不能保存该指针。 */
typedef void (*TZrLspSemanticSnapshotAnalyzerVisitor)(
        SZrSemanticAnalyzer *analyzer,
        void *userData);

/** @brief 随 LSP 上下文创建历史语义分析器容器。 */
SZrLspSemanticSnapshotCache *ZrLanguageServer_LspSemanticSnapshotCache_New(
        SZrState *state);
/** @brief 先释放历史分析器及其 AST，再释放容器；须早于当前分析器销毁。 */
void ZrLanguageServer_LspSemanticSnapshotCache_Free(
        SZrState *state,
        SZrLspContext *context);
/** @brief 在当前分析器从上下文移除前清除该 URI 的历史语义状态。 */
void ZrLanguageServer_LspSemanticSnapshotCache_RemoveUri(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri);
/** @brief 将被新版本替换的分析状态转为历史快照，供版本化语义查询使用。
 *  @pre retainedAst 属于 currentAnalyzer 当前状态，fileVersion 已记录前一版内容，uri 在快照存活期有效。
 *  @return 成功后缓存接管 retainedAst；失败时调用方仍须处理 retainedAst。 */
TZrBool ZrLanguageServer_LspSemanticSnapshotCache_Capture(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const SZrFileVersion *fileVersion,
        SZrSemanticAnalyzer *currentAnalyzer,
        SZrAstNode *retainedAst,
        TZrBool preserveScopedQueryAnalyzer);
/** @brief 借用遍历仍存活的历史分析器，供统一预算与访问顺序维护使用。 */
void ZrLanguageServer_LspSemanticSnapshotCache_VisitAnalyzers(
        const SZrLspContext *context,
        TZrLspSemanticSnapshotAnalyzerVisitor visitor,
        void *userData);

#endif
