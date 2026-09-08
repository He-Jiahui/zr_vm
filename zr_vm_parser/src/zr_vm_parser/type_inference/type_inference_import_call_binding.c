#include "type_inference_import_call_binding.h"

#include "zr_vm_core/module_call_binding.h"

void import_call_binding_export(SZrTypeMemberInfo *member, TZrUInt64 moduleHash) {
    SZrCallBindingContract *contract = &member->callBindingFact;
    contract->bindingKind = ZR_CALL_BINDING_DIRECT;
    contract->targetMetadataToken = member->metadataToken;
    contract->signatureToken = member->signatureToken;
    contract->signatureHash = member->signatureHash;
    contract->moduleSignatureHash = moduleHash;
    contract->dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    member->callBindingLocationKind = ZR_CALL_BINDING_RELOCATION_VM_MODULE;
    member->virtualSlotIndex = ZR_CALL_BINDING_SLOT_NONE;
    member->interfaceContractSlot = ZR_CALL_BINDING_SLOT_NONE;
}

void import_call_binding_method(SZrTypeMemberInfo *member, const SZrFunction *provider) {
    if (member->memberType != ZR_AST_CLASS_METHOD && member->memberType != ZR_AST_STRUCT_METHOD &&
        member->memberType != ZR_AST_CLASS_META_FUNCTION && member->memberType != ZR_AST_STRUCT_META_FUNCTION)
        return;
    if (!ZrCore_CallBinding_ModuleConstantContract(provider, member->functionConstantIndex,
                                                   &member->callBindingFact)) return;
    member->metadataToken = member->callBindingFact.targetMetadataToken;
    member->signatureToken = member->callBindingFact.signatureToken;
    member->signatureHash = member->callBindingFact.signatureHash;
    member->callBindingLocationKind = ZR_CALL_BINDING_RELOCATION_VM_MODULE;
    member->callBindingFact.operation = member->isMetaMethod ? ZR_CALL_BINDING_OPERATION_META :
            member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_GET ? ZR_CALL_BINDING_OPERATION_GET :
            (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_SET ||
             member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_INIT) ? ZR_CALL_BINDING_OPERATION_SET :
            ZR_CALL_BINDING_OPERATION_CALL;
    if (!member->isStatic && member->virtualSlotIndex != ZR_CALL_BINDING_SLOT_NONE &&
        (member->modifierFlags & (ZR_DECLARATION_MODIFIER_VIRTUAL | ZR_DECLARATION_MODIFIER_OVERRIDE |
                                  ZR_DECLARATION_MODIFIER_ABSTRACT)) != 0u) {
        member->callBindingFact.bindingKind = ZR_CALL_BINDING_VIRTUAL;
        member->callBindingFact.dispatchSlot = member->virtualSlotIndex;
    }
}
