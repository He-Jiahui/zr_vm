#include "compiler/compile_time/compile_time_aggregate.h"

#include "compiler/compile_time/comptime_runtime_contract.h"
#include "zr_vm_core/object.h"

TZrBool ct_reserve_aggregate_allocation(
        SZrCompilerState *cs,
        SZrFileRange location) {
    SZrParserComptimeBudgetTransaction transaction;
    EZrParserComptimeBudgetResource failedResource;
    TZrUInt64 failedAmount;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_ComptimeBudget_BeginTransaction(&cs->comptimeBudget, &transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                &transaction,
                ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                sizeof(SZrObject)) ||
        !ZrParser_ComptimeBudget_TryConsumeTransaction(
                &transaction,
                ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT,
                1U) ||
        !ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        failedResource = transaction.exceededResource;
        failedAmount = failedResource == ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES
                               ? sizeof(SZrObject)
                               : 1U;
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        /* Reuse the runtime diagnostic without committing a partial reservation. */
        if (failedResource != ZR_PARSER_COMPTIME_BUDGET_NONE) {
            (void)ZrParser_ComptimeRuntime_Consume(
                    cs, failedResource, failedAmount, location);
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

SZrObject *ct_new_object(SZrCompilerState *cs) {
    SZrObject *object;

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        !ct_reserve_aggregate_allocation(
                cs, (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL})) {
        return ZR_NULL;
    }

    object = ZrCore_Object_New(cs->state, ZR_NULL);
    if (object != ZR_NULL) {
        ZrCore_Object_Init(cs->state, object);
    }
    return object;
}

SZrObject *ct_new_array(SZrCompilerState *cs) {
    SZrObject *array;

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        !ct_reserve_aggregate_allocation(
                cs, (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL})) {
        return ZR_NULL;
    }

    array = ZrCore_Object_NewCustomized(
            cs->state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(cs->state, array);
    }
    return array;
}
