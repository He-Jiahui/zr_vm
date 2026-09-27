#ifndef ZR_VM_CORE_DEBUG_EVALUATION_CONTEXT_INTERNAL_H
#define ZR_VM_CORE_DEBUG_EVALUATION_CONTEXT_INTERNAL_H

#include "zr_vm_core/debug.h"

struct SZrCallInfo;
struct SZrFunction;

/* 重新验证暂停帧仍在调用链中且 generation/指令偏移一致。 */
EZrDebugEvaluationContextStatus debug_evaluation_context_validate(
        struct SZrState *state,
        const SZrDebugEvaluationContext *context,
        struct SZrCallInfo **outCallInfo,
        struct SZrFunction **outFunction);

/* 复制调试可见值时降为借用所有权，避免把帧内值误当可转移引用。 */
void debug_evaluation_context_snapshot_value(
        struct SZrState *state,
        struct SZrTypeValue *destination,
        const struct SZrTypeValue *source);

#endif
