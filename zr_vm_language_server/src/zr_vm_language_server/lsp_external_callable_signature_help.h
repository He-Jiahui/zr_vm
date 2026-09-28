#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_CALLABLE_SIGNATURE_HELP_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_CALLABLE_SIGNATURE_HELP_H

#include "lsp_signature_help_internal.h"

/** @brief 区分非本层外部调用、成功及已识别却缺少完整契约，决定上层是否继续查找。 */
typedef enum EZrLspExternalCallableSignatureStatus {
    ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_NOT_EXTERNAL = 0,
    ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_RESOLVED = 1,
    ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_UNAVAILABLE = 2
} EZrLspExternalCallableSignatureStatus;

/**
 * @brief 以项目元数据解析指定成员调用的签名，并复用规范 contract 的参数格式。
 * @pre calleeRange 指向当前文档的被调用成员；argumentNodes 与 analyzer 属于同一快照；result 预置为 NULL。
 * @return RESOLVED 交出需用 LspSignatureHelp_Free 释放的结果；UNAVAILABLE 禁止弱来源兜底；NOT_EXTERNAL 可继续上层查询。
 */
EZrLspExternalCallableSignatureStatus
ZrLanguageServer_LspExternalCallableSignatureHelp_Resolve(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrFileRange calleeRange,
        SZrAstNodeArray *argumentNodes,
        TZrInt32 activeParameter,
        SZrLspSignatureHelp **result);

#endif
