#include "zr_vm_core/call_binding.h"

#include <string.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/closure.h"

static EZrCallBindingStatus binding_fail(SZrCallBindingDiagnostic *diagnostic,
        EZrCallBindingStatus status, TZrUInt64 expected, TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return status;
}

static void binding_diagnostic_init(SZrCallBindingDiagnostic *diagnostic,
                                    const SZrCallBindingContract *contract) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->targetMetadataToken = contract != ZR_NULL ? contract->targetMetadataToken : 0u;
    }
}

static TZrBool binding_token_is(TZrMetadataToken token, TZrUInt32 table) {
    return ZR_METADATA_TOKEN_TABLE(token) == table && ZR_METADATA_TOKEN_RID(token) != 0u;
}

EZrCallBindingStatus ZrCore_CallBinding_CheckContract(const SZrCallBindingContract *contract,
                                                      SZrCallBindingDiagnostic *diagnostic) {
    binding_diagnostic_init(diagnostic, contract);
    if (contract == ZR_NULL) {
        return binding_fail(diagnostic, ZR_CALL_BINDING_INVALID_ARGUMENT, 0u, 0u);
    }
    if (contract->bindingKind < ZR_CALL_BINDING_DIRECT ||
        contract->bindingKind > ZR_CALL_BINDING_TYPED_FUNCTION ||
        contract->signatureHash == 0u || contract->moduleSignatureHash == 0u) {
        return binding_fail(diagnostic, ZR_CALL_BINDING_MISSING_CONTRACT, 0u, 0u);
    }
    if (!binding_token_is(contract->signatureToken, ZR_METADATA_TABLE_SIGNATURE) ||
        (!(contract->bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION && contract->targetMetadataToken == 0u) &&
         !binding_token_is(contract->targetMetadataToken, ZR_METADATA_TABLE_MEMBER_DEF) &&
         !binding_token_is(contract->targetMetadataToken, ZR_METADATA_TABLE_MEMBER_REF)) ||
        (contract->ownerTypeToken != 0u &&
         !binding_token_is(contract->ownerTypeToken, ZR_METADATA_TABLE_TYPE_DEF) &&
         !binding_token_is(contract->ownerTypeToken, ZR_METADATA_TABLE_TYPE_REF) &&
         !binding_token_is(contract->ownerTypeToken, ZR_METADATA_TABLE_TYPE_SPEC))) {
        return binding_fail(diagnostic, ZR_CALL_BINDING_INVALID_TOKEN, 0u, 0u);
    }
    if (contract->operation > ZR_CALL_BINDING_OPERATION_META || contract->reserved0 != 0u ||
        contract->reserved1 != 0u) {
        return binding_fail(diagnostic, ZR_CALL_BINDING_INVALID_ARGUMENT, 0u, 0u);
    }
    if ((contract->ownerTypeToken == 0u && (contract->layoutVersion != 0u || contract->layoutHash != 0u)) ||
        (contract->ownerTypeToken != 0u && (contract->layoutVersion == 0u || contract->layoutHash == 0u))) {
        return binding_fail(diagnostic, ZR_CALL_BINDING_MISSING_CONTRACT, 0u, 0u);
    }
    if (contract->bindingKind == ZR_CALL_BINDING_VIRTUAL || contract->bindingKind == ZR_CALL_BINDING_INTERFACE) {
        if (contract->ownerTypeToken == 0u || contract->dispatchSlot == ZR_CALL_BINDING_SLOT_NONE) {
            return binding_fail(diagnostic, ZR_CALL_BINDING_INVALID_SLOT, 0u, contract->dispatchSlot);
        }
    } else if (contract->dispatchSlot != ZR_CALL_BINDING_SLOT_NONE) {
        return binding_fail(diagnostic, ZR_CALL_BINDING_INVALID_SLOT, ZR_CALL_BINDING_SLOT_NONE, contract->dispatchSlot);
    }
    return ZR_CALL_BINDING_OK;
}

EZrCallBindingStatus ZrCore_CallBinding_CompareContracts(const SZrCallBindingContract *expected,
        const SZrCallBindingContract *actual, SZrCallBindingDiagnostic *diagnostic) {
    EZrCallBindingStatus status = ZrCore_CallBinding_CheckContract(expected, diagnostic);
    if (status != ZR_CALL_BINDING_OK) return status;
    status = ZrCore_CallBinding_CheckContract(actual, diagnostic);
    if (status != ZR_CALL_BINDING_OK) return status;
#define MATCH(FIELD, STATUS) \
    if (expected->FIELD != actual->FIELD) \
        return binding_fail(diagnostic, STATUS, expected->FIELD, actual->FIELD)
    MATCH(targetMetadataToken, ZR_CALL_BINDING_TARGET_NOT_FOUND);
    MATCH(moduleSignatureHash, ZR_CALL_BINDING_MODULE_MISMATCH);
    MATCH(signatureToken, ZR_CALL_BINDING_SIGNATURE_MISMATCH);
    MATCH(signatureHash, ZR_CALL_BINDING_SIGNATURE_MISMATCH);
    MATCH(ownerTypeToken, ZR_CALL_BINDING_LAYOUT_MISMATCH);
    MATCH(layoutVersion, ZR_CALL_BINDING_LAYOUT_MISMATCH);
    MATCH(layoutHash, ZR_CALL_BINDING_LAYOUT_MISMATCH);
    MATCH(bindingKind, ZR_CALL_BINDING_TARGET_KIND_MISMATCH);
    MATCH(operation, ZR_CALL_BINDING_TARGET_KIND_MISMATCH);
    MATCH(dispatchSlot, ZR_CALL_BINDING_INVALID_SLOT);
#undef MATCH
    return ZR_CALL_BINDING_OK;
}

void ZrCore_CallBinding_Invalidate(SZrCallBinding *binding) {
    if (binding != ZR_NULL) {
        binding->generation = 0u;
        memset(&binding->target, 0, sizeof(binding->target));
    }
}

static TZrBool call_binding_advance_generation(SZrFunction *function, void *context) {
    ZR_UNUSED_PARAMETER(context);
    if (function->callBindingGeneration == UINT64_MAX) {
        function->callBindingGeneration = 1u;
    } else {
        ++function->callBindingGeneration;
        if (function->callBindingGeneration == 0u) {
            function->callBindingGeneration = 1u;
        }
    }
    if (function->callSiteCaches != ZR_NULL) {
        for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
            ZrCore_CallBinding_Invalidate(&function->callSiteCaches[index].binding);
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_CallBinding_AdvanceGeneration(struct SZrFunction *function) {
    return ZrCore_CallBinding_VisitFunctions(function, call_binding_advance_generation, ZR_NULL);
}

static EZrCallBindingStatus binding_validate_target(const SZrCallBinding *binding,
                                                     SZrCallBindingDiagnostic *diagnostic) {
    const SZrCallBindingTarget *target = &binding->target;
    if (target->targetKind == ZR_CALL_BINDING_TARGET_NONE &&
        binding->contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) {
        return ZR_CALL_BINDING_OK;
    }
    if (target->targetKind == ZR_CALL_BINDING_TARGET_NONE &&
        (binding->contract.bindingKind == ZR_CALL_BINDING_VIRTUAL ||
         binding->contract.bindingKind == ZR_CALL_BINDING_INTERFACE)) {
        /* A polymorphic site may be linked to its receiver slot only when a
         * concrete object is available.  The contract and generation remain
         * valid until PrepareMember selects that target. */
        return ZR_CALL_BINDING_OK;
    }
    if ((binding->contract.bindingKind == ZR_CALL_BINDING_VIRTUAL ||
         binding->contract.bindingKind == ZR_CALL_BINDING_INTERFACE) &&
        target->dispatchSlotCount != 0u && binding->contract.dispatchSlot >= target->dispatchSlotCount) {
        return binding_fail(diagnostic, ZR_CALL_BINDING_INVALID_SLOT,
                target->dispatchSlotCount, binding->contract.dispatchSlot);
    }
    switch (target->targetKind) {
        case ZR_CALL_BINDING_TARGET_VM:
            if (target->vm.function != ZR_NULL && target->targetGeneration != 0u &&
                target->vm.function->callBindingGeneration != target->targetGeneration)
                return binding_fail(diagnostic, ZR_CALL_BINDING_STALE_GENERATION,
                        target->targetGeneration, target->vm.function->callBindingGeneration);
            if (target->vm.function != ZR_NULL) return ZR_CALL_BINDING_OK;
            break;
        case ZR_CALL_BINDING_TARGET_NATIVE:
            if (target->callableObject != ZR_NULL && target->callableObject->isNative &&
                target->callableObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE &&
                ((SZrClosureNative *)target->callableObject)->callBindingGeneration != target->targetGeneration)
                return binding_fail(diagnostic, ZR_CALL_BINDING_STALE_GENERATION,
                        target->targetGeneration,
                        ((SZrClosureNative *)target->callableObject)->callBindingGeneration);
            if (target->native.function != ZR_NULL) return ZR_CALL_BINDING_OK;
            break;
        case ZR_CALL_BINDING_TARGET_AOT:
            if (target->callableObject != ZR_NULL && target->targetGeneration != 0u) {
                SZrFunction *function = ZR_NULL;
                if (target->callableObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION && !target->callableObject->isNative)
                    function = (SZrFunction *)target->callableObject;
                else if (target->callableObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE)
                    function = target->callableObject->isNative
                            ? ((SZrClosureNative *)target->callableObject)->aotShimFunction
                            : ((SZrClosure *)target->callableObject)->function;
                if (function != ZR_NULL && function->callBindingGeneration != target->targetGeneration)
                    return binding_fail(diagnostic, ZR_CALL_BINDING_STALE_GENERATION,
                            target->targetGeneration, function->callBindingGeneration);
            }
            if (target->aot.thunk != ZR_NULL && target->aot.methodInfo != ZR_NULL &&
                target->aot.invoker != ZR_NULL) return ZR_CALL_BINDING_OK;
            break;
        default: break;
    }
    return binding_fail(diagnostic, ZR_CALL_BINDING_TARGET_NOT_FOUND, 0u, target->targetKind);
}

EZrCallBindingStatus ZrCore_CallBinding_Validate(SZrCallBinding *binding,
        TZrUInt64 generation, SZrCallBindingDiagnostic *diagnostic) {
    EZrCallBindingStatus status;
    if (binding == ZR_NULL) return binding_fail(diagnostic, ZR_CALL_BINDING_INVALID_ARGUMENT, 0u, 0u);
    status = ZrCore_CallBinding_CheckContract(&binding->contract, diagnostic);
    if (status == ZR_CALL_BINDING_OK && (generation == 0u || binding->generation != generation)) {
        status = binding_fail(diagnostic, ZR_CALL_BINDING_STALE_GENERATION, binding->generation, generation);
    }
    if (status == ZR_CALL_BINDING_OK) status = binding_validate_target(binding, diagnostic);
    if (status != ZR_CALL_BINDING_OK) ZrCore_CallBinding_Invalidate(binding);
    return status;
}

EZrCallBindingStatus ZrCore_CallBinding_Resolve(const SZrCallBindingContract *expected,
        const SZrCallBindingCandidate *candidates, TZrUInt32 count, TZrUInt64 generation,
        SZrCallBinding *binding, SZrCallBindingDiagnostic *diagnostic) {
    SZrCallBindingContract contractCopy;
    const SZrCallBindingCandidate *selected = ZR_NULL;
    EZrCallBindingStatus status;
    if (binding == ZR_NULL || expected == ZR_NULL || (count != 0u && candidates == ZR_NULL)) {
        ZrCore_CallBinding_Invalidate(binding);
        return binding_fail(diagnostic, ZR_CALL_BINDING_INVALID_ARGUMENT, 0u, 0u);
    }
    contractCopy = *expected;
    ZrCore_CallBinding_Invalidate(binding);
    binding->contract = contractCopy;
    status = ZrCore_CallBinding_CheckContract(&contractCopy, diagnostic);
    if (status != ZR_CALL_BINDING_OK) return status;
    for (TZrUInt32 index = 0u; index < count; ++index) {
        if (candidates[index].contract.targetMetadataToken != contractCopy.targetMetadataToken) continue;
        if (selected != ZR_NULL) return binding_fail(diagnostic, ZR_CALL_BINDING_AMBIGUOUS_TARGET, 1u, 2u);
        selected = &candidates[index];
        if (diagnostic != ZR_NULL) diagnostic->candidateIndex = index;
    }
    if (selected == ZR_NULL) return binding_fail(diagnostic, ZR_CALL_BINDING_TARGET_NOT_FOUND, contractCopy.targetMetadataToken, 0u);
    status = ZrCore_CallBinding_CompareContracts(&contractCopy, &selected->contract, diagnostic);
    if (status != ZR_CALL_BINDING_OK) return status;
    binding->generation = selected->generation;
    binding->target = selected->target;
    return ZrCore_CallBinding_Validate(binding, generation, diagnostic);
}

const char *ZrCore_CallBinding_StatusName(EZrCallBindingStatus status) {
    switch (status) {
        case ZR_CALL_BINDING_OK: return "ok";
        case ZR_CALL_BINDING_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_CALL_BINDING_MISSING_CONTRACT: return "missing-contract";
        case ZR_CALL_BINDING_INVALID_TOKEN: return "invalid-token";
        case ZR_CALL_BINDING_TARGET_NOT_FOUND: return "target-not-found";
        case ZR_CALL_BINDING_AMBIGUOUS_TARGET: return "ambiguous-target";
        case ZR_CALL_BINDING_SIGNATURE_MISMATCH: return "signature-mismatch";
        case ZR_CALL_BINDING_MODULE_MISMATCH: return "module-mismatch";
        case ZR_CALL_BINDING_LAYOUT_MISMATCH: return "layout-mismatch";
        case ZR_CALL_BINDING_INVALID_SLOT: return "invalid-slot";
        case ZR_CALL_BINDING_STALE_GENERATION: return "stale-generation";
        case ZR_CALL_BINDING_TARGET_KIND_MISMATCH: return "target-kind-mismatch";
        case ZR_CALL_BINDING_INVALID_RELOCATION: return "invalid-relocation";
        default: return "unknown-link-error";
    }
}
