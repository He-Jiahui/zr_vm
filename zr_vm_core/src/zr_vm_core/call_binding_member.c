#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/typed_call_binding.h"

#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"

static TZrBool member_fail(SZrFunctionCallSiteCacheEntry *entry,
                           SZrCallBindingDiagnostic *diagnostic, EZrCallBindingStatus status) {
    ZrCore_CallBinding_Invalidate(&entry->binding);
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->instructionIndex = entry->instructionIndex;
        diagnostic->targetMetadataToken = entry->binding.contract.targetMetadataToken;
    }
    return ZR_FALSE;
}

TZrBool ZrCore_CallBinding_PrepareMember(SZrState *state, SZrFunction *function, TZrUInt32 cacheIndex,
        const SZrTypeValue *receiver, SZrTypeValue *callable, SZrCallBindingDiagnostic *diagnostic) {
    SZrFunctionCallSiteCacheEntry *entry;
    SZrFunctionCallSitePicSlot *guard;
    SZrFunction *target = ZR_NULL;
    SZrObjectPrototype *prototype;
    SZrObjectPrototype *owner;
    SZrObject *object;
    SZrRawObject *targetObject;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (state == ZR_NULL || function == ZR_NULL || callable == ZR_NULL ||
        function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength) return ZR_FALSE;
    entry = &function->callSiteCaches[cacheIndex];
    if (ZrCore_CallBinding_Validate(&entry->binding, function->callBindingGeneration, diagnostic) != ZR_CALL_BINDING_OK) {
        if (diagnostic != ZR_NULL) diagnostic->instructionIndex = entry->instructionIndex;
        return ZR_FALSE;
    }
    targetObject = entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_VM
                           ? ZR_CAST_RAW_OBJECT_AS_SUPER(entry->binding.target.vm.function)
                           : entry->binding.target.callableObject;
    if (targetObject == ZR_NULL &&
        entry->binding.contract.bindingKind != ZR_CALL_BINDING_VIRTUAL &&
        entry->binding.contract.bindingKind != ZR_CALL_BINDING_INTERFACE) {
        return member_fail(entry, diagnostic, ZR_CALL_BINDING_TARGET_KIND_MISMATCH);
    }
    if (entry->binding.contract.ownerTypeToken != 0u) {
        guard = &entry->picSlots[0];
        owner = guard->cachedOwnerPrototype;
        /* Reflection attachment and static field writes change memberVersion.
         * Callable descriptor and inheritance changes use layoutGeneration. */
        if (owner == ZR_NULL || owner->shapeId != guard->cachedOwnerShapeId ||
            (entry->binding.target.ownerLayoutGeneration != 0u &&
             owner->layoutGeneration != entry->binding.target.ownerLayoutGeneration)) {
            return member_fail(entry, diagnostic, ZR_CALL_BINDING_LAYOUT_MISMATCH);
        }
        if (receiver == ZR_NULL || (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) ||
            receiver->isNative || receiver->value.object == ZR_NULL) {
            return member_fail(entry, diagnostic, ZR_CALL_BINDING_LAYOUT_MISMATCH);
        }
        object = ZR_CAST_OBJECT(state, receiver->value.object);
        if (object == ZR_NULL) return member_fail(entry, diagnostic, ZR_CALL_BINDING_LAYOUT_MISMATCH);
        prototype = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE
                            ? (SZrObjectPrototype *)object : object->prototype;
        if (entry->binding.contract.bindingKind != ZR_CALL_BINDING_INTERFACE) {
            while (prototype != ZR_NULL && prototype != owner) prototype = prototype->superPrototype;
            if (prototype != owner) return member_fail(entry, diagnostic, ZR_CALL_BINDING_LAYOUT_MISMATCH);
        }
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_VIRTUAL ||
            entry->binding.contract.bindingKind == ZR_CALL_BINDING_INTERFACE) {
            target = ZR_NULL;
            if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_INTERFACE) {
                SZrObjectPrototype *receiverPrototype = object->prototype;
                while (receiverPrototype != ZR_NULL && target == ZR_NULL) {
                    for (TZrUInt32 index = 0u; index < receiverPrototype->interfaceDispatchCount; ++index) {
                        const SZrInterfaceDispatchEntry *dispatch = &receiverPrototype->interfaceDispatchEntries[index];
                        if (dispatch->interfacePrototype != owner ||
                            dispatch->interfaceSlot != entry->binding.contract.dispatchSlot ||
                            dispatch->implementationPrototype == ZR_NULL ||
                            dispatch->descriptorIndex >= dispatch->implementationPrototype->memberDescriptorCount) continue;
                        if (dispatch->implementationLayoutGeneration !=
                            dispatch->implementationPrototype->layoutGeneration)
                            return member_fail(entry, diagnostic, ZR_CALL_BINDING_LAYOUT_MISMATCH);
                        target = dispatch->implementationPrototype->memberDescriptors[
                                dispatch->descriptorIndex].methodFunction;
                        break;
                    }
                    receiverPrototype = receiverPrototype->superPrototype;
                }
            } else {
                prototype = object->prototype;
                while (prototype != ZR_NULL && target == ZR_NULL) {
                    for (TZrUInt32 index = 0u; index < prototype->memberDescriptorCount; ++index) {
                        const SZrMemberDescriptor *descriptor = &prototype->memberDescriptors[index];
                        if (descriptor->virtualSlotIndex != entry->binding.contract.dispatchSlot) continue;
                        target = descriptor->methodFunction;
                        if (target != ZR_NULL) break;
                    }
                    prototype = prototype->superPrototype;
                }
            }
            if (target == ZR_NULL || target->super.type != ZR_RAW_OBJECT_TYPE_FUNCTION || target->super.isNative ||
                ZrCore_CallBinding_FunctionSignatureHash(target) != entry->binding.contract.signatureHash) {
                return member_fail(entry, diagnostic, ZR_CALL_BINDING_SIGNATURE_MISMATCH);
            }
            targetObject = ZR_CAST_RAW_OBJECT_AS_SUPER(target);
            entry->binding.target.targetKind = ZR_CALL_BINDING_TARGET_VM;
            entry->binding.target.vm.function = target;
            entry->binding.target.targetGeneration = target->callBindingGeneration;
            entry->binding.target.dispatchSlotCount = owner->nextVirtualSlotIndex;
            entry->binding.target.ownerLayoutGeneration = owner->layoutGeneration;
            entry->binding.generation = function->callBindingGeneration;
            if (entry->picSlotCount != 0u)
                entry->picSlots[0].cachedFunction = target;
            ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function),
                    ZR_CAST_RAW_OBJECT_AS_SUPER(target));
        }
    }
    if (targetObject == ZR_NULL) {
        return member_fail(entry, diagnostic, ZR_CALL_BINDING_TARGET_KIND_MISMATCH);
    }
    ZrCore_Value_InitAsRawObject(state, callable, targetObject);
    ++entry->runtimeHitCount;
    return ZR_TRUE;
}

TZrBool ZrCore_CallBinding_TryPrepareKnownCall(SZrState *state, SZrFunction *function,
        TZrUInt32 instructionIndex, SZrTypeValue *callable, SZrCallBindingDiagnostic *diagnostic) {
    TZrUInt32 mapEntry;
    if (function == ZR_NULL || function->callBindingInstructionMap == ZR_NULL ||
        instructionIndex >= function->callBindingInstructionMapLength) return ZR_TRUE;
    mapEntry = function->callBindingInstructionMap[instructionIndex];
    if (mapEntry == 0u) return ZR_TRUE;
    if (mapEntry - 1u >= function->callSiteCacheLength) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_CALL_BINDING_INVALID_RELOCATION;
            diagnostic->instructionIndex = instructionIndex;
        }
        return ZR_FALSE;
    }
    {
        SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[mapEntry - 1u];
        if (entry->kind != ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL) return ZR_TRUE;
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) {
            TZrBool success = ZrCore_CallBinding_PrepareTypedCall(state, function, entry, callable, diagnostic);
            if (!success && diagnostic != ZR_NULL) diagnostic->instructionIndex = instructionIndex;
            return success;
        }
        SZrRawObject *targetObject = entry->binding.target.callableObject != ZR_NULL
                ? entry->binding.target.callableObject : entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_VM
                ? ZR_CAST_RAW_OBJECT_AS_SUPER(entry->binding.target.vm.function) : ZR_NULL;
        if (entry->binding.contract.ownerTypeToken != 0u && entry->picSlotCount != 0u &&
            (entry->picSlots[0].cachedOwnerPrototype == ZR_NULL ||
             entry->picSlots[0].cachedOwnerPrototype->shapeId != entry->picSlots[0].cachedOwnerShapeId ||
             entry->picSlots[0].cachedOwnerPrototype->layoutGeneration != entry->binding.target.ownerLayoutGeneration)) {
            return member_fail(entry, diagnostic, ZR_CALL_BINDING_LAYOUT_MISMATCH);
        }
        if (ZrCore_CallBinding_Validate(&entry->binding, function->callBindingGeneration,
                                        diagnostic) == ZR_CALL_BINDING_OK &&
            targetObject != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, callable, targetObject);
            ++entry->runtimeHitCount;
            return ZR_TRUE;
        }
        if (diagnostic != ZR_NULL) {
            if (diagnostic->status == ZR_CALL_BINDING_OK)
                diagnostic->status = ZR_CALL_BINDING_TARGET_KIND_MISMATCH;
            diagnostic->instructionIndex = instructionIndex;
        }
        return ZR_FALSE;
    }
}

void ZrCore_CallBinding_PrepareKnownCall(SZrState *state, SZrFunction *function,
                                        TZrUInt32 instructionIndex, SZrTypeValue *callable) {
    if (!ZrCore_CallBinding_TryPrepareKnownCall(state, function, instructionIndex, callable,
                                             &state->lastCallBindingError)) {
        ZrCore_Debug_RunError(state, "CallBinding link error: %s (token=0x%08x, instruction=%u)",
                ZrCore_CallBinding_StatusName(state->lastCallBindingError.status),
                state->lastCallBindingError.targetMetadataToken, instructionIndex);
    }
}
