#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/typed_call_binding.h"
#include "call_binding_site.h"

#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"

static TZrBool link_fail(SZrCallBindingDiagnostic *diagnostic, SZrFunctionCallSiteCacheEntry *entry,
                         EZrCallBindingStatus status) {
    ZrCore_CallBinding_Invalidate(&entry->binding);
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->targetMetadataToken = entry->binding.contract.targetMetadataToken;
        diagnostic->instructionIndex = entry->instructionIndex;
    }
    return ZR_FALSE;
}

static TZrBool link_owner(SZrState *state, SZrFunction *function,
                          SZrFunctionCallSiteCacheEntry *entry, SZrCallBindingDiagnostic *diagnostic) {
    const SZrMetadataTokenRecord *definition = ZR_NULL;
    const SZrFunctionMemberEntry *member;
    SZrFunction *owner;
    SZrObjectPrototype *prototype;
    SZrFunctionCallSitePicSlot *guard = &entry->picSlots[0];
    if (entry->binding.contract.ownerTypeToken == 0u) return ZR_TRUE;
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        if (record->token != entry->binding.contract.ownerTypeToken) continue;
        if (definition != ZR_NULL) return link_fail(diagnostic, entry, ZR_CALL_BINDING_AMBIGUOUS_TARGET);
        definition = record;
    }
    if (definition == ZR_NULL || ZR_METADATA_TOKEN_TABLE(definition->token) != ZR_METADATA_TABLE_TYPE_DEF ||
        definition->reserved0 != ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER ||
        entry->memberEntryIndex >= function->memberEntryLength) {
        return link_fail(diagnostic, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
    }
    member = &function->memberEntries[entry->memberEntryIndex];
    owner = ZrCore_CallBinding_PrototypeOwner(function);
    if (owner == ZR_NULL) {
        return link_fail(diagnostic, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
    }
    if (member->entryKind != ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR ||
        member->prototypeIndex != definition->ownerIndex ||
        entry->binding.contract.layoutVersion != ZR_CALL_BINDING_SCHEMA_VERSION ||
        definition->signatureHash != entry->binding.contract.layoutHash ||
        ZrCore_CallBinding_PrototypeLayoutHash(owner, definition->ownerIndex) != definition->signatureHash) {
        return link_fail(diagnostic, entry, ZR_CALL_BINDING_LAYOUT_MISMATCH);
    }
    if (owner->prototypeInstances == ZR_NULL || member->prototypeIndex >= owner->prototypeInstancesLength ||
        owner->prototypeInstances[member->prototypeIndex] == ZR_NULL) {
        ZrCore_Module_CreatePrototypesFromData(state, ZR_NULL, owner);
    }
    if (owner->prototypeInstances == ZR_NULL || member->prototypeIndex >= owner->prototypeInstancesLength ||
        (prototype = owner->prototypeInstances[member->prototypeIndex]) == ZR_NULL ||
        member->descriptorIndex >= prototype->memberDescriptorCount) {
        return link_fail(diagnostic, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
    }
    if (!zr_call_binding_descriptor_matches(function, entry,
            &prototype->memberDescriptors[member->descriptorIndex])) {
        return link_fail(diagnostic, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
    }
    guard->cachedOwnerPrototype = prototype;
    guard->cachedOwnerShapeId = prototype->shapeId;
    guard->cachedOwnerShapeGeneration = prototype->shapeGeneration;
    guard->cachedOwnerVersion = prototype->super.memberVersion;
    guard->cachedDescriptorIndex = member->descriptorIndex;
    guard->cachedIsStatic = prototype->memberDescriptors[member->descriptorIndex].isStatic;
    guard->cachedReceiverPrototype = prototype;
    guard->cachedReceiverShapeId = prototype->shapeId;
    guard->cachedReceiverShapeGeneration = prototype->shapeGeneration;
    guard->cachedReceiverVersion = prototype->super.memberVersion;
    entry->picSlotCount = 1u;
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    return ZR_TRUE;
}

typedef struct SZrCallBindingLinkContext {
    SZrState *state;
    SZrCallBindingDiagnostic *diagnostic;
} SZrCallBindingLinkContext;

static TZrBool link_function(SZrFunction *function, void *data) {
    SZrCallBindingLinkContext *context = data;
    SZrState *state = context->state;
    SZrCallBindingDiagnostic *diagnostic = context->diagnostic;
    if (function->callBindingGeneration == 0u) function->callBindingGeneration = 1u;
    if (function->callBindingInstructionMap != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global, function->callBindingInstructionMap,
                sizeof(TZrUInt32) * function->callBindingInstructionMapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        function->callBindingInstructionMap = ZR_NULL;
        function->callBindingInstructionMapLength = 0u;
    }
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        const SZrMetadataTokenRecord *definition = ZR_NULL;
        SZrCallBindingCandidate candidate = {0};
        SZrTypeValue *constant;
        SZrFunction *target;
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
        if ((entry->binding.contract.bindingKind != ZR_CALL_BINDING_TYPED_FUNCTION &&
             entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_VM_MODULE &&
             entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_CONSTANT &&
             entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_MODULE) ||
            entry->bindingLocation.ownerDepth != 0u || entry->bindingLocation.flags != 0u ||
            (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_CONSTANT &&
             entry->bindingLocation.targetIndex >= function->constantValueLength) ||
            !zr_call_binding_site_matches(function, index)) {
            return link_fail(diagnostic, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
        }
        if (function->callBindingInstructionMap == ZR_NULL) {
            TZrSize bytes = sizeof(TZrUInt32) * function->instructionsLength;
            function->callBindingInstructionMap = ZrCore_Memory_RawMallocWithType(state->global,
                    bytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            if (function->callBindingInstructionMap == ZR_NULL) return ZR_FALSE;
            memset(function->callBindingInstructionMap, 0, bytes);
            function->callBindingInstructionMapLength = function->instructionsLength;
        }
        if (function->callBindingInstructionMap[entry->instructionIndex] != 0u) {
            return link_fail(diagnostic, entry, ZR_CALL_BINDING_AMBIGUOUS_TARGET);
        }
        function->callBindingInstructionMap[entry->instructionIndex] = index + 1u;

        if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_VM_MODULE) {
            if (entry->bindingLocation.targetIndex != 0u ||
                ZrCore_CallBinding_CheckContract(&entry->binding.contract, diagnostic) != ZR_CALL_BINDING_OK)
                return link_fail(diagnostic, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
            memset(&entry->binding.target, 0, sizeof(entry->binding.target));
            entry->binding.generation = function->callBindingGeneration;
            continue;
        }

        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) {
            if (!ZrCore_CallBinding_LinkTypedSignature(function, entry, diagnostic)) return ZR_FALSE;
            continue;
        }

        /* Provider-backed native bindings are relocated by the registry. The
         * core linker deliberately does not inspect provider names or invent a
         * local metadata record for them. */
        if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE &&
            entry->binding.contract.bindingKind == ZR_CALL_BINDING_DIRECT &&
            state->global->callBindingModuleResolver != ZR_NULL) {
            if (!state->global->callBindingModuleResolver(state, function, index, diagnostic,
                                                         state->global->callBindingModuleResolverUserData)) {
                return link_fail(diagnostic, entry, diagnostic != ZR_NULL &&
                        diagnostic->status != ZR_CALL_BINDING_OK
                                ? diagnostic->status : ZR_CALL_BINDING_TARGET_NOT_FOUND);
            }
            continue;
        }
        if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE &&
            entry->binding.contract.bindingKind == ZR_CALL_BINDING_DIRECT) {
            return link_fail(diagnostic, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
        }
        for (TZrUInt32 recordIndex = 0u; recordIndex < function->metadataTokenRecordLength; ++recordIndex) {
            const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[recordIndex];
            if (record->token != entry->binding.contract.targetMetadataToken) continue;
            if (definition != ZR_NULL) return link_fail(diagnostic, entry, ZR_CALL_BINDING_AMBIGUOUS_TARGET);
            definition = record;
        }
        if (definition == ZR_NULL ||
            definition->reserved0 != (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_CONSTANT
                                              ? ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT
                                              : ZR_METADATA_TOKEN_RECORD_CALLABLE_MODULE) ||
            definition->ownerIndex != entry->bindingLocation.targetIndex) {
            return link_fail(diagnostic, entry, ZR_CALL_BINDING_TARGET_NOT_FOUND);
        }
        if (!link_owner(state, function, entry, diagnostic)) return ZR_FALSE;
        if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE) {
            SZrObjectPrototype *owner = entry->picSlots[0].cachedOwnerPrototype;
            if (entry->binding.contract.bindingKind != ZR_CALL_BINDING_INTERFACE &&
                entry->binding.contract.bindingKind != ZR_CALL_BINDING_VIRTUAL)
                return link_fail(diagnostic, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
            candidate.contract = entry->binding.contract;
            candidate.contract.signatureToken = definition->relatedToken;
            candidate.contract.signatureHash = definition->signatureHash;
            candidate.contract.moduleSignatureHash = function->moduleSignatureHash;
            candidate.generation = function->callBindingGeneration;
            candidate.target.dispatchSlotCount = owner->nextVirtualSlotIndex;
            candidate.target.ownerLayoutGeneration = owner->layoutGeneration;
            if (ZrCore_CallBinding_Resolve(&entry->binding.contract, &candidate, 1u,
                    function->callBindingGeneration, &entry->binding, diagnostic) != ZR_CALL_BINDING_OK)
                return ZR_FALSE;
            continue;
        }
        constant = &function->constantValueList[entry->bindingLocation.targetIndex];
        target = ZrCore_Closure_GetMetadataFunctionFromValue(state, constant);
        if (target == ZR_NULL || target->super.isNative || target->closureValueLength != 0u) {
            return link_fail(diagnostic, entry, ZR_CALL_BINDING_TARGET_KIND_MISMATCH);
        }
        candidate.contract = entry->binding.contract;
        candidate.contract.targetMetadataToken = definition->token;
        candidate.contract.signatureToken = definition->relatedToken;
        candidate.contract.signatureHash = ZrCore_CallBinding_FunctionSignatureHash(target);
        candidate.contract.moduleSignatureHash = function->moduleSignatureHash;
        if (candidate.contract.signatureHash != definition->signatureHash) {
            return link_fail(diagnostic, entry, ZR_CALL_BINDING_SIGNATURE_MISMATCH);
        }
        candidate.generation = function->callBindingGeneration;
        candidate.target.targetKind = ZR_CALL_BINDING_TARGET_VM;
        if (entry->picSlots[0].cachedOwnerPrototype != ZR_NULL) {
            candidate.target.dispatchSlotCount = entry->picSlots[0].cachedOwnerPrototype->nextVirtualSlotIndex;
            candidate.target.ownerLayoutGeneration = entry->picSlots[0].cachedOwnerPrototype->layoutGeneration;
        }
        candidate.target.vm.function = target;
        candidate.target.targetGeneration = target->callBindingGeneration;
        if (entry->picSlotCount != 0u) entry->picSlots[0].cachedFunction = target;
        candidate.target.callableObject = constant->value.object;
        if (ZrCore_CallBinding_Resolve(&entry->binding.contract, &candidate, 1u,
                function->callBindingGeneration, &entry->binding, diagnostic) != ZR_CALL_BINDING_OK) return ZR_FALSE;
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), ZR_CAST_RAW_OBJECT_AS_SUPER(target));
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), candidate.target.callableObject);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_CallBinding_LinkFunction(SZrState *state, SZrFunction *function,
                                       SZrCallBindingDiagnostic *diagnostic) {
    SZrCallBindingLinkContext context = {state, diagnostic};
    if (state == ZR_NULL || function == ZR_NULL) return ZR_FALSE;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    return ZrCore_CallBinding_VisitFunctions(function, link_function, &context);
}
