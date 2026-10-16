#ifndef ZR_VM_CORE_FUNCTION_CALL_SPREAD_INTERNAL_H
#define ZR_VM_CORE_FUNCTION_CALL_SPREAD_INTERNAL_H

#include "zr_vm_core/function.h"
#include "zr_vm_core/function_call_spread.h"

/* 解释器的展开调用入口；outInvocationStarted 区分展开失败与 PreCall 已启动但同步返回。 */
struct SZrCallInfo *ZrCore_Function_CallSpread_PreCallPrepared(
        struct SZrState *state,
        TZrStackValuePointer callWindow,
        TZrSize prefixArgumentCount,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination,
        TZrBool *outInvocationStarted);

#endif
