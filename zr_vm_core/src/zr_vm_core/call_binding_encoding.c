#include "zr_vm_core/call_binding.h"

#include <string.h>

#include "artifact_schema_internal.h"

TZrBool ZrCore_CallBinding_EncodeContract(const SZrCallBindingContract *contract,
                                         TZrByte *bytes, TZrSize length) {
    if (bytes == ZR_NULL || length != ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE ||
        ZrCore_CallBinding_CheckContract(contract, ZR_NULL) != ZR_CALL_BINDING_OK) {
        return ZR_FALSE;
    }
    zr_artifact_write_u32(bytes, contract->bindingKind);
    zr_artifact_write_u32(bytes + 4u, contract->targetMetadataToken);
    zr_artifact_write_u32(bytes + 8u, contract->signatureToken);
    zr_artifact_write_u32(bytes + 12u, contract->ownerTypeToken);
    zr_artifact_write_u64(bytes + 16u, contract->signatureHash);
    zr_artifact_write_u64(bytes + 24u, contract->moduleSignatureHash);
    zr_artifact_write_u32(bytes + 32u, contract->layoutVersion);
    zr_artifact_write_u32(bytes + 36u, contract->dispatchSlot);
    zr_artifact_write_u64(bytes + 40u, contract->layoutHash);
    zr_artifact_write_u32(bytes + 48u, contract->operation);
    zr_artifact_write_u32(bytes + 52u, 0u);
    zr_artifact_write_u64(bytes + 56u, 0u);
    return ZR_TRUE;
}

TZrBool ZrCore_CallBinding_DecodeContract(const TZrByte *bytes, TZrSize length,
                                         SZrCallBindingContract *contract) {
    SZrCallBindingContract decoded = {0};
    if (contract == ZR_NULL) return ZR_FALSE;
    memset(contract, 0, sizeof(*contract));
    if (bytes == ZR_NULL || length != ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE) return ZR_FALSE;
    decoded.bindingKind = zr_artifact_read_u32(bytes);
    decoded.targetMetadataToken = zr_artifact_read_u32(bytes + 4u);
    decoded.signatureToken = zr_artifact_read_u32(bytes + 8u);
    decoded.ownerTypeToken = zr_artifact_read_u32(bytes + 12u);
    decoded.signatureHash = zr_artifact_read_u64(bytes + 16u);
    decoded.moduleSignatureHash = zr_artifact_read_u64(bytes + 24u);
    decoded.layoutVersion = zr_artifact_read_u32(bytes + 32u);
    decoded.dispatchSlot = zr_artifact_read_u32(bytes + 36u);
    decoded.layoutHash = zr_artifact_read_u64(bytes + 40u);
    decoded.operation = zr_artifact_read_u32(bytes + 48u);
    decoded.reserved0 = zr_artifact_read_u32(bytes + 52u);
    decoded.reserved1 = zr_artifact_read_u64(bytes + 56u);
    if (ZrCore_CallBinding_CheckContract(&decoded, ZR_NULL) != ZR_CALL_BINDING_OK) return ZR_FALSE;
    *contract = decoded;
    return ZR_TRUE;
}
