#include "zr_vm_core/execution_binding_guard.h"

#include <string.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"

/* 可空诊断统一写入同一组分类与数值，避免各提前返回留下上次检查的字段。 */
static void guard_diag(SZrExecutionBindingGuardDiagnostic *diagnostic,
        EZrExecutionBindingGuardResult result, EZrCallBindingStatus status,
        TZrUInt32 targetKind, TZrUInt32 slot, TZrUInt64 expected, TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) return;
    diagnostic->result = result;
    diagnostic->bindingStatus = status;
    diagnostic->targetKind = targetKind;
    diagnostic->dispatchSlot = slot;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

/* 所有失败出口共用饱和 miss 统计；保留绑定和目标见证供调用方决定回退或失效。 */
static EZrExecutionBindingGuardResult guard_fail(
        const SZrExecutionBindingGuardInput *input,
        SZrExecutionBindingGuardDiagnostic *diagnostic,
        EZrExecutionBindingGuardResult result, EZrCallBindingStatus status,
        TZrUInt64 expected, TZrUInt64 actual) {
    if (input != ZR_NULL && input->cacheEntry != ZR_NULL &&
        input->cacheEntry->runtimeMissCount != UINT32_MAX)
        ++input->cacheEntry->runtimeMissCount;
    guard_diag(diagnostic, result, status,
            input != ZR_NULL && input->binding != ZR_NULL
                    ? input->binding->target.targetKind : ZR_CALL_BINDING_TARGET_NONE,
            input != ZR_NULL && input->binding != ZR_NULL
                    ? input->binding->contract.dispatchSlot : ZR_CALL_BINDING_SLOT_NONE,
            expected, actual);
    return result;
}

EZrExecutionBindingGuardResult ZrCore_Execution_CheckBindingGuard(
        const SZrExecutionBindingGuardInput *input,
        SZrExecutionBindingGuardDiagnostic *diagnostic) {
    const SZrCallBinding *binding;
    EZrCallBindingStatus status;
    TZrUInt32 slotCount;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (input == ZR_NULL || (binding = input->binding) == ZR_NULL ||
        input->activeGeneration == 0u)
        return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_TARGET_MISSING,
                ZR_CALL_BINDING_INVALID_ARGUMENT, 0u, 0u);

    /* 先判持久契约与所属函数代际，再考虑目标/shape；失配不能降为槽回退。 */
    status = ZrCore_CallBinding_CheckContract(&binding->contract, ZR_NULL);
    if (status != ZR_CALL_BINDING_OK)
        return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_CONTRACT_MISMATCH,
                status, 0u, 0u);
    if (binding->generation != input->activeGeneration)
        return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_STALE_GENERATION,
                ZR_CALL_BINDING_STALE_GENERATION, binding->generation,
                input->activeGeneration);
    if (input->expectedModuleSignatureHash != 0u &&
        input->expectedModuleSignatureHash != binding->contract.moduleSignatureHash)
        return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_CONTRACT_MISMATCH,
                ZR_CALL_BINDING_MODULE_MISMATCH, input->expectedModuleSignatureHash,
                binding->contract.moduleSignatureHash);
    if (input->expectedSignatureHash != 0u &&
        input->expectedSignatureHash != binding->contract.signatureHash)
        return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_CONTRACT_MISMATCH,
                ZR_CALL_BINDING_SIGNATURE_MISMATCH, input->expectedSignatureHash,
                binding->contract.signatureHash);
    if ((input->expectedLayoutVersion != 0u &&
         input->expectedLayoutVersion != binding->contract.layoutVersion) ||
        (input->expectedLayoutHash != 0u &&
         input->expectedLayoutHash != binding->contract.layoutHash))
        return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_CONTRACT_MISMATCH,
                ZR_CALL_BINDING_LAYOUT_MISMATCH, input->expectedLayoutHash,
                binding->contract.layoutHash);

    /* 所属调用点与目标函数的代际独立；VM 见证已过期时，接收者 shape
     * 改变也不能把失败降级为仍可使用的槽回退。零目标代际不参加此检查。 */
    if (binding->target.targetKind == ZR_CALL_BINDING_TARGET_VM &&
        binding->target.vm.function != ZR_NULL &&
        binding->target.targetGeneration != 0u &&
        binding->target.vm.function->callBindingGeneration !=
            binding->target.targetGeneration)
        return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_STALE_GENERATION,
                ZR_CALL_BINDING_STALE_GENERATION, binding->target.targetGeneration,
                binding->target.vm.function->callBindingGeneration);

    if (binding->contract.ownerTypeToken != 0u) {
        if (input->receiverPrototype == ZR_NULL)
            return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_RECEIVER_TYPE_MISMATCH,
                    ZR_CALL_BINDING_LAYOUT_MISMATCH, 0u, 0u);
        if (input->receiverShapeId != 0u &&
            input->receiverShapeId != input->receiverPrototype->shapeId)
            goto shape_miss;
        if (input->receiverShapeGeneration != 0u &&
            input->receiverShapeGeneration != input->receiverPrototype->shapeGeneration)
            goto shape_miss;
        slotCount = input->receiverPrototype->nextVirtualSlotIndex;
        if (binding->contract.dispatchSlot != ZR_CALL_BINDING_SLOT_NONE &&
            binding->contract.dispatchSlot >= slotCount)
            return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_INVALID_SLOT,
                    ZR_CALL_BINDING_INVALID_SLOT, slotCount, binding->contract.dispatchSlot);
    }

    status = ZR_CALL_BINDING_OK;
    switch (binding->target.targetKind) {
        case ZR_CALL_BINDING_TARGET_VM:
            if (binding->target.vm.function == ZR_NULL)
                status = ZR_CALL_BINDING_TARGET_NOT_FOUND;
            break;
        case ZR_CALL_BINDING_TARGET_NATIVE:
            if (binding->target.native.function == ZR_NULL)
                status = ZR_CALL_BINDING_TARGET_NOT_FOUND;
            break;
        case ZR_CALL_BINDING_TARGET_AOT:
            if (binding->target.aot.thunk == ZR_NULL ||
                binding->target.aot.methodInfo == ZR_NULL ||
                binding->target.aot.invoker == ZR_NULL)
                status = ZR_CALL_BINDING_TARGET_NOT_FOUND;
            break;
        case ZR_CALL_BINDING_TARGET_NONE:
            /* 类型化函数值及多态契约可先通过守卫，待后续拿到值/接收者
             * 再选目标；此 OK 不承诺已有可立即调用的函数指针。 */
            if (binding->contract.bindingKind != ZR_CALL_BINDING_TYPED_FUNCTION &&
                binding->contract.bindingKind != ZR_CALL_BINDING_VIRTUAL &&
                binding->contract.bindingKind != ZR_CALL_BINDING_INTERFACE)
                status = ZR_CALL_BINDING_TARGET_NOT_FOUND;
            break;
        default:
            status = ZR_CALL_BINDING_TARGET_KIND_MISMATCH;
            break;
    }
    if (status != ZR_CALL_BINDING_OK)
        return guard_fail(input, diagnostic, status == ZR_CALL_BINDING_STALE_GENERATION
                    ? ZR_EXECUTION_BINDING_GUARD_STALE_GENERATION
                    : ZR_EXECUTION_BINDING_GUARD_TARGET_MISSING, status, 0u, 0u);
    if (input->cacheEntry != ZR_NULL && input->cacheEntry->runtimeHitCount != UINT32_MAX)
        ++input->cacheEntry->runtimeHitCount;
    guard_diag(diagnostic, ZR_EXECUTION_BINDING_GUARD_OK, ZR_CALL_BINDING_OK,
            binding->target.targetKind, binding->contract.dispatchSlot, 0u, 0u);
    return ZR_EXECUTION_BINDING_GUARD_OK;

/* shape 见证不匹配时，只确认声明槽仍落在当前槽范围；不在此查询或执行槽目标。 */
shape_miss:
    if (input->allowSlotFallback &&
        binding->contract.dispatchSlot != ZR_CALL_BINDING_SLOT_NONE &&
        input->receiverPrototype != ZR_NULL &&
        binding->contract.dispatchSlot < input->receiverPrototype->nextVirtualSlotIndex) {
        return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_SLOT_FALLBACK,
                ZR_CALL_BINDING_LAYOUT_MISMATCH, input->receiverPrototype->shapeId,
                input->receiverShapeId);
    }
    return guard_fail(input, diagnostic, ZR_EXECUTION_BINDING_GUARD_SHAPE_MISS,
            ZR_CALL_BINDING_LAYOUT_MISMATCH,
            input->receiverPrototype != ZR_NULL ? input->receiverPrototype->shapeId : 0u,
            input->receiverShapeId);
}

void ZrCore_Execution_ResetBindingCache(struct SZrFunctionCallSiteCacheEntry *cacheEntry) {
    SZrCallBindingContract contract;
    SZrCallBindingLocation location;
    TZrUInt32 kind;
    TZrUInt32 instructionIndex;
    TZrUInt32 memberEntryIndex;
    TZrUInt32 deoptId;
    TZrUInt32 argumentCount;
    if (cacheEntry == ZR_NULL) return;
    /* 持久契约、重定位与静态调用点坐标跨失效保存；PIC 槽和临时计数重新积累。 */
    contract = cacheEntry->binding.contract;
    location = cacheEntry->bindingLocation;
    kind = cacheEntry->kind;
    instructionIndex = cacheEntry->instructionIndex;
    memberEntryIndex = cacheEntry->memberEntryIndex;
    deoptId = cacheEntry->deoptId;
    argumentCount = cacheEntry->argumentCount;
    memset(cacheEntry, 0, sizeof(*cacheEntry));
    cacheEntry->kind = kind;
    cacheEntry->instructionIndex = instructionIndex;
    cacheEntry->memberEntryIndex = memberEntryIndex;
    cacheEntry->deoptId = deoptId;
    cacheEntry->argumentCount = argumentCount;
    cacheEntry->binding.contract = contract;
    cacheEntry->bindingLocation = location;
    cacheEntry->binding.target.targetKind = ZR_CALL_BINDING_TARGET_NONE;
}

const char *ZrCore_Execution_BindingGuardResultName(EZrExecutionBindingGuardResult result) {
    switch (result) {
        case ZR_EXECUTION_BINDING_GUARD_OK: return "ok";
        case ZR_EXECUTION_BINDING_GUARD_STALE_GENERATION: return "stale-generation";
        case ZR_EXECUTION_BINDING_GUARD_CONTRACT_MISMATCH: return "contract-mismatch";
        case ZR_EXECUTION_BINDING_GUARD_RECEIVER_TYPE_MISMATCH: return "receiver-type-mismatch";
        case ZR_EXECUTION_BINDING_GUARD_SHAPE_MISS: return "shape-miss";
        case ZR_EXECUTION_BINDING_GUARD_SLOT_FALLBACK: return "slot-fallback";
        case ZR_EXECUTION_BINDING_GUARD_INVALID_SLOT: return "invalid-slot";
        case ZR_EXECUTION_BINDING_GUARD_TARGET_MISSING: return "target-missing";
        default: return "unknown";
    }
}
