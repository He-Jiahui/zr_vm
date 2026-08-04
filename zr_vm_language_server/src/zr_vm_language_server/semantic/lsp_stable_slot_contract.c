#include "semantic/lsp_stable_slot_contract.h"

#include "zr_vm_core/string.h"

#include <stdio.h>
#include <string.h>

static const TZrChar *stable_slot_member_name(const SZrTypeMemberInfo *member) {
    if (member == ZR_NULL || member->name == ZR_NULL) {
        return "<unavailable>";
    }
    return member->name->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? ZrCore_String_GetNativeStringShort(member->name)
                   : ZrCore_String_GetNativeString(member->name);
}

TZrBool ZrLanguageServer_LspStableSlotContract_Classify(
        const SZrTypePrototypeInfo *prototype,
        SZrLspStableSlotContract *outContract) {
    TZrBool hasPoolId = ZR_FALSE;
    TZrBool hasSlot = ZR_FALSE;
    TZrBool hasGeneration = ZR_FALSE;

    if (outContract != ZR_NULL) {
        memset(outContract, 0, sizeof(*outContract));
    }
    if (prototype == ZR_NULL || outContract == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->members, index);
        if (member == ZR_NULL) {
            continue;
        }
        switch ((EZrMemberContractRole)member->contractRole) {
            case ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_POOL_ID:
                hasPoolId = ZR_TRUE;
                break;
            case ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_SLOT:
                hasSlot = ZR_TRUE;
                break;
            case ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_GENERATION:
                hasGeneration = ZR_TRUE;
                break;
            case ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_READ:
                outContract->acquireRead = member;
                break;
            case ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_WRITE:
                outContract->acquireWrite = member;
                break;
            case ZR_MEMBER_CONTRACT_ROLE_POOL_REF_PROJECTION:
                if (member->hasStructuredReturnType &&
                    (member->structuredReturnType.referenceAccess ==
                             ZR_REFERENCE_ACCESS_WRITABLE ||
                     member->structuredReturnType.referenceAccess ==
                             ZR_REFERENCE_ACCESS_READONLY)) {
                    outContract->projection = member;
                }
                break;
            default:
                break;
        }
    }

    if ((prototype->protocolMask &
         ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_STABLE_SLOT_SOURCE)) != 0u &&
        outContract->acquireRead != ZR_NULL &&
        outContract->acquireWrite != ZR_NULL) {
        outContract->kind = ZR_LSP_STABLE_SLOT_CONTRACT_SOURCE;
        return ZR_TRUE;
    }
    if ((prototype->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE)) != 0u &&
        outContract->projection != ZR_NULL) {
        outContract->kind =
                outContract->projection->structuredReturnType.referenceAccess ==
                                ZR_REFERENCE_ACCESS_WRITABLE
                        ? ZR_LSP_STABLE_SLOT_CONTRACT_WRITABLE_REF
                        : ZR_LSP_STABLE_SLOT_CONTRACT_READONLY_REF;
        return ZR_TRUE;
    }
    if (hasPoolId && hasSlot && hasGeneration) {
        outContract->kind = ZR_LSP_STABLE_SLOT_CONTRACT_HANDLE;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool ZrLanguageServer_LspStableSlotContract_AppendPrototypeHover(
        const SZrTypePrototypeInfo *prototype,
        TZrChar *buffer,
        TZrSize bufferSize) {
    SZrLspStableSlotContract contract;
    TZrSize used;
    int written;

    if (buffer == ZR_NULL || bufferSize == 0u ||
        !ZrLanguageServer_LspStableSlotContract_Classify(prototype, &contract)) {
        return ZR_FALSE;
    }
    used = strlen(buffer);
    if (used >= bufferSize) {
        return ZR_FALSE;
    }
    switch (contract.kind) {
        case ZR_LSP_STABLE_SLOT_CONTRACT_HANDLE:
            written = snprintf(
                    buffer + used,
                    bufferSize - used,
                    "\n\nStable-slot contract: weak identity; access requires validation through its stable slot source.");
            break;
        case ZR_LSP_STABLE_SLOT_CONTRACT_SOURCE:
            written = snprintf(
                    buffer + used,
                    bufferSize - used,
                    "\n\nStable-slot contract: stable slot source; acquire readonly refs with `%s` and writable refs with `%s`.",
                    stable_slot_member_name(contract.acquireRead),
                    stable_slot_member_name(contract.acquireWrite));
            break;
        case ZR_LSP_STABLE_SLOT_CONTRACT_WRITABLE_REF:
            written = snprintf(
                    buffer + used,
                    bufferSize - used,
                    "\n\nStable-slot contract: scoped writable ref; valid only while its active stable-slot guard is held.");
            break;
        case ZR_LSP_STABLE_SLOT_CONTRACT_READONLY_REF:
            written = snprintf(
                    buffer + used,
                    bufferSize - used,
                    "\n\nStable-slot contract: scoped readonly ref; valid only while its active stable-slot guard is held.");
            break;
        default:
            return ZR_FALSE;
    }
    return written >= 0 && (TZrSize)written < bufferSize - used;
}
