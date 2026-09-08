#include "compiler_native_call_binding.h"

#include <string.h>
#include "zr_vm_parser/ast.h"

TZrBool compiler_native_call_binding_is_provider_contract(
        const SZrCallBindingContract *contract) {
    if (contract == ZR_NULL || contract->bindingKind != ZR_CALL_BINDING_DIRECT ||
        contract->targetMetadataToken == 0u || contract->signatureToken == 0u ||
        contract->signatureHash == 0u || contract->moduleSignatureHash == 0u) {
        return ZR_FALSE;
    }
    return ZR_METADATA_TOKEN_TABLE(contract->targetMetadataToken) == ZR_METADATA_TABLE_MEMBER_DEF &&
           ZR_METADATA_TOKEN_TABLE(contract->signatureToken) == ZR_METADATA_TABLE_SIGNATURE;
}

TZrBool compiler_native_call_binding_prepare_cache(
        SZrFunctionCallSiteCacheEntry *entry,
        const SZrCallBindingContract *contract,
        TZrUInt32 memberEntryIndex) {
    if (entry == ZR_NULL || contract == ZR_NULL ||
        !compiler_native_call_binding_is_provider_contract(contract)) {
        return ZR_FALSE;
    }
    entry->binding.contract = *contract;
    entry->bindingLocation.kind = ZR_CALL_BINDING_RELOCATION_MODULE;
    entry->bindingLocation.targetIndex = memberEntryIndex;
    entry->bindingLocation.ownerDepth = 0u;
    entry->bindingLocation.flags = 0u;
    return ZR_TRUE;
}

TZrBool compiler_finalize_native_call_binding(
        SZrCompilerState *compiler, SZrFunctionCallSiteCacheEntry *entry) {
    SZrCallBindingDiagnostic diagnostic;
    if (compiler == ZR_NULL || entry == ZR_NULL) return ZR_FALSE;
    if (entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_MODULE ||
        !compiler_native_call_binding_is_provider_contract(&entry->binding.contract) ||
        ZrCore_CallBinding_CheckContract(&entry->binding.contract, &diagnostic) != ZR_CALL_BINDING_OK) {
        ZrParser_Compiler_Error(compiler, "Native static call binding has no complete provider contract",
                compiler->currentAst != ZR_NULL ? compiler->currentAst->location : (SZrFileRange){0});
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
