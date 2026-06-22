#include "backend_aot_c_typed_direct_calls.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

static const SZrAotFunctionEntry *backend_aot_typed_direct_call_find_function_entry_by_flat_index(
        const SZrAotFunctionTable *table,
        TZrUInt32 flatIndex) {
    TZrUInt32 index;

    if (table == ZR_NULL || table->entries == ZR_NULL || flatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_NULL;
    }

    for (index = 0u; index < table->count; index++) {
        if (table->entries[index].flatIndex == flatIndex) {
            return &table->entries[index];
        }
    }

    return ZR_NULL;
}

static TZrBool backend_aot_can_write_c_static_direct_i64_no_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 calleeFunctionIndex) {
    const SZrAotFunctionEntry *calleeEntry;

    if (argumentCount != 0u ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    return (TZrBool)(calleeEntry != ZR_NULL &&
                     backend_aot_c_can_emit_typed_i64_no_arg_thunk(calleeEntry->function));
}

static TZrBool backend_aot_can_write_c_static_direct_bool_no_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 calleeFunctionIndex) {
    const SZrAotFunctionEntry *calleeEntry;

    if (argumentCount != 0u ||
        !backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    return (TZrBool)(calleeEntry != ZR_NULL &&
                     backend_aot_c_can_emit_typed_bool_no_arg_thunk(calleeEntry->function));
}

static TZrBool backend_aot_can_write_c_static_direct_u64_no_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 calleeFunctionIndex) {
    const SZrAotFunctionEntry *calleeEntry;

    if (argumentCount != 0u ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    return (TZrBool)(calleeEntry != ZR_NULL &&
                     backend_aot_c_can_emit_typed_u64_no_arg_thunk(calleeEntry->function));
}

static TZrBool backend_aot_can_write_c_static_direct_f64_no_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 calleeFunctionIndex) {
    const SZrAotFunctionEntry *calleeEntry;

    if (argumentCount != 0u ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    return (TZrBool)(calleeEntry != ZR_NULL &&
                     backend_aot_c_can_emit_typed_f64_no_arg_thunk(calleeEntry->function));
}

static TZrBool backend_aot_can_write_c_static_direct_f64_one_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outArgumentSlot) {
    const SZrAotFunctionEntry *calleeEntry;
    const TZrUInt32 argumentSlot = functionSlot + 1u;

    if (argumentCount != 1u ||
        outArgumentSlot == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, argumentSlot) ||
        !backend_aot_c_scalar_locals_f64_written_before(functionIr, argumentSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_f64_one_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outArgumentSlot = argumentSlot;
    return ZR_TRUE;
}

static TZrBool backend_aot_can_write_c_static_direct_f64_two_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot) {
    const SZrAotFunctionEntry *calleeEntry;
    const TZrUInt32 firstArgumentSlot = functionSlot + 1u;
    const TZrUInt32 secondArgumentSlot = functionSlot + 2u;

    if (argumentCount != 2u ||
        outFirstArgumentSlot == ZR_NULL ||
        outSecondArgumentSlot == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, firstArgumentSlot) ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, secondArgumentSlot) ||
        !backend_aot_c_scalar_locals_f64_written_before(functionIr, firstArgumentSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_f64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_f64_two_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outFirstArgumentSlot = firstArgumentSlot;
    *outSecondArgumentSlot = secondArgumentSlot;
    return ZR_TRUE;
}

static TZrBool backend_aot_can_write_c_static_direct_u64_one_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outArgumentSlot) {
    const SZrAotFunctionEntry *calleeEntry;
    const TZrUInt32 argumentSlot = functionSlot + 1u;

    if (argumentCount != 1u ||
        outArgumentSlot == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, argumentSlot) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, argumentSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_u64_one_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outArgumentSlot = argumentSlot;
    return ZR_TRUE;
}

static TZrBool backend_aot_can_write_c_static_direct_u64_two_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot) {
    const SZrAotFunctionEntry *calleeEntry;
    const TZrUInt32 firstArgumentSlot = functionSlot + 1u;
    const TZrUInt32 secondArgumentSlot = functionSlot + 2u;

    if (argumentCount != 2u ||
        outFirstArgumentSlot == ZR_NULL ||
        outSecondArgumentSlot == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, firstArgumentSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, secondArgumentSlot) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, firstArgumentSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_u64_two_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outFirstArgumentSlot = firstArgumentSlot;
    *outSecondArgumentSlot = secondArgumentSlot;
    return ZR_TRUE;
}

static TZrBool backend_aot_can_write_c_static_direct_bool_one_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outArgumentSlot) {
    const SZrAotFunctionEntry *calleeEntry;
    const TZrUInt32 argumentSlot = functionSlot + 1u;

    if (argumentCount != 1u ||
        outArgumentSlot == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_bool_slot(functionIr, argumentSlot) ||
        !backend_aot_c_scalar_locals_bool_written_before(functionIr, argumentSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_bool_one_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outArgumentSlot = argumentSlot;
    return ZR_TRUE;
}

static TZrBool backend_aot_can_write_c_static_direct_bool_two_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot) {
    const SZrAotFunctionEntry *calleeEntry;
    const TZrUInt32 firstArgumentSlot = functionSlot + 1u;
    const TZrUInt32 secondArgumentSlot = functionSlot + 2u;

    if (argumentCount != 2u ||
        outFirstArgumentSlot == ZR_NULL ||
        outSecondArgumentSlot == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_bool_slot(functionIr, firstArgumentSlot) ||
        !backend_aot_c_scalar_locals_has_bool_slot(functionIr, secondArgumentSlot) ||
        !backend_aot_c_scalar_locals_bool_written_before(functionIr, firstArgumentSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_bool_written_before(functionIr, secondArgumentSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_bool_two_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outFirstArgumentSlot = firstArgumentSlot;
    *outSecondArgumentSlot = secondArgumentSlot;
    return ZR_TRUE;
}

static TZrBool backend_aot_can_write_c_static_direct_i64_one_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outArgumentSlot) {
    const SZrAotFunctionEntry *calleeEntry;
    const TZrUInt32 argumentSlot = functionSlot + 1u;

    if (argumentCount != 1u ||
        outArgumentSlot == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, argumentSlot) ||
        !backend_aot_c_scalar_locals_i64_written_before(functionIr, argumentSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_i64_one_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outArgumentSlot = argumentSlot;
    return ZR_TRUE;
}

static TZrBool backend_aot_can_write_c_static_direct_i64_two_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot) {
    const SZrAotFunctionEntry *calleeEntry;
    const TZrUInt32 firstArgumentSlot = functionSlot + 1u;
    const TZrUInt32 secondArgumentSlot = functionSlot + 2u;

    if (argumentCount != 2u ||
        outFirstArgumentSlot == ZR_NULL ||
        outSecondArgumentSlot == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, firstArgumentSlot) ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, secondArgumentSlot) ||
        !backend_aot_c_scalar_locals_i64_written_before(functionIr, firstArgumentSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_i64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_call_find_function_entry_by_flat_index(functionTable, calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_i64_two_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outFirstArgumentSlot = firstArgumentSlot;
    *outSecondArgumentSlot = secondArgumentSlot;
    return ZR_TRUE;
}

TZrBool backend_aot_try_write_c_static_direct_typed_function_call(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex) {
    TZrUInt32 typedArgumentSlot = 0u;
    TZrUInt32 typedFirstArgumentSlot = 0u;
    TZrUInt32 typedSecondArgumentSlot = 0u;
    TZrBool syncI64StackSlot;
    TZrBool syncU64StackSlot;
    TZrBool syncF64StackSlot;

    if (file == ZR_NULL || calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_FALSE;
    }

    if (backend_aot_can_write_c_static_direct_bool_no_arg_call(functionTable,
                                                               functionIr,
                                                               destinationSlot,
                                                               argumentCount,
                                                               calleeFunctionIndex)) {
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_bool_no_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   syncBoolStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_bool_one_arg_call(functionTable,
                                                                functionIr,
                                                                destinationSlot,
                                                                functionSlot,
                                                                argumentCount,
                                                                execInstructionIndex,
                                                                calleeFunctionIndex,
                                                                &typedArgumentSlot)) {
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_bool_one_arg_function_call(file,
                                                                    destinationSlot,
                                                                    calleeFunctionIndex,
                                                                    typedArgumentSlot,
                                                                    syncBoolStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_bool_two_arg_call(functionTable,
                                                                functionIr,
                                                                destinationSlot,
                                                                functionSlot,
                                                                argumentCount,
                                                                execInstructionIndex,
                                                                calleeFunctionIndex,
                                                                &typedFirstArgumentSlot,
                                                                &typedSecondArgumentSlot)) {
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_bool_two_arg_function_call(file,
                                                                    destinationSlot,
                                                                    calleeFunctionIndex,
                                                                    typedFirstArgumentSlot,
                                                                    typedSecondArgumentSlot,
                                                                    syncBoolStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_u64_no_arg_call(functionTable,
                                                              functionIr,
                                                              destinationSlot,
                                                              argumentCount,
                                                              calleeFunctionIndex)) {
        syncU64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  calleeFunctionIndex,
                                                                  syncU64StackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_u64_one_arg_call(functionTable,
                                                               functionIr,
                                                               destinationSlot,
                                                               functionSlot,
                                                               argumentCount,
                                                               execInstructionIndex,
                                                               calleeFunctionIndex,
                                                               &typedArgumentSlot)) {
        syncU64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_one_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   typedArgumentSlot,
                                                                   syncU64StackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_u64_two_arg_call(functionTable,
                                                               functionIr,
                                                               destinationSlot,
                                                               functionSlot,
                                                               argumentCount,
                                                               execInstructionIndex,
                                                               calleeFunctionIndex,
                                                               &typedFirstArgumentSlot,
                                                               &typedSecondArgumentSlot)) {
        syncU64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_two_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   typedFirstArgumentSlot,
                                                                   typedSecondArgumentSlot,
                                                                   syncU64StackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_f64_no_arg_call(functionTable,
                                                              functionIr,
                                                              destinationSlot,
                                                              argumentCount,
                                                              calleeFunctionIndex)) {
        syncF64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  calleeFunctionIndex,
                                                                  syncF64StackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_f64_one_arg_call(functionTable,
                                                               functionIr,
                                                               destinationSlot,
                                                               functionSlot,
                                                               argumentCount,
                                                               execInstructionIndex,
                                                               calleeFunctionIndex,
                                                               &typedArgumentSlot)) {
        syncF64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_one_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   typedArgumentSlot,
                                                                   syncF64StackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_f64_two_arg_call(functionTable,
                                                               functionIr,
                                                               destinationSlot,
                                                               functionSlot,
                                                               argumentCount,
                                                               execInstructionIndex,
                                                               calleeFunctionIndex,
                                                               &typedFirstArgumentSlot,
                                                               &typedSecondArgumentSlot)) {
        syncF64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_two_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   typedFirstArgumentSlot,
                                                                   typedSecondArgumentSlot,
                                                                   syncF64StackSlot);
        return ZR_TRUE;
    }

    syncI64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
            functionIr, destinationSlot, execInstructionIndex);

    if (backend_aot_can_write_c_static_direct_i64_two_arg_call(functionTable,
                                                              functionIr,
                                                              destinationSlot,
                                                              functionSlot,
                                                              argumentCount,
                                                              execInstructionIndex,
                                                              calleeFunctionIndex,
                                                              &typedFirstArgumentSlot,
                                                              &typedSecondArgumentSlot)) {
        backend_aot_write_c_static_direct_i64_two_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   typedFirstArgumentSlot,
                                                                   typedSecondArgumentSlot,
                                                                   syncI64StackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_i64_one_arg_call(functionTable,
                                                              functionIr,
                                                              destinationSlot,
                                                              functionSlot,
                                                              argumentCount,
                                                              execInstructionIndex,
                                                              calleeFunctionIndex,
                                                              &typedArgumentSlot)) {
        backend_aot_write_c_static_direct_i64_one_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   typedArgumentSlot,
                                                                   syncI64StackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_f64_no_arg_call(functionTable,
                                                              functionIr,
                                                              destinationSlot,
                                                              0u,
                                                              calleeFunctionIndex)) {
        TZrBool syncStackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  calleeFunctionIndex,
                                                                  syncStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_i64_no_arg_call(functionTable,
                                                             functionIr,
                                                             destinationSlot,
                                                             argumentCount,
                                                             calleeFunctionIndex)) {
        backend_aot_write_c_static_direct_i64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  calleeFunctionIndex,
                                                                  syncI64StackSlot);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool backend_aot_try_write_c_static_direct_typed_no_arg_function_call(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex) {
    if (file == ZR_NULL || calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_FALSE;
    }

    if (backend_aot_can_write_c_static_direct_bool_no_arg_call(functionTable,
                                                               functionIr,
                                                               destinationSlot,
                                                               0u,
                                                               calleeFunctionIndex)) {
        TZrBool syncStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_bool_no_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   syncStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_u64_no_arg_call(functionTable,
                                                              functionIr,
                                                              destinationSlot,
                                                              0u,
                                                              calleeFunctionIndex)) {
        TZrBool syncStackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  calleeFunctionIndex,
                                                                  syncStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_f64_no_arg_call(functionTable,
                                                              functionIr,
                                                              destinationSlot,
                                                              0u,
                                                              calleeFunctionIndex)) {
        TZrBool syncStackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  calleeFunctionIndex,
                                                                  syncStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_i64_no_arg_call(functionTable,
                                                             functionIr,
                                                             destinationSlot,
                                                             0u,
                                                             calleeFunctionIndex)) {
        TZrBool syncStackSlot = (TZrBool)!backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_i64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  calleeFunctionIndex,
                                                                  syncStackSlot);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
