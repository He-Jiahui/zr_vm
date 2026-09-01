#include "semantic/lsp_external_target_identity.h"

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
