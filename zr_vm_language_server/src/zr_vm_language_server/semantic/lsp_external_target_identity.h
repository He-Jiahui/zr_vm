#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_TARGET_IDENTITY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_TARGET_IDENTITY_H

#include "metadata/lsp_metadata_provider.h"

TZrBool ZrLanguageServer_LspExternalTargetIdentity_IsAvailable(
    const SZrParserSemanticSymbolQuery *symbol);
TZrBool ZrLanguageServer_LspExternalTargetIdentity_MatchesMember(
    const SZrParserSemanticSymbolQuery *symbol,
    const SZrLspResolvedMetadataMember *member);
TZrBool ZrLanguageServer_LspExternalTargetIdentity_MatchesReference(
    const SZrParserSemanticSymbolQuery *symbol,
    const SZrParserSemanticExternalReferenceQuery *reference);

#endif
