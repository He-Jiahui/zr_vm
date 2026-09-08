#include "zr_vm_core/typed_call_binding.h"

#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"

static TZrBool typed_fail(SZrFunctionCallSiteCacheEntry *entry,
                         SZrCallBindingDiagnostic *diagnostic, EZrCallBindingStatus status,
                         TZrUInt64 expected, TZrUInt64 actual) {
    ZrCore_CallBinding_Invalidate(&entry->binding);
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->instructionIndex = entry->instructionIndex;
        diagnostic->targetMetadataToken = entry->binding.contract.targetMetadataToken;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return ZR_FALSE;
}

TZrBool ZrCore_CallBinding_LinkTypedSignature(SZrFunction *function,
        SZrFunctionCallSiteCacheEntry *entry, SZrCallBindingDiagnostic *diagnostic) {
    const SZrMetadataTokenRecord *signature = ZR_NULL;
    if (ZrCore_CallBinding_CheckContract(&entry->binding.contract, diagnostic) != ZR_CALL_BINDING_OK)
        return ZR_FALSE;
    if (entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_NONE ||
        entry->bindingLocation.targetIndex != ZR_CALL_BINDING_SLOT_NONE ||
        entry->bindingLocation.ownerDepth != 0u || entry->bindingLocation.flags != 0u ||
        entry->binding.contract.targetMetadataToken != 0u || entry->binding.contract.ownerTypeToken != 0u ||
        entry->binding.contract.operation != ZR_CALL_BINDING_OPERATION_CALL)
        return typed_fail(entry, diagnostic, ZR_CALL_BINDING_INVALID_RELOCATION, 0u, 0u);
    switch ((EZrInstructionCode)function->instructionsList[entry->instructionIndex].instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL_SPREAD):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS): break;
        default: return typed_fail(entry, diagnostic, ZR_CALL_BINDING_TARGET_KIND_MISMATCH, 0u, 0u);
    }
    if (entry->binding.contract.moduleSignatureHash != function->moduleSignatureHash)
        return typed_fail(entry, diagnostic, ZR_CALL_BINDING_MODULE_MISMATCH,
                entry->binding.contract.moduleSignatureHash, function->moduleSignatureHash);
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        if (record->token != entry->binding.contract.signatureToken) continue;
        if (signature != ZR_NULL) return typed_fail(entry, diagnostic, ZR_CALL_BINDING_AMBIGUOUS_TARGET, 0u, 0u);
        signature = record;
    }
    if (signature == ZR_NULL || signature->reserved0 != ZR_METADATA_TOKEN_RECORD_CALLABLE_SIGNATURE ||
        signature->relatedToken != 0u || signature->ownerToken != 0u ||
        signature->ownerIndex != ZR_CALL_BINDING_SLOT_NONE)
        return typed_fail(entry, diagnostic, ZR_CALL_BINDING_INVALID_TOKEN, 0u, 0u);
    if (signature->signatureHash != entry->binding.contract.signatureHash)
        return typed_fail(entry, diagnostic, ZR_CALL_BINDING_SIGNATURE_MISMATCH,
                entry->binding.contract.signatureHash, signature->signatureHash);
    memset(&entry->binding.target, 0, sizeof(entry->binding.target));
    entry->binding.generation = function->callBindingGeneration;
    return ZR_TRUE;
}

TZrBool ZrCore_CallBinding_PrepareTypedCall(SZrState *state, SZrFunction *function,
        SZrFunctionCallSiteCacheEntry *entry, const SZrTypeValue *callable,
        SZrCallBindingDiagnostic *diagnostic) {
    SZrFunction *metadata;
    SZrCallBindingTarget previous = entry->binding.target;
    SZrCallBindingTarget target = {0};
    TZrUInt64 actualHash;
    memset(&entry->binding.target, 0, sizeof(entry->binding.target));
    if (ZrCore_CallBinding_Validate(&entry->binding, function->callBindingGeneration, diagnostic) != ZR_CALL_BINDING_OK)
        return ZR_FALSE;
    metadata = ZrCore_Closure_GetMetadataFunctionFromValue(state, callable);
    actualHash = ZrCore_CallBinding_FunctionSignatureHash(metadata);
    if (metadata == ZR_NULL && state->global->typedCallBindingResolver != ZR_NULL) {
        if (!state->global->typedCallBindingResolver(state, callable, &target, &actualHash,
                    state->global->typedCallBindingResolverUserData))
            return typed_fail(entry, diagnostic, ZR_CALL_BINDING_MISSING_CONTRACT, 0u, 0u);
    } else if (metadata != ZR_NULL && callable->isNative && callable->type == ZR_VALUE_TYPE_CLOSURE) {
        SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, callable->value.object);
        const SZrAotCodeRegistration *registration = metadata->metadataCodeRegistration;
        TZrBool found = ZR_FALSE;
        if (registration != ZR_NULL && registration->functionPointers != ZR_NULL && registration->methodInfos != ZR_NULL) {
            for (TZrUInt32 index = 0u; index < registration->functionCount && index < registration->methodInfoCount; ++index) {
                if (registration->functionPointers[index] != (FZrAotEntryThunk)closure->nativeFunction) continue;
                if (found || registration->methodInfos[index] == ZR_NULL || registration->methodInfos[index]->invoker == ZR_NULL)
                    return typed_fail(entry, diagnostic, ZR_CALL_BINDING_AMBIGUOUS_TARGET, 0u, 0u);
                target.targetKind = ZR_CALL_BINDING_TARGET_AOT;
                target.aot.thunk = registration->functionPointers[index];
                target.aot.methodInfo = registration->methodInfos[index];
                target.aot.invoker = registration->methodInfos[index]->invoker;
                found = ZR_TRUE;
            }
        }
        if (!found) return typed_fail(entry, diagnostic, ZR_CALL_BINDING_TARGET_NOT_FOUND, 0u, 0u);
        target.targetGeneration = metadata->callBindingGeneration;
    } else if (metadata != ZR_NULL) {
        target.targetKind = ZR_CALL_BINDING_TARGET_VM;
        target.vm.function = metadata;
        target.targetGeneration = metadata->callBindingGeneration;
    }
    if (actualHash == 0u || actualHash != entry->binding.contract.signatureHash)
        return typed_fail(entry, diagnostic, ZR_CALL_BINDING_SIGNATURE_MISMATCH,
                entry->binding.contract.signatureHash, actualHash);
    if (previous.callableObject == callable->value.object &&
        previous.targetGeneration != 0u &&
        previous.targetGeneration != target.targetGeneration)
        return typed_fail(entry, diagnostic, ZR_CALL_BINDING_STALE_GENERATION,
                previous.targetGeneration, target.targetGeneration);
    target.callableObject = callable->value.object;
    /* This is a metadata witness. The live value remains the invocation target,
     * so repeated calls can use distinct closures with the same signature. */
    entry->binding.target = target;
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), target.callableObject);
    if (metadata != ZR_NULL)
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), ZR_CAST_RAW_OBJECT_AS_SUPER(metadata));
    ++entry->runtimeHitCount;
    return ZR_TRUE;
}
