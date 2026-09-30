#ifndef ZR_VM_CORE_CALL_BINDING_CONTRACT_INTERNAL_H
#define ZR_VM_CORE_CALL_BINDING_CONTRACT_INTERNAL_H

#include <string.h>

#include "zr_vm_core/call_binding.h"

/* Pure contract checks are shared by the public runtime API and Core-owned
 * ExecIR row validation. Keep this implementation independent from the
 * runtime graph visitor and call-target linker in call_binding.c. */
static inline EZrCallBindingStatus zr_core_call_binding_contract_fail(
        SZrCallBindingDiagnostic *diagnostic,
        EZrCallBindingStatus status,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return status;
}

static inline void zr_core_call_binding_contract_diagnostic_init(
        SZrCallBindingDiagnostic *diagnostic,
        const SZrCallBindingContract *contract) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->targetMetadataToken =
                contract != ZR_NULL ? contract->targetMetadataToken : 0u;
    }
}

static inline TZrBool zr_core_call_binding_contract_token_is(
        TZrMetadataToken token,
        TZrUInt32 table) {
    return ZR_METADATA_TOKEN_TABLE(token) == table &&
           ZR_METADATA_TOKEN_RID(token) != 0u;
}

static inline EZrCallBindingStatus zr_core_call_binding_check_contract(
        const SZrCallBindingContract *contract,
        SZrCallBindingDiagnostic *diagnostic) {
    zr_core_call_binding_contract_diagnostic_init(diagnostic, contract);
    if (contract == ZR_NULL) {
        return zr_core_call_binding_contract_fail(
                diagnostic, ZR_CALL_BINDING_INVALID_ARGUMENT, 0u, 0u);
    }
    if (contract->bindingKind < ZR_CALL_BINDING_DIRECT ||
        contract->bindingKind > ZR_CALL_BINDING_TYPED_FUNCTION ||
        contract->signatureHash == 0u ||
        contract->moduleSignatureHash == 0u) {
        return zr_core_call_binding_contract_fail(
                diagnostic, ZR_CALL_BINDING_MISSING_CONTRACT, 0u, 0u);
    }
    if (!zr_core_call_binding_contract_token_is(
                contract->signatureToken, ZR_METADATA_TABLE_SIGNATURE) ||
        (!(contract->bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION &&
           contract->targetMetadataToken == 0u) &&
         !zr_core_call_binding_contract_token_is(
                 contract->targetMetadataToken, ZR_METADATA_TABLE_MEMBER_DEF) &&
         !zr_core_call_binding_contract_token_is(
                 contract->targetMetadataToken, ZR_METADATA_TABLE_MEMBER_REF)) ||
        (contract->ownerTypeToken != 0u &&
         !zr_core_call_binding_contract_token_is(
                 contract->ownerTypeToken, ZR_METADATA_TABLE_TYPE_DEF) &&
         !zr_core_call_binding_contract_token_is(
                 contract->ownerTypeToken, ZR_METADATA_TABLE_TYPE_REF) &&
         !zr_core_call_binding_contract_token_is(
                 contract->ownerTypeToken, ZR_METADATA_TABLE_TYPE_SPEC))) {
        return zr_core_call_binding_contract_fail(
                diagnostic, ZR_CALL_BINDING_INVALID_TOKEN, 0u, 0u);
    }
    if (contract->operation > ZR_CALL_BINDING_OPERATION_META ||
        contract->reserved0 != 0u || contract->reserved1 != 0u) {
        return zr_core_call_binding_contract_fail(
                diagnostic, ZR_CALL_BINDING_INVALID_ARGUMENT, 0u, 0u);
    }
    /* Owner identity and layout version must be present as one contract. */
    if ((contract->ownerTypeToken == 0u &&
         (contract->layoutVersion != 0u || contract->layoutHash != 0u)) ||
        (contract->ownerTypeToken != 0u &&
         (contract->layoutVersion == 0u || contract->layoutHash == 0u))) {
        return zr_core_call_binding_contract_fail(
                diagnostic, ZR_CALL_BINDING_MISSING_CONTRACT, 0u, 0u);
    }
    if (contract->bindingKind == ZR_CALL_BINDING_VIRTUAL ||
        contract->bindingKind == ZR_CALL_BINDING_INTERFACE) {
        if (contract->ownerTypeToken == 0u ||
            contract->dispatchSlot == ZR_CALL_BINDING_SLOT_NONE) {
            return zr_core_call_binding_contract_fail(
                    diagnostic, ZR_CALL_BINDING_INVALID_SLOT, 0u,
                    contract->dispatchSlot);
        }
    } else if (contract->dispatchSlot != ZR_CALL_BINDING_SLOT_NONE) {
        return zr_core_call_binding_contract_fail(
                diagnostic, ZR_CALL_BINDING_INVALID_SLOT,
                ZR_CALL_BINDING_SLOT_NONE, contract->dispatchSlot);
    }
    return ZR_CALL_BINDING_OK;
}

#endif
