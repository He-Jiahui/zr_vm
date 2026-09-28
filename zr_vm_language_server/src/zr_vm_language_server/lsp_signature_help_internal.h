#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SIGNATURE_HELP_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SIGNATURE_HELP_INTERNAL_H

#include "interface/lsp_interface_internal.h"
#include "semantic/semantic_analyzer_internal.h"

/**
 * @brief 为规范调用、外部 callable 和构造器创建同一所有权模型的签名结果。
 * @pre state、labelText、result 有效；params 和 resolvedSignature 可为空；analyzer 须与 argumentNodes 所属语义快照一致。
 * @return 成功时将新结果交给调用方，由 ZrLanguageServer_LspSignatureHelp_Free 统一释放；失败时调用方不得使用结果。
 * @note params 为空时只建签名外壳，调用方随后依据规范类型或外部契约追加参数；文档字符串由 VM 管理。
 */
TZrBool ZrLanguageServer_LspSignatureHelp_PopulateFromLabel(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const TZrChar *labelText,
        SZrAstNodeArray *params,
        SZrAstNodeArray *argumentNodes,
        const SZrResolvedCallSignature *resolvedSignature,
        TZrInt32 activeParameter,
        SZrLspSignatureHelp **result);

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_SIGNATURE_HELP_INTERNAL_H */
