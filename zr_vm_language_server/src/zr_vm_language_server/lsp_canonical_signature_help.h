#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SIGNATURE_HELP_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SIGNATURE_HELP_H

#include "lsp_signature_help_internal.h"

/**
 * @brief 从当前 analyzer 的规范调用事实生成签名帮助，供函数、构造及 super 构造路径共用。
 * @pre position 与 argumentNodes 必须来自同一次文档分析；result 由调用方预置为 NULL。
 * @return 失败时释放本函数建出的半成品，成功结果由 LspSignatureHelp_Free 释放。
 */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_Resolve(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrAstNodeArray *argumentNodes,
        TZrInt32 activeParameter,
        SZrLspSignatureHelp **result);

/**
 * @brief 为签名 hover 取得已解析调用的引用范围，避免仅高亮光标点。
 * @return 未取得规范解析目标时返回 false，且不写 result。
 */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_TryGetResolvedCallReferenceRange(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrFileRange *result);

/**
 * @brief 识别有调用引用却缺少解析目标的非成员调用，供 hover 阻止弱来源兜底。
 * @note 调用方须先处理已知可调用值的 hover；此查询本身不证明调用源于本地声明。
 */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_HasUnavailableLocalCall(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position);

/**
 * @brief 从已解析接收者调用的规范函数类型生成 hover，与签名帮助使用同一标签。
 * @return 仅对光标位于调用引用且具有 receiverEffect 的调用成功。
 */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_ResolveReceiverHover(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrFileRange position,
        SZrLspHover **result);

/**
 * @brief 为缺少外部声明目标但保留规范调用类型的可调用值生成 hover。
 * @note 该路径先于普通外部元数据 hover，不能据此推导声明身份；成功对象由 hover 调用方释放。
 */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_ResolveExternalCallableHover(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrFileRange position,
        SZrLspHover **result);

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SIGNATURE_HELP_H */
