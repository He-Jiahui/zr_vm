#ifndef ZR_VM_LANGUAGE_SERVER_INCREMENTAL_TOKEN_EQUIVALENCE_H
#define ZR_VM_LANGUAGE_SERVER_INCREMENTAL_TOKEN_EQUIVALENCE_H

#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

/** @brief 判断等长的新旧内容是否保留所有 token 值和源位置，供 AST 快速复用。 */
TZrBool ZrLanguageServer_IncrementalTokenStreams_AreEquivalent(
    SZrState *state,
    SZrString *uri,
    const TZrChar *oldContent,
    TZrSize oldContentLength,
    const TZrChar *newContent,
    TZrSize newContentLength);

#endif // ZR_VM_LANGUAGE_SERVER_INCREMENTAL_TOKEN_EQUIVALENCE_H
