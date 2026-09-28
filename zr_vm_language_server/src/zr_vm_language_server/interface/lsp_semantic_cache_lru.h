#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_CACHE_LRU_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_CACHE_LRU_H

#include "zr_vm_language_server/lsp_interface.h"

/** 当前分析器与历史语义快照共享的缓存存储预算；上下文拥有此状态。 */
typedef struct SZrLspSemanticCacheLru SZrLspSemanticCacheLru;

/** @brief 为 LSP 上下文创建默认预算与统计状态；仅由上下文构造流程调用。 */
SZrLspSemanticCacheLru *ZrLanguageServer_LspSemanticCacheLru_New(
        SZrState *state);
/** @brief 在上下文销毁时释放预算状态；分析器及其缓存由各自持有者释放。 */
void ZrLanguageServer_LspSemanticCacheLru_Free(
        SZrState *state,
        SZrLspContext *context);
/** @brief 在当前或历史分析器被语义查询使用后更新其逐出优先级。
 *  @pre analyzer 仍由 context 的当前分析器表或历史快照持有。 */
void ZrLanguageServer_LspSemanticCacheLru_Touch(
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer);
/** @brief 分析完成或缓存增长后收回超出预算的可丢弃缓存，不释放语义分析器本体。 */
void ZrLanguageServer_LspSemanticCacheLru_Enforce(
        SZrState *state,
        SZrLspContext *context);
/** @brief 调整上下文缓存预算并立即执行逐出；零预算也有效。 */
TZrBool ZrLanguageServer_LspSemanticCacheLru_SetLimit(
        SZrState *state,
        SZrLspContext *context,
        TZrSize limitBytes);
/** @brief 报告当前占用及上次执行预算检查时记录的峰值与逐出计数。
 *  @note outInfo 在失败时也清零；读取会扫描当前与历史分析器。 */
TZrBool ZrLanguageServer_LspSemanticCacheLru_GetInfo(
        const SZrLspContext *context,
        SZrLspSemanticCacheStorageInfo *outInfo);

#endif // ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_CACHE_LRU_H
