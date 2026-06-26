#include "backend_aot_c_typed_direct_calls.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_c_typed_direct_bool_calls.h"
#include "backend_aot_c_typed_direct_f64_calls.h"
#include "backend_aot_c_typed_direct_i64_calls.h"
#include "backend_aot_c_typed_direct_u64_calls.h"

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
    TZrUInt32 typedThirdArgumentSlot = 0u;
    TZrBool syncI64StackSlot;
    TZrBool syncU64StackSlot;
    TZrBool syncF64StackSlot;
    TZrBool typedI64TwoArgCallPassState = ZR_TRUE;
    TZrBool typedI64ThreeArgCallPassState = ZR_TRUE;
    TZrBool typedU64TwoArgCallPassState = ZR_TRUE;
    TZrBool typedU64ThreeArgCallPassState = ZR_TRUE;
    TZrBool typedF64TwoArgCallPassState = ZR_TRUE;
    TZrBool typedF64ThreeArgCallPassState = ZR_TRUE;

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

    if (backend_aot_can_write_c_static_direct_bool_three_arg_call(functionTable,
                                                                  functionIr,
                                                                  destinationSlot,
                                                                  functionSlot,
                                                                  argumentCount,
                                                                  execInstructionIndex,
                                                                  calleeFunctionIndex,
                                                                  &typedFirstArgumentSlot,
                                                                  &typedSecondArgumentSlot,
                                                                  &typedThirdArgumentSlot)) {
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_bool_three_arg_function_call(file,
                                                                      destinationSlot,
                                                                      calleeFunctionIndex,
                                                                      typedFirstArgumentSlot,
                                                                      typedSecondArgumentSlot,
                                                                      typedThirdArgumentSlot,
                                                                      syncBoolStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_i64_bool_two_arg_call(functionTable,
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
        backend_aot_write_c_static_direct_i64_bool_two_arg_function_call(file,
                                                                        destinationSlot,
                                                                        calleeFunctionIndex,
                                                                        typedFirstArgumentSlot,
                                                                        typedSecondArgumentSlot,
                                                                        syncBoolStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_u64_bool_two_arg_call(functionTable,
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
        backend_aot_write_c_static_direct_u64_bool_two_arg_function_call(file,
                                                                        destinationSlot,
                                                                        calleeFunctionIndex,
                                                                        typedFirstArgumentSlot,
                                                                        typedSecondArgumentSlot,
                                                                        syncBoolStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_f64_bool_two_arg_call(functionTable,
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
        backend_aot_write_c_static_direct_f64_bool_two_arg_function_call(file,
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
                                                               &typedSecondArgumentSlot,
                                                               &typedU64TwoArgCallPassState)) {
        syncU64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_two_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   typedFirstArgumentSlot,
                                                                   typedSecondArgumentSlot,
                                                                   syncU64StackSlot,
                                                                   typedU64TwoArgCallPassState);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_u64_three_arg_call(functionTable,
                                                                 functionIr,
                                                                 destinationSlot,
                                                                 functionSlot,
                                                                 argumentCount,
                                                                 execInstructionIndex,
                                                                 calleeFunctionIndex,
                                                                 &typedFirstArgumentSlot,
                                                                 &typedSecondArgumentSlot,
                                                                 &typedThirdArgumentSlot,
                                                                 &typedU64ThreeArgCallPassState)) {
        syncU64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_three_arg_function_call(file,
                                                                     destinationSlot,
                                                                     calleeFunctionIndex,
                                                                     typedFirstArgumentSlot,
                                                                     typedSecondArgumentSlot,
                                                                     typedThirdArgumentSlot,
                                                                     syncU64StackSlot,
                                                                     typedU64ThreeArgCallPassState);
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
                                                               &typedSecondArgumentSlot,
                                                               &typedF64TwoArgCallPassState)) {
        syncF64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_two_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   typedFirstArgumentSlot,
                                                                   typedSecondArgumentSlot,
                                                                   syncF64StackSlot,
                                                                   typedF64TwoArgCallPassState);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_f64_three_arg_call(functionTable,
                                                                 functionIr,
                                                                 destinationSlot,
                                                                 functionSlot,
                                                                 argumentCount,
                                                                 execInstructionIndex,
                                                                 calleeFunctionIndex,
                                                                 &typedFirstArgumentSlot,
                                                                 &typedSecondArgumentSlot,
                                                                 &typedThirdArgumentSlot,
                                                                 &typedF64ThreeArgCallPassState)) {
        syncF64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_three_arg_function_call(file,
                                                                     destinationSlot,
                                                                     calleeFunctionIndex,
                                                                     typedFirstArgumentSlot,
                                                                     typedSecondArgumentSlot,
                                                                     typedThirdArgumentSlot,
                                                                     syncF64StackSlot,
                                                                     typedF64ThreeArgCallPassState);
        return ZR_TRUE;
    }

    syncI64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
            functionIr, destinationSlot, execInstructionIndex);

    if (backend_aot_can_write_c_static_direct_i64_three_arg_call(functionTable,
                                                                functionIr,
                                                                destinationSlot,
                                                                functionSlot,
                                                                argumentCount,
                                                                execInstructionIndex,
                                                                calleeFunctionIndex,
                                                                &typedFirstArgumentSlot,
                                                                &typedSecondArgumentSlot,
                                                                &typedThirdArgumentSlot,
                                                                &typedI64ThreeArgCallPassState)) {
        backend_aot_write_c_static_direct_i64_three_arg_function_call(file,
                                                                     destinationSlot,
                                                                     calleeFunctionIndex,
                                                                     typedFirstArgumentSlot,
                                                                     typedSecondArgumentSlot,
                                                                     typedThirdArgumentSlot,
                                                                     syncI64StackSlot,
                                                                     typedI64ThreeArgCallPassState);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_i64_two_arg_call(functionTable,
                                                              functionIr,
                                                              destinationSlot,
                                                              functionSlot,
                                                              argumentCount,
                                                              execInstructionIndex,
                                                              calleeFunctionIndex,
                                                              &typedFirstArgumentSlot,
                                                              &typedSecondArgumentSlot,
                                                              &typedI64TwoArgCallPassState)) {
        backend_aot_write_c_static_direct_i64_two_arg_function_call(file,
                                                                   destinationSlot,
                                                                   calleeFunctionIndex,
                                                                   typedFirstArgumentSlot,
                                                                   typedSecondArgumentSlot,
                                                                   syncI64StackSlot,
                                                                   typedI64TwoArgCallPassState);
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
