#ifndef ZR_VM_LANGUAGE_SERVER_LSP_NATIVE_DECLARATION_PROJECTION_H
#define ZR_VM_LANGUAGE_SERVER_LSP_NATIVE_DECLARATION_PROJECTION_H

#include "lsp_virtual_documents.h"

typedef struct SZrLspVirtualRecord {
    EZrLspVirtualDeclarationKind kind;
    const void *declarationIdentity;
    const TZrChar *ownerName;
    const TZrChar *name;
    const TZrChar *targetModuleName;
    SZrFileRange range;
} SZrLspVirtualRecord;

/* Text is GC-owned; callers free the record array. Descriptor identities are
 * borrowed and valid only while the supplied descriptor remains alive. */
TZrBool ZrLanguageServer_LspNativeDeclarationProjection_Build(
        SZrState *state,
        const ZrLibModuleDescriptor *descriptor,
        SZrString *uri,
        SZrString **outText,
        SZrArray *outRecords);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspNativeDeclarationProjection_Find(
        SZrState *state,
        const ZrLibModuleDescriptor *descriptor,
        SZrString *uri,
        EZrLspVirtualDeclarationKind kind,
        const void *declarationIdentity,
        SZrFileRange *outRange);

struct SZrLspResolvedMetadataMember;
TZrBool ZrLanguageServer_LspNativeDeclarationProjection_ResolveMember(
        SZrState *state,
        struct SZrLspResolvedMetadataMember *resolvedMember);

#endif
