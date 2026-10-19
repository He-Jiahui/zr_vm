#include "call/call_binding_inline_frame.h"
#include "call/call_binding_site.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/type_layout.h"

#include <stdint.h>
#include <string.h>

static TZrBool inline_frame_member_fail(SZrFunctionCallSiteCacheEntry *entry,
        SZrCallBindingDiagnostic *diagnostic, EZrCallBindingStatus status) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        if (entry != ZR_NULL) {
            diagnostic->instructionIndex = entry->instructionIndex;
            diagnostic->targetMetadataToken = entry->binding.contract.targetMetadataToken;
        }
    }
    if (entry != ZR_NULL) ZrCore_CallBinding_Invalidate(&entry->binding);
    return ZR_FALSE;
}

TZrBool zr_call_binding_prepare_inline_frame_member(
        SZrState *state, SZrCallInfo *callerCallInfo, TZrUInt32 cacheIndex,
        TZrUInt32 sourceSlot, TZrStackValuePointer *ioCallWindow,
        TZrStackValuePointer *ioSourceFrame, TZrStackValuePointer *ioReturnDestination,
        SZrCallBindingDiagnostic *diagnostic) {
    SZrFunction *function;
    SZrFunction *callee;
    SZrFunction *owner;
    SZrFunctionCallSiteCacheEntry *entry = ZR_NULL;
    const SZrFunctionFrameSlotLayout *sourceLayout;
    const SZrFunctionFrameSlotLayout *calleeLayout;
    const SZrFunctionMemberEntry *member;
    const SZrMemberDescriptor *descriptor;
    const SZrTypeLayout *typeLayout;
    SZrObjectPrototype *prototype;
    SZrStackFramePlace sourcePlace;
    SZrFunctionStackAnchor callWindowAnchor, sourceFrameAnchor, returnAnchor;
    SZrTypeValue prototypeWitness;
    TZrUInt32 typeLayoutId;
    TZrBool hasReturnDestination;
    TZrBool ready = ZR_FALSE;
    const TZrUInt16 receiverFlags = ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT;
    const TZrUInt16 borrowedFlags = ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS;
    const TZrUInt16 borrowedReceiverFlags = receiverFlags | borrowedFlags;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (state == ZR_NULL || callerCallInfo == ZR_NULL ||
        state->callInfoList != callerCallInfo || !ZR_CALL_INFO_IS_VM(callerCallInfo) ||
        ioCallWindow == ZR_NULL || *ioCallWindow == ZR_NULL ||
        ioSourceFrame == ZR_NULL || *ioSourceFrame == ZR_NULL ||
        ioReturnDestination == ZR_NULL) {
        return inline_frame_member_fail(ZR_NULL, diagnostic, ZR_CALL_BINDING_INVALID_ARGUMENT);
    }
    function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
    if (function == ZR_NULL || function->callSiteCaches == ZR_NULL ||
        cacheIndex >= function->callSiteCacheLength) {
        return inline_frame_member_fail(ZR_NULL, diagnostic, ZR_CALL_BINDING_INVALID_ARGUMENT);
    }
    entry = &function->callSiteCaches[cacheIndex];
    if (diagnostic != ZR_NULL) {
        diagnostic->instructionIndex = entry->instructionIndex;
        diagnostic->targetMetadataToken = entry->binding.contract.targetMetadataToken;
    }
    sourceLayout = ZrCore_Function_FindFrameSlotLayout(function, sourceSlot);
    if (sourceLayout == ZR_NULL ||
        sourceLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        (sourceLayout->reserved0 != receiverFlags && sourceLayout->reserved0 != borrowedReceiverFlags) ||
        sourceLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return inline_frame_member_fail(entry, diagnostic, ZR_CALL_BINDING_LAYOUT_MISMATCH);
    }
    typeLayoutId = sourceLayout->typeLayoutId;
    hasReturnDestination = *ioReturnDestination != ZR_NULL;
    ZrCore_Function_StackAnchorInit(state, *ioCallWindow, &callWindowAnchor);
    ZrCore_Function_StackAnchorInit(state, *ioSourceFrame, &sourceFrameAnchor);
    if (hasReturnDestination)
        ZrCore_Function_StackAnchorInit(state, *ioReturnDestination, &returnAnchor);

    /* Warm both caches, then reacquire every GC-visible pointer before validation. */
    entry = ZR_NULL;
    if (ZrCore_Function_ResolvePrototypeFrameStructPrototype(state, function, typeLayoutId) == ZR_NULL)
        goto cleanup;
    if (state->callInfoList != callerCallInfo || !ZR_CALL_INFO_IS_VM(callerCallInfo)) goto cleanup;
    function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
    if (function == ZR_NULL ||
        ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, typeLayoutId, state) == ZR_NULL)
        goto cleanup;
    if (state->callInfoList != callerCallInfo || !ZR_CALL_INFO_IS_VM(callerCallInfo)) goto cleanup;
    function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
    if (function == ZR_NULL || function->callSiteCaches == ZR_NULL ||
        cacheIndex >= function->callSiteCacheLength) goto cleanup;
    entry = &function->callSiteCaches[cacheIndex];
    sourceLayout = ZrCore_Function_FindFrameSlotLayout(function, sourceSlot);
    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, typeLayoutId, state);
    prototype = ZrCore_Function_ResolvePrototypeFrameStructPrototype(state, function, typeLayoutId);
    *ioCallWindow = ZrCore_Function_StackAnchorRestore(state, &callWindowAnchor);
    *ioSourceFrame = ZrCore_Function_StackAnchorRestore(state, &sourceFrameAnchor);
    if (hasReturnDestination)
        *ioReturnDestination = ZrCore_Function_StackAnchorRestore(state, &returnAnchor);
    if (sourceLayout == ZR_NULL || typeLayout == ZR_NULL || prototype == ZR_NULL ||
        sourceLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        (sourceLayout->reserved0 != receiverFlags && sourceLayout->reserved0 != borrowedReceiverFlags) ||
        sourceLayout->typeLayoutId != typeLayoutId || typeLayout->kind != ZR_TYPE_LAYOUT_KIND_STRUCT ||
        typeLayout->byteSize == 0u || typeLayout->byteAlign == 0u ||
        sourceLayout->byteSize != typeLayout->byteSize || sourceLayout->byteAlign < typeLayout->byteAlign ||
        sourceLayout->byteOffset > function->frameByteSize ||
        sourceLayout->byteSize > function->frameByteSize - sourceLayout->byteOffset ||
        (sourceLayout->reserved0 == borrowedReceiverFlags &&
         (sizeof(SZrFunctionFrameBorrowedAliasBinding) > function->frameByteSize - sourceLayout->byteOffset ||
          sourceLayout->byteOffset % _Alignof(SZrFunctionFrameBorrowedAliasBinding) != 0u ||
          function->frameByteAlign < _Alignof(SZrFunctionFrameBorrowedAliasBinding))) ||
        function->frameByteAlign < sourceLayout->byteAlign ||
        !ZrCore_Function_MakeFrameSlotPlace(state, function, *ioSourceFrame, sourceSlot, &sourcePlace) ||
        sourcePlace.address == ZR_NULL || sourcePlace.byteSize != typeLayout->byteSize ||
        sourcePlace.byteAlign < typeLayout->byteAlign ||
        (uintptr_t)sourcePlace.address % typeLayout->byteAlign != 0u) goto cleanup;
    if (entry->kind != ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET || entry->argumentCount == 0u ||
        entry->binding.contract.bindingKind != ZR_CALL_BINDING_DIRECT ||
        entry->binding.contract.operation != ZR_CALL_BINDING_OPERATION_CALL ||
        entry->binding.contract.ownerTypeToken == 0u ||
        entry->binding.contract.layoutVersion != ZR_CALL_BINDING_SCHEMA_VERSION ||
        entry->binding.target.targetKind != ZR_CALL_BINDING_TARGET_VM ||
        entry->binding.target.vm.function == ZR_NULL || function->memberEntries == ZR_NULL ||
        entry->memberEntryIndex >= function->memberEntryLength) goto cleanup;
    if (ZrCore_CallBinding_Validate(&entry->binding, function->callBindingGeneration, diagnostic) !=
        ZR_CALL_BINDING_OK) goto cleanup;
    member = &function->memberEntries[entry->memberEntryIndex];
    owner = ZrCore_CallBinding_PrototypeOwner(function);
    if (member->entryKind != ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR ||
        member->prototypeIndex != typeLayoutId || entry->picSlotCount == 0u ||
        entry->picSlots[0].cachedOwnerPrototype != prototype || owner == ZR_NULL ||
        ZrCore_CallBinding_PrototypeLayoutHash(owner, member->prototypeIndex) != entry->binding.contract.layoutHash ||
        prototype->memberDescriptors == ZR_NULL ||
        member->descriptorIndex >= prototype->memberDescriptorCount) goto cleanup;
    descriptor = &prototype->memberDescriptors[member->descriptorIndex];
    callee = entry->binding.target.vm.function;
    if (!zr_call_binding_descriptor_matches(function, entry, descriptor) ||
        descriptor->kind != ZR_MEMBER_DESCRIPTOR_KIND_METHOD || descriptor->isStatic ||
        descriptor->receiverEffect != ZR_MEMBER_RECEIVER_EFFECT_READONLY ||
        descriptor->methodFunction != callee || callee->super.type != ZR_RAW_OBJECT_TYPE_FUNCTION ||
        callee->super.isNative || callee->closureValueLength != 0u ||
        ZrCore_CallBinding_PrototypeOwner(callee) != owner ||
        ZrCore_CallBinding_FunctionSignatureHash(callee) != entry->binding.contract.signatureHash) goto cleanup;
    calleeLayout = ZrCore_Function_FindFrameSlotLayout(callee, 0u);
    if (calleeLayout == ZR_NULL || !calleeLayout->isParameter ||
        calleeLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        (calleeLayout->reserved0 & borrowedFlags) != borrowedFlags ||
        calleeLayout->typeLayoutId != typeLayoutId || calleeLayout->byteSize != sourceLayout->byteSize ||
        calleeLayout->byteAlign == 0u || calleeLayout->byteAlign > sourceLayout->byteAlign ||
        calleeLayout->byteOffset > callee->frameByteSize ||
        sizeof(SZrFunctionFrameBorrowedAliasBinding) > callee->frameByteSize - calleeLayout->byteOffset ||
        calleeLayout->byteOffset % _Alignof(SZrFunctionFrameBorrowedAliasBinding) != 0u ||
        callee->frameByteAlign < _Alignof(SZrFunctionFrameBorrowedAliasBinding)) goto cleanup;

    ZrCore_Value_InitAsRawObject(state, &prototypeWitness, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    ready = ZrCore_CallBinding_PrepareMember(state, function, cacheIndex, &prototypeWitness,
            ZrCore_Stack_GetValueNoProfile(*ioCallWindow), diagnostic);
cleanup:
    *ioCallWindow = ZrCore_Function_StackAnchorRestore(state, &callWindowAnchor);
    *ioSourceFrame = ZrCore_Function_StackAnchorRestore(state, &sourceFrameAnchor);
    if (hasReturnDestination)
        *ioReturnDestination = ZrCore_Function_StackAnchorRestore(state, &returnAnchor);
    if (!ready) {
        EZrCallBindingStatus status = diagnostic != ZR_NULL && diagnostic->status != ZR_CALL_BINDING_OK
                ? diagnostic->status : ZR_CALL_BINDING_LAYOUT_MISMATCH;
        entry = ZR_NULL;
        if (state->callInfoList == callerCallInfo && ZR_CALL_INFO_IS_VM(callerCallInfo)) {
            function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
            if (function != ZR_NULL && function->callSiteCaches != ZR_NULL &&
                cacheIndex < function->callSiteCacheLength)
                entry = &function->callSiteCaches[cacheIndex];
        }
        return inline_frame_member_fail(entry, diagnostic, status);
    }
    return ready;
}
