#include "zr_vm_core/call_binding.h"
#include "call_binding_contract_internal.h"

#include <string.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/closure.h"

/* Runtime-path diagnostics share the same pure status/field writer as the
 * public contract check and ExecIR-owned row validation. */
/* 为运行时拒绝补状态和差值，沿用已初始化的位置字段；此处不清诊断或释放目标。 */
static EZrCallBindingStatus binding_fail(SZrCallBindingDiagnostic *diagnostic,
        EZrCallBindingStatus status, TZrUInt64 expected, TZrUInt64 actual) {
    return zr_core_call_binding_contract_fail(diagnostic, status, expected,
                                              actual);
}

/** @brief 供持久化和链接入口复用静态契约检查，避免 ExecIR 与运行时采用不同的字段规则。
 * @note 这里只检查 token 形状与字段组合，不查元数据实体、不重算哈希，也不验证运行时目标。
 */
EZrCallBindingStatus ZrCore_CallBinding_CheckContract(const SZrCallBindingContract *contract,
                                                      SZrCallBindingDiagnostic *diagnostic) {
    return zr_core_call_binding_check_contract(contract, diagnostic);
}

/** @brief 对照编译期预期和当前 provider 契约，按首个差异给出可定位的链接状态。
 * @note 先检查预期，合法后才检查实际契约；不按名称寻找替代目标，也不持有或安装运行时入口。
 */
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

/** @brief 撤销可丢弃的运行时见证，保留 token 契约供后续重新链接。
 * @note 不释放目标对象或指令映射；目标保活和访问同步仍由缓存拥有者负责。
 */
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

/** @brief 在模块卸载或重挂前使整张函数图的旧调用目标失效。
 * @note 先完整收集去重图再推进；收集失败不执行代际推进回调。图和缓存须保持有效，接口不提供并发同步。
 */
TZrBool ZrCore_CallBinding_AdvanceGeneration(struct SZrFunction *function) {
    return ZrCore_CallBinding_VisitFunctions(function, call_binding_advance_generation, ZR_NULL);
}

/* 目标见证与持久化契约分开验证：typed 和多态调用点允许链接时尚无具体目标。 */
/* 检查已有目标的入口和代际见证；typed/多态的延迟选择不是丢失目标，接收者布局另由 PrepareMember 核查。 */
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

/** @brief 调用前检查静态契约、所属函数代际及已有目标见证，失败时撤销运行时目标。
 * @note 被读取的函数或闭包元数据须仍有效；本检查不建立 GC root，也不替代接收者布局检查。
 */
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

/** @brief 从唯一 token 候选重建调用目标，使失败路径保留契约而不遗留旧入口。
 * @note expected 可指向 binding->contract；候选只在本次读取，复制的目标引用仍依赖拥有者保活和同步。
 */
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
/* TODO: 非零候选位置在 CompareContracts 的检查中会被清零；从多候选 Resolve 入口核查 candidateIndex 的诊断用途并补位置测试。 */
    status = ZrCore_CallBinding_CompareContracts(&contractCopy, &selected->contract, diagnostic);
    if (status != ZR_CALL_BINDING_OK) return status;
    binding->generation = selected->generation;
    binding->target = selected->target;
    return ZrCore_CallBinding_Validate(binding, generation, diagnostic);
}

/* 面向运行错误的有限状态名；未知数值保持独立的兜底名称。 */
/** @brief 为解释器、模块加载和 AOT 错误报告返回稳定的状态短名称。
 * @return 借用静态字符串；未知值使用兜底名称，无需释放。
 */
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
