#include "backend_aot_c_typed_direct_calls.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_c_typed_direct_bool_calls.h"
#include "backend_aot_c_typed_direct_f64_calls.h"
#include "backend_aot_c_typed_direct_i64_calls.h"
#include "backend_aot_c_typed_direct_u64_calls.h"

static void backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(
        FILE *file,
        const char *kindName,
        const char *localPrefix,
        TZrUInt32 destinationSlot,
        TZrUInt32 calleeFunctionIndex);
static void backend_aot_write_c_static_direct_typed_one_arg_full_aot_function_call(
        FILE *file,
        const char *kindName,
        const char *localPrefix,
        TZrUInt32 destinationSlot,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 argumentSlot);
static void backend_aot_write_c_static_direct_typed_two_arg_full_aot_function_call(
        FILE *file,
        const char *kindName,
        const char *localPrefix,
        TZrUInt32 destinationSlot,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 firstArgumentSlot,
        TZrUInt32 secondArgumentSlot,
        TZrBool passStateToThunk);
static void backend_aot_write_c_static_direct_typed_three_arg_full_aot_function_call(
        FILE *file,
        const char *kindName,
        const char *localPrefix,
        TZrUInt32 destinationSlot,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 firstArgumentSlot,
        TZrUInt32 secondArgumentSlot,
        TZrUInt32 thirdArgumentSlot,
        TZrBool passStateToThunk);
static void backend_aot_write_c_static_direct_typed_bool_two_arg_full_aot_function_call(
        FILE *file,
        const char *markerPrefix,
        const char *argumentLocalPrefix,
        TZrUInt32 destinationSlot,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 firstArgumentSlot,
        TZrUInt32 secondArgumentSlot);

TZrBool backend_aot_try_write_c_static_direct_typed_function_call(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrBool requireFullAot) {
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(file,
                                                                                  "bool",
                                                                                  "b",
                                                                                  destinationSlot,
                                                                                  calleeFunctionIndex);
            return ZR_TRUE;
        }
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_bool_no_arg_function_call(file,
                                                                    destinationSlot,
                                                                    functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_one_arg_full_aot_function_call(file,
                                                                                   "bool",
                                                                                   "b",
                                                                                   destinationSlot,
                                                                                   calleeFunctionIndex,
                                                                                   typedArgumentSlot);
            return ZR_TRUE;
        }
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_bool_one_arg_function_call(file,
                                                                     destinationSlot,
                                                                     functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_two_arg_full_aot_function_call(file,
                                                                                   "bool",
                                                                                   "b",
                                                                                   destinationSlot,
                                                                                   calleeFunctionIndex,
                                                                                   typedFirstArgumentSlot,
                                                                                   typedSecondArgumentSlot,
                                                                                   ZR_FALSE);
            return ZR_TRUE;
        }
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_bool_two_arg_function_call(file,
                                                                     destinationSlot,
                                                                     functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_three_arg_full_aot_function_call(file,
                                                                                     "bool",
                                                                                     "b",
                                                                                     destinationSlot,
                                                                                     calleeFunctionIndex,
                                                                                     typedFirstArgumentSlot,
                                                                                     typedSecondArgumentSlot,
                                                                                     typedThirdArgumentSlot,
                                                                                     ZR_FALSE);
            return ZR_TRUE;
        }
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_bool_three_arg_function_call(file,
                                                                       destinationSlot,
                                                                       functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_bool_two_arg_full_aot_function_call(file,
                                                                                        "i64_bool",
                                                                                        "s",
                                                                                        destinationSlot,
                                                                                        calleeFunctionIndex,
                                                                                        typedFirstArgumentSlot,
                                                                                        typedSecondArgumentSlot);
            return ZR_TRUE;
        }
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_i64_bool_two_arg_function_call(file,
                                                                         destinationSlot,
                                                                         functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_bool_two_arg_full_aot_function_call(file,
                                                                                        "u64_bool",
                                                                                        "u",
                                                                                        destinationSlot,
                                                                                        calleeFunctionIndex,
                                                                                        typedFirstArgumentSlot,
                                                                                        typedSecondArgumentSlot);
            return ZR_TRUE;
        }
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_bool_two_arg_function_call(file,
                                                                         destinationSlot,
                                                                         functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_bool_two_arg_full_aot_function_call(file,
                                                                                        "f64_bool",
                                                                                        "f",
                                                                                        destinationSlot,
                                                                                        calleeFunctionIndex,
                                                                                        typedFirstArgumentSlot,
                                                                                        typedSecondArgumentSlot);
            return ZR_TRUE;
        }
        TZrBool syncBoolStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_bool_two_arg_function_call(file,
                                                                         destinationSlot,
                                                                         functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(file,
                                                                                  "u64",
                                                                                  "u",
                                                                                  destinationSlot,
                                                                                  calleeFunctionIndex);
            return ZR_TRUE;
        }
        syncU64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_one_arg_full_aot_function_call(file,
                                                                                   "u64",
                                                                                   "u",
                                                                                   destinationSlot,
                                                                                   calleeFunctionIndex,
                                                                                   typedArgumentSlot);
            return ZR_TRUE;
        }
        syncU64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_one_arg_function_call(file,
                                                                   destinationSlot,
                                                                   functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_two_arg_full_aot_function_call(file,
                                                                                   "u64",
                                                                                   "u",
                                                                                   destinationSlot,
                                                                                   calleeFunctionIndex,
                                                                                   typedFirstArgumentSlot,
                                                                                   typedSecondArgumentSlot,
                                                                                   typedU64TwoArgCallPassState);
            return ZR_TRUE;
        }
        syncU64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_two_arg_function_call(file,
                                                                   destinationSlot,
                                                                   functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_three_arg_full_aot_function_call(file,
                                                                                     "u64",
                                                                                     "u",
                                                                                     destinationSlot,
                                                                                     calleeFunctionIndex,
                                                                                     typedFirstArgumentSlot,
                                                                                     typedSecondArgumentSlot,
                                                                                     typedThirdArgumentSlot,
                                                                                     typedU64ThreeArgCallPassState);
            return ZR_TRUE;
        }
        syncU64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_three_arg_function_call(file,
                                                                     destinationSlot,
                                                                     functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(file,
                                                                                  "f64",
                                                                                  "f",
                                                                                  destinationSlot,
                                                                                  calleeFunctionIndex);
            return ZR_TRUE;
        }
        syncF64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_no_arg_function_call(file,
                                                                   destinationSlot,
                                                                   functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_one_arg_full_aot_function_call(file,
                                                                                   "f64",
                                                                                   "f",
                                                                                   destinationSlot,
                                                                                   calleeFunctionIndex,
                                                                                   typedArgumentSlot);
            return ZR_TRUE;
        }
        syncF64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_one_arg_function_call(file,
                                                                    destinationSlot,
                                                                    functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_two_arg_full_aot_function_call(file,
                                                                                   "f64",
                                                                                   "f",
                                                                                   destinationSlot,
                                                                                   calleeFunctionIndex,
                                                                                   typedFirstArgumentSlot,
                                                                                   typedSecondArgumentSlot,
                                                                                   typedF64TwoArgCallPassState);
            return ZR_TRUE;
        }
        syncF64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_two_arg_function_call(file,
                                                                    destinationSlot,
                                                                    functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_three_arg_full_aot_function_call(file,
                                                                                     "f64",
                                                                                     "f",
                                                                                     destinationSlot,
                                                                                     calleeFunctionIndex,
                                                                                     typedFirstArgumentSlot,
                                                                                     typedSecondArgumentSlot,
                                                                                     typedThirdArgumentSlot,
                                                                                     typedF64ThreeArgCallPassState);
            return ZR_TRUE;
        }
        syncF64StackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_three_arg_function_call(file,
                                                                      destinationSlot,
                                                                      functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_three_arg_full_aot_function_call(file,
                                                                                     "i64",
                                                                                     "s",
                                                                                     destinationSlot,
                                                                                     calleeFunctionIndex,
                                                                                     typedFirstArgumentSlot,
                                                                                     typedSecondArgumentSlot,
                                                                                     typedThirdArgumentSlot,
                                                                                     typedI64ThreeArgCallPassState);
            return ZR_TRUE;
        }
        backend_aot_write_c_static_direct_i64_three_arg_function_call(file,
                                                                     destinationSlot,
                                                                     functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_two_arg_full_aot_function_call(file,
                                                                                   "i64",
                                                                                   "s",
                                                                                   destinationSlot,
                                                                                   calleeFunctionIndex,
                                                                                   typedFirstArgumentSlot,
                                                                                   typedSecondArgumentSlot,
                                                                                   typedI64TwoArgCallPassState);
            return ZR_TRUE;
        }
        backend_aot_write_c_static_direct_i64_two_arg_function_call(file,
                                                                   destinationSlot,
                                                                   functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_one_arg_full_aot_function_call(file,
                                                                                   "i64",
                                                                                   "s",
                                                                                   destinationSlot,
                                                                                   calleeFunctionIndex,
                                                                                   typedArgumentSlot);
            return ZR_TRUE;
        }
        backend_aot_write_c_static_direct_i64_one_arg_function_call(file,
                                                                   destinationSlot,
                                                                   functionSlot,
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
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(file,
                                                                                  "f64",
                                                                                  "f",
                                                                                  destinationSlot,
                                                                                  calleeFunctionIndex);
            return ZR_TRUE;
        }
        TZrBool syncStackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_no_arg_function_call(file,
                                                                   destinationSlot,
                                                                   functionSlot,
                                                                   calleeFunctionIndex,
                                                                   syncStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_i64_no_arg_call(functionTable,
                                                             functionIr,
                                                             destinationSlot,
                                                             argumentCount,
                                                             calleeFunctionIndex)) {
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(file,
                                                                                  "i64",
                                                                                  "s",
                                                                                  destinationSlot,
                                                                                  calleeFunctionIndex);
            return ZR_TRUE;
        }
        backend_aot_write_c_static_direct_i64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  functionSlot,
                                                                  calleeFunctionIndex,
                                                                  syncI64StackSlot);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(
        FILE *file,
        const char *kindName,
        const char *localPrefix,
        TZrUInt32 destinationSlot,
        TZrUInt32 calleeFunctionIndex) {
    if (file == ZR_NULL ||
        kindName == ZR_NULL ||
        localPrefix == ZR_NULL ||
        calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_%s_no_arg_direct_call */\n"
            "        /* zr_aot_static_%s_no_arg_direct_call_full_aot */\n"
            "        zr_aot_%s%u = zr_aot_typed_%s_fn_%u();\n"
            "    }\n",
            kindName,
            kindName,
            localPrefix,
            (unsigned)destinationSlot,
            kindName,
            (unsigned)calleeFunctionIndex);
}

static void backend_aot_write_c_static_direct_typed_one_arg_full_aot_function_call(
        FILE *file,
        const char *kindName,
        const char *localPrefix,
        TZrUInt32 destinationSlot,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 argumentSlot) {
    if (file == ZR_NULL ||
        kindName == ZR_NULL ||
        localPrefix == ZR_NULL ||
        calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_%s_one_arg_direct_call */\n"
            "        /* zr_aot_static_%s_one_arg_direct_call_full_aot */\n"
            "        zr_aot_%s%u = zr_aot_typed_%s_fn_%u(zr_aot_%s%u);\n"
            "    }\n",
            kindName,
            kindName,
            localPrefix,
            (unsigned)destinationSlot,
            kindName,
            (unsigned)calleeFunctionIndex,
            localPrefix,
            (unsigned)argumentSlot);
}

static void backend_aot_write_c_static_direct_typed_two_arg_full_aot_function_call(
        FILE *file,
        const char *kindName,
        const char *localPrefix,
        TZrUInt32 destinationSlot,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 firstArgumentSlot,
        TZrUInt32 secondArgumentSlot,
        TZrBool passStateToThunk) {
    if (file == ZR_NULL ||
        kindName == ZR_NULL ||
        localPrefix == ZR_NULL ||
        calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_%s_two_arg_direct_call */\n"
            "        /* zr_aot_static_%s_two_arg_direct_call_full_aot */\n",
            kindName,
            kindName);
    if (passStateToThunk) {
        fprintf(file,
                "        zr_aot_%s%u = zr_aot_typed_%s_fn_%u(state, zr_aot_%s%u, zr_aot_%s%u);\n",
                localPrefix,
                (unsigned)destinationSlot,
                kindName,
                (unsigned)calleeFunctionIndex,
                localPrefix,
                (unsigned)firstArgumentSlot,
                localPrefix,
                (unsigned)secondArgumentSlot);
    } else {
        fprintf(file,
                "        zr_aot_%s%u = zr_aot_typed_%s_fn_%u(zr_aot_%s%u, zr_aot_%s%u);\n",
                localPrefix,
                (unsigned)destinationSlot,
                kindName,
                (unsigned)calleeFunctionIndex,
                localPrefix,
                (unsigned)firstArgumentSlot,
                localPrefix,
                (unsigned)secondArgumentSlot);
    }
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_static_direct_typed_three_arg_full_aot_function_call(
        FILE *file,
        const char *kindName,
        const char *localPrefix,
        TZrUInt32 destinationSlot,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 firstArgumentSlot,
        TZrUInt32 secondArgumentSlot,
        TZrUInt32 thirdArgumentSlot,
        TZrBool passStateToThunk) {
    if (file == ZR_NULL ||
        kindName == ZR_NULL ||
        localPrefix == ZR_NULL ||
        calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_%s_three_arg_direct_call */\n"
            "        /* zr_aot_static_%s_three_arg_direct_call_full_aot */\n",
            kindName,
            kindName);
    if (passStateToThunk) {
        fprintf(file,
                "        zr_aot_%s%u = zr_aot_typed_%s_fn_%u(state, zr_aot_%s%u, zr_aot_%s%u, zr_aot_%s%u);\n",
                localPrefix,
                (unsigned)destinationSlot,
                kindName,
                (unsigned)calleeFunctionIndex,
                localPrefix,
                (unsigned)firstArgumentSlot,
                localPrefix,
                (unsigned)secondArgumentSlot,
                localPrefix,
                (unsigned)thirdArgumentSlot);
    } else {
        fprintf(file,
                "        zr_aot_%s%u = zr_aot_typed_%s_fn_%u(zr_aot_%s%u, zr_aot_%s%u, zr_aot_%s%u);\n",
                localPrefix,
                (unsigned)destinationSlot,
                kindName,
                (unsigned)calleeFunctionIndex,
                localPrefix,
                (unsigned)firstArgumentSlot,
                localPrefix,
                (unsigned)secondArgumentSlot,
                localPrefix,
                (unsigned)thirdArgumentSlot);
    }
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_static_direct_typed_bool_two_arg_full_aot_function_call(
        FILE *file,
        const char *markerPrefix,
        const char *argumentLocalPrefix,
        TZrUInt32 destinationSlot,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 firstArgumentSlot,
        TZrUInt32 secondArgumentSlot) {
    if (file == ZR_NULL ||
        markerPrefix == ZR_NULL ||
        argumentLocalPrefix == ZR_NULL ||
        calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_static_%s_two_arg_direct_call */\n"
            "        /* zr_aot_static_%s_two_arg_direct_call_full_aot */\n"
            "        zr_aot_b%u = zr_aot_typed_bool_fn_%u(zr_aot_%s%u, zr_aot_%s%u);\n"
            "    }\n",
            markerPrefix,
            markerPrefix,
            (unsigned)destinationSlot,
            (unsigned)calleeFunctionIndex,
            argumentLocalPrefix,
            (unsigned)firstArgumentSlot,
            argumentLocalPrefix,
            (unsigned)secondArgumentSlot);
}

TZrBool backend_aot_try_write_c_static_direct_typed_no_arg_function_call(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrBool requireFullAot) {
    if (file == ZR_NULL || calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_FALSE;
    }

    if (backend_aot_can_write_c_static_direct_bool_no_arg_call(functionTable,
                                                               functionIr,
                                                               destinationSlot,
                                                               0u,
                                                               calleeFunctionIndex)) {
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(file,
                                                                                  "bool",
                                                                                  "b",
                                                                                  destinationSlot,
                                                                                  calleeFunctionIndex);
            return ZR_TRUE;
        }
        TZrBool syncStackSlot = (TZrBool)!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_bool_no_arg_function_call(file,
                                                                    destinationSlot,
                                                                    functionSlot,
                                                                    calleeFunctionIndex,
                                                                    syncStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_u64_no_arg_call(functionTable,
                                                              functionIr,
                                                              destinationSlot,
                                                              0u,
                                                              calleeFunctionIndex)) {
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(file,
                                                                                  "u64",
                                                                                  "u",
                                                                                  destinationSlot,
                                                                                  calleeFunctionIndex);
            return ZR_TRUE;
        }
        TZrBool syncStackSlot = (TZrBool)!backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_u64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  functionSlot,
                                                                  calleeFunctionIndex,
                                                                  syncStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_f64_no_arg_call(functionTable,
                                                              functionIr,
                                                              destinationSlot,
                                                              0u,
                                                              calleeFunctionIndex)) {
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(file,
                                                                                  "f64",
                                                                                  "f",
                                                                                  destinationSlot,
                                                                                  calleeFunctionIndex);
            return ZR_TRUE;
        }
        TZrBool syncStackSlot = (TZrBool)!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_f64_no_arg_function_call(file,
                                                                   destinationSlot,
                                                                   functionSlot,
                                                                   calleeFunctionIndex,
                                                                   syncStackSlot);
        return ZR_TRUE;
    }

    if (backend_aot_can_write_c_static_direct_i64_no_arg_call(functionTable,
                                                             functionIr,
                                                             destinationSlot,
                                                             0u,
                                                             calleeFunctionIndex)) {
        if (requireFullAot) {
            backend_aot_write_c_static_direct_typed_no_arg_full_aot_function_call(file,
                                                                                  "i64",
                                                                                  "s",
                                                                                  destinationSlot,
                                                                                  calleeFunctionIndex);
            return ZR_TRUE;
        }
        TZrBool syncStackSlot = (TZrBool)!backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex);
        backend_aot_write_c_static_direct_i64_no_arg_function_call(file,
                                                                  destinationSlot,
                                                                  functionSlot,
                                                                  calleeFunctionIndex,
                                                                  syncStackSlot);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
