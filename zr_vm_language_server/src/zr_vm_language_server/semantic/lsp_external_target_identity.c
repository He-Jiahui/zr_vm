#include "semantic/lsp_external_target_identity.h"

/* parser 与元数据提供者的成员枚举不一一对应，先限定能代表同一目标的类别。 */
static TZrBool external_target_kind_matches_member(
    EZrSemanticExternalTargetKind targetKind,
    EZrLspMetadataMemberKind memberKind) {
    switch (targetKind) {
        case ZR_SEMANTIC_EXTERNAL_TARGET_MODULE:
            return memberKind == ZR_LSP_METADATA_MEMBER_MODULE;
        case ZR_SEMANTIC_EXTERNAL_TARGET_CALLABLE:
            return memberKind == ZR_LSP_METADATA_MEMBER_FUNCTION ||
                   memberKind == ZR_LSP_METADATA_MEMBER_METHOD;
        case ZR_SEMANTIC_EXTERNAL_TARGET_TYPE:
            return memberKind == ZR_LSP_METADATA_MEMBER_TYPE;
        case ZR_SEMANTIC_EXTERNAL_TARGET_VALUE:
            return memberKind == ZR_LSP_METADATA_MEMBER_CONSTANT;
        case ZR_SEMANTIC_EXTERNAL_TARGET_FIELD:
            return memberKind == ZR_LSP_METADATA_MEMBER_FIELD ||
                   memberKind == ZR_LSP_METADATA_MEMBER_CONSTANT;
        case ZR_SEMANTIC_EXTERNAL_TARGET_PROPERTY:
            return memberKind == ZR_LSP_METADATA_MEMBER_PROPERTY;
        default:
            return ZR_FALSE;
    }
}

/* 缺少任一身份字段时不能把外部引用安全地合并到导航或项目引用结果。 */
TZrBool ZrLanguageServer_LspExternalTargetIdentity_IsAvailable(
    const SZrParserSemanticSymbolQuery *symbol) {
    return symbol != ZR_NULL && symbol->symbolId != ZR_SEMANTIC_ID_INVALID &&
           symbol->hasExternalTarget &&
           symbol->externalTargetKind != ZR_SEMANTIC_EXTERNAL_TARGET_UNKNOWN &&
           symbol->externalOwnerIdentity != ZR_NULL &&
           ZrCore_String_GetByteLength(symbol->externalOwnerIdentity) > 0U &&
           symbol->externalMetadataToken != 0U &&
           symbol->externalSignatureToken != 0U &&
           symbol->externalSignatureHash != 0U;
}

/* 定义跳转从 provider 重新解析 descriptor 后，核对 owner/token/hash 防止同名重载误跳。 */
TZrBool ZrLanguageServer_LspExternalTargetIdentity_MatchesMember(
    const SZrParserSemanticSymbolQuery *symbol,
    const SZrLspResolvedMetadataMember *member) {
    const SZrTypeMemberInfo *memberInfo;

    if (!ZrLanguageServer_LspExternalTargetIdentity_IsAvailable(symbol) ||
        member == ZR_NULL || member->typeMemberInfo == ZR_NULL ||
        !external_target_kind_matches_member(
                symbol->externalTargetKind, member->memberKind)) {
        return ZR_FALSE;
    }

    memberInfo = member->typeMemberInfo;
    return memberInfo->ownerTypeName != ZR_NULL &&
           ZrCore_String_Equal(
                   symbol->externalOwnerIdentity, memberInfo->ownerTypeName) &&
           symbol->externalMetadataToken == memberInfo->metadataToken &&
           symbol->externalSignatureToken == memberInfo->signatureToken &&
           symbol->externalSignatureHash == memberInfo->signatureHash;
}

/* 跨 analyzer 搜索还必须比较 provider generation，避免旧快照身份撞上新元数据。 */
TZrBool ZrLanguageServer_LspExternalTargetIdentity_MatchesReference(
    const SZrParserSemanticSymbolQuery *symbol,
    const SZrParserSemanticExternalReferenceQuery *reference) {
    return ZrLanguageServer_LspExternalTargetIdentity_IsAvailable(symbol) &&
           reference != ZR_NULL &&
           reference->symbolId != ZR_SEMANTIC_ID_INVALID &&
           reference->externalOwnerIdentity != ZR_NULL &&
           ZrCore_String_Equal(
                   reference->externalOwnerIdentity,
                   symbol->externalOwnerIdentity) &&
           reference->externalProviderGeneration ==
                   symbol->externalProviderGeneration &&
           reference->externalMetadataToken == symbol->externalMetadataToken &&
           reference->externalSignatureToken == symbol->externalSignatureToken &&
           reference->externalSignatureHash == symbol->externalSignatureHash &&
           reference->externalTargetKind == symbol->externalTargetKind;
}
