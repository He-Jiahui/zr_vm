#include "zr_vm_library/native_call_plan.h"

#include "zr_vm_library/native_binding.h"

#include <string.h>

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
