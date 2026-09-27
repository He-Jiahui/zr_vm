#include "zr_vm_library/native_call_plan.h"

#include "zr_vm_library/native_binding.h"

#include <string.h>

/* 执行期再次限制计划的能力位，防止序列化计划绕过 Prepare 的验证。 */
static TZrBool native_call_plan_capabilities_supported(
        const SZrNativeCallPlan *plan,
        SZrNativeCallDiagnostic *diagnostic) {
    TZrUInt64 unsupported;

    if (plan == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT;
            diagnostic->phase = ZR_NATIVE_CALL_PHASE_VALIDATE;
        }
        return ZR_FALSE;
    }
    unsupported = plan->requiredCapabilities &
                  ~ZR_VM_NATIVE_RUNTIME_CAPABILITIES;
    if (unsupported == 0u) {
        return ZR_TRUE;
    }
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT;
        diagnostic->phase = ZR_NATIVE_CALL_PHASE_VALIDATE;
        diagnostic->sourceId = plan->sourceId;
        diagnostic->expected = ZR_VM_NATIVE_RUNTIME_CAPABILITIES;
        diagnostic->actual = plan->requiredCapabilities;
    }
    return ZR_FALSE;
}

/* 在 core 生成持久化计划前，先按库运行时能力集合拒绝不可执行契约。 */
TZrBool ZrLibrary_NativeCall_Prepare(
        const SZrNativeCallRequest *request,
        SZrNativeCallPlan *plan,
        SZrNativeCallDiagnostic *diagnostic) {
    if (request != ZR_NULL && request->contract != ZR_NULL &&
        (request->contract->requiredCapabilities &
         ~ZR_VM_NATIVE_RUNTIME_CAPABILITIES) != 0u) {
        ZrCore_NativeCall_DiagnosticClear(diagnostic);
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT;
            diagnostic->phase = ZR_NATIVE_CALL_PHASE_VALIDATE;
            diagnostic->sourceId = request->contract->symbolId != 0u
                                           ? request->contract->symbolId
                                           : request->contract->declaringModuleId;
            diagnostic->expected = ZR_VM_NATIVE_RUNTIME_CAPABILITIES;
            diagnostic->actual = request->contract->requiredCapabilities;
        }
        if (plan != ZR_NULL) {
            memset(plan, 0, sizeof(*plan));
        }
        return ZR_FALSE;
    }
    return ZrCore_NativeCall_Prepare(request, plan, diagnostic);
}

/* 计划不携带进程内地址；此入口只返回需显式解析的诊断。 */
TZrBool ZrLibrary_NativeCall_Invoke(
        const SZrNativeCallPlan *plan,
        SZrState *state,
        SZrNativeCallDiagnostic *diagnostic) {
    if (plan == ZR_NULL || state == ZR_NULL) {
        ZrCore_NativeCall_DiagnosticClear(diagnostic);
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT;
            diagnostic->phase = ZR_NATIVE_CALL_PHASE_VALIDATE;
        }
        return ZR_FALSE;
    }
    if (!ZrCore_NativeCall_ValidatePlan(plan, diagnostic)) {
        return ZR_FALSE;
    }
    if (!native_call_plan_capabilities_supported(plan, diagnostic)) {
        return ZR_FALSE;
    }
    if (diagnostic != ZR_NULL) {
        ZrCore_NativeCall_DiagnosticClear(diagnostic);
        diagnostic->status = ZR_NATIVE_CALL_STATUS_INVOKE_UNRESOLVED;
        diagnostic->phase = ZR_NATIVE_CALL_PHASE_INVOKE;
        diagnostic->sourceId = plan->sourceId;
    }
    return ZR_FALSE;
}

/* 调用点注入当前进程解析器，core 才能把稳定计划绑定到可执行目标。 */
TZrBool ZrLibrary_NativeCall_InvokeResolved(
        const SZrNativeCallPlan *plan,
        SZrState *state,
        FZrNativeCallResolvedInvoker invoker,
        TZrPtr userData,
        SZrNativeCallDiagnostic *diagnostic) {
    if (plan == ZR_NULL || state == ZR_NULL) {
        ZrCore_NativeCall_DiagnosticClear(diagnostic);
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT;
            diagnostic->phase = ZR_NATIVE_CALL_PHASE_VALIDATE;
        }
        return ZR_FALSE;
    }
    if (!ZrCore_NativeCall_ValidatePlan(plan, diagnostic)) {
        return ZR_FALSE;
    }
    if (!native_call_plan_capabilities_supported(plan, diagnostic)) {
        return ZR_FALSE;
    }
    return ZrCore_NativeCall_InvokeResolved(
            state, plan, invoker, userData, diagnostic);
}
