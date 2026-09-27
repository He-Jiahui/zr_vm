#ifndef ZR_VM_LIBRARY_NATIVE_CALL_PLAN_H
#define ZR_VM_LIBRARY_NATIVE_CALL_PLAN_H

#include "zr_vm_library/conf.h"
#include "zr_vm_core/native_call_contract.h"

/** @brief 按 core 的公共 Native 契约建立可持久化调用计划，统一 C、LLVM 和 FFI 入口。
 * @note 计划和诊断的数据布局由 core 定义；此层额外检查库运行时能力位。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeCall_Prepare(
        const SZrNativeCallRequest *request,
        SZrNativeCallPlan *plan,
        SZrNativeCallDiagnostic *diagnostic);

/** @brief 拒绝直接执行只有稳定身份、没有进程内地址的持久化计划。
 * @return 始终失败；有效计划报告 INVOKE_UNRESOLVED，调用方须使用 InvokeResolved。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeCall_Invoke(
        const SZrNativeCallPlan *plan,
        SZrState *state,
        SZrNativeCallDiagnostic *diagnostic);
/** @brief 在调用点提供进程内解析器后执行计划，并由 core 处理调用诊断。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeCall_InvokeResolved(
        const SZrNativeCallPlan *plan,
        SZrState *state,
        FZrNativeCallResolvedInvoker invoker,
        TZrPtr userData,
        SZrNativeCallDiagnostic *diagnostic);

#endif /* ZR_VM_LIBRARY_NATIVE_CALL_PLAN_H */
