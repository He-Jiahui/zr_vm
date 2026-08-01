#ifndef ZR_VM_CORE_DEBUG_EVALUATION_CONTEXT_INTERNAL_H
#define ZR_VM_CORE_DEBUG_EVALUATION_CONTEXT_INTERNAL_H

#include "zr_vm_core/debug.h"

struct SZrCallInfo;
struct SZrFunction;

EZrDebugEvaluationContextStatus debug_evaluation_context_validate(
        struct SZrState *state,
        const SZrDebugEvaluationContext *context,
        struct SZrCallInfo **outCallInfo,
        struct SZrFunction **outFunction);

void debug_evaluation_context_snapshot_value(
        struct SZrState *state,
        struct SZrTypeValue *destination,
        const struct SZrTypeValue *source);

#endif
