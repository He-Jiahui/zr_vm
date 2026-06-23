#include "backend_aot_c_typed_direct_u64_calls.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

static const SZrAotFunctionEntry *backend_aot_typed_direct_u64_call_find_function_entry_by_flat_index(
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

TZrBool backend_aot_can_write_c_static_direct_u64_no_arg_call(
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

    calleeEntry = backend_aot_typed_direct_u64_call_find_function_entry_by_flat_index(functionTable,
                                                                                     calleeFunctionIndex);
    return (TZrBool)(calleeEntry != ZR_NULL &&
                     backend_aot_c_can_emit_typed_u64_no_arg_thunk(calleeEntry->function));
}

TZrBool backend_aot_can_write_c_static_direct_u64_one_arg_call(
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

    calleeEntry = backend_aot_typed_direct_u64_call_find_function_entry_by_flat_index(functionTable,
                                                                                     calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_u64_one_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outArgumentSlot = argumentSlot;
    return ZR_TRUE;
}

TZrBool backend_aot_can_write_c_static_direct_u64_two_arg_call(
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

    calleeEntry = backend_aot_typed_direct_u64_call_find_function_entry_by_flat_index(functionTable,
                                                                                     calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_u64_two_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outFirstArgumentSlot = firstArgumentSlot;
    *outSecondArgumentSlot = secondArgumentSlot;
    return ZR_TRUE;
}

TZrBool backend_aot_can_write_c_static_direct_u64_bool_two_arg_call(
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
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, firstArgumentSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, secondArgumentSlot) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, firstArgumentSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_u64_call_find_function_entry_by_flat_index(functionTable,
                                                                                     calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outFirstArgumentSlot = firstArgumentSlot;
    *outSecondArgumentSlot = secondArgumentSlot;
    return ZR_TRUE;
}

TZrBool backend_aot_can_write_c_static_direct_u64_three_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot,
        TZrUInt32 *outThirdArgumentSlot) {
    const SZrAotFunctionEntry *calleeEntry;
    const TZrUInt32 firstArgumentSlot = functionSlot + 1u;
    const TZrUInt32 secondArgumentSlot = functionSlot + 2u;
    const TZrUInt32 thirdArgumentSlot = functionSlot + 3u;

    if (argumentCount != 3u ||
        outFirstArgumentSlot == ZR_NULL ||
        outSecondArgumentSlot == ZR_NULL ||
        outThirdArgumentSlot == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, firstArgumentSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, secondArgumentSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, thirdArgumentSlot) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, firstArgumentSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, secondArgumentSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, thirdArgumentSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    calleeEntry = backend_aot_typed_direct_u64_call_find_function_entry_by_flat_index(functionTable,
                                                                                     calleeFunctionIndex);
    if (calleeEntry == ZR_NULL ||
        !backend_aot_c_can_emit_typed_u64_three_arg_thunk(calleeEntry->function)) {
        return ZR_FALSE;
    }

    *outFirstArgumentSlot = firstArgumentSlot;
    *outSecondArgumentSlot = secondArgumentSlot;
    *outThirdArgumentSlot = thirdArgumentSlot;
    return ZR_TRUE;
}
