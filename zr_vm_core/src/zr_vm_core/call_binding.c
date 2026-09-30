#include "zr_vm_core/call_binding.h"
#include "call_binding_contract_internal.h"

#include <string.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/closure.h"

/* Runtime-path diagnostics share the same pure status/field writer as the
 * public contract check and ExecIR-owned row validation. */
static EZrCallBindingStatus binding_fail(SZrCallBindingDiagnostic *diagnostic,
        EZrCallBindingStatus status, TZrUInt64 expected, TZrUInt64 actual) {
    return zr_core_call_binding_contract_fail(diagnostic, status, expected,
                                              actual);
}

EZrCallBindingStatus ZrCore_CallBinding_CheckContract(const SZrCallBindingContract *contract,
                                                      SZrCallBindingDiagnostic *diagnostic) {
    return zr_core_call_binding_check_contract(contract, diagnostic);
}

EZrCallBindingStatus ZrCore_CallBinding_CompareContracts(const SZrCallBindingContract *expected,
        const SZrCallBindingContract *actual, SZrCallBindingDiagnostic *diagnostic) {
    EZrCallBindingStatus status = ZrCore_CallBinding_CheckContract(expected, diagnostic);
    if (status != ZR_CALL_BINDING_OK) return status;
    status = ZrCore_CallBinding_CheckContract(actual, diagnostic);
    if (status != ZR_CALL_BINDING_OK) return status;
    /* 差异分类供导入、AOT 注册及用户错误共享，不在目标不匹配时退回名称解析。 */
#define MATCH(FIELD, STATUS) \
    if (expected->FIELD != actual->FIELD) \
        return zr_core_call_binding_contract_fail(diagnostic, STATUS, expected->FIELD, actual->FIELD)
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

/* 模块卸载沿完整函数图撤销目标；零代际保留为“未链接”哨兵。 */
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

/* 目标见证与持久化契约分开验证：typed 和多态调用点允许链接时尚无具体目标。 */
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
    /* 失效保留契约，导入模块重载和下一次链接仍可按 token 重新定位。 */
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
    /* expected 允许指向 binding->contract；必须先复制，再清除旧目标。 */
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

/* 面向运行错误的有限状态名；未知数值保持独立的兜底名称。 */
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
