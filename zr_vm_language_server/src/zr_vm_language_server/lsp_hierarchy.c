#include "lsp_editor_features_internal.h"
#include "semantic/lsp_semantic_call_hierarchy.h"
#include "semantic/lsp_semantic_type_hierarchy.h"

/* stdio 的 prepare 请求经此入口进入 parser SymbolAt 查询；仅发布带语义身份的可回传节点。 */
TZrBool ZrLanguageServer_Lsp_PrepareCallHierarchy(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticCallHierarchy_Prepare(
            state, context, uri, position, result);
}

/* follow-up 使用 prepare 返回的身份查询入边；语义层校验文档版本并负责边的归并。 */
TZrBool ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticCallHierarchy_AppendIncoming(
            state, context, item, result);
}

/* 出边与入边保持同一身份契约，方向由语义事实查询决定而非由协议文本推断。 */
TZrBool ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticCallHierarchy_AppendOutgoing(
            state, context, item, result);
}

/* 类型 prepare 只接受可回查的声明事实，避免同名文本生成无法验证的层级根。 */
TZrBool ZrLanguageServer_Lsp_PrepareTypeHierarchy(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticTypeHierarchy_Prepare(
            state, context, uri, position, result);
}

/* 基类型方向直接消费 parser 的类型关系；旧版本 item 的拒绝由语义层统一处理。 */
TZrBool ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticTypeHierarchy_AppendSupertypes(
            state, context, item, result);
}

/* 派生类型方向复用相同的关系身份与版本检查，供 stdio 序列化直接子类型。 */
TZrBool ZrLanguageServer_Lsp_GetTypeHierarchySubtypes(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticTypeHierarchy_AppendSubtypes(
            state, context, item, result);
}
