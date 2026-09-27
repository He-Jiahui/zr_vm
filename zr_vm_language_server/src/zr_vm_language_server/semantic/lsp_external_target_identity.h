#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_TARGET_IDENTITY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_TARGET_IDENTITY_H

#include "metadata/lsp_metadata_provider.h"

/** @brief 判断 parser 结果是否携带足以跨快照比较的外部符号身份。 */
TZrBool ZrLanguageServer_LspExternalTargetIdentity_IsAvailable(
    const SZrParserSemanticSymbolQuery *symbol);
/**
 * @brief 把查询目标与提供者当前解析出的成员核对，防止仅凭同名成员跳转。
 * @pre member 的 descriptor 与 symbol 的提供者身份在调用期间有效。
 */
TZrBool ZrLanguageServer_LspExternalTargetIdentity_MatchesMember(
    const SZrParserSemanticSymbolQuery *symbol,
    const SZrLspResolvedMetadataMember *member);
/**
 * @brief 比较两个 analyzer 快照中的外部引用身份；提供者代数必须一致。
 */
TZrBool ZrLanguageServer_LspExternalTargetIdentity_MatchesReference(
    const SZrParserSemanticSymbolQuery *symbol,
    const SZrParserSemanticExternalReferenceQuery *reference);

#endif
