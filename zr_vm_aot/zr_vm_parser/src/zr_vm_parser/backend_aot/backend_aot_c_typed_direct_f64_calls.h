#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_F64_CALLS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_F64_CALLS_H

#include "backend_aot_internal.h"

TZrBool backend_aot_can_write_c_static_direct_f64_no_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 calleeFunctionIndex);
TZrBool backend_aot_can_write_c_static_direct_f64_one_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outArgumentSlot);
TZrBool backend_aot_can_write_c_static_direct_f64_two_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot,
        TZrBool *outPassStateToThunk);
TZrBool backend_aot_can_write_c_static_direct_f64_bool_two_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot);
TZrBool backend_aot_can_write_c_static_direct_f64_three_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot,
        TZrUInt32 *outThirdArgumentSlot,
        TZrBool *outPassStateToThunk);

#endif
