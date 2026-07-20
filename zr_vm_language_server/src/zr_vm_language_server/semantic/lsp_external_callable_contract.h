#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_CALLABLE_CONTRACT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_CALLABLE_CONTRACT_H

#include "metadata/lsp_metadata_provider.h"

typedef enum EZrLspExternalCallableKind {
    ZR_LSP_EXTERNAL_CALLABLE_NONE = 0,
    ZR_LSP_EXTERNAL_CALLABLE_FUNCTION = 1
} EZrLspExternalCallableKind;

typedef struct SZrLspExternalCallableContract {
    EZrLspExternalCallableKind kind;
    const TZrChar *name;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
} SZrLspExternalCallableContract;

TZrBool ZrLanguageServer_LspExternalCallableContract_FromResolvedMember(
        const SZrLspResolvedMetadataMember *member,
        SZrLspExternalCallableContract *contract);
TZrBool ZrLanguageServer_LspExternalCallableContract_Format(
        const SZrLspExternalCallableContract *contract,
        TZrChar *buffer,
        TZrSize bufferSize);
TZrBool ZrLanguageServer_LspExternalCallableContract_FormatParameter(
        const SZrLspExternalCallableContract *contract,
        TZrSize index,
        TZrChar *buffer,
        TZrSize bufferSize);

#endif
