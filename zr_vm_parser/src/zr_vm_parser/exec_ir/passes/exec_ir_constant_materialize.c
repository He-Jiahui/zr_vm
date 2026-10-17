#include "exec_ir/exec_ir_pass_internal.h"
#include "exec_ir/exec_ir_mutation_internal.h"
#include "zr_vm_common/zr_type_conf.h"

#include <limits.h>
#include <stdint.h>

TZrBool ZrParser_ExecIr_MaterializeInt64Constant(
        SZrExecIrModule *owner, SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrExecIrInstructionId instructionId,
        TZrUInt64 bits, TZrUInt32 *outIndex, TZrBool *outReady,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt64 requested;
    TZrUInt64 capacity;
    SZrExecIrConstant constant;
    SZrExecIrRange range;

    if (outIndex != ZR_NULL) *outIndex = 0u;
    if (outReady != ZR_NULL) *outReady = ZR_FALSE;
    if (owner == ZR_NULL || function == ZR_NULL || context == ZR_NULL ||
        outIndex == ZR_NULL || outReady == ZR_NULL ||
        owner->functions == ZR_NULL ||
        owner->functionCount > owner->functionCapacity ||
        owner->constants == ZR_NULL || owner->constantCount == 0u ||
        owner->constantCount > owner->constantCapacity ||
        instructionId == 0u || instructionId > function->instructionCount)
        goto invalid_argument;
    for (index = 0u; index < owner->functionCount; ++index) {
        if (&owner->functions[index] == function) break;
    }
    if (index == owner->functionCount) goto invalid_argument;
    if ((TZrUInt64)owner->constantCapacity >
            (TZrUInt64)SIZE_MAX / sizeof(*owner->constants))
        goto capacity_overflow;

    context->constants = owner->constants;
    context->constantCount = owner->constantCount;
    for (index = 0u; index < owner->constantCount; ++index) {
        const SZrExecIrConstant *entry;
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u))
            return ZR_TRUE;
        entry = &owner->constants[index];
        if (entry->typeToken == ZR_VALUE_TYPE_INT64 &&
            entry->flags == 0u && entry->bits == bits) {
            if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u))
                return ZR_TRUE;
            *outIndex = index;
            *outReady = ZR_TRUE;
            return ZR_TRUE;
        }
    }

    requested = (TZrUInt64)owner->constantCount + 1u;
    if (requested > UINT32_MAX ||
        requested > (TZrUInt64)SIZE_MAX / sizeof(*owner->constants))
        goto capacity_overflow;
    /* Nonempty validated candidates have a nonzero capacity.  Match Core's
     * doubling rule so a byte overflow is distinguished from allocator OOM. */
    capacity = owner->constantCapacity;
    while (capacity < requested) {
        if (capacity > (TZrUInt64)UINT32_MAX / 2u) {
            capacity = requested;
            break;
        }
        capacity *= 2u;
    }
    if (capacity > (TZrUInt64)SIZE_MAX / sizeof(*owner->constants))
        goto capacity_overflow;
    /* Reserve both append and caller rewrite before changing the pool.  The
     * caller has no allocation, budget check or other failure after ready. */
    if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 2u))
        return ZR_TRUE;
    constant.typeToken = ZR_VALUE_TYPE_INT64;
    constant.flags = 0u;
    constant.bits = bits;
    if (!ZrCore_ExecIr_ModuleAppendConstant(owner, &constant, 1u, &range)) {
        ZrParser_ExecIr_PassDiagnostic(
                diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                function, 0u, instructionId, (TZrUInt32)requested,
                owner->constantCapacity);
        return ZR_FALSE;
    }
    context->constants = owner->constants;
    context->constantCount = owner->constantCount;
    *outIndex = range.start;
    *outReady = ZR_TRUE;
    return ZR_TRUE;

capacity_overflow:
    ZrParser_ExecIr_PassDiagnostic(
            diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
            function, 0u, instructionId, UINT32_MAX, owner->constantCount);
    return ZR_FALSE;

invalid_argument:
    ZrParser_ExecIr_PassDiagnostic(
            diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
            function, 0u, instructionId, 0u, 0u);
    return ZR_FALSE;
}
