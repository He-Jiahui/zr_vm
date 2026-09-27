#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CROSS_SNAPSHOT_REFERENCES_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CROSS_SNAPSHOT_REFERENCES_H

#include "semantic/lsp_semantic_query.h"

/**
 * @brief 为本地声明搜索项目中其他语义快照里的外部引用。
 * @pre query 已解析为带 canonical 身份的本地符号，项目索引与请求上下文仍有效。
 * @note 返回值只表示是否追加引用；扫描期间取消请求会提前终止。
 */
TZrBool ZrLanguageServer_LspCrossSnapshotReferences_Append(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        SZrArray *result);

/**
 * @brief 按元数据提供者代数与外部符号身份合并项目范围的引用。
 * @pre query 的 canonical 外部身份必须来自当前提供者代；索引中的 analyzer 可按需建立。
 * @note 返回值只表示是否追加引用，不能代表整个项目被完整扫描。
 */
TZrBool ZrLanguageServer_LspCrossSnapshotReferences_AppendExternal(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        SZrArray *result);

/**
 * @brief 从原生插件虚拟声明反查项目源码中的实际使用点。
 * @pre declaration 的 URI 必须归属 projectIndex；调用方维护结果数组生命周期。
 */
TZrBool ZrLanguageServer_LspCrossSnapshotReferences_AppendNativeDeclaration(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        const SZrFileRange *declaration,
        SZrArray *result);

#endif
