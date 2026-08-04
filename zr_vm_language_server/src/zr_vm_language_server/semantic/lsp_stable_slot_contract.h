#ifndef ZR_VM_LANGUAGE_SERVER_LSP_STABLE_SLOT_CONTRACT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_STABLE_SLOT_CONTRACT_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_parser/compiler.h"

typedef enum EZrLspStableSlotContractKind {
    ZR_LSP_STABLE_SLOT_CONTRACT_NONE = 0,
    ZR_LSP_STABLE_SLOT_CONTRACT_HANDLE,
    ZR_LSP_STABLE_SLOT_CONTRACT_SOURCE,
    ZR_LSP_STABLE_SLOT_CONTRACT_WRITABLE_REF,
    ZR_LSP_STABLE_SLOT_CONTRACT_READONLY_REF,
} EZrLspStableSlotContractKind;

typedef struct SZrLspStableSlotContract {
    EZrLspStableSlotContractKind kind;
    const SZrTypeMemberInfo *acquireRead;
    const SZrTypeMemberInfo *acquireWrite;
    const SZrTypeMemberInfo *projection;
} SZrLspStableSlotContract;

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspStableSlotContract_Classify(
        const SZrTypePrototypeInfo *prototype,
        SZrLspStableSlotContract *outContract);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspStableSlotContract_AppendPrototypeHover(
        const SZrTypePrototypeInfo *prototype,
        TZrChar *buffer,
        TZrSize bufferSize);

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_STABLE_SLOT_CONTRACT_H */
