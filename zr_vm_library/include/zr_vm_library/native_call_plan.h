#ifndef ZR_VM_LIBRARY_NATIVE_CALL_PLAN_H
#define ZR_VM_LIBRARY_NATIVE_CALL_PLAN_H

#include "zr_vm_library/conf.h"
#include "zr_vm_core/native_call_contract.h"

/*
 * Library-facing spelling of the shared native-call boundary.  The plan and
 * diagnostic are owned by core; this header merely exposes the provider
 * entry points so C, LLVM and FFI users consume one contract.
 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeCall_Prepare(
        const SZrNativeCallRequest *request,
        SZrNativeCallPlan *plan,
        SZrNativeCallDiagnostic *diagnostic);

/* A persistent plan intentionally has no native address.  Callers resolving
 * a module/function for this process must use the explicit resolver form. */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeCall_Invoke(
        const SZrNativeCallPlan *plan,
        SZrState *state,
        SZrNativeCallDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeCall_InvokeResolved(
        const SZrNativeCallPlan *plan,
        SZrState *state,
        FZrNativeCallResolvedInvoker invoker,
        TZrPtr userData,
        SZrNativeCallDiagnostic *diagnostic);

#endif /* ZR_VM_LIBRARY_NATIVE_CALL_PLAN_H */
